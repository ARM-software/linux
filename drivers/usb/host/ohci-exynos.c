/*
 * SAMSUNG EXYNOS USB HOST OHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/platform_data/usb-ohci-exynos.h>
#include <linux/usb/phy.h>
#include <linux/usb/samsung_usb_phy.h>

struct exynos_ohci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
	struct usb_phy *phy;
	struct usb_otg *otg;
	struct exynos4_ohci_platdata *pdata;
	struct notifier_block lpa_nb;
	int power_on;
	unsigned post_lpa_resume:1;
};

static void exynos_ohci_phy_enable(struct exynos_ohci_hcd *exynos_ohci)
{
	struct platform_device *pdev = to_platform_device(exynos_ohci->dev);

	if (exynos_ohci->phy)
		usb_phy_init(exynos_ohci->phy);
	else if (exynos_ohci->pdata && exynos_ohci->pdata->phy_init)
		exynos_ohci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);
}

static void exynos_ohci_phy_disable(struct exynos_ohci_hcd *exynos_ohci)
{
	struct platform_device *pdev = to_platform_device(exynos_ohci->dev);

	if (exynos_ohci->phy)
		usb_phy_shutdown(exynos_ohci->phy);
	else if (exynos_ohci->pdata && exynos_ohci->pdata->phy_exit)
		exynos_ohci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
}

static int ohci_exynos_reset(struct usb_hcd *hcd)
{
	return ohci_init(hcd_to_ohci(hcd));
}

static int ohci_exynos_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret;

	ohci_dbg(ohci, "ohci_exynos_start, ohci:%p", ohci);

	ret = ohci_run(ohci);
	if (ret < 0) {
		dev_err(hcd->self.controller, "can't start %s\n",
			hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM
static int exynos_ohci_bus_resume(struct usb_hcd *hcd)
{
	/* When suspend is failed, re-enable clocks & PHY */
	pm_runtime_resume(hcd->self.controller);

	return ohci_bus_resume(hcd);
}
#else
#define exynos_ohci_bus_resume	NULL
#endif

static const struct hc_driver exynos_ohci_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "EXYNOS OHCI Host Controller",
	.hcd_priv_size		= sizeof(struct ohci_hcd),

	.irq			= ohci_irq,
	.flags			= HCD_MEMORY|HCD_USB11,

	.reset			= ohci_exynos_reset,
	.start			= ohci_exynos_start,
	.stop			= ohci_stop,
	.shutdown		= ohci_shutdown,

	.get_frame_number	= ohci_get_frame,

	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume		= exynos_ohci_bus_resume,
#endif
	.start_port_reset	= ohci_start_port_reset,
};

static ssize_t show_ohci_power(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_ohci_hcd *exynos_ohci = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "EHCI Power %s\n",
			(exynos_ohci->power_on) ? "on" : "off");
}

static ssize_t store_ohci_power(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_ohci_hcd *exynos_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = exynos_ohci->hcd;
	int power_on;
	int irq;
	int retval;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	device_lock(dev);
	if (!power_on && exynos_ohci->power_on) {
		printk(KERN_DEBUG "%s: EHCI turns off\n", __func__);
		pm_runtime_forbid(dev);
		exynos_ohci->power_on = 0;
		usb_remove_hcd(hcd);
		exynos_ohci_phy_disable(exynos_ohci);
	} else if (power_on) {
		printk(KERN_DEBUG "%s: EHCI turns on\n", __func__);
		if (exynos_ohci->power_on) {
			pm_runtime_forbid(dev);
			usb_remove_hcd(hcd);
		} else {
			exynos_ohci_phy_enable(exynos_ohci);
		}

		irq = platform_get_irq(pdev, 0);
		retval = usb_add_hcd(hcd, irq, IRQF_SHARED);
		if (retval < 0) {
			dev_err(dev, "Power On Fail\n");
			goto exit;
		}

		/*
		 * OHCI root hubs are expected to handle remote wakeup.
		 * So, wakeup flag init defaults for root hubs.
		 */
		device_wakeup_enable(&hcd->self.root_hub->dev);

		exynos_ohci->power_on = 1;
		pm_runtime_allow(dev);
	}

exit:
	device_unlock(dev);
	return count;
}
static DEVICE_ATTR(ohci_power, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
	show_ohci_power, store_ohci_power);

static inline int create_ohci_sys_file(struct ohci_hcd *ohci)
{
	return device_create_file(ohci_to_hcd(ohci)->self.controller,
			&dev_attr_ohci_power);
}

