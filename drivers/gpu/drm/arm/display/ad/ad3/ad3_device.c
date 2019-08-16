// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include "../ad_device.h"
#include "ad3_device.h"
#include "ad3_regs.h"

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
	u32 arch_id, core_id;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	ad_dev->ad_regmap = ad3_register_regmap_init(dev, reg);
	if (IS_ERR(ad_dev->ad_regmap))
		return NULL;

	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_ARCH_ID_REG_OFFSET,
				AD_ARCH_ID_REG_MASK, &arch_id);

	if (AD3_ARCH_ID != arch_id) {
		dev_err(dev, "The hardware arch id not match.\n");
		return NULL;
	}

	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_CORE_ID_REG_OFFSET,
				AD_CORE_ID_REG_MASK, &core_id);

	dev_info(dev, "arch_id 0x%x, core_id 0x%x\n", arch_id, core_id);

	ad_dev->chip_info.arch_id = arch_id;
	ad_dev->chip_info.core_id = core_id;

	return  &ad3_dev_func;
}
