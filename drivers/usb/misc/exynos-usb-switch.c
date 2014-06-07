/*
 * exynos-usb-switch.c - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_data/exynos-usb-switch.h>
#include <linux/usb/gadget.h>

#include "exynos-usb-switch.h"

#define DRIVER_DESC "Exynos USB Switch Driver"
#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

static const char switch_name[] = "exynos_usb_switch";
static struct exynos_usb_switch *our_switch;

const char *exynos_usbswitch_mode_string(unsigned long mode)
{
	if (!mode)
		return "IDLE";
	else if (test_bit(USB_HOST_ATTACHED, &mode))
		return "USB_HOST_ATTACHED";
	else if (test_bit(USB_DEVICE_ATTACHED, &mode))
		return "USB_DEVICE_ATTACHED";
	else
		/* something wrong */
		return "undefined";
}

static int is_host_detect(struct exynos_usb_switch *usb_switch)
{
	if (!gpio_is_valid(usb_switch->gpio_host_detect))
		return 0;
	return !gpio_get_value(usb_switch->gpio_host_detect);
}

static int is_device_detect(struct exynos_usb_switch *usb_switch)
{
	if (!gpio_is_valid(usb_switch->gpio_device_detect))
		return 0;
	return gpio_get_value(usb_switch->gpio_device_detect);
}

static void set_host_vbus(struct exynos_usb_switch *usb_switch, int value)
{
	if (gpio_is_valid(usb_switch->gpio_host_vbus))
		gpio_set_value(usb_switch->gpio_host_vbus, value);
}

struct usb_gadget *
__attribute__((weak)) s3c_udc_get_gadget(struct device *dev)
{
	return NULL;
}

static int exynos_change_usb_mode(struct exynos_usb_switch *usb_switch,
				enum usb_cable_status mode)
{
	struct usb_gadget *gadget;
	unsigned long cur_mode = usb_switch->connect;
	int ret = 0;

	if (test_bit(USB_DEVICE_ATTACHED, &cur_mode) ||
	    test_bit(USB_HOST_ATTACHED, &cur_mode)) {
		if (mode == USB_DEVICE_ATTACHED ||
			mode == USB_HOST_ATTACHED) {
			printk(KERN_DEBUG "Skip request %d, current %lu\n",
				mode, cur_mode);
			return -EPERM;
		}
	}

	if (!test_bit(USB_DEVICE_ATTACHED, &cur_mode) &&
			mode == USB_DEVICE_DETACHED) {
		printk(KERN_DEBUG "Skip request %d, current %lu\n",
			mode, cur_mode);
		return -EPERM;
	} else if (!test_bit(USB_HOST_ATTACHED, &cur_mode) &&
			mode == USB_HOST_DETACHED) {
		printk(KERN_DEBUG "Skip request %d, current %lu\n",
			mode, cur_mode);
		return -EPERM;
	}

	/*
	 * FIXME: Currently we get gadget every time usb mode is going
	 * to change. This prevents from problems related to UDC module
	 * removing and wrong probe order. S3C UDC driver should provide
	 * corresponding helper function. If S3C UDC is not available weak
	 * analog is used instead.
	 *
	 * Correct solution would be to implement binding similar to OTG's
	 * set_host() and set_peripheral().
	 */
	gadget = s3c_udc_get_gadget(usb_switch->s3c_udc_dev);

	switch (mode) {
	case USB_DEVICE_DETACHED:
		if (test_bit(USB_HOST_ATTACHED, &cur_mode)) {
			printk(KERN_ERR "Abnormal request %d, current %lu\n",
				mode, cur_mode);
			return -EPERM;
		}
		if (gadget && gadget->ops)
			usb_gadget_vbus_disconnect(gadget);
		clear_bit(USB_DEVICE_ATTACHED, &usb_switch->connect);
		break;
	case USB_DEVICE_ATTACHED:
		if (gadget && gadget->ops)
			usb_gadget_vbus_connect(gadget);
		set_bit(USB_DEVICE_ATTACHED, &usb_switch->connect);
		break;
	case USB_HOST_DETACHED:
		if (test_bit(USB_DEVICE_ATTACHED, &cur_mode)) {
			printk(KERN_ERR "Abnormal request %d, current %lu\n",
				mode, cur_mode);
			return -EPERM;
		}
		if (usb_switch->ohci_dev)
			pm_runtime_allow(usb_switch->ohci_dev);
		if (usb_switch->ehci_dev)
			pm_runtime_allow(usb_switch->ehci_dev);
		if (usb_switch->gpio_host_vbus)
			set_host_vbus(usb_switch, 0);

		wake_unlock(&usb_switch->wake_lock);

		clear_bit(USB_HOST_ATTACHED, &usb_switch->connect);
		break;
	case USB_HOST_ATTACHED:
		wake_lock(&usb_switch->wake_lock);

		if (usb_switch->gpio_host_vbus)
			set_host_vbus(usb_switch, 1);

		if (usb_switch->ehci_dev)
			pm_runtime_forbid(usb_switch->ehci_dev);
		if (usb_switch->ohci_dev)
			pm_runtime_forbid(usb_switch->ohci_dev);
		set_bit(USB_HOST_ATTACHED, &usb_switch->connect);
		break;
	default:
		printk(KERN_ERR "Does not changed\n");
	}
	printk(KERN_ERR "usb cable = %d\n", mode);

