/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/irqs.h>

#include "fimc-is-time.h"
#include "fimc-is-core.h"
#include "fimc-is-regs.h"
#include "fimc-is-interface.h"
#include "fimc-is-device-flite.h"

#define FIMCLITE0_REG_BASE	(S5P_VA_FIMCLITE0)  /* phy : 0x13c0_0000 */
#define FIMCLITE1_REG_BASE	(S5P_VA_FIMCLITE1)  /* phy : 0x13c1_0000 */
#define FIMCLITE2_REG_BASE	(S5P_VA_FIMCLITE2)  /* phy : 0x13c9_0000 */

#define FLITE_MAX_RESET_READY_TIME	(20) /* 100ms */
#define FLITE_MAX_WIDTH_SIZE		(8192)
#define FLITE_MAX_HEIGHT_SIZE		(8192)

/*FIMCLite*/
/* Camera Source size */
#define FLITE_REG_CISRCSIZE				(0x00)
#define FLITE_REG_CISRCSIZE_SIZE_H(x)			((x) << 16)
#define FLITE_REG_CISRCSIZE_SIZE_V(x)			((x) << 0)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_YCBYCR		(0 << 14)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_YCRYCB		(1 << 14)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_CBYCRY		(2 << 14)
#define FLITE_REG_CISRCSIZE_ORDER422_IN_CRYCBY		(3 << 14)

/* Global control */
#define FLITE_REG_CIGCTRL				0x04
#define FLITE_REG_CIGCTRL_YUV422_1P			(0x1E << 24)
#define FLITE_REG_CIGCTRL_RAW8				(0x2A << 24)
#define FLITE_REG_CIGCTRL_RAW10				(0x2B << 24)
#define FLITE_REG_CIGCTRL_RAW12				(0x2C << 24)
#define FLITE_REG_CIGCTRL_RAW14				(0x2D << 24)

/* User defined formats. x = 0...0xF. */
#define FLITE_REG_CIGCTRL_USER(x)			(0x30 + x - 1)
#define FLITE_REG_CIGCTRL_OLOCAL_DISABLE		(1 << 22)
#define FLITE_REG_CIGCTRL_SHADOWMASK_DISABLE		(1 << 21)
#define FLITE_REG_CIGCTRL_ODMA_DISABLE			(1 << 20)
#define FLITE_REG_CIGCTRL_SWRST_REQ			(1 << 19)
#define FLITE_REG_CIGCTRL_SWRST_RDY			(1 << 18)
#define FLITE_REG_CIGCTRL_SWRST				(1 << 17)
#define FLITE_REG_CIGCTRL_TEST_PATTERN_COLORBAR		(1 << 15)
#define FLITE_REG_CIGCTRL_INVPOLPCLK			(1 << 14)
#define FLITE_REG_CIGCTRL_INVPOLVSYNC			(1 << 13)
#define FLITE_REG_CIGCTRL_INVPOLHREF			(1 << 12)
#define FLITE_REG_CIGCTRL_IRQ_LASTEN0_ENABLE		(0 << 8)
#define FLITE_REG_CIGCTRL_IRQ_LASTEN0_DISABLE		(1 << 8)
#define FLITE_REG_CIGCTRL_IRQ_ENDEN0_ENABLE		(0 << 7)
#define FLITE_REG_CIGCTRL_IRQ_ENDEN0_DISABLE		(1 << 7)
#define FLITE_REG_CIGCTRL_IRQ_STARTEN0_ENABLE		(0 << 6)
#define FLITE_REG_CIGCTRL_IRQ_STARTEN0_DISABLE		(1 << 6)
#define FLITE_REG_CIGCTRL_IRQ_OVFEN0_ENABLE		(0 << 5)
#define FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE		(1 << 5)
#define FLITE_REG_CIGCTRL_SELCAM_MIPI			(1 << 3)

/* Image Capture Enable */
#define FLITE_REG_CIIMGCPT				(0x08)
#define FLITE_REG_CIIMGCPT_IMGCPTEN			(1 << 31)
#define FLITE_REG_CIIMGCPT_CPT_FREN			(1 << 25)
#define FLITE_REG_CIIMGCPT_CPT_FRPTR(x)			((x) << 19)
#define FLITE_REG_CIIMGCPT_CPT_MOD_FRCNT		(1 << 18)
#define FLITE_REG_CIIMGCPT_CPT_MOD_FREN			(0 << 18)
#define FLITE_REG_CIIMGCPT_CPT_FRCNT(x)			((x) << 10)

/* Capture Sequence */
#define FLITE_REG_CICPTSEQ				(0x0C)
#define FLITE_REG_CPT_FRSEQ(x)				((x) << 0)

/* Camera Window Offset */
#define FLITE_REG_CIWDOFST				(0x10)
#define FLITE_REG_CIWDOFST_WINOFSEN			(1 << 31)
#define FLITE_REG_CIWDOFST_CLROVIY			(1 << 31)
#define FLITE_REG_CIWDOFST_WINHOROFST(x)		((x) << 16)
#define FLITE_REG_CIWDOFST_HOROFF_MASK			(0x1fff << 16)
#define FLITE_REG_CIWDOFST_CLROVFICB			(1 << 15)
#define FLITE_REG_CIWDOFST_CLROVFICR			(1 << 14)
#define FLITE_REG_CIWDOFST_WINVEROFST(x)		((x) << 0)
#define FLITE_REG_CIWDOFST_VEROFF_MASK			(0x1fff << 0)

/* Cmaera Window Offset2 */
#define FLITE_REG_CIWDOFST2				(0x14)
#define FLITE_REG_CIWDOFST2_WINHOROFST2(x)		((x) << 16)
#define FLITE_REG_CIWDOFST2_WINVEROFST2(x)		((x) << 0)

/* Camera Output DMA Format */
#define FLITE_REG_CIODMAFMT				(0x18)
#define FLITE_REG_CIODMAFMT_1D_DMA			(1 << 15)
#define FLITE_REG_CIODMAFMT_2D_DMA			(0 << 15)
#define FLITE_REG_CIODMAFMT_PACK12			(1 << 14)
#define FLITE_REG_CIODMAFMT_NORMAL			(0 << 14)
#define FLITE_REG_CIODMAFMT_CRYCBY			(0 << 4)
#define FLITE_REG_CIODMAFMT_CBYCRY			(1 << 4)
#define FLITE_REG_CIODMAFMT_YCRYCB			(2 << 4)
#define FLITE_REG_CIODMAFMT_YCBYCR			(3 << 4)

/* Camera Output Canvas */
#define FLITE_REG_CIOCAN				(0x20)
#define FLITE_REG_CIOCAN_OCAN_V(x)			((x) << 16)
#define FLITE_REG_CIOCAN_OCAN_H(x)			((x) << 0)

/* Camera Output DMA Offset */
#define FLITE_REG_CIOOFF				(0x24)
#define FLITE_REG_CIOOFF_OOFF_V(x)			((x) << 16)
#define FLITE_REG_CIOOFF_OOFF_H(x)			((x) << 0)

/* Camera Output DMA Address */
#define FLITE_REG_CIOSA					(0x30)
#define FLITE_REG_CIOSA_OSA(x)				((x) << 0)

