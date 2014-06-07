/* linux/drivers/media/video/exynos/fimg2d/fimg2d_clk.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __FIMG2D_CLK_H__
#define __FIMG2D_CLK_H__

#include <linux/pm_qos.h>

#include "fimg2d.h"

enum fimg2d_clocks {
	G2D_GATE_CLK1,
	G2D_GATE_CLK2,
	G2D_CHLD1_CLK,
	G2D_PARN1_CLK,
	G2D_CHLD2_CLK,
	G2D_PARN2_CLK,
};

extern void enable_hlt(void);
extern void disable_hlt(void);
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
extern struct pm_qos_request exynos5_g2d_cpu_qos;
#endif

int fimg2d_clk_setup(struct fimg2d_control *ctrl);
int fimg2d_clk_set_gate(struct fimg2d_control *ctrl);
int fimg2d_clk_get(struct fimg2d_control *ctrl, struct device *dev);
void fimg2d_clk_release(struct fimg2d_control *ctrl);
void fimg2d_clk_on(struct fimg2d_control *ctrl);
void fimg2d_clk_off(struct fimg2d_control *ctrl);
void fimg2d_clk_save(struct fimg2d_control *ctrl);
void fimg2d_clk_restore(struct fimg2d_control *ctrl);
#ifdef CONFIG_OF
int exynos5430_fimg2d_clk_setup(struct fimg2d_control *ctrl);
int exynos5430_fimg2d_clk_set(struct fimg2d_control *ctrl);
#endif


#endif /* __FIMG2D_CLK_H__ */
