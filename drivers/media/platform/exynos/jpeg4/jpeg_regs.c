/* linux/drivers/media/platform/exynos/jpeg4/jpeg_regs.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Register interface file for jpeg v4.x driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/io.h>
#include <linux/delay.h>

#include "jpeg_regs.h"
#include "jpeg_conf.h"
#include "jpeg_core.h"
#include "regs_jpeg_v4_x.h"

void jpeg_sw_reset(void __iomem *base)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_CNTL_REG);
	writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK),
			base + S5P_JPEG_CNTL_REG);

	reg = readl(base + S5P_JPEG_CNTL_REG);
	writel(reg & ~S5P_JPEG_SOFT_RESET_HI,
			base + S5P_JPEG_CNTL_REG);

	ndelay(100000);

	writel(reg | S5P_JPEG_SOFT_RESET_HI,
			base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_enc_dec_mode(void __iomem *base, enum jpeg_mode mode)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_CNTL_REG);
	/* set jpeg mod register */
	if (mode == DECODING) {
		writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK) | S5P_JPEG_DEC_MODE,
			base + S5P_JPEG_CNTL_REG);
	} else {/* encode */
		writel((reg & S5P_JPEG_ENC_DEC_MODE_MASK) | S5P_JPEG_ENC_MODE,
			base + S5P_JPEG_CNTL_REG);
	}
}

void jpeg_set_dec_in_fmt(void __iomem *base,
		enum jpeg_frame_format in_fmt)
{
	unsigned int reg = 0;

	writel(0x0, base + S5P_JPEG_IMG_FMT_REG); /* clear */
	reg = readl(base + S5P_JPEG_IMG_FMT_REG) &
		~S5P_JPEG_FMT_MASK; /* clear dec format */

	switch (in_fmt) {
		case JPEG_GRAY:
			reg = reg | S5P_JPEG_FMT_GRAY;
			break;

		case JPEG_444:
			reg = reg | S5P_JPEG_FMT_YUV_444;
			break;

		case JPEG_422:
			reg = reg | S5P_JPEG_FMT_YUV_422;
			break;

		case JPEG_420:
			reg = reg | S5P_JPEG_FMT_YUV_420;
			break;

		case JPEG_422V:
			reg = reg | S5P_JPEG_FMT_YUV_422V;
			break;

		default:
			break;
	}
	writel(reg, base + S5P_JPEG_IMG_FMT_REG);
}

void jpeg_set_dec_out_fmt(void __iomem *base,
					enum jpeg_frame_format out_fmt)
{
	unsigned int reg = 0;

	reg = readl(base + S5P_JPEG_IMG_FMT_REG);

	/* set jpeg deocde ouput format register */
	switch (out_fmt) {
	case GRAY:
		reg = reg | S5P_JPEG_DEC_GRAY_IMG | S5P_JPEG_GRAY_IMG_IP;
		break;

	case ARGB_8888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_32BIT_IMG |
			S5P_JPEG_ENC_FMT_RGB;
		break;

	case ABGR_8888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_32BIT_IMG |
			S5P_JPEG_ENC_FMT_BGR;
		break;

	case RGB_888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_24BIT_IMG |
			S5P_JPEG_ENC_FMT_RGB;
		break;

	case BGR_888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_24BIT_IMG |
			S5P_JPEG_ENC_FMT_BGR;
		break;

	case RGB_565:
		reg = reg | S5P_JPEG_ENC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_16BIT_IMG |
			S5P_JPEG_ENC_FMT_RGB;
		break;

	case BGR_565:
		reg = reg | S5P_JPEG_ENC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_16BIT_IMG |
			S5P_JPEG_ENC_FMT_BGR;
		break;

	case YCRCB_444_2P:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
			S5P_JPEG_YUV_444_IP_YUV_444_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CrCb;
		break;

	case YCBCR_444_2P:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
			S5P_JPEG_YUV_444_IP_YUV_444_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case YCBCR_444_3P:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
			S5P_JPEG_YUV_444_IP_YUV_444_3P_IMG;
		break;

	case CRCBY_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_VYUY;
		break;
	case CBCRY_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_UYVY;
		break;

	case YCRCB_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_YVYU;
		break;

	case YCBCR_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_YUYV;
		break;

	case YCBCR_422V_2P:
		reg = reg | S5P_JPEG_DEC_YUV_422V_IMG |
			S5P_JPEG_YUV_422V_IP_YUV_422V_2P_IMG;
		break;

	case YCBCR_422V_3P:
		reg = reg | S5P_JPEG_DEC_YUV_422V_IMG |
			S5P_JPEG_YUV_422V_IP_YUV_422V_3P_IMG;
		break;

	case YCRCB_422_2P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CrCb;
		break;

	case YCBCR_422_2P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case YCBCR_422_3P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_3P_IMG;
		break;

	case YCRCB_420_2P:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
			S5P_JPEG_YUV_420_IP_YUV_420_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CrCb;
		break;

	case YCBCR_420_2P:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
			S5P_JPEG_YUV_420_IP_YUV_420_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case YCBCR_420_3P:
	case YCRCB_420_3P:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
			S5P_JPEG_YUV_420_IP_YUV_420_3P_IMG;
		break;

	default:
		break;

	}

