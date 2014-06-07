/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <mach/pm_domains.h>

#ifdef CONFIG_SOC_EXYNOS5422

#define SHIFT_GSCL_BLK_300_DIV      4
#define SHIFT_GSCL_BLK_333_432_DIV  6
#define SHIFT_MSCL_BLK_DIV      28
#define SHIFT_MFC_BLK_DIV       0
static DEFINE_SPINLOCK(clk_div2_ratio0_lock);
static DEFINE_SPINLOCK(clk_save_restore_lock);
void __iomem *hdmi_regs;

struct exynos5422_pd_state {
	void __iomem *reg;
	unsigned long val;
	unsigned long set_val;
};

static void exynos5_pd_save_reg(struct exynos5422_pd_state *ptr, int count)
{
	spin_lock(&clk_save_restore_lock);
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("read %p (store %08x)\n", ptr->reg, __raw_readl(ptr->reg));
		ptr->val = __raw_readl(ptr->reg);
	}
	spin_unlock(&clk_save_restore_lock);
}

static void exynos5_pd_restore_reg(struct exynos5422_pd_state *ptr, int count)
{
	spin_lock(&clk_save_restore_lock);
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("restore %p (restore %08lx, was %08x)\n", ptr->reg, ptr->val, __raw_readl(ptr->reg));
		__raw_writel(ptr->val, ptr->reg);
	}
	spin_unlock(&clk_save_restore_lock);
}

static void exynos5_enable_clk(struct exynos5422_pd_state *ptr, int count)
{
	spin_lock(&clk_save_restore_lock);
	for (; count > 0; count--, ptr++) {
		DEBUG_PRINT_INFO("set %p (change %08lx, was %08x) for power on/off\n", ptr->reg, ptr->set_val, __raw_readl(ptr->reg));
		__raw_writel(__raw_readl(ptr->reg) | ptr->set_val, ptr->reg);
	}
	spin_unlock(&clk_save_restore_lock);
}

static void exynos5_pwr_reg_set(struct exynos5422_pd_state *ptr, int count)
{
	spin_lock(&clk_save_restore_lock);
	for (; count > 0; count--, ptr++)
		__raw_writel(ptr->set_val, ptr->reg);
	spin_unlock(&clk_save_restore_lock);
}

static void exynos5_pd_set_fake_rate(void __iomem *regs, unsigned int shift_val)
{
	unsigned int clk_div2_ratio0_value;
	unsigned int clk_div2_ratio0_old;

	clk_div2_ratio0_value = __raw_readl(regs);
	clk_div2_ratio0_old = (clk_div2_ratio0_value >> shift_val) & 0x3;
	clk_div2_ratio0_value &= ~(0x3 << shift_val);
	clk_div2_ratio0_value |= (((clk_div2_ratio0_old + 1) & 0x3) << shift_val);
	__raw_writel(clk_div2_ratio0_value, regs);

	clk_div2_ratio0_value = __raw_readl(regs);
	clk_div2_ratio0_value &= ~(0x3 << shift_val);
	clk_div2_ratio0_value |= (((clk_div2_ratio0_old) & 0x3) << shift_val);
	__raw_writel(clk_div2_ratio0_value, regs);
}

struct exynos5422_pd_state exynos5422_g3d_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_IP_G3D,			.val = 0,
		.set_val = 0x3 << 8 | 0x1 << 6 | 0x1 << 1, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_g3d[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_G3D_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_G3D_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_G3D_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_g3d_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_g3d_clk, ARRAY_SIZE(exynos5422_g3d_clk));
	exynos5_enable_clk(exynos5422_g3d_clk, ARRAY_SIZE(exynos5422_g3d_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_g3d, ARRAY_SIZE(exynos54xx_pwr_reg_g3d));

	return 0;
}

static int exynos5_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	/* restore mout_aclk_g3d_user parent set to mout_aclk_g3d_sw */
	exynos5_pd_restore_reg(exynos5422_g3d_clk, ARRAY_SIZE(exynos5422_g3d_clk));

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP5);
	reg |= (1 << 16);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP5);

	return 0;
}

static int exynos5_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_g3d_clk, ARRAY_SIZE(exynos5422_g3d_clk));
	exynos5_enable_clk(exynos5422_g3d_clk, ARRAY_SIZE(exynos5422_g3d_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_g3d, ARRAY_SIZE(exynos54xx_pwr_reg_g3d));

	return 0;
}

