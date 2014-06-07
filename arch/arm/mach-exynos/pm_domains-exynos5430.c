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

#include <plat/pm.h>

#include <mach/pm_domains.h>
#include <mach/devfreq.h>

#ifdef CONFIG_SOC_EXYNOS5430
static void exynos_pd_notify_power_state(struct exynos_pm_domain *pd, unsigned int turn_on)
{
#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
	exynos5_int_notify_power_status(pd->genpd.name, true);
	exynos5_isp_notify_power_status(pd->genpd.name, true);
	exynos5_disp_notify_power_status(pd->genpd.name, true);
#endif
}

static struct sleep_save exynos_pd_maudio_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_AUD1),
};

static int exynos_pd_maudio_power_on_pre(struct exynos_pm_domain *pd)
{
#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP0);
	reg |= (1<<4);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP0);
#endif

	return 0;
}

/* exynos_pd_maudio_power_on_post - callback after power on.
 * @pd: power domain.
 */
static int exynos_pd_maudio_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	return 0;
}

static int exynos_pd_maudio_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_maudio_clk_save,
			ARRAY_SIZE(exynos_pd_maudio_clk_save));

	return 0;
}

static int exynos_pd_maudio_power_off_post(struct exynos_pm_domain *pd)
{
#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP0);
	reg &= ~(1<<4);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP0);
#endif

	return 0;
}

static struct sleep_save exynos_pd_g3d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G3D1),
};

static int exynos_pd_g3d_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	DEBUG_PRINT_INFO("EXYNOS5430_DIV_G3D: %08x\n", __raw_readl(EXYNOS5430_DIV_G3D));
	DEBUG_PRINT_INFO("EXYNOS5430_SRC_SEL_G3D: %08x\n", __raw_readl(EXYNOS5430_SRC_SEL_G3D));

	return 0;
}

static int exynos_pd_g3d_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_g3d_clk_save,
			ARRAY_SIZE(exynos_pd_g3d_clk_save));

	return 0;
}

static struct sleep_save exynos_pd_mfc0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC0_SECURE_SMMU_MFC),
};

/* exynos_pd_mfc0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc0_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP4);
	reg |= (1<<8 | 1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP4);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_mfc0_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_mfc0_clk_save,
			ARRAY_SIZE(exynos_pd_mfc0_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(3, S5P_VA_SYSREG_MFC0 + 0x200);
	__raw_writel(1, S5P_VA_SYSREG_MFC0 + 0x204);

	return 0;
}

static int exynos_pd_mfc0_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mfc0_clk_save,
			ARRAY_SIZE(exynos_pd_mfc0_clk_save));

	return 0;
}

/* exynos_pd_mfc0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc0_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP4);
	reg &= ~(1<<8 | 1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP4);
#endif

	return 0;
}

static struct sleep_save exynos_pd_mfc1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MFC1_SECURE_SMMU_MFC),
};

/* exynos_pd_mfc1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mfc1_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP4);
	reg |= (1<<24 | 1<<20 | 1<<16);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP4);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_mfc1_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_mfc1_clk_save,
			ARRAY_SIZE(exynos_pd_mfc1_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(3, S5P_VA_SYSREG_MFC1 + 0x200);
	__raw_writel(1, S5P_VA_SYSREG_MFC1 + 0x204);


	return 0;
}

static int exynos_pd_mfc1_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mfc1_clk_save,
			ARRAY_SIZE(exynos_pd_mfc1_clk_save));

	return 0;
}

/* exynos_pd_mfc1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mfc1_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<2);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP4);
	reg &= ~(1<<24 | 1<<20 | 1<<16);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP4);
#endif

	return 0;
}

static struct sleep_save exynos_pd_hevc_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_HEVC_SECURE_SMMU_HEVC),
};

/* exynos_pd_hevc_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_hevc_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP2);
	reg |= (1<<28);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP2);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<3);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_hevc_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(3, S5P_VA_SYSREG_HEVC + 0x200);
	__raw_writel(1, S5P_VA_SYSREG_HEVC + 0x204);

	return 0;
}

static int exynos_pd_hevc_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_hevc_clk_save,
			ARRAY_SIZE(exynos_pd_hevc_clk_save));

	return 0;
}

/* exynos_pd_hevc_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_hevc_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<3);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP2);
	reg &= ~(1<<28);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP2);
#endif

	return 0;
}

static struct sleep_save exynos_pd_gscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_GSCL_SECURE_SMMU_GSCL2),
};

/* exynos_pd_gscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_gscl_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP3);
	reg |= (1<<8);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP3);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<7);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_gscl_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

static int exynos_pd_gscl_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_gscl_clk_save,
			ARRAY_SIZE(exynos_pd_gscl_clk_save));

	return 0;
}

/* exynos_pd_gscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_gscl_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<7);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP3);
	reg &= ~(1<<8);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP3);
#endif

	return 0;
}

static struct sleep_save exynos_pd_disp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_DISP1),
};

/* exynos_pd_disp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 */
static int exynos_pd_disp_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= (1<<7 | 1<<6 | 1<<5 | 1<<2 | 1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	return 0;
}

