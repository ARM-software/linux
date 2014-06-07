/* linux/drivers/media/platform/exynos/jpeg4/jpeg_core.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Definition for core file of the jpeg operation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __JPEG_CORE_H__
#define __JPEG_CORE_H__

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-ioctl.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-ion.h>
#include "jpeg_mem.h"

#define INT_TIMEOUT		1000

#define JPEG_NUM_INST		4
#define JPEG_MAX_PLANE		3
#define is_ver_5a (pdata->ip_ver == IP_VER_JPEG_5A)
#define is_ver_5h (pdata->ip_ver == IP_VER_JPEG_5H)

#define JPEG_TIMEOUT		((200 * HZ) / 1000)	/* 200 ms */

/* JPEG hardware device state */
#define DEV_RUN		1
#define DEV_SUSPEND	2

/* JPEG m2m context state */
#define CTX_PARAMS	1
#define CTX_STREAMING	2
#define CTX_RUN		3
#define CTX_ABORT	4
#define CTX_SRC_FMT	5
#define CTX_DST_FMT	6
#define CTX_INT_FRAME	7 /* intermediate frame available */

/*
 * struct exynos_platform_jpeg
 */

enum jpeg_clk_status {
	JPEG_CLK_ON,
	JPEG_CLK_OFF,
};

enum jpeg_clocks {
	JPEG_GATE_CLK,
	JPEG_CHLD1_CLK,
	JPEG_PARN1_CLK,
	JPEG_CHLD2_CLK,
	JPEG_PARN2_CLK,
	JPEG_CHLD3_CLK,
	JPEG_PARN3_CLK,
};

enum jpeg_ip_version {
	IP_VER_JPEG_5G,
	IP_VER_JPEG_5A,
	IP_VER_JPEG_5H,
};

struct exynos_platform_jpeg {
	u32 ip_ver;
	u32 mif_min;
	u32 int_min;
};

enum jpeg_state {
	JPEG_IDLE,
	JPEG_SRC_ADDR,
	JPEG_DST_ADDR,
	JPEG_ISR,
	JPEG_STREAM,
};

enum jpeg_mode {
	ENCODING,
	DECODING,
};

enum jpeg_result {
	OK_ENC_OR_DEC,
	ERR_PROT,
	ERR_DEC_INVALID_FORMAT,
	ERR_MULTI_SCAN,
	ERR_FRAME,
	ERR_TIME_OUT,
	ERR_UNKNOWN,
};

enum  jpeg_img_quality_level {
	QUALITY_LEVEL_1 = 0,	/* high */
	QUALITY_LEVEL_2,
	QUALITY_LEVEL_3,
	QUALITY_LEVEL_4,
	QUALITY_LEVEL_5,
	QUALITY_LEVEL_6,	/* low */
};

enum jpeg_frame_format {
/* raw data image format */
	YCRCB_444_2P,
	YCBCR_444_2P,
	YCBCR_444_3P,
	YCRCB_444_3P,
	YCBCR_422_1P,
	YCRCB_422_1P,
	CBCRY_422_1P,
	CRCBY_422_1P,
	YCBCR_422_2P,
	YCRCB_422_2P,
	YCBCR_422_3P,
	YCBCR_422V_2P,
	YCBCR_422V_3P,
	YCBCR_420_3P,
	YCRCB_420_3P,
	YCBCR_420_2P,
	YCRCB_420_2P,
	YCBCR_420_2P_M,
	YCRCB_420_2P_M,
	RGB_565,
	BGR_565,
	RGB_888,
	BGR_888,
	ARGB_8888,
	ABGR_8888,
	GRAY,
/* jpeg data format */
	JPEG_422,	/* decode input, encode output */
	JPEG_420,	/* decode input, encode output */
	JPEG_444,	/* decode input, encode output */
	JPEG_422V,	/* decode input, encode output */
	JPEG_GRAY,	/* decode input, encode output */
	JPEG_RESERVED,
};

enum jpeg_scale_value {
	JPEG_SCALE_NORMAL,
	JPEG_SCALE_2,
	JPEG_SCALE_4,
	JPEG_SCALE_8,
};

enum jpeg_interface {
	M2M_OUTPUT,
	M2M_CAPTURE,
};

struct jpeg_fmt {
	char			*name;
	unsigned int			fourcc;
	int			depth[JPEG_MAX_PLANE];
	int			color;
	int			memplanes;
	int			colplanes;
	enum jpeg_interface	types;
};

struct jpeg_param {
	unsigned int in_width;
	unsigned int in_height;
	unsigned int out_width;
	unsigned int out_height;
	unsigned int color;
	unsigned int size;
	unsigned int mem_size;
	unsigned int in_plane;
	unsigned int out_plane;
	unsigned int in_depth[JPEG_MAX_PLANE];
	unsigned int out_depth[JPEG_MAX_PLANE];
	unsigned int top_margin;
	unsigned int left_margin;
	unsigned int bottom_margin;
	unsigned int right_margin;

