// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include "ad_device.h"
#include "ad_ambient_light.h"

static int ad_ambient_light_cb(const void *data, void *private)
{
	int ret;
	int al_value;
	struct device *dev = (struct device *)private;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct iio_channel *al_iio_chan = ad_dev->ambient_light.iio_chan;

	ret = iio_read_channel_raw(al_iio_chan, &al_value);
	if (0 > ret) {
		dev_err(dev, "falied to read ALS channel, error:%d.\n", ret);
		return ret;
	}

	if (ad_dev->ambient_light.value != (u16)al_value) {
		ret = ad_dev->ad_dev_funcs->ad_update_ambient_light(
			dev, (u16)al_value);
		if (!ret)
			ad_dev->ambient_light.value = (u16)al_value;
	}

	return ret;
}

int ad_ambient_light_init(struct device *dev)
{
	struct iio_channel *iio_chan;
	struct iio_cb_buffer *iio_cb;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	iio_chan = iio_channel_get_all(dev);
	if (IS_ERR(iio_chan)) {
		dev_err(dev, "falied to get the iio channel.\n");
		return -1;
	}

	iio_cb = iio_channel_get_all_cb(dev, ad_ambient_light_cb, dev);
	if (IS_ERR(iio_cb)) {
		dev_err(dev, "falied to alloc iio callback buffer.\n");
		return -1;
	}

	ad_dev->ambient_light.dev = dev;
	ad_dev->ambient_light.iio_chan = iio_chan;
	ad_dev->ambient_light.iio_cb = iio_cb;
	ad_dev->ambient_light.is_working = false;
	return 0;
}

void ad_ambient_light_term(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	if (ad_dev->ambient_light.iio_chan)
		iio_channel_release_all(ad_dev->ambient_light.iio_chan);

	if (ad_dev->ambient_light.iio_cb)
		iio_channel_release_all_cb(ad_dev->ambient_light.iio_cb);
}

void ad_ambient_light_start_cb(struct device *dev)
{
	int err;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	if (ad_dev->ambient_light.iio_cb &&
		!ad_dev->ambient_light.is_working) {

		err = iio_channel_start_all_cb(ad_dev->ambient_light.iio_cb);
		if (err)
			dev_err(dev, "falied to start callback buffer.\n");
		ad_dev->ambient_light.is_working = true;
	}
}

void ad_ambient_light_stop_cb(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	if (ad_dev->ambient_light.iio_cb &&
		ad_dev->ambient_light.is_working) {

		iio_channel_stop_all_cb(ad_dev->ambient_light.iio_cb);
		ad_dev->ambient_light.is_working = false;
	}
}
