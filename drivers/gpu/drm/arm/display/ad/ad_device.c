// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/version.h>
#include <linux/device.h>
#include "ad_device.h"

const struct ad_dev_data ad_products[] = {
	[AD3] = {
		.ad_version = AD3_VERSION,
		.identify = ad3_identify,
		.interface_funcs = &ad3_intf_funcs
	},
};

int ad_device_get_resources(struct ad_dev *ad_dev)
{
	int ret = 0;
	struct device *dev = ad_dev->dev;
	struct platform_device *pdev = to_platform_device(dev);

	ad_dev->aclk = devm_clk_get(dev, "aclk");
	if (IS_ERR(ad_dev->aclk)) {
		dev_err(dev, "failed to get ad_aclk.\n");
		return PTR_ERR(ad_dev->aclk);
	}

	ad_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	dev_info(dev, "Reg [0x%x, 0x%x]\n", (u32)ad_dev->res->start,
		 (u32)ad_dev->res->end);

	ad_dev->regs_size = resource_size(ad_dev->res);
	ad_dev->regs_base = devm_ioremap_resource(dev, ad_dev->res);

	if (IS_ERR(ad_dev->regs_base)) {
		dev_err(dev, "failed to ioremap\n");
		return PTR_ERR(ad_dev->regs_base);
	}

	clk_prepare_enable(ad_dev->aclk);

	ad_dev->ad_dev_funcs = ad_dev->dev_data->identify(dev,
						          ad_dev->regs_base);
	if (!ad_dev->ad_dev_funcs) {
		dev_err(dev, "failed to identify AD deivce\n");
		clk_disable_unprepare(ad_dev->aclk);
		return -1;
	}

	if (ad_dev->ad_dev_funcs->ad_init)
		ret = ad_dev->ad_dev_funcs->ad_init(dev);

	clk_disable_unprepare(ad_dev->aclk);
	return ret;
}
