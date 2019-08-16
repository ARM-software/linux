/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include "ad3_regs.h"

#ifndef __AD3_FIRMWARE_H__
#define __AD3_FIRMWARE_H__

#define AD3_FIRMWARE_VERSION	0x1

#pragma pack(2)

struct ad3_global_data {
	u32 ad_control;
	u32 ad_input_size;
	u16 ad_dither;
};

struct ad3_iridix_data {
	u16 iridix_control;
	u16 iridix_stats;
	u16 iridix_local;
	u16 iridix_max;
	u16 iridix_min;
	u16 iridix_black_level;
	u16 iridix_white_level;
	u16 iridix_amp;
};

struct ad3_calc_data {
	u16 calc_strength;
	u16 calc_drc;
	u32 calc_light;
	u16 calc_auto_strength_limit;
	u16 calc_calibration_a;
	u16 calc_calibration_b;
	u16 calc_calibration_c;
	u16 calc_calibration_d;
	u16 calc_auto_strength_tfilter;
	u16 calc_backlight_min;
	u16 calc_backlight_max;
	u16 calc_backlight_scale;
	u16 calc_ambientlight_min;
	u16 calc_ambientlight_tfilter0;
	u16 calc_ambientlight_tfilter1;
};

struct ad3_firmware_data {
	u16 firmware_version;
	u32 arch_id;
	struct ad3_global_data global_data;
	struct ad3_iridix_data iridix_data;
	struct ad3_calc_data calc_data;
	u16 alcoeff[AD3_LUT_SIZE];
	u16 asymcoeff[AD3_LUT_SIZE];
	u16 colorcoeff[AD3_LUT_SIZE];
	u16 bl_linearity_lut[AD3_LUT_SIZE];
	u16 bl_linearity_inverse_lut[AD3_LUT_SIZE];
	u16 bl_att_lut[AD3_LUT_SIZE];
	u32 alpha;
};

#pragma pack()

void ad3_firmware_data_reset(struct device *dev,
			     struct ad3_firmware_data *data);
void ad3_firmware_load_data(struct device *dev,
			    struct ad3_firmware_data *data);
void ad3_firmware_save_change(struct device *dev,
			      struct ad3_firmware_data *fw_data,
			      unsigned int offset,
			      unsigned int value);
#endif /* __AD3_FIRMWARE_H__ */