	writel(reg, base + S5P_JPEG_IMG_FMT_REG);
}

void jpeg_set_enc_in_fmt(void __iomem *base,
					enum jpeg_frame_format in_fmt)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_IMG_FMT_REG) &
			S5P_JPEG_ENC_IN_FMT_MASK; /* clear except enc format */

	switch (in_fmt) {
	case GRAY:
		reg = reg | S5P_JPEG_ENC_GRAY_IMG | S5P_JPEG_GRAY_IMG_IP;
		break;

	case ARGB_8888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_32BIT_IMG |
			S5P_JPEG_ENC_FMT_RGB;
		break;

	case ABGR_8888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_32BIT_IMG |
			S5P_JPEG_ENC_FMT_BGR;
		break;

	case RGB_888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_24BIT_IMG |
			S5P_JPEG_ENC_FMT_RGB;
		break;

	case BGR_888:
		reg = reg | S5P_JPEG_DEC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_24BIT_IMG |
			S5P_JPEG_ENC_FMT_BGR;
		break;

	case RGB_565:
		reg = reg | S5P_JPEG_ENC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_16BIT_IMG |
			S5P_JPEG_ENC_FMT_RGB;
		break;

	case BGR_565:
		reg = reg | S5P_JPEG_ENC_RGB_IMG |
			S5P_JPEG_RGB_IP_RGB_16BIT_IMG |
			S5P_JPEG_ENC_FMT_BGR;
		break;

	case YCRCB_444_2P:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
			S5P_JPEG_YUV_444_IP_YUV_444_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CrCb;
		break;

	case YCBCR_444_2P:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
			S5P_JPEG_YUV_444_IP_YUV_444_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case YCBCR_444_3P:
		reg = reg | S5P_JPEG_ENC_YUV_444_IMG |
			S5P_JPEG_YUV_444_IP_YUV_444_3P_IMG;
		break;

	case CRCBY_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_VYUY;
		break;
	case CBCRY_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_UYVY;
		break;

	case YCRCB_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_YVYU;
		break;

	case YCBCR_422_1P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_1P_IMG |
			S5P_JPEG_ENC_FMT_YUYV;
		break;

	case YCBCR_422V_2P:
		reg = reg | S5P_JPEG_DEC_YUV_422V_IMG |
			S5P_JPEG_YUV_422V_IP_YUV_422V_2P_IMG;
		break;

	case YCBCR_422V_3P:
		reg = reg | S5P_JPEG_DEC_YUV_422V_IMG |
			S5P_JPEG_YUV_422V_IP_YUV_422V_3P_IMG;
		break;

	case YCRCB_422_2P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CrCb;
		break;

	case YCBCR_422_2P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case YCBCR_422_3P:
		reg = reg | S5P_JPEG_DEC_YUV_422_IMG |
			S5P_JPEG_YUV_422_IP_YUV_422_3P_IMG;
		break;

	case YCRCB_420_2P:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
			S5P_JPEG_YUV_420_IP_YUV_420_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CrCb;
		break;

	case YCBCR_420_2P:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
			S5P_JPEG_YUV_420_IP_YUV_420_2P_IMG |
			S5P_JPEG_SWAP_CHROMA_CbCr;
		break;

	case YCBCR_420_3P:
	case YCRCB_420_3P:
		reg = reg | S5P_JPEG_DEC_YUV_420_IMG |
			S5P_JPEG_YUV_420_IP_YUV_420_3P_IMG;
		break;

	default:
		break;

	}

	writel(reg, base + S5P_JPEG_IMG_FMT_REG);

}

