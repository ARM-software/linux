/*
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

#define SOC_NAME 5422

#if SOC_NAME == 5422
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
#define GPU_FREQ_KHZ_MAX    600000
#else
#define GPU_FREQ_KHZ_MAX    600000
#endif /* CONFIG_SOC_EXYNOS5422_REV_0 */
#define GPU_FREQ_KHZ_MIN    177000
#elif SOC_NAME == 5430
#define GPU_FREQ_KHZ_MAX    600000
#define GPU_FREQ_KHZ_MIN    160000
#elif SOC_NAME == 5260
#define GPU_FREQ_KHZ_MAX    480000
#define GPU_FREQ_KHZ_MIN    160000
#else
#error SOC_NAME should be specified.
#endif /* SOC_NAME */