/* exynos_pd_disp_power_on_post - setup after power on.
 * @pd: power domain.
 *
 * enable DISP dynamic clock gating
 */
static int exynos_pd_disp_power_on_post(struct exynos_pm_domain *pd)
{
	void __iomem* reg;

	s3c_pm_do_restore_core(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	/* Enable DISP dynamic clock gating */
	reg = ioremap(0x13B80000, SZ_4K);
	writel(0x3, reg + 0x200);
	writel(0xf, reg + 0x204);
	writel(0x1f, reg + 0x208);
	writel(0x0, reg + 0x500);
	iounmap(reg);

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

/* exynos_pd_disp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_MIF3 clk.
 * check Decon has been reset.
 */
static int exynos_pd_disp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("disp pre power off\n");
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg |= (1<<7 | 1<<6 | 1<<5 | 1<<2 | 1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	s3c_pm_do_save(exynos_pd_disp_clk_save,
			ARRAY_SIZE(exynos_pd_disp_clk_save));

	return 0;
}

/* exynos_pd_disp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable IP_MIF3 clk.
 * disable SRC_SEL_MIF4/5
 */
static int exynos_pd_disp_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_MIF3);
	reg &= ~(1<<7 | 1<<6 | 1<<5 | 1<<2 | 1<<1);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_MIF3);

	__raw_writel(0x0, EXYNOS5430_SRC_SEL_MIF4);
	__raw_writel(0x0, EXYNOS5430_SRC_SEL_MIF5);

	return 0;
}

static struct sleep_save exynos_pd_mscl_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_M2MSCALER1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_MSCL_SECURE_SMMU_JPEG),
};

/* exynos_pd_mscl_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_mscl_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP_MSCL);
	reg |= (1<<8 | 1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP_MSCL);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<10);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_mscl_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	exynos_pd_notify_power_state(pd, true);

	/* dynamic clock gating enabled */
	__raw_writel(3, S5P_VA_SYSREG_MSCL + 0x200);
	__raw_writel(1, S5P_VA_SYSREG_MSCL + 0x204);
	__raw_writel(0, S5P_VA_SYSREG_MSCL + 0x500);

	return 0;
}

static int exynos_pd_mscl_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_mscl_clk_save,
			ARRAY_SIZE(exynos_pd_mscl_clk_save));

	return 0;
}

/* exynos_pd_mscl_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_mscl_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<10);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP_MSCL);
	reg &= ~(1<<8 | 1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP_MSCL);
#endif

	return 0;
}

static struct sleep_save exynos_pd_g2d_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_G2D_SECURE_SMMU_G2D),
};

/* exynos_pd_g2d_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_g2d_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP3);
	reg |= (1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP3);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<0);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_g2d_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

static int exynos_pd_g2d_power_off_pre(struct exynos_pm_domain *pd)
{
	s3c_pm_do_save(exynos_pd_g2d_clk_save,
			ARRAY_SIZE(exynos_pd_g2d_clk_save));

	return 0;
}

/* exynos_pd_g2d_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_g2d_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<0);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP3);
	reg &= ~(1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP3);
#endif

	return 0;
}

static struct sleep_save exynos_pd_isp_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP0),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP1),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP2),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_ISP3),
};

/* exynos_pd_isp_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_isp_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP0);
	reg |= (1<<4);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP0);

	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP2);
	reg |= (1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP2);

	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP_CAM1);
	reg |= (1<<8 | 1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP_CAM1);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<4);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_isp_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

/* exynos_pd_isp_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_ISP1 clk.
 */
static int exynos_pd_isp_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_ISP1);
	reg |= (1 << 12 | 1 << 11);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_ISP1);

	s3c_pm_do_save(exynos_pd_isp_clk_save,
			ARRAY_SIZE(exynos_pd_isp_clk_save));

	return 0;
}

/* exynos_pd_isp_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_isp_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<4);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP_CAM1);
	reg &= ~(1<<8 | 1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP_CAM1);

	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP2);
	reg &= ~(1<<4 | 1<<0);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP2);

	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP0);
	reg &= ~(1<<4);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP0);
#endif

	return 0;
}

static struct sleep_save exynos_pd_cam0_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM00),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM01),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM02),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM03),
};

/* exynos_pd_cam0_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam0_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<5);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_cam0_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

/* exynos_pd_cam0_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM01 clk.
 */
