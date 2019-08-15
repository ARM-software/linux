// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include "../ad_device.h"
#include "ad3_device.h"

static void ad3_runtime_suspend(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	clk_disable_unprepare(ad_dev->aclk);
}

static void ad3_runtime_resume(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	clk_prepare_enable(ad_dev->aclk);
}

static int ad3_init(struct device *dev)
{
	return 0;
}

static void ad3_destroy(struct device *dev)
{
	return;
}

static struct ad_dev_funcs ad3_dev_func = {
	.ad_init = ad3_init,
	.ad_destroy = ad3_destroy,
	.ad_runtime_suspend = ad3_runtime_suspend,
	.ad_runtime_resume= ad3_runtime_resume,
};

struct ad_dev_funcs *ad3_identify(struct device *dev,
				    u32 __iomem *reg)
{
	return  &ad3_dev_func;
}
