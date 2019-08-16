// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include "ad_regs.h"

struct regmap *ad_register_regmap_init(struct device *dev,
				       void __iomem *regs,
				       const struct regmap_config *config)
{
	return devm_regmap_init_mmio(dev, regs, config);
}

int ad_register_regmap_write(struct regmap *regmap,
			     unsigned int offset,
			     unsigned int write_mask,
			     unsigned int val)
{
	int ret;
	struct device *dev = regmap_get_device(regmap);

	if (val & ~write_mask) {
		dev_err(dev, "Invalid value 0x%x to write reg 0x%x,mask 0x%x.\n",
			val, offset, write_mask);
		return -1;
	}
	ret = regmap_update_bits(regmap, offset, write_mask, val);
	if (ret != 0)
		pr_err("Failed to write reg 0x%x mask 0x%x\n",
		       offset, write_mask);

	return ret;
}

int ad_register_regmap_read(struct regmap *regmap,
			    unsigned int offset,
			    unsigned int read_mask,
			    unsigned int *val)
{
	int ret;
	unsigned int reg_value = 0;

	ret = regmap_read(regmap, offset, &reg_value);
	if (ret != 0) {
		pr_err("Failed to read reg 0x%x mask 0x%x\n",
		       offset, read_mask);
		return ret;
	}

	*val = reg_value & read_mask;

	return ret;
}
