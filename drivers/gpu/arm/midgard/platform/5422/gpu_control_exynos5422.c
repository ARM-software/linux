/*
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
 * @file gpu_control_exynos5422.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>

#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

extern struct kbase_device *pkbdev;

#ifdef KBASE_PM_RUNTIME
struct exynos_pm_domain *gpu_get_pm_domain(struct kbase_device *kbdev)
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
		return PTR_ERR(cpu_clk);
	freq = clk_get_rate(cpu_clk);
	*cpu_clock = (freq/MHZ);
	return 0;
}

int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS5422_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN) ? 1 : 0;
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, "g3d power initialized\n");

	return 0;
}

static int gpu_update_clock(struct exynos_context *platform)
{
	if (!platform->clk_g3d_ip) {
		GPU_LOG(DVFS_ERROR, "clk_g3d_ip is not initialized\n");
		return -ENODEV;
	}

	platform->cur_clock = clk_get_rate(platform->clk_g3d_ip)/MHZ;
	return 0;
}

int gpu_is_clock_on(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	return __clk_is_enabled(platform->clk_g3d_ip);
}

int gpu_clock_on(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, "can't set clock on in g3d power off status\n");
		return -1;
	}

	if (platform->clk_g3d_ip && !gpu_is_clock_on(platform)) {
		(void) clk_prepare_enable(platform->clk_g3d_ip);
	}

	return 0;
}

int gpu_clock_off(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (platform->clk_g3d_ip && gpu_is_clock_on(platform)) {
		(void)clk_disable_unprepare(platform->clk_g3d_ip);
	}

	return 0;
}


unsigned long get_dpll_freq(int curr, int targ)
{
	unsigned long dpll_clk;
	int divider;

	switch(targ)
	{
		case 480: case 420: case 350:
			divider = 2;
			break;
		case 266:
			divider = 2 + (targ < curr ? 0:1);
			break;
		case 177:
			divider = 3 + (targ < curr ? 0:1);
			break;
		case 100:
			divider = 4;
			break;
		default:
			divider = 1;
		 break;
	}
	dpll_clk = curr / divider + 5;

	return (dpll_clk*1000000);
}

int gpu_set_clock(struct exynos_context *platform, int freq)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = freq * MHZ;
	unsigned long tmp = 0;
	int ret;

	if (platform->clk_g3d_ip == 0)
		return -ENODEV;

#ifdef KBASE_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* KBASE_PM_RUNTIME */

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

	g3d_rate_prev = clk_get_rate(platform->fout_vpll)/MHZ;

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (freq != g3d_rate_prev) {
		/*for stable clock input.*/
		ret = clk_set_rate(platform->dout_aclk_g3d,
			get_dpll_freq(clk_get_rate(platform->clk_g3d_ip)/1000000, freq));
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [dout_aclk_g3d]\n");
			goto err;
		}

		/*change here for future stable clock changing*/
		ret = clk_set_parent(platform->mout_aclk_g3d, platform->mout_dpll_ctrl);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_parent [mout_aclk_g3d]\n");
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(platform->fout_vpll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [fout_vpll]\n");
			goto err;
		}

		/*restore parent*/
		ret = clk_set_parent(platform->mout_aclk_g3d, platform->mout_vpll_ctrl);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, "failed to clk_set_parent [mout_aclk_g3d]\n");
			goto err;
		}

		g3d_rate_prev = g3d_rate;
	}

	ret = clk_set_rate(platform->dout_aclk_g3d, g3d_rate);
	if (ret < 0) {
		GPU_LOG(DVFS_ERROR, "failed to clk_set_rate [dout_aclk_g3d]\n");
		goto err;
	}

	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(EXYNOS5_CLK_DIV_STAT_TOP2);
	} while (tmp & 0x10000);

	gpu_update_clock(platform);
	KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_CLOCK_VALUE, NULL, NULL, 0u, g3d_rate/MHZ);
	GPU_LOG(DVFS_DEBUG, "[G3D] clock set: %ld\n", g3d_rate / MHZ);
	GPU_LOG(DVFS_DEBUG, "[G3D] clock get: %d\n", platform->cur_clock);
err:
#ifdef KBASE_PM_RUNTIME
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* KBASE_PM_RUNTIME */
	return ret;
}