static int exynos5_pd_g3d_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_g3d_clk, ARRAY_SIZE(exynos5422_g3d_clk));

	return 0;
}

struct exynos5422_pd_state exynos5422_mfc_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_IP_MFC,			.val = 0,
		.set_val = 0x7F, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_mfc[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_MFC_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_MFC_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_MFC_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_mfc_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_mfc_clk, ARRAY_SIZE(exynos5422_mfc_clk));
	exynos5_enable_clk(exynos5422_mfc_clk, ARRAY_SIZE(exynos5422_mfc_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_mfc, ARRAY_SIZE(exynos54xx_pwr_reg_mfc));

	return 0;
}

static int exynos5_pd_mfc_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	/* Do not acquire lock to synchronize for MFC BLK */
	exynos5_pd_restore_reg(exynos5422_mfc_clk, ARRAY_SIZE(exynos5422_mfc_clk));
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV4_RATIO, SHIFT_MFC_BLK_DIV);

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP4);
	reg |= (1 << 28);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP4);

	return 0;
}

static int exynos5_pd_mfc_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_mfc_clk, ARRAY_SIZE(exynos5422_mfc_clk));
	exynos5_enable_clk(exynos5422_mfc_clk, ARRAY_SIZE(exynos5422_mfc_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_mfc, ARRAY_SIZE(exynos54xx_pwr_reg_mfc));

	return 0;
}
static int exynos5_pd_mfc_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_mfc_clk, ARRAY_SIZE(exynos5422_mfc_clk));

	return 0;
}

struct exynos5422_pd_state exynos5422_maudio_clk[] = {
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ .reg = EXYNOS5_CLK_SRC_TOP7,			.val = 0,
		.set_val = 0x1 << 20, },
#else
	{ .reg = EXYNOS5_CLK_SRC_TOP6,			.val = 0,
		.set_val = 0x1 << 20, },
#endif
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP7,		.val = 0,
		.set_val = 0x1 << 20, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_MAUDIO,	.val = 0,
		.set_val = 0x1 << 28, },
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_MAU,	.val = 0,
		.set_val = 0x3 << 0, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_mau[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_MAU_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_MAU_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_MAU_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_maudio_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_maudio_clk, ARRAY_SIZE(exynos5422_maudio_clk));
	exynos5_enable_clk(exynos5422_maudio_clk, ARRAY_SIZE(exynos5422_maudio_clk));
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_mau, ARRAY_SIZE(exynos54xx_pwr_reg_mau));
#endif
	return 0;
}

static int exynos5_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_maudio_clk, ARRAY_SIZE(exynos5422_maudio_clk));

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP9);
	reg |= (1 << 8);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP9);

	__raw_writel(0x10000000, EXYNOS_PAD_RET_MAUDIO_OPTION);

	return 0;
}

static int exynos5_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_maudio_clk, ARRAY_SIZE(exynos5422_maudio_clk));
	exynos5_enable_clk(exynos5422_maudio_clk, ARRAY_SIZE(exynos5422_maudio_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_mau, ARRAY_SIZE(exynos54xx_pwr_reg_mau));

	__raw_writel(0, EXYNOS5_PAD_RETENTION_MAU_SYS_PWR_REG);

	return 0;
}

static int exynos5_pd_maudio_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_maudio_clk, ARRAY_SIZE(exynos5422_maudio_clk));

	return 0;
}

struct exynos5422_pd_state exynos5422_disp1_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_BUS_DISP1,		.val = 0,
		.set_val = 0xDFFFDFFF, },
	{ .reg = EXYNOS5_CLK_GATE_IP_DISP1,			.val = 0,
		.set_val = 0x1E6FFF, },
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_DISP1,	.val = 0,
		.set_val = 0x3 << 20 | 0x3 << 9 | 0xF << 0, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_disp1[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_DISP1_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_DISP1_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_DISP1_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_disp1_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_disp1_clk, ARRAY_SIZE(exynos5422_disp1_clk));
	exynos5_enable_clk(exynos5422_disp1_clk, ARRAY_SIZE(exynos5422_disp1_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_disp1, ARRAY_SIZE(exynos54xx_pwr_reg_disp1));

	return 0;
}

