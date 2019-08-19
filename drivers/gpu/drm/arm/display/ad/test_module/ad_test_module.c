// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: Jin.Gao <Jin.Gao@arm.com>, Tiannan.Zhu <tiannan.zhu@arm.com>
 *
 */
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/backlight.h>

#define ad_info(fmt, ...) \
	printk(KERN_INFO "[AD Info] " fmt, ##__VA_ARGS__)

/**
 * struct test_module_led_device - state container for virtual based device
 * @cdev: LED class device
 * @dirret: root entry in debugfs for the device
 * @brightness: entry for brightness
 */
struct test_module_led_device {
	struct led_classdev cdev;
	struct dentry *dirret, *brightness;
};

static void virtual_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	led_cdev->brightness = value;
}

static enum led_brightness virtual_led_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int init_debug(struct test_module_led_device *mdev)
{
	mdev->dirret = debugfs_create_dir("ad_test_module", NULL);
	mdev->brightness = debugfs_create_u32("backlight_brightness", 0644,
		mdev->dirret, &mdev->cdev.brightness);
	if (!mdev->brightness) {
		pr_err("error creating int file");
		return (-ENODEV);
	}
	return 0;
}

static void __exit exit_debug(struct test_module_led_device *mdev)
{
	debugfs_remove_recursive(mdev->dirret);
}

static int test_led_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct test_module_led_device *mdev;
	int ret;

	mdev = devm_kzalloc(dev, sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->cdev.name =
		of_get_property(np, "label", NULL) ? : np->name;
	ad_info("led name : %s\n", mdev->cdev.name);
	mdev->cdev.default_trigger =
		of_get_property(np, "linux,default-trigger", NULL);
	ad_info("default-trigger %s\n", mdev->cdev.default_trigger);

	mdev->cdev.brightness_set = virtual_led_set;
	mdev->cdev.brightness_get = virtual_led_get;

	ret = led_classdev_register(dev, &mdev->cdev);
	if (ret < 0) {
		ad_info("led classdev register failed\n");
		return ret;
	}

	platform_set_drvdata(pdev, mdev);
	ad_info("registered LED %s\n", mdev->cdev.name);

	init_debug(mdev);
	return 0;
}

static int test_led_remove(struct platform_device *pdev)
{
	struct test_module_led_device *mdev = platform_get_drvdata(pdev);

	ad_info("%s\n", __func__);
	exit_debug(mdev);
	led_classdev_unregister(&mdev->cdev);

	return 0;
}

/**
 * struct test_als_data - state container for test ambient light sensor
 * @illuminance: ambient light illuminance
 * @int_time: measure integration time
 */
struct test_als_data {
	int illuminance;
	int int_time[2];
};

static int test_als_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	struct test_als_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ad_info("reading als raw data\n");
		*val = data->illuminance;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_INT_TIME:
		ad_info("reading als integration time\n");
		*val  = data->int_time[0];
		*val2 = data->int_time[1];;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}

static int test_als_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     int val, int val2, long mask)
{
	struct test_als_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ad_info("writing als raw data\n");
		data->illuminance = val;
		iio_push_to_buffers(indio_dev, &val);
		return 0;
	case IIO_CHAN_INFO_INT_TIME:
		ad_info("writing als integration time\n");
		data->int_time[0] = val;
		data->int_time[1] = val2;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct iio_info test_als_info = {
	.read_raw  = test_als_read_raw,
	.write_raw = test_als_write_raw,
};

static const struct iio_chan_spec test_als_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_INT_TIME),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 12,
			.storagebits = 16,
			.shift = 4,
			.endianness = IIO_LE,
		},
	}
};

static int test_als_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct device *dev = &pdev->dev;
	struct test_als_data *als_dat;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*als_dat));
	if (!indio_dev)
		return -ENOMEM;

	indio_dev->info = &test_als_info;
	indio_dev->channels = test_als_channels;
	indio_dev->num_channels = ARRAY_SIZE(test_als_channels);
	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE | INDIO_BUFFER_TRIGGERED;

	platform_set_drvdata(pdev, indio_dev);
	ret = iio_device_register(indio_dev);
	if (ret)
		ad_info("test ambient light sensor register failed\n");
	else
		ad_info("test ambient light sensor is loaded\n");
	return ret;

}

static int test_als_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_device_unregister(indio_dev);
	ad_info("test ambient light sensor is removed\n");

	return 0;
}

static int test_backlight_send_intensity(struct backlight_device *bd)
{
	return 0;
}

static const struct backlight_ops test_backlight_ops = {
	.update_status  = test_backlight_send_intensity,
};

static int test_backlight_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct backlight_properties props;
	struct backlight_device *bd;
	const char *name;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 512;
	name = of_get_property(np, "label", NULL) ? : "test-backlight";
	ad_info("backlight name : %s\n", name);
	bd = devm_backlight_device_register(&pdev->dev, name, &pdev->dev,
					NULL, &test_backlight_ops, &props);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	platform_set_drvdata(pdev, bd);

	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.brightness = 0;
	backlight_update_status(bd);

	ad_info("Ad Test Backlight Driver Initialized.\n");
	return 0;
}

static int test_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);

	bd->props.power = 0;
	bd->props.brightness = 0;
	backlight_update_status(bd);

	devm_backlight_device_unregister(&pdev->dev, bd);
	ad_info("Ad Test Backlight Driver Unloaded\n");
	return 0;
}

static int test_module_probe(struct platform_device *pdev)
{
	int ret;

	if (of_device_is_compatible(pdev->dev.of_node, "arm,ad-test-led"))
		ret = test_led_probe(pdev);
	else if (of_device_is_compatible(pdev->dev.of_node, "arm,ad-test-als"))
		ret = test_als_probe(pdev);
	else
		ret = test_backlight_probe(pdev);

	return ret;
}

static int test_module_remove(struct platform_device *pdev)
{
	if (of_device_is_compatible(pdev->dev.of_node, "arm,ad-test-led"))
		test_led_remove(pdev);
	else if (of_device_is_compatible(pdev->dev.of_node, "arm,ad-test-als"))
		test_als_remove(pdev);
	else
		test_backlight_remove(pdev);

	return 0;
}

static const struct of_device_id of_test_module_match[] = {
	{ .compatible = "arm,ad-test-led", },
	{ .compatible = "arm,ad-test-als", },
	{ .compatible = "arm,ad-test-backlight", },
	{},
};

static struct platform_driver test_module_driver = {
	.probe		= test_module_probe,
	.remove		= test_module_remove,
	.driver		= {
		.name	= "ad-test-module",
		.of_match_table = of_test_module_match,
	},
};
module_platform_driver(test_module_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Komeda ad3 test module");
