/* drivers/gpu/t6xx/kbase/src/platform/mali_kbase_platform.h
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
 * @file mali_kbase_platform.h
 * Platform-dependent init
 */

#ifndef _GPU_PLATFORM_H_
#define _GPU_PLATFORM_H_

#define GPU_LOG(level, msg, args...) \
do { \
	if (level >= gpu_get_debug_level()) { \
		printk(KERN_INFO msg, ## args); \
	} \
} while (0)

typedef enum {
	DVFS_DEBUG = 0,
	DVFS_INFO,
	DVFS_WARNING,
	DVFS_ERROR,
	DVFS_DEBUG_END,
} gpu_dvfs_debug_level;

typedef enum gpu_lock_type {
	TMU_LOCK = 0,
	SYSFS_LOCK,
#ifdef CONFIG_CPU_THERMAL_IPA
	IPA_LOCK,
#endif /* CONFIG_CPU_THERMAL_IPA */
	NUMBER_LOCK
} gpu_lock_type;

typedef struct _gpu_dvfs_info {
	unsigned int voltage;
	unsigned int clock;
	int min_threshold;
	int max_threshold;
	int stay_count;
	unsigned long long time;
	int mem_freq;
	int int_freq;
	int cpu_freq;
	int cpu_max_freq;
} gpu_dvfs_info;

struct exynos_context {
	/** Indicator if system clock to mail-t604 is active */
	int cmu_pmu_status;

#if SOC_NAME == 5422
	struct clk *fout_vpll;
	struct clk *mout_vpll_ctrl;
	struct clk *mout_dpll_ctrl;
	struct clk *mout_aclk_g3d;
	struct clk *dout_aclk_g3d;
	struct clk *mout_aclk_g3d_sw;
	struct clk *mout_aclk_g3d_user;
	struct clk *clk_g3d_ip;
#elif SOC_NAME == 5430
	struct clk *fin_pll;
	struct clk *fout_g3d_pll;
	struct clk *aclk_g3d;
	struct clk *mout_g3d_pll;
	struct clk *dout_aclk_g3d;
#elif SOC_NAME == 5260
	struct clk *fout_vpll;
	struct clk *ext_xtal;
	struct clk *aclk_g3d;
	struct clk *g3d;
#else
#error SOC_NAME should be specified.
#endif

	int clk_g3d_status;
	struct regulator *g3d_regulator;

	gpu_dvfs_info *table;
#ifdef CONFIG_DYNIMIC_ABB
	int *devfreq_g3d_asv_abb;
#endif
	int table_size;
	int step;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *exynos_pm_domain;
#endif /* CONFIG_PM_RUNTIME */
	struct mutex gpu_clock_lock;
	struct mutex gpu_dvfs_handler_lock;
	spinlock_t gpu_dvfs_spinlock;
#ifdef CONFIG_MALI_T6XX_DVFS
	int utilization;
#ifdef CONFIG_CPU_THERMAL_IPA
	int norm_utilisation;
	int freq_for_normalisation;
	unsigned long long power;
#endif /* CONFIG_CPU_THERMAL_IPA */
	int max_lock;
	int min_lock;
	int user_max_lock[NUMBER_LOCK];
	int user_min_lock[NUMBER_LOCK];
	int target_lock_type;
	int down_requirement;
	bool wakeup_lock;
	int governor_num;
	int governor_type;
	char governor_list[100];
	bool dvfs_status;
#ifdef CONFIG_CPU_THERMAL_IPA
	int time_tick;
	u32 time_busy;
	u32 time_idle;
#endif /* CONFIG_CPU_THERMAL_IPA */
#endif
	int cur_clock;
	int cur_voltage;
	int voltage_margin;
	bool tmu_status;
	int debug_level;
	int polling_speed;
	struct workqueue_struct *dvfs_wq;
};

#ifdef CONFIG_CPU_THERMAL_IPA
struct mali_utilisation_stats
{
	int utilisation;
	int norm_utilisation;
	int freq_for_norm;
};

struct mali_debug_utilisation_stats
{
	struct mali_utilisation_stats s;
	u32 time_busy;
	u32 time_idle;
	int time_tick;
};
#endif /* CONFIG_CPU_THERMAL_IPA */

void gpu_set_debug_level(int level);
int gpu_get_debug_level(void);

int kbase_platform_early_init(struct platform_device *pdev);

#endif /* _GPU_PLATFORM_H_ */
