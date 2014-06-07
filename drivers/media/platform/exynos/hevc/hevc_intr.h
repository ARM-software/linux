/*
 * drivers/media/video/exynos/hevc/hevc_intr.h
 *
 * Header file for Samsung HEVC driver
 * It contains waiting functions declarations.
 *
 * Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HEVC_INTR_H_
#define _HEVC_INTR_H_

#include "hevc_common.h"

int hevc_wait_for_done_ctx(struct hevc_ctx *ctx, int command);
int hevc_wait_for_done_dev(struct hevc_dev *dev, int command);
void hevc_clean_ctx_int_flags(struct hevc_ctx *ctx);
void hevc_clean_dev_int_flags(struct hevc_dev *dev);
void hevc_cleanup_timeout(struct hevc_ctx *ctx);

#endif /* _HEVC_INTR_H_ */
