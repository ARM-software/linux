/* linux/arch/arm/mach-exynos/include/mach/cpufreq.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPUFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/notifier.h>

enum cpufreq_level_index {
	L0, L1, L2, L3, L4,
	L5, L6, L7, L8, L9,
	L10, L11, L12, L13, L14,
	L15, L16, L17, L18, L19,
	L20, L21, L22, L23, L24,
};

#define APLL_FREQ(f, a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, m, p, s) \
	{ \
		.freq = (f) * 1000, \
		.clk_div_cpu0 = ((a0) | (a1) << 4 | (a2) << 8 | (a3) << 12 | \
			(a4) << 16 | (a5) << 20 | (a6) << 24 | (a7) << 28), \
		.clk_div_cpu1 = (b0 << 0 | b1 << 4 | b2 << 8), \
		.mps = ((m) << 16 | (p) << 8 | (s)), \
	}

struct apll_freq {
	unsigned int freq;
	u32 clk_div_cpu0;
	u32 clk_div_cpu1;
	u32 mps;
};

struct exynos_dvfs_info {
	unsigned long	mpll_freq_khz;
	unsigned int	pll_safe_idx;
	unsigned int	max_support_idx;
	unsigned int	min_support_idx;
	unsigned int	cluster_num;
	unsigned int	boot_freq;
	unsigned int	boot_cpu_min_qos;
	unsigned int	boot_cpu_max_qos;
	int		boot_freq_idx;
	int		*bus_table;
	bool		blocked;
	struct clk	*cpu_clk;
	unsigned int	*volt_table;
        unsigned int    *abb_table;
	const unsigned int	*max_op_freqs;
	struct cpufreq_frequency_table	*freq_table;
	struct regulator *regulator;
	void (*set_freq)(unsigned int, unsigned int);
	void (*set_ema)(unsigned int);
	bool (*need_apll_change)(unsigned int, unsigned int);
	bool (*is_alive)(void);
	void (*set_int_skew)(int);
};

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv0;
	unsigned int	clkdiv1;
};

#if defined(CONFIG_SOC_EXYNOS5422)
#define EMA_VAL_0 0x4
#define EMA_VAL_1 0x3
#define EMA_VAL_2 0x1
#define EMA_VAL_3 0x1
#define EMA_VOLT_LEV_0 900000
#define EMA_VOLT_LEV_1 950000
#define EMA_VOLT_LEV_2 1045000
#define EMA_VOLT_LEV_3 1155000
#define EMA_ON_CHANGE 0x11
#endif

#if defined(CONFIG_ARCH_EXYNOS4)
extern int exynos4210_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos4x12_cpufreq_init(struct exynos_dvfs_info *);
static inline int exynos5250_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5410_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos5410_cpufreq_CA15_init(struct exynos_dvfs_info *info)
{
	return 0;
}

#elif defined(CONFIG_ARCH_EXYNOS5)
static inline int exynos4210_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

static inline int exynos4x12_cpufreq_init(struct exynos_dvfs_info *info)
{
	return 0;
}

extern int exynos5250_cpufreq_init(struct exynos_dvfs_info *);
extern int exynos5_cpufreq_CA7_init(struct exynos_dvfs_info *);
extern int exynos5_cpufreq_CA15_init(struct exynos_dvfs_info *);
#else
	#warning "Should define CONFIG_ARCH_EXYNOS4(5)\n"
#endif
extern void exynos_thermal_throttle(void);
extern void exynos_thermal_unthrottle(void);

/* CPUFREQ init events */
#define CPUFREQ_INIT_COMPLETE	0x0001

#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
extern int exynos_cpufreq_init_register_notifier(struct notifier_block *nb);
extern int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb);
#else
static inline int exynos_cpufreq_init_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif

#if defined(CONFIG_ARCH_EXYNOS5)
typedef enum {
	CA7,
	CA15,
	CA_END,
} cluster_type;

#if defined(CONFIG_ARM_EXYNOS5430_CPUFREQ) || defined(CONFIG_ARM_EXYNOS5422_CPUFREQ)
#define COLD_VOLT_OFFSET	37500
#define ENABLE_MIN_COLD		1
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000
#define NR_CA7			4
#define NR_CA15			4
#endif

enum op_state {
	NORMAL,		/* Operation : Normal */
	SUSPEND,	/* Direct API will be blocked in this state */
	RESUME,		/* Re-enabling DVFS using direct API after resume */
};

/*
 * Keep frequency value for counterpart cluster DVFS
 * cur, min, max : Frequency (KHz),
 * c_id : Counter cluster with booting cluster, if booting cluster is
 * A15, c_id will be A7.
 */
struct cpu_info_alter {
	unsigned int cur;
	unsigned int min;
	unsigned int max;
	cluster_type boot_cluster;
	cluster_type c_id;
};

extern cluster_type exynos_boot_cluster;
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
extern void (*disable_c3_idle)(bool disable);
#endif
#ifdef CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG
extern void force_dynamic_hotplug(bool out_flag);
#endif
#if defined(CONFIG_SCHED_HMP) && defined(CONFIG_EXYNOS5_DYNAMIC_CPU_HOTPLUG)
int big_cores_hotplug(bool out_flag);
void event_hotplug_in(void);
#else
static inline int big_cores_hotplug(bool out_flag)
{
	return 0;
}

static inline void event_hotplug_in(void)
{
	return;
}
#endif

/*
 * interface for IPA
 */
#if defined(CONFIG_ARM_EXYNOS_MP_CPUFREQ) || defined(CONFIG_ARM_EXYNOS_CPUFREQ)
void exynos_set_max_freq(int max_freq, unsigned int cpu);
void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target);
#else
void exynos_set_max_freq(int max_freq, unsigned int cpu) {}
void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target) {}
#endif
#endif
