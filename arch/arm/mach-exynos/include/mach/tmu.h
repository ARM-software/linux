/*
 * Copyright 2012 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * Header file for tmu support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_TMU_H
#define __ASM_ARCH_TMU_H

#include <linux/platform_data/exynos_thermal.h>

#define MUX_ADDR_VALUE 6
#define TMU_SAVE_NUM 10
#define TMU_DC_VALUE 25
#define UNUSED_THRESHOLD 0xFF

#define COLD_TEMP		19
#define HOT_NORMAL_TEMP		95
#define HOT_CRITICAL_TEMP	110
#define MIF_TH_TEMP1		85
#define MIF_TH_TEMP2		95

#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
#define GPU_TH_TEMP1		90
#define GPU_TH_TEMP2		95
#define GPU_TH_TEMP3		100
#define GPU_TH_TEMP4		105
#define GPU_TH_TEMP5		115
#elif defined(CONFIG_SOC_EXYNOS5422)
#define GPU_TH_TEMP1		80
#define GPU_TH_TEMP2		90
#define GPU_TH_TEMP3		95
#define GPU_TH_TEMP4		100
#define GPU_TH_TEMP5		110
#else
#define GPU_TH_TEMP1		85
#define GPU_TH_TEMP2		90
#define GPU_TH_TEMP3		95
#define GPU_TH_TEMP4		100
#define GPU_TH_TEMP5		110
#endif

enum tmu_status_t {
	TMU_STATUS_INIT = 0,
	TMU_STATUS_NORMAL,
	TMU_STATUS_THROTTLED,
	TMU_STATUS_TRIPPED,
};

enum mif_noti_state_t {
	MIF_TH_LV1 = 4,
	MIF_TH_LV2,
	MIF_TH_LV3,
};

enum tmu_noti_state_t {
	TMU_NORMAL,
	TMU_COLD,
	TMU_HOT,
	TMU_CRITICAL,
	TMU_95,
	TMU_109,
	TMU_110,
	TMU_111, // for detect thermal runaway caused by fimc
};

enum gpu_noti_state_t {
	GPU_NORMAL,
	GPU_COLD,
	GPU_THROTTLING1,
	GPU_THROTTLING2,
	GPU_THROTTLING3,
	GPU_THROTTLING4,
	GPU_TRIPPING,
};

#ifdef CONFIG_EXYNOS_THERMAL
extern int exynos_tmu_add_notifier(struct notifier_block *n);
extern int exynos_gpu_add_notifier(struct notifier_block *n);
#else
static inline int exynos_tmu_add_notifier(struct notifier_block *n)
{
	return 0;
}
#endif
#endif /* __ASM_ARCH_TMU_H */