static int exynos5_pd_disp1_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_disp1_clk, ARRAY_SIZE(exynos5422_disp1_clk));

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP5);
	reg |= (1 << 24 | 1 << 5);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP5);

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP3);
	reg |= (1 << 8);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP3);

	/* disable HDMI_PHY */
	reg = __raw_readl(EXYNOS5_CLK_GATE_IP_DISP1);
	reg |= (1 << 6);
	__raw_writel(reg, EXYNOS5_CLK_GATE_IP_DISP1);

	__raw_writel(0x1, hdmi_regs + 0x30);

	DEBUG_PRINT_INFO("HDMI phy power off : %x\n", __raw_readl(hdmi_regs + 0x30));

	reg = __raw_readl(EXYNOS5_CLK_GATE_IP_DISP1);
	reg &= ~(1 << 6);
	__raw_writel(reg, EXYNOS5_CLK_GATE_IP_DISP1);

	return 0;
}

static int exynos5_pd_disp1_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_disp1_clk, ARRAY_SIZE(exynos5422_disp1_clk));
	exynos5_enable_clk(exynos5422_disp1_clk, ARRAY_SIZE(exynos5422_disp1_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_disp1, ARRAY_SIZE(exynos54xx_pwr_reg_disp1));

	return 0;
}

static int exynos5_pd_disp1_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_disp1_clk, ARRAY_SIZE(exynos5422_disp1_clk));

	return 0;
}

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
struct exynos5422_pd_state exynos5422_scl_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_IP_GSCL0,			.val = 0,
		.set_val = 0x3 << 28 | 0x3 << 14 | 0x3 << 0, },
	{ .reg = EXYNOS5_CLK_GATE_IP_GSCL1,			.val = 0,
		.set_val = 0x1 << 7 | 0x1 << 6, },
	{ .reg = EXYNOS5_CLK_GATE_IP_CAM,			.val = 0,
		.set_val = 0x1 << 30, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_gscl[] = {
	{ .reg = EXYNOS5_CMU_CLKSTOP_GSCL_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5_CMU_SYSCLK_GSCL_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5_CMU_RESET_GSCL_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_scl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_scl_clk, ARRAY_SIZE(exynos5422_scl_clk));
	exynos5_enable_clk(exynos5422_scl_clk, ARRAY_SIZE(exynos5422_scl_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_gscl, ARRAY_SIZE(exynos54xx_pwr_reg_gscl));

	return 0;
}

static int exynos5_pd_scl_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_scl_clk, ARRAY_SIZE(exynos5422_scl_clk));

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP9);
	reg |= (1 << 28);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP9);

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP5);
	reg |= (1 << 28);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP5);
	return 0;
}

static int exynos5_pd_scl_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_scl_clk, ARRAY_SIZE(exynos5422_scl_clk));
	exynos5_enable_clk(exynos5422_scl_clk, ARRAY_SIZE(exynos5422_scl_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_gscl, ARRAY_SIZE(exynos54xx_pwr_reg_gscl));

	return 0;
}

static int exynos5_pd_scl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_scl_clk, ARRAY_SIZE(exynos5422_scl_clk));

	return 0;
}
struct exynos5422_pd_state exynos5422_cam_clk[] = {

	{ .reg = EXYNOS5_CLK_GATE_IP_GSCL0,			.val = 0,
		.set_val = 0x7 << 24 | 0x1 << 13 | 0xF << 8 | 0x7 << 4, },
	{ .reg = EXYNOS5_CLK_GATE_IP_GSCL1,			.val = 0,
		.set_val = 0x7 << 16 | 0x3 << 12 | 0x7 << 2, },
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_GSCL,	.val = 0,
		.set_val = 0x3 << 6, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_cam[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_CAM_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_CAM_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_CAM_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_cam_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_cam_clk, ARRAY_SIZE(exynos5422_cam_clk));
	exynos5_enable_clk(exynos5422_cam_clk, ARRAY_SIZE(exynos5422_cam_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_cam, ARRAY_SIZE(exynos54xx_pwr_reg_cam));

	return 0;
}

static int exynos5_pd_cam_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_cam_clk, ARRAY_SIZE(exynos5422_cam_clk));

	return 0;
}

static int exynos5_pd_cam_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_cam_clk, ARRAY_SIZE(exynos5422_cam_clk));
	exynos5_enable_clk(exynos5422_cam_clk, ARRAY_SIZE(exynos5422_cam_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_cam, ARRAY_SIZE(exynos54xx_pwr_reg_cam));

	return 0;
}

static int exynos5_pd_cam_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_cam_clk, ARRAY_SIZE(exynos5422_cam_clk));

	return 0;
}
#else
struct exynos5422_pd_state exynos5422_gscl_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_GSCL,	.val = 0,
		.set_val = 0x3<<6, },
};

