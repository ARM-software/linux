/*
 * drivers/media/video/exynos/hevc/hevc_intr.c
 *
 * C file for Samsung HEVC driver
 * This file contains functions used to wait for command completion.
 *
 * Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/io.h>

#include "hevc_common.h"

#include "hevc_intr.h"
#include "hevc_debug.h"
#include "hevc_pm.h"

#define wait_condition(x, c) (x->int_cond &&		\
		(R2H_BIT(x->int_type) & r2h_bits(c)))
#define is_err_cond(x)	((x->int_cond) && (x->int_type == HEVC_R2H_CMD_ERR_RET))
int hevc_wait_for_done_dev(struct hevc_dev *dev, int command)
{
	int ret;

	ret = wait_event_timeout(dev->queue,
			wait_condition(dev, command),
			msecs_to_jiffies(HEVC_INT_TIMEOUT));
	if (ret == 0) {
		hevc_err("Interrupt (dev->int_type:%d, command:%d) timed out.\n",
							dev->int_type, command);
		return 1;
	}
	hevc_debug(1, "Finished waiting (dev->int_type:%d, command: %d).\n",
							dev->int_type, command);
	return 0;
}

void hevc_clean_dev_int_flags(struct hevc_dev *dev)
{
	dev->int_cond = 0;
	dev->int_type = 0;
	dev->int_err = 0;
}

int hevc_wait_for_done_ctx(struct hevc_ctx *ctx, int command)
{
	int ret;

	ret = wait_event_timeout(ctx->queue,
			wait_condition(ctx, command),
			msecs_to_jiffies(HEVC_INT_TIMEOUT));
	if (ret == 0) {
		hevc_err("Interrupt (ctx->int_type:%d, command:%d) timed out.\n",
							ctx->int_type, command);
		return 1;
	} else if (ret > 0) {
		if (is_err_cond(ctx)) {
			hevc_err("Finished (ctx->int_type:%d, command: %d).\n",
					ctx->int_type, command);
			hevc_err("But error (ctx->int_err:%d).\n", ctx->int_err);
			return -1;
		}
	}
	hevc_debug(1, "Finished waiting (ctx->int_type:%d, command: %d).\n",
							ctx->int_type, command);
	return 0;
}

void hevc_clean_ctx_int_flags(struct hevc_ctx *ctx)
{
	ctx->int_cond = 0;
	ctx->int_type = 0;
	ctx->int_err = 0;
}

void hevc_cleanup_timeout(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;

	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	if (hevc_clear_hw_bit(ctx) > 0)
		hevc_clock_off();

	hevc_try_run(dev);
}
