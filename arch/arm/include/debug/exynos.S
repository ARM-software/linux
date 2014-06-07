/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <mach/map.h>

#define PART_NUMBER_SHIFT	(4)
#define PART_NUMBER_MASK	(0xff)
#define PART_NUMBER_A7		(0x07)
#define PART_NUMBER_A9		(0x09)
#define PART_NUMBER_A15		(0x0f)

	.macro addruart, rp, rv, tmp
#ifdef CONFIG_SOC_EXYNOS5430
		ldr	\rp, =EXYNOS5430_PA_UART
#else
		mrc	p15, 0, \tmp, c0, c0, 0
		and	\tmp, \tmp, #(PART_NUMBER_MASK << PART_NUMBER_SHIFT)
		teq	\tmp, #(PART_NUMBER_A9 << PART_NUMBER_SHIFT)
		ldreq	\rp, =EXYNOS4_PA_UART	@@ EXYNOS4
		ldrne	\rp, =EXYNOS5_PA_UART	@@ EXYNOS5
#endif
		ldr	\rv, =S3C_VA_UART
#if CONFIG_DEBUG_S3C_UART != 0
		add	\rp, \rp, #(0x10000 * CONFIG_DEBUG_S3C_UART)
		add	\rv, \rv, #(0x10000 * CONFIG_DEBUG_S3C_UART)
#endif
	.endm

#define fifo_full fifo_full_s5pv210
#define fifo_level fifo_level_s5pv210

#include <debug/samsung.S>