static int exynos_pd_cam0_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM01);
	reg |= (1 << 12);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM01);

	s3c_pm_do_save(exynos_pd_cam0_clk_save,
			ARRAY_SIZE(exynos_pd_cam0_clk_save));

	return 0;
}

/* exynos_pd_cam0_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam0_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<5);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static struct sleep_save exynos_pd_cam1_clk_save[] = {
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM10),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM11),
	SAVE_ITEM(EXYNOS5430_ENABLE_IP_CAM12),
};

/* exynos_pd_cam1_power_on_pre - setup before power on.
 * @pd: power domain.
 *
 * enable top clock.
 */
static int exynos_pd_cam1_power_on_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-on sequence.\n", pd->name);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP2);
	reg |= (1<<16 | 1<<12 | 1<<8);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP2);
#endif

	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg |= (1<<6);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

	return 0;
}

static int exynos_pd_cam1_power_on_post(struct exynos_pm_domain *pd)
{
	s3c_pm_do_restore_core(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	exynos_pd_notify_power_state(pd, true);

	return 0;
}

/* exynos_pd_cam1_power_off_pre - setup before power off.
 * @pd: power domain.
 *
 * enable IP_CAM11 clk.
 */
static int exynos_pd_cam1_power_off_pre(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is preparing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_CAM11);
	reg |= (1 << 19);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_CAM11);

	s3c_pm_do_save(exynos_pd_cam1_clk_save,
			ARRAY_SIZE(exynos_pd_cam1_clk_save));

	return 0;
}

/* exynos_pd_cam1_power_off_post - clean up after power off.
 * @pd: power domain.
 *
 * disable top clock.
 */
static int exynos_pd_cam1_power_off_post(struct exynos_pm_domain *pd)
{
	unsigned int reg;

	DEBUG_PRINT_INFO("%s is clearing power-off sequence.\n", pd->name);
	reg = __raw_readl(EXYNOS5430_ENABLE_IP_TOP);
	reg &= ~(1<<6);
	__raw_writel(reg, EXYNOS5430_ENABLE_IP_TOP);

#if defined(EXYNOS5430_CLK_SRC_GATING) && defined(CONFIG_SOC_EXYNOS5430_REV_1)
	reg = __raw_readl(EXYNOS5430_SRC_ENABLE_TOP2);
	reg &= ~(1<<16 | 1<<12 | 1<<8);
	__raw_writel(reg, EXYNOS5430_SRC_ENABLE_TOP2);
#endif

	return 0;
}

/* helpers for special power-off sequence with LPI control */
#define __set_mask(name) __raw_writel(name##_ALL, name)
#define __clr_mask(name) __raw_writel(~(name##_ALL), name)