void jpeg_set_enc_out_fmt(void __iomem *base,
					enum jpeg_frame_format out_fmt)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_IMG_FMT_REG) &
			~S5P_JPEG_FMT_MASK; /* clear enc format */

	switch (out_fmt) {
	case JPEG_GRAY:
		reg = reg | S5P_JPEG_FMT_GRAY;
		break;

	case JPEG_444:
		reg = reg | S5P_JPEG_FMT_YUV_444;
		break;

	case JPEG_422:
		reg = reg | S5P_JPEG_FMT_YUV_422;
		break;

	case JPEG_420:
		reg = reg | S5P_JPEG_FMT_YUV_420;
		break;

	case JPEG_422V:
		reg = reg | S5P_JPEG_FMT_YUV_422V;
		break;

	default:
		break;
	}

	writel(reg, base + S5P_JPEG_IMG_FMT_REG);
}

void jpeg_set_enc_tbl(void __iomem *base,
		enum jpeg_img_quality_level level)
{
	int i;

	switch (level) {
	case QUALITY_LEVEL_1:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[0][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[1][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[0][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[1][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;

	case QUALITY_LEVEL_2:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[2][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[3][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[2][i],
			base + S5P_JPEG_QUAN_TBL_ENTRY_REG
			+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[3][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;

	case QUALITY_LEVEL_3:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[4][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[5][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[4][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[5][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;

	case QUALITY_LEVEL_4:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[6][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[7][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[6][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[7][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;

	case QUALITY_LEVEL_5:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[8][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[9][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[8][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[9][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;

	case QUALITY_LEVEL_6:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[10][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[11][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[10][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[11][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;

	default:
		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[0][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[1][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x40 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[0][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0x80 + (i*0x04));
		}

		for (i = 0; i < 16; i++) {
			writel((unsigned int)ITU_Q_tbl[1][i],
				base + S5P_JPEG_QUAN_TBL_ENTRY_REG
				+ 0xc0 + (i*0x04));
		}
		break;
	}
	for (i = 0; i < 4; i++) {
		writel((unsigned int)ITU_H_tbl_len_DC_luminance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + (i*0x04));
	}

	for (i = 0; i < 3; i++) {
		writel((unsigned int)ITU_H_tbl_val_DC_luminance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x10 + (i*0x04));
	}

	for (i = 0; i < 4; i++) {
		writel((unsigned int)ITU_H_tbl_len_DC_chrominance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x20 + (i*0x04));
	}

	for (i = 0; i < 3; i++) {
		writel((unsigned int)ITU_H_tbl_val_DC_chrominance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x30 + (i*0x04));
	}

	for (i = 0; i < 4; i++) {
		writel((unsigned int)ITU_H_tbl_len_AC_luminance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x40 + (i*0x04));
	}

	for (i = 0; i < 41; i++) {
		writel((unsigned int)ITU_H_tbl_val_AC_luminance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x50 + (i*0x04));
	}

	for (i = 0; i < 4; i++) {
		writel((unsigned int)ITU_H_tbl_len_AC_chrominance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x100 + (i*0x04));
	}

	for (i = 0; i < 41; i++) {
		writel((unsigned int)ITU_H_tbl_val_AC_chrominance[i],
			base + S5P_JPEG_HUFF_TBL_ENTRY_REG + 0x110 + (i*0x04));
	}
}

void jpeg_set_interrupt(void __iomem *base)
{
	unsigned int reg;

	reg = readl(base + S5P_JPEG_INT_EN_REG) & ~S5P_JPEG_INT_EN_MASK;
	writel(S5P_JPEG_INT_EN_ALL, base + S5P_JPEG_INT_EN_REG);
}

void jpeg_clean_interrupt(void __iomem *base)
{
	writel(0, base + S5P_JPEG_INT_EN_REG);
}

unsigned int jpeg_get_int_status(void __iomem *base)
{
	unsigned int	int_status;

	int_status = readl(base + S5P_JPEG_INT_STATUS_REG);

	return int_status;
}

void jpeg_set_huf_table_enable(void __iomem *base, int value)
{
	unsigned int	reg;

	reg = readl(base + S5P_JPEG_CNTL_REG) & ~S5P_JPEG_HUF_TBL_EN;

	if (value == 1)
		writel(reg | S5P_JPEG_HUF_TBL_EN, base + S5P_JPEG_CNTL_REG);
	else
		writel(reg | ~S5P_JPEG_HUF_TBL_EN, base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_dec_scaling(void __iomem *base,
		enum jpeg_scale_value value)
{
	unsigned int	reg;
	unsigned int	denom;

	reg = readl(base + S5P_JPEG_CNTL_REG) &
				~S5P_JPEG_DEC_IMG_RESLN_TYPE_MASK;

	switch (value) {
		case JPEG_SCALE_NORMAL:
			denom = 0;
			break;
		case JPEG_SCALE_2:
			denom = 1;
			break;
		case JPEG_SCALE_4:
			denom = 2;
			break;
		case JPEG_SCALE_8:
			denom = 3;
			break;
		default:
			denom = 0;
			break;
	}

	writel(reg | S5P_JPEG_DEC_IMG_RESLN_TYPE(denom), base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_sys_int_enable(void __iomem *base, int value)
{
	unsigned int	reg;

	reg = readl(base + S5P_JPEG_CNTL_REG) & ~(S5P_JPEG_SYS_INT_EN);

	if (value == 1)
		writel(S5P_JPEG_SYS_INT_EN, base + S5P_JPEG_CNTL_REG);
	else
		writel(~S5P_JPEG_SYS_INT_EN, base + S5P_JPEG_CNTL_REG);
}

void jpeg_set_stream_buf_address(void __iomem *base, unsigned int address)
{
	writel(address, base + S5P_JPEG_OUT_MEM_BASE_REG);
}

void jpeg_set_stream_size(void __iomem *base,
		unsigned int x_value, unsigned int y_value)
{
	writel(0x0, base + S5P_JPEG_IMG_SIZE_REG); /* clear */
	writel(S5P_JPEG_X_SIZE(x_value) | S5P_JPEG_Y_SIZE(y_value),
			base + S5P_JPEG_IMG_SIZE_REG);
}

void jpeg_set_frame_buf_address(void __iomem *base,
		enum jpeg_frame_format fmt, unsigned int address, unsigned int width, unsigned int height)
{
	switch (fmt) {
	case GRAY:
	case RGB_565:
	case RGB_888:
	case BGR_888:
	case YCRCB_422_1P:
	case YCBCR_422_1P:
	case CBCRY_422_1P:
	case CRCBY_422_1P:
	case ARGB_8888:
	case ABGR_8888:
		writel(address, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case YCBCR_444_2P:
	case YCRCB_444_2P:
	case YCRCB_422_2P:
	case YCBCR_422_2P:
	case YCBCR_420_2P:
	case YCRCB_420_2P:
	case YCBCR_422V_2P:
		writel(address, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address + (width * height), base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(0, base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case YCBCR_444_3P:
		writel(address, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address + (width * height), base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(address + (width * height * 2), base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case YCBCR_422_3P:
	case YCBCR_422V_3P:
		writel(address, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address + (width * height), base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(address + (width * height +  (width * height / 2)), base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case YCBCR_420_3P:
		writel(address, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address + (width * height), base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(address + (width * height + (width * height / 4)), base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	case YCRCB_420_3P:
		writel(address, base + S5P_JPEG_IMG_BA_PLANE_1_REG);
		writel(address + (width * height + (width * height / 4)), base + S5P_JPEG_IMG_BA_PLANE_2_REG);
		writel(address + (width * height), base + S5P_JPEG_IMG_BA_PLANE_3_REG);
		break;
	default:
		break;
	}
}
void jpeg_set_encode_tbl_select(void __iomem *base,
		enum jpeg_img_quality_level level)
{
	unsigned int	reg;

	switch (level) {
	case QUALITY_LEVEL_1:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_1 |
			S5P_JPEG_Q_TBL_COMP3_1 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_2:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_3 |
			S5P_JPEG_Q_TBL_COMP3_3 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_3:
		reg = S5P_JPEG_Q_TBL_COMP1_2 | S5P_JPEG_Q_TBL_COMP2_1 |
			S5P_JPEG_Q_TBL_COMP3_1 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_4:
		reg = S5P_JPEG_Q_TBL_COMP1_2 | S5P_JPEG_Q_TBL_COMP2_3 |
			S5P_JPEG_Q_TBL_COMP3_3 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_5:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_0 |
			S5P_JPEG_Q_TBL_COMP3_0 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	case QUALITY_LEVEL_6:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_0 |
			S5P_JPEG_Q_TBL_COMP3_1 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	default:
		reg = S5P_JPEG_Q_TBL_COMP1_0 | S5P_JPEG_Q_TBL_COMP2_1 |
			S5P_JPEG_Q_TBL_COMP3_1 |
			S5P_JPEG_HUFF_TBL_COMP1_AC_0_DC_0 |
			S5P_JPEG_HUFF_TBL_COMP2_AC_1_DC_1 |
			S5P_JPEG_HUFF_TBL_COMP3_AC_1_DC_1;
		break;
	}
	writel(reg, base + S5P_JPEG_TBL_SEL_REG);
}

void jpeg_set_encode_hoff_cnt(void __iomem *base, enum jpeg_frame_format fmt)
{
	if (fmt == JPEG_GRAY)
		writel(0xd2, base + S5P_JPEG_HUFF_CNT_REG);
	else
		writel(0x1a2, base + S5P_JPEG_HUFF_CNT_REG);
}

unsigned int jpeg_get_stream_size(void __iomem *base)
{
	unsigned int size;

	size = readl(base + S5P_JPEG_BITSTREAM_SIZE_REG);
	return size;
}

void jpeg_set_dec_bitstream_size(void __iomem *base, unsigned int size)
{
	writel(size, base + S5P_JPEG_BITSTREAM_SIZE_REG);
}

void jpeg_set_timer_count(void __iomem *base, unsigned int size)
{
	writel(size, base + S5P_JPEG_INT_TIMER_COUNT_REG);
}

void jpeg_get_frame_size(void __iomem *base,
			unsigned int *width, unsigned int *height)
{
    unsigned int reg = readl(base + S5P_JPEG_IMG_SIZE_REG);
	*width = reg & S5P_JPEG_X_SIZE_MASK;
	*height = (reg & S5P_JPEG_Y_SIZE_MASK) >> S5P_JPEG_Y_SIZE_SHIFT;
}

enum jpeg_frame_format jpeg_get_frame_fmt(void __iomem *base)
{
	unsigned int	reg;
	enum jpeg_frame_format out_format;

	reg = readl(base + S5P_JPEG_IMG_FMT_REG);

	out_format =
		((reg & 0x03) == 0x01) ? JPEG_444 :
		((reg & 0x03) == 0x02) ? JPEG_422 :
		((reg & 0x03) == 0x03) ? JPEG_420 :
		((reg & 0x03) == 0x00) ? JPEG_GRAY : JPEG_RESERVED;

	return out_format;
}

int jpeg_set_number_of_component(void __iomem *base, unsigned int num_component)
{
	unsigned int reg = 0;

	if (num_component < 1 || num_component > 3)
		return -EINVAL;

	reg = readl(base + S5P_JPEG_TBL_SEL_REG);

	writel(reg | S5P_JPEG_NUMBER_OF_COMPONENTS(num_component), base + S5P_JPEG_TBL_SEL_REG);
	return 0;
}

void jpeg_alpha_value_set(void __iomem *base, unsigned int alpha)
{
	writel(S5P_JPEG_ARGB32(alpha), base + S5P_JPEG_PADDING_REG);
}

void jpeg_dec_window_ctrl(void __iomem *base, unsigned int is_start)
{
	writel(is_start & 1, base + S5P_JPEG_DEC_WINDOW_CNTL);
}

void jpeg_set_window_margin(void __iomem *base, unsigned int top, unsigned int bottom,
			unsigned int left, unsigned int right)
{
	jpeg_dec_window_ctrl(base, true);
	writel(S5P_JPEG_IMG_TOP_MARGIN(top), base + S5P_JPEG_DEC_WINDOW_MARN_1);
	writel(S5P_JPEG_IMG_BOTTOM_MARGIN(bottom), base + S5P_JPEG_DEC_WINDOW_MARN_1);

	writel(S5P_JPEG_IMG_LEFT_MARGIN(left), base + S5P_JPEG_DEC_WINDOW_MARN_2);
	writel(S5P_JPEG_IMG_RIGHT_MARGIN(right), base + S5P_JPEG_DEC_WINDOW_MARN_2);
	jpeg_dec_window_ctrl(base, false);
}

void jpeg_get_window_margin(void __iomem *base, unsigned int *top, unsigned int *bottom,
			unsigned int *left, unsigned int *right)
{
	*top = readl(base + S5P_JPEG_DEC_WINDOW_MARN_1) & S5P_JPEG_IMG_TOP_MARGIN_MASK;
	*bottom = readl(base + S5P_JPEG_DEC_WINDOW_MARN_1) & S5P_JPEG_IMG_BOTTOM_MARGIN_MASK;

	*left = readl(base + S5P_JPEG_DEC_WINDOW_MARN_2) & S5P_JPEG_IMG_LEFT_MARGIN_MASK;
	*right = readl(base + S5P_JPEG_DEC_WINDOW_MARN_2) & S5P_JPEG_IMG_RIGHT_MARGIN_MASK;
}
