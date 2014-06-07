/*
 * drivers/media/video/exynos/hevc/hevc_debug.h
 *
 * Header file for Samsung HEVC driver
 * This file contains debug macros
 *
 * Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef HEVC_DEBUG_H_
#define HEVC_DEBUG_H_

#define DEBUG

#ifdef DEBUG
extern int debug_hevc;
#define hevc_debug(level, fmt, args...)				\
	do {							\
		if (debug_hevc >= level)				\
			printk(KERN_DEBUG "%s:%d: " fmt,	\
				__func__, __LINE__, ##args);	\
	} while (0)
#else
#define hevc_debug(fmt, args...)
#endif

#define hevc_debug_enter() hevc_debug(5, "enter")
#define hevc_debug_leave() hevc_debug(5, "leave")

#define hevc_err(fmt, args...)				\
	do {						\
		printk(KERN_ERR "%s:%d: " fmt,		\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define hevc_info(fmt, args...)				\
	do {						\
		printk(KERN_INFO "%s:%d: " fmt,		\
			__func__, __LINE__, ##args);	\
	} while (0)

#endif /* HEVC_DEBUG_H_ */