static int exynos5_pd_gscl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_gscl_clk, ARRAY_SIZE(exynos5422_gscl_clk));
	exynos5_enable_clk(exynos5422_gscl_clk, ARRAY_SIZE(exynos5422_gscl_clk));

	return 0;
}

static int exynos5_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_gscl_clk, ARRAY_SIZE(exynos5422_gscl_clk));

	spin_lock(&clk_div2_ratio0_lock);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_GSCL_BLK_300_DIV);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_GSCL_BLK_333_432_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	return 0;
}

static int exynos5_pd_gscl_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_gscl_clk, ARRAY_SIZE(exynos5422_gscl_clk));
	exynos5_enable_clk(exynos5422_gscl_clk, ARRAY_SIZE(exynos5422_gscl_clk));

	return 0;
}

static int exynos5_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_gscl_clk, ARRAY_SIZE(exynos5422_gscl_clk));

	return 0;
}
#endif

struct exynos5422_pd_state exynos5422_mscl_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_IP_MSCL,			.val = 0,
		.set_val = 0x7FFF1F, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_mscl[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_MSC_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_MSC_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_MSC_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_mscl_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_mscl_clk, ARRAY_SIZE(exynos5422_mscl_clk));
	exynos5_enable_clk(exynos5422_mscl_clk, ARRAY_SIZE(exynos5422_mscl_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_mscl, ARRAY_SIZE(exynos54xx_pwr_reg_mscl));

	return 0;
}

static int exynos5_pd_mscl_power_on_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_mscl_clk, ARRAY_SIZE(exynos5422_mscl_clk));

	reg = __raw_readl(EXYNOS5_CLK_SRC_TOP3);
	reg |= (1 << 4);
	__raw_writel(reg, EXYNOS5_CLK_SRC_TOP3);

	spin_lock(&clk_div2_ratio0_lock);
	exynos5_pd_set_fake_rate(EXYNOS5_CLK_DIV2_RATIO0, SHIFT_MSCL_BLK_DIV);
	spin_unlock(&clk_div2_ratio0_lock);

	return 0;
}

static int exynos5_pd_mscl_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_mscl_clk, ARRAY_SIZE(exynos5422_mscl_clk));
	exynos5_enable_clk(exynos5422_mscl_clk, ARRAY_SIZE(exynos5422_mscl_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_mscl, ARRAY_SIZE(exynos54xx_pwr_reg_mscl));

	return 0;
}

static int exynos5_pd_mscl_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_mscl_clk, ARRAY_SIZE(exynos5422_mscl_clk));

	return 0;
}

struct exynos5422_pd_state exynos5422_fsys_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_BUS_FSYS0,		.val = 0,
		.set_val = 0x3 << 27 | 0x3 << 20 | 0xF << 6 | 0x7 << 0, },
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_FSYS,	.val = 0,
		.set_val = 0x1 << 0, },
};

static int exynos5_pd_fsys_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_fsys_clk, ARRAY_SIZE(exynos5422_fsys_clk));
	exynos5_enable_clk(exynos5422_fsys_clk, ARRAY_SIZE(exynos5422_fsys_clk));

	return 0;
}

static int exynos5_pd_fsys_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_fsys_clk, ARRAY_SIZE(exynos5422_fsys_clk));

	return 0;
}

static int exynos5_pd_fsys_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_fsys_clk, ARRAY_SIZE(exynos5422_fsys_clk));
	exynos5_enable_clk(exynos5422_fsys_clk, ARRAY_SIZE(exynos5422_fsys_clk));

	return 0;
}

static int exynos5_pd_fsys_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_fsys_clk, ARRAY_SIZE(exynos5422_fsys_clk));

	return 0;
}

struct exynos5422_pd_state exynos5422_peric_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_IP_PERIC,			.val = 0,
		.set_val = 0xF5F7FFDF, },
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_PERIC,	.val = 0,
		.set_val = 0xF << 15 | 0x1 << 11 | 0xF << 6 | 0x1F << 0, },
};

static int exynos5_pd_peric_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_peric_clk, ARRAY_SIZE(exynos5422_peric_clk));
	exynos5_enable_clk(exynos5422_peric_clk, ARRAY_SIZE(exynos5422_peric_clk));

	return 0;
}

static int exynos5_pd_peric_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_peric_clk, ARRAY_SIZE(exynos5422_peric_clk));

	return 0;
}

