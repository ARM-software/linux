/*
 * SAMSUNG S5P USB HOST EHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/platform_data/usb-ehci-s5p.h>
#include <linux/usb/phy.h>
#include <linux/usb/samsung_usb_phy.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include "ehci.h"

#define DRIVER_DESC "EHCI s5p driver"

#define EHCI_INSNREG00(base)			(base + 0x90)
#define EHCI_INSNREG00_ENA_INCR16		(0x1 << 25)
#define EHCI_INSNREG00_ENA_INCR8		(0x1 << 24)
#define EHCI_INSNREG00_ENA_INCR4		(0x1 << 23)
#define EHCI_INSNREG00_ENA_INCRX_ALIGN		(0x1 << 22)
#define EHCI_INSNREG00_ENABLE_DMA_BURST	\
	(EHCI_INSNREG00_ENA_INCR16 | EHCI_INSNREG00_ENA_INCR8 |	\
	 EHCI_INSNREG00_ENA_INCR4 | EHCI_INSNREG00_ENA_INCRX_ALIGN)

static const char hcd_name[] = "ehci-s5p";
static struct hc_driver __read_mostly s5p_ehci_hc_driver;

static int (*bus_resume)(struct usb_hcd *) = NULL;

struct s5p_ehci_hcd {
	struct clk *clk;
	struct usb_phy *phy;
	struct usb_otg *otg;
	struct s5p_ehci_platdata *pdata;
	struct notifier_block lpa_nb;
	int power_on;
	unsigned post_lpa_resume:1;
};

#define to_s5p_ehci(hcd)      (struct s5p_ehci_hcd *)(hcd_to_ehci(hcd)->priv)

static void s5p_setup_vbus_gpio(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err;
	int gpio;

	if (!dev->of_node)
		return;

#if !defined(CONFIG_USB_EXYNOS_SWITCH)
	gpio = of_get_named_gpio(dev->of_node, "samsung,vbus-gpio", 0);
	if (!gpio_is_valid(gpio))
		return;

	err = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_HIGH,
				    "ehci_vbus_gpio");
	if (err)
		dev_err(dev, "can't request ehci vbus gpio %d", gpio);
	else
		gpio_set_value(gpio, 1);
#endif

	gpio = of_get_named_gpio(dev->of_node, "samsung,boost5v-gpio", 0);
	if (!gpio_is_valid(gpio))
		return;

	err = devm_gpio_request_one(dev, gpio, GPIOF_OUT_INIT_HIGH,
				    "usb_boost5v_gpio");
	if (err)
		dev_err(dev, "can't request usb boost5v gpio %d", gpio);
	else
		gpio_set_value(gpio, 1);
}

static int s5p_ehci_configurate(struct usb_hcd *hcd)
{
	int delay_count = 0;

	/* This is for waiting phy before ehci configuration */
	do {
		if (readl(hcd->regs))
			break;
		udelay(1);
		++delay_count;
	} while (delay_count < 200);
	if (delay_count)
		dev_info(hcd->self.controller, "phy delay count = %d\n",
			delay_count);

	return 0;
}

static void s5p_ehci_phy_init(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	if (s5p_ehci->phy) {
		usb_phy_init(s5p_ehci->phy);
	} else if (s5p_ehci->pdata->phy_init) {
		s5p_ehci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);
	} else {
		dev_err(&pdev->dev, "Failed to init ehci phy\n");
		return;
	}

	s5p_ehci_configurate(hcd);
}

static ssize_t show_ehci_power(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	return snprintf(buf, PAGE_SIZE, "EHCI Power %s\n",
			(s5p_ehci->power_on) ? "on" : "off");
}

static ssize_t store_ehci_power(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	int power_on;
	int irq;
	int retval;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	device_lock(dev);

	if (!power_on && s5p_ehci->power_on) {
		dev_info(dev, "EHCI turn off\n");
		pm_runtime_forbid(dev);
		s5p_ehci->power_on = 0;
		usb_remove_hcd(hcd);

		if (s5p_ehci->phy)
			usb_phy_shutdown(s5p_ehci->phy);
		else if (s5p_ehci->pdata->phy_exit)
			s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
	} else if (power_on) {
		dev_info(dev, "EHCI turn on\n");
		if (s5p_ehci->power_on) {
			pm_runtime_forbid(dev);
			usb_remove_hcd(hcd);
		} else {
			s5p_ehci_phy_init(pdev);
		}

		irq = platform_get_irq(pdev, 0);
		retval = usb_add_hcd(hcd, irq, IRQF_SHARED);
		if (retval < 0) {
			dev_err(dev, "Power On Fail\n");
			goto exit;
		}

		/*
		 * EHCI root hubs are expected to handle remote wakeup.
		 * So, wakeup flag init defaults for root hubs.
		 */
		device_wakeup_enable(&hcd->self.root_hub->dev);

		s5p_ehci->power_on = 1;
		pm_runtime_allow(dev);
	}
exit:
	device_unlock(dev);
	return count;
}