/* Camera Status */
#define FLITE_REG_CISTATUS				(0x40)
#define FLITE_REG_CISTATUS_MIPI_VVALID			(1 << 22)
#define FLITE_REG_CISTATUS_MIPI_HVALID			(1 << 21)
#define FLITE_REG_CISTATUS_MIPI_DVALID			(1 << 20)
#define FLITE_REG_CISTATUS_ITU_VSYNC			(1 << 14)
#define FLITE_REG_CISTATUS_ITU_HREFF			(1 << 13)
#define FLITE_REG_CISTATUS_OVFIY			(1 << 10)
#define FLITE_REG_CISTATUS_OVFICB			(1 << 9)
#define FLITE_REG_CISTATUS_OVFICR			(1 << 8)
#define FLITE_REG_CISTATUS_IRQ_SRC_OVERFLOW		(1 << 7)
#define FLITE_REG_CISTATUS_IRQ_SRC_LASTCAPEND		(1 << 6)
#define FLITE_REG_CISTATUS_IRQ_SRC_FRMSTART		(1 << 5)
#define FLITE_REG_CISTATUS_IRQ_SRC_FRMEND		(1 << 4)
#define FLITE_REG_CISTATUS_IRQ_CAM			(1 << 0)
#define FLITE_REG_CISTATUS_IRQ_MASK			(0xf << 4)

/* Camera Status2 */
#define FLITE_REG_CISTATUS2				(0x44)
#define FLITE_REG_CISTATUS2_LASTCAPEND			(1 << 1)
#define FLITE_REG_CISTATUS2_FRMEND			(1 << 0)

/* Camera Status3 */
#define FLITE_REG_CISTATUS3				0x48
#define FLITE_REG_CISTATUS3_PRESENT_MASK		(0x3F)

/* Qos Threshold */
#define FLITE_REG_CITHOLD				(0xF0)
#define FLITE_REG_CITHOLD_W_QOS_EN			(1 << 30)
#define FLITE_REG_CITHOLD_WTH_QOS(x)			((x) << 0)

/* Camera General Purpose */
#define FLITE_REG_CIGENERAL				(0xFC)
#define FLITE_REG_CIGENERAL_CAM_A			(0)
#define FLITE_REG_CIGENERAL_CAM_B			(1)
#define FLITE_REG_CIGENERAL_CAM_C			(2)
#define FLITE_REG_CIGENERAL_CAM_D			(3)
#define FLITE_REG_CIGENERAL_3AA1_CAM_A			(0 << 14)
#define FLITE_REG_CIGENERAL_3AA1_CAM_B			(1 << 14)
#define FLITE_REG_CIGENERAL_3AA1_CAM_C			(2 << 14)
#define FLITE_REG_CIGENERAL_3AA1_CAM_D			(3 << 14)

#define FLITE_REG_CIFCNTSEQ				0x100

/* BNS */
#define FLITE_REG_BINNINGON				(0x120)
#define FLITE_REG_BINNINGON_CLKGATE_ON(x)		(~(x) << 1)
#define FLITE_REG_BINNINGON_EN(x)			((x) << 0)

#define FLITE_REG_BINNINGCTRL				(0x124)
#define FLITE_REG_BINNINGCTRL_FACTOR_Y(x)		((x) << 22)
#define FLITE_REG_BINNINGCTRL_FACTOR_X(x)		((x) << 17)
#define FLITE_REG_BINNINGCTRL_SHIFT_UP_Y(x)		((x) << 15)
#define FLITE_REG_BINNINGCTRL_SHIFT_UP_X(x)		((x) << 13)
#define FLITE_REG_BINNINGCTRL_PRECISION_BITS(x)		((x) << 10)
#define FLITE_REG_BINNINGCTRL_BITTAGE(x)		((x) << 5)
#define FLITE_REG_BINNINGCTRL_UNITY_SIZE(x)		((x) << 0)

#define FLITE_REG_PEDESTAL				(0x128)
#define FLITE_REG_PEDESTAL_OUT(x)			((x) << 12)
#define FLITE_REG_PEDESTAL_IN(x)			((x) << 0)

#define FLITE_REG_BINNINGTOTAL				(0x12C)
#define FLITE_REG_BINNINGTOTAL_HEIGHT(x)		((x) << 16)
#define FLITE_REG_BINNINGTOTAL_WIDTH(x)			((x) << 0)

#define FLITE_REG_BINNINGINPUT				(0x130)
#define FLITE_REG_BINNINGINPUT_HEIGHT(x)		((x) << 16)
#define FLITE_REG_BINNINGINPUT_WIDTH(x)			((x) << 0)

#define FLITE_REG_BINNINGMARGIN				(0x134)
#define FLITE_REG_BINNINGMARGIN_TOP(x)			((x) << 16)
#define FLITE_REG_BINNINGMARGIN_LEFT(x)			((x) << 0)

#define FLITE_REG_BINNINGOUTPUT				(0x138)
#define FLITE_REG_BINNINGOUTPUT_HEIGHT(x)		((x) << 16)
#define FLITE_REG_BINNINGOUTPUT_WIDTH(x)		((x) << 0)

#define FLITE_REG_WEIGHTX01				(0x13C)
#define FLITE_REG_WEIGHTX01_1(x)			((x) << 16)
#define FLITE_REG_WEIGHTX01_0(x)			((x) << 0)

#define FLITE_REG_WEIGHTY01				(0x15C)
#define FLITE_REG_WEIGHTY01_1(x)			((x) << 16)
#define FLITE_REG_WEIGHTY01_0(x)			((x) << 0)

static void flite_hw_enable_bns(unsigned long __iomem *base_reg, bool enable)
{
	u32 cfg = 0;

	/* enable */
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGON));
	cfg |= FLITE_REG_BINNINGON_CLKGATE_ON(enable);
	cfg |= FLITE_REG_BINNINGON_EN(enable);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGON));
}

static void flite_hw_s_coeff_bns(unsigned long __iomem *base_reg,
	u32 factor_x, u32 factor_y)
{
	u32 cfg = 0;

	/* control */
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGCTRL));
	cfg |= FLITE_REG_BINNINGCTRL_FACTOR_Y(factor_y);
	cfg |= FLITE_REG_BINNINGCTRL_FACTOR_X(factor_x);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGCTRL));

	/* coefficient */
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_WEIGHTX01));
	cfg |= FLITE_REG_WEIGHTX01_1(0x40);
	cfg |= FLITE_REG_WEIGHTX01_0(0xC0);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_WEIGHTX01));

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_WEIGHTY01));
	cfg |= FLITE_REG_WEIGHTY01_1(0x40);
	cfg |= FLITE_REG_WEIGHTY01_0(0xC0);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_WEIGHTY01));
}

static void flite_hw_s_size_bns(unsigned long __iomem *base_reg,
	u32 width, u32 height, u32 otf_width, u32 otf_height)
{
	u32 cfg = 0;

	/* size */
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGTOTAL));
	cfg |= FLITE_REG_BINNINGTOTAL_HEIGHT(height);
	cfg |= FLITE_REG_BINNINGTOTAL_WIDTH(width);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGTOTAL));

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGINPUT));
	cfg |= FLITE_REG_BINNINGINPUT_HEIGHT(height);
	cfg |= FLITE_REG_BINNINGINPUT_WIDTH(width);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGINPUT));

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGMARGIN));
	cfg |= FLITE_REG_BINNINGMARGIN_TOP(0);
	cfg |= FLITE_REG_BINNINGMARGIN_LEFT(0);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGMARGIN));

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGOUTPUT));
	cfg |= FLITE_REG_BINNINGOUTPUT_HEIGHT(otf_height);
	cfg |= FLITE_REG_BINNINGOUTPUT_WIDTH(otf_width);
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_BINNINGOUTPUT));
}

static int flite_hw_set_bns(unsigned long __iomem *base_reg,
        struct fimc_is_image *image)
{
	int ret = 0;
	u32 width, height;
	u32 otf_width, otf_height;
	u32 factor_x, factor_y;

	BUG_ON(!image);

	width = image->window.width;
	height = image->window.height;
	otf_width = image->window.otf_width;
	otf_height = image->window.otf_height;

	if (otf_width == width && otf_height == height) {
		info("input & output sizes are same(%d, %d)\n", otf_width, otf_height);
		goto exit;
	}

	if (otf_width == 0 || otf_height == 0) {
		warn("bns size is zero. s_ctrl(V4L2_CID_IS_S_BNS) first\n");
		goto exit;
	}