static inline void remove_ohci_sys_file(struct ohci_hcd *ohci)
{
	device_remove_file(ohci_to_hcd(ohci)->self.controller,
			&dev_attr_ohci_power);
}

static int exynos_ohci_lpa_event(struct notifier_block *nb,
				 unsigned long event,
				 void *data)
{
	struct exynos_ohci_hcd *exynos_ohci = container_of(nb,
					struct exynos_ohci_hcd, lpa_nb);
	int ret = NOTIFY_OK;

	switch (event) {
	case USB_LPA_PREPARE:
		/*
		 * For the purpose of reducing of power consumption in LPA mode
		 * the PHY should be completely shutdown and reinitialized after
		 * exit from LPA.
		 */
		if (exynos_ohci->phy)
			usb_phy_shutdown(exynos_ohci->phy);

		exynos_ohci->post_lpa_resume = 1;
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return ret;
}

static int exynos_ohci_probe(struct platform_device *pdev)
{
	struct exynos4_ohci_platdata *pdata = pdev->dev.platform_data;
	struct exynos_ohci_hcd *exynos_ohci;
	struct usb_hcd *hcd;
	struct ohci_hcd *ohci;
	struct resource *res;
	struct usb_phy *phy;
	int irq;
	int err;

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	exynos_ohci = devm_kzalloc(&pdev->dev, sizeof(struct exynos_ohci_hcd),
					GFP_KERNEL);
	if (!exynos_ohci)
		return -ENOMEM;

	if (of_device_is_compatible(pdev->dev.of_node,
					"samsung,exynos5440-ohci"))
		goto skip_phy;

	phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
	if (IS_ERR(phy)) {
		/* Fallback to pdata */
		if (!pdata) {
			dev_warn(&pdev->dev, "no platform data or transceiver defined\n");
			return -EPROBE_DEFER;
		} else {
			exynos_ohci->pdata = pdata;
		}
	} else {
		exynos_ohci->phy = phy;
		exynos_ohci->otg = phy->otg;
	}

skip_phy:

	exynos_ohci->dev = &pdev->dev;

	hcd = usb_create_hcd(&exynos_ohci_hc_driver, &pdev->dev,
					dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return -ENOMEM;
	}

	exynos_ohci->hcd = hcd;
	exynos_ohci->clk = devm_clk_get(&pdev->dev, "usbhost");

	if (IS_ERR(exynos_ohci->clk)) {
		dev_err(&pdev->dev, "Failed to get usbhost clock\n");
		err = PTR_ERR(exynos_ohci->clk);
		goto fail_clk;
	}

	err = clk_prepare_enable(exynos_ohci->clk);
	if (err)
		goto fail_clk;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENODEV;
		goto fail_io;
	}

	if (exynos_ohci->otg)
		exynos_ohci->otg->set_host(exynos_ohci->otg,
					&exynos_ohci->hcd->self);

	exynos_ohci_phy_enable(exynos_ohci);

	ohci = hcd_to_ohci(hcd);
	ohci_hcd_init(ohci);

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail_add_hcd;
	}

	platform_set_drvdata(pdev, exynos_ohci);

	if (create_ohci_sys_file(ohci))
		dev_err(&pdev->dev, "Failed to create ehci sys file\n");

	exynos_ohci->lpa_nb.notifier_call = exynos_ohci_lpa_event;
	exynos_ohci->lpa_nb.next = NULL;
	exynos_ohci->lpa_nb.priority = 0;

	err = register_samsung_usb_lpa_notifier(&exynos_ohci->lpa_nb);
	if (err)
		dev_err(&pdev->dev, "Failed to register lpa notifier\n");

	exynos_ohci->power_on = 1;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

fail_add_hcd:
	exynos_ohci_phy_disable(exynos_ohci);
fail_io:
	clk_disable_unprepare(exynos_ohci->clk);
fail_clk:
	usb_put_hcd(hcd);
	return err;
}

static int exynos_ohci_remove(struct platform_device *pdev)
{
	struct exynos_ohci_hcd *exynos_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = exynos_ohci->hcd;

	pm_runtime_disable(&pdev->dev);

	exynos_ohci->power_on = 0;
	unregister_samsung_usb_lpa_notifier(&exynos_ohci->lpa_nb);
	remove_ohci_sys_file(hcd_to_ohci(hcd));

	usb_remove_hcd(hcd);

	if (exynos_ohci->otg)
		exynos_ohci->otg->set_host(exynos_ohci->otg,
					&exynos_ohci->hcd->self);

	exynos_ohci_phy_disable(exynos_ohci);

	clk_disable_unprepare(exynos_ohci->clk);

	usb_put_hcd(hcd);

	return 0;
}

