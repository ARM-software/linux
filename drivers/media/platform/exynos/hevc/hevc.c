/*
 * Exynos HEVC driver
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * Sooyoung Kang, <sooyoung.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define DEBUG

#include <linux/io.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/proc_fs.h>
#include <mach/videonode.h>
#include <media/videobuf2-core.h>
#include <linux/exynos_iovmm.h>
#include <linux/of.h>

#include "hevc_common.h"

#include "hevc_intr.h"
#include "hevc_inst.h"
#include "hevc_mem.h"
#include "hevc_debug.h"
#include "hevc_reg.h"
#include "hevc_ctrl.h"
#include "hevc_dec.h"
#include "hevc_pm.h"
#include "hevc_opr.h"

#define HEVC_NAME		"exynos-hevc"
#define HEVC_DEC_NAME	"exynos-hevc-dec"

int debug_hevc;
module_param(debug_hevc, int, S_IRUGO | S_IWUSR);

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
static struct proc_dir_entry *hevc_proc_entry;

#define HEVC_PROC_ROOT			"hevc"
#define HEVC_PROC_INSTANCE_NUMBER	"instance_number"
#define HEVC_PROC_DRM_INSTANCE_NUMBER	"drm_instance_number"
#define HEVC_PROC_FW_STATUS		"fw_status"

#define HEVC_DRM_MAGIC_SIZE	0x10
#define HEVC_DRM_MAGIC_CHUNK0	0x13cdbf16
#define HEVC_DRM_MAGIC_CHUNK1	0x8b803342
#define HEVC_DRM_MAGIC_CHUNK2	0x5e87f4f5
#define HEVC_DRM_MAGIC_CHUNK3	0x3bd05317

#define HEVC_SFR_AREA_COUNT	11
void hevc_dump_regs(struct hevc_dev *dev)
{
	int i;
	int addr[HEVC_SFR_AREA_COUNT][2] = {
		{ 0x0, 0x50 },
		{ 0x1000, 0x138 },
		{ 0x2000, 0x604 },
		{ 0x6000, 0xA0 },
		{ 0x7000, 0x220 },
		{ 0x8000, 0x764 },
		{ 0x9000, 0xA04 },
		{ 0xA000, 0x200 },
		{ 0xB000, 0x40 },
		{ 0xD000, 0x70 },
		{ 0xF000, 0x5FF },
	};

	pr_err("dumping registers (SFR base = %p)\n", dev->regs_base);

	for (i = 0; i < HEVC_SFR_AREA_COUNT; i++) {
		printk("[%04X .. %04X]\n", addr[i][0], addr[i][0] + addr[i][1]);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, dev->regs_base + addr[i][0],
				addr[i][1], false);
		printk("...\n");
	}
}

static int check_magic(unsigned char *addr)
{
	if (((u32)*(u32 *)(addr) == HEVC_DRM_MAGIC_CHUNK0) &&
	    ((u32)*(u32 *)(addr + 0x4) == HEVC_DRM_MAGIC_CHUNK1) &&
	    ((u32)*(u32 *)(addr + 0x8) == HEVC_DRM_MAGIC_CHUNK2) &&
	    ((u32)*(u32 *)(addr + 0xC) == HEVC_DRM_MAGIC_CHUNK3))
		return 0;
	else if (((u32)*(u32 *)(addr + 0x10) == HEVC_DRM_MAGIC_CHUNK0) &&
	    ((u32)*(u32 *)(addr + 0x14) == HEVC_DRM_MAGIC_CHUNK1) &&
	    ((u32)*(u32 *)(addr + 0x18) == HEVC_DRM_MAGIC_CHUNK2) &&
	    ((u32)*(u32 *)(addr + 0x1C) == HEVC_DRM_MAGIC_CHUNK3))
		return 0x10;
	else
		return -1;
}

static inline void clear_magic(unsigned char *addr)
{
	memset((void *)addr, 0x00, HEVC_DRM_MAGIC_SIZE);
}
#endif

/*
 * A framerate table determines framerate by the interval(us) of each frame.
 * Framerate is not accurate, just rough value to seperate overload section.
 * Base line of each section are selected from 25fps(40000us), 48fps(20833us)
 * and 100fps(10000us).
 *
 * interval(us) | 0           10000         20833         40000           |
 * framerate    |     120fps    |    60fps    |    30fps    |    25fps    |
 */

#define COL_FRAME_RATE		0
#define COL_FRAME_INTERVAL	1
static unsigned long framerate_table[][2] = {
	{ 25000, 40000 },
	{ 30000, 20833 },
	{ 60000, 10000 },
	{ 120000, 0 },
};

static inline unsigned long timeval_diff(struct timeval *to,
					struct timeval *from)
{
	return (to->tv_sec * USEC_PER_SEC + to->tv_usec)
		- (from->tv_sec * USEC_PER_SEC + from->tv_usec);
}

int hevc_get_framerate(struct timeval *to, struct timeval *from)
{
	int i;
	unsigned long interval;

	if (timeval_compare(to, from) <= 0)
		return 0;

	interval = timeval_diff(to, from);

	/* if the interval is too big (2sec), framerate set to 0 */
	if (interval > (2 * USEC_PER_SEC))
		return 0;

	for (i = 0; i < ARRAY_SIZE(framerate_table); i++) {
		if (interval > framerate_table[i][COL_FRAME_INTERVAL])
			return framerate_table[i][COL_FRAME_RATE];
	}

	return 0;
}

void hevc_sched_worker(struct work_struct *work)
{
	struct hevc_dev *dev;

	dev = container_of(work, struct hevc_dev, sched_work);

	hevc_try_run(dev);
}

inline int hevc_clear_hw_bit(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	int ret = -1;

	if (!atomic_read(&dev->watchdog_run))
		ret = test_and_clear_bit(ctx->num, &dev->hw_lock);

	return ret;
}

