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
#include <linux/completion.h>

#include "ad_regs.h"
#include "../include/ad_coprocessor_defs.h"
#include "ad_ambient_light.h"
#include "ad_backlight.h"

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
	/* Load the ad firmware data. */
	void (*ad_load_firmware)(struct device *dev, const u8 *data,
				 size_t size);
	/* Get all ad registers, set the reg and return the register number. */
	unsigned int (*ad_reg_get_all)(const struct ad_reg **reg);
	/* update the ambient light value */
	int (*ad_update_ambient_light)(struct device *dev, u16 value);
	/* Get the backlight output. */
	unsigned int (*ad_get_backlight)(struct device *dev);
	/* Set the backlight input. */
	int (*ad_set_backlight) (struct device *dev, u32 value);
	/* Save hw change. */
	void (*ad_save_hw_status)(struct device *dev, unsigned int offset,
				  unsigned int value);
	/*Create debugfs for sw params.*/
	void (*ad_create_debugfs_sw)(struct device *dev);
};

struct ad_dev_data {
	u32 ad_version;
	struct ad_dev_funcs *(*identify)(struct device *dev,
				         void __iomem *reg);
	struct ad_coprocessor_funcs *interface_funcs;
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
	struct regmap *ad_regmap;
	struct resource *res;
	struct dentry *ad_debugfs_dir;
	u32 regs_size;
	const struct ad_dev_data *dev_data;
	struct ad_dev_funcs *ad_dev_funcs;
	struct ad_ambient_light ambient_light;
	struct ad_backlight backlight;
	u32 is_enabled : 1;
	void *private_data;
};

extern const struct ad_dev_data ad_products[];

int ad_device_get_resources(struct ad_dev *ad_dev);
#endif /* __AD_DEVICE_H__ */
