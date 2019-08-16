/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#ifndef __AD_REGS_H__
#define __AD_REGS_H__

#include <linux/regmap.h>

struct ad_reg {
	char name[30];
	u32 is_valid: 1,
	    is_writable : 1,
	    is_lut : 1;
	u32 offset;
	u32 mask;
	u32 bits;
	u32 number;
};

struct regmap *ad_register_regmap_init(struct device *dev,
				       void __iomem *regs,
				       const struct regmap_config *config);

int ad_register_regmap_write(struct regmap *regmap,
			     unsigned int offset,
			     unsigned int write_mask,
			     unsigned int val);

int ad_register_regmap_read(struct regmap *regmap,
			    unsigned int offset,
			    unsigned int read_mask,
			    unsigned int *val);
#endif /* __AD_REGS_H__ */
