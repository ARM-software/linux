/*
 * XpressRICH3-AXI PCIe Host Bridge Driver.
 *
 * Copyright (C) 2012-2013 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>

#include <asm/pci-bridge.h>

#include "pci-xr3.h"

struct xr3pci_port {
	void __iomem	*base;
	void __iomem	*reset;
	void __iomem	*ecam;
	struct resource ecam_space;
#ifdef CONFIG_PCI_MSI
	struct resource msi_res;
#endif
};

void __iomem *xr3pci_map_bus(struct pci_bus *bus, unsigned int devfn, int where)
{
	struct xr3pci_port *pp = bus->sysdata;

	return pp->ecam + XR3PCI_ECAM_OFFSET(bus->number, devfn, where);
}

struct pci_ops xr3pci_ops = {
	.map_bus = xr3pci_map_bus,
	.read	= pci_generic_config_read,
	.write	= pci_generic_config_write,
};

static int xr3pci_enable_device(struct xr3pci_port *pp)
{
	u32 val;
	int timeout = 200;

	/* add credits */
	writel(0x00f0b818, pp->base + XR3PCI_VIRTCHAN_CREDITS);
	writel(0x1, pp->base + XR3PCI_VIRTCHAN_CREDITS + 4);

	/* allow ECRC */
	writel(0x6006, pp->base + XR3PCI_PEX_SPC2);

	writel(JUNO_RESET_CTRL_PHY | JUNO_RESET_CTRL_RC,
		pp->reset + JUNO_RESET_CTRL);
	do {
		msleep(1);
		val = readl(pp->reset + JUNO_RESET_STATUS);
	} while (--timeout &&
		(val & JUNO_RESET_STATUS_MASK) != JUNO_RESET_STATUS_MASK);

	if (!timeout) {
		pr_err("Unable to bring " DEVICE_NAME " out of reset");
		return -EAGAIN;
	}

	msleep(20);
	timeout = 20;
	do {
		msleep(1);
		val = readl(pp->base + XR3PCI_BASIC_STATUS);
	} while (--timeout && !(val & XR3PCI_BS_LINK_MASK));

	if (!(val & XR3PCI_BS_LINK_MASK)) {
		pr_warn(DEVICE_NAME ": No link negotiated\n");
		return -EIO;
	}

	pr_info(DEVICE_NAME " %dx link negotiated (gen %d), maxpayload %d, maxreqsize %d\n",
		val & XR3PCI_BS_LINK_MASK, (val & XR3PCI_BS_GEN_MASK) >> 8,
		2 << (7 + ((val & XR3PCI_BS_NEG_PAYLOAD_MASK) << 24)),
		2 << (7 + ((val & XR3PCI_BS_NEG_REQSIZE_MASK) >> 28)));

	return 0;
}

static void xr3pci_update_atr_entry(void __iomem *base,
			resource_size_t src_addr, resource_size_t trsl_addr,
			int trsl_param, int window_size)
{
	/* bit 0: enable entry, bits 1-6: ATR window size (2^window_size + 1) */
	writel(src_addr | (window_size << 1) | 0x1, base + XR3PCI_ATR_SRC_ADDR_LOW);
	writel(trsl_addr, base + XR3PCI_ATR_TRSL_ADDR_LOW);

#ifdef CONFIG_PHYS_ADDR_T_64BIT
	writel(src_addr >> 32, base + XR3PCI_ATR_SRC_ADDR_HIGH);
	writel(trsl_addr >> 32, base + XR3PCI_ATR_TRSL_ADDR_HIGH);
#endif

	writel(trsl_param, base + XR3PCI_ATR_TRSL_PARAM);
}

static int xr3pci_setup_atr(struct xr3pci_port *pp, struct device *dev,
			struct list_head *resources, resource_size_t io_base)
{
	int window_size;
	struct resource_entry *window;
	struct resource *res;
	resource_size_t offset;
	void __iomem *table_base;