	factor_x = 2 * width / otf_width;
	factor_y = 2 * height / otf_height;

	flite_hw_s_size_bns(base_reg, width, height, otf_width, otf_height);

	flite_hw_s_coeff_bns(base_reg, factor_x, factor_y);

	flite_hw_enable_bns(base_reg, true);

	info("BNS in(%d, %d), BNS out(%d, %d), ratio(%d, %d)\n",
	width, height, otf_width, otf_height, factor_x, factor_y);
exit:
	return ret;
}

static void flite_hw_set_cam_source_size(unsigned long __iomem *base_reg,
	struct fimc_is_image *image)
{
	u32 cfg = 0;

	BUG_ON(!image);

	cfg |= FLITE_REG_CISRCSIZE_SIZE_H(image->window.o_width);
	cfg |= FLITE_REG_CISRCSIZE_SIZE_V(image->window.o_height);

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CISRCSIZE));
}

static void flite_hw_set_dma_offset(unsigned long __iomem *base_reg,
	struct fimc_is_image *image)
{
	u32 cfg = 0;

	BUG_ON(!image);

	/* HACK */
	if (image->format.pixelformat == V4L2_PIX_FMT_SBGGR12)
		cfg |= FLITE_REG_CIOCAN_OCAN_H(roundup(image->window.o_width, 10));
	else
		cfg |= FLITE_REG_CIOCAN_OCAN_H(image->window.o_width);

	cfg |= FLITE_REG_CIOCAN_OCAN_V(image->window.o_height);

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIOCAN));
}

static void flite_hw_set_cam_channel(unsigned long __iomem *base_reg,
	u32 otf_setting)
{
	u32 cfg = 0;

	cfg |= otf_setting;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGENERAL));
}

static void flite_hw_set_capture_start(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIIMGCPT));
	cfg |= FLITE_REG_CIIMGCPT_IMGCPTEN;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIIMGCPT));
}

static void flite_hw_set_capture_stop(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIIMGCPT));
	cfg &= ~FLITE_REG_CIIMGCPT_IMGCPTEN;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIIMGCPT));
}

static int flite_hw_set_source_format(unsigned long __iomem *base_reg, struct fimc_is_image *image)
{
	u32 cfg = 0;

	BUG_ON(!image);

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	if (image->format.pixelformat == V4L2_PIX_FMT_SGRBG8)
		cfg |= FLITE_REG_CIGCTRL_RAW8;
	else
		cfg |= FLITE_REG_CIGCTRL_RAW10;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	return 0;
}

static void flite_hw_set_dma_fmt(unsigned long __iomem *base_reg,
	u32 pixelformat)
{
	u32 cfg = 0;

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIODMAFMT));

	if (pixelformat == V4L2_PIX_FMT_SBGGR12)
		cfg |= FLITE_REG_CIODMAFMT_PACK12;
	else
		cfg |= FLITE_REG_CIODMAFMT_NORMAL;

	if (pixelformat == V4L2_PIX_FMT_SGRBG8)
		cfg |= FLITE_REG_CIODMAFMT_1D_DMA;
	else
		cfg |= FLITE_REG_CIODMAFMT_2D_DMA;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIODMAFMT));
}

static void flite_hw_set_output_dma(unsigned long __iomem *base_reg, bool enable,
	u32 pixelformat)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	if (enable) {
		cfg &= ~FLITE_REG_CIGCTRL_ODMA_DISABLE;
		flite_hw_set_dma_fmt(base_reg, pixelformat);
	} else {
		cfg |= FLITE_REG_CIGCTRL_ODMA_DISABLE;
	}

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static void flite_hw_set_output_local(unsigned long __iomem *base_reg, bool enable)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	if (enable)
		cfg &= ~FLITE_REG_CIGCTRL_OLOCAL_DISABLE;
	else
		cfg |= FLITE_REG_CIGCTRL_OLOCAL_DISABLE;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

/* will use for pattern generation testing
static void flite_hw_set_test_pattern_enable(void)
{
	u32 cfg = 0;
	cfg = readl(flite_reg_base + FLITE_REG_CIGCTRL);
	cfg |= FLITE_REG_CIGCTRL_TEST_PATTERN_COLORBAR;

	writel(cfg, flite_reg_base + FLITE_REG_CIGCTRL);
}
*/

static void flite_hw_set_config_irq(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
	cfg &= ~(FLITE_REG_CIGCTRL_INVPOLPCLK | FLITE_REG_CIGCTRL_INVPOLVSYNC
			| FLITE_REG_CIGCTRL_INVPOLHREF);

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static void flite_hw_set_interrupt_source(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* for checking stop complete */
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_LASTEN0_DISABLE;

	/* for checking frame start */
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_STARTEN0_DISABLE;

	/* for checking frame end */
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_ENDEN0_DISABLE;

	/* for checking overflow */
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static void flite_hw_clr_interrupt_source(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* for checking stop complete */
	cfg |= FLITE_REG_CIGCTRL_IRQ_LASTEN0_DISABLE;

	/* for checking frame start */
	cfg |= FLITE_REG_CIGCTRL_IRQ_STARTEN0_DISABLE;

	/* for checking frame end */
	cfg |= FLITE_REG_CIGCTRL_IRQ_ENDEN0_DISABLE;

	/* for checking overflow */
	cfg |= FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static void flite_hw_set_ovf_interrupt_source(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* for checking overflow */
	cfg &= ~FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static void flite_hw_clr_ovf_interrupt_source(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* for checking overflow */
	cfg |= FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static int flite_hw_check_ovf_interrupt_source(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* for checking overflow */
	if (cfg & FLITE_REG_CIGCTRL_IRQ_OVFEN0_DISABLE)
		return true;

	return false;
}

static void flite_hw_force_reset(unsigned long __iomem *base_reg)
{
	u32 cfg = 0, retry = 100;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* request sw reset */
	cfg |= FLITE_REG_CIGCTRL_SWRST_REQ;
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	/* checking reset ready */
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
	while (retry-- && !(cfg & FLITE_REG_CIGCTRL_SWRST_RDY))
		cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	if (!(cfg & FLITE_REG_CIGCTRL_SWRST_RDY))
		warn("[CamIF] sw reset is not read but forcelly");

	/* sw reset */
	cfg |= FLITE_REG_CIGCTRL_SWRST;
	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
	warn("[CamIF] sw reset");
}

static void flite_hw_set_camera_type(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;
	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));

	cfg |= FLITE_REG_CIGCTRL_SELCAM_MIPI;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIGCTRL));
}

static void flite_hw_set_window_offset(unsigned long __iomem *base_reg,
        struct fimc_is_image *image)
{
	u32 cfg = 0;
	u32 hoff2, voff2;

	BUG_ON(!image);

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIWDOFST));
	cfg &= ~(FLITE_REG_CIWDOFST_HOROFF_MASK |
		FLITE_REG_CIWDOFST_VEROFF_MASK);
	cfg |= FLITE_REG_CIWDOFST_WINOFSEN |
		FLITE_REG_CIWDOFST_WINHOROFST(image->window.offs_h) |
		FLITE_REG_CIWDOFST_WINVEROFST(image->window.offs_v);

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIWDOFST));

	hoff2 = image->window.o_width - image->window.width - image->window.offs_h;
	voff2 = image->window.o_height - image->window.height - image->window.offs_v;
	cfg = FLITE_REG_CIWDOFST2_WINHOROFST2(hoff2) |
		FLITE_REG_CIWDOFST2_WINVEROFST2(voff2);

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CIWDOFST2));
}

static void flite_hw_set_last_capture_end_clear(unsigned long __iomem *base_reg)
{
	u32 cfg = 0;

	cfg = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS2));
	cfg &= ~FLITE_REG_CISTATUS2_LASTCAPEND;

	writel(cfg, base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS2));
}

