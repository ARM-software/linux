/* linux/arch/arm/mach-exynos/include/mach/exynos-hevc.h
 *
 * Copyright 2011 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * Header file for exynos hevc support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_HEVC_H
#define _EXYNOS_HEVC_H

#include <linux/platform_device.h>

#if	defined(CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5420_BUS_DEVFREQ) ||	\
	defined(CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ)
#define CONFIG_HEVC_USE_BUS_DEVFREQ
#endif

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
/*
 * thrd_mb - threshold of total MB(macroblock) count
 * Total MB count can be calculated by
 *	(MB of width) * (MB of height) * fps
 */
struct hevc_qos {
	unsigned int thrd_mb;
	unsigned int freq_hevc;
	unsigned int freq_int;
	unsigned int freq_mif;
	unsigned int freq_cpu;
#ifndef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
	unsigned int freq_kfc;
#endif
};
#endif

enum hevc_ip_version {
	IP_VER_HEVC_1 = 1,
};

struct hevc_platdata {
	int ip_ver;
	int clock_rate;
	int min_rate;
#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	int num_qos_steps;
	struct hevc_qos *qos_table;
#endif
};

void hevc_set_platdata(struct hevc_platdata *pd);
void hevc_setname(struct platform_device *pdev, char *name);

#endif /* _EXYNOS_HEVC_H */