/* Helper functions for interrupt processing */
/* Remove from hw execution round robin */
static inline void clear_work_bit(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = NULL;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	spin_lock(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock(&dev->condlock);
}

/* Wake up context wait_queue */
static inline void wake_up_ctx(struct hevc_ctx *ctx, unsigned int reason,
			       unsigned int err)
{
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	ctx->int_cond = 1;
	ctx->int_type = reason;
	ctx->int_err = err;
	wake_up(&ctx->queue);
}

/* Wake up device wait_queue */
static inline void wake_up_dev(struct hevc_dev *dev, unsigned int reason,
			       unsigned int err)
{
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	dev->int_cond = 1;
	dev->int_type = reason;
	dev->int_err = err;
	wake_up(&dev->queue);
}

void hevc_watchdog(unsigned long arg)
{
	struct hevc_dev *dev = (struct hevc_dev *)arg;

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	spin_lock_irq(&dev->condlock);
	if (dev->hw_lock)
		atomic_inc(&dev->watchdog_cnt);
	spin_unlock_irq(&dev->condlock);

	if (atomic_read(&dev->watchdog_cnt) >= HEVC_WATCHDOG_CNT) {
		/* This means that hw is busy and no interrupts were
		 * generated by hw for the Nth time of running this
		 * watchdog timer. This usually means a serious hw
		 * error. Now it is time to kill all instances and
		 * reset the HEVC. */
		hevc_err("Time out during waiting for HW.\n");
		queue_work(dev->watchdog_wq, &dev->watchdog_work);
	}
	dev->watchdog_timer.expires = jiffies +
					msecs_to_jiffies(HEVC_WATCHDOG_INTERVAL);
	add_timer(&dev->watchdog_timer);
}

static void hevc_watchdog_worker(struct work_struct *work)
{
	struct hevc_dev *dev;
	struct hevc_ctx *ctx;
	int i, ret;
	int mutex_locked;
	unsigned long flags;
	int ref_cnt;

	dev = container_of(work, struct hevc_dev, watchdog_work);

	if (atomic_read(&dev->watchdog_run)) {
		hevc_err("watchdog already running???\n");
		return;
	}

	atomic_set(&dev->watchdog_run, 1);

	hevc_err("Driver timeout error handling.\n");
	/* Lock the mutex that protects open and release.
	 * This is necessary as they may load and unload firmware. */
	mutex_locked = mutex_trylock(&dev->hevc_mutex);
	if (!mutex_locked)
		hevc_err("This is not good. Some instance may be "
							"closing/opening.\n");

	/* Call clock on/off to make ref count 0 */
	ref_cnt = hevc_get_clk_ref_cnt();
	hevc_info("Clock reference count: %d\n", ref_cnt);
	if (ref_cnt < 0) {
		for (i = ref_cnt; i < 0; i++)
			hevc_clock_on();
	} else if (ref_cnt > 0) {
		for (i = ref_cnt; i > 0; i--)
			hevc_clock_off();
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	for (i = 0; i < HEVC_NUM_CONTEXTS; i++) {
		ctx = dev->ctx[i];
		if (ctx && (ctx->inst_no != HEVC_NO_INSTANCE_SET)) {
			ctx->state = HEVCINST_ERROR;
			ctx->inst_no = HEVC_NO_INSTANCE_SET;
			hevc_cleanup_queue(&ctx->dst_queue,
				&ctx->vq_dst);
			hevc_cleanup_queue(&ctx->src_queue,
				&ctx->vq_src);
			clear_work_bit(ctx);
			wake_up_ctx(ctx, HEVC_R2H_CMD_ERR_RET, 0);
		}
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	for (i = 0; i < HEVC_NUM_CONTEXTS; i++) {
		ctx = dev->ctx[i];
		if (ctx && (ctx->inst_no != HEVC_NO_INSTANCE_SET))
			hevc_qos_off(ctx);
	}

	spin_lock_irq(&dev->condlock);
	dev->hw_lock = 0;
	spin_unlock_irq(&dev->condlock);

	/* Double check if there is at least one instance running.
	 * If no instance is in memory than no firmware should be present */
	if (dev->num_inst > 0) {
		ret = hevc_load_firmware(dev);
		if (ret != 0) {
			hevc_err("Failed to reload FW.\n");
			goto watchdog_exit;
		}

		ret = hevc_init_hw(dev);
		if (ret != 0) {
			hevc_err("Failed to reinit FW.\n");
			goto watchdog_exit;
		}
	}

watchdog_exit:
	if (mutex_locked)
		mutex_unlock(&dev->hevc_mutex);

	atomic_set(&dev->watchdog_run, 0);
}

static inline enum hevc_node_type hevc_get_node_type(struct file *file)
{
	struct video_device *vdev = video_devdata(file);

	if (!vdev) {
		hevc_err("failed to get video_device");
		return HEVCNODE_INVALID;
	}

	hevc_debug(2, "video_device index: %d\n", vdev->index);

	if (vdev->index == 0)
		return HEVCNODE_DECODER;
	else
		return HEVCNODE_INVALID;
}

static void hevc_handle_frame_all_extracted(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec;
	struct hevc_buf *dst_buf;
	int index, is_first = 1;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dec = ctx->dec_priv;

	hevc_debug(2, "Decided to finish\n");
	ctx->sequence++;
	while (!list_empty(&ctx->dst_queue)) {
		dst_buf = list_entry(ctx->dst_queue.next,
				     struct hevc_buf, list);
		hevc_debug(2, "Cleaning up buffer: %d\n",
					  dst_buf->vb.v4l2_buf.index);
		vb2_set_plane_payload(&dst_buf->vb, 0, 0);
		vb2_set_plane_payload(&dst_buf->vb, 1, 0);
		list_del(&dst_buf->list);
		ctx->dst_queue_cnt--;
		dst_buf->vb.v4l2_buf.sequence = (ctx->sequence++);

		if (hevc_read_info(ctx, PIC_TIME_TOP) ==
			hevc_read_info(ctx, PIC_TIME_BOT))
			dst_buf->vb.v4l2_buf.field = V4L2_FIELD_NONE;
		else
			dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED;

		clear_bit(dst_buf->vb.v4l2_buf.index, &dec->dpb_status);

		index = dst_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
			hevc_err("failed in get_buf_ctrls_val\n");

		if (is_first) {
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				dec->stored_tag);
			is_first = 0;
		} else {
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				DEFAULT_TAG);
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_VIDEO_H264_SEI_FP_AVAIL,
				0);
		}

		vb2_buffer_done(&dst_buf->vb, VB2_BUF_STATE_DONE);
		hevc_debug(2, "Cleaned up buffer: %d\n",
			  dst_buf->vb.v4l2_buf.index);
	}
	if (ctx->state != HEVCINST_ABORT && ctx->state != HEVCINST_HEAD_PARSED)
		ctx->state = HEVCINST_RUNNING;
	hevc_debug(2, "After cleanup\n");
}

/*
 * Used only when dynamic DPB is enabled.
 * Check released buffers are enqueued again.
 */
static void hevc_check_ref_frame(struct hevc_ctx *ctx,
			struct list_head *ref_list, int ref_index)
{
	struct hevc_dec *dec = ctx->dec_priv;
	struct hevc_buf *ref_buf, *tmp_buf;
	int index;

	list_for_each_entry_safe(ref_buf, tmp_buf, ref_list, list) {
		index = ref_buf->vb.v4l2_buf.index;
		if (index == ref_index) {
			list_del(&ref_buf->list);
			dec->ref_queue_cnt--;

			list_add_tail(&ref_buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;

			dec->assigned_fd[index] =
					ref_buf->vb.v4l2_planes[0].m.fd;
			clear_bit(index, &dec->dpb_status);
			hevc_debug(2, "Move buffer[%d], fd[%d] to dst queue\n",
					index, dec->assigned_fd[index]);
			break;
		}
	}
}

/* Process the released reference information */
static void hevc_handle_released_info(struct hevc_ctx *ctx,
				struct list_head *dst_queue_addr,
				unsigned int released_flag, int index)
{
	struct hevc_dec *dec = ctx->dec_priv;
	struct dec_dpb_ref_info *refBuf;
	int t, ncount = 0;

	refBuf = &dec->ref_info[index];

	if (released_flag) {
		for (t = 0; t < HEVC_MAX_DPBS; t++) {
			if (released_flag & (1 << t)) {
				hevc_debug(2, "Release FD[%d] = %03d !! ",
						t, dec->assigned_fd[t]);
				refBuf->dpb[ncount].fd[0] = dec->assigned_fd[t];
				dec->assigned_fd[t] = HEVC_INFO_INIT_FD;
				ncount++;
				hevc_check_ref_frame(ctx, dst_queue_addr, t);
			}
		}
	}

	if (ncount != HEVC_MAX_DPBS)
		refBuf->dpb[ncount].fd[0] = HEVC_INFO_INIT_FD;
}

static void hevc_handle_frame_copy_timestamp(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec;
	struct hevc_buf *dst_buf, *src_buf;
	dma_addr_t dec_y_addr;
	struct list_head *dst_queue_addr;

	dec_y_addr = HEVC_GET_ADR(DEC_DECODED_Y);

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dec = ctx->dec_priv;

	if (dec->is_dynamic_dpb)
		dst_queue_addr = &dec->ref_queue;
	else
		dst_queue_addr = &ctx->dst_queue;

	/* Copy timestamp from consumed src buffer to decoded dst buffer */
	src_buf = list_entry(ctx->src_queue.next, struct hevc_buf, list);
	list_for_each_entry(dst_buf, dst_queue_addr, list) {
		if (hevc_mem_plane_addr(ctx, &dst_buf->vb, 0) ==
								dec_y_addr) {
			memcpy(&dst_buf->vb.v4l2_buf.timestamp,
					&src_buf->vb.v4l2_buf.timestamp,
					sizeof(struct timeval));
			break;
		}
	}

}

static void hevc_handle_frame_new(struct hevc_ctx *ctx, unsigned int err)
{
	struct hevc_dec *dec;
	struct hevc_dev *dev;
	struct hevc_buf *dst_buf;
	struct hevc_raw_info *raw;
	dma_addr_t dspl_y_addr;
	unsigned int index;
	unsigned int frame_type;
	unsigned int dst_frame_status;
	struct list_head *dst_queue_addr;
	unsigned int prev_flag, released_flag = 0;
	int i;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return;
	}

	raw = &ctx->raw_buf;
	frame_type = hevc_get_disp_frame_type();

	hevc_debug(2, "frame_type : %d\n", frame_type);

	ctx->sequence++;

	dspl_y_addr = HEVC_GET_ADR(DEC_DISPLAY_Y);

	if (dec->immediate_display == 1) {
		dspl_y_addr = HEVC_GET_ADR(DEC_DECODED_Y);
		frame_type = hevc_get_dec_frame_type();
	}

	/* If frame is same as previous then skip and do not dequeue */
	if (frame_type == HEVC_DISPLAY_FRAME_NOT_CODED)
		return;