int flite_hw_get_present_frame_buffer(unsigned long __iomem *base_reg)
{
	u32 status = 0;

	status = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS3));
	status &= FLITE_REG_CISTATUS3_PRESENT_MASK;

	return status;
}

int flite_hw_get_status2(unsigned long __iomem *base_reg)
{
	u32 status = 0;

	status = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS2));

	return status;
}

void flite_hw_set_status1(unsigned long __iomem *base_reg, u32 val)
{
	writel(val, base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS));
}

int flite_hw_get_status1(unsigned long __iomem *base_reg)
{
	u32 status = 0;

	status = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS));

	return status;
}

int flite_hw_getnclr_status1(unsigned long __iomem *base_reg)
{
	u32 status = 0;

	status = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS));
	writel(0, base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS));

	return status;
}

void flite_hw_set_status2(unsigned long __iomem *base_reg, u32 val)
{
	writel(val, base_reg + TO_WORD_OFFSET(FLITE_REG_CISTATUS2));
}

void flite_hw_set_start_addr(unsigned long __iomem *base_reg, u32 number, u32 addr)
{
	unsigned long __iomem *target_reg;

	if (number == 0) {
		target_reg = base_reg + TO_WORD_OFFSET(0x30);
	} else {
		number--;
		target_reg = base_reg + TO_WORD_OFFSET(0x200 + (0x4*number));
	}

	writel(addr, target_reg);
}

void flite_hw_set_use_buffer(unsigned long __iomem *base_reg, u32 number)
{
	u32 buffer;
	buffer = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIFCNTSEQ));
	buffer |= 1<<number;
	writel(buffer, base_reg + TO_WORD_OFFSET(FLITE_REG_CIFCNTSEQ));
}

void flite_hw_set_unuse_buffer(unsigned long __iomem *base_reg, u32 number)
{
	u32 buffer;
	buffer = readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIFCNTSEQ));
	buffer &= ~(1<<number);
	writel(buffer, base_reg + TO_WORD_OFFSET(FLITE_REG_CIFCNTSEQ));
}

u32 flite_hw_get_buffer_seq(unsigned long __iomem *base_reg)
{
	return readl(base_reg + TO_WORD_OFFSET(FLITE_REG_CIFCNTSEQ));
}

int init_fimc_lite(unsigned long __iomem *base_reg)
{
	int i;

	writel(0, base_reg + TO_WORD_OFFSET(FLITE_REG_CIFCNTSEQ));

	for (i = 0; i < 32; i++)
		flite_hw_set_start_addr(base_reg , i, 0xffffffff);

	return 0;
}

static int start_fimc_lite(unsigned long __iomem *base_reg,
	struct fimc_is_image *image, u32 otf_setting, u32 bns)
{
	flite_hw_set_cam_channel(base_reg, otf_setting);
	flite_hw_set_cam_source_size(base_reg, image);
	flite_hw_set_dma_offset(base_reg, image);
	flite_hw_set_camera_type(base_reg);
	flite_hw_set_source_format(base_reg, image);
	/*flite_hw_set_output_dma(mipi_reg_base, false);
	flite_hw_set_output_local(base_reg, false);*/

	flite_hw_set_interrupt_source(base_reg);
	/*flite_hw_set_interrupt_starten0_disable(mipi_reg_base);*/
	flite_hw_set_config_irq(base_reg);
	flite_hw_set_window_offset(base_reg, image);
	/* flite_hw_set_test_pattern_enable(); */

	if (bns)
		flite_hw_set_bns(base_reg, image);

	flite_hw_set_last_capture_end_clear(base_reg);
	flite_hw_set_capture_start(base_reg);

	/*dbg_front("lite config : %08X\n",
		*((unsigned int*)(base_reg + FLITE_REG_CIFCNTSEQ)));*/

	return 0;
}

static inline void stop_fimc_lite(unsigned long __iomem *base_reg)
{
	flite_hw_set_capture_stop(base_reg);
}

static inline void flite_s_buffer_addr(struct fimc_is_device_flite *device,
	u32 bindex, u32 baddr)
{
	flite_hw_set_start_addr(device->base_reg, bindex, baddr);
}

static inline int flite_s_use_buffer(struct fimc_is_device_flite *flite,
	u32 bindex)
{
	int ret = 0;
	unsigned long target_time;

	BUG_ON(!flite);

	if (!atomic_read(&flite->bcount)) {
		if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY) {
			target_time = jiffies +
				msecs_to_jiffies(flite->buf_done_wait_time);
			while ((target_time > jiffies) &&
					(flite_hw_get_status1(flite->base_reg) && (7 << 20)))
				pr_debug("over vblank (early buffer done)");
		}

		if (flite_hw_get_status1(flite->base_reg) && (7 << 20)) {
			merr("over vblank (buf-mode : %d)", flite, flite->buf_done_mode);
			ret = -EINVAL;
			goto p_err;
		}

		flite_hw_set_use_buffer(flite->base_reg, bindex);
		atomic_inc(&flite->bcount);
		flite_hw_set_output_dma(flite->base_reg, true, flite->image.format.pixelformat);
	} else {
		flite_hw_set_use_buffer(flite->base_reg, bindex);
		atomic_inc(&flite->bcount);
	}

p_err:
	return ret;
}

static inline int flite_s_unuse_buffer(struct fimc_is_device_flite *flite,
	u32 bindex)
{
	int ret = 0;
	unsigned long target_time;

	BUG_ON(!flite);

	if (atomic_read(&flite->bcount) == 1) {
		if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY) {
			target_time = jiffies +
				msecs_to_jiffies(flite->buf_done_wait_time);
			while ((target_time > jiffies) &&
					(flite_hw_get_status1(flite->base_reg) && (7 << 20)))
				pr_debug("over vblank (early buffer done)");
		}

		if (flite_hw_get_status1(flite->base_reg) && (7 << 20)) {
			merr("over vblank (buf-mode : %d)", flite, flite->buf_done_mode);
			ret = -EINVAL;
			goto p_err;
		}

		flite_hw_set_output_dma(flite->base_reg, false, flite->image.format.pixelformat);
		flite_hw_set_unuse_buffer(flite->base_reg, bindex);
		atomic_dec(&flite->bcount);
	} else {
		flite_hw_set_unuse_buffer(flite->base_reg, bindex);
		atomic_dec(&flite->bcount);
	}

p_err:
	return ret;
}

static u32 g_print_cnt;
#define LOG_INTERVAL_OF_DROPS 30
static void tasklet_flite_str0(unsigned long data)
{
	struct v4l2_subdev *subdev;
	struct fimc_is_device_flite *flite;
	struct fimc_is_device_sensor *device;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group_3aa, *group_isp;
	u32 bstart, fcount, present;

	subdev = (struct v4l2_subdev *)data;
	flite = v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		BUG();
	}

	device = v4l2_get_subdev_hostdata(subdev);
	if (!device) {
		err("device is NULL");
		BUG();
	}

	present = flite_hw_get_present_frame_buffer(flite->base_reg) - 1;
	bstart = flite->tasklet_param_str;
	fcount = atomic_read(&flite->fcount);
	ischain = device->ischain;

#ifdef TASKLET_MSG
	info("S%d %d\n", bstart, fcount);
