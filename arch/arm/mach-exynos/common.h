/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common Header for EXYNOS machines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_EXYNOS_COMMON_H
#define __ARCH_ARM_MACH_EXYNOS_COMMON_H

#include <linux/of.h>

void mct_init(void __iomem *base, int irq_g0, int irq_l0, int irq_l1);
void exynos_init_time(void);
extern unsigned long xxti_f, xusbxti_f;

struct map_desc;
void exynos_init_io(struct map_desc *mach_desc, int size);
void exynos4_init_irq(void);
void exynos5_init_irq(void);
void exynos4_restart(char mode, const char *cmd);
void exynos5_restart(char mode, const char *cmd);
void exynos_init_late(void);

/* ToDo: remove these after migrating legacy exynos4 platforms to dt */
void exynos4_clk_init(struct device_node *np, int is_exynos4210, void __iomem *reg_base, unsigned long xom);
void exynos4_clk_register_fixed_ext(unsigned long, unsigned long);

void exynos_firmware_init(void);

void exynos_set_timer_source(u8 channels);

#ifdef CONFIG_PM_GENERIC_DOMAINS
int exynos_pm_late_initcall(void);
#else
static inline int exynos_pm_late_initcall(void) { return 0; }
#endif

#ifdef CONFIG_ARCH_EXYNOS4
void exynos4_register_clocks(void);
void exynos4_setup_clocks(void);

#else
#define exynos4_register_clocks()
#define exynos4_setup_clocks()
#endif

#ifdef CONFIG_ARCH_EXYNOS5
void exynos5_register_clocks(void);
void exynos5_setup_clocks(void);

#else
#define exynos5_register_clocks()
#define exynos5_setup_clocks()
#endif

#ifdef CONFIG_CPU_EXYNOS4210
void exynos4210_register_clocks(void);

#else
#define exynos4210_register_clocks()
#endif

#ifdef CONFIG_SOC_EXYNOS4212
void exynos4212_register_clocks(void);

#else
#define exynos4212_register_clocks()
#endif

#ifdef CONFIG_SOC_EXYNOS5430
int exynos5430_pmu_init(void);
#else
#define exynos5430_pmu_init()
#endif

#ifdef CONFIG_SOC_EXYNOS5422
int exynos5422_pmu_init(void);
#else
#define exynos5422_pmu_init()
#endif

struct device_node;
void combiner_init(void __iomem *combiner_base, struct device_node *np,
			unsigned int max_nr, int irq_base);

extern struct smp_operations exynos_smp_ops;
extern void exynos_cpu_die(unsigned int cpu);

extern void set_boot_flag(unsigned int cpu, unsigned int mode);
extern void clear_boot_flag(unsigned int cpu, unsigned int mode);
extern void cci_snoop_disable(unsigned int sif);

struct exynos_cpu_power_ops {
	void (*power_up)(unsigned int cpu_id);
	unsigned int (*power_state)(unsigned int cpu_id);
	void (*power_down)(unsigned int cpu_id);
	void (*cluster_down)(unsigned int cluster);
	unsigned int (*cluster_state)(unsigned int cluster);
	bool (*is_last_core)(unsigned int cpu);
};

extern struct exynos_cpu_power_ops exynos_cpu;

#endif /* __ARCH_ARM_MACH_EXYNOS_COMMON_H */
