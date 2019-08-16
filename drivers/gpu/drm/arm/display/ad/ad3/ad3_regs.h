/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include "../ad_regs.h"

#ifndef __AD3_REGS_H__
#define __AD3_REGS_H__

#define AD3_LUT_SIZE					33
#define AD3_REG_BITS					32
#define AD3_ARCH_ID					0x41443000

#define AD3_REG_ZONE_MASK				0xF00
#define AD3_REG_GLOBAL					0x000
#define AD3_REG_IRIDIX					0x100
#define AD3_REG_CALCULATOR				0x200
#define AD3_REG_ALCOEFF					0x300
#define AD3_REG_ASYMCOEFF				0x400
#define AD3_REG_COLORCOEFF				0x500

/*
 * AD3 register set
 */

/*Global Control Registers. */
#define AD_ARCH_ID_REG_OFFSET                   	0x000
#define AD_ARCH_ID_REG_MASK                     	0xFFFFFFFF

#define AD_CORE_ID_REG_OFFSET                   	0x004
#define AD_CORE_ID_REG_MASK                     	0xFFFFFFFF

#define AD_STATUS_REG_OFFSET                    	0x0010
#define AD_STATUS_REG_MASK                      	0xF0000707

#define AD_CONTROL_REG_OFFSET                   	0x0020
#define AD_CONTROL_REG_MASK                     	0x80000701
#define AD_CONTROL_REG_DEFAULT_VAL			0x100
#define AD_CONTROL_REG_COPR_MASK			0x00000001
#define AD_CONTROL_REG_MODE_MASK			0x00000300

#define AD_INPUT_SIZE_REG_OFFSET                	0x0024
#define AD_INPUT_SIZE_REG_MASK                  	0xFFFFFFFF
#define AD_INPUT_SIZE_REG_DEFAULT_VAL			0x1E00280

#define AD_INPUT_SIZE_H_MASK                    	0xFFFF
#define AD_INPUT_SIZE_H_MAX                     	0x1000
#define AD_INPUT_SIZE_H_MIN                     	0xe0
#define AD_INPUT_SIZE_V_SHIFT                   	16
#define AD_INPUT_SIZE_V_MAX                     	0x1000
#define AD_INPUT_SIZE_V_MIN                     	0x70

#define AD_DITHER_REG_OFFSET                    	0x0028
#define AD_DITHER_REG_MASK                      	0x0000000D
#define AD_DITHER_REG_DEFAULT_VAL			0x0

#define AD_STRENGTH_REG_OFFSET                  	0x0030
#define AD_STRENGTH_REG_MASK                    	0x000000FF
#define AD_STRENGTH_MIN                         	0x1

#define AD_DRC_REG_OFFSET                       	0x0034
#define AD_DRC_REG_MASK                         	0x0000FFFF

#define AD_BACKLIGHT_REG_OFFSET                 	0x0038
#define AD_BACKLIGHT_REG_MASK                   	0x0000FFFF

/*Iridix Control Registers. */
#define IRIDIX_CONTROL_REG_OFFSET               	0x0100
#define IRIDIX_CONTROL_REG_MASK                 	0x00000017
#define IRIDIX_CONTROL_REG_DEFAULT_VAL			0x4

#define IRIDIX_STATS_REG_OFFSET                 	0x0104
#define IRIDIX_STATS_REG_MASK                   	0x000000C3
#define IRIDIX_STATS_REG_DEFAULT_VAL			0x2

#define IRIDIX_LOCAL_REG_OFFSET              		0x010c
#define IRIDIX_LOCAL_REG_MASK                		0x000000FF
#define IRIDIX_LOCAL_REG_DEFAULT_VAL			0x41

#define IRIDIX_MAX_REG_OFFSET             		0x0110
#define IRIDIX_MAX_REG_MASK               		0x000000FF
#define IRIDIX_MAX_REG_DEFAULT_VAL			0x3C

#define IRIDIX_MIN_REG_OFFSET             		0x0114
#define IRIDIX_MIN_REG_MASK               		0x000000FF
#define IRIDIX_MIN_REG_DEFAULT_VAL			0x80

#define IRIDIX_BLACK_LEVEL_REG_OFFSET           	0x0118
#define IRIDIX_BLACK_LEVEL_REG_MASK             	0x000003FF
#define IRIDIX_BLACK_LEVEL_REG_DEFAULT_VAL		0x0

#define IRIDIX_WHITE_LEVEL_REG_OFFSET           	0x011c
#define IRIDIX_WHITE_LEVEL_REG_MASK             	0x000003FF
#define IRIDIX_WHITE_LEVEL_REG_DEFAULT_VAL		0x3FF

#define IRIDIX_AMP_REG_OFFSET                   	0x0120
#define IRIDIX_AMP_REG_MASK                     	0x000000FF
#define IRIDIX_AMP_REG_DEFAULT_VAL			0xF0