	if (dec->is_dynamic_dpb) {
		prev_flag = dec->dynamic_used;
		dec->dynamic_used = hevc_get_dec_used_flag();
		released_flag = prev_flag & (~dec->dynamic_used);

		hevc_debug(2, "Used flag = %08x, Released Buffer = %08x\n",
				dec->dynamic_used, released_flag);
	}

	/* The HEVC returns address of the buffer, now we have to
	 * check which videobuf does it correspond to */
	if (dec->is_dynamic_dpb)
		dst_queue_addr = &dec->ref_queue;
	else
		dst_queue_addr = &ctx->dst_queue;

	/* The HEVC  returns address of the buffer, now we have to
	 * check which videobuf does it correspond to */
	list_for_each_entry(dst_buf, dst_queue_addr, list) {
		hevc_debug(2, "Listing: %d\n", dst_buf->vb.v4l2_buf.index);
		/* Check if this is the buffer we're looking for */
		hevc_debug(2, "0x%08lx, 0x%08x",
				(unsigned long)hevc_mem_plane_addr(
							ctx, &dst_buf->vb, 0),
				dspl_y_addr);
		if (hevc_mem_plane_addr(ctx, &dst_buf->vb, 0)
							== dspl_y_addr) {
			index = dst_buf->vb.v4l2_buf.index;
			list_del(&dst_buf->list);

			if (dec->is_dynamic_dpb)
				dec->ref_queue_cnt--;
			else
				ctx->dst_queue_cnt--;

			dst_buf->vb.v4l2_buf.sequence = ctx->sequence;

			if (hevc_read_info(ctx, PIC_TIME_TOP) ==
				hevc_read_info(ctx, PIC_TIME_BOT))
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_NONE;
			else
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED;

			for (i = 0; i < raw->num_planes; i++)
				vb2_set_plane_payload(&dst_buf->vb, i,
							raw->plane_size[i]);

			clear_bit(index, &dec->dpb_status);

			dst_buf->vb.v4l2_buf.flags &=
					~(V4L2_BUF_FLAG_KEYFRAME |
					V4L2_BUF_FLAG_PFRAME |
					V4L2_BUF_FLAG_BFRAME);

			switch (frame_type) {
			case HEVC_DISPLAY_FRAME_I:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_KEYFRAME;
				break;
			case HEVC_DISPLAY_FRAME_P:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_PFRAME;
				break;
			case HEVC_DISPLAY_FRAME_B:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_BFRAME;
				break;
			default:
				break;
			}

			if (hevc_err_dspl(err))
				hevc_err("Warning for displayed frame: %d\n",
							hevc_err_dspl(err));

			if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
				hevc_err("failed in get_buf_ctrls_val\n");

			if (dec->is_dynamic_dpb)
				hevc_handle_released_info(ctx, dst_queue_addr,
							released_flag, index);

			if (dec->immediate_display == 1) {
				dst_frame_status = hevc_get_dec_status()
					& HEVC_DEC_STATUS_DECODING_STATUS_MASK;

				call_cop(ctx, get_buf_update_val, ctx,
						&ctx->dst_ctrls[index],
						V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
						dst_frame_status);

				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
					dec->stored_tag);

				dec->immediate_display = 0;
			}

			/* Update frame tag for packed PB */
			if (dec->is_packedpb &&
					(dec->y_addr_for_pb == dspl_y_addr)) {
				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
					dec->stored_tag);
				dec->y_addr_for_pb = 0;
			}

			if (!dec->is_dts_mode) {
				hevc_debug(7, "timestamp: %ld %ld\n",
					dst_buf->vb.v4l2_buf.timestamp.tv_sec,
					dst_buf->vb.v4l2_buf.timestamp.tv_usec);
				hevc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
				ctx->last_framerate =
					(ctx->qos_ratio * hevc_get_framerate(
						&dst_buf->vb.v4l2_buf.timestamp,
						&ctx->last_timestamp)) / 100;

				memcpy(&ctx->last_timestamp,
					&dst_buf->vb.v4l2_buf.timestamp,
					sizeof(struct timeval));
			}

			vb2_buffer_done(&dst_buf->vb,
				hevc_err_dspl(err) ?
				VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);

			break;
		}
	}
}

static int hevc_find_start_code(unsigned char *src_mem, unsigned int remainSize)
{
	unsigned int index = 0;

	for (index = 0; index < remainSize - 3; index++) {
		if ((src_mem[index] == 0x00) && (src_mem[index+1] == 0x00) &&
				(src_mem[index+2] == 0x01))
			return index;
	}

	return -1;
}

static void hevc_handle_frame_error(struct hevc_ctx *ctx,
		unsigned int reason, unsigned int err)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	struct hevc_buf *src_buf;
	unsigned long flags;
	unsigned int index;

	hevc_err("\n irq err!!!\n\n");

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return;
	}

	hevc_err("Interrupt Error: %d\n", err);

	dec->dpb_flush = 0;
	dec->remained = 0;

	spin_lock_irqsave(&dev->irqlock, flags);
	if (!list_empty(&ctx->src_queue)) {
		src_buf = list_entry(ctx->src_queue.next, struct hevc_buf, list);
		index = src_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, recover_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
			hevc_err("failed in recover_buf_ctrls_val\n");

		hevc_debug(2, "HEVC needs next buffer.\n");
		dec->consumed = 0;
		list_del(&src_buf->list);
		ctx->src_queue_cnt--;

		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
			hevc_err("failed in get_buf_ctrls_val\n");

		vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_ERROR);
	}

	hevc_debug(2, "Assesing whether this context should be run again.\n");
	/* This context state is always RUNNING */
	if (ctx->src_queue_cnt == 0 || ctx->dst_queue_cnt < ctx->dpb_count) {
		hevc_debug(2, "No need to run again.\n");
		clear_work_bit(ctx);
	}
	hevc_debug(2, "After assesing whether this context should be run again. %d\n", ctx->src_queue_cnt);

	hevc_clear_int_flags();
	if (hevc_clear_hw_bit(ctx) == 0)
		BUG();
	wake_up_ctx(ctx, reason, err);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	hevc_clock_off();

	queue_work(dev->sched_wq, &dev->sched_work);
}

static void hevc_handle_ref_frame(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec = ctx->dec_priv;
	struct hevc_buf *dec_buf;
	dma_addr_t dec_addr, buf_addr;

	dec_buf = list_entry(ctx->dst_queue.next, struct hevc_buf, list);

	dec_addr = HEVC_GET_ADR(DEC_DECODED_Y);
	buf_addr = hevc_mem_plane_addr(ctx, &dec_buf->vb, 0);

	if ((buf_addr == dec_addr) && (dec_buf->used == 1)) {
		hevc_debug(2, "Find dec buffer y = 0x%x\n", dec_addr);

		list_del(&dec_buf->list);
		ctx->dst_queue_cnt--;

		list_add_tail(&dec_buf->list, &dec->ref_queue);
		dec->ref_queue_cnt++;
	} else {
		hevc_debug(2, "Can't find buffer for addr = 0x%x\n", dec_addr);
		hevc_debug(2, "Expected addr = 0x%x, used = %d\n",
						buf_addr, dec_buf->used);
	}
}