	return ret;
}

static void exynos_usb_switch_worker(struct work_struct *work)
{
	struct exynos_usb_switch *usb_switch =
		container_of(work, struct exynos_usb_switch, switch_work);
	int cnt = 0;

	mutex_lock(&usb_switch->mutex);
	/* If already device detached or host_detected, */
	if (!is_device_detect(usb_switch) || is_host_detect(usb_switch))
		goto done;
	if (!usb_switch->ehci_dev || !usb_switch->ohci_dev)
		goto detect;

	while (!pm_runtime_suspended(usb_switch->ehci_dev) ||
		!pm_runtime_suspended(usb_switch->ohci_dev)) {

		mutex_unlock(&usb_switch->mutex);
		msleep(SWITCH_WAIT_TIME);
		mutex_lock(&usb_switch->mutex);

		/* If already device detached or host_detected, */
		if (!is_device_detect(usb_switch) || is_host_detect(usb_switch))
			goto done;

		if (cnt++ > WAIT_TIMES) {
			printk(KERN_ERR "%s:device not attached by host\n",
				__func__);
			goto done;
		}

	}

	if (cnt > 1)
		printk(KERN_INFO "Device wait host power during %d\n", (cnt-1));
detect:
	/* Check Device, VBUS PIN high active */
	exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
done:
	mutex_unlock(&usb_switch->mutex);
}

static irqreturn_t exynos_host_detect_thread(int irq, void *data)
{
	struct exynos_usb_switch *usb_switch = data;

	pr_err("%s\n", __func__);

	mutex_lock(&usb_switch->mutex);

	if (is_host_detect(usb_switch))
		exynos_change_usb_mode(usb_switch, USB_HOST_ATTACHED);
	else
		exynos_change_usb_mode(usb_switch, USB_HOST_DETACHED);

	mutex_unlock(&usb_switch->mutex);

	return IRQ_HANDLED;
}

static irqreturn_t exynos_device_detect_thread(int irq, void *data)
{
	struct exynos_usb_switch *usb_switch = data;

	mutex_lock(&usb_switch->mutex);

	/* Debounce connect delay */
	msleep(20);

	if (is_host_detect(usb_switch))
		printk(KERN_DEBUG "Not expected situation\n");
	else if (is_device_detect(usb_switch)) {
		if (usb_switch->gpio_host_vbus)
			exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
		else
			queue_work(usb_switch->workqueue, &usb_switch->switch_work);
	} else {
		/* VBUS PIN low */
		exynos_change_usb_mode(usb_switch, USB_DEVICE_DETACHED);
	}

	mutex_unlock(&usb_switch->mutex);

	return IRQ_HANDLED;
}

static int exynos_usb_status_init(struct exynos_usb_switch *usb_switch)
{
	mutex_lock(&usb_switch->mutex);

	/* 2.0 USB */
	if (is_host_detect(usb_switch))
		exynos_change_usb_mode(usb_switch, USB_HOST_ATTACHED);
	else if (is_device_detect(usb_switch)) {
		if (usb_switch->gpio_host_vbus)
			exynos_change_usb_mode(usb_switch,
				USB_DEVICE_ATTACHED);
		else
			queue_work(usb_switch->workqueue,
				&usb_switch->switch_work);
	}

	mutex_unlock(&usb_switch->mutex);

	return 0;
}

#ifdef CONFIG_PM
static int exynos_usbswitch_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);
	mutex_lock(&usb_switch->mutex);
	if (test_bit(USB_DEVICE_ATTACHED, &usb_switch->connect))
		exynos_change_usb_mode(usb_switch, USB_DEVICE_DETACHED);

	usb_switch->connect = 0;
	mutex_unlock(&usb_switch->mutex);

	return 0;
}

static int exynos_usbswitch_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s\n", __func__);
	exynos_usb_status_init(usb_switch);

	return 0;
}
#else
#define exynos_usbswitch_suspend	NULL
#define exynos_usbswitch_resume		NULL
#endif

/* SysFS interface */

