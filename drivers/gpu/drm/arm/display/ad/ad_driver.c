// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <drm/drm_device.h>
#include <linux/firmware.h>
#include "ad_device.h"
#include "ad_debugfs.h"
#include "ad_ambient_light.h"
#include "ad_backlight.h"

#define AD_NAME "assertive_display"

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int ad_driver_runtime_suspend(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	ad_ambient_light_stop_cb(dev);
	ad_backlight_output_stop(dev);

	ad_dev->ad_dev_funcs->ad_runtime_suspend(dev);

	return 0;
}

static int ad_driver_runtime_resume(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	ad_dev->ad_dev_funcs->ad_runtime_resume(dev);

	ad_ambient_light_start_cb(dev);
	ad_backlight_output_start(dev);

	return 0;
}
#endif

static int ad_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ad_close(struct inode *i, struct file *file)
{
	return 0;
}

static ssize_t
ad_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t
ad_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static long ad_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations ad_fops = {
	.owner   = THIS_MODULE,
	.open    = ad_open,
	.release = ad_close,
	.read    = ad_read,
	.write   = ad_write,
	.unlocked_ioctl = ad_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ad_ioctl,
#endif
};

static int ad_driver_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct ad_list *ad_head = drm->dev_private;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad_coprocessor *ad;

	ad = devm_kzalloc(dev, sizeof(*ad), GFP_KERNEL);
	if (!ad) {
		dev_err(dev, "Fail to create ad coprocessor!\n");
		return -ENOMEM;
	}

	ad->dev = dev;
	ad->funcs = ad_dev->dev_data->interface_funcs;
	list_add_tail(&ad->ad_node, &ad_head->head);

	return 0;
}

static void ad_driver_unbind(struct device *dev, struct device *master,
			     void *data)
{
	struct drm_device *drm = data;
	struct ad_list *ad_head = drm->dev_private;
	struct ad_coprocessor *ad;

	list_for_each_entry(ad, &ad_head->head, ad_node)
		if (ad->dev == dev) {
			list_del(&ad->ad_node);
			devm_kfree(dev, ad);
			break;
		}
	return;
}

static const struct component_ops ad_component_ops = {
	.bind = ad_driver_bind,
	.unbind = ad_driver_unbind,
};

static void ad_request_firmware(struct ad_dev *ad_dev)
{
	int ret;
	const char *fw_name;
	const struct firmware *fw;

	ret = of_property_read_string_index(ad_dev->dev->of_node,
				            "firmware_name",
				            0,
				            &fw_name);

	if(ret) {
		dev_info(ad_dev->dev,
			 "No firmware found, continue with default!\n");
		return;
	}

	ret = request_firmware(&fw, fw_name, ad_dev->dev);

	if (ret) {
		dev_err(ad_dev->dev,
			"Failed to get firmware, continue with default!\n");
		return;
	}

	if (!fw || !fw->data) {
		dev_err(ad_dev->dev,
			"The firmware is invalid, continue with default!\n");
		return;
	}

	clk_prepare_enable(ad_dev->aclk);

	ad_dev->ad_dev_funcs->ad_load_firmware(ad_dev->dev,
					       fw->data,
					       fw->size);

	clk_disable_unprepare(ad_dev->aclk);
	release_firmware(fw);
}

