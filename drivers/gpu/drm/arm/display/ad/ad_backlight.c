// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/moduleparam.h>
#include <linux/pm_runtime.h>

#include "ad_device.h"
#include "ad_backlight.h"

/*define the time for AD backlight timer: ms*/
#define AD_BACKLIGHT_TIMEOUT		16

#define ad_dev_from_backlight(backlight)	\
	container_of(backlight, struct ad_dev, backlight)

#define ad_backlight_from_trig(led_trigger)	\
	container_of(led_trigger, struct ad_backlight, led_trigger)

static ssize_t ad_backlight_input_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad_backlight *ad_backlight = &ad_dev->backlight;

	return sprintf(buf, "%u\n", ad_backlight->backlight_input);
}

static ssize_t ad_backlight_input_set(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t size)
{
	int ret;
	u32 value;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad_backlight *ad_backlight = &ad_dev->backlight;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		return ret;

	ret = ad_dev->ad_dev_funcs->ad_set_backlight(dev, value);
	if (!ret)
		ad_backlight->backlight_input = value;
	return size;
}

static DEVICE_ATTR(backlight_input, S_IRUGO|S_IWUSR,
		ad_backlight_input_show, ad_backlight_input_set);

static int ad_backlight_output_trig_activate(struct led_classdev *led_cdev)
{
	struct led_trigger *led_trigger = led_cdev->trigger;
	struct ad_backlight *ad_backlight = ad_backlight_from_trig(led_trigger);

	ad_backlight->trigger_is_active = true;
	return 0;
}

static void ad_backlight_output_trig_deactivate(struct led_classdev *led_cdev)
{
	struct led_trigger *led_trigger = led_cdev->trigger;
	struct ad_backlight *ad_backlight = ad_backlight_from_trig(led_trigger);

	ad_backlight->trigger_is_active = false;
}

static int led_device_set_brightness(struct ad_dev *ad_dev, u32 brightness)
{
	int ret = 0;
	struct ad_backlight *backlight = &ad_dev->backlight;
	struct led_trigger *led_trigger = &backlight->led_trigger;
	if (!backlight->trigger_is_active) {
		ret = led_trigger_register(led_trigger);
		if (!ret) {
			dev_err(ad_dev->dev, "failed to register the led trigger!\n");
			return ret;
		}
	}

	led_trigger_event(led_trigger, brightness);
	return 0;
}

static void ad_backlight_timer_callback(struct timer_list *t)
{
	int ret;
	u32 backlight_output;
	struct ad_backlight *backlight = from_timer(backlight, t, backlight_timer);
	struct ad_dev *ad_dev = ad_dev_from_backlight(backlight);

	if (0 >= pm_runtime_get_if_in_use(ad_dev->dev))
		goto mod_timer;

	backlight_output = (u32)ad_dev->ad_dev_funcs->ad_get_backlight(ad_dev->dev);

	if (backlight->backlight_output != backlight_output) {
		if (ad_dev->backlight.backlight)
			ret = backlight_device_set_brightness(backlight->backlight,
						backlight_output);
		else
			ret = led_device_set_brightness(ad_dev, backlight_output);

		if (ret)
			dev_err(ad_dev->dev,
				"Failed to set the brightness, err %d !\n", ret);
		else
			backlight->backlight_output = backlight_output;
	}

	pm_runtime_put(ad_dev->dev);

mod_timer:
	mod_timer(&backlight->backlight_timer, jiffies + AD_BACKLIGHT_TIMEOUT);
}

int ad_backlight_init(struct device *dev)
{
	int ret;
	struct backlight_device *backlight;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	ret = device_create_file(dev, &dev_attr_backlight_input);
	if (ret) {
		dev_err(dev, "failed to create sys file for backlight input!\n");
		return ret;
	}

	/* Parse the backlight from DT*/
	backlight = devm_of_find_backlight(dev);
	if (IS_ERR_OR_NULL(backlight)) {
		struct led_trigger *led_trigger = &ad_dev->backlight.led_trigger;
		dev_info(dev,
			 "No BL device in DT, try to use led trigger instead!\n");
		ret = of_property_read_string(dev->of_node,
					"led_trigger",
					&led_trigger->name);
		if (ret) {
			dev_err(dev,
				"failed to initialize the backlight support!\n");
			return ret;
		}
		led_trigger->activate = ad_backlight_output_trig_activate;
		led_trigger->deactivate = ad_backlight_output_trig_deactivate;

		ret = led_trigger_register(led_trigger);
		if (ret) {
			dev_err(dev, "failed to register the led trigger!\n");
			return ret;
		}
	} else
		ad_dev->backlight.backlight = backlight;

	timer_setup(&ad_dev->backlight.backlight_timer,
		    ad_backlight_timer_callback, 0);
	ad_dev->backlight.is_working = false;
	return 0;
}

void ad_backlight_term(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct led_trigger *led_trigger = &ad_dev->backlight.led_trigger;

	device_remove_file(dev, &dev_attr_backlight_input);
	if (led_trigger->name)
		led_trigger_unregister(led_trigger);
}

void ad_backlight_output_start(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct led_trigger *led_trigger = &ad_dev->backlight.led_trigger;

	if ((ad_dev->backlight.backlight || led_trigger->name) &&
		!ad_dev->backlight.is_working) {
		ad_dev->backlight.is_working = true;
		mod_timer(&ad_dev->backlight.backlight_timer,
				jiffies + AD_BACKLIGHT_TIMEOUT);
	}
}

void ad_backlight_output_stop(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct led_trigger *led_trigger = &ad_dev->backlight.led_trigger;

	if ((ad_dev->backlight.backlight || led_trigger->name) &&
		ad_dev->backlight.is_working) {
		ad_dev->backlight.is_working = false;
		del_timer(&ad_dev->backlight.backlight_timer);
	}
}
