/*
 * linux/drivers/media/video/exynos/hevc/hevc_pm.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HEVC_PM_H
#define __HEVC_PM_H __FILE__

int hevc_init_pm(struct hevc_dev *dev);
void hevc_final_pm(struct hevc_dev *dev);

int hevc_clock_on(void);
void hevc_clock_off(void);
int hevc_power_on(void);
int hevc_power_off(void);
int hevc_get_clk_ref_cnt(void);
int hevc_set_clock_parent(struct hevc_dev *dev);

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
void hevc_qos_on(struct hevc_ctx *ctx);
void hevc_qos_off(struct hevc_ctx *ctx);
#else
#define hevc_qos_on(ctx)	do {} while (0)
#define hevc_qos_off(ctx)	do {} while (0)
#endif

#endif /* __HEVC_PM_H */
