/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_cmd_v6.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "s5p_mfc_common.h"

#include "s5p_mfc_debug.h"
#include "s5p_mfc_reg.h"
#include "s5p_mfc_cmd.h"
#include "s5p_mfc_mem.h"

int s5p_mfc_cmd_host2risc(struct s5p_mfc_dev *dev, int cmd,
				struct s5p_mfc_cmd_args *args)
{
	mfc_debug(2, "Issue the command: %d\n", cmd);

	/* Reset RISC2HOST command */
	s5p_mfc_write_reg(dev, 0x0, S5P_FIMV_RISC2HOST_CMD);

	/* Issue the command */
	s5p_mfc_write_reg(dev, cmd, S5P_FIMV_HOST2RISC_CMD);
	s5p_mfc_write_reg(dev, 0x1, S5P_FIMV_HOST2RISC_INT);

	return 0;
}

int s5p_mfc_sys_init_cmd(struct s5p_mfc_dev *dev,
					enum mfc_buf_usage_type buf_type)
{
	struct s5p_mfc_buf_size_v6 *buf_size;
	struct s5p_mfc_extra_buf *ctx_buf, *dis_shm_buf;
	int ret;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	buf_size = dev->variant->buf_size->buf;
	ctx_buf = &dev->ctx_buf;
	dis_shm_buf = &dev->dis_shm_buf;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (buf_type == MFCBUF_DRM) {
		ctx_buf = &dev->ctx_buf_drm;
		dis_shm_buf = &dev->dis_shm_buf_drm;
	}
#endif
	s5p_mfc_write_reg(dev, ctx_buf->ofs, S5P_FIMV_CONTEXT_MEM_ADDR);
	s5p_mfc_write_reg(dev, buf_size->dev_ctx, S5P_FIMV_CONTEXT_MEM_SIZE);
	if (IS_MFCv7X(dev)) {
		s5p_mfc_write_reg(dev, dis_shm_buf->ofs,
					S5P_FIMV_DIS_SHARED_MEM_ADDR);
		mfc_debug(2, "Setting shared memory = 0x%x\n",
					(unsigned int)dis_shm_buf->ofs);
	}

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_SYS_INIT, NULL);

	mfc_debug_leave();

	return ret;
}

int s5p_mfc_sleep_cmd(struct s5p_mfc_dev *dev)
{
	int ret;

	mfc_debug_enter();

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_SLEEP, NULL);

	mfc_debug_leave();

	return ret;
}

int s5p_mfc_wakeup_cmd(struct s5p_mfc_dev *dev)
{
	int ret;

	mfc_debug_enter();

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_WAKEUP, NULL);

	mfc_debug_leave();

	return ret;
}

/* Open a new instance and get its number */
int s5p_mfc_open_inst_cmd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	int ret;

	mfc_debug_enter();

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	mfc_debug(2, "Requested codec mode: %d\n", ctx->codec_mode);

	s5p_mfc_write_reg(dev, ctx->codec_mode, S5P_FIMV_CODEC_TYPE);
	s5p_mfc_write_reg(dev, ctx->ctx.ofs, S5P_FIMV_CONTEXT_MEM_ADDR);
	s5p_mfc_write_reg(dev, ctx->ctx_buf_size, S5P_FIMV_CONTEXT_MEM_SIZE);
	if (ctx->type == MFCINST_DECODER)
		s5p_mfc_write_reg(dev, ctx->dec_priv->crc_enable,
							S5P_FIMV_D_CRC_CTRL);

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_OPEN_INSTANCE, NULL);

	mfc_debug_leave();

	return ret;
}

/* Close instance */
int s5p_mfc_close_inst_cmd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret;

	mfc_debug_enter();

	s5p_mfc_write_reg(dev, ctx->inst_no, S5P_FIMV_INSTANCE_ID);

	ret = s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_CLOSE_INSTANCE, NULL);

	mfc_debug_leave();

	return ret;
}