/*Ambient Light Adaptation Control Registers. */
#define CALC_STRENGTH_REG_OFFSET                	0x0204
#define CALC_STRENGTH_REG_MASK                  	0x000000FF
#define CALC_STRENGTH_REG_DEFAULT_VAL			0x1

#define CALC_DRC_REG_OFFSET                     	0x0208
#define CALC_DRC_REG_MASK                       	0x0000FFFF
#define CALC_DRC_REG_DEFAULT_VAL			0xFFFF

#define CALC_LIGHT_REG_OFFSET                   	0x020c
#define CALC_LIGHT_REG_MASK                     	0xFFFFFFFF
#define CALC_LIGHT_REG_DEFAULT_VAL			0xFFFFFFFF
#define CALC_LIGHT_REG_AL_MASK                  	0xFFFF0000
#define CALC_LIGHT_REG_BL_MASK                  	0x0000FFFF
#define CALC_LIGHT_REG_AL_SHIFT                 	16

#define CALC_AUTO_STRENGTH_LIMIT_REG_OFFSET     	0x0218
#define CALC_AUTO_STRENGTH_LIMIT_REG_MASK       	0x000000FF
#define CALC_AUTO_STRENGTH_LIMIT_REG_DEFAULT_VAL	0x80


#define CALC_CALIBRATION_A_REG_OFFSET           	0x021c
#define CALC_CALIBRATION_A_REG_MASK             	0x0000FFFF
#define CALC_CALIBRATION_A_REG_DEFAULT_VAL		0x20

#define CALC_CALIBRATION_B_REG_OFFSET           	0x0220
#define CALC_CALIBRATION_B_REG_MASK             	0x0000FFFF
#define CALC_CALIBRATION_B_REG_DEFAULT_VAL		0x5F

#define CALC_CALIBRATION_C_REG_OFFSET           	0x0224
#define CALC_CALIBRATION_C_REG_MASK             	0x0000FFFF
#define CALC_CALIBRATION_C_REG_DEFAULT_VAL		0x8

#define CALC_CALIBRATION_D_REG_OFFSET           	0x0228
#define CALC_CALIBRATION_D_REG_MASK             	0x0000FFFF
#define CALC_CALIBRATION_D_REG_DEFAULT_VAL		0x0

#define CALC_AUTO_STRENGTH_TFILTER_REG_OFFSET   	0x022c
#define CALC_AUTO_STRENGTH_TFILTER_REG_MASK     	0x000000FF
#define CALC_AUTO_STRENGTH_TFILTER_REG_DEFAULT_VAL	0x5

#define CALC_BACKLIGHT_MIN_REG_OFFSET           	0x0230
#define CALC_BACKLIGHT_MIN_REG_MASK             	0x0000FFFF
#define CALC_BACKLIGHT_MIN_REG_DEFAULT_VAL		0x1000

#define CALC_BACKLIGHT_MAX_REG_OFFSET           	0x0234
#define CALC_BACKLIGHT_MAX_REG_MASK             	0x0000FFFF
#define CALC_BACKLIGHT_MAX_REG_DEFAULT_VAL		0xFFFF

#define CALC_BACKLIGHT_SCALE_REG_OFFSET         	0x0238
#define CALC_BACKLIGHT_SCALE_REG_MASK           	0x0000FFFF
#define CALC_BACKLIGHT_SCALE_REG_DEFAULT_VAL		0xFFFF

#define CALC_AMBIENTLIGHT_MIN_REG_OFFSET        	0x023c
#define CALC_AMBIENTLIGHT_MIN_REG_MASK          	0x0000FFFF
#define CALC_AMBIENTLIGHT_MIN_REG_DEFAULT_VAL		0xE

#define CALC_AMBIENTLIGHT_TFILTER0_REG_OFFSET   	0x0240
#define CALC_AMBIENTLIGHT_TFILTER0_REG_MASK     	0x00000EDB
#define CALC_AMBIENTLIGHT_TFILTER0_REG_DEFAULT_VAL	0x6CA

#define CALC_AMBIENTLIGHT_TFILTER1_REG_OFFSET   	0x0244
#define CALC_AMBIENTLIGHT_TFILTER1_REG_MASK     	0x000000FF
#define CALC_AMBIENTLIGHT_TFILTER1_REG_REG_DEFAULT_VAL	0x6

/*Ambient Light Correction Coefficients Table. */
#define ALCOEFF_REG_OFFSET                     		0x0300
#define ALCOEFF_REG_MASK                       		0xFFFF

/*Iridix Asymeytry Coefficients Table.*/
#define ASYMCOEFF_REG_OFFSET                   		0x0400
#define ASYMCOEFF_REG_MASK                     		0x0FFF

/*Iridix Color Correction Transform Coefficients Table.*/
#define COLORCOEFF_REG_OFFSET                   	0x0500
#define COLORCOEFF_REG_MASK                     	0x0FFF

struct regmap *ad3_register_regmap_init(struct device *dev,
				        void __iomem *regs);
unsigned int ad3_register_get_all(const struct ad_reg **reg);
#endif /* __AD3_REGS_H__ */