	enum jpeg_frame_format in_fmt;
	enum jpeg_frame_format out_fmt;
	enum jpeg_img_quality_level quality;
};

struct jpeg_frame {
	struct jpeg_fmt		*jpeg_fmt;
	unsigned int		width;
	unsigned int		height;
	__u32			pixelformat;
	unsigned long		byteused[VIDEO_MAX_PLANES];
};

struct jpeg_ctx {
	spinlock_t			slock;
	struct jpeg_dev		*jpeg_dev;
	struct v4l2_m2m_ctx	*m2m_ctx;

	struct jpeg_frame	s_frame;
	struct jpeg_frame	d_frame;
	struct jpeg_param	param;
	int			index;
	unsigned long		payload[VIDEO_MAX_PLANES];
	unsigned long		flags;
};

struct jpeg_vb2 {
	const struct vb2_mem_ops *ops;
	void *(*init)(struct jpeg_dev *dev);
	void (*cleanup)(void *alloc_ctx);

	unsigned long (*plane_addr)(struct vb2_buffer *vb, u32 plane_no);

	int (*resume)(void *alloc_ctx);
	void (*suspend)(void *alloc_ctx);

	void (*set_cacheable)(void *alloc_ctx, bool cacheable);
	int (*buf_prepare)(struct vb2_buffer *vb);
	int (*buf_finish)(struct vb2_buffer *vb);
};

struct jpeg_dev {
	spinlock_t			slock;
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd;
	struct v4l2_m2m_dev	*m2m_dev;
	struct v4l2_m2m_ops	*m2m_ops;
	struct vb2_alloc_ctx	*alloc_ctx;

	struct platform_device	*plat_dev;

	struct clk		*clk;
	struct clk		*sclk_clk;
	struct clk		*clk_parn1;
	struct clk		*clk_chld1;
	struct clk		*clk_parn2;
	struct clk		*clk_chld2;
	struct clk		*clk_parn3;
	struct clk		*clk_chld3;
	atomic_t		clk_cnt;

	struct mutex		lock;

	int			id;
	int			irq_no;
	enum jpeg_result	irq_ret;
	wait_queue_head_t	wait;
	unsigned long		state;
	void __iomem		*reg_base;	/* register i/o */
	enum jpeg_mode		mode;
	const struct jpeg_vb2	*vb2;

	unsigned long		hw_run;
	atomic_t		watchdog_cnt;
	struct timer_list	watchdog_timer;
	struct workqueue_struct	*watchdog_workqueue;
	struct work_struct	watchdog_work;
	struct device			*bus_dev;
	struct exynos_platform_jpeg	*pdata;
#ifdef JPEG_PERF
	unsigned long long start_time;
	unsigned long long end_time;
#endif
};

enum jpeg_log {
	JPEG_LOG_DEBUG		= 0x1000,
	JPEG_LOG_INFO		= 0x0100,
	JPEG_LOG_WARN		= 0x0010,
	JPEG_LOG_ERR		= 0x0001,
};

static inline struct jpeg_frame *ctx_get_frame(struct jpeg_ctx *ctx,
						enum v4l2_buf_type type)
{
	struct jpeg_frame *frame;

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			frame = &ctx->s_frame;
		else
			frame = &ctx->d_frame;
	} else {
		dev_err(ctx->jpeg_dev->bus_dev,
				"Wrong V4L2 buffer type %d\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

/* debug macro */
#define JPEG_LOG_DEFAULT       (JPEG_LOG_WARN | JPEG_LOG_ERR)

#define jpeg_dbg(fmt, ...)						\
	do {								\
		if (JPEG_LOG_DEFAULT & JPEG_LOG_DEBUG)			\
			printk(KERN_DEBUG "%s: "			\
				fmt, __func__, ##__VA_ARGS__);		\
	} while (0)

#define jpeg_info(fmt, ...)						\
	do {								\
		if (JPEG_LOG_DEFAULT & JPEG_LOG_INFO)			\
			printk(KERN_INFO "%s: "				\
				fmt, __func__, ##__VA_ARGS__);		\
	} while (0)

#define jpeg_warn(fmt, ...)						\
	do {								\
		if (JPEG_LOG_DEFAULT & JPEG_LOG_WARN)			\
			printk(KERN_WARNING "%s: "			\
				fmt, __func__, ##__VA_ARGS__);		\
	} while (0)


#define jpeg_err(fmt, ...)						\
	do {								\
		if (JPEG_LOG_DEFAULT & JPEG_LOG_ERR)			\
			printk(KERN_ERR "%s: "				\
				fmt, __func__, ##__VA_ARGS__);		\
	} while (0)

/*=====================================================================*/
const struct v4l2_ioctl_ops *get_jpeg_dec_v4l2_ioctl_ops(void);
const struct v4l2_ioctl_ops *get_jpeg_enc_v4l2_ioctl_ops(void);
const struct v4l2_ioctl_ops *get_jpeg_v4l2_ioctl_ops(void);

int jpeg_int_pending(struct jpeg_dev *ctrl);

#endif /*__JPEG_CORE_H__*/

