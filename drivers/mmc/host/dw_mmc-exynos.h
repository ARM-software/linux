/*
 * Exynos Specific Extensions for Synopsys DW Multimedia Card Interface driver
 *
 * Copyright (C) 2012, Samsung Electronics Co., Ltd.
 * Copyright (C) 2013, The Chromium OS Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define NUM_PINS(x)			(x + 2)

#define DWMCI_CLKSEL			0x09C	/* Ken : need to unify definition */
#define SDMMC_CLKSEL_CCLK_SAMPLE(x)	(((x) & 7) << 0)
#define SDMMC_CLKSEL_CCLK_FINE_SAMPLE(x)	(((x) & 0xF) << 0)
#define SDMMC_CLKSEL_CCLK_DRIVE(x)	(((x) & 7) << 16)
#define SDMMC_CLKSEL_CCLK_DIVIDER(x)	(((x) & 7) << 24)
#define SDMMC_CLKSEL_GET_DRV_WD3(x)	(((x) >> 16) & 0x7)
#define SDMMC_CLKSEL_GET_DIVRATIO(x)	((((x) >> 24) & 0x7) + 1)
#define SDMMC_CLKSEL_TIMING(x, y, z)	(SDMMC_CLKSEL_CCLK_SAMPLE(x) |	\
					SDMMC_CLKSEL_CCLK_DRIVE(y) |	\
					SDMMC_CLKSEL_CCLK_DIVIDER(z))

#define SDMMC_CMD_USE_HOLD_REG		BIT(29)

/*
 * DDR200 dependent
 */
#define DWMCI_DDR200_RDDQS_EN		0x110
#define DWMCI_DDR200_ASYNC_FIFO_CTRL	0x114
#define DWMCI_DDR200_DLINE_CTRL		0x118
/* DDR200 RDDQS Enable*/
#define DWMCI_TXDT_CRC_TIMER_FASTLIMIT(x)	(((x) & 0xFF) << 16)
#define DWMCI_TXDT_CRC_TIMER_INITVAL(x)		(((x) & 0xFF) << 8)
#define DWMCI_TXDT_CRC_TIMER_SET(x, y)	(DWMCI_TXDT_CRC_TIMER_FASTLIMIT(x) | \
					DWMCI_TXDT_CRC_TIMER_INITVAL(y))
#define DWMCI_AXI_NON_BLOCKING_WRITE		BIT(7)
#define DWMCI_BUSY_CHK_CLK_STOP_EN		BIT(2)
#define DWMCI_RXDATA_START_BIT_SEL		BIT(1)
#define DWMCI_RDDQS_EN				BIT(0)
#define DWMCI_DDR200_RDDQS_EN_DEF	DWMCI_TXDT_CRC_TIMER_FASTLIMIT(0x13) | \
					DWMCI_TXDT_CRC_TIMER_INITVAL(0x15)
/* DDR200 DLINE CTRL */
#define DWMCI_WD_DQS_DELAY_CTRL(x)		(((x) & 0x3FF) << 20)
#define DWMCI_FIFO_CLK_DELAY_CTRL(x)		(((x) & 0x3) << 16)
#define DWMCI_RD_DQS_DELAY_CTRL(x)		((x) & 0x3FF)
#define DWMCI_DDR200_DLINE_CTRL_SET(x, y, z)	(DWMCI_WD_DQS_DELAY_CTRL(x) | \
						DWMCI_FIFO_CLK_DELAY_CTRL(y) | \
						DWMCI_RD_DQS_DELAY_CTRL(z))
#define DWMCI_DDR200_DLINE_CTRL_DEF	DWMCI_FIFO_CLK_DELAY_CTRL(0x2) | \
					DWMCI_RD_DQS_DELAY_CTRL(0x40)

/* DDR200 Async FIFO Control */
#define DWMCI_ASYNC_FIFO_RESET		BIT(0)

/* Block number in eMMC */
#define DWMCI_BLOCK_NUM			0xFFFFFFFF

#define DWMCI_EMMCP_BASE		0x1000
#define DWMCI_MPSTAT			(DWMCI_EMMCP_BASE + 0x0008)
#define DWMCI_MPSECURITY		(DWMCI_EMMCP_BASE + 0x0010)
#define DWMCI_MPENCKEY			(DWMCI_EMMCP_BASE + 0x0020)
#define DWMCI_MPSBEGIN0			(DWMCI_EMMCP_BASE + 0x0200)
#define DWMCI_MPSEND0			(DWMCI_EMMCP_BASE + 0x0204)
#define DWMCI_MPSCTRL0			(DWMCI_EMMCP_BASE + 0x020C)
#define DWMCI_MPSBEGIN1			(DWMCI_EMMCP_BASE + 0x0210)
#define DWMCI_MPSEND1			(DWMCI_EMMCP_BASE + 0x0214)
#define DWMCI_MPSCTRL1			(DWMCI_EMMCP_BASE + 0x021C)

/* SMU control bits */
#define DWMCI_MPSCTRL_SECURE_READ_BIT		BIT(7)
#define DWMCI_MPSCTRL_SECURE_WRITE_BIT		BIT(6)
#define DWMCI_MPSCTRL_NON_SECURE_READ_BIT	BIT(5)
#define DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT	BIT(4)
#define DWMCI_MPSCTRL_USE_FUSE_KEY		BIT(3)
#define DWMCI_MPSCTRL_ECB_MODE			BIT(2)
#define DWMCI_MPSCTRL_ENCRYPTION		BIT(1)
#define DWMCI_MPSCTRL_VALID			BIT(0)

#define EXYNOS4210_FIXED_CIU_CLK_DIV	2
#define EXYNOS4412_FIXED_CIU_CLK_DIV	4

#define EXYNOS_DEF_MMC_0_CAPS	(MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR | \
				MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23)
#define EXYNOS_DEF_MMC_1_CAPS	MMC_CAP_CMD23
#define EXYNOS_DEF_MMC_2_CAPS	(MMC_CAP_CMD23 | MMC_CAP_UHS_SDR104)

#define MAX_TUNING_RETRIES	6
#define MAX_TUNING_LOOP		(MAX_TUNING_RETRIES * 8 * 2)

/* Variations in Exynos specific dw-mshc controller */
enum dw_mci_exynos_type {
	DW_MCI_TYPE_EXYNOS4210,
	DW_MCI_TYPE_EXYNOS4412,
	DW_MCI_TYPE_EXYNOS5250,
	DW_MCI_TYPE_EXYNOS5422,
	DW_MCI_TYPE_EXYNOS5430,
};

/* Exynos implementation specific driver private data */
struct dw_mci_exynos_priv_data {
	u8			ciu_div;
	u32			sdr_timing;
	u32			ddr_timing;
	u32			hs200_timing;
	u32			ddr200_timing;
	u32			*ref_clk;
	const char		*drv_str_pin;
	const char		*drv_str_addr;
	int			drv_str_val;
	u32			delay_line;
	int			drv_str_base_val;
	u32			drv_str_num;
	int			cd_gpio;
	u32			caps;
	u32			ctrl_flag;

#define DW_MMC_EXYNOS_USE_FINE_TUNING		BIT(0)
};

/*
 * Tunning patterns are from emmc4.5 spec section 6.6.7.1
 * Figure 27 (for 8-bit) and Figure 28 (for 4bit).
 */
static const u8 tuning_blk_pattern_4bit[] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

static const u8 tuning_blk_pattern_8bit[] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

extern int dw_mci_exynos_request_status(void);
