/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#ifndef __AD_BACKLIGHT_H__
#define __AD_BACKLIGHT_H__
#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/leds.h>

struct ad_backlight {
	struct backlight_device *backlight;
	struct led_trigger led_trigger;
	struct timer_list backlight_timer;
	u32 backlight_output;
	u32 backlight_input;
	u32 trigger_is_active : 1,
		is_working : 1;
};

int ad_backlight_init(struct device *dev);
void ad_backlight_term(struct device *dev);
void ad_backlight_output_start(struct device *dev);
void ad_backlight_output_stop(struct device *dev);
#endif /* __AD_BACKLIGHT_H__ */