/* Handle frame decoding interrupt */
static void hevc_handle_frame(struct hevc_ctx *ctx,
					unsigned int reason, unsigned int err)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	unsigned int dst_frame_status, sei_avail_status;
	struct hevc_buf *src_buf;
	unsigned long flags;
	unsigned int res_change;
	unsigned int index, remained;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return;
	}

	dst_frame_status = hevc_get_dspl_status()
				& HEVC_DEC_STATUS_DECODING_STATUS_MASK;
	res_change = (hevc_get_dspl_status()
				& HEVC_DEC_STATUS_RESOLUTION_MASK)
				>> HEVC_DEC_STATUS_RESOLUTION_SHIFT;
	sei_avail_status = hevc_get_sei_avail_status();

	if (dec->immediate_display == 1)
		dst_frame_status = hevc_get_dec_status()
				& HEVC_DEC_STATUS_DECODING_STATUS_MASK;

	hevc_debug(2, "Frame Status: %x\n", dst_frame_status);
	hevc_debug(2, "SEI available status: %x\n", sei_avail_status);
	hevc_debug(2, "Used flag: old = %08x, new = %08x\n",
				dec->dynamic_used, hevc_get_dec_used_flag());

	if (ctx->state == HEVCINST_RES_CHANGE_INIT)
		ctx->state = HEVCINST_RES_CHANGE_FLUSH;

	if (res_change) {
		hevc_info("Resolution change set to %d\n", res_change);
		ctx->state = HEVCINST_RES_CHANGE_INIT;
		ctx->wait_state = WAIT_DECODING;
		hevc_debug(2, "Decoding waiting! : %d\n", ctx->wait_state);

		hevc_clear_int_flags();
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);

		hevc_clock_off();

		queue_work(dev->sched_wq, &dev->sched_work);
		return;
	}
	if (dec->dpb_flush)
		dec->dpb_flush = 0;
	if (dec->remained)
		dec->remained = 0;

	spin_lock_irqsave(&dev->irqlock, flags);
	if (ctx->codec_mode == EXYNOS_CODEC_HEVC_DEC &&
		dst_frame_status == HEVC_DEC_STATUS_DECODING_ONLY && sei_avail_status) {
		hevc_info("Frame packing SEI exists for a frame.\n");
		hevc_info("Reallocate DPBs and issue init_buffer.\n");
		ctx->is_dpb_realloc = 1;
		ctx->state = HEVCINST_HEAD_PARSED;
		ctx->capture_state = QUEUE_FREE;
		hevc_handle_frame_all_extracted(ctx);
		goto leave_handle_frame;
	}

	/* All frames remaining in the buffer have been extracted  */
	if (dst_frame_status == HEVC_DEC_STATUS_DECODING_EMPTY) {
		if (ctx->state == HEVCINST_RES_CHANGE_FLUSH) {
			hevc_debug(2, "Last frame received after resolution change.\n");
			hevc_handle_frame_all_extracted(ctx);
			ctx->state = HEVCINST_RES_CHANGE_END;
			goto leave_handle_frame;
		} else {
			hevc_handle_frame_all_extracted(ctx);
		}
	}

	if (dec->is_dynamic_dpb) {
		switch (dst_frame_status) {
		case HEVC_DEC_STATUS_DECODING_ONLY:
			dec->dynamic_used = hevc_get_dec_used_flag();
			/* Fall through */
		case HEVC_DEC_STATUS_DECODING_DISPLAY:
			hevc_handle_ref_frame(ctx);
			break;
		default:
			break;
		}
	}

	if (dst_frame_status == HEVC_DEC_STATUS_DECODING_DISPLAY ||
	    dst_frame_status == HEVC_DEC_STATUS_DECODING_ONLY)
		hevc_handle_frame_copy_timestamp(ctx);

	/* A frame has been decoded and is in the buffer  */
	if (dst_frame_status == HEVC_DEC_STATUS_DISPLAY_ONLY ||
	    dst_frame_status == HEVC_DEC_STATUS_DECODING_DISPLAY) {
		hevc_handle_frame_new(ctx, err);
	} else {
		hevc_debug(2, "No frame decode.\n");
	}
	/* Mark source buffer as complete */
	if (dst_frame_status != HEVC_DEC_STATUS_DISPLAY_ONLY
		&& !list_empty(&ctx->src_queue)) {
		src_buf = list_entry(ctx->src_queue.next, struct hevc_buf,
								list);
		hevc_debug(2, "Packed PB test. Size:%d, prev offset: %ld, this run:"
			" %d\n", src_buf->vb.v4l2_planes[0].bytesused,
			dec->consumed, hevc_get_consumed_stream());
		dec->consumed += hevc_get_consumed_stream();
		remained = src_buf->vb.v4l2_planes[0].bytesused - dec->consumed;

		if (dec->is_packedpb && remained > STUFF_BYTE &&
			dec->consumed < src_buf->vb.v4l2_planes[0].bytesused &&
			hevc_get_dec_frame_type() ==
						HEVC_DECODED_FRAME_P) {
			unsigned char *stream_vir;
			int offset = 0;

			/* Run HEVC again on the same buffer */
			hevc_debug(2, "Running again the same buffer.\n");

			dec->y_addr_for_pb = HEVC_GET_ADR(DEC_DECODED_Y);

			stream_vir = vb2_plane_vaddr(&src_buf->vb, 0);
			hevc_mem_inv_vb(&src_buf->vb, 1);

			offset = hevc_find_start_code(
					stream_vir + dec->consumed, remained);

			if (offset > STUFF_BYTE)
				dec->consumed += offset;

			hevc_set_dec_stream_buffer(ctx,
				src_buf->planes.stream, dec->consumed,
				src_buf->vb.v4l2_planes[0].bytesused -
							dec->consumed);
			dev->curr_ctx = ctx->num;
			dev->curr_ctx_drm = ctx->is_drm;
			hevc_clean_ctx_int_flags(ctx);
			hevc_clear_int_flags();
			wake_up_ctx(ctx, reason, err);
			spin_unlock_irqrestore(&dev->irqlock, flags);
			hevc_decode_one_frame(ctx, 0);
			return;
		} else {
			index = src_buf->vb.v4l2_buf.index;
			if (call_cop(ctx, recover_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
				hevc_err("failed in recover_buf_ctrls_val\n");

			hevc_debug(2, "HEVC needs next buffer.\n");
			dec->consumed = 0;
			list_del(&src_buf->list);
			ctx->src_queue_cnt--;

			if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
				hevc_err("failed in get_buf_ctrls_val\n");

			vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
		}
	}
leave_handle_frame:
	hevc_debug(2, "Assesing whether this context should be run again.\n");
	if (!hevc_dec_ctx_ready(ctx)) {
		hevc_debug(2, "No need to run again.\n");
		clear_work_bit(ctx);
	}
	hevc_debug(2, "After assesing whether this context should be run again.\n");

	hevc_clear_int_flags();
	if (hevc_clear_hw_bit(ctx) == 0)
		BUG();
	wake_up_ctx(ctx, reason, err);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	hevc_clock_off();

	queue_work(dev->sched_wq, &dev->sched_work);
}

/* Error handling for interrupt */
static inline void hevc_handle_error(struct hevc_ctx *ctx,
	unsigned int reason, unsigned int err)
{
	struct hevc_dev *dev;
	unsigned long flags;
	struct hevc_buf *src_buf;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	hevc_err("Interrupt Error: %d\n", err);
	hevc_clear_int_flags();
	wake_up_dev(dev, reason, err);

	/* Error recovery is dependent on the state of context */
	switch (ctx->state) {
	case HEVCINST_INIT:
		/* This error had to happen while acquireing instance */
	case HEVCINST_GOT_INST:
		/* This error had to happen while parsing vps only */
		if (err == HEVC_VPS_ONLY_ERROR) {
			ctx->state = HEVCINST_VPS_PARSED_ONLY;
			if (!list_empty(&ctx->src_queue)) {
				src_buf = list_entry(ctx->src_queue.next, struct hevc_buf,
						list);
				list_del(&src_buf->list);
				ctx->src_queue_cnt--;
				vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
			}
		}

	case HEVCINST_RES_CHANGE_END:
		/* This error had to happen while parsing the header */
	case HEVCINST_HEAD_PARSED:
		/* This error had to happen while setting dst buffers */
	case HEVCINST_RETURN_INST:
		/* This error had to happen while releasing instance */
	case HEVCINST_DPB_FLUSHING:
		/* This error had to happen while flushing DPB */
		clear_work_bit(ctx);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		hevc_clock_off();
		wake_up_ctx(ctx, reason, err);

		break;
	case HEVCINST_FINISHING:
	case HEVCINST_FINISHED:
		/* It is higly probable that an error occured
		 * while decoding a frame */
		clear_work_bit(ctx);
		ctx->state = HEVCINST_ERROR;
		/* Mark all dst buffers as having an error */
		spin_lock_irqsave(&dev->irqlock, flags);
		hevc_cleanup_queue(&ctx->dst_queue, &ctx->vq_dst);
		/* Mark all src buffers as having an error */
		hevc_cleanup_queue(&ctx->src_queue, &ctx->vq_src);
		spin_unlock_irqrestore(&dev->irqlock, flags);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();

		hevc_clock_off();

		break;
	default:
		hevc_err("Encountered an error interrupt which had not been handled.\n");
		hevc_err("ctx->state = %d, ctx->inst_no = %d\n",
						ctx->state, ctx->inst_no);

		clear_work_bit(ctx);
		if (test_and_clear_bit(ctx->num, &dev->hw_lock) == 0)
			BUG();
		hevc_clock_off();

		break;
	}
	return;
}

/* Interrupt processing */
static irqreturn_t hevc_irq(int irq, void *priv)
{
	struct hevc_dev *dev = priv;
	struct hevc_buf *src_buf;
	struct hevc_ctx *ctx;
	struct hevc_dec *dec = NULL;
	unsigned int reason;
	unsigned int err;
	unsigned long flags;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		goto irq_cleanup_hw;
	}

	/* Reset the timeout watchdog */
	atomic_set(&dev->watchdog_cnt, 0);
	/* Get the reason of interrupt and the error code */
	reason = hevc_get_int_reason();
	err = hevc_get_int_err();
	hevc_debug(2, "Int reason: %d (err: %d)\n", reason, err);

	switch (reason) {
	case HEVC_R2H_CMD_SYS_INIT_RET:
	case HEVC_R2H_CMD_FW_STATUS_RET:
	case HEVC_R2H_CMD_SLEEP_RET:
	case HEVC_R2H_CMD_WAKEUP_RET:
		hevc_clear_int_flags();
		/* Initialize hw_lock */
		atomic_clear_mask(HW_LOCK_CLEAR_MASK, &dev->hw_lock);
		wake_up_dev(dev, reason, err);
		goto done;
	}

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		goto irq_cleanup_hw;
	}

	if (ctx->type == HEVCINST_DECODER)
		dec = ctx->dec_priv;

	dev->preempt_ctx = HEVC_NO_INSTANCE_SET;

	switch (reason) {
	case HEVC_R2H_CMD_ERR_RET:
		/* An error has occured */
		if (ctx->state == HEVCINST_RUNNING) {
			if ((hevc_err_dec(err) >= HEVC_ERR_WARNINGS_START) &&
				(hevc_err_dec(err) <= HEVC_ERR_WARNINGS_END))
				hevc_handle_frame(ctx, reason, err);
			else
				hevc_handle_frame_error(ctx, reason, err);
		} else {
			hevc_handle_error(ctx, reason, err);
		}
		break;
	case HEVC_R2H_CMD_SLICE_DONE_RET:
	case HEVC_R2H_CMD_FIELD_DONE_RET:
	case HEVC_R2H_CMD_FRAME_DONE_RET:
		if (ctx->type == HEVCINST_DECODER)
			hevc_handle_frame(ctx, reason, err);
		break;
	case HEVC_R2H_CMD_SEQ_DONE_RET:
		if (ctx->type == HEVCINST_DECODER) {
			ctx->img_width = hevc_get_img_width();
			ctx->img_height = hevc_get_img_height();
			ctx->lcu_size = hevc_get_lcu_size();

			hevc_debug(2, "ctx->img_width = %d\n", ctx->img_width);
			hevc_debug(2, "ctx->img_height = %d\n", ctx->img_height);

			switch (ctx->lcu_size) {
			case 0:
				ctx->lcu_size = 16;
				break;
			case 1:
				ctx->lcu_size = 32;
				break;
			case 2:
				ctx->lcu_size = 64;
				break;
			default:
				break;
			}

			ctx->dpb_count = hevc_get_dpb_count();
			hevc_debug(2, "dpb_count = %d\n", ctx->dpb_count);

			dec->internal_dpb = 0;
			hevc_dec_store_crop_info(ctx);
			if (ctx->img_width == 0 || ctx->img_height == 0)
				ctx->state = HEVCINST_ERROR;
			else
				ctx->state = HEVCINST_HEAD_PARSED;

			if (ctx->state == HEVCINST_HEAD_PARSED)
				dec->is_interlaced =
					hevc_is_interlace_picture();

			if ((ctx->codec_mode == EXYNOS_CODEC_HEVC_DEC) &&
					!list_empty(&ctx->src_queue)) {
				struct hevc_buf *src_buf;
				src_buf = list_entry(ctx->src_queue.next,
						struct hevc_buf, list);
				hevc_debug(2, "Check consumed size of header. ");
				hevc_debug(2, "source : %d, consumed : %d\n",
						hevc_get_consumed_stream(),
						src_buf->vb.v4l2_planes[0].bytesused);
				if (hevc_get_consumed_stream() <
						src_buf->vb.v4l2_planes[0].bytesused)
					dec->remained = 1;
			}
		}

		hevc_clear_int_flags();
		clear_work_bit(ctx);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);

		hevc_clock_off();

		queue_work(dev->sched_wq, &dev->sched_work);
		break;
	case HEVC_R2H_CMD_OPEN_INSTANCE_RET:
		ctx->inst_no = hevc_get_inst_no();
		ctx->state = HEVCINST_GOT_INST;
		clear_work_bit(ctx);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case HEVC_R2H_CMD_CLOSE_INSTANCE_RET:
		ctx->state = HEVCINST_FREE;
		clear_work_bit(ctx);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case HEVC_R2H_CMD_NAL_ABORT_RET:
		ctx->state = HEVCINST_ABORT;
		clear_work_bit(ctx);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case HEVC_R2H_CMD_DPB_FLUSH_RET:
		ctx->state = HEVCINST_ABORT;
		clear_work_bit(ctx);
		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up_ctx(ctx, reason, err);
		goto irq_cleanup_hw;
		break;
	case HEVC_R2H_CMD_INIT_BUFFERS_RET:
		hevc_clear_int_flags();
		ctx->int_type = reason;
		ctx->int_err = err;
		ctx->int_cond = 1;
		spin_lock(&dev->condlock);
		clear_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock(&dev->condlock);
		if (err != 0) {
			if (hevc_clear_hw_bit(ctx) == 0)
				BUG();
			wake_up(&ctx->queue);

			hevc_clock_off();
			break;
		}

		ctx->state = HEVCINST_RUNNING;
		if (ctx->type == HEVCINST_DECODER) {
			if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
				if (!dec->dpb_flush && !dec->remained) {
					hevc_debug(2, "INIT_BUFFERS with dpb_flush - leaving image in src queue.\n");
					spin_lock_irqsave(&dev->irqlock, flags);
					if (!list_empty(&ctx->src_queue)) {
						src_buf = list_entry(ctx->src_queue.next,
								struct hevc_buf, list);
						list_del(&src_buf->list);
						ctx->src_queue_cnt--;
						vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
					}
					spin_unlock_irqrestore(&dev->irqlock, flags);
				} else {
					if (dec->dpb_flush)
						dec->dpb_flush = 0;
				}
			}
			if (ctx->wait_state == WAIT_DECODING) {
				ctx->wait_state = WAIT_INITBUF_DONE;
				hevc_debug(2, "INIT_BUFFER has done, but can't start decoding\n");
			}
			if (ctx->is_dpb_realloc)
				ctx->is_dpb_realloc = 0;
			if (hevc_dec_ctx_ready(ctx)) {
				spin_lock(&dev->condlock);
				set_bit(ctx->num, &dev->ctx_work_bits);
				spin_unlock(&dev->condlock);
			}
		}

		if (hevc_clear_hw_bit(ctx) == 0)
			BUG();
		wake_up(&ctx->queue);

		hevc_clock_off();

		queue_work(dev->sched_wq, &dev->sched_work);
		break;
	default:
		hevc_debug(2, "Unknown int reason.\n");
		hevc_clear_int_flags();
	}

