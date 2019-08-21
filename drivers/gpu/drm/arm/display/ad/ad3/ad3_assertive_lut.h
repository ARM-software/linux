// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 */
#ifndef __AD3_ASSERTIVE_LUT_H__
#define __AD3_ASSERTIVE_LUT_H__

#define BL_DATA_WIDH		16

/* result for 2^(8*(x-128)/255), x=[0, 255] */
extern const uint32_t assertive_lut[];

int ad3_assertive_bl_lut(int input, u16 *lut_nodes);
/*
 *Function calculates the attenuated and blended BL values from the BL input.
 */
int ad3_assertive_calc_bl_input(u16 *bl_lin_lut,
				u16 *bl_att_lut,
				int alpha, int num_bl_bits,
				int bl_input);
#endif
