/*
 * linux/drivers/media/video/exynos/hevc/hevc_dec.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HEVC_DEC_H_
#define __HEVC_DEC_H_ __FILE__

#define MAX_FRAME_SIZE		(2*1024*1024)
#define DEFAULT_TAG		(0xE05)
#define DEC_MAX_FPS		(60000)

const struct v4l2_ioctl_ops *hevc_get_dec_v4l2_ioctl_ops(void);
int hevc_init_dec_ctx(struct hevc_ctx *ctx);
int hevc_dec_ctx_ready(struct hevc_ctx *ctx);
void hevc_dec_store_crop_info(struct hevc_ctx *ctx);
int hevc_dec_cleanup_user_shared_handle(struct hevc_ctx *ctx);

#endif /* __HEVC_DEC_H_ */
