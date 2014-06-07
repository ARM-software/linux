/*
 * linux/drivers/media/video/exynos/hevc/hevc_ctrl.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HEVC_CTRL_H
#define __HEVC_CTRL_H __FILE__

int hevc_release_firmware(struct hevc_dev *dev);
int hevc_alloc_firmware(struct hevc_dev *dev);
int hevc_load_firmware(struct hevc_dev *dev);

int hevc_init_hw(struct hevc_dev *dev);
void hevc_deinit_hw(struct hevc_dev *dev);

int hevc_sleep(struct hevc_dev *dev);
int hevc_wakeup(struct hevc_dev *dev);

#endif /* __HEVC_CTRL_H */