static int ad_probe(struct platform_device *pdev)
{
	int ret, id;
	struct ad_dev *ad_dev;
	struct device *dev = &pdev->dev;

	if (pdev->dev.of_node)
		id = of_alias_get_id(pdev->dev.of_node, "maliad");
	else
		id = pdev->id;
	if (id < 0) {
		dev_err(dev, "Failed to get ID: 0x%x!\n", id);
		return id;
	}

	ad_dev = devm_kzalloc(dev, sizeof(*ad_dev), GFP_KERNEL);
	if (!ad_dev) {
		dev_err(dev, "Failed to alloc ad_dev!\n");
		return -ENOMEM;
	}

	sprintf(ad_dev->name, "assertive_display%d", id);
	ad_dev->dev = dev;
	ad_dev->dev_data = of_device_get_match_data(dev);
	if (!ad_dev->dev_data) {
		dev_err(dev, "No device match found!\n");
		ret = -ENODEV;
		goto match_dev_failed;
	}

	dev_set_drvdata(dev, ad_dev);
	ad_dev->is_enabled = false;

	ret = ad_device_get_resources(ad_dev);
	if (ret) {
		dev_err(dev, "Failed to get resources: 0x%x!\n", ret);
		goto get_res_failed;
	}

	ad_request_firmware(ad_dev);

	ret = ad_ambient_light_init(dev);
	if (ret) {
		dev_err(dev, "Failed to initialize AL!\n");
		goto init_al_failed;
	}
	ad_ambient_light_start_cb(dev);

	ret = ad_backlight_init(dev);
	if (ret) {
		dev_err(dev, "Failed to initialize BL!\n");
		goto init_bl_failed;
	}
	ad_backlight_output_start(dev);

	ad_dev->miscdev.minor	= MISC_DYNAMIC_MINOR;
	ad_dev->miscdev.name	= ad_dev->name;
	ad_dev->miscdev.fops	= &ad_fops;
	ad_dev->miscdev.parent	= dev;

	ret = misc_register(&ad_dev->miscdev);
	if (ret) {
		dev_err(dev, "Failed to register misc device: 0x%x!\n", ret);
		goto reg_misc_failed;
	}

	ad_debugfs_register(ad_dev);

	pm_runtime_enable(dev);
	if (!pm_runtime_enabled(dev)) {
		dev_info(dev, "Continuing without Runtime PM support\n");
		ad_driver_runtime_resume(dev);
		pm_runtime_set_active(dev);
	}

	ret = component_add(ad_dev->dev, &ad_component_ops);
	if (ret) {
		dev_err(dev, "Failed to add component: 0x%x!\n", ret);
		goto add_component_failed;
	}

	return ret;

add_component_failed:
	pm_runtime_disable(dev);
	ad_debugfs_unregister(ad_dev);
	misc_deregister(&ad_dev->miscdev);
reg_misc_failed:
	ad_backlight_output_stop(dev);
	ad_backlight_term(dev);
init_bl_failed:
	ad_ambient_light_stop_cb(dev);
	ad_ambient_light_term(dev);
init_al_failed:
get_res_failed:
	if (ad_dev->ad_dev_funcs && ad_dev->ad_dev_funcs->ad_destroy)
		ad_dev->ad_dev_funcs->ad_destroy(ad_dev->dev);
match_dev_failed:
	devm_kfree(dev, ad_dev);
	return ret;
}

static int ad_remove(struct platform_device *pdev)
{
	struct ad_dev *ad_dev = platform_get_drvdata(pdev);

	component_del(ad_dev->dev, &ad_component_ops);

	if (pm_runtime_enabled(&pdev->dev))
		pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	ad_debugfs_unregister(ad_dev);
	misc_deregister(&ad_dev->miscdev);
	ad_backlight_output_stop(&pdev->dev);
	ad_backlight_term(&pdev->dev);
	ad_ambient_light_stop_cb(&pdev->dev);
	ad_ambient_light_term(&pdev->dev);

	if (ad_dev->ad_dev_funcs && ad_dev->ad_dev_funcs->ad_destroy)
		ad_dev->ad_dev_funcs->ad_destroy(ad_dev->dev);

	devm_kfree(&pdev->dev, ad_dev);

	return 0;
}

static const struct of_device_id ad_match[] = {
	{
		.compatible = "arm,ad-v3",
		.data = &ad_products[AD3],
	},
	{},
};
MODULE_DEVICE_TABLE(of, ad_match);

#ifdef CONFIG_PM_SLEEP
static int ad_driver_pm_suspend_late(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	if (!pm_runtime_status_suspended(dev)) {
		ad_ambient_light_stop_cb(dev);
		ad_backlight_output_stop(dev);

		ad_dev->ad_dev_funcs->ad_runtime_suspend(dev);
	}
	return 0;
}

static int ad_driver_pm_resume_early(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	if (!pm_runtime_status_suspended(dev)) {
		ad_dev->ad_dev_funcs->ad_runtime_resume(dev);

		ad_ambient_light_start_cb(dev);
		ad_backlight_output_start(dev);
	}

	return 0;
}
#endif

static const struct dev_pm_ops ad_pm_ops = {
	SET_RUNTIME_PM_OPS(ad_driver_runtime_suspend,
			   ad_driver_runtime_resume,
			   NULL)
#ifdef CONFIG_PM_SLEEP
	.suspend_late = ad_driver_pm_suspend_late,
	.resume_early = ad_driver_pm_resume_early,
#endif
};

static struct platform_driver ad_driver = {
	.probe		= ad_probe,
	.remove		= ad_remove,
	.driver	= {
		.of_match_table = ad_match,
		.name = AD_NAME,
		.pm   = &ad_pm_ops,
	},
};

module_platform_driver(ad_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luffy Yuan <luffy.yuan@arm.com>");