static int exynos5_pd_peric_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_peric_clk, ARRAY_SIZE(exynos5422_peric_clk));
	exynos5_enable_clk(exynos5422_peric_clk, ARRAY_SIZE(exynos5422_peric_clk));

	return 0;
}

static int exynos5_pd_peric_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_peric_clk, ARRAY_SIZE(exynos5422_peric_clk));

	return 0;
}

struct exynos5422_pd_state exynos5422_wcore_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_BUS_WCORE,		.val = 0,
		.set_val = 0x1, },
	{ .reg = EXYNOS5_CLK_SRC_MASK_TOP0,	.val = 0,
		.set_val = 0x1 << 20 | 0x1 << 16, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_wcore[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_WCORE_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_WCORE_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_WCORE_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_wcore_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_wcore_clk, ARRAY_SIZE(exynos5422_wcore_clk));
	exynos5_enable_clk(exynos5422_wcore_clk, ARRAY_SIZE(exynos5422_wcore_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_wcore, ARRAY_SIZE(exynos54xx_pwr_reg_wcore));

	return 0;
}

static int exynos5_pd_wcore_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_wcore_clk, ARRAY_SIZE(exynos5422_wcore_clk));

	return 0;
}

static int exynos5_pd_wcore_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_wcore_clk, ARRAY_SIZE(exynos5422_wcore_clk));
	exynos5_enable_clk(exynos5422_wcore_clk, ARRAY_SIZE(exynos5422_wcore_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_wcore, ARRAY_SIZE(exynos54xx_pwr_reg_wcore));

	return 0;
}

static int exynos5_pd_wcore_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_wcore_clk, ARRAY_SIZE(exynos5422_wcore_clk));

	return 0;
}

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
struct exynos5422_pd_state exynos5422_fimc_is_clk[] = {
	{ .reg = EXYNOS5_CLK_GATE_TOP_SCLK_ISP,			.val = 0,
		.set_val = 0x1 << 12 | 0x1 << 8 | 0x1F, },
	/* related Async-bridge Clock On */
	{ .reg = EXYNOS5_CLK_GATE_IP_GSCL1,			.val = 0,
		.set_val = 0x1 << 18, },
	{ .reg = EXYNOS5_CLK_GATE_BUS_CPU,			.val = 0,
		.set_val = 0x1 << 1, },
};

struct exynos5422_pd_state exynos54xx_pwr_reg_isp[] = {
	{ .reg = EXYNOS5422_CMU_CLKSTOP_ISP_SYS_PWR_REG,	.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_SYSCLK_ISP_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_CMU_RESET_ISP_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
	{ .reg = EXYNOS5422_ISP_ARM_SYS_PWR_REG,		.val = 0,
		.set_val = 0, },
};

static int exynos5_pd_fimc_is_power_on_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_fimc_is_clk, ARRAY_SIZE(exynos5422_fimc_is_clk));
	exynos5_enable_clk(exynos5422_fimc_is_clk, ARRAY_SIZE(exynos5422_fimc_is_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_isp, ARRAY_SIZE(exynos54xx_pwr_reg_isp));

	return 0;
}

static int exynos5_pd_fimc_is_power_on_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_fimc_is_clk, ARRAY_SIZE(exynos5422_fimc_is_clk));

	return 0;
}

static int exynos5_pd_fimc_is_power_off_pre(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_save_reg(exynos5422_fimc_is_clk, ARRAY_SIZE(exynos5422_fimc_is_clk));
	exynos5_enable_clk(exynos5422_fimc_is_clk, ARRAY_SIZE(exynos5422_fimc_is_clk));
	exynos5_pwr_reg_set(exynos54xx_pwr_reg_isp, ARRAY_SIZE(exynos54xx_pwr_reg_isp));

	return 0;
}

static int exynos5_pd_fimc_is_power_off_post(struct exynos_pm_domain *pd)
{
	DEBUG_PRINT_INFO("%s: %08x %08x\n", __func__, __raw_readl(pd->base), __raw_readl(pd->base+4));
	exynos5_pd_restore_reg(exynos5422_fimc_is_clk, ARRAY_SIZE(exynos5422_fimc_is_clk));

	return 0;
}
#endif

