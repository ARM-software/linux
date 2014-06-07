/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <mach/regs-clock.h>

#define MIPI_PHY_BIT0					(1 << 0)
#define MIPI_PHY_BIT1					(1 << 1)

#if defined(CONFIG_SOC_EXYNOS5422)
static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	void __iomem *addr_phy;
	u32 cfg;

	addr_phy = S5P_MIPI_DPHY_CONTROL(id);

	cfg = __raw_readl(addr_phy);
	cfg = (cfg | S5P_MIPI_DPHY_SRESETN);
	__raw_writel(cfg, addr_phy);

	if (1) {
		cfg |= S5P_MIPI_DPHY_ENABLE;
	} else if (!(cfg & (S5P_MIPI_DPHY_SRESETN | S5P_MIPI_DPHY_MRESETN)
			& (~S5P_MIPI_DPHY_SRESETN))) {
		cfg &= ~S5P_MIPI_DPHY_ENABLE;
	}

	__raw_writel(cfg, addr_phy);

	return 0;
}
#else
static int __exynos5_mipi_phy_control(int id, bool on, u32 reset)
{
	static DEFINE_SPINLOCK(lock);
	void __iomem *addr_phy;
	void __iomem *addr_reset;
	unsigned long flags;
	u32 cfg;
	u32 csi_reset = 0;
	u32 dsi_reset = 0;

	addr_phy = S5P_MIPI_DPHY_CONTROL(id);

	spin_lock_irqsave(&lock, flags);

	/* PHY reset */
	switch(id) {
	case 0:
		if (reset == S5P_MIPI_DPHY_SRESETN) {
			if (readl(S5P_VA_PMU + 0x4024) & 0x1) {
				addr_reset = S5P_VA_SYSREG_CAM0 + 0x0014;
				cfg = __raw_readl(addr_reset);
				cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		} else {
			if (readl(S5P_VA_PMU + 0x4084) & 0x1) {
				addr_reset = S5P_VA_SYSREG_DISP + 0x000c;
				cfg = __raw_readl(addr_reset);
				cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
				__raw_writel(cfg, addr_reset);
			}
		}
		break;
	case 1:
		if (readl(S5P_VA_PMU + 0x4024) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM0 + 0x0014;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT1) : (cfg & ~MIPI_PHY_BIT1);
			__raw_writel(cfg, addr_reset);
		}
		break;
	case 2:
		if (readl(S5P_VA_PMU + 0x40A4) & 0x1) {
			addr_reset = S5P_VA_SYSREG_CAM1 + 0x0020;
			cfg = __raw_readl(addr_reset);
			cfg = on ? (cfg | MIPI_PHY_BIT0) : (cfg & ~MIPI_PHY_BIT0);
			__raw_writel(cfg, addr_reset);
		}
		break;
	default:
		pr_err("id(%d) is invalid", id);
		return -EINVAL;
	}

	/* CHECK CMA0 PD STATUS */
	if (readl(S5P_VA_PMU + 0x4024) & 0x1) {
		addr_reset = S5P_VA_SYSREG_CAM0 + 0x0014;
		csi_reset = __raw_readl(addr_reset);
	}

	/* CHECK DISP PD STATUS */
	if (readl(S5P_VA_PMU + 0x4084) & 0x1) {
		addr_reset = S5P_VA_SYSREG_DISP + 0x000c;
		dsi_reset = __raw_readl(addr_reset);
	}

	/* PHY PMU enable */
	cfg = __raw_readl(addr_phy);

	if (on)
		cfg |= S5P_MIPI_DPHY_ENABLE;
	else {
		if (id == 0) {
			if(!((csi_reset | dsi_reset) & MIPI_PHY_BIT0))
				cfg &= ~S5P_MIPI_DPHY_ENABLE;
		} else {
			cfg &= ~S5P_MIPI_DPHY_ENABLE;
		}
	}

	__raw_writel(cfg, addr_phy);
	spin_unlock_irqrestore(&lock, flags);

	return 0;
}
#endif

int exynos5_csis_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, S5P_MIPI_DPHY_SRESETN);
}
EXPORT_SYMBOL(exynos5_csis_phy_enable);

int exynos5_dism_phy_enable(int id, bool on)
{
	return __exynos5_mipi_phy_control(id, on, S5P_MIPI_DPHY_MRESETN);
}
EXPORT_SYMBOL(exynos5_dism_phy_enable);
