/* linux/drivers/media/platform/exynos/jpeg4/jpeg_dev.h
  *
  * Copyright (c) 2013 Samsung Electronics Co., Ltd.
  * http://www.samsung.com/
  *
  * Header file for Samsung Jpeg v4.x Interface driver
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
 */

#ifndef __JPEG_DEV_H__
#define __JPEG_DEV_H__

#define JPEG_NAME		"fimp_v4"
#define JPEG_NODE_NAME		"video12"
#define EXYNOS_VIDEONODE_JPEG	12

#if defined(CONFIG_BUSFREQ_OPP)
#define BUSFREQ_400MHZ	400266
#endif

#endif /*__JPEG_DEV_H__*/