done:
	hevc_debug_leave();
	return IRQ_HANDLED;

irq_cleanup_hw:
	hevc_clear_int_flags();

	hevc_clock_off();

	if (dev)
		queue_work(dev->sched_wq, &dev->sched_work);

	hevc_debug(2, "via irq_cleanup_hw\n");
	return IRQ_HANDLED;
}

/* Open an HEVC node */
static int hevc_open(struct file *file)
{
	struct hevc_ctx *ctx = NULL;
	struct hevc_dev *dev = video_drvdata(file);
	int ret = 0;
	enum hevc_node_type node;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	int magic_offset;
#endif

	hevc_info("hevc driver open called\n");

	if (!dev) {
		hevc_err("no hevc device to run\n");
		goto err_no_device;
	}

	if (mutex_lock_interruptible(&dev->hevc_mutex))
		return -ERESTARTSYS;

	node = hevc_get_node_type(file);
	if (node == HEVCNODE_INVALID) {
		hevc_err("cannot specify node type\n");
		ret = -ENOENT;
		goto err_node_type;
	}

	dev->num_inst++;	/* It is guarded by hevc_mutex in vfd */

	/* Allocate memory for context */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		hevc_err("Not enough memory.\n");
		ret = -ENOMEM;
		goto err_ctx_alloc;
	}

	v4l2_fh_init(&ctx->fh, (node == HEVCNODE_DECODER) ? dev->vfd_dec : dev->vfd_enc);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->dev = dev;

	/* Get context number */
	ctx->num = 0;
	while (dev->ctx[ctx->num]) {
		ctx->num++;
		if (ctx->num >= HEVC_NUM_CONTEXTS) {
			hevc_err("Too many open contexts.\n");
			ret = -EBUSY;
			goto err_ctx_num;
		}
	}

	/* Mark context as idle */
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);
	dev->ctx[ctx->num] = ctx;

	init_waitqueue_head(&ctx->queue);

	if (node == HEVCNODE_DECODER)
		ret = hevc_init_dec_ctx(ctx);

	if (ret)
		goto err_ctx_init;

	ret = call_cop(ctx, init_ctx_ctrls, ctx);
	if (ret) {
		hevc_err("failed int init_buf_ctrls\n");
		goto err_ctx_ctrls;
	}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	/* Multi-instance is supported for same DRM type */
	magic_offset = check_magic(dev->drm_info.virt);
	if (magic_offset >= 0) {
		clear_magic(dev->drm_info.virt + magic_offset);
		if (dev->num_drm_inst < HEVC_MAX_DRM_CTX) {
			hevc_info("DRM instance opened\n");

			dev->num_drm_inst++;
			ctx->is_drm = 1;
		} else {
			hevc_err("Too many instance are opened for DRM\n");
			ret = -EINVAL;
			goto err_drm_start;
		}
		if (dev->num_inst != dev->num_drm_inst) {
			hevc_err("Can not open DRM instance\n");
			hevc_err("Non-DRM instance is already opened.\n");
			ret = -EINVAL;
			goto err_drm_inst;
		}
	} else {
		if (dev->num_drm_inst) {
			hevc_err("Can not open non-DRM instance\n");
			hevc_err("DRM instance is already opened.\n");
			ret = -EINVAL;
			goto err_drm_start;
		}
	}