static ssize_t
exynos_usbswitch_show_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_usb_switch *usb_switch = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			exynos_usbswitch_mode_string(usb_switch->connect));
}

static DEVICE_ATTR(mode, S_IRUSR | S_IRGRP,
	exynos_usbswitch_show_mode, NULL);

static ssize_t
exynos_usbswitch_store_id(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct exynos_usb_switch *usb_switch = dev_get_drvdata(dev);
	int id;

	if (sscanf(buf, "%d", &id) != 1)
		return -EINVAL;

	mutex_lock(&usb_switch->mutex);

	if (!id)
		exynos_change_usb_mode(usb_switch, USB_HOST_ATTACHED);
	else
		exynos_change_usb_mode(usb_switch, USB_HOST_DETACHED);

	mutex_unlock(&usb_switch->mutex);

	return n;
}

static DEVICE_ATTR(id, S_IWUSR | S_IRUSR | S_IRGRP,
	NULL, exynos_usbswitch_store_id);

static struct attribute *exynos_usbswitch_attributes[] = {
	&dev_attr_id.attr,
	&dev_attr_mode.attr,
	NULL
};

static const struct attribute_group exynos_usbswitch_attr_group = {
	.attrs = exynos_usbswitch_attributes,
};

static int exynos_usbswitch_parse_dt(struct exynos_usb_switch *usb_switch,
				     struct device *dev)
{
	struct device_node *node;
	struct platform_device *pdev;
	struct pinctrl	*pinctrl;
	int ret;

	/* Host detection */
	usb_switch->gpio_host_detect = of_get_named_gpio(dev->of_node,
					"samsung,id-gpio", 0);
	if (!gpio_is_valid(usb_switch->gpio_host_detect)) {
		dev_info(dev, "host detect gpio is not available\n");
	} else {
		ret = devm_gpio_request(dev, usb_switch->gpio_host_detect,
						"usbswitch_id_gpio");
		if (ret)
			dev_err(dev, "failed to request host detect gpio");
		else
			usb_switch->host_detect_irq =
				gpio_to_irq(usb_switch->gpio_host_detect);
	}

	/* Device detection */
	usb_switch->gpio_device_detect = of_get_named_gpio(dev->of_node,
					"samsung,bsess-gpio", 0);
	if (!gpio_is_valid(usb_switch->gpio_device_detect)) {
		dev_info(dev, "device detect gpio is not available\n");
	} else {
		ret = devm_gpio_request(dev, usb_switch->gpio_device_detect,
						"usbswitch_b_sess_gpio");
		if (ret)
			dev_err(dev, "failed to request host detect gpio");
		else
			usb_switch->device_detect_irq =
				gpio_to_irq(usb_switch->gpio_device_detect);
	}

	/* VBus control */
	usb_switch->gpio_host_vbus = of_get_named_gpio(dev->of_node,
					"samsung,vbus-gpio", 0);
	if (!gpio_is_valid(usb_switch->gpio_host_vbus)) {
		dev_info(dev, "vbus control gpio is not available\n");
	} else {
		ret = devm_gpio_request(dev, usb_switch->gpio_host_vbus,
						"usbswitch_vbus_gpio");
		if (ret)
			dev_err(dev, "failed to request vbus control gpio");
	}

	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl))
		dev_info(dev, "failed to configure pins\n");

	/* EHCI */
	node = of_parse_phandle(dev->of_node, "ehci", 0);
	if (!node) {
		dev_info(dev, "ehci device is not available\n");
	} else {
		pdev = of_find_device_by_node(node);
		if (!pdev)
			dev_err(dev, "failed to find ehci device\n");
		else
			usb_switch->ehci_dev = &pdev->dev;
		of_node_put(node);
	}

	/* OHCI */
	node = of_parse_phandle(dev->of_node, "ohci", 0);
	if (!node) {
		dev_info(dev, "ohci device is not available\n");
	} else {
		pdev = of_find_device_by_node(node);
		if (!pdev)
			dev_err(dev, "failed to find ohci device\n");
		else
			usb_switch->ohci_dev = &pdev->dev;
		of_node_put(node);
	}

	/* UDC */
	node = of_parse_phandle(dev->of_node, "udc", 0);
	if (!node) {
		dev_info(dev, "udc device is not available\n");
	} else {
		pdev = of_find_device_by_node(node);
		if (!pdev)
			dev_err(dev, "failed to find udc device\n");
		else
			usb_switch->s3c_udc_dev = &pdev->dev;
		of_node_put(node);
	}

	return 0;
}

