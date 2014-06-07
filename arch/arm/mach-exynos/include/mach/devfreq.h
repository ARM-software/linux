/* linux/arch/arm/mach-exynos/include/mach/exynos-devfreq.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_DEVFREQ_H_
#define __EXYNOS_DEVFREQ_H_
enum devfreq_media_type {
	TYPE_FIMC_LITE,
	TYPE_MIXER,
#ifdef CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ
	TYPE_FIMD1,
	TYPE_UD_ENCODING,
	TYPE_UD_DECODING,
#else
	TYPE_DECON,
#endif
	TYPE_TV,
	TYPE_GSCL_LOCAL,
	TYPE_RESOLUTION,
};

enum devfreq_media_resolution {
	RESOLUTION_FULLHD,
	RESOLUTION_WQHD
};

enum devfreq_layer_count {
	NUM_LAYER_0,
	NUM_LAYER_1,
	NUM_LAYER_2,
	NUM_LAYER_3,
	NUM_LAYER_4,
	NUM_LAYER_5,
	NUM_LAYER_6,
};

#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value);
void exynos5_int_notify_power_status(const char* pd_name, unsigned int turn_on);
void exynos5_isp_notify_power_status(const char* pd_name, unsigned int turn_on);
void exynos5_disp_notify_power_status(const char* pd_name, unsigned int turn_on);
#endif

#ifdef CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ
enum devfreq_transition {
    MIF_DEVFREQ_PRECHANGE,
    MIF_DEVFREQ_POSTCHANGE,
    MIF_DEVFREQ_EN_MONITORING,
    MIF_DEVFREQ_DIS_MONITORING,
};

void exynos5_int_nocp_resume(void);
void exynos5_mif_transition_disable(bool disable);
void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value);
#endif /* CONFIG_ARM_EXYNOS5422_BUS_DEVFREQ */

#endif
