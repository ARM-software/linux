/*
 * exynos-mipi-lli-mphy.h - Exynos MIPI-LLI MPHY Header
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DRIVERS_EXYNOS_MIPI_LLI_MPHY_H
#define __DRIVERS_EXYNOS_MIPI_LLI_MPHY_H

/* Phy Attributes */
#define PHY_TX_HSMODE_CAP(LANE)				(1024*LANE + 0x01*4)
#define PHY_TX_HSGEAR_CAP(LANE)				(1024*LANE + 0x02*4)
#define PHY_TX_PWMG0_CAP(LANE)				(1024*LANE + 0x03*4)
#define PHY_TX_PWMGEAR_CAP(LANE)			(1024*LANE + 0x04*4)
#define PHY_TX_AMP_CAP(LANE)				(1024*LANE + 0x05*4)
#define PHY_TX_EXTSYNC_CAP(LANE)			(1024*LANE + 0x06*4)
#define PHY_TX_HS_UNT_LINE_DRV_CAP(LANE)		(1024*LANE + 0x07*4)
#define PHY_TX_LS_TERM_LINE_DRV_CAP(LANE)		(1024*LANE + 0x08*4)
#define PHY_TX_MIN_SLEEP_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x09*4)
#define PHY_TX_MIN_STALL_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x0a*4)
#define PHY_TX_MIN_SAVE_CONFIG_TIME_CAP(LANE)		(1024*LANE + 0x0b*4)
#define PHY_TX_REF_CLK_SHARED_CAP(LANE)			(1024*LANE + 0x0c*4)
#define PHY_TX_PHY_MAJORMINOR_RELEASE_CAP(LANE)		(1024*LANE + 0x0d*4)
#define PHY_TX_PHY_EDITORIAL_RELEASE_CAP(LANE)		(1024*LANE + 0x0e*4)
#define PHY_TX_MODE(LANE)				(1024*LANE + 0x21*4)
#define PHY_TX_HSRATE_SERIES(LANE)			(1024*LANE + 0x22*4)
#define PHY_TX_HSGEAR(LANE)				(1024*LANE + 0x23*4)
#define PHY_TX_PWMGEAR(LANE)				(1024*LANE + 0x24*4)
#define PHY_TX_AMPLITUDE(LANE)				(1024*LANE + 0x25*4)
#define PHY_TX_HS_SLEWRATE(LANE)			(1024*LANE + 0x26*4)
#define PHY_TX_SYNC_SOURCE(LANE)			(1024*LANE + 0x27*4)
#define PHY_TX_HS_SYNC_LENGTH(LANE)			(1024*LANE + 0x28*4)
#define PHY_TX_HS_PREPARE_LENGTH(LANE)			(1024*LANE + 0x29*4)
#define PHY_TX_LS_PREPARE_LENGTH(LANE)			(1024*LANE + 0x2a*4)
#define PHY_TX_HIBERN8_CONTROL(LANE)			(1024*LANE + 0x2b*4)
#define PHY_TX_LCC_ENABLE(LANE)				(1024*LANE + 0x2C*4)
#define PHY_TX_PWM_BURST_CLOSURE_EXT(LANE)		(1024*LANE + 0x2D*4)
#define PHY_TX_BYPASS_8B10B_ENABLE(LANE)		(1024*LANE + 0x2E*4)
#define PHY_TX_DRIVER_POLARITY(LANE)			(1024*LANE + 0x2F*4)
#define PHY_TX_HS_UNT_LINE_DRV_ENABLE(LANE)		(1024*LANE + 0x30*4)
#define PHY_TX_LS_TERM_LINE_DRV_ENABLE(LANE)		(1024*LANE + 0x31*4)
#define PHY_TX_LCC_SEQUENCER(LANE)			(1024*LANE + 0x32*4)
#define PHY_TX_MIN_ACTIVATETIME(LANE)			(1024*LANE + 0x33*4)
#define PHY_TX_FSM_STATE(LANE)				(1024*LANE + 0x41*4)
#define PHY_RX_HSMODE_CAP(LANE)				(1024*LANE + 0x81*4)
#define PHY_RX_HSGEAR_CAP(LANE)				(1024*LANE + 0x82*4)
#define PHY_RX_PWMG0_CAP(LANE)				(1024*LANE + 0x83*4)
#define PHY_RX_PWMGEAR_CAP(LANE)			(1024*LANE + 0x84*4)
#define PHY_RX_HS_UNT_CAP(LANE)				(1024*LANE + 0x85*4)
#define PHY_RX_LS_TERM_CAP(LANE)			(1024*LANE + 0x86*4)
#define PHY_RX_MIN_SLEEP_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x87*4)
#define PHY_RX_MIN_STALL_NOCFG_TIME_CAP(LANE)		(1024*LANE + 0x88*4)
#define PHY_RX_MIN_SAVE_CONFIG_TIME_CAP(LANE)		(1024*LANE + 0x89*4)
#define PHY_RX_REF_CLK_SHARED_CAP(LANE)			(1024*LANE + 0x8A*4)
#define PHY_RX_HS_SYNC_LENGTH(LANE)			(1024*LANE + 0x8B*4)
#define PHY_RX_HS_PREPARE_LENGTH_CAP(LANE)		(1024*LANE + 0x8C*4)
#define PHY_RX_LS_PREPARE_LENGTH_CAP(LANE)		(1024*LANE + 0x8D*4)
#define PHY_RX_PWM_BURST_CLOSURE_LENGTH_CAP(LANE)	(1024*LANE + 0x8E*4)
#define PHY_RX_MIN_ACTIVATETIME(LANE)			(1024*LANE + 0x8F*4)
#define PHY_RX_PHY_MAJORMINOR_RELEASE_CAP(LANE)		(1024*LANE + 0x90*4)
#define PHY_RX_PHY_EDITORIAL_RELEASE_CAP(LANE)		(1024*LANE + 0x91*4)
#define PHY_RX_MODE(LANE)				(1024*LANE + 0xA1*4)
#define PHY_RX_HSRATE_SERIES(LANE)			(1024*LANE + 0xA2*4)
#define PHY_RX_HSGEAR(LANE)				(1024*LANE + 0xA3*4)
#define PHY_RX_PWMGEAR(LANE)				(1024*LANE + 0xA4*4)
#define PHY_RX_LS_TERM_ENABLE(LANE)			(1024*LANE + 0xA5*4)
#define PHY_RX_HS_UNT_ENABLE(LANE)			(1024*LANE + 0xA6*4)
#define PHY_RX_ENTER_HIBERN8(LANE)			(1024*LANE + 0xA7*4)
#define PHY_RX_BYPASS_8B10B_ENABLE(LANE)		(1024*LANE + 0xA8*4)
#define PHY_RX_FSM_STATE(LANE)				(1024*LANE + 0xC1*4)
#define PHY_RX_FILLER_INSERTION_ENABLE(LANE)		(1024*LANE + 0x16*4)

enum phy_sfr_type {
	LOCAL_SFR, REMOTE_SFR,
};

enum phy_mode_type {
	PWM_G1, PWM_G2, PWM_G3, PWM_G4, PWM_G5, PWM_G6, PWM_G7,
	HS_G1, HS_G2, HS_G3,
};

struct exynos_mphy {
	struct device	*dev;
	struct clk	*clk;
	void __iomem	*loc_regs;
	void __iomem	*rem_regs;
	u8		lane;
	spinlock_t	lock;

	enum phy_mode_type default_mode;
	bool is_shared_clk;

	int (*init)(struct exynos_mphy *phy);
	int (*cmn_init)(struct exynos_mphy *phy);
	int (*ovtm_init)(struct exynos_mphy *phy);
	int (*shutdown)(struct exynos_mphy *phy);
};

struct device *exynos_get_mphy(void);
int exynos_mphy_init(struct exynos_mphy *phy);
int exynos_mphy_init2(struct exynos_mphy *phy);
int exynos_mphy_shared_clock(struct exynos_mphy *phy);

#endif /* __DRIVERS_EXYNOS_MIPI_LLI_MPHY_H */