static int exynos_usbswitch_probe(struct platform_device *pdev)
{
	struct s5p_usbswitch_platdata *pdata = dev_get_platdata(&pdev->dev);
	struct device *dev = &pdev->dev;
	struct exynos_usb_switch *usb_switch;
	int ret = 0;

	usb_switch = devm_kzalloc(dev, sizeof(struct exynos_usb_switch),
					GFP_KERNEL);
	if (!usb_switch)
		return -ENOMEM;

	our_switch = usb_switch;
	mutex_init(&usb_switch->mutex);
	wake_lock_init(&usb_switch->wake_lock, WAKE_LOCK_SUSPEND,
			"usb_switch_present");
	usb_switch->workqueue = create_singlethread_workqueue("usb_switch");
	INIT_WORK(&usb_switch->switch_work, exynos_usb_switch_worker);

	if (dev->of_node) {
		ret = exynos_usbswitch_parse_dt(usb_switch, dev);
		if (ret < 0) {
			dev_err(dev, "Failed to parse dt\n");
			goto fail;
		}
	} else if (pdata) {
		usb_switch->gpio_host_detect = pdata->gpio_host_detect;
		usb_switch->gpio_device_detect = pdata->gpio_device_detect;
		usb_switch->gpio_host_vbus = pdata->gpio_host_vbus;

		usb_switch->ehci_dev = pdata->ehci_dev;
		usb_switch->ohci_dev = pdata->ohci_dev;
		usb_switch->s3c_udc_dev = pdata->s3c_udc_dev;

		usb_switch->host_detect_irq = platform_get_irq(pdev, 0);
		usb_switch->device_detect_irq = platform_get_irq(pdev, 1);
	} else {
		dev_err(dev, "Platform data is not available\n");
		ret = -ENODEV;
		goto fail;
	}

	/* USB Device detect IRQ */
	if (usb_switch->device_detect_irq > 0 && usb_switch->s3c_udc_dev) {
		ret = devm_request_threaded_irq(dev,
				usb_switch->device_detect_irq,
				NULL, exynos_device_detect_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "DEVICE_DETECT", usb_switch);
		if (ret) {
			dev_err(dev, "Failed to request device irq %d\n",
					usb_switch->device_detect_irq);
			goto fail;
		}
	} else if (usb_switch->s3c_udc_dev) {
		exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
	} else {
		dev_info(dev, "Disable device detect IRQ\n");
	}

	/* USB Host detect IRQ */
	if (usb_switch->host_detect_irq > 0 && (usb_switch->ehci_dev ||
						usb_switch->ohci_dev)) {
		ret = devm_request_threaded_irq(dev,
				usb_switch->host_detect_irq,
				NULL, exynos_host_detect_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "HOST_DETECT", usb_switch);
		if (ret) {
			dev_err(dev, "Failed to request host irq %d\n",
					usb_switch->host_detect_irq);
			goto fail;
		}
	} else if (usb_switch->ehci_dev || usb_switch->ohci_dev) {
		exynos_change_usb_mode(usb_switch, USB_HOST_ATTACHED);
	} else {
		dev_info(dev, "Disable host detect IRQ\n");
	}

	exynos_usb_status_init(usb_switch);

	ret = sysfs_create_group(&dev->kobj, &exynos_usbswitch_attr_group);
	if (ret)
		dev_warn(dev, "failed to create dwc3 otg attributes\n");

	platform_set_drvdata(pdev, usb_switch);

	return 0;

fail:
	wake_unlock(&usb_switch->wake_lock);
	cancel_work_sync(&usb_switch->switch_work);
	destroy_workqueue(usb_switch->workqueue);
	mutex_destroy(&usb_switch->mutex);
	return ret;
}

static int exynos_usbswitch_remove(struct platform_device *pdev)
{
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, 0);

	sysfs_remove_group(&pdev->dev.kobj, &exynos_usbswitch_attr_group);
	wake_unlock(&usb_switch->wake_lock);
	cancel_work_sync(&usb_switch->switch_work);
	destroy_workqueue(usb_switch->workqueue);
	mutex_destroy(&usb_switch->mutex);

	return 0;
}

static const struct dev_pm_ops exynos_usbswitch_pm_ops = {
	.suspend                = exynos_usbswitch_suspend,
	.resume                 = exynos_usbswitch_resume,
};

static const struct of_device_id of_exynos_usbswitch_match[] = {
	{
		.compatible =	"samsung,exynos-usb-switch"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_usbswitch_match);

static struct platform_driver exynos_usbswitch_driver = {
	.probe		= exynos_usbswitch_probe,
	.remove		= exynos_usbswitch_remove,
	.driver		= {
		.name	= "exynos-usb-switch",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_exynos_usbswitch_match),
		.pm	= &exynos_usbswitch_pm_ops,
	},
};

module_platform_driver(exynos_usbswitch_driver);

MODULE_DESCRIPTION("Exynos USB switch driver");
MODULE_AUTHOR("<yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