#endif

	/* Load firmware if this is the first instance */
	if (dev->num_inst == 1) {
		dev->watchdog_timer.expires = jiffies +
					msecs_to_jiffies(HEVC_WATCHDOG_INTERVAL);
		add_timer(&dev->watchdog_timer);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		if (ctx->is_drm) {
			if (!dev->fw_status) {
				ret = hevc_alloc_firmware(dev);
				if (ret)
					goto err_fw_alloc;
			}

			dev->fw_status = 1;
		} else {
			if (!dev->fw_status) {
				ret = hevc_alloc_firmware(dev);
				if (ret)
					goto err_fw_alloc;

				ret = hevc_load_firmware(dev);
				if (ret)
					goto err_fw_load;

				dev->fw_status = 1;
			}
		}
#else
		/* Load the FW */
		ret = hevc_alloc_firmware(dev);
		if (ret)
			goto err_fw_alloc;
		ret = hevc_load_firmware(dev);
		if (ret)
			goto err_fw_load;

#endif

		ret = hevc_alloc_dev_context_buffer(dev);
		if (ret)
			goto err_fw_load;

		hevc_debug(2, "power on\n");
		ret = hevc_power_on();
		if (ret < 0) {
			hevc_err("power on failed\n");
			goto err_pwr_enable;
		}

		/* Set clock source again after power on */
		hevc_set_clock_parent(dev);

		dev->curr_ctx = ctx->num;
		dev->preempt_ctx = HEVC_NO_INSTANCE_SET;
		dev->curr_ctx_drm = ctx->is_drm;

		/* Init the FW */
		ret = hevc_init_hw(dev);
		if (ret) {
			hevc_err("Failed to init hevc h/w\n");
			goto err_hw_init;
		}
	}

	hevc_info("HEVC instance open completed\n");
	mutex_unlock(&dev->hevc_mutex);
	return ret;

	/* Deinit when failure occured */
err_hw_init:
	hevc_power_off();

err_pwr_enable:
	hevc_release_dev_context_buffer(dev);

err_fw_load:
	hevc_release_firmware(dev);
	dev->fw_status = 0;

err_fw_alloc:
	del_timer_sync(&dev->watchdog_timer);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
err_drm_inst:
	if (ctx->is_drm)
		dev->num_drm_inst--;

err_drm_start:
#endif
	call_cop(ctx, cleanup_ctx_ctrls, ctx);

err_ctx_ctrls:
	if (node == HEVCNODE_DECODER){
		kfree(ctx->dec_priv->ref_info);
		kfree(ctx->dec_priv);
	}

err_ctx_init:
	dev->ctx[ctx->num] = 0;

err_ctx_num:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);

err_ctx_alloc:
	dev->num_inst--;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
/* err_drm_playback: // unused label */
#endif
err_node_type:
	mutex_unlock(&dev->hevc_mutex);
err_no_device:
	hevc_info("hevc driver open finished\n");

	return ret;
}

#define need_to_wait_frame_start(ctx)		\
	(((ctx->state == HEVCINST_FINISHING) ||	\
	  (ctx->state == HEVCINST_RUNNING)) &&	\
	 test_bit(ctx->num, &ctx->dev->hw_lock))
/* Release HEVC context */
static int hevc_release(struct file *file)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dev *dev = NULL;

	hevc_info("hevc driver release called\n");

	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	mutex_lock(&dev->hevc_mutex);

	if (need_to_wait_frame_start(ctx)) {
		ctx->state = HEVCINST_ABORT;
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_FRAME_DONE_RET))
			hevc_cleanup_timeout(ctx);
	}

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	hevc_qos_off(ctx);
#endif

	if (call_cop(ctx, cleanup_ctx_ctrls, ctx) < 0)
		hevc_err("failed in init_buf_ctrls\n");

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	/* Mark context as idle */
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	/* If instance was initialised then
	 * return instance and free reosurces */
	if (!atomic_read(&dev->watchdog_run) &&
		(ctx->inst_no != HEVC_NO_INSTANCE_SET)) {
		ctx->state = HEVCINST_RETURN_INST;
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);

		/* Wait for hw_lock == 0 for this context */
		wait_event_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(HEVC_INT_TIMEOUT));

		/* To issue the command 'CLOSE_INSTANCE' */
		hevc_try_run(dev);

		/* Wait until instance is returned or timeout occured */
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_CLOSE_INSTANCE_RET)) {
			dev->curr_ctx_drm = ctx->is_drm;
			set_bit(ctx->num, &dev->hw_lock);
			hevc_clock_on();
			hevc_close_inst(ctx);
			if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_CLOSE_INSTANCE_RET)) {
				hevc_err("Abnormal h/w state.\n");

				/* cleanup for the next open */
				if (dev->curr_ctx == ctx->num)
					clear_bit(ctx->num, &dev->hw_lock);
				if (ctx->is_drm)
					dev->num_drm_inst--;
				dev->num_inst--;

				mutex_unlock(&dev->hevc_mutex);
				return -EIO;
			}
		}

		ctx->inst_no = HEVC_NO_INSTANCE_SET;
	}
	/* hardware locking scheme */
	if (dev->curr_ctx == ctx->num)
		clear_bit(ctx->num, &dev->hw_lock);

	if (ctx->is_drm)
		dev->num_drm_inst--;
	dev->num_inst--;

	if (dev->num_inst == 0) {
		hevc_deinit_hw(dev);

		del_timer_sync(&dev->watchdog_timer);

		flush_workqueue(dev->sched_wq);

		hevc_info("power off\n");
		hevc_power_off();

		/* reset <-> F/W release */
		hevc_release_firmware(dev);
		hevc_release_dev_context_buffer(dev);
		dev->fw_status = 0;


	}

	/* Free resources */
	vb2_queue_release(&ctx->vq_src);
	vb2_queue_release(&ctx->vq_dst);

	hevc_release_codec_buffers(ctx);
	hevc_release_instance_buffer(ctx);

	if (ctx->type == HEVCINST_DECODER){
		hevc_dec_cleanup_user_shared_handle(ctx);
		kfree(ctx->dec_priv);
	}

	dev->ctx[ctx->num] = 0;
	kfree(ctx);

	hevc_info("hevc driver release finished\n");

	mutex_unlock(&dev->hevc_mutex);

	return 0;
}

