// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include "../ad_device.h"
#include "ad3_firmware.h"

static const u16 alcoeff_default_val[AD3_LUT_SIZE] = {
	0x0, 0x800, 0x1000, 0x1800, 0x2000, 0x2800,
	0x3000, 0x3800, 0x4000, 0x4800, 0x5000, 0x5800,
	0x6000, 0x6800, 0x7000, 0x7800, 0x8000, 0x87ff,
	0x8fff, 0x97ff, 0x9fff, 0xa7ff, 0xafff, 0xb7ff,
	0xbfff, 0xc7ff, 0xcfff, 0xd7ff, 0xdfff, 0xe7ff,
	0xefff, 0xf7ff, 0xffff
};

static const u16 asymcoeff_default_val[AD3_LUT_SIZE] = {
	0x0, 0xd3, 0x19e, 0x261, 0x31c, 0x3cf,
	0x47c, 0x523, 0x5c3, 0x65e, 0x6f3, 0x782,
	0x80d, 0x893, 0x915, 0x993, 0xa0c, 0xa82,
	0xaf4, 0xb63, 0xbce, 0xc36, 0xc9b, 0xcfd,
	0xd5c, 0xdb9, 0xe13, 0xe6b, 0xec0, 0xf13,
	0xf64, 0xfb3, 0xfff
};

static const u16 colorcoeff_default_val[AD3_LUT_SIZE] = {
	0xff, 0x116, 0x12e, 0x146, 0x15e, 0x176,
	0x18e, 0x1a6, 0x1be, 0x1d6, 0x1ee, 0x205,
	0x21d, 0x235, 0x24d, 0x265, 0x27d, 0x295,
	0x2ac, 0x2c4, 0x2dc, 0x2f3, 0x30b, 0x323,
	0x33a, 0x352, 0x36a, 0x381, 0x399, 0x3b1,
	0x3c8, 0x3e0, 0x3f8
};

