/*
 * linux/drivers/media/video/exynos/hevc/hevc_inst.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "hevc_common.h"
#include "hevc_cmd.h"
#include "hevc_debug.h"
#include "hevc_intr.h"

int hevc_open_inst(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	int ret;

	/* Preparing decoding - getting instance number */
	hevc_info("Getting instance number ctx->num = %d\n", ctx->num);
	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);
	ret = hevc_open_inst_cmd(ctx);
	if (ret) {
		hevc_err("Failed to create a new instance.\n");
		ctx->state = HEVCINST_ERROR;
	}
	return ret;
}

int hevc_close_inst(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	int ret;

	/* Closing decoding instance  */
	hevc_info("Returning instance number\n");
	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);
	ret = hevc_close_inst_cmd(ctx);
	if (ret) {
		hevc_err("Failed to return an instance.\n");
		ctx->state = HEVCINST_ERROR;
		return ret;
	}
	return ret;
}

