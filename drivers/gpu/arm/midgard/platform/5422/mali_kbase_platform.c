/*
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
#include "gpu_ipa.h"

#ifndef CONFIG_OF
static struct kbase_io_resources io_resources = {
	.job_irq_number   = JOB_IRQ_NUMBER,
	.mmu_irq_number   = MMU_IRQ_NUMBER,
	.gpu_irq_number   = GPU_IRQ_NUMBER,
	.io_memory_region = {
		.start = EXYNOS5_PA_G3D,
		.end   = EXYNOS5_PA_G3D + (4096 * 4) - 1
	}
};
#endif

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
static int kbase_platform_exynos5_init(struct kbase_device *kbdev)
{
	int err;

#ifdef CONFIG_MALI_MIDGARD_DVFS
	unsigned long flags;
#endif /* CONFIG_MALI_MIDGARD_DVFS */
	struct exynos_context *platform;

	platform = kmalloc(sizeof(struct exynos_context), GFP_KERNEL);

	if (!platform)
		return -ENOMEM;

	memset(platform, 0, sizeof(struct exynos_context));

	kbdev->platform_context = (void *) platform;

	platform->cmu_pmu_status = 0;
	platform->dvfs_wq = NULL;
	platform->polling_speed = 100;
	gpu_debug_level = DVFS_WARNING;

	mutex_init(&platform->gpu_clock_lock);
	mutex_init(&platform->gpu_dvfs_handler_lock);
	spin_lock_init(&platform->gpu_dvfs_spinlock);

	err = gpu_control_module_init(kbdev);
	if (err)
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

	err = gpu_notifier_init(kbdev);
	if (err)
		goto notifier_init_fail;

	err = gpu_create_sysfs_file(kbdev->dev);
	if (err)
		goto sysfs_init_fail;

	return 0;

clock_init_fail:
notifier_init_fail:
sysfs_init_fail:
	kfree(platform);

	return err;
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

	gpu_remove_sysfs_file(kbdev->dev);
}

struct kbase_platform_funcs_conf platform_funcs = {
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

extern struct kbase_pm_callback_conf pm_callbacks;
extern int get_cpu_clock_speed(u32 *cpu_clock);

struct kbase_platform_config platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &platform_config;
}

int kbase_platform_early_init(void)
{
	/* Nothing to do */
	return 0;
}

#ifdef CONFIG_MALI_MIDGARD_DVFS

#define POWER_COEFF_GPU_ALL_CORE_ON 64  /* all core on param */

/* This is a temporary value. To get STATIC_POWER_COEFF you need to do
 * a "one off" calculation - i.e. you attach the necessary probes to a board,
 * get the board into "GPU on but idle", switch to the always_on policy and
 * measure the power at different frequency points
 * STATIC_POWER_COEFF = div_u64(power * 100000,(u64)freq * voltage * voltage)
 */
#define STATIC_POWER_COEFF 3
#define DYNAMIC_POWER_COEFF (POWER_COEFF_GPU_ALL_CORE_ON - STATIC_POWER_COEFF)

extern struct kbase_device *pkbdev;

/*  Get the current utilization of the gpu */
int get_gpu_util(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	return platform->utilization;
}

/* Get power consumption of GPU assuming utilization */
int get_gpu_power(int util)
{
	unsigned int freq;
	unsigned int vol;
	int power = 0;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	/* Get the frequency and voltage for the level */
	freq = platform->table[platform->step].clock;
	vol = platform->table[platform->step].voltage / 10000;

	/* Calculate power */
	power = (int)div_u64(
			((u64)STATIC_POWER_COEFF * freq * vol * vol) +
			((u64)DYNAMIC_POWER_COEFF * util * freq * vol * vol),
			100000);

	return power;
}

/* Limit the frequency of the GPU to match the power assuming utilization */
int set_gpu_power_cap(int gpu_power, int util)
{
	int level = 1;
	unsigned int freq;
	unsigned int vol;
	int required_power;

	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	BUG_ON(platform->table_size == 0);

	while (level < platform->table_size) {
		vol = platform->table[level].voltage / 10000;
		freq = platform->table[level].clock;
		required_power = (int)div_u64(
				((u64)STATIC_POWER_COEFF * freq * vol * vol) +
				((u64)DYNAMIC_POWER_COEFF * util * freq * vol * vol),
				100000);

		if (required_power > gpu_power)
			break;
		level++;
	}
	level--;

	gpu_ipa_dvfs_max_lock(platform->table[level].clock);

	return platform->table[level].clock;
}

#endif /* CONFIG_MALI_MIDGARD_DVFS */