static int ad3_firmware_load_global_data(struct ad_dev *ad_dev,
					 struct ad3_global_data *global_data)
{
	int ret = 0;
	int h_size, v_size;
	struct device *dev = ad_dev->dev;
	/* Load ad control register*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       AD_CONTROL_REG_OFFSET,
				       AD_CONTROL_REG_MASK,
				       global_data->ad_control);
	if (ret)
		return ret;
	h_size = global_data->ad_input_size & AD_INPUT_SIZE_H_MASK;
	if (h_size > AD_INPUT_SIZE_H_MAX || h_size < AD_INPUT_SIZE_H_MIN) {
		dev_err(dev, "the h_szie %d is out of AD HW range %d ~ %d!\n",
			h_size, AD_INPUT_SIZE_H_MIN, AD_INPUT_SIZE_H_MAX);
		return -1;
	}
	v_size = global_data->ad_input_size >> AD_INPUT_SIZE_V_SHIFT;
	if (v_size > AD_INPUT_SIZE_V_MAX || v_size < AD_INPUT_SIZE_V_MIN) {
		dev_err(dev, "the v_szie %d is out of AD HW range %d ~ %d!\n",
			v_size, AD_INPUT_SIZE_V_MIN, AD_INPUT_SIZE_V_MAX);
		return -1;
	}
	/* Load ad input buffer size*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       AD_INPUT_SIZE_REG_OFFSET,
				       AD_INPUT_SIZE_REG_MASK,
				       global_data->ad_input_size);
	if (ret)
		return ret;
	/* Load ad dither*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       AD_DITHER_REG_OFFSET,
				       AD_DITHER_REG_MASK,
				       global_data->ad_dither);

	return ret;
}

static int ad3_firmware_load_iridix_data(
				         struct ad_dev *ad_dev,
				         struct ad3_iridix_data *iridix_data)
{
	int ret = 0;
	/*Load iridix control register*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_CONTROL_REG_OFFSET,
				       IRIDIX_CONTROL_REG_MASK,
				       iridix_data->iridix_control);
	if (ret)
		return ret;
	/*Load iridix stats*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_STATS_REG_OFFSET,
				       IRIDIX_STATS_REG_MASK,
				       iridix_data->iridix_stats);
	if (ret)
		return ret;
	/*Load iridix local variance*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_LOCAL_REG_OFFSET,
				       IRIDIX_LOCAL_REG_MASK,
				       iridix_data->iridix_local);
	if (ret)
		return ret;
	/*Load iridix max value*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_MAX_REG_OFFSET,
				       IRIDIX_MAX_REG_MASK,
				       iridix_data->iridix_max);
	if (ret)
		return ret;
	/*Load iridix min value*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_MIN_REG_OFFSET,
				       IRIDIX_MIN_REG_MASK,
				       iridix_data->iridix_min);
	if (ret)
		return ret;
	/*Load iridix black level*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_BLACK_LEVEL_REG_OFFSET,
				       IRIDIX_BLACK_LEVEL_REG_MASK,
				       iridix_data->iridix_black_level);
	if (ret)
		return ret;
	/*Load iridix white level*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_WHITE_LEVEL_REG_OFFSET,
				       IRIDIX_WHITE_LEVEL_REG_MASK,
				       iridix_data->iridix_white_level);
	if (ret)
		return ret;
	/*Load iridix AMP*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       IRIDIX_AMP_REG_OFFSET,
				       IRIDIX_AMP_REG_MASK,
				       iridix_data->iridix_amp);
	return ret;
}

static int ad3_firmware_load_calc_data(
				       struct ad_dev *ad_dev,
				       struct ad3_calc_data *calc_data)
{
	int ret =0;
	struct device *dev = ad_dev->dev;

	if (calc_data->calc_strength < AD_STRENGTH_MIN) {
		dev_err(dev,
			"the calc_strength %d is less than AD min strength %d!\n",
			calc_data->calc_strength, AD_STRENGTH_MIN);
		return -1;
	}

	/*Load calculator strength*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_STRENGTH_REG_OFFSET,
				       CALC_STRENGTH_REG_MASK,
				       calc_data->calc_strength);
	if (ret)
		return ret;
	/*Load calculator DRC*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_DRC_REG_OFFSET,
				       CALC_DRC_REG_MASK,
				       calc_data->calc_drc);
	if (ret)
		return ret;
	/*Load calculator light*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_LIGHT_REG_OFFSET,
				       CALC_LIGHT_REG_MASK,
				       calc_data->calc_light);
	if (ret)
		return ret;
	/*Load calculator auto strength limit*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_AUTO_STRENGTH_LIMIT_REG_OFFSET,
				       CALC_AUTO_STRENGTH_LIMIT_REG_MASK,
				       calc_data->calc_auto_strength_limit);
	if (ret)
		return ret;
	/*Load calculator calibration A*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_CALIBRATION_A_REG_OFFSET,
				       CALC_CALIBRATION_A_REG_MASK,
				       calc_data->calc_calibration_a);
	if (ret)
		return ret;
	/*Load calculator calibration B*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_CALIBRATION_B_REG_OFFSET,
				       CALC_CALIBRATION_B_REG_MASK,
				       calc_data->calc_calibration_b);
	if (ret)
		return ret;
	/*Load calculator calibration C*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_CALIBRATION_C_REG_OFFSET,
				       CALC_CALIBRATION_C_REG_MASK,
				       calc_data->calc_calibration_c);
	if (ret)
		return ret;
	/*Load calculator calibration D*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_CALIBRATION_D_REG_OFFSET,
				       CALC_CALIBRATION_D_REG_MASK,
				       calc_data->calc_calibration_d);
	if (ret)
		return ret;
	/*Load calculator auto strength tfilter*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_AUTO_STRENGTH_TFILTER_REG_OFFSET,
				       CALC_AUTO_STRENGTH_TFILTER_REG_MASK,
				       calc_data->calc_auto_strength_tfilter);
	if (ret)
		return ret;
	/*Load calculator backlight min value*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_BACKLIGHT_MIN_REG_OFFSET,
				       CALC_BACKLIGHT_MIN_REG_MASK,
				       calc_data->calc_backlight_min);
	if (ret)
		return ret;
	/*Load calculator backlight max value*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_BACKLIGHT_MAX_REG_OFFSET,
				       CALC_BACKLIGHT_MAX_REG_MASK,
				       calc_data->calc_backlight_max);
	if (ret)
		return ret;
	/*Load calculator backlight scale value*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				CALC_BACKLIGHT_SCALE_REG_OFFSET,
				CALC_BACKLIGHT_SCALE_REG_MASK,
				calc_data->calc_backlight_scale);
	if (ret)
		return ret;
	/*Load calculator ambientlight min value*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_AMBIENTLIGHT_MIN_REG_OFFSET,
				       CALC_AMBIENTLIGHT_MIN_REG_MASK,
				       calc_data->calc_ambientlight_min);
	if (ret)
		return ret;
	/*Load calculator ambientlight tfilter0*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_AMBIENTLIGHT_TFILTER0_REG_OFFSET,
				       CALC_AMBIENTLIGHT_TFILTER0_REG_MASK,
				       calc_data->calc_ambientlight_tfilter0);
	if (ret)
		return ret;
	/*Load calculator ambientlight tfilter1*/
	ret = ad_register_regmap_write(ad_dev->ad_regmap,
				       CALC_AMBIENTLIGHT_TFILTER1_REG_OFFSET,
				       CALC_AMBIENTLIGHT_TFILTER1_REG_MASK,
				       calc_data->calc_ambientlight_tfilter1);
	return ret;
}

