/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos TVOUT driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef SAMSUNG_DEOCN_TV_H
#define SAMSUNG_DECON_TV_H

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/pm_qos.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <media/exynos_mc.h>
#include <mach/devfreq.h>

#include <mach/exynos-tv.h>

#if	defined(CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ)
#define CONFIG_DECONTV_USE_BUS_DEVFREQ
#endif

extern int dex_log_level;

#define dex_dbg(fmt, args...)					\
	do {							\
		if (dex_log_level >= 6)				\
			printk(KERN_DEBUG "[DEBUG: %s] "		\
			fmt, __func__, ##args);			\
	} while (0)

#define dex_warn(fmt, args...)					\
	do {							\
		if (dex_log_level >= 4)				\
			printk(KERN_WARNING "[WARN: %s] "	\
			fmt, __func__, ##args);			\
	} while (0)

#define dex_err(fmt, args...)					\
	do {							\
		if (dex_log_level >= 3)				\
			printk(KERN_ERR "[ERROR: %s] "		\
			fmt, __func__, ##args);			\
	} while (0)

#define DEX_DRIVER_NAME		"decon-tv"
#define DEX_MAX_WINDOWS		5
#define DEX_BACKGROUND		0
#define DEX_DEFAULT_WIN		1
#define DEX_OUTPUT_YUV444	0
#define DEX_OUTPUT_RGB888	1
#define DEX_ENABLE		1
#define DEX_DISABLE		0

/** decon_tv pad definitions */
#define DEX_PAD_SINK		0
#define DEX_PAD_SOURCE		1
#define DEX_PADS_NUM		2

/* HDMI and HPD state definitions */
#define HPD_LOW		0
#define HPD_HIGH	1
#define HDMI_STOP	0 << 1
#define HDMI_STREAMING	1 << 1

struct display_driver;
extern struct ion_device *ion_exynos;

struct decon_tv_porch {
	char *name;
	u32 xres;
	u32 yres;
	u32 vbp;
	u32 vfp;
	u32 vsa;
	u32 hbp;
	u32 hfp;
	u32 hsa;
	u32 vmode;
};

enum s5p_decon_tv_rgb {
	MIXER_RGB601_0_255,
	MIXER_RGB601_16_235,
	MIXER_RGB709_0_255,
	MIXER_RGB709_16_235
};

struct dex_dma_buf_data {
	struct ion_handle		*ion_handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sg_table;
	dma_addr_t			dma_addr;
	struct sync_fence		*fence;
};

struct dex_reg_data {
	struct list_head	list;
	u32			wincon[DEX_MAX_WINDOWS];
	u32			vidosd_a[DEX_MAX_WINDOWS];
	u32			vidosd_b[DEX_MAX_WINDOWS];
	u32			vidosd_c[DEX_MAX_WINDOWS];
	u32			vidosd_d[DEX_MAX_WINDOWS];
	u32			blendeq[DEX_MAX_WINDOWS - 1];
	u32			buf_start[DEX_MAX_WINDOWS];
	u32			buf_end[DEX_MAX_WINDOWS];
	u32			buf_size[DEX_MAX_WINDOWS];
	struct dex_dma_buf_data dma_buf_data[DEX_MAX_WINDOWS];
	u32			win_overlap_cnt;
};

struct dex_resources {
	int irq;
	void __iomem *dex_regs;
	struct clk *decon_tv;
};

struct dex_win {
	struct dex_device		*dex;
	struct dex_dma_buf_data		dma_buf_data;
	struct fb_info			*fbinfo;
	struct video_device		vfd;

	struct v4l2_subdev		sd;
	struct media_pad		pads[DEX_PADS_NUM];

	int	idx;
	int	use;
	int	local;
	int	gsc_num;
};

struct dex_device {
	struct device			*dev;
	struct v4l2_subdev		*hdmi_sd;
	struct s5p_dex_platdata		*pdata;
	const struct decon_tv_porch	*porch;
	struct dex_win			*windows[DEX_MAX_WINDOWS];
	struct dex_resources		res;
	enum s5p_decon_tv_rgb		color_range;
	int			n_streamer;
	int			n_power;

	struct mutex		update_list_lock;
	struct list_head	update_list;
	struct task_struct	*update_thread;
	struct kthread_worker	update_worker;
	struct kthread_work	update_work;

	struct ion_client	*ion_client;
	struct sw_sync_timeline *timeline;
	int			timeline_max;

	struct mutex		mutex;
	struct mutex		s_mutex;
	spinlock_t		reg_slock;

	wait_queue_head_t vsync_wait;
	ktime_t           vsync_timestamp;
};

/* transform entity structure into sub device */
static inline struct dex_win *v4l2_subdev_to_dex_win(struct v4l2_subdev *sd)
{
	return container_of(sd, struct dex_win, sd);
}

/* find sink pad of output via enabled link*/
static inline struct v4l2_subdev *dex_remote_subdev(struct dex_win *win)
{
	struct media_pad *remote;

	remote = media_entity_remote_source(&win->pads[DEX_PAD_SOURCE]);

	if (remote == NULL)
		return NULL;
	if (media_entity_type(remote->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
		dex_warn("cannot find remote pad\n");

	return media_entity_to_v4l2_subdev(remote->entity);
}

irqreturn_t dex_irq_handler(int irq, void *dev_data);
void dex_shadow_protect(struct dex_device *dex, int idx, int en);
void dex_reg_sw_reset(struct dex_device *dex);
int dex_get_status(struct dex_device *dex);
void dex_tv_update(struct dex_device *dex);
void dex_set_background(struct dex_device *dex);
void dex_reg_reset(struct dex_device *dex);
void dex_update_regs(struct dex_device *dex, struct dex_reg_data *regs);
int dex_reg_compare(struct dex_device *dex, int i, dma_addr_t addr);
void dex_reg_local_on(struct dex_device *dex, int idx);
void dex_reg_local_off(struct dex_device *dex, int idx);
void dex_reg_streamon(struct dex_device *dex);
void dex_reg_streamoff(struct dex_device *dex);
int dex_reg_wait4update(struct dex_device *dex);
void dex_reg_porch(struct dex_device *dex);
void dex_reg_dump(struct dex_device *dex);

struct exynos_hdmi_data {
	enum {
		EXYNOS_HDMI_STATE_PRESET = 0,
		EXYNOS_HDMI_STATE_ENUM_PRESET,
		EXYNOS_HDMI_STATE_CEC_ADDR,
		EXYNOS_HDMI_STATE_HDCP,
		EXYNOS_HDMI_STATE_AUDIO,
	} state;
	struct	v4l2_dv_timings timings;
	struct	v4l2_enum_dv_timings etimings;
	__u32	cec_addr;
	__u32	audio_info;
	int	hdcp;
};

#define EXYNOS_GET_HDMI_CONFIG		_IOW('F', 220, \
						struct exynos_hdmi_data)
#define EXYNOS_SET_HDMI_CONFIG		_IOW('F', 221, \
						struct exynos_hdmi_data)
#endif /* SAMSUNG_DECON_TV_H */
