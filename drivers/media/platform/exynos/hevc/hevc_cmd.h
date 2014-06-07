/*
 * linux/drivers/media/video/exynos/hevc/hevc_cmd.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HEVC_CMD_H
#define __HEVC_CMD_H __FILE__

#define MAX_H2R_ARG		4

struct hevc_cmd_args {
	unsigned int	arg[MAX_H2R_ARG];
};

int hevc_cmd_host2risc(int cmd, struct hevc_cmd_args *args);
int hevc_sys_init_cmd(struct hevc_dev *dev);
int hevc_sleep_cmd(struct hevc_dev *dev);
int hevc_wakeup_cmd(struct hevc_dev *dev);
int hevc_open_inst_cmd(struct hevc_ctx *ctx);
int hevc_close_inst_cmd(struct hevc_ctx *ctx);

#endif /* __HEVC_CMD_H */
