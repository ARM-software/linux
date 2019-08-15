/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#ifndef __AD_DEVICE_H__
#define __AD_DEVICE_H__

#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>

#define AD3_VERSION 0x3

enum {
	AD3 = 0,
};

struct ad_dev_funcs {
	/* Initialize ad such as clock, return 0 if successful, or error. */
	int (*ad_init)(struct device *dev);
	void (*ad_destroy)(struct device *dev);
	/* Save HW status, then runtime suspend the ad device.*/
	void (*ad_runtime_suspend)(struct device *dev);
	/* Runtime resume the ad device, then store HW status. */
	void (*ad_runtime_resume)(struct device *dev);
};

struct ad_dev_data {
	u32 ad_version;
	struct ad_dev_funcs *(*identify)(struct device *dev,
				         void __iomem *reg);
};

struct ad_chip_info {
	u32 arch_id;
	u32 core_id;
};

struct ad_dev {
	char name[32];
	u32 ad_version;
	struct miscdevice miscdev;
	struct device *dev;
	struct ad_chip_info chip_info;
	struct clk *aclk;
	void __iomem *regs_base;
	struct resource *res;
	u32 regs_size;
	const struct ad_dev_data *dev_data;
	struct ad_dev_funcs *ad_dev_funcs;
};

extern const struct ad_dev_data ad_products[];

int ad_device_get_resources(struct ad_dev *ad_dev);
#endif /* __AD_DEVICE_H__ */
