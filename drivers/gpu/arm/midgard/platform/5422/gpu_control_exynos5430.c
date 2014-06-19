/* drivers/gpu/t6xx/kbase/src/platform/gpu_control_exynos5430.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_control_exynos5430.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>

#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

#define L2CONFIG_MO_1BY8			0b0101
#define L2CONFIG_MO_1BY4			0b1010
#define L2CONFIG_MO_1BY2			0b1111
#define L2CONFIG_MO_NO_RESTRICT		0

#if GPU_DYNAMIC_CLK_GATING
#define G3D_NOC_DCG_EN 0x14a90200
#endif

extern struct kbase_device *pkbdev;

#ifdef CONFIG_PM_RUNTIME
struct exynos_pm_domain *gpu_get_pm_domain(kbase_device *kbdev)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	struct exynos_pm_domain *pd_temp, *pd = NULL;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd")
	{
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd_temp = platform_get_drvdata(pdev);
		if (!strcmp("pd-g3d", pd_temp->genpd.name)) {
			pd = pd_temp;
			break;
		}
	}

	return pd;
}
#endif

int get_cpu_clock_speed(u32 *cpu_clock)
{
	struct clk *cpu_clk;
	u32 freq = 0;
	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return -1;
	freq = clk_get_rate(cpu_clk);
	*cpu_clock = (freq/MHZ);
	return 0;
}

int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS5430_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN) ? 1 : 0;
}

int gpu_power_init(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, "g3d power initialized\n");

	return 0;
}

static int gpu_update_clock(struct exynos_context *platform)
{
	if (!platform->aclk_g3d) {
		GPU_LOG(DVFS_ERROR, "aclk_g3d is not initialized\n");
		return -1;
	}

	platform->cur_clock = clk_get_rate(platform->aclk_g3d)/MHZ;
	return 0;
}

int gpu_is_clock_on(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	return __clk_is_enabled(platform->aclk_g3d);
}

#if GPU_DYNAMIC_CLK_GATING
int gpu_dcg_enable(struct exynos_context *platform)
{
       int *p_dcg;

       if (!platform)
               return -ENODEV;

       if (!gpu_is_power_on()) {
               GPU_LOG(DVFS_WARNING, "can't set clock on in g3d power off status\n");
               return -1;
       }

       p_dcg = (int *)ioremap_nocache(G3D_NOC_DCG_EN, 4);
       if (platform->cur_clock < 275) *p_dcg = 0x3;    /* ACLK_G3DND_600 is the same as the ACLK_G3D, so the current clock is read */
       else *p_dcg = 0x1;
       iounmap(p_dcg);

       return 0;
}

int gpu_dcg_disable(struct exynos_context *platform)
{
       int *p_dcg;

       if (!platform)
               return -ENODEV;

       p_dcg = (int *)ioremap_nocache(G3D_NOC_DCG_EN, 4);
       *p_dcg = 0x0;
       iounmap(p_dcg);

       return 0;
}
#endif

int gpu_clock_on(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, "can't set clock on in g3d power off status\n");
		return -1;
	}

	if (platform->clk_g3d_status == 1)
		return 0;

	if (platform->aclk_g3d)
		(void) clk_prepare_enable(platform->aclk_g3d);

#if GPU_DYNAMIC_CLK_GATING
	gpu_dcg_enable(platform);
#endif
	platform->clk_g3d_status = 1;

	return 0;
}

int gpu_clock_off(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

#if GPU_DYNAMIC_CLK_GATING
	gpu_dcg_disable(platform);
#endif

	if (platform->clk_g3d_status == 0)
		return 0;

	if (platform->aclk_g3d)
		(void)clk_disable_unprepare(platform->aclk_g3d);

	platform->clk_g3d_status = 0;

	return 0;
}

static int gpu_set_maximum_outstanding_req(int val)
{
	volatile unsigned int reg;

	if(val > 0b1111)
		return -1;

	if (!pkbdev)
		return -2;

	if (!gpu_is_power_on())
		return -3;

	reg = kbase_os_reg_read(pkbdev, GPU_CONTROL_REG(L2_MMU_CONFIG));
	reg &= ~(0b1111 << 24);
	reg |= ((val & 0b1111) << 24);
	kbase_os_reg_write(pkbdev, GPU_CONTROL_REG(L2_MMU_CONFIG), reg);

	return 0;
}

int gpu_set_clock(struct exynos_context *platform, int freq)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = freq * MHZ;
	int ret = 0;

	if (platform->aclk_g3d == 0)
		return -1;