#endif

	/* comparing sw state and hw state */
	if (atomic_read(&flite->bcount) == 2) {
		if ((bstart == FLITE_A_SLOT_VALID) &&
			(present != FLITE_A_SLOT_VALID)) {
			err("invalid state1(sw:%d != hw:%d)", bstart, present);
			flite->sw_trigger = bstart = FLITE_B_SLOT_VALID;
		}

		if ((bstart == FLITE_B_SLOT_VALID) &&
			(present != FLITE_B_SLOT_VALID)) {
			err("invalid state2(sw:%d != hw:%d)", bstart, present);
			flite->sw_trigger = bstart = FLITE_A_SLOT_VALID;
		}
	}

	groupmgr = ischain->groupmgr;
	group_3aa = &ischain->group_3aa;
	group_isp = &ischain->group_isp;
	if (unlikely(list_empty(&group_3aa->smp_trigger.wait_list))) {
		atomic_set(&group_3aa->sensor_fcount, fcount + group_3aa->async_shots);

		/*
		 * pcount : program count
		 * current program count(location) in kthread
		 */
		if (((g_print_cnt % LOG_INTERVAL_OF_DROPS) == 0) ||
			(g_print_cnt < LOG_INTERVAL_OF_DROPS)) {
			info("grp1(res %d, rcnt %d, scnt %d), "
				"grp2(res %d, rcnt %d, scnt %d), "
				"fcount %d(%d, %d) pcount %d\n",
				groupmgr->group_smp_res[group_3aa->id].count,
				atomic_read(&group_3aa->rcount),
				atomic_read(&group_3aa->scount),
				groupmgr->group_smp_res[group_isp->id].count,
				atomic_read(&group_isp->rcount),
				atomic_read(&group_isp->scount),
				fcount + group_3aa->async_shots,
				*last_fcount0, *last_fcount1, group_3aa->pcount);
		}
		g_print_cnt++;
	} else {
		g_print_cnt = 0;
		atomic_set(&group_3aa->sensor_fcount, fcount + group_3aa->async_shots);
		up(&group_3aa->smp_trigger);
	}

	v4l2_subdev_notify(subdev, FLITE_NOTIFY_FSTART, &fcount);
}

static void tasklet_flite_str1(unsigned long data)
{
	struct v4l2_subdev *subdev;
	struct fimc_is_device_flite *flite;
	u32 bstart, fcount, present;

	subdev = (struct v4l2_subdev *)data;
	flite = v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		BUG();
	}

	present = flite_hw_get_present_frame_buffer(flite->base_reg) - 1;
	bstart = flite->tasklet_param_str;
	fcount = atomic_read(&flite->fcount);

#ifdef TASKLET_MSG
	info("S%d %d\n", bstart, fcount);
#endif

	/* comparing sw state and hw state */
	if (atomic_read(&flite->bcount) == 2) {
		if ((bstart == FLITE_A_SLOT_VALID) &&
			(present != FLITE_A_SLOT_VALID)) {
			err("invalid state1(sw:%d != hw:%d)", bstart, present);
			flite->sw_trigger = bstart = FLITE_B_SLOT_VALID;
		}

		if ((bstart == FLITE_B_SLOT_VALID) &&
			(present != FLITE_B_SLOT_VALID)) {
			err("invalid state2(sw:%d != hw:%d)", bstart, present);
			flite->sw_trigger = bstart = FLITE_A_SLOT_VALID;
		}
	}

	if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY) {
		flite->early_work_called = false;
		if (flite->early_work_skip == true) {
			info("flite early work skip");
			flite->early_work_skip = false;
		} else {
			queue_delayed_work(flite->early_workqueue, &flite->early_work_wq,
					msecs_to_jiffies(flite->buf_done_wait_time));
		}
	}
	v4l2_subdev_notify(subdev, FLITE_NOTIFY_FSTART, &fcount);
}

static void tasklet_flite_end(unsigned long data)
{
	struct fimc_is_device_flite *flite;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_frame *frame_done;
	struct v4l2_subdev *subdev;
	u32 bdone;

	frame_done = NULL;
	subdev = (struct v4l2_subdev *)data;
	flite = v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		BUG();
	}

	if ((flite->buf_done_mode == FLITE_BUF_DONE_EARLY) &&
		test_bit(FLITE_LAST_CAPTURE, &flite->state)) {
		info("Skip due to Last Frame Capture\n");
		return;
	}

	framemgr = flite->framemgr;
	bdone = flite->tasklet_param_end;

#ifdef TASKLET_MSG
	info("E%d %d\n", bdone, atomic_read(&flite->fcount));
#endif

	if (flite_hw_check_ovf_interrupt_source(flite->base_reg))
		flite_hw_set_ovf_interrupt_source(flite->base_reg);

	framemgr_e_barrier(framemgr, FMGR_IDX_1 + bdone);

	if (test_bit(bdone, &flite->state)) {
		fimc_is_frame_process_head(framemgr, &frame);
		if (frame) {
#ifdef MEASURE_TIME
#ifdef EXTERNAL_TIME
			do_gettimeofday(&frame->tzone[TM_FLITE_END]);
#endif
#endif
			/* 1. current frame transition to completion */
			frame_done = frame;
			fimc_is_frame_trans_pro_to_com(framemgr, frame);

			/* 2. next frame ready */
			fimc_is_frame_request_head(framemgr, &frame);
			if (frame) {
				flite_s_buffer_addr(flite, bdone,
					frame->dvaddr_buffer[0]);
				set_bit(bdone, &flite->state);
				fimc_is_frame_trans_req_to_pro(framemgr, frame);
			} else {
				if (!flite_s_unuse_buffer(flite, bdone)) {
					clear_bit(bdone, &flite->state);
#ifdef TASKLET_MSG
					merr("[SEN] request is empty0(%d slot)", flite, bdone);
#endif
				}
			}
		} else {
#ifdef TASKLET_MSG
			merr("[SEN] process is empty(%d, %ld)", flite, bdone, flite->state);
			fimc_is_frame_print_all(framemgr);
#endif
		}
	} else {
		fimc_is_frame_request_head(framemgr, &frame);
		if (frame) {
			flite_s_buffer_addr(flite, bdone, frame->dvaddr_buffer[0]);
			if (!flite_s_use_buffer(flite, bdone)) {
				set_bit(bdone, &flite->state);
				fimc_is_frame_trans_req_to_pro(framemgr, frame);
			}
		} else {
#ifdef TASKLET_MSG
			merr("request is empty1(%d slot)", flite, bdone);
			fimc_is_frame_print_all(framemgr);
#endif
		}
	}

	framemgr_x_barrier(framemgr, FMGR_IDX_1 + bdone);

	v4l2_subdev_notify(subdev, FLITE_NOTIFY_FEND, frame_done);
	flite->early_work_called = true;
}

static void tasklet_flite_end_chk(unsigned long data)
{
	struct v4l2_subdev *subdev = NULL;
	struct fimc_is_device_flite *flite = NULL;
	subdev = (struct v4l2_subdev *)data;
	flite = v4l2_get_subdevdata(subdev);

	if (flite->early_work_called == false) {
		u32 fcount = atomic_read(&flite->fcount);
		info("[CamIF%d][%08d] delayed work queue was slower than end irq",
			flite->instance, fcount);
	}

	return;
}

#ifdef SUPPORTED_EARLY_BUF_DONE
static void wq_func_flite_early_work(struct work_struct *data)
{
	struct fimc_is_device_flite *flite = NULL;
	struct delayed_work *early_work_wq = NULL;

	early_work_wq = container_of(data, struct delayed_work,
		work);
	flite = container_of(early_work_wq, struct fimc_is_device_flite,
		early_work_wq);

	if (!flite) {
		err("flite is NULL");
		BUG();
	}

	flite->tasklet_param_end = flite->sw_trigger;
	tasklet_schedule(&flite->tasklet_flite_early_end);
}