static struct exynos_pd_callback pd_callback_list[] = {
	{
		.on_pre = exynos5_pd_g3d_power_on_pre,
		.on_post = exynos5_pd_g3d_power_on_post,
		.name = "pd-g3d",
		.off_pre = exynos5_pd_g3d_power_off_pre,
		.off_post = exynos5_pd_g3d_power_off_post,
	} , {
		.on_pre = exynos5_pd_mfc_power_on_pre,
		.on_post = exynos5_pd_mfc_power_on_post,
		.name = "pd-mfc",
		.off_pre = exynos5_pd_mfc_power_off_pre,
		.off_post = exynos5_pd_mfc_power_off_post,
	} , {
		.on_pre = exynos5_pd_maudio_power_on_pre,
		.on_post = exynos5_pd_maudio_power_on_post,
		.name = "pd-maudio",
		.off_pre = exynos5_pd_maudio_power_off_pre,
		.off_post = exynos5_pd_maudio_power_off_post,
	} , {
		.on_pre = exynos5_pd_disp1_power_on_pre,
		.on_post = exynos5_pd_disp1_power_on_post,
		.name = "pd-disp1",
		.off_pre = exynos5_pd_disp1_power_off_pre,
		.off_post = exynos5_pd_disp1_power_off_post,
	} , {
		.on_pre = exynos5_pd_mscl_power_on_pre,
			.on_post = exynos5_pd_mscl_power_on_post,
			.name = "pd-mscl",
			.off_pre = exynos5_pd_mscl_power_off_pre,
			.off_post = exynos5_pd_mscl_power_off_post,
	} , {
		.on_pre = exynos5_pd_fsys_power_on_pre,
			.on_post = exynos5_pd_fsys_power_on_post,
			.name = "pd-fsys",
			.off_pre = exynos5_pd_fsys_power_off_pre,
			.off_post = exynos5_pd_fsys_power_off_post,
	} , {
#ifndef CONFIG_SOC_EXYNOS5422_REV_0
		.name = "pd-fsys2",
	} , {
#endif
#ifdef NOT_USE_SPEC_OUT
		.name = "pd-psgen",
	} , {
		.name = "pd-g2d",
	} , {
#endif
		.on_pre = exynos5_pd_peric_power_on_pre,
			.on_post = exynos5_pd_peric_power_on_post,
			.name = "pd-peric",
			.on_pre = exynos5_pd_peric_power_off_pre,
			.on_post = exynos5_pd_peric_power_off_post,
	} , {
		.on_pre = exynos5_pd_wcore_power_on_pre,
			.on_post = exynos5_pd_wcore_power_on_post,
			.name = "pd-wcore",
			.on_pre = exynos5_pd_wcore_power_off_pre,
			.on_post = exynos5_pd_wcore_power_off_post,
	} , {
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
		.on_pre = exynos5_pd_fimc_is_power_on_pre,
			.on_post = exynos5_pd_fimc_is_power_on_post,
			.name = "pd-isp",
			.off_pre = exynos5_pd_fimc_is_power_off_pre,
			.off_post = exynos5_pd_fimc_is_power_off_post,
	} , {
#else
		.name = "pd-isp",
	} , {
#endif
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
		.on_pre = exynos5_pd_scl_power_on_pre,
			.on_post = exynos5_pd_scl_power_on_post,
			.name = "pd-gscl",
			.off_pre = exynos5_pd_scl_power_off_pre,
			.off_post = exynos5_pd_scl_power_off_post,
	} , {
		.on_pre = exynos5_pd_cam_power_on_pre,
			.on_post = exynos5_pd_cam_power_on_post,
			.name = "pd-cam",
			.off_pre = exynos5_pd_cam_power_off_pre,
			.off_post = exynos5_pd_cam_power_off_post,
#else
			.on_pre = exynos5_pd_gscl_power_on_pre,
			.on_post = exynos5_pd_gscl_power_on_post,
			.name = "pd-gscl",
			.off_pre = exynos5_pd_gscl_power_off_pre,
			.off_post = exynos5_pd_gscl_power_off_post,
#endif
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

	spin_lock_init(&clk_div2_ratio0_lock);
	spin_lock_init(&clk_save_restore_lock);

	hdmi_regs = ioremap(0x14530000, SZ_32);
	if (IS_ERR_OR_NULL(hdmi_regs))
		pr_err("PM DOMAIN : can't remap of hdmi phy address\n");

	/* find callback function for power domain */
	for (i=0, cb = &pd_callback_list[0]; i<ARRAY_SIZE(pd_callback_list); i++, cb++) {
		if (strcmp(cb->name, pd->name))
			continue;

		DEBUG_PRINT_INFO("%s: found callback function\n", pd->name);
		break;
	}

	pd->cb = cb;
	return cb;
}
#endif