static int ad3_firmware_load_hw_luts(
				     struct ad_dev *ad_dev,
				     struct ad3_firmware_data *data)
{
	int i, ret = 0;
	u32 offset = 0;

	for (i = 0; i < AD3_LUT_SIZE; i++) {
		/*Load ambient light correction coeffcients table*/
		ret = ad_register_regmap_write(ad_dev->ad_regmap,
				               ALCOEFF_REG_OFFSET + offset,
				               ALCOEFF_REG_MASK,
				               data->alcoeff[i]);
		if (ret)
			return ret;
		/*Load ambient asymeytry coeffcients table*/
		ret = ad_register_regmap_write(ad_dev->ad_regmap,
				               ASYMCOEFF_REG_OFFSET + offset,
				               ASYMCOEFF_REG_MASK,
				               data->asymcoeff[i]);
		if (ret)
			return ret;
		/*Load iridix color correction transform coeffcients table*/
		ret = ad_register_regmap_write(ad_dev->ad_regmap,
				               COLORCOEFF_REG_OFFSET + offset,
				               COLORCOEFF_REG_MASK,
				               data->colorcoeff[i]);
		if (ret)
			return ret;
		offset += AD3_REG_BITS >> 3;
	}
	return ret;
}

static void ad3_firmware_set_global_data(struct device *dev,
				         struct ad3_global_data *global_data,
				         unsigned int offset,
				         unsigned int value)
{
	switch (offset) {
		case AD_CONTROL_REG_OFFSET:
			global_data->ad_control = value;
			break;
		case AD_INPUT_SIZE_REG_OFFSET:
			global_data->ad_input_size = value;
			break;
		case AD_DITHER_REG_OFFSET:
			global_data->ad_dither = value;
			break;
		default:
			dev_err(dev, "Invalid global register to save!\n");
	}
}

static void ad3_firmware_set_iridix_data(struct device *dev,
				         struct ad3_iridix_data *iridix_data,
				         unsigned int offset,
				         unsigned int value)
{
	switch (offset) {
		case IRIDIX_CONTROL_REG_OFFSET:
			iridix_data->iridix_control = value;
			break;
		case IRIDIX_STATS_REG_OFFSET:
			iridix_data->iridix_stats = value;
			break;
		case IRIDIX_LOCAL_REG_OFFSET:
			iridix_data->iridix_local = value;
			break;
		case IRIDIX_MAX_REG_OFFSET:
			iridix_data->iridix_max = value;
			break;
		case IRIDIX_MIN_REG_OFFSET:
			iridix_data->iridix_min = value;
			break;
		case IRIDIX_BLACK_LEVEL_REG_OFFSET:
			iridix_data->iridix_black_level = value;
			break;
		case IRIDIX_WHITE_LEVEL_REG_OFFSET:
			iridix_data->iridix_white_level = value;
			break;
		case IRIDIX_AMP_REG_OFFSET:
			iridix_data->iridix_amp = value;
			break;
		default:
			dev_err(dev, "Invalid iridix register to save!\n");
	}
}