/* Poll */
static unsigned int hevc_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	unsigned int ret = 0;

	if (hevc_get_node_type(file) == HEVCNODE_DECODER)
		ret = vb2_poll(&ctx->vq_src, file, wait);

	return ret;
}

/* Mmap */
static int hevc_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	int ret;

	hevc_debug_enter();

	if (offset < DST_QUEUE_OFF_BASE) {
		hevc_debug(2, "mmaping source.\n");
		ret = vb2_mmap(&ctx->vq_src, vma);
	} else {		/* capture */
		hevc_debug(2, "mmaping destination.\n");
		vma->vm_pgoff -= (DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
		ret = vb2_mmap(&ctx->vq_dst, vma);
	}
	hevc_debug_leave();
	return ret;
}

/* v4l2 ops */
static const struct v4l2_file_operations hevc_fops = {
	.owner = THIS_MODULE,
	.open = hevc_open,
	.release = hevc_release,
	.poll = hevc_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = hevc_mmap,
};

/* videodec structure */
static struct video_device hevc_dec_videodev = {
	.name = HEVC_DEC_NAME,
	.fops = &hevc_fops,
	.minor = -1,
	.release = video_device_release,
};

static void *hevc_get_drv_data(struct platform_device *pdev);

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
#define QOS_STEP_NUM (4)
static struct hevc_qos g_hevc_qos_table[QOS_STEP_NUM];
#endif


#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
static int parse_hevc_qos_platdata(struct device_node *np, char *node_name,
	struct hevc_qos *pdata)
{
	int ret = 0;
	struct device_node *np_qos;

	np_qos = of_find_node_by_name(np, node_name);
	if (!np_qos) {
		pr_err("%s: could not find hevc_qos_platdata node\n",
			node_name);
		return -EINVAL;
	}

	of_property_read_u32(np_qos, "thrd_mb", &pdata->thrd_mb);
	of_property_read_u32(np_qos, "freq_hevc", &pdata->freq_hevc);
	of_property_read_u32(np_qos, "freq_int", &pdata->freq_int);
	of_property_read_u32(np_qos, "freq_mif", &pdata->freq_mif);
	of_property_read_u32(np_qos, "freq_cpu", &pdata->freq_cpu);
	of_property_read_u32(np_qos, "freq_kfc", &pdata->freq_kfc);

	return ret;
}
#endif

static void hevc_parse_dt(struct device_node *np, struct hevc_dev *hevc)
{
	struct hevc_platdata	*pdata = hevc->pdata;

	if (!np)
		return;

	of_property_read_u32(np, "ip_ver", &pdata->ip_ver);
	of_property_read_u32(np, "clock_rate", &pdata->clock_rate);
	of_property_read_u32(np, "min_rate", &pdata->min_rate);
#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	of_property_read_u32(np, "num_qos_steps", &pdata->num_qos_steps);

	parse_hevc_qos_platdata(np, "hevc_qos_variant_0", &g_hevc_qos_table[0]);
	parse_hevc_qos_platdata(np, "hevc_qos_variant_1", &g_hevc_qos_table[1]);
	parse_hevc_qos_platdata(np, "hevc_qos_variant_2", &g_hevc_qos_table[2]);
	parse_hevc_qos_platdata(np, "hevc_qos_variant_3", &g_hevc_qos_table[3]);
#endif
}

