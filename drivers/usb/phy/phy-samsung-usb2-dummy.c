/* linux/drivers/usb/phy/phy-samsung-usb2-dummy.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/usb/samsung_usb_phy.h>

#include "phy-samsung-usb.h"

static bool samsung_usb2phy_dummy_is_active(struct usb_phy *phy)
{
	struct samsung_usbphy *sphy = phy_to_sphy(phy);
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&sphy->lock, flags);

	/* Dummy phy is used by DWC3 for OTG */
	if (phy->otg && (phy->state == OTG_STATE_B_PERIPHERAL ||
			 phy->state == OTG_STATE_A_HOST))
		ret = true;
	else
		ret = false;

	spin_unlock_irqrestore(&sphy->lock, flags);

	return ret;
}

static int samsung_usb2phy_dummy_probe(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy;
	const struct samsung_usbphy_drvdata *drv_data;
	struct device *dev = &pdev->dev;

	sphy = devm_kzalloc(dev, sizeof(*sphy), GFP_KERNEL);
	if (!sphy)
		return -ENOMEM;

	drv_data = samsung_usbphy_get_driver_data(pdev);

	sphy->dev = dev;

	sphy->drv_data		= drv_data;
	sphy->phy.dev		= sphy->dev;
	sphy->phy.label		= "samsung-usb2phy-dummy";
	sphy->phy.type		= USB_PHY_TYPE_USB2;
	sphy->phy.is_active	= samsung_usb2phy_dummy_is_active;

	spin_lock_init(&sphy->lock);

	platform_set_drvdata(pdev, sphy);

	return usb_add_phy_dev(&sphy->phy);
}

static int samsung_usb2phy_dummy_remove(struct platform_device *pdev)
{
	struct samsung_usbphy *sphy = platform_get_drvdata(pdev);

	usb_remove_phy(&sphy->phy);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id samsung_usbphy_dt_match[] = {
	{
		.compatible = "samsung,exynos5-usb2phy-dummy",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_usbphy_dt_match);
#endif

static struct platform_device_id samsung_usbphy_dummy_driver_ids[] = {
	{
		.name		= "exynos5-usb2phy-dmy",
	},
	{},
};

MODULE_DEVICE_TABLE(platform, samsung_usbphy_driver_ids);

static struct platform_driver samsung_usb2phy_dummy_driver = {
	.probe		= samsung_usb2phy_dummy_probe,
	.remove		= samsung_usb2phy_dummy_remove,
	.id_table	= samsung_usbphy_dummy_driver_ids,
	.driver		= {
		.name	= "samsung-usb2phy-dummy",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_usbphy_dt_match),
	},
};

module_platform_driver(samsung_usb2phy_dummy_driver);

MODULE_DESCRIPTION("Samsung USB 2.0 phy dummy controller");
MODULE_AUTHOR("Minho Lee <minho55.lee@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:samsung-usb2phy-dummy");