static void chk_early_buf_done(struct fimc_is_device_flite *flite, u32 framerate, u32 position)
{
	/* ms */
	u32 margin = 0;
	u32 duration = 0;

	/* HACK: applied on 15~30fps forcely */
	if (framerate > 15 && framerate <= 30) {

		duration = 1000 / framerate;

		if (position == SENSOR_POSITION_REAR)
			flite->early_buf_done_mode = FLITE_BUF_EARLY_30P;
		else
			flite->early_buf_done_mode = FLITE_BUF_EARLY_20P;

		margin = FLITE_VVALID_TIME_BASE * (flite->early_buf_done_mode * 0.1f);

		if (margin >= duration) {
			/* normal buffer done mode */
			flite->buf_done_mode = FLITE_BUF_DONE_NORMAL;
			flite->early_buf_done_mode = FLITE_BUF_EARLY_NOTHING;
			flite->buf_done_wait_time = 0;
		} else {
			/* early buffer done mode */
			flite->buf_done_mode = FLITE_BUF_DONE_EARLY;
			flite->buf_done_wait_time = duration - margin;
		}
	} else {
		/* normal buffer done mode */
		flite->buf_done_mode = FLITE_BUF_DONE_NORMAL;
		flite->early_buf_done_mode = FLITE_BUF_EARLY_NOTHING;
		flite->buf_done_wait_time = 0;
	}

	info("[CamIF%d] buffer done mode [m%d/em%d/du%d/mg%d/wt%d]", flite->instance,
		flite->buf_done_mode, flite->early_buf_done_mode, duration, margin,
		flite->buf_done_wait_time);
}
#endif

static inline void notify_fcount(u32 channel, u32 fcount)
{
	if (channel == FLITE_ID_A)
		writel(fcount, notify_fcount_sen0);
	else if (channel == FLITE_ID_B)
		writel(fcount, notify_fcount_sen1);
	else if (channel == FLITE_ID_C)
		writel(fcount, notify_fcount_sen2);
	else
		err("unresolved channel(%d)", channel);
}

static irqreturn_t fimc_is_flite_isr(int irq, void *data)
{
	u32 status, status1, status2, i;
	struct fimc_is_device_flite *flite;

	flite = data;
	status1 = flite_hw_getnclr_status1(flite->base_reg);
	status = status1 & (3 << 4);

	if (test_bit(FLITE_LAST_CAPTURE, &flite->state)) {
		if (status1) {
			info("[CamIF%d] last status1 : 0x%08X\n", flite->instance, status1);
			goto clear_status;
		}

		err("[CamIF%d] unintended intr is occured", flite->instance);

		for (i = 0; i < 278; i += 4)
			info("REG[%X] : 0x%08X\n", i, readl(flite->base_reg + i));

		flite_hw_force_reset(flite->base_reg);

		goto clear_status;
	}

	if (status) {
		if (status == (3 << 4)) {
#ifdef DBG_FLITEISR
			printk(KERN_CONT "*");
#endif
			/* frame both interrupt since latency */
			if (flite->sw_checker) {
#ifdef DBG_FLITEISR
				printk(KERN_CONT ">");
#endif
				/* frame end interrupt */
				flite->sw_checker = EXPECT_FRAME_START;
				flite->tasklet_param_end = flite->sw_trigger;
				tasklet_schedule(&flite->tasklet_flite_end);
#ifdef DBG_FLITEISR
				printk(KERN_CONT "<");
#endif
				/* frame start interrupt */
				flite->sw_checker = EXPECT_FRAME_END;
				if (flite->sw_trigger)
					flite->sw_trigger = FLITE_A_SLOT_VALID;
				else
					flite->sw_trigger = FLITE_B_SLOT_VALID;
				flite->tasklet_param_str = flite->sw_trigger;
				atomic_inc(&flite->fcount);
				notify_fcount(flite->instance, atomic_read(&flite->fcount));
				tasklet_schedule(&flite->tasklet_flite_str);
			} else {
				/* W/A: Skip start tasklet at interrupt lost case */
				warn("[CamIF%d] invalid interrupt interval",
					flite->instance);
				goto clear_status;
/* HACK: Disable dead code because of Prevent Issue */
#if 0
#ifdef DBG_FLITEISR
				printk(KERN_CONT "<");
#endif
				/* frame start interrupt */
				flite->sw_checker = EXPECT_FRAME_END;
				if (flite->sw_trigger)
					flite->sw_trigger = FLITE_A_SLOT_VALID;
				else
					flite->sw_trigger = FLITE_B_SLOT_VALID;
				flite->tasklet_param_str = flite->sw_trigger;
				atomic_inc(&flite->fcount);
				notify_fcount(flite->instance, atomic_read(&flite->fcount));
				if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY)
					flite->early_work_skip = true;
				tasklet_schedule(&flite->tasklet_flite_str);
#ifdef DBG_FLITEISR
				printk(KERN_CONT ">");
#endif
				/* frame end interrupt */
				flite->sw_checker = EXPECT_FRAME_START;
				flite->tasklet_param_end = flite->sw_trigger;
				if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY)
					tasklet_schedule(&flite->tasklet_flite_early_end);
				tasklet_schedule(&flite->tasklet_flite_end);
#endif
			}
		} else if (status == (2 << 4)) {
			/* W/A: Skip start tasklet at interrupt lost case */
			if (flite->sw_checker != EXPECT_FRAME_START) {
				warn("[CamIF%d] Lost end interupt\n",
					flite->instance);
				goto clear_status;
			}
#ifdef DBG_FLITEISR
			printk(KERN_CONT "<");
#endif
			/* frame start interrupt */
			flite->sw_checker = EXPECT_FRAME_END;
			if (flite->sw_trigger)
				flite->sw_trigger = FLITE_A_SLOT_VALID;
			else
				flite->sw_trigger = FLITE_B_SLOT_VALID;
			flite->tasklet_param_str = flite->sw_trigger;
			atomic_inc(&flite->fcount);
			notify_fcount(flite->instance, atomic_read(&flite->fcount));
			tasklet_schedule(&flite->tasklet_flite_str);
		} else {
			/* W/A: Skip end tasklet at interrupt lost case */
			if (flite->sw_checker != EXPECT_FRAME_END) {
				warn("[CamIF%d] Lost start interupt\n",
					flite->instance);
				goto clear_status;
			}
#ifdef DBG_FLITEISR
			printk(KERN_CONT ">");
#endif
			/* frame end interrupt */
			flite->sw_checker = EXPECT_FRAME_START;
			if (flite->buf_done_mode == FLITE_BUF_DONE_NORMAL)
				flite->tasklet_param_end = flite->sw_trigger;
			tasklet_schedule(&flite->tasklet_flite_end);
		}
	}

clear_status:
	if (status1 & (1 << 6)) {
		/* Last Frame Capture Interrupt */
		info("[CamIF%d] Last Frame Capture(fcount : %d)\n",
			flite->instance, atomic_read(&flite->fcount));

		/* Clear LastCaptureEnd bit */
		status2 = flite_hw_get_status2(flite->base_reg);
		status2 &= ~(0x1 << 1);
		flite_hw_set_status2(flite->base_reg, status2);

		/* Notify last capture */
		set_bit(FLITE_LAST_CAPTURE, &flite->state);
		wake_up(&flite->wait_queue);
	}

	if (status1 & (1 << 8)) {
		u32 ciwdofst;

		flite_hw_clr_ovf_interrupt_source(flite->base_reg);

		if (flite->overflow_cnt % FLITE_OVERFLOW_COUNT == 0)
			pr_err("[CamIF%d] OFCR(cnt:%u)\n", flite->instance, flite->overflow_cnt);
		ciwdofst = readl(flite->base_reg + 0x10);
		ciwdofst  |= (0x1 << 14);
		writel(ciwdofst, flite->base_reg + 0x10);
		ciwdofst  &= ~(0x1 << 14);
		writel(ciwdofst, flite->base_reg + 0x10);
		flite->overflow_cnt++;
	}

	if (status1 & (1 << 9)) {
		u32 ciwdofst;

		flite_hw_clr_ovf_interrupt_source(flite->base_reg);

		if (flite->overflow_cnt % FLITE_OVERFLOW_COUNT == 0)
			pr_err("[CamIF%d] OFCB(cnt:%u)\n", flite->instance, flite->overflow_cnt);
		ciwdofst = readl(flite->base_reg + 0x10);
		ciwdofst  |= (0x1 << 15);
		writel(ciwdofst, flite->base_reg + 0x10);
		ciwdofst  &= ~(0x1 << 15);
		writel(ciwdofst, flite->base_reg + 0x10);
		flite->overflow_cnt++;
	}

	if (status1 & (1 << 10)) {
		u32 ciwdofst;

		flite_hw_clr_ovf_interrupt_source(flite->base_reg);

		if (flite->overflow_cnt % FLITE_OVERFLOW_COUNT == 0)
			pr_err("[CamIF%d] OFY(cnt:%u)\n", flite->instance, flite->overflow_cnt);
		ciwdofst = readl(flite->base_reg + 0x10);
		ciwdofst  |= (0x1 << 30);
		writel(ciwdofst, flite->base_reg + 0x10);
		ciwdofst  &= ~(0x1 << 30);
		writel(ciwdofst, flite->base_reg + 0x10);
		flite->overflow_cnt++;
	}

	return IRQ_HANDLED;
}

