/*
 * linux/drivers/media/video/exynos/hevc/hevc_inst.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HEVC_INST_H_
#define __HEVC_INST_H_ __FILE__

int hevc_open_inst(struct hevc_ctx *ctx);
int hevc_close_inst(struct hevc_ctx *ctx);

#endif /* __HEVC_INST_H_  */
