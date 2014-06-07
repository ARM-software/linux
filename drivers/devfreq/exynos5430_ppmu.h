/*
 * Copyright (C) 2013 Samsung Electronics
 *               http://www.samsung.com/
 *               Sangkyu Kim <skwith.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS5430_PPMU_H
#define __DEVFREQ_EXYNOS5430_PPMU_H __FILE__

#include <linux/notifier.h>

#include "exynos_ppmu2.h"

enum DEVFREQ_TYPE {
	MIF,
	INT,
	DEVFREQ_TYPE_COUNT,
};

enum DEVFREQ_PPMU {
	PPMU_D0_CPU,
	PPMU_D0_GEN,
	PPMU_D0_RT,
	PPMU_D1_CPU,
	PPMU_D1_GEN,
	PPMU_D1_RT,
	PPMU_COUNT,
};

enum DEVFREQ_PPMU_ADDR {
	PPMU_D0_CPU_ADDR = 0x10480000,
	PPMU_D0_GEN_ADDR = 0x10490000,
	PPMU_D0_RT_ADDR = 0x104A0000,
	PPMU_D1_CPU_ADDR = 0x104B0000,
	PPMU_D1_GEN_ADDR = 0x104C0000,
	PPMU_D1_RT_ADDR = 0x104D0000,
};

struct devfreq_exynos {
	struct list_head node;
	struct ppmu_info *ppmu_list;
	unsigned int ppmu_count;
	unsigned long val_ccnt;
	unsigned long val_pmcnt;
};

int exynos5430_devfreq_init(struct devfreq_exynos *de);
int exynos5430_devfreq_register(struct devfreq_exynos *de);
int exynos5430_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);
int exynos5430_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb);

#endif /* __DEVFREQ_EXYNOS5430_PPMU_H */
