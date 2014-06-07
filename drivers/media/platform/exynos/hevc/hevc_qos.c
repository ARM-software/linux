/*
 * linux/drivers/media/video/exynos/hevc/hevc_qos.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>

#include <plat/cpu.h>

#include "hevc_common.h"
#include "hevc_debug.h"
#include "hevc_pm.h"
#include "hevc_reg.h"

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
enum {
	HEVC_QOS_ADD,
	HEVC_QOS_UPDATE,
	HEVC_QOS_REMOVE,
};

static void hevc_qos_operate(struct hevc_ctx *ctx, int opr_type, int idx)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_platdata *pdata = dev->pdata;
	struct hevc_qos *qos_table = pdata->qos_table;

	switch (opr_type) {
	case HEVC_QOS_ADD:
		pm_qos_add_request(&dev->qos_req_int,
				PM_QOS_DEVICE_THROUGHPUT,
				qos_table[idx].freq_int);
		pm_qos_add_request(&dev->qos_req_mif,
				PM_QOS_BUS_THROUGHPUT,
				qos_table[idx].freq_mif);
		dev->curr_rate = qos_table[idx].freq_hevc;

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		pm_qos_add_request(&dev->qos_req_cpu,
				PM_QOS_CPU_FREQ_MIN,
				qos_table[idx].freq_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_add_request(&dev->qos_req_cpu,
				PM_QOS_CPU_FREQ_MIN,
				qos_table[idx].freq_cpu);
		pm_qos_add_request(&dev->qos_req_kfc,
				PM_QOS_KFC_FREQ_MIN,
				qos_table[idx].freq_kfc);
#endif
		atomic_set(&dev->qos_req_cur, idx + 1);
		hevc_debug(5, "QoS request: %d\n", idx + 1);
		break;
	case HEVC_QOS_UPDATE:
		pm_qos_update_request(&dev->qos_req_int,
				qos_table[idx].freq_int);
		pm_qos_update_request(&dev->qos_req_mif,
				qos_table[idx].freq_mif);
		dev->curr_rate = qos_table[idx].freq_hevc;

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		pm_qos_update_request(&dev->qos_req_cpu,
				qos_table[idx].freq_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_update_request(&dev->qos_req_cpu,
				qos_table[idx].freq_cpu);
		pm_qos_update_request(&dev->qos_req_kfc,
				qos_table[idx].freq_kfc);
#endif

		atomic_set(&dev->qos_req_cur, idx + 1);
		hevc_debug(5, "QoS update: %d\n", idx + 1);
		break;
	case HEVC_QOS_REMOVE:
		pm_qos_remove_request(&dev->qos_req_int);
		pm_qos_remove_request(&dev->qos_req_mif);
		dev->curr_rate = dev->min_rate;

#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
		pm_qos_remove_request(&dev->qos_req_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_remove_request(&dev->qos_req_cpu);
		pm_qos_remove_request(&dev->qos_req_kfc);
#endif
		atomic_set(&dev->qos_req_cur, 0);
		hevc_debug(5, "QoS remove\n");
		break;
	default:
		hevc_err("Unknown request for opr [%d]\n", opr_type);
		break;
	}
}

static void hevc_qos_add_or_update(struct hevc_ctx *ctx, int total_mb)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_platdata *pdata = dev->pdata;
	struct hevc_qos *qos_table = pdata->qos_table;
	int i;

	for (i = (pdata->num_qos_steps - 1); i >= 0; i--) {
		hevc_debug(7, "QoS index: %d\n", i + 1);
		if (total_mb > qos_table[i].thrd_mb) {
#ifdef CONFIG_ARM_EXYNOS_IKS_CPUFREQ
			hevc_debug(2, "\tint: %d, mif: %d, cpu: %d\n",
					qos_table[i].freq_int,
					qos_table[i].freq_mif,
					qos_table[i].freq_cpu);
#endif
#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
			hevc_debug(2, "\tint: %d, mif: %d, cpu: %d, kfc: %d\n",
					qos_table[i].freq_int,
					qos_table[i].freq_mif,
					qos_table[i].freq_cpu,
					qos_table[i].freq_kfc);
#endif
			if (atomic_read(&dev->qos_req_cur) == 0)
				hevc_qos_operate(ctx, HEVC_QOS_ADD, i);
			else if (atomic_read(&dev->qos_req_cur) != (i + 1))
				hevc_qos_operate(ctx, HEVC_QOS_UPDATE, i);

			break;
		}
	}
}

static inline int get_ctx_mb(struct hevc_ctx *ctx)
{
	int mb_width, mb_height, fps;

	mb_width = (ctx->img_width + 15) / 16;
	mb_height = (ctx->img_height + 15) / 16;
	fps = ctx->framerate / 1000;

	return mb_width * mb_height * fps;
}

void hevc_qos_on(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_ctx *qos_ctx;
	int found = 0, total_mb = 0;
	/* TODO: cpu lock is not separated yet */
	int need_cpulock = 0;

	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		total_mb += get_ctx_mb(qos_ctx);
		if (qos_ctx == ctx)
			found = 1;
	}

	if (!found) {
		list_add_tail(&ctx->qos_list, &dev->qos_queue);
		total_mb += get_ctx_mb(ctx);
	}

	/* TODO: need_cpulock will be used for cpu lock */
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (ctx->type == HEVCINST_DECODER)
			need_cpulock++;
	}

	hevc_qos_add_or_update(ctx, total_mb);
}

void hevc_qos_off(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_ctx *qos_ctx;
	int found = 0, total_mb = 0;

	if (list_empty(&dev->qos_queue)) {
		if (atomic_read(&dev->qos_req_cur) != 0) {
			hevc_err("HEVC request count is wrong!\n");
			hevc_qos_operate(ctx, HEVC_QOS_REMOVE, 0);
		}

		return;
	}

	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		total_mb += get_ctx_mb(qos_ctx);
		if (qos_ctx == ctx)
			found = 1;
	}

	if (found) {
		list_del(&ctx->qos_list);
		total_mb -= get_ctx_mb(ctx);
	}

	if (list_empty(&dev->qos_queue))
		hevc_qos_operate(ctx, HEVC_QOS_REMOVE, 0);
	else
		hevc_qos_add_or_update(ctx, total_mb);
}
#endif