int fimc_is_flite_open(struct v4l2_subdev *subdev,
	struct fimc_is_framemgr *framemgr)
{
	int ret = 0;
	struct fimc_is_device_flite *flite;

	BUG_ON(!subdev);
	BUG_ON(!framemgr);

	flite = v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	flite->group = 0;
	flite->framemgr = framemgr;
	atomic_set(&flite->fcount, 0);
	atomic_set(&flite->bcount, 0);

	clear_bit(FLITE_OTF_WITH_3AA, &flite->state);
	clear_bit(FLITE_LAST_CAPTURE, &flite->state);
	clear_bit(FLITE_A_SLOT_VALID, &flite->state);
	clear_bit(FLITE_B_SLOT_VALID, &flite->state);

	switch(flite->instance) {
	case FLITE_ID_A:
		ret = request_irq(IRQ_FIMC_LITE0,
			fimc_is_flite_isr,
			IRQF_SHARED,
			"fimc-lite0",
			flite);
		if (ret)
			err("request_irq(L0) failed\n");
		break;
	case FLITE_ID_B:
		ret = request_irq(IRQ_FIMC_LITE1,
			fimc_is_flite_isr,
			IRQF_SHARED,
			"fimc-lite1",
			flite);
		if (ret)
			err("request_irq(L1) failed\n");
		break;
	case FLITE_ID_C:
#if defined(CONFIG_ARCH_EXYNOS4) || defined(CONFIG_SOC_EXYNOS5260)
		ret = request_irq(IRQ_FIMC_LITE1,
			fimc_is_flite_isr,
			IRQF_SHARED,
			"fimc-lite2",
			flite);
#else
		ret = request_irq(IRQ_FIMC_LITE2,
			fimc_is_flite_isr,
			IRQF_SHARED,
			"fimc-lite2",
			flite);
#endif
		if (ret)
			err("request_irq(L2) failed\n");
		break;
	default:
		err("instance is invalid(%d)", flite->instance);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_flite_close(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_device_flite *flite;

	BUG_ON(!subdev);

	flite = v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch(flite->instance) {
	case FLITE_ID_A:
		free_irq(IRQ_FIMC_LITE0, flite);
		break;
	case FLITE_ID_B:
		free_irq(IRQ_FIMC_LITE1, flite);
		break;
	case FLITE_ID_C:
#if defined(CONFIG_ARCH_EXYNOS4) || defined(CONFIG_SOC_EXYNOS5260)
		free_irq(IRQ_FIMC_LITE1, flite);
#else
		free_irq(IRQ_FIMC_LITE2, flite);
#endif
		break;
	default:
		err("instance is invalid(%d)", flite->instance);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static int flite_stream_on(struct v4l2_subdev *subdev,
	struct fimc_is_device_flite *flite)
{
	int ret = 0;
	u32 otf_setting;
	bool buffer_ready;
	unsigned long flags;
	struct fimc_is_image *image;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_device_sensor *sensor = v4l2_get_subdev_hostdata(subdev);

	BUG_ON(!flite);
	BUG_ON(!flite->framemgr);

	otf_setting = 0;
	buffer_ready = false;
	framemgr = flite->framemgr;
	image = &flite->image;

	flite->overflow_cnt = 0;
	flite->sw_trigger = FLITE_B_SLOT_VALID;
	flite->sw_checker = EXPECT_FRAME_START;
	flite->tasklet_param_str = 0;
	flite->tasklet_param_end = 0;
	atomic_set(&flite->bcount, 0);
	clear_bit(FLITE_LAST_CAPTURE, &flite->state);
	clear_bit(FLITE_A_SLOT_VALID, &flite->state);
	clear_bit(FLITE_B_SLOT_VALID, &flite->state);

	flite_hw_force_reset(flite->base_reg);
	init_fimc_lite(flite->base_reg);

	framemgr_e_barrier_irqs(framemgr, 0, flags);

	if (framemgr->frame_req_cnt >= 1) {
		fimc_is_frame_request_head(framemgr, &frame);
		flite_s_use_buffer(flite, FLITE_A_SLOT_VALID);
		flite_s_buffer_addr(flite, FLITE_A_SLOT_VALID, frame->dvaddr_buffer[0]);
		set_bit(FLITE_A_SLOT_VALID, &flite->state);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		buffer_ready = true;
	}

	if (framemgr->frame_req_cnt >= 1) {
		fimc_is_frame_request_head(framemgr, &frame);
		flite_s_use_buffer(flite, FLITE_B_SLOT_VALID);
		flite_s_buffer_addr(flite, FLITE_B_SLOT_VALID, frame->dvaddr_buffer[0]);
		set_bit(FLITE_B_SLOT_VALID, &flite->state);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		buffer_ready = true;
	}

	framemgr_x_barrier_irqr(framemgr, 0, flags);

	flite_hw_set_output_dma(flite->base_reg, buffer_ready, image->format.pixelformat);

	if (test_bit(FLITE_OTF_WITH_3AA, &flite->state)) {
		tasklet_init(&flite->tasklet_flite_str, tasklet_flite_str0, (unsigned long)subdev);
		tasklet_init(&flite->tasklet_flite_end, tasklet_flite_end, (unsigned long)subdev);

		mdbgd_back("Enabling OTF path. target 3aa(%d)\n", flite, flite->group);
		if (flite->instance == FLITE_ID_A) {
			if (flite->group == GROUP_ID_3A0)
				otf_setting = FLITE_REG_CIGENERAL_CAM_A;
			else
				otf_setting = FLITE_REG_CIGENERAL_3AA1_CAM_A;
		} else if (flite->instance == FLITE_ID_B) {
			if (flite->group == GROUP_ID_3A0)
				otf_setting = FLITE_REG_CIGENERAL_CAM_B;
			else
				otf_setting = FLITE_REG_CIGENERAL_3AA1_CAM_B;
		} else {
			merr("invalid FLITE channel for OTF setting", flite);
			ret = -EINVAL;
			goto p_err;
		}

		flite_hw_set_output_local(flite->base_reg, true);
	} else {
		switch (flite->buf_done_mode) {
		case FLITE_BUF_DONE_NORMAL:
			tasklet_init(&flite->tasklet_flite_str, tasklet_flite_str1, (unsigned long)subdev);
			tasklet_init(&flite->tasklet_flite_end, tasklet_flite_end, (unsigned long)subdev);
			break;
		case FLITE_BUF_DONE_EARLY:
			flite->early_work_skip = false;
			flite->early_work_called = false;
			tasklet_init(&flite->tasklet_flite_str, tasklet_flite_str1, (unsigned long)subdev);
			tasklet_init(&flite->tasklet_flite_early_end, tasklet_flite_end, (unsigned long)subdev);
			tasklet_init(&flite->tasklet_flite_end, tasklet_flite_end_chk, (unsigned long)subdev);
			break;
		default:
			tasklet_init(&flite->tasklet_flite_str, tasklet_flite_str1, (unsigned long)subdev);
			tasklet_init(&flite->tasklet_flite_end, tasklet_flite_end, (unsigned long)subdev);
			break;
		}
		flite_hw_set_output_local(flite->base_reg, false);
	}
	start_fimc_lite(flite->base_reg, image, otf_setting, sensor->pdata->is_bns);

p_err:
	return ret;
}

static int flite_stream_off(struct v4l2_subdev *subdev,
	struct fimc_is_device_flite *flite,
	bool wait)
{
	int ret = 0;
	unsigned long __iomem *base_reg;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!flite);
	BUG_ON(!flite->base_reg);
	BUG_ON(!flite->framemgr);

	base_reg = flite->base_reg;
	framemgr = flite->framemgr;

	/* for preventing invalid memory access */
	flite_hw_set_unuse_buffer(base_reg, FLITE_A_SLOT_VALID);
	flite_hw_set_unuse_buffer(base_reg, FLITE_B_SLOT_VALID);
	flite_hw_set_output_dma(base_reg, false, flite->image.format.pixelformat);
	flite_hw_set_output_local(base_reg, false);

	stop_fimc_lite(base_reg);
	if (wait) {
		u32 timetowait;

		timetowait = wait_event_timeout(flite->wait_queue,
			test_bit(FLITE_LAST_CAPTURE, &flite->state),
			FIMC_IS_FLITE_STOP_TIMEOUT);

		if (!timetowait) {
			/* forcely stop */
			stop_fimc_lite(base_reg);
			set_bit(FLITE_LAST_CAPTURE, &flite->state);
			err("last capture timeout:%s", __func__);
			msleep(200);
			flite_hw_force_reset(base_reg);
			ret = -ETIME;
		}
	} else {
		if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY)
			flush_delayed_work(&flite->early_work_wq);
		/*
		 * DTP test can make iommu fault because senosr is streaming
		 * therefore it need  force reset
		 */
		flite_hw_force_reset(base_reg);
	}

	if (flite->buf_done_mode == FLITE_BUF_DONE_EARLY)
		cancel_delayed_work_sync(&flite->early_work_wq);

	/* clr interrupt source */
	flite_hw_clr_interrupt_source(base_reg);

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_3, flags);

	fimc_is_frame_complete_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_com_to_fre(framemgr, frame);
		fimc_is_frame_complete_head(framemgr, &frame);
	}