/* HEVC probe function */
static int hevc_probe(struct platform_device *pdev)
{
	struct hevc_dev *dev;
	struct video_device *vfd;
	struct resource *res;
	int ret = -ENOENT;
	unsigned int alloc_ctx_num;
#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	int i;
#endif

	pr_info("+ %s\n", __func__);

	dev_dbg(&pdev->dev, "%s()\n", __func__);
	dev = devm_kzalloc(&pdev->dev, sizeof(struct hevc_dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "Not enough memory for HEVC device.\n");
		return -ENOMEM;
	}

	spin_lock_init(&dev->irqlock);
	spin_lock_init(&dev->condlock);

	dev->device = &pdev->dev;

	dev->pdata = devm_kzalloc(&pdev->dev, sizeof(struct hevc_platdata), GFP_KERNEL);
	if (!dev->pdata) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	hevc_parse_dt(dev->device->of_node, dev);

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	/* initial clock rate should be min rate */
	dev->curr_rate = dev->min_rate = dev->pdata->min_rate;
	dev->pdata->qos_table = g_hevc_qos_table;
#endif

	dev->variant = hevc_get_drv_data(pdev);

	ret = hevc_init_pm(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to setup hevc clock & power\n");
		goto err_pm;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		ret = -ENOENT;
		goto err_res_mem;
	}
	dev->hevc_mem = request_mem_region(res->start, resource_size(res),
					pdev->name);
	if (dev->hevc_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req_mem;
	}
	dev->regs_base = ioremap(dev->hevc_mem->start,
				resource_size(dev->hevc_mem));
	if (dev->regs_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap address region\n");
		ret = -ENOENT;
		goto err_ioremap;
	}

	hevc_init_reg(dev->regs_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		ret = -ENOENT;
		goto err_res_irq;
	}
	dev->irq = res->start;
	ret = request_threaded_irq(dev->irq, NULL, hevc_irq, IRQF_ONESHOT, pdev->name,
									dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to install irq (%d)\n", ret);
		goto err_req_irq;
	}

	mutex_init(&dev->hevc_mutex);

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		goto err_v4l2_dev;

	init_waitqueue_head(&dev->queue);

	/* decoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_dec;
	}
	*vfd = hevc_dec_videodev;

	vfd->ioctl_ops = hevc_get_dec_v4l2_ioctl_ops();

	vfd->lock = &dev->hevc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	snprintf(vfd->name, sizeof(vfd->name), "%s", hevc_dec_videodev.name);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, EXYNOS_VIDEONODE_HEVC_DEC);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_dec;
	}
	v4l2_info(&dev->v4l2_dev, "decoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_dec = vfd;

	video_set_drvdata(vfd, dev);

	platform_set_drvdata(pdev, dev);

	dev->hw_lock = 0;
	dev->watchdog_wq =
		create_singlethread_workqueue("hevc/watchdog");
	if (!dev->watchdog_wq) {
		dev_err(&pdev->dev, "failed to create workqueue for watchdog\n");
		goto err_wq_watchdog;
	}
	INIT_WORK(&dev->watchdog_work, hevc_watchdog_worker);
	atomic_set(&dev->watchdog_cnt, 0);
	atomic_set(&dev->watchdog_run, 0);
	init_timer(&dev->watchdog_timer);
	dev->watchdog_timer.data = (unsigned long)dev;
	dev->watchdog_timer.function = hevc_watchdog;

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	INIT_LIST_HEAD(&dev->qos_queue);
#endif

	/* default FW alloc is added */
	alloc_ctx_num = NUM_OF_ALLOC_CTX(dev);
	dev->alloc_ctx = (struct vb2_alloc_ctx **)
			hevc_mem_init_multi(&pdev->dev, alloc_ctx_num);

	if (IS_ERR(dev->alloc_ctx)) {
		hevc_err("Couldn't prepare allocator ctx.\n");
		ret = PTR_ERR(dev->alloc_ctx);
		goto alloc_ctx_fail;
	}

	exynos_create_iovmm(&pdev->dev, 3, 3);
	dev->sched_wq = alloc_workqueue("hevc/sched", WQ_UNBOUND
					| WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (dev->sched_wq == NULL) {
		dev_err(&pdev->dev, "failed to create workqueue for scheduler\n");
		goto err_wq_sched;
	}
	INIT_WORK(&dev->sched_work, hevc_sched_worker);

#ifdef CONFIG_ION_EXYNOS
	dev->hevc_ion_client = ion_client_create(ion_exynos, "hevc");
	if (IS_ERR(dev->hevc_ion_client)) {
		dev_err(&pdev->dev, "failed to ion_client_create\n");
		goto err_ion_client;
	}
#endif

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	dev->alloc_ctx_fw = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev, SZ_4K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_MFCFW |
			VB2ION_CTX_KVA_STATIC);
	if (IS_ERR(dev->alloc_ctx_fw)) {
		hevc_err("failed to prepare F/W allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_fw);
		goto alloc_ctx_fw_fail;
	}

	dev->alloc_ctx_sh = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			SZ_4K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_MFCSH |
			VB2ION_CTX_KVA_STATIC);
	if (IS_ERR(dev->alloc_ctx_sh)) {
		hevc_err("failed to prepare shared allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_sh);
		goto alloc_ctx_sh_fail;
	}

	dev->drm_info.alloc = hevc_mem_alloc_priv(dev->alloc_ctx_sh,
						PAGE_SIZE);
	if (IS_ERR(dev->drm_info.alloc)) {
		hevc_err("failed to allocate shared region\n");
		ret = PTR_ERR(dev->drm_info.alloc);
		goto shared_alloc_fail;
	}
	dev->drm_info.virt = hevc_mem_vaddr_priv(dev->drm_info.alloc);
	if (!dev->drm_info.virt) {
		hevc_err("failed to get vaddr for shared region\n");
		ret = -ENOMEM;
		goto shared_vaddr_fail;
	}

	dev->alloc_ctx_drm = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			SZ_4K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_VIDEO |
			VB2ION_CTX_KVA_STATIC);
	if (IS_ERR(dev->alloc_ctx_drm)) {
		hevc_err("failed to prepare DRM allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_drm);
		goto alloc_ctx_drm_fail;
	}
#endif

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	atomic_set(&dev->qos_req_cur, 0);

	dev->qos_req_cnt = kzalloc(dev->pdata->num_qos_steps * sizeof(atomic_t),
					GFP_KERNEL);
	if (!dev->qos_req_cnt) {
		dev_err(&pdev->dev, "failed to allocate QoS request count\n");
		ret = -ENOMEM;
		goto err_qos_cnt;
	}
	for (i = 0; i < dev->pdata->num_qos_steps; i++)
		atomic_set(&dev->qos_req_cnt[i], 0);
#endif

	pr_debug("%s--\n", __func__);
	return 0;

/* Deinit HEVC if probe had failed */
#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
err_qos_cnt:
#endif
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	vb2_ion_destroy_context(dev->alloc_ctx_drm);
shared_vaddr_fail:
	hevc_mem_free_priv(dev->drm_info.alloc);
shared_alloc_fail:
alloc_ctx_drm_fail:
	vb2_ion_destroy_context(dev->alloc_ctx_sh);
alloc_ctx_sh_fail:
	vb2_ion_destroy_context(dev->alloc_ctx_fw);
alloc_ctx_fw_fail:
	destroy_workqueue(dev->sched_wq);
#endif
#ifdef CONFIG_ION_EXYNOS
	ion_client_destroy(dev->hevc_ion_client);
err_ion_client:
#endif
err_wq_sched:
	hevc_mem_cleanup_multi((void **)dev->alloc_ctx,
			alloc_ctx_num);
alloc_ctx_fail:
	destroy_workqueue(dev->watchdog_wq);
err_wq_watchdog:
	video_unregister_device(dev->vfd_enc);
	video_unregister_device(dev->vfd_dec);
reg_vdev_dec:
alloc_vdev_dec:
	v4l2_device_unregister(&dev->v4l2_dev);
err_v4l2_dev:
	free_irq(dev->irq, dev);
err_req_irq:
err_res_irq:
	iounmap(dev->regs_base);
err_ioremap:
	release_mem_region(dev->hevc_mem->start, resource_size(dev->hevc_mem));
err_req_mem:
err_res_mem:
	hevc_final_pm(dev);
err_pm:
	return ret;
}

/* Remove the driver */
static int hevc_remove(struct platform_device *pdev)
{
	struct hevc_dev *dev = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s++\n", __func__);
	v4l2_info(&dev->v4l2_dev, "Removing %s\n", pdev->name);
	del_timer_sync(&dev->watchdog_timer);
	flush_workqueue(dev->watchdog_wq);
	destroy_workqueue(dev->watchdog_wq);
	flush_workqueue(dev->sched_wq);
	destroy_workqueue(dev->sched_wq);
	video_unregister_device(dev->vfd_enc);
	video_unregister_device(dev->vfd_dec);
	v4l2_device_unregister(&dev->v4l2_dev);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	remove_proc_entry(HEVC_PROC_FW_STATUS, hevc_proc_entry);
	remove_proc_entry(HEVC_PROC_DRM_INSTANCE_NUMBER, hevc_proc_entry);
	remove_proc_entry(HEVC_PROC_INSTANCE_NUMBER, hevc_proc_entry);
	remove_proc_entry(HEVC_PROC_ROOT, NULL);
	vb2_ion_destroy_context(dev->alloc_ctx_drm);
	hevc_mem_free_priv(dev->drm_info.alloc);
	vb2_ion_destroy_context(dev->alloc_ctx_sh);
	vb2_ion_destroy_context(dev->alloc_ctx_fw);
#endif
#ifdef CONFIG_ION_EXYNOS
	ion_client_destroy(dev->hevc_ion_client);
#endif
	hevc_mem_cleanup_multi((void **)dev->alloc_ctx,
					NUM_OF_ALLOC_CTX(dev));
	hevc_debug(2, "Will now deinit HW\n");
	hevc_deinit_hw(dev);
	free_irq(dev->irq, dev);
	iounmap(dev->regs_base);
	release_mem_region(dev->hevc_mem->start, resource_size(dev->hevc_mem));
	hevc_final_pm(dev);
	kfree(dev);
	dev_dbg(&pdev->dev, "%s--\n", __func__);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int hevc_suspend(struct device *dev)
{
	struct hevc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (!m_dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	if (m_dev->num_inst == 0)
		return 0;

	ret = hevc_sleep(m_dev);

	return ret;
}

static int hevc_resume(struct device *dev)
{
	struct hevc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (!m_dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	if (m_dev->num_inst == 0)
		return 0;

	ret = hevc_wakeup(m_dev);

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int hevc_runtime_suspend(struct device *dev)
{
	struct hevc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int pre_power;

	if (!m_dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	pre_power = atomic_read(&m_dev->pm.power);
	atomic_set(&m_dev->pm.power, 0);

	return 0;
}

static int hevc_runtime_idle(struct device *dev)
{
	return 0;
}

static int hevc_runtime_resume(struct device *dev)
{
	struct hevc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int pre_power;

	if (!m_dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	if (!m_dev->alloc_ctx)
		return 0;

	pre_power = atomic_read(&m_dev->pm.power);
	atomic_set(&m_dev->pm.power, 1);

	return 0;
}
#endif

/* Power management */
static const struct dev_pm_ops hevc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hevc_suspend, hevc_resume)
	SET_RUNTIME_PM_OPS(
			hevc_runtime_suspend,
			hevc_runtime_resume,
			hevc_runtime_idle
	)
};

struct hevc_buf_size_v6 hevc_buf_size_v6 = {
	.dev_ctx	= PAGE_ALIGN(0x7800),	/* 30KB */
	.dec_ctx	= PAGE_ALIGN(0x500000),	/* 5MB */
};

struct hevc_buf_size buf_size_v1 = {
	.firmware_code	= PAGE_ALIGN(0x300000),	/* 1MB */
	.cpb_buf	= PAGE_ALIGN(0x100000),	/* 3MB */
	.buf		= &hevc_buf_size_v6,
};

static struct hevc_variant hevc_drvdata_v1 = {
	.buf_size = &buf_size_v1,
	.buf_align = 0,
};

static struct platform_device_id hevc_driver_ids[] = {
	{
		.name = "exynos-hevc",
		.driver_data = (unsigned long)&hevc_drvdata_v1,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, hevc_driver_ids);

static const struct of_device_id exynos_hevc_match[] = {
	{
		.compatible = "samsung,hevc",
		.data = &hevc_drvdata_v1,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_hevc_match);

static void *hevc_get_drv_data(struct platform_device *pdev)
{
	struct hevc_variant *driver_data = NULL;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(of_match_ptr(exynos_hevc_match),
				pdev->dev.of_node);
		if (match)
			driver_data = (struct hevc_variant *)match->data;
	} else {
		driver_data = (struct hevc_variant *)
			platform_get_device_id(pdev)->driver_data;
	}
	return driver_data;
}

static struct platform_driver hevc_driver = {
	.probe		= hevc_probe,
	.remove		= hevc_remove,
	.id_table	= hevc_driver_ids,
	.driver	= {
		.name	= HEVC_NAME,
		.owner	= THIS_MODULE,
		.pm	= &hevc_pm_ops,
		.of_match_table = exynos_hevc_match,
	},
};


module_platform_driver(hevc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sooyoung Kang <sooyoung.kang@samsung.com>");

