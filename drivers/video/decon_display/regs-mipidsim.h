/* drivers/video/decon_display/regs-mipidsim.h
 *
 * Register definition file for Samsung MIPI-DSIM driver
 *
 * Copyright (c) 2011 Samsung Electronics
 * Haowei li <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_MIPIDSIM_H
#define _REGS_MIPIDSIM_H
#ifdef CONFIG_SOC_EXYNOS5430
#define S5P_DSIM_STATUS		(0x4)	/* Status register */
#define DSIM_STOP_STATE_DAT(x)	(((x) & 0xf) << 0)
#define DSIM_ULPS_DAT(x)	(((x) & 0xf) << 4)
#define DSIM_STOP_STATE_CLK	(1 << 8)
#define DSIM_ULPS_CLK		(1 << 9)
#define DSIM_TX_READY_HS_CLK	(1 << 10)

#define S5P_DSIM_SWRST		(0xc)	/* Software reset register */
#define DSIM_FUNCRST		(1 << 16)
#define DSIM_SWRST		(1 << 0)

#define S5P_DSIM_CLKCTRL	(0x10)	/* Clock control register */
#define DSIM_LANE_ESC_CLKEN_SHIFT	(19)
#define DSIM_BYTE_CLKEN_SHIFT		(24)
#define DSIM_BYTE_CLK_SRC_SHIFT		(25)
#define DSIM_PLL_BYPASS_SHIFT		(27)
#define DSIM_ESC_CLKEN_SHIFT		(28)
#define DSIM_TX_REQUEST_HSCLK_SHIFT	(31)
#define DSIM_LANE_ESC_CLKEN(x)		(((x) & 0x1f) << \
						DSIM_LANE_ESC_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_ENABLE		(1 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_DISABLE		(0 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_PLL_BYPASS_EXTERNAL	(1 << DSIM_PLL_BYPASS_SHIFT)
#define DSIM_ESC_CLKEN_ENABLE		(1 << DSIM_ESC_CLKEN_SHIFT)
#define DSIM_ESC_CLKEN_DISABLE		(0 << DSIM_ESC_CLKEN_SHIFT)

#define S5P_DSIM_TIMEOUT	(0x14)	/* Time out register */
#define DSIM_LPDR_TOUT_SHIFT	(0)
#define DSIM_BTA_TOUT_SHIFT	(16)

#define S5P_DSIM_CONFIG		(0x18)	/* Configuration register */
#define DSIM_NUM_OF_DATALANE_SHIFT	(5)
#define DSIM_HSA_MODE_SHIFT		(20)
#define DSIM_HBP_MODE_SHIFT		(21)
#define DSIM_HFP_MODE_SHIFT		(22)
#define DSIM_HSE_MODE_SHIFT		(23)
#define DSIM_AUTO_MODE_SHIFT		(24)
#define DSIM_LANE_ENx(x)		(((x) & 0x1f) << 0)
#define DSIM_NUM_OF_DATA_LANE(x)	((x) << DSIM_NUM_OF_DATALANE_SHIFT)

#define S5P_DSIM_ESCMODE	(0x1c)	/* Escape mode register */
#define DSIM_TX_ULPS_CLK_EXIT		(1 << 0)
#define DSIM_TX_ULPS_CLK		(1 << 1)
#define DSIM_TX_ULPS_DATA_EXIT		(1 << 2)
#define DSIM_TX_ULPS_DATA		(1 << 3)
#define DSIM_TX_LPDT_SHIFT		(6)
#define DSIM_CMD_LPDT_SHIFT		(7)
#define DSIM_TX_LPDT_LP			(1 << DSIM_TX_LPDT_SHIFT)
#define DSIM_CMD_LPDT_LP		(1 << DSIM_CMD_LPDT_SHIFT)
#define DSIM_STOP_STATE_CNT_SHIFT	(21)
#define DSIM_FORCE_STOP_STATE_SHIFT	(20)

/* Display image resolution register */
#define S5P_DSIM_MDRESOL	(0x20)
#define DSIM_MAIN_STAND_BY		(1 << 31)
#define DSIM_MAIN_HRESOL_SHIFT		(0)
#define DSIM_MAIN_VRESOL_SHIFT		(16)
#define DSIM_MAIN_VRESOL(x)		(((x) & 0xfff) << DSIM_MAIN_VRESOL_SHIFT)
#define DSIM_MAIN_HRESOL(x)		(((x) & 0Xfff) << DSIM_MAIN_HRESOL_SHIFT)
#define DSIM_MAIN_HRESOL_MASK		(0xfff << DSIM_MAIN_HRESOL_SHIFT)
#define DSIM_MAIN_VRESOL_MASK		(0xfff << DSIM_MAIN_VRESOL_SHIFT)

#define S5P_DSIM_MVPORCH	(0x24)	/* Main display Vporch register */
#define DSIM_CMD_ALLOW_SHIFT		(28)
#define DSIM_STABLE_VFP_SHIFT		(16)
#define DSIM_MAIN_VBP_SHIFT		(0)
#define DSIM_CMD_ALLOW_MASK		(0xf << DSIM_CMD_ALLOW_SHIFT)
#define DSIM_STABLE_VFP_MASK		(0x7ff << DSIM_STABLE_VFP_SHIFT)
#define DSIM_MAIN_VBP_MASK		(0x7ff << DSIM_MAIN_VBP_SHIFT)

#define S5P_DSIM_MHPORCH	(0x28)	/* Main display Hporch register */
#define DSIM_MAIN_HFP_SHIFT		(16)
#define DSIM_MAIN_HBP_SHIFT		(0)
#define DSIM_MAIN_HFP_MASK		((0xffff) << DSIM_MAIN_HFP_SHIFT)
#define DSIM_MAIN_HBP_MASK		((0xffff) << DSIM_MAIN_HBP_SHIFT)

#define S5P_DSIM_MSYNC		(0x2C)	/* Main display sync area register */
#define DSIM_MAIN_VSA_SHIFT		(22)
#define DSIM_MAIN_HSA_SHIFT		(0)
#define DSIM_MAIN_VSA_MASK		((0x3ff) << DSIM_MAIN_VSA_SHIFT)
#define DSIM_MAIN_HSA_MASK		((0xffff) << DSIM_MAIN_HSA_SHIFT)

#define S5P_DSIM_INTSRC		(0x34)	/* Interrupt source register */
#define INTSRC_FRAME_DONE		(1 << 24)
#define INTSRC_PLL_STABLE		(1 << 31)

#define INTSRC_SFR_FIFO_EMPTY		(1 << 29)
#define SFR_HEADER_EMPTY		(1 << 22)

#define S5P_DSIM_INTMSK		(0x38)	/* Interrupt mask register */
#define INTMSK_FRAME_DONE		(1 << 24)

#define S5P_DSIM_PKTHDR		(0x3c)	/* Packet Header FIFO register */
#define S5P_DSIM_PAYLOAD	(0x40)	/* Payload FIFO register */
#define S5P_DSIM_RXFIFO		(0x44)	/* Read FIFO register */

#define S5P_DSIM_FIFOCTRL	(0x4c)	/* FIFO status and control register */

#define S5P_DSIM_STEREO_3D	(0x54)	/* Stereo scope 3D register */
#define S5P_DSIM_PROPRIETARY_3D	(0x58)	/* Proprietary 3D register */

#define S5P_DSIM_MIC		(0x5c)	/* MIC control register */
#define DSIM_MIC_EN		(1 << 1)

#define S5P_DSIM_PRO_ON_MIC_OFF_H	(0x60)
#define S5P_DSIM_PRO_OFF_MIC_ON_H	(0x64)
#define S5P_DSIM_PRO_ON_MIC_ON_H	(0x68)

#define S5P_DSIM_PRO_ON_MIC_OFF_HFP	(0x6c)
#define S5P_DSIM_PRO_OFF_MIC_ON_HFP	(0x70)
#define S5P_DSIM_PRO_ON_MIC_ON_HFP	(0x74)

#define S5P_DSIM_MULTI_PKT		(0x78)

#define S5P_DSIM_PLLCTRL	(0x94)	/* PLL control register */
#define DSIM_PLL_EN_SHIFT		(23)
#define DSIM_FREQ_BAND_SHIFT		(24)

#define S5P_DSIM_PLLTMR		(0xa0)	/* PLL timer register */

/* D-PHY Master & Slave Analog block characteristics control register (B_DPHYCTL) */
#define S5P_DSIM_PHYCTRL	(0xa4)
/* D-PHY Master global operating timing register */
#define S5P_DSIM_PHYTIMING	(0xb4)
#define S5P_DSIM_PHYTIMING1	(0xb8)
#define S5P_DSIM_PHYTIMING2	(0xbC)

/* Version register */
#define S5P_DSIM_VERSION	(0x1000)  /* Specifies the DSIM version information */
#else
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
#define S5P_DSIM_VERSION	(0x0)  /* Specifies the DSIM version information */
#define S5P_DSIM_STATUS		(0x04)	/* Status register */
#define S5P_DSIM_RGB_STATUS	(0x08)  /* Status register */
#define DSIM_STOP_STATE_DAT(x)	(((x) & 0xf) << 0)
#define DSIM_ULPS_DAT(x)	(((x) & 0xf) << 4)
#define DSIM_STOP_STATE_CLK	(1 << 8)
#define DSIM_ULPS_CLK		(1 << 9)
#define DSIM_TX_READY_HS_CLK	(1 << 10)

#define S5P_DSIM_SWRST		(0xC)	/* Software reset register */
#define DSIM_FUNCRST		(1 << 16)
#define DSIM_SWRST		(1 << 0)

#define S5P_DSIM_CLKCTRL	(0x10)	/* Clock control register */
#define DSIM_LANE_ESC_CLKEN_SHIFT	(19)
#define DSIM_BYTE_CLKEN_SHIFT		(24)
#define DSIM_BYTE_CLK_SRC_SHIFT		(25)
#define DSIM_PLL_BYPASS_SHIFT		(27)
#define DSIM_ESC_CLKEN_SHIFT		(28)
#define DSIM_TX_REQUEST_HSCLK_SHIFT	(31)
#define DSIM_LANE_ESC_CLKEN(x)		(((x) & 0x1f) << \
						DSIM_LANE_ESC_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_ENABLE		(1 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_DISABLE		(0 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_PLL_BYPASS_EXTERNAL	(1 << DSIM_PLL_BYPASS_SHIFT)
#define DSIM_ESC_CLKEN_ENABLE		(1 << DSIM_ESC_CLKEN_SHIFT)
#define DSIM_ESC_CLKEN_DISABLE		(0 << DSIM_ESC_CLKEN_SHIFT)

#define S5P_DSIM_TIMEOUT	(0x14)	/* Time out register */
#define DSIM_LPDR_TOUT_SHIFT	(0)
#define DSIM_BTA_TOUT_SHIFT	(16)

#define S5P_DSIM_CONFIG		(0x18)	/* Configuration register */
#define DSIM_NUM_OF_DATALANE_SHIFT	(5)
#define DSIM_HSA_MODE_SHIFT		(20)
#define DSIM_HBP_MODE_SHIFT		(21)
#define DSIM_HFP_MODE_SHIFT		(22)
#define DSIM_HSE_MODE_SHIFT		(23)
#define DSIM_AUTO_MODE_SHIFT		(24)
#define DSIM_LANE_ENx(x)		(((x) & 0x1f) << 0)
#define DSIM_NUM_OF_DATA_LANE(x)	((x) << DSIM_NUM_OF_DATALANE_SHIFT)

#define S5P_DSIM_ESCMODE	(0x1C)	/* Escape mode register */
#define DSIM_TX_ULPS_CLK_EXIT		(1 << 0)
#define DSIM_TX_ULPS_CLK		(1 << 1)
#define DSIM_TX_ULPS_DATA_EXIT		(1 << 2)
#define DSIM_TX_ULPS_DATA		(1 << 3)
#define DSIM_TX_LPDT_SHIFT		(6)
#define DSIM_CMD_LPDT_SHIFT		(7)
#define DSIM_TX_LPDT_LP			(1 << DSIM_TX_LPDT_SHIFT)
#define DSIM_CMD_LPDT_LP		(1 << DSIM_CMD_LPDT_SHIFT)
#define DSIM_STOP_STATE_CNT_SHIFT	(21)
#define DSIM_FORCE_STOP_STATE_SHIFT	(20)

/* Display image resolution register */
#define S5P_DSIM_MDRESOL	(0x20)
#define DSIM_MAIN_STAND_BY		(1 << 31)
#define DSIM_MAIN_HRESOL_SHIFT		(0)
#define DSIM_MAIN_VRESOL_SHIFT		(16)
#define DSIM_MAIN_VRESOL(x)		(((x) & 0xfff) << DSIM_MAIN_VRESOL_SHIFT)
#define DSIM_MAIN_HRESOL(x)		(((x) & 0Xfff) << DSIM_MAIN_HRESOL_SHIFT)
#define DSIM_MAIN_HRESOL_MASK		(0xfff << DSIM_MAIN_HRESOL_SHIFT)
#define DSIM_MAIN_VRESOL_MASK		(0xfff << DSIM_MAIN_VRESOL_SHIFT)

#define S5P_DSIM_MVPORCH	(0x24)	/* Main display Vporch register */
#define DSIM_CMD_ALLOW_SHIFT		(28)
#define DSIM_STABLE_VFP_SHIFT		(16)
#define DSIM_MAIN_VBP_SHIFT		(0)
#define DSIM_CMD_ALLOW_MASK		(0xf << DSIM_CMD_ALLOW_SHIFT)
#define DSIM_STABLE_VFP_MASK		(0x7ff << DSIM_STABLE_VFP_SHIFT)
#define DSIM_MAIN_VBP_MASK		(0x7ff << DSIM_MAIN_VBP_SHIFT)

#define S5P_DSIM_MHPORCH	(0x28)	/* Main display Hporch register */
#define DSIM_MAIN_HFP_SHIFT		(16)
#define DSIM_MAIN_HBP_SHIFT		(0)
#define DSIM_MAIN_HFP_MASK		((0xffff) << DSIM_MAIN_HFP_SHIFT)
#define DSIM_MAIN_HBP_MASK		((0xffff) << DSIM_MAIN_HBP_SHIFT)

#define S5P_DSIM_MSYNC		(0x2C)	/* Main display sync area register */
#define DSIM_MAIN_VSA_SHIFT		(22)
#define DSIM_MAIN_HSA_SHIFT		(0)
#define DSIM_MAIN_VSA_MASK		((0x3ff) << DSIM_MAIN_VSA_SHIFT)
#define DSIM_MAIN_HSA_MASK		((0xffff) << DSIM_MAIN_HSA_SHIFT)

#define S5P_DSIM_SDRESOL	(0x30)	/* Sub display image resolution register */
#define S5P_DSIM_INTSRC		(0x34)	/* Interrupt source register */
#define INTSRC_FRAME_DONE		(1 << 24)
#define INTSRC_PLL_STABLE		(1 << 31)

#define INTSRC_SFR_FIFO_EMPTY		(1 << 29)
#define SFR_HEADER_EMPTY		(1 << 22)

#define S5P_DSIM_INTMSK		(0x38)	/* Interrupt mask register */
#define INTMSK_FRAME_DONE		(1 << 24)

#define S5P_DSIM_PKTHDR		(0x3C)	/* Packet Header FIFO register */
#define S5P_DSIM_PAYLOAD	(0x40)	/* Payload FIFO register */
#define S5P_DSIM_RXFIFO		(0x44)	/* Read FIFO register */

#define S5P_DSIM_FIFOCTRL	(0x4C)	/* FIFO status and control register */

#define S5P_DSIM_MEMACCHR	(0x50)	/* FIFO memory AC characteristic register */
#define S5P_DSIM_STEREO_3D	(0x54)	/* Stereo scope 3D register */
#define S5P_DSIM_PROPRIETARY_3D	(0x58)	/* Proprietary 3D register */

#define S5P_DSIM_MIC		(0x5C)	/* MIC control register */
#define DSIM_MIC_EN		(1 << 1)

#define S5P_DSIM_PRO_ON_MIC_OFF_H	(0x60)
#define S5P_DSIM_PRO_OFF_MIC_ON_H	(0x64)
#define S5P_DSIM_PRO_ON_MIC_ON_H	(0x68)

#define S5P_DSIM_PRO_ON_MIC_OFF_HFP	(0x6C)
#define S5P_DSIM_PRO_OFF_MIC_ON_HFP	(0x70)
#define S5P_DSIM_PRO_ON_MIC_ON_HFP	(0x74)

#define S5P_DSIM_MULTI_PKT		(0x78)

#define S5P_DSIM_PLLCTRL_1G	(0x90)	/* PLL control register */
#define S5P_DSIM_PLLCTRL	(0x94)	/* PLL control register */
#define S5P_DSIM_PLLCTRL1	(0x98)	/* PLL control register */
#define S5P_DSIM_PLLCTRL2	(0x9C)	/* PLL control register */
#define DSIM_PLL_EN_SHIFT		(23)
#define DSIM_FREQ_BAND_SHIFT		(24)

#define S5P_DSIM_PLLTMR		(0xa0)	/* PLL timer register */

/* D-PHY Master & Slave Analog block characteristics control register (B_DPHYCTL) */
#define S5P_DSIM_PHYCTRL	(0xA4)
#define S5P_DSIM_PHYCTRL_B2	(0xA8)
#define S5P_DSIM_PHYCTRL_M1	(0xAC)
#define S5P_DSIM_PHYCTRL_M2	(0xB0)
/* D-PHY Master global operating timing register */
#define S5P_DSIM_PHYTIMING	(0xB4)
#define S5P_DSIM_PHYTIMING1	(0xB8)
#define S5P_DSIM_PHYTIMING2	(0xBC)

#else
#define S5P_DSIM_STATUS		(0x0)	/* Status register */
#define DSIM_STOP_STATE_DAT(x)	(((x) & 0xf) << 0)
#define DSIM_ULPS_DAT(x)	(((x) & 0xf) << 4)
#define DSIM_STOP_STATE_CLK	(1 << 8)
#define DSIM_ULPS_CLK		(1 << 9)
#define DSIM_TX_READY_HS_CLK	(1 << 10)



#define S5P_DSIM_SWRST		(0x4)	/* Software reset register */


#define S5P_DSIM_CLKCTRL	(0x8)	/* Clock control register */


#define S5P_DSIM_TIMEOUT	(0xc)	/* Time out register */


#define S5P_DSIM_CONFIG		(0x10)	/* Configuration register */


#define S5P_DSIM_ESCMODE	(0x14)	/* Escape mode register */
#define DSIM_TX_ULPS_CLK_EXIT		(1 << 0)
#define DSIM_TX_ULPS_CLK		(1 << 1)
#define DSIM_TX_ULPS_DATA_EXIT		(1 << 2)
#define DSIM_TX_ULPS_DATA		(1 << 3)
#define DSIM_TX_LPDT_SHIFT		(6)
#define DSIM_CMD_LPDT_SHIFT		(7)
#define DSIM_TX_LPDT_LP		(1 << DSIM_TX_LPDT_SHIFT)
#define DSIM_CMD_LPDT_LP		(1 << DSIM_CMD_LPDT_SHIFT)


#define S5P_DSIM_MDRESOL	(0x18)	/* Main display image resolution register */


#define S5P_DSIM_MVPORCH	(0x1c)	/* Main display Vporch register */


#define S5P_DSIM_MHPORCH	(0x20)	/* Main display Hporch register */



#define S5P_DSIM_MSYNC		(0x24)	/* Main display sync area register */


#define S5P_DSIM_SDRESOL	(0x28)	/* Sub display image resolution register */

#define S5P_DSIM_INTSRC		(0x2c)	/* Interrupt source register */
#define INTSRC_PLL_STABLE		(1 << 31)
#define INTSRC_FRAME_DONE		(1 << 24)

#define S5P_DSIM_INTMSK		(0x30)	/* Interrupt mask register */
#define INTMSK_FRAME_DONE		(1 << 24)

#define S5P_DSIM_MULTI_PKT		(0xA4)
#define S5P_DSIM_VERSION	(0x70)  /* Specifies the DSIM version information */


#define S5P_DSIM_PKTHDR		(0x34)	/* Packet Header FIFO register */
#define S5P_DSIM_PAYLOAD	(0x38)	/* Payload FIFO register */
#define S5P_DSIM_RXFIFO		(0x3c)	/* Read FIFO register */
#define S5P_DSIM_FIFOTHLD	(0x40)	/* FIFO threshold level register */
#define S5P_DSIM_FIFOCTRL	(0x44)	/* FIFO status and control register */
#define S5P_DSIM_MEMACCHR	(0x48)	/* FIFO memory AC characteristic register */
#define S5P_DSIM_PLLCTRL	(0x4c)	/* PLL control register */
#define S5P_DSIM_PLLTMR		(0x50)	/* PLL timer register */
#define S5P_DSIM_PHYACCHR	(0x54)	/* D-PHY AC characteristic register */
#define S5P_DSIM_PHYACCHR1	(0x58)	/* D-PHY AC characteristic register 1 */
/* D-PHY Master & Slave Analog block characteristics control register (B_DPHYCTL) */
#define S5P_DSIM_PHYCTRL	(0x5c)
/* D-PHY Master global operating timing register */
#define S5P_DSIM_PHYTIMING	(0x64)
#define S5P_DSIM_PHYTIMING1	(0x68)
#define S5P_DSIM_PHYTIMING2	(0x6C)

/* DSIM_SWRST */
#define DSIM_FUNCRST		(1 << 16)
#define DSIM_SWRST		(1 << 0)

/* S5P_DSIM_TIMEOUT */
#define DSIM_LPDR_TOUT_SHIFT	(0)
#define DSIM_BTA_TOUT_SHIFT	(16)
#define DSIM_LPDR_TOUT(x)	(((x) & 0xffff) << DSIM_LPDR_TOUT_SHIFT)
#define DSIM_BTA_TOUT(x)	(((x) & 0xff) << DSIM_BTA_TOUT_SHIFT)

/* S5P_DSIM_CLKCTRL */
#define DSIM_ESC_PRESCALER_SHIFT	(0)
#define DSIM_LANE_ESC_CLKEN_SHIFT	(19)
#define DSIM_BYTE_CLKEN_SHIFT		(24)
#define DSIM_BYTE_CLK_SRC_SHIFT		(25)
#define DSIM_PLL_BYPASS_SHIFT		(27)
#define DSIM_ESC_CLKEN_SHIFT		(28)
#define DSIM_TX_REQUEST_HSCLK_SHIFT	(31)
#define DSIM_ESC_PRESCALER(x)		(((x) & 0xffff) << DSIM_ESC_PRESCALER_SHIFT)
#define DSIM_LANE_ESC_CLKEN(x)		(((x) & 0x1f) << DSIM_LANE_ESC_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_ENABLE		(1 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_BYTE_CLK_DISABLE		(0 << DSIM_BYTE_CLKEN_SHIFT)
#define DSIM_BYTE_CLKSRC(x)		(((x) & 0x3) << DSIM_BYTE_CLK_SRC_SHIFT)
#define DSIM_PLL_BYPASS_PLL		(0 << DSIM_PLL_BYPASS_SHIFT)
#define DSIM_PLL_BYPASS_EXTERNAL	(1 << DSIM_PLL_BYPASS_SHIFT)
#define DSIM_ESC_CLKEN_ENABLE		(1 << DSIM_ESC_CLKEN_SHIFT)
#define DSIM_ESC_CLKEN_DISABLE		(0 << DSIM_ESC_CLKEN_SHIFT)

/* S5P_DSIM_CONFIG */
#define DSIM_LANE_EN_SHIFT		(0)
#define DSIM_NUM_OF_DATALANE_SHIFT	(5)
#define DSIM_SUB_PIX_FORMAT_SHIFT	(8)
#define DSIM_MAIN_PIX_FORMAT_SHIFT	(12)
#define DSIM_SUB_VC_SHIFT		(16)
#define DSIM_MAIN_VC_SHIFT		(18)
#define DSIM_HSA_MODE_SHIFT		(20)
#define DSIM_HBP_MODE_SHIFT		(21)
#define DSIM_HFP_MODE_SHIFT		(22)
#define DSIM_HSE_MODE_SHIFT		(23)
#define DSIM_AUTO_MODE_SHIFT		(24)
#define DSIM_VIDEO_MODE_SHIFT		(25)
#define DSIM_BURST_MODE_SHIFT		(26)
#define DSIM_SYNC_INFORM_SHIFT		(27)
#define DSIM_EOT_R03_SHIFT		(28)
#define DSIM_LANE_ENx(x)		(((x) & 0x1f) << 0)
#define DSIM_NUM_OF_DATA_LANE(x)	((x) << 5)	/* in case of Gemunus, it should be 0x1. */
#define DSIM_SUB_PIX_FORMAT_3BPP	(0 << 8)	/* command mode only */
#define DSIM_SUB_PIX_FORMAT_8BPP	(1 << 8)	/* command mode only */
#define DSIM_SUB_PIX_FORMAT_12BPP	(2 << 8)	/* command mode only */
#define DSIM_SUB_PIX_FORMAT_16BPP	(3 << 8)	/* command mode only */
#define DSIM_SUB_PIX_FORMAT_16BPP_RGB	(4 << 8)	/* video mode only */
#define DSIM_SUB_PIX_FORMAT_18BPP_PRGB	(5 << 8)	/* video mode only */
#define DSIM_SUB_PIX_FORMAT_18BPP_LRGB	(6 << 8)	/* common */
#define DSIM_SUB_PIX_FORMAT_24BPP_RGB	(7 << 8)	/* common */
#define DSIM_MAIN_PIX_FORMAT_3BPP	(0 << 12)	/* command mode only */
#define DSIM_MAIN_PIX_FORMAT_8BPP	(1 << 12)	/* command mode only */
#define DSIM_MAIN_PIX_FORMAT_12BPP	(2 << 12)	/* command mode only */
#define DSIM_MAIN_PIX_FORMAT_16BPP	(3 << 12)	/* command mode only */
#define DSIM_MAIN_PIX_FORMAT_16BPP_RGB	(4 << 12)	/* video mode only */
#define DSIM_MAIN_PIX_FORMAT_18BPP_PRGB	(5 << 12)	/* video mode only */
#define DSIM_MAIN_PIX_FORMAT_18BPP_LRGB	(6 << 12)	/* common */
#define DSIM_MAIN_PIX_FORMAT_24BPP_RGB	(7 << 12)	/* common */
#define DSIM_SUB_VC(x)			(((x) & 0x3) << 16)	/* Virtual channel number for sub display */
#define DSIM_MAIN_VC(x)			(((x) & 0x3) << 18)	/* Virtual channel number for main display */
#define DSIM_HSA_MODE_ENABLE		(1 << 20)
#define DSIM_HSA_MODE_DISABLE		(0 << 20)
#define DSIM_HBP_MODE_ENABLE		(1 << 21)
#define DSIM_HBP_MODE_DISABLE		(0 << 21)
#define DSIM_HFP_MODE_ENABLE		(1 << 22)
#define DSIM_HFP_MODE_DISABLE		(0 << 22)
#define DSIM_HSE_MODE_ENABLE		(1 << 23)
#define DSIM_HSE_MODE_DISABLE		(0 << 23)
#define DSIM_AUTO_MODE			(1 << 24)
#define DSIM_CONFIGURATION_MODE		(0 << 24)
#define DSIM_VIDEO_MODE			(1 << 25)
#define DSIM_COMMAND_MODE		(0 << 25)
#define DSIM_BURST_MODE			(1 << 26)
#define DSIM_NON_BURST_MODE		(0 << 26)
#define DSIM_SYNC_INFORM_PULSE		(1 << 27)
#define DSIM_SYNC_INFORM_EVENT		(0 << 27)
#define DSIM_EOT_R03_ENABLE		(0 << 28)	/* enable EoT packet generation for V1.01r11 */
#define DSIM_EOT_R03_DISABLE		(1 << 28)	/* disable EoT packet generation for V1.01r03 */

/* S5P_DSIM_ESCMODE */
#define DSIM_STOP_STATE_CNT_SHIFT	(21)
#define DSIM_STOP_STATE_CNT(x)		(((x) & 0x3ff) << DSIM_STOP_STATE_CNT_SHIFT)
#define DSIM_FORCE_STOP_STATE_SHIFT	(20)
#define DSIM_FORCE_BTA_SHIFT		(16)
#define DSIM_CMD_LPDT_HS_MODE		(0 << 7)
#define DSIM_CMD_LPDT_LP_MODE		(1 << 7)
#define DSIM_TX_LPDT_HS_MODE		(0 << 6)
#define DSIM_TX_LPDT_LP_MODE		(1 << 6)
#define DSIM_TX_TRIGGER_RST_SHIFT	(4)
#define DSIM_TX_UIPS_DAT_SHIFT		(3)
#define DSIM_TX_UIPS_EXIT_SHIFT		(2)
#define DSIM_TX_UIPS_CLK_SHIFT		(1)
#define DSIM_TX_UIPS_CLK_EXIT_SHIFT	(0)

/* S5P_DSIM_MDRESOL */
#define DSIM_MAIN_STAND_BY		(1 << 31)
#define DSIM_MAIN_NOT_READY		(0 << 31)
#define DSIM_MAIN_HRESOL_SHIFT         (0)
#define DSIM_MAIN_VRESOL_SHIFT         (16)
#define DSIM_MAIN_VRESOL(x)            (((x) & 0xfff) << DSIM_MAIN_VRESOL_SHIFT)
#define DSIM_MAIN_HRESOL(x)            (((x) & 0Xfff) << DSIM_MAIN_HRESOL_SHIFT)
#define DSIM_MAIN_HRESOL_MASK          (0xfff << DSIM_MAIN_HRESOL_SHIFT)
#define DSIM_MAIN_VRESOL_MASK          (0xfff << DSIM_MAIN_VRESOL_SHIFT)

/* S5P_DSIM_MVPORCH */
#define DSIM_CMD_ALLOW_SHIFT		(28)
#define DSIM_STABLE_VFP_SHIFT		(16)
#define DSIM_MAIN_VBP_SHIFT		(0)
#define DSIM_CMD_ALLOW_MASK		(0xf << DSIM_CMD_ALLOW_SHIFT)
#define DSIM_STABLE_VFP_MASK		(0x7ff << DSIM_STABLE_VFP_SHIFT)
#define DSIM_MAIN_VBP_MASK		(0x7ff << DSIM_MAIN_VBP_SHIFT)
#define DSIM_CMD_ALLOW(x)		(((x) & 0xf) << DSIM_CMD_ALLOW_SHIFT)
#define DSIM_STABLE_VFP(x)		(((x) & 0x7ff) << DSIM_STABLE_VFP_SHIFT)
#define DSIM_MAIN_VBP(x)		(((x) & 0x7ff) << DSIM_MAIN_VBP_SHIFT)

/* S5P_DSIM_MHPORCH */
#define DSIM_MAIN_HFP_SHIFT		(16)
#define DSIM_MAIN_HBP_SHIFT		(0)
#define DSIM_MAIN_HFP_MASK		((0xffff) << DSIM_MAIN_HFP_SHIFT)
#define DSIM_MAIN_HBP_MASK		((0xffff) << DSIM_MAIN_HBP_SHIFT)
#define DSIM_MAIN_HFP(x)		(((x) & 0xffff) << DSIM_MAIN_HFP_SHIFT)
#define DSIM_MAIN_HBP(x)		(((x) & 0xffff) << DSIM_MAIN_HBP_SHIFT)

/* S5P_DSIM_MSYNC */
#define DSIM_MAIN_VSA_SHIFT		(22)
#define DSIM_MAIN_HSA_SHIFT		(0)
#define DSIM_MAIN_VSA_MASK		((0x3ff) << DSIM_MAIN_VSA_SHIFT)
#define DSIM_MAIN_HSA_MASK		((0xffff) << DSIM_MAIN_HSA_SHIFT)
#define DSIM_MAIN_VSA(x)		(((x) & 0x3ff) << DSIM_MAIN_VSA_SHIFT)
#define DSIM_MAIN_HSA(x)		(((x) & 0xffff) << DSIM_MAIN_HSA_SHIFT)

/* S5P_DSIM_SDRESOL */
#define DSIM_SUB_STANDY_SHIFT		(31)
#define DSIM_SUB_VRESOL_SHIFT		(16)
#define DSIM_SUB_HRESOL_SHIFT		(0)
#define DSIM_SUB_STANDY_MASK		((0x1) << DSIM_SUB_STANDY_SHIFT)
#define DSIM_SUB_VRESOL_MASK		((0x7ff) << DSIM_SUB_VRESOL_SHIFT)
#define DSIM_SUB_HRESOL_MASK		((0x7ff) << DSIM_SUB_HRESOL_SHIFT)
#define DSIM_SUB_STANDY			(1 << DSIM_SUB_STANDY_SHIFT)
#define DSIM_SUB_NOT_READY		(0 << DSIM_SUB_STANDY_SHIFT)
#define DSIM_SUB_VRESOL(x)		(((x) & 0x7ff) << DSIM_SUB_VRESOL_SHIFT)
#define DSIM_SUB_HRESOL(x)		(((x) & 0x7ff) << DSIM_SUB_HRESOL_SHIFT)

/* S5P_DSIM_PKTHDR */
#define DSIM_PACKET_HEADER_DI(x)	(((x) & 0xff) << 0)
#define DSIM_PACKET_HEADER_DAT0(x)	(((x) & 0xff) << 8)	/* Word count lower byte for long packet */
#define DSIM_PACKET_HEADER_DAT1(x)	(((x) & 0xff) << 16)	/* Word count upper byte for long packet */

/* S5P_DSIM_FIFOCTRL */
#define DSIM_RX_FIFO			(1 << 4)
#define DSIM_TX_SFR_FIFO		(1 << 3)
#define DSIM_I80_FIFO			(1 << 2)
#define DSIM_SUB_DISP_FIFO		(1 << 1)
#define DSIM_MAIN_DISP_FIFO		(1 << 0)

/* S5P_DSIM_PHYACCHR */
#define DSIM_AFC_CTL(x)			(((x) & 0x7) << 5)
#define DSIM_AFC_ENABLE			(1 << 14)
#define DSIM_AFC_DISABLE		(0 << 14)

/* S5P_DSIM_PLLCTRL */
#define DSIM_PMS_SHIFT			(1)
#define DSIM_PLL_EN_SHIFT		(23)
#define DSIM_FREQ_BAND_SHIFT		(24)
#define DSIM_PMS(x)			(((x) & 0x7ffff) << DSIM_PMS_SHIFT)
#define DSIM_FREQ_BAND(x)		(((x) & 0xf) << DSIM_FREQ_BAND_SHIFT)

typedef enum {
	PllStable = 1 << 31,
	SwRstRelease = 1 << 30,
	SFRFifoEmpty = 1 << 29,
	BusTrunOVer = 1 << 25,
	FrameDone = 1 << 24,
	LpdrTout = 1 << 21,
	TaTout = 1 << 20,
	RxDatDone = 1 << 18,
	RxTE = 1 << 17,
	ErrRxEcc = 1 << 15,
	ErrRxCRC = 1 << 14,
	ErrEsc3 = 1 << 13,
	ErrEsc2 = 1 << 12,
	ErrEsc1 = 1 << 11,
	ErrEsc0 = 1 << 10,
	ErrSync3 = 1 << 9,
	ErrSync2 = 1 << 8,
	ErrSync1 = 1 << 7,
	ErrSync0 = 1 << 6,
	ErrControl3 = 1 << 5,
	ErrControl2 = 1 << 4,
	ErrControl1 = 1 << 3,
	ErrControl0 = 1 << 2,
	ErrContentLP0 = 1 << 1,
	ErrContentLP1 = 1 << 0,

	AllDsimIntr = 0xffffffff,
	ErrDsimIntr = 0xffff,
} DSIM_INTSRC;
#endif
#endif
#endif /* _REGS_MIPIDSIM_H */