static DEVICE_ATTR(ehci_power, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
	show_ehci_power, store_ehci_power);

static inline int create_ehci_sys_file(struct ehci_hcd *ehci)
{
	return device_create_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static inline void remove_ehci_sys_file(struct ehci_hcd *ehci)
{
	device_remove_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static int
s5p_ehci_lpa_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct s5p_ehci_hcd *s5p_ehci = container_of(nb,
					struct s5p_ehci_hcd, lpa_nb);
	int ret = NOTIFY_OK;

	switch (event) {
	case USB_LPA_PREPARE:
		/*
		 * For the purpose of reducing of power consumption in LPA mode
		 * the PHY should be completely shutdown and reinitialized after
		 * exit from LPA.
		 */
		if (s5p_ehci->phy)
			usb_phy_shutdown(s5p_ehci->phy);

		s5p_ehci->post_lpa_resume = 1;
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return ret;
}

static int s5p_ehci_probe(struct platform_device *pdev)
{
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
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

	s5p_setup_vbus_gpio(pdev);

	hcd = usb_create_hcd(&s5p_ehci_hc_driver,
			     &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		return -ENOMEM;
	}
	s5p_ehci = to_s5p_ehci(hcd);
	phy = devm_usb_get_phy_by_phandle(&pdev->dev, "usb-phy", 0);
	if (IS_ERR(phy)) {
		/* Fallback to pdata */
		if (!pdata) {
			usb_put_hcd(hcd);
			dev_warn(&pdev->dev, "no platform data or transceiver defined\n");
			return -EPROBE_DEFER;
		} else {
			s5p_ehci->pdata = pdata;
		}
	} else {
		s5p_ehci->phy = phy;
		s5p_ehci->otg = phy->otg;
	}

	s5p_ehci->clk = devm_clk_get(&pdev->dev, "usbhost");

	if (IS_ERR(s5p_ehci->clk)) {
		dev_err(&pdev->dev, "Failed to get usbhost clock\n");
		err = PTR_ERR(s5p_ehci->clk);
		goto fail_clk;
	}

	err = clk_prepare_enable(s5p_ehci->clk);
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

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy)
		usb_phy_init(s5p_ehci->phy);
	else if (s5p_ehci->pdata->phy_init)
		s5p_ehci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;

	/* DMA burst Enable */
	writel(EHCI_INSNREG00_ENABLE_DMA_BURST, EHCI_INSNREG00(hcd->regs));

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail_add_hcd;
	}

	platform_set_drvdata(pdev, hcd);

	if (create_ehci_sys_file(ehci))
		dev_err(&pdev->dev, "Failed to create ehci sys file\n");

	s5p_ehci->lpa_nb.notifier_call = s5p_ehci_lpa_event;
	s5p_ehci->lpa_nb.next = NULL;
	s5p_ehci->lpa_nb.priority = 0;

	err = register_samsung_usb_lpa_notifier(&s5p_ehci->lpa_nb);
	if (err)
		dev_err(&pdev->dev, "Failed to register lpa notifier\n");

	s5p_ehci->power_on = 1;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return 0;

fail_add_hcd:
	if (s5p_ehci->phy)
		usb_phy_shutdown(s5p_ehci->phy);
	else if (s5p_ehci->pdata->phy_exit)
		s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);
fail_io:
	clk_disable_unprepare(s5p_ehci->clk);
fail_clk:
	usb_put_hcd(hcd);
	return err;
}

static int s5p_ehci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);

	pm_runtime_disable(&pdev->dev);

	s5p_ehci->power_on = 0;
	unregister_samsung_usb_lpa_notifier(&s5p_ehci->lpa_nb);
	remove_ehci_sys_file(hcd_to_ehci(hcd));
	usb_remove_hcd(hcd);

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy)
		usb_phy_shutdown(s5p_ehci->phy);
	else if (s5p_ehci->pdata->phy_exit)
		s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);

	clk_disable_unprepare(s5p_ehci->clk);

	usb_put_hcd(hcd);

	return 0;
}

static void s5p_ehci_shutdown(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

#ifdef CONFIG_PM_RUNTIME
static int s5p_ehci_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	bool do_wakeup = device_may_wakeup(dev);
	int rc;

	dev_dbg(dev, "%s\n", __func__);

	rc = ehci_suspend(hcd, do_wakeup);

	if (s5p_ehci->phy)
		pm_runtime_put_sync(s5p_ehci->phy->dev);
	else if (pdata && pdata->phy_suspend)
		pdata->phy_suspend(pdev, USB_PHY_TYPE_HOST);

	return rc;
}

