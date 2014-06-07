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

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static kbase_io_resources io_resources = {
	.job_irq_number   = JOB_IRQ_NUMBER,
	.mmu_irq_number   = MMU_IRQ_NUMBER,
	.gpu_irq_number   = GPU_IRQ_NUMBER,
	.io_memory_region = {
		.start = EXYNOS5_PA_G3D,
		.end   = EXYNOS5_PA_G3D + (4096 * 5) - 1
	}
};
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0) */

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
static mali_bool kbase_platform_exynos5_init(kbase_device *kbdev)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	unsigned long flags;
#endif /* CONFIG_MALI_T6XX_DVFS */
	struct exynos_context *platform;

	platform = kmalloc(sizeof(struct exynos_context), GFP_KERNEL);

	if (NULL == platform)
		return MALI_FALSE;

	memset(platform, 0, sizeof(struct exynos_context));

	kbdev->platform_context = (void *) platform;

	platform->cmu_pmu_status = 0;
	platform->dvfs_wq = NULL;
	platform->polling_speed = 100;
	gpu_debug_level = DVFS_WARNING;

	mutex_init(&platform->gpu_clock_lock);
	mutex_init(&platform->gpu_dvfs_handler_lock);
	spin_lock_init(&platform->gpu_dvfs_spinlock);

	/* gpu control module init*/
	if (gpu_control_module_init(kbdev))
		goto clock_init_fail;

	/* dvfs gobernor init*/
	gpu_dvfs_governor_init(kbdev, G3D_DVFS_GOVERNOR_DEFAULT);
#ifdef CONFIG_MALI_T6XX_DVFS
	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	platform->wakeup_lock = 0;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
#endif /* CONFIG_MALI_T6XX_DVFS */
	/* dvfs handler init*/
	gpu_dvfs_handler_init(kbdev);

	if (!gpu_notifier_init(kbdev))
		goto notifier_init_fail;

#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
	if (gpu_create_sysfs_file(kbdev->osdev.dev))
		goto sysfs_init_fail;
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */

	return MALI_TRUE;

clock_init_fail:
notifier_init_fail:
#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
sysfs_init_fail:
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */
	kfree(platform);

	return MALI_FALSE;
}

/**
 ** Exynos5 hardware specific termination
 **/
static void kbase_platform_exynos5_term(kbase_device *kbdev)
{
	struct exynos_context *platform;
	platform = (struct exynos_context *) kbdev->platform_context;

	gpu_notifier_term();

#ifdef CONFIG_MALI_T6XX_DVFS
	gpu_dvfs_handler_deinit(kbdev);
#endif /* CONFIG_MALI_T6XX_DVFS */

	gpu_control_module_term(kbdev);

	kfree(kbdev->platform_context);
	kbdev->platform_context = 0;

#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
	gpu_remove_sysfs_file(kbdev->osdev.dev);
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */
}

kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

extern kbase_pm_callback_conf pm_callbacks;
extern int get_cpu_clock_speed(u32 *cpu_clock);

static kbase_attribute config_attributes[] = {
	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
		2048 * 1024 * 1024UL /* 2048MB */
	},
	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
		KBASE_MEM_PERF_FAST
	},
#ifdef CONFIG_MALI_T6XX_RT_PM
	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
		(uintptr_t)&pm_callbacks
	},
#endif /* CONFIG_MALI_T6XX_RT_PM */
	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_DVFS_FREQ,
		100
	}, /* 100ms */
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&platform_funcs
	},
	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
		G3D_MAX_FREQ
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
		G3D_MIN_FREQ
	},
	{
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		500 /* 500ms before cancelling stuck jobs */
	},
	{
		KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
		(uintptr_t)&get_cpu_clock_speed
	},
	{
		KBASE_CONFIG_ATTR_END,
		0
	}
};

kbase_platform_config platform_config = {
		.attributes                = config_attributes,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		.io_resources              = &io_resources,
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0) */
		.midgard_type              = KBASE_MALI_T604
};

int kbase_platform_early_init(struct platform_device *pdev)
{
	kbase_platform_config *config;
	int attribute_count;

	config = &platform_config;
	attribute_count = kbasep_get_config_attribute_count(config->attributes);

	return platform_device_add_data(
#ifndef CONFIG_MALI_PLATFORM_FAKE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
		pdev,
#else
		&exynos5_device_g3d,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0) */
#endif /* CONFIG_MALI_PLATFORM_FAKE */
		config->attributes,
		attribute_count * sizeof(config->attributes[0]));
}