	/* Address translation from PCIe to CPU */
	table_base = pp->base + XR3PCI_ATR_PCIE_WIN0;
#ifdef CONFIG_PCI_MSI
	/* map the MSI resources as accessible device from PCIe transactions */
	window_size = ilog2(resource_size(&pp->msi_res)) - 1;
	xr3pci_update_atr_entry(table_base, pp->msi_res.start, pp->msi_res.start,
				XR3PCI_ATR_TRSLID_AXIDEVICE, window_size);
	table_base += XR3PCI_ATR_TABLE_SIZE;
#endif
	/* 1:1 mapping for inbound PCIe transactions to memory */
	xr3pci_update_atr_entry(table_base, 0x80000000, 0x80000000,
				XR3PCI_ATR_TRSLID_AXIMEMORY, 0x1e);
	table_base += XR3PCI_ATR_TABLE_SIZE;
	xr3pci_update_atr_entry(table_base, 0x880000000, 0x880000000,
				XR3PCI_ATR_TRSLID_AXIMEMORY, 0x1f);

	/* Address translation from CPU to PCIe */
	table_base = pp->base + XR3PCI_ATR_AXI4_SLV0;
	/* map ECAM space to bus configuration interface */
	window_size = ilog2(resource_size(&pp->ecam_space)) - 1;
	xr3pci_update_atr_entry(pp->base + XR3PCI_ATR_AXI4_SLV0,
				pp->ecam_space.start, 0,
				XR3PCI_ATR_TRSLID_PCIE_CONF, window_size);
	table_base += XR3PCI_ATR_TABLE_SIZE;

	resource_list_for_each_entry(window, resources) {
		res = window->res;
		offset = window->offset;
		window_size = ilog2(resource_size(res)) - 1;

		if (resource_type(res) == IORESOURCE_MEM) {
			if (devm_request_resource(dev, &iomem_resource, res)) {
				dev_info(dev, "failed to request MEM resource %pR\n", res);
			} else {
				xr3pci_update_atr_entry(table_base, res->start,
							res->start - offset,
							XR3PCI_ATR_TRSLID_PCIE_MEMORY,
							window_size);
			}
		} else if (resource_type(res) == IORESOURCE_IO) {
			pci_remap_iospace(res, res->start + io_base);
			if (devm_request_resource(dev, &ioport_resource, res)) {
				dev_info(dev, "failed to request IO resource %pR\n", res);
			} else {
				xr3pci_update_atr_entry(table_base,
							res->start + io_base,
							res->start - offset,
							XR3PCI_ATR_TRSLID_PCIE_IO,
							window_size);
			}
		}
		table_base += XR3PCI_ATR_TABLE_SIZE;
	}

	return 0;
}

static int xr3pci_setup_int(struct xr3pci_port *pp)
{
	/* Enable IRQs for MSIs and legacy interrupts */
	writel(~(XR3PCI_INT_MSI | XR3PCI_INT_INTx),
			pp->base + XR3PCI_LOCAL_INT_MASK);

	return 0;
}

static int xr3pci_get_resources(struct xr3pci_port *pp, struct device *dev)
{
	int err;
	struct resource res;
	struct device_node *np = dev->of_node;

	err = of_address_to_resource(np, 0, &res);
	if (err) {
		dev_err(dev, "Failed to find configuration registers\n");
		return err;
	}
	pp->base = devm_ioremap_resource(dev, &res);
	if (IS_ERR(pp->base))
		return PTR_ERR(pp->base);

	err = of_address_to_resource(np, 1, &res);
	if (err) {
		dev_err(dev, "Failed to find reset registers\n");
		return err;
	}
	pp->reset = devm_ioremap_resource(dev, &res);
	if (IS_ERR(pp->reset))
		return PTR_ERR(pp->reset);

	err = of_address_to_resource(np, 2, &pp->ecam_space);
	if (err) {
		dev_err(dev, "Failed to find ECAM configuration space\n");
		return -EINVAL;
	}
	pp->ecam = devm_ioremap_resource(dev, &pp->ecam_space);
	if (IS_ERR(pp->ecam))
		return PTR_ERR(pp->ecam);

	return 0;
}

