/* drivers/gpu/t6xx/kbase/src/platform/mali_kbase_platform.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 platform-dependent codes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_platform.c
 * Platform-dependent init.
 */

#include <mali_kbase.h>

#include <mach/regs-pmu.h>
#include <plat/devs.h>

#include "mali_kbase_platform.h"
#include "gpu_custom_interface.h"
#include "gpu_dvfs_handler.h"
#include "gpu_notifier.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"

static int gpu_debug_level;

void gpu_set_debug_level(int level)
{
	gpu_debug_level = level;
}

int gpu_get_debug_level(void)
{
	return gpu_debug_level;
}

/**
 ** Exynos5 hardware specific initialization
 **/
int kbase_platform_exynos5_init(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_MIDGARD_DVFS
	unsigned long flags;
#endif /* CONFIG_MALI_MIDGARD_DVFS */
	struct exynos_context *platform;
	int stat;

	platform = kmalloc(sizeof(struct exynos_context), GFP_KERNEL);
	if (!platform)
		return -ENOMEM;

	memset(platform, 0, sizeof(struct exynos_context));

	kbdev->platform_context = platform;

	platform->cmu_pmu_status = 0;
	platform->dvfs_wq = NULL;
	platform->polling_speed = 100;
	gpu_debug_level = DVFS_WARNING;

	mutex_init(&platform->gpu_clock_lock);
	mutex_init(&platform->gpu_dvfs_handler_lock);
	spin_lock_init(&platform->gpu_dvfs_spinlock);

	/* gpu control module init*/
	stat = gpu_control_module_init(kbdev);
	if (stat)
		goto clock_init_fail;

	/* dvfs gobernor init*/
	gpu_dvfs_governor_init(kbdev, G3D_DVFS_GOVERNOR_DEFAULT);
#ifdef CONFIG_MALI_MIDGARD_DVFS
	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->wakeup_lock = 0;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
#endif /* CONFIG_MALI_MIDGARD_DVFS */
	/* dvfs handler init*/
	gpu_dvfs_handler_init(kbdev);

	stat = gpu_notifier_init(kbdev);
	if (stat)
		goto notifier_init_fail;

#ifdef CONFIG_MALI_MIDGARD_DEBUG_SYS
	stat = gpu_create_sysfs_file(kbdev->dev);
	if (stat)
		goto sysfs_init_fail;
#endif /* CONFIG_MALI_MIDGARD_DEBUG_SYS */

	return 0;

clock_init_fail:
notifier_init_fail:
#ifdef CONFIG_MALI_MIDGARD_DEBUG_SYS
sysfs_init_fail:
#endif /* CONFIG_MALI_MIDGARD_DEBUG_SYS */
	kfree(platform);

	return stat;
}

/**
 ** Exynos5 hardware specific termination
 **/
static void kbase_platform_exynos5_term(struct kbase_device *kbdev)
{
	struct exynos_context *platform;
	platform = (struct exynos_context *) kbdev->platform_context;

	gpu_notifier_term();

#ifdef CONFIG_MALI_MIDGARD_DVFS
	gpu_dvfs_handler_deinit(kbdev);
#endif /* CONFIG_MALI_MIDGARD_DVFS */

	gpu_control_module_term(kbdev);

	kfree(kbdev->platform_context);
	kbdev->platform_context = 0;

#ifdef CONFIG_MALI_MIDGARD_DEBUG_SYS
	gpu_remove_sysfs_file(kbdev->dev);
#endif /* CONFIG_MALI_MIDGARD_DEBUG_SYS */
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

struct kbase_platform_config e5422_platform_config;

struct kbase_platform_config *kbase_get_platform_config(void) {
	return &e5422_platform_config;
}

extern struct kbase_pm_callback_conf pm_callbacks;
extern int get_cpu_clock_speed(u32 *cpu_clock);