#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_WARNING, "gpu_set_clk_vol in the G3D power-off state!\n");
		goto err;
	}

	if (!gpu_is_clock_on(platform)) {
		ret = -1;
		GPU_LOG(DVFS_WARNING, "gpu_set_clk_vol in the G3D clock-off state!\n");
		goto err;
	}

	g3d_rate_prev = clk_get_rate(platform->fout_g3d_pll);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {
		ret = gpu_set_maximum_outstanding_req(L2CONFIG_MO_1BY8);
		if ( ret < 0)
			GPU_LOG(DVFS_ERROR, "failed to set MO (%d)\n", ret);

		/*change here for future stable clock changing*/
		ret = clk_set_parent(platform->mout_g3d_pll, platform->fin_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_parent [mout_g3d_pll]\n");
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(platform->fout_g3d_pll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [fout_g3d_pll]\n");
			goto err;
		}

		/*restore parent*/
		ret = clk_set_parent(platform->mout_g3d_pll, platform->fout_g3d_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_parent [mout_g3d_pll]\n");
			goto err;
		}

		ret = gpu_set_maximum_outstanding_req(L2CONFIG_MO_NO_RESTRICT);
		if ( ret < 0)
			GPU_LOG(DVFS_ERROR, "failed to restore MO (%d)\n", ret);

		g3d_rate_prev = g3d_rate;
	}

	gpu_update_clock(platform);
	GPU_LOG(DVFS_DEBUG, "[G3D] clock set: %ld\n", g3d_rate / MHZ);
	GPU_LOG(DVFS_DEBUG, "[G3D] clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_PM_RUNTIME */
	return ret;
}

static int gpu_get_clock(kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	platform->fin_pll = clk_get(kbdev->osdev.dev, "fin_pll");
	if (IS_ERR(platform->fin_pll)  || (platform->fin_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [fin_pll]\n");
		return -1;
	}

	platform->fout_g3d_pll = clk_get(NULL, "fout_g3d_pll");
	if (IS_ERR(platform->fout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [fout_g3d_pll]\n");
		return -1;
	}

	platform->aclk_g3d = clk_get(kbdev->osdev.dev, "aclk_g3d");
	if (IS_ERR(platform->aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [aclk_g3d]\n");
		return -1;
	}

	platform->dout_aclk_g3d = clk_get(kbdev->osdev.dev, "dout_aclk_g3d");
	if (IS_ERR(platform->dout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [dout_aclk_g3d]\n");
		return -1;
	}

	platform->mout_g3d_pll = clk_get(kbdev->osdev.dev, "mout_g3d_pll");
	if (IS_ERR(platform->mout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_g3d_pll]\n");
		return -1;
	}

	return 0;
}

int gpu_clock_init(kbase_device *kbdev)
{
	int ret;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

	GPU_LOG(DVFS_INFO, "g3d clock initialized\n");

	return 0;
}

static int gpu_update_voltage(struct exynos_context *platform)
{
#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	platform->cur_voltage = regulator_get_voltage(platform->g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return 0;
}

int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return 0;

#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	if (regulator_set_voltage(platform->g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to set voltage, voltage: %d\n", vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	_vol = vol;

	gpu_update_voltage(platform);
	GPU_LOG(DVFS_DEBUG, "[G3D] voltage set:%d\n", vol);
	GPU_LOG(DVFS_DEBUG, "[G3D] voltage get:%d\n", platform->cur_voltage);

	return 0;
}

#ifdef CONFIG_REGULATOR
int gpu_regulator_enable(struct exynos_context *platform)
{
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	if (regulator_enable(platform->g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to enable g3d regulator\n");
		return -1;
	}
	return 0;
}

int gpu_regulator_disable(struct exynos_context *platform)
{
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -1;
	}

	if (regulator_disable(platform->g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to disable g3d regulator\n");
		return -1;
	}
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

	platform->g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(platform->g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, "failed to get mali t6xx regulator, 0x%p\n", platform->g3d_regulator);
		platform->g3d_regulator = NULL;
		return -1;
	}

	if (gpu_regulator_enable(platform) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to enable mali t6xx regulator\n");
		platform->g3d_regulator = NULL;
		return -1;
	}

	gpu_voltage = get_match_volt(ID_G3D, MALI_DVFS_BL_CONFIG_FREQ*1000);
	if (gpu_voltage == 0)
		gpu_voltage = GPU_DEFAULT_VOLTAGE;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, "failed to set mali t6xx operating voltage [%d]\n", gpu_voltage);
		return -1;
	}

	GPU_LOG(DVFS_INFO, "g3d regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */
