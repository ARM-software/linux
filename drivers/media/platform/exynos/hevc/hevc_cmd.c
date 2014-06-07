/*
 * linux/drivers/media/video/exynos/hevc/hevc_cmd_v6.c
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

#include "hevc_debug.h"
#include "hevc_reg.h"
#include "hevc_cmd.h"
#include "hevc_mem.h"
#include "regs-hevc.h"

int hevc_cmd_host2risc(int cmd, struct hevc_cmd_args *args)
{
	if (cmd != HEVC_CH_NAL_START)
		hevc_info("Issue the command: %d\n", cmd);
	else
		hevc_debug(2, "Issue the command: %d\n", cmd);

	/* Reset RISC2HOST command */
	hevc_write_reg(0x0, HEVC_RISC2HOST_CMD);

	/* Issue the command */
	hevc_write_reg(cmd, HEVC_HOST2RISC_CMD);
	hevc_write_reg(0x1, HEVC_HOST2RISC_INT);

	return 0;
}

int hevc_sys_init_cmd(struct hevc_dev *dev)
{
	struct hevc_cmd_args h2r_args;
	struct hevc_buf_size_v6 *buf_size;
	int ret;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	buf_size = dev->variant->buf_size->buf;

	hevc_write_reg(dev->ctx_buf.ofs, HEVC_CONTEXT_MEM_ADDR);
	hevc_write_reg(buf_size->dev_ctx, HEVC_CONTEXT_MEM_SIZE);

	ret = hevc_cmd_host2risc(HEVC_H2R_CMD_SYS_INIT, &h2r_args);

	hevc_debug_leave();

	return ret;
}

int hevc_sleep_cmd(struct hevc_dev *dev)
{
	struct hevc_cmd_args h2r_args;
	int ret;

	hevc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct hevc_cmd_args));

	ret = hevc_cmd_host2risc(HEVC_H2R_CMD_SLEEP, &h2r_args);

	hevc_debug_leave();

	return ret;
}

int hevc_wakeup_cmd(struct hevc_dev *dev)
{
	struct hevc_cmd_args h2r_args;
	int ret;

	hevc_debug_enter();

	memset(&h2r_args, 0, sizeof(struct hevc_cmd_args));

	ret = hevc_cmd_host2risc(HEVC_H2R_CMD_WAKEUP, &h2r_args);

	hevc_debug_leave();

	return ret;
}

/* Open a new instance and get its number */
int hevc_open_inst_cmd(struct hevc_ctx *ctx)
{
	struct hevc_cmd_args h2r_args;
	struct hevc_dec *dec;
	int ret;

	hevc_debug_enter();

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	hevc_debug(2, "Requested codec mode: %d\n", ctx->codec_mode);

	hevc_write_reg(ctx->codec_mode, HEVC_CODEC_TYPE);
	hevc_write_reg(ctx->ctx.ofs, HEVC_CONTEXT_MEM_ADDR);
	hevc_write_reg(ctx->ctx_buf_size, HEVC_CONTEXT_MEM_SIZE);
	if (ctx->type == HEVCINST_DECODER)
		hevc_write_reg(dec->crc_enable, HEVC_D_CRC_CTRL);

	ret = hevc_cmd_host2risc(HEVC_H2R_CMD_OPEN_INSTANCE, &h2r_args);

	hevc_debug_leave();

	return ret;
}

/* Close instance */
int hevc_close_inst_cmd(struct hevc_ctx *ctx)
{
	struct hevc_cmd_args h2r_args;
	int ret = 0;

	hevc_debug_enter();

	if (ctx->state != HEVCINST_FREE) {
		hevc_write_reg(ctx->inst_no, HEVC_INSTANCE_ID);

		ret = hevc_cmd_host2risc(HEVC_H2R_CMD_CLOSE_INSTANCE,
					    &h2r_args);
	} else {
		ret = -EINVAL;
	}

	hevc_debug_leave();

	return ret;
}
