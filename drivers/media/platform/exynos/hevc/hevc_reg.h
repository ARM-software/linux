/*
 * linux/drivers/media/video/exynos/hevc/hevc_reg.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HEVC_REG_H_
#define __HEVC_REG_H_ __FILE__

#define HEVC_SYS_SW_RESET_ADDR		HEVC_DEC_RESET
#define HEVC_SYS_SW_RESET_MASK		0x3FF
#define HEVC_SYS_SW_RESET_SHFT		0x0
#define HEVC_SYS_R2H_INT_ADDR		HEVC_RISC_HOST_INT
#define HEVC_SYS_R2H_INT_MASK		0x1
#define HEVC_SYS_R2H_INT_SHFT		0x0
#define HEVC_SYS_H2R_CMD_ADDR		HEVC_HOST2RISC_CMD
#define HEVC_SYS_H2R_ARG1_ADDR		HEVC_HOST2RISC_ARG1
#define HEVC_SYS_CODEC_TYPE_ADDR		HEVC_HOST2RISC_ARG1
#define HEVC_SYS_INST_ID_ADDR		HEVC_HOST2RISC_ARG1
#define HEVC_SYS_FW_MEM_SIZE_ADDR	HEVC_HOST2RISC_ARG1
#define HEVC_SYS_H2R_ARG2_ADDR		HEVC_HOST2RISC_ARG2
#define HEVC_SYS_CRC_GEN_EN_ADDR		HEVC_HOST2RISC_ARG2
#define HEVC_SYS_CRC_GEN_EN_MASK		0x1
#define HEVC_SYS_CRC_GEN_EN_SHFT		0x1F
#define HEVC_SYS_DEC_PIXEL_CACHE_ADDR	HEVC_HOST2RISC_ARG2
#define HEVC_SYS_DEC_PIXEL_CACHE_MASK	0x2
#define HEVC_SYS_DEC_PIXEL_CACHE_SHFT	0x0
#define HEVC_SYS_H2R_ARG3_ADDR		HEVC_HOST2RISC_ARG3

#define HEVC_SYS_H2R_ARG4_ADDR		HEVC_HOST2RISC_ARG4

#define HEVC_SYS_FW_FIMV_INFO_ADDR	HEVC_FW_VERSION
#define HEVC_SYS_FW_FIMV_INFO_MASK	0xFF
#define HEVC_SYS_FW_FIMV_INFO_SHFT	24
#define HEVC_SYS_FW_VER_YEAR_ADDR	HEVC_FW_VERSION
#define HEVC_SYS_FW_VER_YEAR_MASK	0xFF
#define HEVC_SYS_FW_VER_YEAR_SHFT	16
#define HEVC_SYS_FW_VER_MONTH_ADDR	HEVC_FW_VERSION
#define HEVC_SYS_FW_VER_MONTH_MASK	0xFF
#define HEVC_SYS_FW_VER_MONTH_SHFT	8
#define HEVC_SYS_FW_VER_DATE_ADDR	HEVC_FW_VERSION
#define HEVC_SYS_FW_VER_DATE_MASK	0xFF
#define HEVC_SYS_FW_VER_DATE_SHFT	0
#define HEVC_SYS_FW_VER_ALL_ADDR		HEVC_FW_VERSION
#define HEVC_SYS_FW_VER_ALL_MASK		0xFFFFFF
#define HEVC_SYS_FW_VER_ALL_SHFT	0

#define HEVC_DEC_DISPLAY_Y_ADR_ADDR	HEVC_SI_DISPLAY_Y_ADR
#define HEVC_DEC_DISPLAY_Y_ADR_MASK	0xFFFFFFFF
#define HEVC_DEC_DISPLAY_Y_ADR_SHFT	HEVC_MEM_OFFSET
#define HEVC_DEC_DISPLAY_C_ADR_ADDR	HEVC_SI_DISPLAY_C_ADR
#define HEVC_DEC_DISPLAY_C_ADR_MASK	0xFFFFFFFF
#define HEVC_DEC_DISPLAY_C_ADR_SHFT	HEVC_MEM_OFFSET

#define HEVC_DEC_DECODED_Y_ADR_ADDR	HEVC_SI_DECODED_Y_ADR
#define HEVC_DEC_DECODED_Y_ADR_MASK	0xFFFFFFFF
#define HEVC_DEC_DECODED_Y_ADR_SHFT	HEVC_MEM_OFFSET
#define HEVC_DEC_DECODED_C_ADR_ADDR	HEVC_SI_DECODED_C_ADR
#define HEVC_DEC_DECODED_C_ADR_MASK	0xFFFFFFFF
#define HEVC_DEC_DECODED_C_ADR_SHFT	HEVC_MEM_OFFSET

#define HEVC_DEC_DISPLAY_STATUS_MASK	0x7
#define HEVC_DEC_DISPLAY_STATUS_SHFT	0x0
#define HEVC_DEC_DISPLAY_INTERACE_MASK	0x1
#define HEVC_DEC_DISPLAY_INTERACE_SHFT	0x3
#define HEVC_DEC_DISPLAY_RES_CHG_MASK	0x3
#define HEVC_DEC_DISPLAY_RES_CHG_SHFT	0x4

#define HEVC_DEC_DECODE_FRAME_TYPE_ADDR	HEVC_DECODE_FRAME_TYPE
#define HEVC_DEC_DECODE_FRAME_TYPE_MASK	0x7
#define HEVC_DEC_DECODE_FRAME_TYPE_SHFT	0

#define HEVC_DEC_DECODE_STATUS_MASK	0x7
#define HEVC_DEC_DECODE_STATUS_SHFT	0x0
#define HEVC_DEC_DECODE_INTERACE_MASK	0x1
#define HEVC_DEC_DECODE_INTERACE_SHFT	0x3
#define HEVC_DEC_DECODE_NUM_CRC_MASK	0x1
#define HEVC_DEC_DECODE_NUM_CRC_SHFT	0x4
#define HEVC_DEC_DECODE_GEN_CRC_MASK	0x1
#define HEVC_DEC_DECODE_GEN_CRC_SHFT	0x5

#define _HEVC_SET_REG(target, val)	hevc_write_reg(val, HEVC_##target##_ADDR)
#define HEVC_SET_REG(target, val, shadow)					\
	do {									\
		shadow = hevc_read_reg(HEVC_##target##_ADDR);			\
		shadow &= ~(HEVC_##target##_MASK << HEVC_##target##_SHFT);	\
		shadow |= ((val & HEVC_##target##_MASK) << HEVC_##target##_SHFT);	\
		hevc_write_reg(shadow, HEVC_##target##_ADDR);			\
	} while (0)

#define _HEVC_GET_REG(target)	hevc_read_reg(HEVC_##target##_ADDR)
#define HEVC_GET_REG(target)						\
	((hevc_read_reg(HEVC_##target##_ADDR) >> HEVC_##target##_SHFT)	\
	& HEVC_##target##_MASK)

#define HEVC_GET_ADR(target)						\
	(hevc_read_reg(HEVC_##target##_ADR_ADDR) << HEVC_##target##_ADR_SHFT)

#define HEVC_NV12M_HALIGN			16

void hevc_init_reg(void __iomem *base);
void hevc_write_reg(unsigned int data, unsigned int offset);
unsigned int hevc_read_reg(unsigned int offset);
#endif /* __HEVC_REG_H_ */