static void ad3_firmware_set_calc_data(struct device *dev,
				       struct ad3_calc_data *calc_data,
				       unsigned int offset,
				       unsigned int value)
{
	switch (offset) {
		case CALC_STRENGTH_REG_OFFSET:
			calc_data->calc_strength = value;
			break;
		case CALC_DRC_REG_OFFSET:
			calc_data->calc_drc =value;
			break;
		case CALC_LIGHT_REG_OFFSET:
			calc_data->calc_light = value;
			break;
		case CALC_AUTO_STRENGTH_LIMIT_REG_OFFSET:
			calc_data->calc_auto_strength_limit = value;
			break;
		case CALC_CALIBRATION_A_REG_OFFSET:
			calc_data->calc_calibration_a = value;
			break;
		case CALC_CALIBRATION_B_REG_OFFSET:
			calc_data->calc_calibration_b = value;
			break;
		case CALC_CALIBRATION_C_REG_OFFSET:
			calc_data->calc_calibration_c = value;
			break;
		case CALC_CALIBRATION_D_REG_OFFSET:
			calc_data->calc_calibration_d = value;
			break;
		case CALC_AUTO_STRENGTH_TFILTER_REG_OFFSET:
			calc_data->calc_auto_strength_tfilter = value;
			break;
		case CALC_BACKLIGHT_MIN_REG_OFFSET:
			calc_data->calc_backlight_min = value;
			break;
		case CALC_BACKLIGHT_MAX_REG_OFFSET:
			calc_data->calc_backlight_max = value;
			break;
		case CALC_BACKLIGHT_SCALE_REG_OFFSET:
			calc_data->calc_backlight_scale = value;
			break;
		case CALC_AMBIENTLIGHT_MIN_REG_OFFSET:
			calc_data->calc_ambientlight_min = value;
			break;
		case CALC_AMBIENTLIGHT_TFILTER0_REG_OFFSET:
			calc_data->calc_ambientlight_tfilter0 = value;
			break;
		case CALC_AMBIENTLIGHT_TFILTER1_REG_OFFSET:
			calc_data->calc_ambientlight_tfilter1 = value;
			break;
		default:
			dev_err(dev, "Invalid calculator register to save!\n");
	}
}