static void exynos_ohci_shutdown(struct platform_device *pdev)
{
	struct exynos_ohci_hcd *exynos_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = exynos_ohci->hcd;

	if (!exynos_ohci->power_on)
		return;

	if (!hcd->rh_registered)
		return;

	/*
	 * OHCI receives clock from the PHY. If PHY is suspended,
	 * SFR cannot be accessed. Here we make sure, that PHY
	 * is not suspended, so shutdown call will complete
	 * successfully.
	 */
	pm_runtime_forbid(&pdev->dev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM_RUNTIME
static int exynos_ohci_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos4_ohci_platdata *pdata = pdev->dev.platform_data;
	struct exynos_ohci_hcd *exynos_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = exynos_ohci->hcd;

	dev_dbg(dev, "%s\n", __func__);

	ohci_suspend(hcd, false);

	if (exynos_ohci->phy)
		pm_runtime_put_sync(exynos_ohci->phy->dev);
	else if (pdata->phy_suspend)
		pdata->phy_suspend(pdev, USB_PHY_TYPE_HOST);

	return 0;
}

static int exynos_ohci_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos4_ohci_platdata *pdata = pdev->dev.platform_data;
	struct exynos_ohci_hcd *exynos_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = exynos_ohci->hcd;

	if (dev->power.is_suspended)
		return 0;

	dev_dbg(dev, "%s\n", __func__);

	if (exynos_ohci->phy) {
		struct usb_phy *phy = exynos_ohci->phy;

		if (exynos_ohci->post_lpa_resume) {
			usb_phy_init(phy);
			exynos_ohci->post_lpa_resume = 0;
		} else {
			pm_runtime_get_sync(phy->dev);
		}
	} else if (pdata->phy_resume) {
		pdata->phy_resume(pdev, USB_PHY_TYPE_HOST);
	}

	ohci_resume(hcd, false);

	return 0;
}
#else
#define exynos_ohci_runtime_suspend	NULL
#define exynos_ohci_runtime_resume	NULL
#endif


#ifdef CONFIG_PM
static int exynos_ohci_suspend(struct device *dev)
{
	struct exynos_ohci_hcd *exynos_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = exynos_ohci->hcd;
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	unsigned long flags;
	int rc = 0;

	/*
	 * Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 */
	spin_lock_irqsave(&ohci->lock, flags);
	if (ohci->rh_state != OHCI_RH_SUSPENDED &&
			ohci->rh_state != OHCI_RH_HALTED) {
		rc = -EINVAL;
		goto fail;
	}

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	if (exynos_ohci->otg)
		exynos_ohci->otg->set_host(exynos_ohci->otg,
					&exynos_ohci->hcd->self);

	exynos_ohci_phy_disable(exynos_ohci);

	clk_disable_unprepare(exynos_ohci->clk);

fail:
	spin_unlock_irqrestore(&ohci->lock, flags);

	return rc;
}

static int exynos_ohci_resume(struct device *dev)
{
	struct exynos_ohci_hcd *exynos_ohci = dev_get_drvdata(dev);
	struct usb_hcd *hcd = exynos_ohci->hcd;

	clk_prepare_enable(exynos_ohci->clk);

	if (exynos_ohci->otg)
		exynos_ohci->otg->set_host(exynos_ohci->otg,
					&exynos_ohci->hcd->self);

	exynos_ohci_phy_enable(exynos_ohci);

	ohci_resume(hcd, false);

	/*Update runtime PM status and clear runtime_error */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#else
#define exynos_ohci_suspend	NULL
#define exynos_ohci_resume	NULL
#endif

static const struct dev_pm_ops exynos_ohci_pm_ops = {
	.suspend	= exynos_ohci_suspend,
	.resume		= exynos_ohci_resume,
	.runtime_suspend	= exynos_ohci_runtime_suspend,
	.runtime_resume		= exynos_ohci_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ohci_match[] = {
	{ .compatible = "samsung,exynos4210-ohci" },
	{ .compatible = "samsung,exynos5440-ohci" },
	{ .compatible = "samsung,exynos5-ohci" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ohci_match);
#endif

static struct platform_driver exynos_ohci_driver = {
	.probe		= exynos_ohci_probe,
	.remove		= exynos_ohci_remove,
	.shutdown	= exynos_ohci_shutdown,
	.driver = {
		.name	= "exynos-ohci",
		.owner	= THIS_MODULE,
		.pm	= &exynos_ohci_pm_ops,
		.of_match_table	= of_match_ptr(exynos_ohci_match),
	}
};

MODULE_ALIAS("platform:exynos-ohci");
MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
