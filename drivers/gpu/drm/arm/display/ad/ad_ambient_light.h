/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>

#ifndef __AD_AMBIENT_LIGHT_H__
#define __AD_AMBIENT_LIGHT_H__

struct ad_ambient_light {
	struct device *dev;
	struct iio_channel *iio_chan;
	struct iio_cb_buffer *iio_cb;
	u16 value;
	u32 is_working : 1;
};

int ad_ambient_light_init(struct device *dev);
void ad_ambient_light_term(struct device *dev);
void ad_ambient_light_start_cb(struct device *dev);
void ad_ambient_light_stop_cb(struct device *dev);

#endif /* __AD_AMBIENT_LIGHT_H__ */