void ad3_firmware_data_reset(struct device *dev,
			     struct ad3_firmware_data *data)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	data->firmware_version = AD3_FIRMWARE_VERSION;
	data->arch_id = ad_dev->chip_info.arch_id;
	/*Reset AD3 global registers as default*/
	data->global_data.ad_control = AD_CONTROL_REG_DEFAULT_VAL;
	data->global_data.ad_input_size = AD_INPUT_SIZE_REG_DEFAULT_VAL;
	data->global_data.ad_dither = AD_DITHER_REG_DEFAULT_VAL;

	ad3_firmware_load_global_data(ad_dev, &data->global_data);

	/* Reset AD3 iridix registers as default*/
	data->iridix_data.iridix_control = IRIDIX_CONTROL_REG_DEFAULT_VAL;
	data->iridix_data.iridix_stats = IRIDIX_STATS_REG_DEFAULT_VAL;
	data->iridix_data.iridix_local = IRIDIX_LOCAL_REG_DEFAULT_VAL;
	data->iridix_data.iridix_max = IRIDIX_MAX_REG_DEFAULT_VAL;
	data->iridix_data.iridix_min = IRIDIX_MIN_REG_DEFAULT_VAL;
	data->iridix_data.iridix_black_level = IRIDIX_BLACK_LEVEL_REG_DEFAULT_VAL;
	data->iridix_data.iridix_white_level = IRIDIX_WHITE_LEVEL_REG_DEFAULT_VAL;
	data->iridix_data.iridix_amp = IRIDIX_AMP_REG_DEFAULT_VAL;
	ad3_firmware_load_iridix_data(ad_dev, &data->iridix_data);

	/*Reset AD3 calculator regisers as default*/
	data->calc_data.calc_strength = CALC_STRENGTH_REG_DEFAULT_VAL;
	data->calc_data.calc_drc = CALC_DRC_REG_DEFAULT_VAL;
	data->calc_data.calc_light = CALC_LIGHT_REG_DEFAULT_VAL;
	data->calc_data.calc_auto_strength_limit = CALC_AUTO_STRENGTH_LIMIT_REG_DEFAULT_VAL;
	data->calc_data.calc_calibration_a = CALC_CALIBRATION_A_REG_DEFAULT_VAL;
	data->calc_data.calc_calibration_b = CALC_CALIBRATION_B_REG_DEFAULT_VAL;
	data->calc_data.calc_calibration_c = CALC_CALIBRATION_C_REG_DEFAULT_VAL;
	data->calc_data.calc_calibration_d = CALC_CALIBRATION_D_REG_DEFAULT_VAL;
	data->calc_data.calc_auto_strength_tfilter = CALC_AUTO_STRENGTH_TFILTER_REG_DEFAULT_VAL;
	data->calc_data.calc_backlight_min = CALC_BACKLIGHT_MIN_REG_DEFAULT_VAL;
	data->calc_data.calc_backlight_max = CALC_BACKLIGHT_MAX_REG_DEFAULT_VAL;
	data->calc_data.calc_backlight_scale = CALC_BACKLIGHT_SCALE_REG_DEFAULT_VAL;
	data->calc_data.calc_ambientlight_min = CALC_AMBIENTLIGHT_MIN_REG_DEFAULT_VAL;
	data->calc_data.calc_ambientlight_tfilter0 = CALC_AMBIENTLIGHT_TFILTER0_REG_DEFAULT_VAL;
	data->calc_data.calc_ambientlight_tfilter1 = CALC_AMBIENTLIGHT_TFILTER1_REG_REG_DEFAULT_VAL;
	ad3_firmware_load_calc_data(ad_dev, &data->calc_data);

	/* Reset AD3 HW coefficients tables as default*/
	memcpy(data->alcoeff, alcoeff_default_val, sizeof(data->alcoeff));
	memcpy(data->asymcoeff, asymcoeff_default_val, sizeof(data->asymcoeff));
	memcpy(data->colorcoeff, colorcoeff_default_val, sizeof(data->colorcoeff));
	ad3_firmware_load_hw_luts(ad_dev, data);
}

void ad3_firmware_load_data(struct device *dev,
		            struct ad3_firmware_data *data)
{
	int ret = 0;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	/*Load AD3 global registers*/
	ret = ad3_firmware_load_global_data(ad_dev, &data->global_data);
	if (ret)
		goto firmware_reset;
	/* Load AD3 iridix registers*/
	ret = ad3_firmware_load_iridix_data(ad_dev, &data->iridix_data);
	if (ret)
		goto firmware_reset;
	/* Load AD3 calculator regisers*/
	ret = ad3_firmware_load_calc_data(ad_dev, &data->calc_data);
	if (ret)
		goto firmware_reset;
	/* Load AD3 HW coefficients tables*/
	ret = ad3_firmware_load_hw_luts(ad_dev, data);
	if (!ret)
		return;
firmware_reset:
	dev_err(dev, "Failed to use the HW default value!\n");
	ad3_firmware_data_reset(dev, data);
}

void ad3_firmware_save_change(struct device *dev,
			      struct ad3_firmware_data *fw_data,
			      unsigned int offset,
			      unsigned int value)
{
	int index;

	index = (offset & (~AD3_REG_ZONE_MASK)) >> 2;

	switch(offset & AD3_REG_ZONE_MASK) {
		case AD3_REG_GLOBAL:
			ad3_firmware_set_global_data(dev,
					             &fw_data->global_data,
					             offset,
						     value);
			break;

		case AD3_REG_IRIDIX:
			ad3_firmware_set_iridix_data(dev,
						     &fw_data->iridix_data,
					             offset,
						     value);
			break;
		case AD3_REG_CALCULATOR:
			ad3_firmware_set_calc_data(dev,
						   &fw_data->calc_data,
						   offset,
						   value);
			break;
		case AD3_REG_ALCOEFF:
			fw_data->alcoeff[index] = value;
			break;
		case AD3_REG_ASYMCOEFF:
			fw_data->asymcoeff[index] = value;
			break;
		case AD3_REG_COLORCOEFF:
			fw_data->colorcoeff[index] = value;
			break;
		default:
			dev_err(dev, "Invalid register to save!\n");
	}
}