static int xr3pci_setup(struct xr3pci_port *pp, struct device *dev,
			struct list_head *resources, resource_size_t io_base)
{
	int err;

	if ((err = xr3pci_get_resources(pp, dev)) != 0)
		return err;

	if ((err = xr3pci_setup_atr(pp, dev, resources, io_base)) != 0)
		return err;

	if ((err = xr3pci_enable_device(pp)) != 0)
		return err;

	if ((err = xr3pci_setup_int(pp)) != 0)
		return err;

	return 0;
}

/*
 * The XpressRICH3 doesn't describe itself as a bridge. This is required for
 * correct/normal enumeration. This quirk changes that.
 */
static void xr3pci_quirk_class(struct pci_dev *pdev)
{
	pdev->class = PCI_CLASS_BRIDGE_PCI << 8;
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_PLDA, PCI_DEVICE_ID_XR3PCI,
			xr3pci_quirk_class);

static int xr3pci_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device_node *dn;
#ifdef CONFIG_PCI_MSI
	struct device_node *msi_parent;
#endif
	struct xr3pci_port *pp;
	struct pci_bus *bus;
	resource_size_t io_base = 0;	/* physical address for start of I/O area */
	LIST_HEAD(res);

	dn = pdev->dev.of_node;

	if (!of_device_is_available(dn)) {
		pr_warn("%s: disabled\n", dn->full_name);
		return -ENODEV;
	}

	pp = kzalloc(sizeof(*pp), GFP_KERNEL);
	if (!pp)
		return -ENOMEM;

	err = of_pci_get_host_bridge_resources(dn, 0, 0xff, &res, &io_base);
	if (err)
		goto probe_err;

	err = xr3pci_setup(pp, &pdev->dev, &res, io_base);
	if (err)
		goto probe_err;

	/* We always enable PCI domains and we keep domain 0 backward
	 * compatible in /proc for video cards
	 */
	pci_add_flags(PCI_ENABLE_PROC_DOMAINS);
	pci_add_flags(PCI_REASSIGN_ALL_BUS | PCI_REASSIGN_ALL_RSRC);

	bus = pci_scan_root_bus(&pdev->dev, 0, &xr3pci_ops, pp, &res);
	if (!bus)
		err = -ENXIO;

#ifdef CONFIG_PCI_MSI
	msi_parent = of_parse_phandle(dn, "msi-parent", 0);
	if (!msi_parent) {
		dev_err(&pdev->dev, "Unable to locate msi-parent node.\n");
		goto probe_err;
	}
	err = of_address_to_resource(msi_parent, 0, &pp->msi_res);
	if (err) {
		dev_err(&pdev->dev, "Failed to parse MSI parent resource\n");
		goto probe_err;
	}
	bus->msi = of_pci_find_msi_chip_by_node(msi_parent);
#endif

	pci_assign_unassigned_bus_resources(bus);
	pci_bus_add_devices(bus);

probe_err:
	if (err)
		kfree(pp);
	pci_free_resource_list(&res);
	return err;

}

static const struct of_device_id xr3pci_device_id[] = {
	{ .compatible = "arm,pcie-xr3", },
};
MODULE_DEVICE_TABLE(of, xr3pci_device_id);

static struct platform_driver xr3pci_driver = {
	.driver		= {
		.name	= "pcie-xr3",
		.owner	= THIS_MODULE,
		.of_match_table = xr3pci_device_id,
	},
	.probe = xr3pci_probe,
};

module_platform_driver(xr3pci_driver);

MODULE_AUTHOR("Liviu Dudau <Liviu.Dudau@arm.com>");
MODULE_DESCRIPTION("XpressRICH3-AXI PCIe Host Bridge");
MODULE_LICENSE("GPL v2");