	fimc_is_frame_process_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_pro_to_fre(framemgr, frame);
		fimc_is_frame_process_head(framemgr, &frame);
	}

	fimc_is_frame_request_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_req_to_fre(framemgr, frame);
		fimc_is_frame_request_head(framemgr, &frame);
	}

	/* buffer done mode init */
	flite->buf_done_mode = FLITE_BUF_DONE_NORMAL;
	flite->early_buf_done_mode = FLITE_BUF_EARLY_NOTHING;

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_3, flags);

	return ret;
}

/*
 * enable
 * @X0 : disable
 * @X1 : enable
 * @1X : no waiting flag
 * @0X : waiting flag
 */
static int flite_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	bool nowait;
	struct fimc_is_device_flite *flite;

	BUG_ON(!subdev);

	nowait = (enable & FLITE_NOWAIT_MASK) >> FLITE_NOWAIT_SHIFT;
	enable = enable & FLITE_ENABLE_MASK;

	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (enable) {
		ret = flite_stream_on(subdev, flite);
		if (ret) {
			err("flite_stream_on is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = flite_stream_off(subdev, flite, nowait);
		if (ret) {
			err("flite_stream_off is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	mdbgd_back("%s(%d, %d)\n", flite, __func__, enable, ret);
	return 0;
}

static int flite_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct fimc_is_device_flite *flite;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	flite->image.window.offs_h = 0;
	flite->image.window.offs_v = 0;
	flite->image.window.width = fmt->width;
	flite->image.window.height = fmt->height;
	flite->image.window.o_width = fmt->width;
	flite->image.window.o_height = fmt->height;
	flite->image.format.pixelformat = fmt->code;

p_err:
	mdbgd_back("%s(%dx%d, %X)\n", flite, __func__, fmt->width, fmt->height, fmt->code);
	return ret;
}

static int flite_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_device_flite *flite;

	BUG_ON(!subdev);
	BUG_ON(!ctrl);

	flite = (struct fimc_is_device_flite *)v4l2_get_subdevdata(subdev);
	if (!flite) {
		err("flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_S_BNS:
		{
			u32 width, height, ratio;

			width = flite->image.window.width;
			height = flite->image.window.height;
			ratio = ctrl->value;

			flite->image.window.otf_width
				= rounddown((width * 1000 / ratio), 4);
			flite->image.window.otf_height
				= rounddown((height * 1000 / ratio), 2);
		}
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.s_ctrl = flite_s_ctrl,
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = flite_s_stream,
	.s_mbus_fmt = flite_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

int fimc_is_flite_probe(struct fimc_is_device_sensor *device,
	u32 instance)
{
	int ret = 0;
	struct v4l2_subdev *subdev_flite;
	struct fimc_is_device_flite *flite;

	BUG_ON(!device);

	subdev_flite = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_flite) {
		merr("subdev_flite is NULL", device);
		ret = -ENOMEM;
		goto err_alloc_subdev_flite;
	}
	device->subdev_flite = subdev_flite;

	flite = kzalloc(sizeof(struct fimc_is_device_flite), GFP_KERNEL);
	if (!flite) {
		merr("flite is NULL", device);
		ret = -ENOMEM;
		goto err_alloc_flite;
	}

	flite->instance = instance;
	init_waitqueue_head(&flite->wait_queue);
	switch(instance) {
	case FLITE_ID_A:
		flite->base_reg = (unsigned long *)S5P_VA_FIMCLITE0;
		break;
	case FLITE_ID_B:
		flite->base_reg = (unsigned long *)S5P_VA_FIMCLITE1;
		break;
	case FLITE_ID_C:
		flite->base_reg = (unsigned long *)S5P_VA_FIMCLITE2;
		break;
	default:
		err("instance is invalid(%d)", instance);
		ret = -EINVAL;
		goto err_invalid_instance;
	}

	v4l2_subdev_init(subdev_flite, &subdev_ops);
	v4l2_set_subdevdata(subdev_flite, (void *)flite);
	v4l2_set_subdev_hostdata(subdev_flite, device);
	snprintf(subdev_flite->name, V4L2_SUBDEV_NAME_SIZE, "flite-subdev.%d", instance);
	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_flite);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto err_reg_v4l2_subdev;
	}

	/* buffer done mode is normal (default) */
	flite->buf_done_mode = FLITE_BUF_DONE_NORMAL;
	flite->early_buf_done_mode = FLITE_BUF_EARLY_NOTHING;
	flite->chk_early_buf_done = NULL;
#ifdef SUPPORTED_EARLY_BUF_DONE
	flite->chk_early_buf_done = chk_early_buf_done;
	flite->early_workqueue = alloc_workqueue("fimc-is/early_workqueue/highpri", WQ_HIGHPRI, 0);
	if (!flite->early_workqueue) {
		warn("failed to alloc own workqueue, will be use global one");
		goto err_reg_v4l2_subdev;
	} else {
		INIT_DELAYED_WORK(&flite->early_work_wq, wq_func_flite_early_work);
	}
#endif
	info("[BAK:D:%d] %s(%d)\n", instance, __func__, ret);
	return 0;

err_reg_v4l2_subdev:
err_invalid_instance:
	kfree(flite);

err_alloc_flite:
	kfree(subdev_flite);
	device->subdev_flite = NULL;

err_alloc_subdev_flite:
	err("[BAK:D:%d] %s(%d)\n", instance, __func__, ret);
	return ret;
}