static int s5p_ehci_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	int rc = 0;

	if (dev->power.is_suspended)
		return 0;

	dev_dbg(dev, "%s\n", __func__);

	if (s5p_ehci->phy) {
		struct usb_phy *phy = s5p_ehci->phy;

		if (s5p_ehci->post_lpa_resume)
			usb_phy_init(phy);
		else
			pm_runtime_get_sync(phy->dev);
	} else if (pdata && pdata->phy_resume) {
		rc = pdata->phy_resume(pdev, USB_PHY_TYPE_HOST);
		s5p_ehci->post_lpa_resume = !!rc;
	}

	if (s5p_ehci->post_lpa_resume)
		s5p_ehci_configurate(hcd);

	ehci_resume(hcd, false);

	/*
	 * REVISIT: in case of LPA bus won't be resumed, so we do it here.
	 * Alternatively, we can try to setup HC in such a way that it starts
	 * to sense connections. In this case, root hub will be resumed from
	 * interrupt (ehci_irq()).
	 */
	if (s5p_ehci->post_lpa_resume)
		usb_hcd_resume_root_hub(hcd);

	s5p_ehci->post_lpa_resume = 0;

	return 0;
}
#else
#define s5p_ehci_runtime_suspend	NULL
#define s5p_ehci_runtime_resume		NULL
#endif

#ifdef CONFIG_PM
static int s5p_ehci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	struct platform_device *pdev = to_platform_device(dev);

	bool do_wakeup = device_may_wakeup(dev);
	int rc;

	rc = ehci_suspend(hcd, do_wakeup);

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy)
		usb_phy_shutdown(s5p_ehci->phy);
	else if (s5p_ehci->pdata->phy_exit)
		s5p_ehci->pdata->phy_exit(pdev, USB_PHY_TYPE_HOST);

	clk_disable_unprepare(s5p_ehci->clk);

	return rc;
}

static int s5p_ehci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct  s5p_ehci_hcd *s5p_ehci = to_s5p_ehci(hcd);
	struct platform_device *pdev = to_platform_device(dev);

	clk_prepare_enable(s5p_ehci->clk);

	if (s5p_ehci->otg)
		s5p_ehci->otg->set_host(s5p_ehci->otg, &hcd->self);

	if (s5p_ehci->phy)
		usb_phy_init(s5p_ehci->phy);
	else if (s5p_ehci->pdata->phy_init)
		s5p_ehci->pdata->phy_init(pdev, USB_PHY_TYPE_HOST);

	/* DMA burst Enable */
	writel(EHCI_INSNREG00_ENABLE_DMA_BURST, EHCI_INSNREG00(hcd->regs));

	ehci_resume(hcd, false);

	/* Update runtime PM status and clear runtime_error */
	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}

int s5p_ehci_bus_resume(struct usb_hcd *hcd)
{
	/* When suspend is failed, re-enable clocks & PHY */
	pm_runtime_resume(hcd->self.controller);

	return bus_resume(hcd);
}
#else
#define s5p_ehci_suspend	NULL
#define s5p_ehci_resume		NULL
#define s5p_ehci_bus_resume	NULL
#endif

static const struct dev_pm_ops s5p_ehci_pm_ops = {
	.suspend	= s5p_ehci_suspend,
	.resume		= s5p_ehci_resume,
	.runtime_suspend	= s5p_ehci_runtime_suspend,
	.runtime_resume		= s5p_ehci_runtime_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ehci_match[] = {
	{ .compatible = "samsung,exynos4210-ehci" },
	{ .compatible = "samsung,exynos5-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ehci_match);
#endif

static struct platform_driver s5p_ehci_driver = {
	.probe		= s5p_ehci_probe,
	.remove		= s5p_ehci_remove,
	.shutdown	= s5p_ehci_shutdown,
	.driver = {
		.name	= "s5p-ehci",
		.owner	= THIS_MODULE,
		.pm	= &s5p_ehci_pm_ops,
		.of_match_table = of_match_ptr(exynos_ehci_match),
	}
};
static const struct ehci_driver_overrides s5p_overrides __initdata = {
	.extra_priv_size = sizeof(struct s5p_ehci_hcd),
};

static int __init ehci_s5p_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);
	ehci_init_driver(&s5p_ehci_hc_driver, &s5p_overrides);

	bus_resume = s5p_ehci_hc_driver.bus_resume;
	s5p_ehci_hc_driver.bus_resume = s5p_ehci_bus_resume;

	return platform_driver_register(&s5p_ehci_driver);
}
module_init(ehci_s5p_init);

static void __exit ehci_s5p_cleanup(void)
{
	platform_driver_unregister(&s5p_ehci_driver);
}
module_exit(ehci_s5p_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_ALIAS("platform:s5p-ehci");
MODULE_AUTHOR("Jingoo Han");
MODULE_AUTHOR("Joonyoung Shim");
MODULE_LICENSE("GPL v2");
