/* linux/arch/arm/mach-exynos/include/mach/pmu.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_PMU_H
#define __ASM_ARCH_PMU_H __FILE__

#define PMU_TABLE_END	NULL
#define CLUSTER_NUM	2
#define CPUS_PER_CLUSTER	4

/* PMU(Power Management Unit) support */
enum sys_powerdown {
#ifdef CONFIG_SOC_EXYNOS5430
	SYS_AFTR,
	SYS_LPD,
	SYS_LPA,
	SYS_ALPA,
	SYS_DSTOP,
	SYS_DSTOP_PSR,
#else
	SYS_AFTR,
	SYS_LPA,
	SYS_DSTOP,
#endif
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};
#define PMU_TABLE_END	NULL

enum cpu_type {
	ARM,
	KFC,
	CPU_TYPE_MAX,
};

enum type_pmu_wdt_reset {
	/* if pmu_wdt_reset is EXYNOS_SYS_WDTRESET */
	PMU_WDT_RESET_TYPE0 = 0,
	/* if pmu_wdt_reset is EXYNOS5410_SYS_WDTRESET, EXYNOS5422_SYS_WDTRESET/ */
	PMU_WDT_RESET_TYPE1,
	/* if pmu_wdt_reset is EXYNOS5430_SYS_WDTRESET_EGL */
	PMU_WDT_RESET_TYPE2,
	/* if pmu_wdt_reset is EXYNOS5430_SYS_WDTRESET_KFC */
	PMU_WDT_RESET_TYPE3,
};

extern unsigned long l2x0_regs_phys;
struct exynos_pmu_conf {
	void __iomem *reg;
	unsigned int val[NUM_SYS_POWERDOWN];
};

/* cpu boot mode flag */
#define RESET			(1 << 0)
#define SECONDARY_RESET		(1 << 1)
#define HOTPLUG			(1 << 2)
#define C2_STATE		(1 << 3)
#define CORE_SWITCH		(1 << 4)
#define WAIT_FOR_OB_L2FLUSH	(1 << 5)

#define BOOT_MODE_MASK  0x1f

extern void set_boot_flag(unsigned int cpu, unsigned int mode);
extern void clear_boot_flag(unsigned int cpu, unsigned int mode);
extern void exynos_sys_powerdown_conf(enum sys_powerdown mode);
extern void exynos_xxti_sys_powerdown(bool enable);
extern void s3c_cpu_resume(void);
extern void exynos_set_core_flag(void);
extern void exynos_l2_common_pwr_ctrl(void);
extern void exynos_enable_idle_clock_down(unsigned int cluster);
extern void exynos_disable_idle_clock_down(unsigned int cluster);
extern void exynos_lpi_mask_ctrl(bool on);
extern void exynos_set_dummy_state(bool on);
extern void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type);
extern void exynos_cpu_sequencer_ctrl(bool enable);

#endif /* __ASM_ARCH_PMU_H */