static int force_down_pre(const char *name)
{
	unsigned int reg;

	if (strncmp(name, "pd-cam0", 7) == 0) {
		__set_mask(EXYNOS5430_LPI_MASK_CAM0_BUSMASTER);
		__set_mask(EXYNOS5430_LPI_MASK_CAM0_ASYNCBRIDGE);
		__set_mask(EXYNOS5430_LPI_MASK_CAM0_NOCBUS);
	} else if (strncmp(name, "pd-cam1", 7) == 0) {
		/* in case of cam1, should be clear STANDBY_WFI */
		reg = __raw_readl(EXYNOS5430_A5IS_OPTION);
		reg &= ~(1 << 16);
		__raw_writel(reg, EXYNOS5430_A5IS_OPTION);

		__set_mask(EXYNOS5430_LPI_MASK_CAM1_BUSMASTER);
		__set_mask(EXYNOS5430_LPI_MASK_CAM1_ASYNCBRIDGE);
		__set_mask(EXYNOS5430_LPI_MASK_CAM1_NOCBUS);
	} else if (strncmp(name, "pd-isp", 6) == 0) {
		__set_mask(EXYNOS5430_LPI_MASK_ISP_BUSMASTER);
		__set_mask(EXYNOS5430_LPI_MASK_ISP_ASYNCBRIDGE);
		__set_mask(EXYNOS5430_LPI_MASK_ISP_NOCBUS);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int force_down_post(const char *name)
{
	return 0;
}

static unsigned int check_power_status(struct exynos_pm_domain *pd, int power_flags,
					unsigned int timeout)
{
	/* check STATUS register */
	while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
		if (timeout == 0) {
			pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			return 0;
		}
		--timeout;
		cpu_relax();
		usleep_range(8, 10);
	}

	return timeout;
}

#define TIMEOUT_COUNT	100 /* about 1ms, based on 10us */
static int exynos_pd_power_off_custom(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* sc_feedback to OPTION register */
		__raw_writel(0x0102, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		timeout = check_power_status(pd, power_flags, TIMEOUT_COUNT);

		if (unlikely(!timeout)) {
			pr_err(PM_DOMAIN_PREFIX "%s can't control power, try again\n", pd->name);

			/* check power ON status */
			if (__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) {
				if (force_down_pre(pd->name))
					pr_warn("%s: failed to make force down state\n", pd->name);

				timeout = check_power_status(pd, power_flags, TIMEOUT_COUNT);

				if (force_down_post(pd->name))
					pr_warn("%s: failed to restore normal state\n", pd->name);

				if (unlikely(!timeout)) {
					pr_err(PM_DOMAIN_PREFIX "%s can't control power forcedly, timeout\n",
							pd->name);
					mutex_unlock(&pd->access_lock);
					return -ETIMEDOUT;
				} else {
					pr_warn(PM_DOMAIN_PREFIX "%s force power down success\n", pd->name);
				}
			} else {
				pr_warn(PM_DOMAIN_PREFIX "%s power-off already\n", pd->name);
			}
		}

		if (unlikely(timeout < (TIMEOUT_COUNT >> 1))) {
			pr_warn("%s@%p: %08x, %08x, %08x\n",
					pd->name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n",
					pd->name, power_flags ? "on":"off");
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
				pd->genpd.name, pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));

	return 0;
}
static struct exynos_pd_callback pd_callback_list[] = {
	{
		.name = "pd-maudio",
		.on_pre = exynos_pd_maudio_power_on_pre,
		.on_post = exynos_pd_maudio_power_on_post,
		.off_pre = exynos_pd_maudio_power_off_pre,
		.off_post = exynos_pd_maudio_power_off_post,
	}, {
		.name = "pd-mfc0",
		.on_pre = exynos_pd_mfc0_power_on_pre,
		.on_post = exynos_pd_mfc0_power_on_post,
		.off_pre = exynos_pd_mfc0_power_off_pre,
		.off_post = exynos_pd_mfc0_power_off_post,
	}, {
		.name = "pd-mfc1",
		.on_pre = exynos_pd_mfc1_power_on_pre,
		.on_post = exynos_pd_mfc1_power_on_post,
		.off_pre = exynos_pd_mfc1_power_off_pre,
		.off_post = exynos_pd_mfc1_power_off_post,
	}, {
		.name = "pd-hevc",
		.on_pre = exynos_pd_hevc_power_on_pre,
		.on_post = exynos_pd_hevc_power_on_post,
		.off_pre = exynos_pd_hevc_power_off_pre,
		.off_post = exynos_pd_hevc_power_off_post,
	}, {
		.name = "pd-gscl",
		.on_pre = exynos_pd_gscl_power_on_pre,
		.on_post = exynos_pd_gscl_power_on_post,
		.off_pre = exynos_pd_gscl_power_off_pre,
		.off_post = exynos_pd_gscl_power_off_post,
	}, {
		.name = "pd-g3d",
		.on_post = exynos_pd_g3d_power_on_post,
		.off_pre = exynos_pd_g3d_power_off_pre,
	}, {
		.name = "pd-disp",
		.on_pre = exynos_pd_disp_power_on_pre,
		.on_post = exynos_pd_disp_power_on_post,
		.off_pre = exynos_pd_disp_power_off_pre,
		.off_post = exynos_pd_disp_power_off_post,
	}, {
		.name = "pd-mscl",
		.on_pre = exynos_pd_mscl_power_on_pre,
		.on_post = exynos_pd_mscl_power_on_post,
		.off_pre = exynos_pd_mscl_power_off_pre,
		.off_post = exynos_pd_mscl_power_off_post,
	}, {
		.name = "pd-g2d",
		.on_pre = exynos_pd_g2d_power_on_pre,
		.on_post = exynos_pd_g2d_power_on_post,
		.off_pre = exynos_pd_g2d_power_off_pre,
		.off_post = exynos_pd_g2d_power_off_post,
	}, {
		.name = "pd-isp",
		.on_pre = exynos_pd_isp_power_on_pre,
		.on_post = exynos_pd_isp_power_on_post,
		.off_pre = exynos_pd_isp_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_isp_power_off_post,
	}, {
		.name = "pd-cam0",
		.on_pre = exynos_pd_cam0_power_on_pre,
		.on_post = exynos_pd_cam0_power_on_post,
		.off_pre = exynos_pd_cam0_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam0_power_off_post,
	}, {
		.name = "pd-cam1",
		.on_pre = exynos_pd_cam1_power_on_pre,
		.on_post = exynos_pd_cam1_power_on_post,
		.off_pre = exynos_pd_cam1_power_off_pre,
		.off = exynos_pd_power_off_custom,
		.off_post = exynos_pd_cam1_power_off_post,
	},
};

struct exynos_pd_callback * exynos_pd_find_callback(struct exynos_pm_domain *pd)
{
	struct exynos_pd_callback *cb = NULL;
	int i;

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
