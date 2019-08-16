// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include "ad3_regs.h"

#define AD3_REG_TOTAL_NUM 35

#define AD3_NORMAL_REG(name, reg, is_writable) \
{\
	name,	\
	true,	\
	is_writable,	\
	false,	\
	reg##_OFFSET,	\
	reg##_MASK,	\
	32,	\
	1,	\
}

#define AD3_LUT_REGS(name, reg, is_writable, number) \
{\
	name,	\
	true,	\
	is_writable,	\
	true,	\
	reg##_OFFSET,	\
	reg##_MASK,	\
	32,	\
	number,	\
}

const struct ad_reg ad3_regs[AD3_REG_TOTAL_NUM] = {
	/*Global Control Registers. */
	AD3_NORMAL_REG("ad_arch_id", AD_ARCH_ID_REG, false),
	AD3_NORMAL_REG("ad_core_id", AD_CORE_ID_REG, false),
	AD3_NORMAL_REG("ad_status", AD_STATUS_REG, false),
	AD3_NORMAL_REG("ad_control", AD_CONTROL_REG, true),
	AD3_NORMAL_REG("ad_input_size", AD_INPUT_SIZE_REG, true),
	AD3_NORMAL_REG("ad_dither", AD_DITHER_REG,true),
	AD3_NORMAL_REG("ad_strength",AD_STRENGTH_REG, false),
	AD3_NORMAL_REG("ad_drc", AD_DRC_REG, false),
	AD3_NORMAL_REG("ad_backlight", AD_BACKLIGHT_REG, false),
	/*Iridix Control Registers. */
	AD3_NORMAL_REG("iridix_control", IRIDIX_CONTROL_REG, true),
	AD3_NORMAL_REG("iridix_stats", IRIDIX_STATS_REG, true),
	AD3_NORMAL_REG("iridix_local", IRIDIX_LOCAL_REG, true),
	AD3_NORMAL_REG("iridix_max", IRIDIX_MAX_REG, true),
	AD3_NORMAL_REG("iridix_min", IRIDIX_MIN_REG, true),
	AD3_NORMAL_REG("iridix_black_level", IRIDIX_BLACK_LEVEL_REG, true),
	AD3_NORMAL_REG("iridix_white_level", IRIDIX_WHITE_LEVEL_REG, true),
	AD3_NORMAL_REG("iridix_amp", IRIDIX_AMP_REG, true),
	/*Ambient Light Adaptation Control Registers. */
	AD3_NORMAL_REG("calc_strength", CALC_STRENGTH_REG, true),
	AD3_NORMAL_REG("calc_drc", CALC_DRC_REG, true),
	AD3_NORMAL_REG("calc_light", CALC_LIGHT_REG, true),
	AD3_NORMAL_REG("calc_auto_strength_limit", CALC_AUTO_STRENGTH_LIMIT_REG, true),
	AD3_NORMAL_REG("calc_calib_a", CALC_CALIBRATION_A_REG, true),
	AD3_NORMAL_REG("calc_calib_b", CALC_CALIBRATION_B_REG, true),
	AD3_NORMAL_REG("calc_calib_c", CALC_CALIBRATION_C_REG, true),
	AD3_NORMAL_REG("calc_calib_d", CALC_CALIBRATION_D_REG, true),
	AD3_NORMAL_REG("calc_auto_strength_tfilter", CALC_AUTO_STRENGTH_TFILTER_REG, true),
	AD3_NORMAL_REG("calc_backlight_min", CALC_BACKLIGHT_MIN_REG, true),
	AD3_NORMAL_REG("calc_backlight_max", CALC_BACKLIGHT_MAX_REG, true),
	AD3_NORMAL_REG("calc_backlight_scale", CALC_BACKLIGHT_SCALE_REG, true),
	AD3_NORMAL_REG("calc_ambientlight_min", CALC_AMBIENTLIGHT_MIN_REG, true),
	AD3_NORMAL_REG("calc_ambientlight_tfilter0", CALC_AMBIENTLIGHT_TFILTER0_REG, true),
	AD3_NORMAL_REG("calc_ambientlight_tfilter1", CALC_AMBIENTLIGHT_TFILTER1_REG, true),
	/*Ambient Light Correction Coefficients Table. */
	AD3_LUT_REGS("alcoeff", ALCOEFF_REG, true, AD3_LUT_SIZE),
	/*Iridix Asymeytry Coefficients Table.*/
	AD3_LUT_REGS("asymcoeff", ASYMCOEFF_REG, true, AD3_LUT_SIZE),
	/*Iridix Color Correction Transform Coefficients Table.*/
	AD3_LUT_REGS("colorcoeff", COLORCOEFF_REG, true, AD3_LUT_SIZE),
};

static const struct regmap_config ad3_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0x0580,
};

struct regmap *ad3_register_regmap_init(struct device *dev,
				        void __iomem *regs)
{
	struct regmap *ad_regmap;

	ad_regmap = ad_register_regmap_init(dev,
					    regs,
					    &ad3_regmap_config);
	if (IS_ERR(ad_regmap)) {
		pr_err("ad3 regmap init failed\n");
		return NULL;
	}

	return ad_regmap;
};

unsigned int ad3_register_get_all(const struct ad_reg **reg)
{
	(*reg) = ad3_regs;
	return AD3_REG_TOTAL_NUM;
}
