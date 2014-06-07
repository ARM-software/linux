/*
 * Copyright (C) 2012 Samsung Electronics.
 *
 * Copied from omap-smc.S Copyright (C) 2010 Texas Instruments, Inc.
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>

#include <mach/smc.h>

/*
 * Function signature: void exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
 */

ENTRY(exynos_smc)
	stmfd	sp!, {r4-r11, lr}
	dsb
	smc	#0
	ldmfd	sp!, {r4-r11, pc}
ENDPROC(exynos_smc)

ENTRY(exynos_smc_readsfr)
	stmfd	sp!, {r2-r11, lr}
	stmfd	sp!, {r1}

	lsr	r1, r0, #2
	orr	r1, r1, #SMC_REG_CLASS_SFR_R
	mov	r0, #SMC_CMD_REG
	dsb
	smc	#0
	mov	r3, #SMC_CMD_REG
	cmp	r0, r3
	moveq	r0, r1
	ldmfd	sp!, {r3}
	cmp	r0, #0
	streq	r2, [r3]
	ldmfd	sp!, {r2-r11, pc}
ENDPROC(exynos_smc_readsfr)