static int gpu_get_clock(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	/*
	 * EXYNOS5422 3D clock description
	 * normal usage: mux(vpll) -> divider -> mux_sw -> mux_user -> aclk_g3d
	 * on clock changing: mux(dpll) -> divider(3) -> mux_sw -> mux_user -> aclk_g3d
	 */

	platform->fout_vpll = clk_get(NULL, "fout_vpll");
	if (IS_ERR(platform->fout_vpll)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [fout_vpll]\n");
		return PTR_ERR(platform->fout_vpll);
	}

	platform->mout_vpll_ctrl = clk_get(kbdev->dev, "mout_vpll_ctrl"); /* same as sclk_vpll */
	if (IS_ERR(platform->mout_vpll_ctrl)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_vpll_ctrl]\n");
		return PTR_ERR(platform->mout_vpll_ctrl);
	}

	platform->mout_dpll_ctrl = clk_get(kbdev->dev, "mout_dpll_ctrl"); /* same as sclk_dpll */
	if (IS_ERR(platform->mout_dpll_ctrl)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_dpll_ctrl]\n");
		return PTR_ERR(platform->mout_dpll_ctrl);
	}

	platform->mout_aclk_g3d = clk_get(kbdev->dev, "mout_aclk_g3d"); /* set parents v or d pll */
	if (IS_ERR(platform->mout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_aclk_g3d]\n");
		return PTR_ERR(platform->mout_aclk_g3d);
	}

	platform->dout_aclk_g3d = clk_get(kbdev->dev, "dout_aclk_g3d"); /* divider usage */
	if (IS_ERR(platform->dout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [dout_aclk_g3d]\n");
		return PTR_ERR(platform->dout_aclk_g3d);
	}

	platform->mout_aclk_g3d_sw = clk_get(kbdev->dev, "mout_aclk_g3d_sw");
	if (IS_ERR(platform->mout_aclk_g3d_sw)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_aclk_g3d_sw]\n");
		return PTR_ERR(platform->mout_aclk_g3d_sw);
	}

	platform->mout_aclk_g3d_user = clk_get(kbdev->dev, "mout_aclk_g3d_user");
	if (IS_ERR(platform->mout_aclk_g3d_user)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [mout_aclk_g3d_user]\n");
		return PTR_ERR(platform->mout_aclk_g3d_user);
	}

	platform->clk_g3d_ip = clk_get(kbdev->dev, "clk_g3d_ip");
	clk_prepare_enable(platform->clk_g3d_ip);
	if (IS_ERR(platform->clk_g3d_ip)) {
		GPU_LOG(DVFS_ERROR, "failed to clk_get [clk_g3d_ip]\n");
		return PTR_ERR(platform->clk_g3d_ip);
	}

	return 0;
}

int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return ret;

	GPU_LOG(DVFS_INFO, "g3d clock initialized\n");

	return 0;
}

static int gpu_update_voltage(struct exynos_context *platform)
{
#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -ENODEV;
	}

	platform->cur_voltage = regulator_get_voltage(platform->g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return 0;
}

int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	static int _vol = -1;
	int err;

	if (_vol == vol)
		return 0;

#ifdef CONFIG_REGULATOR
	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -ENODEV;
	}

	err = regulator_set_voltage(platform->g3d_regulator, vol, vol);
	if (err) {
		GPU_LOG(DVFS_ERROR, "failed to set voltage, voltage: %d\n", vol);
		return err;
	}
#endif /* CONFIG_REGULATOR */

	_vol = vol;

	gpu_update_voltage(platform);
	KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_VOL_VALUE, NULL, NULL, 0u, vol);
	GPU_LOG(DVFS_DEBUG, "[G3D] voltage set:%d\n", vol);
	GPU_LOG(DVFS_DEBUG, "[G3D] voltage get:%d\n", platform->cur_voltage);

	return 0;
}

#ifdef CONFIG_REGULATOR
int gpu_regulator_enable(struct exynos_context *platform)
{
	int err;

	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -ENODEV;
	}

	err = regulator_enable(platform->g3d_regulator);
	if (err) {
		GPU_LOG(DVFS_ERROR, "failed to enable g3d regulator\n");
		return err;
	}
	return 0;
}

int gpu_regulator_disable(struct exynos_context *platform)
{
	int err;

	if (!platform->g3d_regulator) {
		GPU_LOG(DVFS_ERROR, "g3d_regulator is not initialized\n");
		return -ENODEV;
	}

	err = regulator_disable(platform->g3d_regulator);
	if (err) {
		GPU_LOG(DVFS_ERROR, "failed to disable g3d regulator\n");
		return err;
	}
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;
	int err;

	platform->g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(platform->g3d_regulator)) {
		err = PTR_ERR(platform->g3d_regulator);
		GPU_LOG(DVFS_ERROR, "failed to get g3d regulator, 0x%p\n",
				platform->g3d_regulator);
		platform->g3d_regulator = NULL;
		return err;
	}

	err = gpu_regulator_enable(platform);
	if (err) {
		GPU_LOG(DVFS_ERROR, "failed to enable g3d regulator\n");
		platform->g3d_regulator = NULL;
		return err;
	}

	gpu_voltage = get_match_volt(ID_G3D, MALI_DVFS_BL_CONFIG_FREQ*1000);
	if (gpu_voltage == 0)
		gpu_voltage = GPU_DEFAULT_VOLTAGE;

	err = gpu_set_voltage(platform, gpu_voltage);
	if (err) {
		GPU_LOG(DVFS_ERROR, "failed to set g3d regulator operating voltage [%d]\n",
				gpu_voltage);
		return err;
	}

	GPU_LOG(DVFS_INFO, "g3d regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */
