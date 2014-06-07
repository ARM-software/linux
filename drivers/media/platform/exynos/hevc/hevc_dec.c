/*
 * linux/drivers/media/video/exynos/hevc/hevc_dec.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 * Sooyoung Kang, <sooyoung.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

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
#include <media/videobuf2-core.h>

#include "hevc_common.h"

#include "hevc_intr.h"
#include "hevc_mem.h"
#include "hevc_debug.h"
#include "hevc_reg.h"
#include "hevc_dec.h"
#include "hevc_pm.h"

#define DEF_SRC_FMT	2
#define DEF_DST_FMT	0

static struct hevc_fmt formats[] = {
	{
		.name = "4:2:0 3 Planes Y/Cb/Cr",
		.fourcc = V4L2_PIX_FMT_YUV420M,
		.codec_mode = HEVC_FORMATS_NO_CODEC,
		.type = HEVC_FMT_RAW,
		.num_planes = 3,
	},
	{
		.name = "4:2:0 3 Planes Y/Cr/Cb",
		.fourcc = V4L2_PIX_FMT_YVU420M,
		.codec_mode = HEVC_FORMATS_NO_CODEC,
		.type = HEVC_FMT_RAW,
		.num_planes = 3,
	},
	{
		.name = "4:2:0 2 Planes Y/CbCr",
		.fourcc = V4L2_PIX_FMT_NV12M,
		.codec_mode = HEVC_FORMATS_NO_CODEC,
		.type = HEVC_FMT_RAW,
		.num_planes = 2,
	},
	{
		.name = "4:2:0 2 Planes Y/CrCb",
		.fourcc = V4L2_PIX_FMT_NV21M,
		.codec_mode = HEVC_FORMATS_NO_CODEC,
		.type = HEVC_FMT_RAW,
		.num_planes = 2,
	},
	{
		.name = "HEVC Encoded Stream",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.codec_mode = EXYNOS_CODEC_HEVC_DEC,
		.type = HEVC_FMT_DEC,
		.num_planes = 1,
	},
};

#define NUM_FORMATS ARRAY_SIZE(formats)

/* Find selected format description */
static struct hevc_fmt *find_format(struct v4l2_format *f, unsigned int t)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (formats[i].fourcc == f->fmt.pix_mp.pixelformat &&
		    formats[i].type == t)
			return (struct hevc_fmt *)&formats[i];
	}

	return NULL;
}

static struct v4l2_queryctrl controls[] = {
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "H.264 Display Delay",
		.minimum = -1,
		.maximum = 32,
		.step = 1,
		.default_value = -1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Mpeg4 Loop Filter Enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Slice Interface Enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_PACKED_PB,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Packed PB Enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frame Tag",
		.minimum = 0,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CACHEABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Cacheable flag",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "CRC enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_LUMA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "CRC data",
		.minimum = 0,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "CRC data",
		.minimum = 0,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Display status",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TYPE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frame type",
		.minimum = 0,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Frame pack sei parse flag",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_I_FRAME_DECODING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "I frame decoding mode",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Frames per second in 1000x scale",
		.minimum = 1,
		.maximum = 300000,
		.step = 1,
		.default_value = 30000,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_DECODER_IMMEDIATE_DISPLAY,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Immediate Display Enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_DECODER_DECODING_TIMESTAMP_MODE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Decoding Timestamp Mode Enable",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_DECODER_WAIT_DECODING_START,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Wait until buffer setting done",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC_GET_VERSION_INFO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Get HEVC version information",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC_SET_DUAL_DPB_MODE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Set Dual DPB mode",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_QOS_RATIO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "QoS ratio value",
		.minimum = 20,
		.maximum = 200,
		.step = 10,
		.default_value = 100,
	},
	{
		.id = V4L2_CID_MPEG_MFC_SET_DYNAMIC_DPB_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Set dynamic DPB",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC_SET_USER_SHARED_HANDLE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Set dynamic DPB",
		.minimum = 0,
		.maximum = 65535,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC_GET_EXT_INFO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Get extra information",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
};

#define NUM_CTRLS ARRAY_SIZE(controls)

static struct v4l2_queryctrl *get_ctrl(int id)
{
	int i;

	for (i = 0; i < NUM_CTRLS; ++i)
		if (id == controls[i].id)
			return &controls[i];
	return NULL;
}

/* Check whether a ctrl value if correct */
static int check_ctrl_val(struct hevc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct hevc_dev *dev = ctx->dev;
	struct v4l2_queryctrl *c;

	c = get_ctrl(ctrl->id);
	if (!c)
		return -EINVAL;

	if (ctrl->value < c->minimum || ctrl->value > c->maximum
		|| (c->step != 0 && ctrl->value % c->step != 0)) {
		v4l2_err(&dev->v4l2_dev, "invalid control value\n");
		return -ERANGE;
	}

	return 0;
}

static struct hevc_ctrl_cfg hevc_ctrl_list[] = {
	{
		.type = HEVC_CTRL_TYPE_SET,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 1,
		.mode = HEVC_CTRL_MODE_CUSTOM,
		.addr = HEVC_SHARED_SET_FRAME_TAG,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_CUSTOM,
		.addr = HEVC_SHARED_GET_FRAME_TAG_TOP,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_SFR,
		.addr = HEVC_SI_DISPLAY_STATUS,
		.mask = 0x7,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_SRC,
		.id = V4L2_CID_MPEG_MFC51_VIDEO_CRC_GENERATED,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_SFR,
		.addr = HEVC_SI_DECODED_STATUS,
		.mask = HEVC_DEC_CRC_GEN_MASK,
		.shft = HEVC_DEC_CRC_GEN_SHIFT,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_AVAIL,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_CUSTOM,
		.addr = HEVC_FRAME_PACK_SEI_AVAIL,
		.mask = 0x1,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_ARRGMENT_ID,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_CUSTOM,
		.addr = HEVC_FRAME_PACK_ARRGMENT_ID,
		.mask = 0xFFFFFFFF,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_INFO,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_CUSTOM,
		.addr = HEVC_FRAME_PACK_SEI_INFO,
		.mask = 0x3FFFF,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
	{
		.type = HEVC_CTRL_TYPE_GET_DST,
		.id = V4L2_CID_MPEG_VIDEO_H264_SEI_FP_GRID_POS,
		.is_volatile = 0,
		.mode = HEVC_CTRL_MODE_CUSTOM,
		.addr = HEVC_FRAME_PACK_GRID_POS,
		.mask = 0xFFFF,
		.shft = 0,
		.flag_mode = HEVC_CTRL_MODE_NONE,
		.flag_addr = 0,
		.flag_shft = 0,
	},
};

#define NUM_CTRL_CFGS ARRAY_SIZE(hevc_ctrl_list)

/* Check whether a context should be run on hardware */
int hevc_dec_ctx_ready(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec = ctx->dec_priv;
	hevc_debug(2, "src=%d, dst=%d, ref=%d, state=%d capstat=%d\n",
		  ctx->src_queue_cnt, ctx->dst_queue_cnt, dec->ref_queue_cnt,
		  ctx->state, ctx->capture_state);
	hevc_debug(2, "wait_state = %d\n", ctx->wait_state);

	/* Context is to parse header */
	if (ctx->src_queue_cnt >= 1 && ctx->state == HEVCINST_GOT_INST)
		return 1;
	/* Context is to decode a frame */
	if (ctx->src_queue_cnt >= 1 &&
		ctx->state == HEVCINST_RUNNING &&
		ctx->wait_state == WAIT_NONE &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt >= ctx->dpb_count)))
		return 1;
	/* Context is to return last frame */
	if (ctx->state == HEVCINST_FINISHING &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt >= ctx->dpb_count)))
		return 1;
	/* Context is to set buffers */
	if (ctx->state == HEVCINST_HEAD_PARSED &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		(!dec->is_dynamic_dpb &&
				ctx->capture_state == QUEUE_BUFS_MMAPED)))
		return 1;
	/* Resolution change */
	if ((ctx->state == HEVCINST_RES_CHANGE_INIT ||
		ctx->state == HEVCINST_RES_CHANGE_FLUSH) &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt >= ctx->dpb_count)))
		return 1;
	if (ctx->state == HEVCINST_RES_CHANGE_END &&
		ctx->src_queue_cnt >= 1)
		return 1;

	hevc_debug(2, "hevc_dec_ctx_ready: ctx is not ready.\n");

	return 0;
}

static int dec_cleanup_ctx_ctrls(struct hevc_ctx *ctx)
{
	struct hevc_ctx_ctrl *ctx_ctrl;

	while (!list_empty(&ctx->ctrls)) {
		ctx_ctrl = list_entry((&ctx->ctrls)->next,
				      struct hevc_ctx_ctrl, list);

		hevc_debug(7, "Cleanup context control "\
				"id: 0x%08x, type: %d\n",
				ctx_ctrl->id, ctx_ctrl->type);

		list_del(&ctx_ctrl->list);
		kfree(ctx_ctrl);
	}

	INIT_LIST_HEAD(&ctx->ctrls);

	return 0;
}
static int dec_init_ctx_ctrls(struct hevc_ctx *ctx)
{
	int i;
	struct hevc_ctx_ctrl *ctx_ctrl;

	INIT_LIST_HEAD(&ctx->ctrls);

	for (i = 0; i < NUM_CTRL_CFGS; i++) {
		ctx_ctrl = kzalloc(sizeof(struct hevc_ctx_ctrl), GFP_KERNEL);
		if (ctx_ctrl == NULL) {
			hevc_err("Failed to allocate context control "\
					"id: 0x%08x, type: %d\n",
					hevc_ctrl_list[i].id,
					hevc_ctrl_list[i].type);

			dec_cleanup_ctx_ctrls(ctx);

			return -ENOMEM;
		}

		ctx_ctrl->type = hevc_ctrl_list[i].type;
		ctx_ctrl->id = hevc_ctrl_list[i].id;
		ctx_ctrl->addr = hevc_ctrl_list[i].addr;
		ctx_ctrl->has_new = 0;
		ctx_ctrl->val = 0;

		list_add_tail(&ctx_ctrl->list, &ctx->ctrls);

		hevc_debug(7, "Add context control id: 0x%08x, type : %d\n",
				ctx_ctrl->id, ctx_ctrl->type);
	}

	return 0;
}

static void __dec_reset_buf_ctrls(struct list_head *head)
{
	struct hevc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		hevc_debug(8, "Reset buffer control value "\
				"id: 0x%08x, type: %d\n",
				buf_ctrl->id, buf_ctrl->type);

		buf_ctrl->has_new = 0;
		buf_ctrl->val = 0;
		buf_ctrl->old_val = 0;
		buf_ctrl->updated = 0;
	}
}

static void __dec_cleanup_buf_ctrls(struct list_head *head)
{
	struct hevc_buf_ctrl *buf_ctrl;

	while (!list_empty(head)) {
		buf_ctrl = list_entry(head->next,
				struct hevc_buf_ctrl, list);

		hevc_debug(7, "Cleanup buffer control "\
				"id: 0x%08x, type: %d\n",
				buf_ctrl->id, buf_ctrl->type);

		list_del(&buf_ctrl->list);
		kfree(buf_ctrl);
	}

	INIT_LIST_HEAD(head);
}

static int dec_init_buf_ctrls(struct hevc_ctx *ctx,
	enum hevc_ctrl_type type, unsigned int index)
{
	int i;
	struct hevc_ctx_ctrl *ctx_ctrl;
	struct hevc_buf_ctrl *buf_ctrl;
	struct list_head *head;

	if (index >= HEVC_MAX_BUFFERS) {
		hevc_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & HEVC_CTRL_TYPE_SRC) {
		if (test_bit(index, &ctx->src_ctrls_avail)) {
			hevc_debug(7, "Source per-buffer control is already "\
					"initialized [%d]\n", index);

			__dec_reset_buf_ctrls(&ctx->src_ctrls[index]);

			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & HEVC_CTRL_TYPE_DST) {
		if (test_bit(index, &ctx->dst_ctrls_avail)) {
			hevc_debug(7, "Dest. per-buffer control is already "\
					"initialized [%d]\n", index);

			__dec_reset_buf_ctrls(&ctx->dst_ctrls[index]);

			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		hevc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	INIT_LIST_HEAD(head);

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (!(type & ctx_ctrl->type))
			continue;

		/* find matched control configuration index */
		for (i = 0; i < NUM_CTRL_CFGS; i++) {
			if (ctx_ctrl->id == hevc_ctrl_list[i].id)
				break;
		}

		if (i == NUM_CTRL_CFGS) {
			hevc_err("Failed to find buffer control "\
					"id: 0x%08x, type: %d\n",
					ctx_ctrl->id, ctx_ctrl->type);
			continue;
		}

		buf_ctrl = kzalloc(sizeof(struct hevc_buf_ctrl), GFP_KERNEL);
		if (buf_ctrl == NULL) {
			hevc_err("Failed to allocate buffer control "\
					"id: 0x%08x, type: %d\n",
					hevc_ctrl_list[i].id,
					hevc_ctrl_list[i].type);

			__dec_cleanup_buf_ctrls(head);

			return -ENOMEM;
		}

		buf_ctrl->id = ctx_ctrl->id;
		buf_ctrl->type = ctx_ctrl->type;
		buf_ctrl->addr = ctx_ctrl->addr;

		buf_ctrl->is_volatile = hevc_ctrl_list[i].is_volatile;
		buf_ctrl->mode = hevc_ctrl_list[i].mode;
		buf_ctrl->mask = hevc_ctrl_list[i].mask;
		buf_ctrl->shft = hevc_ctrl_list[i].shft;
		buf_ctrl->flag_mode = hevc_ctrl_list[i].flag_mode;
		buf_ctrl->flag_addr = hevc_ctrl_list[i].flag_addr;
		buf_ctrl->flag_shft = hevc_ctrl_list[i].flag_shft;

		list_add_tail(&buf_ctrl->list, head);

		hevc_debug(7, "Add buffer control id: 0x%08x, type : %d\n",\
				buf_ctrl->id, buf_ctrl->type);
	}

	__dec_reset_buf_ctrls(head);

	if (type & HEVC_CTRL_TYPE_SRC)
		set_bit(index, &ctx->src_ctrls_avail);
	else
		set_bit(index, &ctx->dst_ctrls_avail);

	return 0;
}

static int dec_cleanup_buf_ctrls(struct hevc_ctx *ctx,
	enum hevc_ctrl_type type, unsigned int index)
{
	struct list_head *head;

	if (index >= HEVC_MAX_BUFFERS) {
		hevc_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & HEVC_CTRL_TYPE_SRC) {
		if (!(test_and_clear_bit(index, &ctx->src_ctrls_avail))) {
			hevc_debug(7, "Source per-buffer control is "\
					"not available [%d]\n", index);
			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & HEVC_CTRL_TYPE_DST) {
		if (!(test_and_clear_bit(index, &ctx->dst_ctrls_avail))) {
			hevc_debug(7, "Dest. per-buffer Control is "\
					"not available [%d]\n", index);
			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		hevc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	__dec_cleanup_buf_ctrls(head);

	return 0;
}

static int dec_to_buf_ctrls(struct hevc_ctx *ctx, struct list_head *head)
{
	struct hevc_ctx_ctrl *ctx_ctrl;
	struct hevc_buf_ctrl *buf_ctrl;

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (!(ctx_ctrl->type & HEVC_CTRL_TYPE_SET) || !ctx_ctrl->has_new)
			continue;

		list_for_each_entry(buf_ctrl, head, list) {
			if (!(buf_ctrl->type & HEVC_CTRL_TYPE_SET))
				continue;

			if (buf_ctrl->id == ctx_ctrl->id) {
				buf_ctrl->has_new = 1;
				buf_ctrl->val = ctx_ctrl->val;
				if (buf_ctrl->is_volatile)
					buf_ctrl->updated = 0;

				ctx_ctrl->has_new = 0;
				break;
			}
		}
	}

	list_for_each_entry(buf_ctrl, head, list) {
		if (buf_ctrl->has_new)
			hevc_debug(8, "Updated buffer control "\
					"id: 0x%08x val: %d\n",
					buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int dec_to_ctx_ctrls(struct hevc_ctx *ctx, struct list_head *head)
{
	struct hevc_ctx_ctrl *ctx_ctrl;
	struct hevc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & HEVC_CTRL_TYPE_GET) || !buf_ctrl->has_new)
			continue;

		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & HEVC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == buf_ctrl->id) {
				if (ctx_ctrl->has_new)
					hevc_debug(8,
					"Overwrite context control "\
					"value id: 0x%08x, val: %d\n",
						ctx_ctrl->id, ctx_ctrl->val);

				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = buf_ctrl->val;

				buf_ctrl->has_new = 0;
			}
		}
	}

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (ctx_ctrl->has_new)
			hevc_debug(8, "Updated context control "\
					"id: 0x%08x val: %d\n",
					ctx_ctrl->id, ctx_ctrl->val);
	}

	return 0;
}

static int dec_set_buf_ctrls_val(struct hevc_ctx *ctx, struct list_head *head)
{
	struct hevc_buf_ctrl *buf_ctrl;
	struct hevc_dec *dec = ctx->dec_priv;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & HEVC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;

		/* read old vlaue */
		if (buf_ctrl->mode == HEVC_CTRL_MODE_SFR)
			value = hevc_read_reg(buf_ctrl->addr);
		else if (buf_ctrl->mode == HEVC_CTRL_MODE_SHM)
			value = hevc_read_info(ctx, buf_ctrl->addr);

		/* save old vlaue for recovery */
		if (buf_ctrl->is_volatile)
			buf_ctrl->old_val = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		/* write new value */
		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft);

		if (buf_ctrl->mode == HEVC_CTRL_MODE_SFR)
			hevc_write_reg(value, buf_ctrl->addr);
		else if (buf_ctrl->mode == HEVC_CTRL_MODE_SHM)
			hevc_write_info(ctx, value, buf_ctrl->addr);

		/* set change flag bit */
		if (buf_ctrl->flag_mode == HEVC_CTRL_MODE_SFR) {
			value = hevc_read_reg(buf_ctrl->flag_addr);
			value |= (1 << buf_ctrl->flag_shft);
			hevc_write_reg(value, buf_ctrl->flag_addr);
		} else if (buf_ctrl->flag_mode == HEVC_CTRL_MODE_SHM) {
			value = hevc_read_info(ctx, buf_ctrl->flag_addr);
			value |= (1 << buf_ctrl->flag_shft);
			hevc_write_info(ctx, value, buf_ctrl->flag_addr);
		}

		buf_ctrl->has_new = 0;
		buf_ctrl->updated = 1;

		if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG)
			dec->stored_tag = buf_ctrl->val;

		hevc_debug(8, "Set buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int dec_get_buf_ctrls_val(struct hevc_ctx *ctx, struct list_head *head)
{
	struct hevc_buf_ctrl *buf_ctrl;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & HEVC_CTRL_TYPE_GET))
			continue;

		if (buf_ctrl->mode == HEVC_CTRL_MODE_SFR)
			value = hevc_read_reg(buf_ctrl->addr);
		else if (buf_ctrl->mode == HEVC_CTRL_MODE_SHM)
			value = hevc_read_info(ctx, buf_ctrl->addr);

		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		hevc_debug(8, "Get buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int dec_recover_buf_ctrls_val(struct hevc_ctx *ctx, struct list_head *head)
{
	struct hevc_buf_ctrl *buf_ctrl;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & HEVC_CTRL_TYPE_SET)
			|| !buf_ctrl->is_volatile
			|| !buf_ctrl->updated)
			continue;

		if (buf_ctrl->mode == HEVC_CTRL_MODE_SFR)
			value = hevc_read_reg(buf_ctrl->addr);
		else if (buf_ctrl->mode == HEVC_CTRL_MODE_SHM)
			value = hevc_read_info(ctx, buf_ctrl->addr);

		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->old_val & buf_ctrl->mask) << buf_ctrl->shft);

		if (buf_ctrl->mode == HEVC_CTRL_MODE_SFR)
			hevc_write_reg(value, buf_ctrl->addr);
		else if (buf_ctrl->mode == HEVC_CTRL_MODE_SHM)
			hevc_write_info(ctx, value, buf_ctrl->addr);

		/* clear change flag bit */
		if (buf_ctrl->flag_mode == HEVC_CTRL_MODE_SFR) {
			value = hevc_read_reg(buf_ctrl->flag_addr);
			value &= ~(1 << buf_ctrl->flag_shft);
			hevc_write_reg(value, buf_ctrl->flag_addr);
		} else if (buf_ctrl->flag_mode == HEVC_CTRL_MODE_SHM) {
			value = hevc_read_info(ctx, buf_ctrl->flag_addr);
			value &= ~(1 << buf_ctrl->flag_shft);
			hevc_write_info(ctx, value, buf_ctrl->flag_addr);
		}

		hevc_debug(8, "Recover buffer control "\
				"id: 0x%08x old val: %d\n",
				buf_ctrl->id, buf_ctrl->old_val);
	}

	return 0;
}

static int dec_get_buf_update_val(struct hevc_ctx *ctx,
			struct list_head *head, unsigned int id, int value)
{
	struct hevc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if ((buf_ctrl->id == id)) {
			buf_ctrl->val = value;
			hevc_debug(5, "++id: 0x%08x val: %d\n",
					buf_ctrl->id, buf_ctrl->val);
			break;
		}
	}

	return 0;
}

static struct hevc_codec_ops decoder_codec_ops = {
	.pre_seq_start		= NULL,
	.post_seq_start		= NULL,
	.pre_frame_start	= NULL,
	.post_frame_start	= NULL,
	.init_ctx_ctrls		= dec_init_ctx_ctrls,
	.cleanup_ctx_ctrls	= dec_cleanup_ctx_ctrls,
	.init_buf_ctrls		= dec_init_buf_ctrls,
	.cleanup_buf_ctrls	= dec_cleanup_buf_ctrls,
	.to_buf_ctrls		= dec_to_buf_ctrls,
	.to_ctx_ctrls		= dec_to_ctx_ctrls,
	.set_buf_ctrls_val	= dec_set_buf_ctrls_val,
	.get_buf_ctrls_val	= dec_get_buf_ctrls_val,
	.recover_buf_ctrls_val	= dec_recover_buf_ctrls_val,
	.get_buf_update_val	= dec_get_buf_update_val,
};

/* Query capabilities of the device */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strncpy(cap->driver, "HEVC", sizeof(cap->driver) - 1);
	strncpy(cap->card, "decoder", sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE
			| V4L2_CAP_STREAMING;

	return 0;
}

/* Enumerate format */
static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool mplane, bool out)
{
	struct hevc_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (mplane && formats[i].num_planes == 1)
			continue;
		else if (!mplane && formats[i].num_planes > 1)
			continue;
		if (out && formats[i].type != HEVC_FMT_DEC)
			continue;
		else if (!out && formats[i].type != HEVC_FMT_RAW)
			continue;

		if (j == f->index)
			break;
		++j;
	}
	if (i == ARRAY_SIZE(formats))
		return -EINVAL;
	fmt = &formats[i];
	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *pirv,
							struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false, false);
}

static int vidioc_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
							struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true, false);
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *prov,
							struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false, true);
}

static int vidioc_enum_fmt_vid_out_mplane(struct file *file, void *prov,
							struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true, true);
}

/* Get format */
static int vidioc_g_fmt_vid_cap_mplane(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct hevc_raw_info *raw;
	int i;

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}
	hevc_debug_enter();
	hevc_debug(2, "f->type = %d ctx->state = %d\n", f->type, ctx->state);

	if (ctx->state == HEVCINST_VPS_PARSED_ONLY) {
		hevc_err("HEVCINST_VPS_PARSED_ONLY !!!\n");
		return -EAGAIN;
	}

	if (ctx->state == HEVCINST_GOT_INST ||
	    ctx->state == HEVCINST_RES_CHANGE_FLUSH ||
	    ctx->state == HEVCINST_RES_CHANGE_END) {
		/* If the HEVC is parsing the header,
		 * so wait until it is finished */
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_SEQ_DONE_RET)) {
			if (ctx->state == HEVCINST_VPS_PARSED_ONLY) {
				hevc_err("HEVCINST_VPS_PARSED_ONLY !!!\n");
				return -EAGAIN;
			} else {
				hevc_cleanup_timeout(ctx);
				return -EIO;
			}
		}
	}

	if (ctx->state >= HEVCINST_HEAD_PARSED &&
	    ctx->state < HEVCINST_ABORT) {
		/* This is run on CAPTURE (deocde output) */
		raw = &ctx->raw_buf;
		/* Width and height are set to the dimensions
		   of the movie, the buffer is bigger and
		   further processing stages should crop to this
		   rectangle. */
		hevc_dec_calc_dpb_size(ctx);

		pix_mp->width = ctx->img_width;
		pix_mp->height = ctx->img_height;
		pix_mp->num_planes = raw->num_planes;

		if (dec->is_interlaced)
			pix_mp->field = V4L2_FIELD_INTERLACED;
		else
			pix_mp->field = V4L2_FIELD_NONE;

		/* Set pixelformat to the format in which HEVC
		   outputs the decoded frame */
		pix_mp->pixelformat = ctx->dst_fmt->fourcc;
		for (i = 0; i < raw->num_planes; i++) {
			pix_mp->plane_fmt[i].bytesperline = raw->stride[i];
			pix_mp->plane_fmt[i].sizeimage = raw->plane_size[i];
		}
	}

	hevc_debug_leave();

	return 0;
}

static int vidioc_g_fmt_vid_out_mplane(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;

	hevc_debug_enter();

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}
	hevc_debug(2, "f->type = %d ctx->state = %d\n", f->type, ctx->state);

	/* This is run on OUTPUT
	   The buffer contains compressed image
	   so width and height have no meaning */
	pix_mp->width = 0;
	pix_mp->height = 0;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->plane_fmt[0].bytesperline = dec->src_buf_size;
	pix_mp->plane_fmt[0].sizeimage = dec->src_buf_size;
	pix_mp->pixelformat = ctx->src_fmt->fourcc;
	pix_mp->num_planes = ctx->src_fmt->num_planes;

	hevc_debug_leave();

	return 0;
}

/* Try format */
static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct hevc_dev *dev = video_drvdata(file);
	struct hevc_fmt *fmt;

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	hevc_debug(2, "Type is %d\n", f->type);
	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = find_format(f, HEVC_FMT_DEC);
		if (!fmt) {
			hevc_err("Unsupported format for source.\n");
			return -EINVAL;
		}

	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = find_format(f, HEVC_FMT_RAW);
		if (!fmt) {
			hevc_err("Unsupported format for destination.\n");
			return -EINVAL;
		}

	}

	return 0;
}

/* Set format */
static int vidioc_s_fmt_vid_cap_mplane(struct file *file, void *priv,
							struct v4l2_format *f)
{
	struct hevc_dev *dev = video_drvdata(file);
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	int ret = 0;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	if (ctx->vq_dst.streaming) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	ctx->dst_fmt = find_format(f, HEVC_FMT_RAW);
	if (!ctx->dst_fmt) {
		hevc_err("Unsupported format for destination.\n");
		return -EINVAL;
	}
	ctx->raw_buf.num_planes = ctx->dst_fmt->num_planes;

	hevc_debug_leave();

	return 0;
}

static int vidioc_s_fmt_vid_out_mplane(struct file *file, void *priv,
							struct v4l2_format *f)
{
	struct hevc_dev *dev = video_drvdata(file);
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec;
	int ret = 0;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	if (ctx->vq_src.streaming) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	ctx->src_fmt = find_format(f, HEVC_FMT_DEC);
	ctx->codec_mode = ctx->src_fmt->codec_mode;
	hevc_info("The codec number is: %d\n", ctx->codec_mode);
	ctx->pix_format = pix_mp->pixelformat;
	if ((pix_mp->width > 0) && (pix_mp->height > 0)) {
		ctx->img_height = pix_mp->height;
		ctx->img_width = pix_mp->width;
	}
	/* As this buffer will contain compressed data, the size is set
	 * to the maximum size. */
	if (pix_mp->plane_fmt[0].sizeimage)
		dec->src_buf_size = pix_mp->plane_fmt[0].sizeimage;
	else
		dec->src_buf_size = MAX_FRAME_SIZE;
	hevc_info("sizeimage: %d\n", pix_mp->plane_fmt[0].sizeimage);
	pix_mp->plane_fmt[0].bytesperline = 0;

	/* In case of calling s_fmt twice or more */
	if (ctx->inst_no != HEVC_NO_INSTANCE_SET) {
		ctx->state = HEVCINST_RETURN_INST;
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		hevc_try_run(dev);
		/* Wait until instance is returned or timeout occured */
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_CLOSE_INSTANCE_RET)) {
			hevc_cleanup_timeout(ctx);
			return -EIO;
		}
		/* Free resources */
		hevc_release_instance_buffer(ctx);

		ctx->state = HEVCINST_INIT;
	}

	hevc_alloc_instance_buffer(ctx);

	spin_lock_irq(&dev->condlock);
	set_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);
	hevc_try_run(dev);
	if (hevc_wait_for_done_ctx(ctx,
			HEVC_R2H_CMD_OPEN_INSTANCE_RET)) {
		hevc_cleanup_timeout(ctx);
		hevc_release_instance_buffer(ctx);
		return -EIO;
	}
	hevc_debug(2, "Got instance number: %d\n", ctx->inst_no);

	hevc_debug_leave();

	return 0;
}

/* Reqeust buffers */
static int vidioc_reqbufs(struct file *file, void *priv,
		struct v4l2_requestbuffers *reqbufs)
{
	struct hevc_dev *dev = video_drvdata(file);
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec;
	int ret = 0;
	void *alloc_ctx;

	hevc_debug_enter();
	hevc_info("Memory type: %d\n", reqbufs->memory);

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (ctx->is_drm)
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		else
			alloc_ctx =
				ctx->dev->alloc_ctx[HEVC_BANK_A_ALLOC_CTX];

		/* Can only request buffers after
		   an instance has been opened.*/
		if ((ctx->state == HEVCINST_GOT_INST) ||
			(ctx->state == HEVCINST_VPS_PARSED_ONLY)) {
			if (reqbufs->count == 0) {
				hevc_info("Freeing buffers.\n");
				ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
				ctx->output_state = QUEUE_FREE;
				return ret;
			}

			/* Decoding */
			if (ctx->output_state != QUEUE_FREE) {
				hevc_err("Bufs have already been requested.\n");
				return -EINVAL;
			}

			if (ctx->cacheable & HEVCMASK_SRC_CACHE)
				hevc_mem_set_cacheable(alloc_ctx, true);

			ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
			if (ret) {
				hevc_err("vb2_reqbufs on output failed.\n");
				hevc_mem_set_cacheable(alloc_ctx, false);
				return ret;
			}

			hevc_mem_set_cacheable(alloc_ctx, false);
			ctx->output_state = QUEUE_BUFS_REQUESTED;
		}
	} else if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (reqbufs->count == 0) {
			hevc_info("Freeing buffers.\n");
			ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
			hevc_release_codec_buffers(ctx);
			dec->dpb_queue_cnt = 0;
			ctx->capture_state = QUEUE_FREE;
			return ret;
		}

		dec->dst_memtype = reqbufs->memory;

		if (ctx->is_drm)
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		else
			alloc_ctx = ctx->dev->alloc_ctx[HEVC_BANK_A_ALLOC_CTX];

		if (ctx->capture_state != QUEUE_FREE) {
			hevc_err("Bufs have already been requested.\n");
			return -EINVAL;
		}

		if (ctx->cacheable & HEVCMASK_DST_CACHE)
			hevc_mem_set_cacheable(alloc_ctx, true);

		ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
		if (ret) {
			hevc_err("vb2_reqbufs on capture failed.\n");
			hevc_mem_set_cacheable(alloc_ctx, false);
			return ret;
		}

		if (reqbufs->count < ctx->dpb_count) {
			hevc_err("Not enough buffers allocated.\n");
			reqbufs->count = 0;
			vb2_reqbufs(&ctx->vq_dst, reqbufs);
			hevc_mem_set_cacheable(alloc_ctx, false);
			return -ENOMEM;
		}

		hevc_mem_set_cacheable(alloc_ctx, false);
		ctx->capture_state = QUEUE_BUFS_REQUESTED;

		dec->total_dpb_count = reqbufs->count;

		ret = hevc_alloc_codec_buffers(ctx);
		if (ret) {
			hevc_err("Failed to allocate decoding buffers.\n");
			reqbufs->count = 0;
			vb2_reqbufs(&ctx->vq_dst, reqbufs);
			return -ENOMEM;
		}

		if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
			if (dec->dpb_queue_cnt == dec->total_dpb_count) {
				ctx->capture_state = QUEUE_BUFS_MMAPED;
			} else {
				hevc_err("Not all buffers passed to buf_init.\n");
				reqbufs->count = 0;
				vb2_reqbufs(&ctx->vq_dst, reqbufs);
				hevc_release_codec_buffers(ctx);
				return -ENOMEM;
			}
		}

		if (hevc_dec_ctx_ready(ctx)) {
			spin_lock_irq(&dev->condlock);
			set_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}

		hevc_try_run(dev);

		if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
			if (hevc_wait_for_done_ctx(ctx,
					HEVC_R2H_CMD_INIT_BUFFERS_RET)) {
				hevc_cleanup_timeout(ctx);
				return -EIO;
			}
		}
	}

	hevc_debug_leave();

	return ret;
}

/* Query buffer */
static int vidioc_querybuf(struct file *file, void *priv,
						   struct v4l2_buffer *buf)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	int ret;
	int i;

	hevc_debug_enter();

	if (buf->memory != V4L2_MEMORY_MMAP) {
		hevc_err("Only mmaped buffers can be used.\n");
		return -EINVAL;
	}

	hevc_debug(2, "State: %d, buf->type: %d\n", ctx->state, buf->type);
	if (ctx->state == HEVCINST_GOT_INST &&
			buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_querybuf(&ctx->vq_src, buf);
	} else if (ctx->state == HEVCINST_RUNNING &&
			buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = vb2_querybuf(&ctx->vq_dst, buf);
		for (i = 0; i < buf->length; i++)
			buf->m.planes[i].m.mem_offset += DST_QUEUE_OFF_BASE;
	} else {
		hevc_err("vidioc_querybuf called in an inappropriate state.\n");
		ret = -EINVAL;
	}
	hevc_debug_leave();
	return ret;
}

/* Queue a buffer */
static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec = ctx->dec_priv;
	int ret = -EINVAL;

	hevc_debug_enter();

	hevc_debug(2, "Enqueued buf: %d, (type = %d)\n", buf->index, buf->type);
	if (ctx->state == HEVCINST_ERROR) {
		hevc_err("Call on QBUF after unrecoverable error.\n");
		return -EIO;
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (buf->m.planes[0].bytesused > ctx->vq_src.plane_sizes[0]) {
			hevc_err("data size (%d) must be less than "
					"plane size(%d)\n",
					buf->m.planes[0].bytesused,
					ctx->vq_src.plane_sizes[0]);
			return -EIO;
		}

		if (dec->is_dts_mode) {
			hevc_debug(7, "timestamp: %ld %ld\n",
					buf->timestamp.tv_sec,
					buf->timestamp.tv_usec);
			hevc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
			ctx->last_framerate = (ctx->qos_ratio *
						hevc_get_framerate(&buf->timestamp,
						&ctx->last_timestamp)) / 100;

			memcpy(&ctx->last_timestamp, &buf->timestamp,
				sizeof(struct timeval));
		}

		if (ctx->last_framerate != 0 &&
				ctx->last_framerate != ctx->framerate) {
			hevc_info("fps changed: %d -> %d\n",
					ctx->framerate, ctx->last_framerate);
			ctx->framerate = ctx->last_framerate;
			hevc_qos_on(ctx);
		}
		ret = vb2_qbuf(&ctx->vq_src, buf);
	} else {
		ret = vb2_qbuf(&ctx->vq_dst, buf);
		hevc_debug(2, "End of enqueue(%d) : %d\n", buf->index, ret);
	}

	hevc_debug_leave();
	return ret;
}

/* Dequeue a buffer */
static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec = ctx->dec_priv;
	struct dec_dpb_ref_info *dstBuf, *srcBuf;
	int ret;
	int ncount = 0;

	hevc_debug_enter();
	hevc_debug(2, "Addr: %p %p %p Type: %d\n", &ctx->vq_src, buf, buf->m.planes,
								buf->type);
	if (ctx->state == HEVCINST_ERROR) {
		hevc_err("Call on DQBUF after unrecoverable error.\n");
		return -EIO;
	}
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_dqbuf(&ctx->vq_src, buf, file->f_flags & O_NONBLOCK);
	} else {
		ret = vb2_dqbuf(&ctx->vq_dst, buf, file->f_flags & O_NONBLOCK);
		/* Memcpy from dec->ref_info to shared memory */
		srcBuf = &dec->ref_info[buf->index];
		for (ncount = 0; ncount < HEVC_MAX_DPBS; ncount++) {
			if (srcBuf->dpb[ncount].fd[0] == HEVC_INFO_INIT_FD)
				break;
			hevc_debug(2, "DQ index[%d] Released FD = %d\n",
					buf->index, srcBuf->dpb[ncount].fd[0]);
		}

		if ((dec->is_dynamic_dpb) && (dec->sh_handle.virt != NULL)) {
			dstBuf = (struct dec_dpb_ref_info *)
					dec->sh_handle.virt + buf->index;
			memcpy(dstBuf, srcBuf, sizeof(struct dec_dpb_ref_info));
			dstBuf->index = buf->index;
		}
	}
	hevc_debug_leave();
	return ret;
}

/* Stream on */
static int vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	int ret = -EINVAL;

	hevc_debug_enter();

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_streamon(&ctx->vq_src, type);
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = vb2_streamon(&ctx->vq_dst, type);

		if (!ret)
			hevc_qos_on(ctx);
	} else {
		hevc_err("unknown v4l2 buffer type\n");
	}

	hevc_info("ctx->src_queue_cnt = %d ctx->state = %d "
		  "ctx->dst_queue_cnt = %d ctx->dpb_count = %d\n",
		  ctx->src_queue_cnt, ctx->state, ctx->dst_queue_cnt,
		  ctx->dpb_count);

	hevc_debug_leave();

	return ret;
}

/* Stream off, which equals to a pause */
static int vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	int ret = -EINVAL;

	hevc_debug_enter();

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->last_framerate = 0;
		memset(&ctx->last_timestamp, 0, sizeof(struct timeval));
		ret = vb2_streamoff(&ctx->vq_src, type);
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		hevc_qos_off(ctx);
		ret = vb2_streamoff(&ctx->vq_dst, type);
	} else {
		hevc_err("unknown v4l2 buffer type\n");
	}

	hevc_info("streamoff\n");
	hevc_debug_leave();

	return ret;
}

/* Query a ctrl */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	struct v4l2_queryctrl *c;

	c = get_ctrl(qc->id);
	if (!c)
		return -EINVAL;
	*qc = *c;
	return 0;
}

static int dec_ext_info(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	int val = 0;

	if (FW_HAS_DYNAMIC_DPB(dev))
		val |= DEC_SET_DYNAMIC_DPB;

	return val;
}

/* Get ctrl */
static int get_ctrl_val(struct hevc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	struct hevc_ctx_ctrl *ctx_ctrl;
	int found = 0;

	hevc_debug_enter();
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
		ctrl->value = dec->loop_filter_mpeg4;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY:
		ctrl->value = dec->display_delay;
		break;
	case V4L2_CID_CACHEABLE:
		ctrl->value = ctx->cacheable;
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= HEVCINST_HEAD_PARSED &&
		    ctx->state < HEVCINST_ABORT) {
			ctrl->value = ctx->dpb_count;
			break;
		} else if (ctx->state != HEVCINST_INIT) {
			v4l2_err(&dev->v4l2_dev, "Decoding not initialised.\n");
			return -EINVAL;
		}

		/* Should wait for the header to be parsed */
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_SEQ_DONE_RET)) {
			hevc_cleanup_timeout(ctx);
			return -EIO;
		}

		if (ctx->state >= HEVCINST_HEAD_PARSED &&
		    ctx->state < HEVCINST_ABORT) {
			ctrl->value = ctx->dpb_count;
		} else {
			v4l2_err(&dev->v4l2_dev,
					 "Decoding not initialised.\n");
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE:
		ctrl->value = dec->slice_enable;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PACKED_PB:
		ctrl->value = dec->is_packedpb;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CRC_ENABLE:
		ctrl->value = dec->crc_enable;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CHECK_STATE:
		if (ctx->is_dpb_realloc && ctx->state == HEVCINST_HEAD_PARSED)
			ctrl->value = HEVCSTATE_DEC_S3D_REALLOC;
		else if (ctx->state == HEVCINST_RES_CHANGE_FLUSH
				|| ctx->state == HEVCINST_RES_CHANGE_END
				|| ctx->state == HEVCINST_HEAD_PARSED)
			ctrl->value = HEVCSTATE_DEC_RES_DETECT;
		else if (ctx->state == HEVCINST_FINISHING)
			ctrl->value = HEVCSTATE_DEC_TERMINATING;
		else
			ctrl->value = HEVCSTATE_PROCESSING;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
		ctrl->value = dec->sei_parse;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_I_FRAME_DECODING:
		ctrl->value = dec->idr_decoding;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE:
		ctrl->value = ctx->framerate;
		break;
	case V4L2_CID_MPEG_MFC_GET_VERSION_INFO:
		ctrl->value = hevc_version(dev);
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctrl->value = ctx->qos_ratio;
		break;
	case V4L2_CID_MPEG_MFC_SET_DYNAMIC_DPB_MODE:
		ctrl->value = dec->is_dynamic_dpb;
		break;
	case V4L2_CID_MPEG_MFC_GET_EXT_INFO:
		ctrl->value = dec_ext_info(ctx);
		break;
	default:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & HEVC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				if (ctx_ctrl->has_new) {
					ctx_ctrl->has_new = 0;
					ctrl->value = ctx_ctrl->val;
				} else {
					hevc_debug(8, "Control value "\
							"is not up to date: "\
							"0x%08x\n", ctrl->id);
					return -EINVAL;
				}

				found = 1;
				break;
			}
		}

		if (!found) {
			v4l2_err(&dev->v4l2_dev, "Invalid control 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	}

	hevc_debug_leave();

	return 0;
}

/* Get a ctrl */
static int vidioc_g_ctrl(struct file *file, void *priv,
			struct v4l2_control *ctrl)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	int ret = 0;

	hevc_debug_enter();
	ret = get_ctrl_val(ctx, ctrl);
	hevc_debug_leave();

	return ret;
}

static int process_user_shared_handle(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_dec *dec = ctx->dec_priv;
	int ret = 0;

	dec->sh_handle.ion_handle =
		ion_import_dma_buf(dev->hevc_ion_client, dec->sh_handle.fd);
	if (IS_ERR(dec->sh_handle.ion_handle)) {
		hevc_err("Failed to import fd\n");
		ret = PTR_ERR(dec->sh_handle.ion_handle);
		goto import_dma_fail;
	}

	dec->sh_handle.virt =
		ion_map_kernel(dev->hevc_ion_client, dec->sh_handle.ion_handle);
	if (dec->sh_handle.virt == NULL) {
		hevc_err("Failed to get kernel virtual address\n");
		ret = -EINVAL;
		goto map_kernel_fail;
	}

	hevc_debug(2, "User Handle: fd = %d, virt = 0x%x\n",
				dec->sh_handle.fd, (int)dec->sh_handle.virt);

	return 0;

map_kernel_fail:
	ion_free(dev->hevc_ion_client, dec->sh_handle.ion_handle);

import_dma_fail:
	return ret;
}

int hevc_dec_cleanup_user_shared_handle(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_dec *dec = ctx->dec_priv;

	if (dec->sh_handle.fd == -1)
		return 0;

	if (dec->sh_handle.virt)
		ion_unmap_kernel(dev->hevc_ion_client,
					dec->sh_handle.ion_handle);

	ion_free(dev->hevc_ion_client, dec->sh_handle.ion_handle);

	return 0;
}

/* Set a ctrl */
static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct hevc_dev *dev = video_drvdata(file);
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec;
	struct hevc_ctx_ctrl *ctx_ctrl;
	int ret = 0;
	int found = 0;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	ret = check_ctrl_val(ctx, ctrl);
	if (ret)
		return ret;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
		dec->loop_filter_mpeg4 = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY:
		dec->display_delay = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE:
		dec->slice_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PACKED_PB:
		dec->is_packedpb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CRC_ENABLE:
		dec->crc_enable = ctrl->value;
		break;
	case V4L2_CID_CACHEABLE:
		if (ctrl->value)
			ctx->cacheable |= ctrl->value;
		else
			ctx->cacheable = 0;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
		dec->sei_parse = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_I_FRAME_DECODING:
		dec->idr_decoding = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE:
		if (ctx->framerate != ctrl->value) {
			ctx->framerate = ctrl->value;
			hevc_qos_on(ctx);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_IMMEDIATE_DISPLAY:
		dec->immediate_display = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_DECODING_TIMESTAMP_MODE:
		dec->is_dts_mode = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_WAIT_DECODING_START:
		ctx->wait_state = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctx->qos_ratio = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_SET_DYNAMIC_DPB_MODE:
		if (FW_HAS_DYNAMIC_DPB(dev))
			dec->is_dynamic_dpb = ctrl->value;
		else
			dec->is_dynamic_dpb = 0;
		break;
	case V4L2_CID_MPEG_MFC_SET_USER_SHARED_HANDLE:
		dec->sh_handle.fd = ctrl->value;
		if (process_user_shared_handle(ctx)) {
			dec->sh_handle.fd = -1;
			return -EINVAL;
		}
		break;
	default:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & HEVC_CTRL_TYPE_SET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = ctrl->value;

				found = 1;
				break;
			}
		}

		if (!found) {
			v4l2_err(&dev->v4l2_dev, "Invalid control 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	}

	hevc_debug_leave();

	return 0;
}

void hevc_dec_store_crop_info(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec = ctx->dec_priv;
	u32 left, right, top, bottom;

	left = hevc_read_info(ctx, CROP_INFO_H);
	right = left >> HEVC_SHARED_CROP_RIGHT_SHIFT;
	left = left & HEVC_SHARED_CROP_LEFT_MASK;
	top = hevc_read_info(ctx, CROP_INFO_V);
	bottom = top >> HEVC_SHARED_CROP_BOTTOM_SHIFT;
	top = top & HEVC_SHARED_CROP_TOP_MASK;

	dec->cr_left = left;
	dec->cr_right = right;
	dec->cr_top = top;
	dec->cr_bot = bottom;
}

#define ready_to_get_crop(ctx)			\
	((ctx->state == HEVCINST_HEAD_PARSED) ||	\
	 (ctx->state == HEVCINST_RUNNING) ||	\
	 (ctx->state == HEVCINST_FINISHING) ||	\
	 (ctx->state == HEVCINST_FINISHED))
/* Get cropping information */
static int vidioc_g_crop(struct file *file, void *priv,
		struct v4l2_crop *cr)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct hevc_dec *dec = ctx->dec_priv;

	hevc_debug_enter();

	if (!ready_to_get_crop(ctx)) {
		hevc_debug(2, "ready to get crop failed\n");
		return -EINVAL;
	}

	if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_HEVC) {
		cr->c.left = dec->cr_left;
		cr->c.top = dec->cr_top;
		cr->c.width = ctx->img_width - dec->cr_left - dec->cr_right;
		cr->c.height = ctx->img_height - dec->cr_top - dec->cr_bot;
		hevc_debug(2, "Cropping info [h264]: l=%d t=%d "	\
			"w=%d h=%d (r=%d b=%d fw=%d fh=%d\n",
			dec->cr_left, dec->cr_top, cr->c.width, cr->c.height,
			dec->cr_right, dec->cr_bot,
			ctx->buf_width, ctx->buf_height);
	} else {
		cr->c.left = 0;
		cr->c.top = 0;
		cr->c.width = ctx->img_width;
		cr->c.height = ctx->img_height;
		hevc_debug(2, "Cropping info: w=%d h=%d fw=%d "
			"fh=%d\n", cr->c.width,	cr->c.height, ctx->buf_width,
							ctx->buf_height);
	}
	hevc_debug_leave();
	return 0;
}

static int vidioc_g_ext_ctrls(struct file *file, void *priv,
			struct v4l2_ext_controls *f)
{
	struct hevc_ctx *ctx = fh_to_hevc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;

		ret = get_ctrl_val(ctx, &ctrl);
		if (ret == 0) {
			ext_ctrl->value = ctrl.value;
		} else {
			f->error_idx = i;
			break;
		}

		hevc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

/* v4l2_ioctl_ops */
static const struct v4l2_ioctl_ops hevc_dec_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_out_mplane = vidioc_g_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane = vidioc_s_fmt_vid_out_mplane,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_g_ext_ctrls = vidioc_g_ext_ctrls,
};

static int hevc_queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *fmt,
				unsigned int *buf_count, unsigned int *plane_count,
				unsigned int psize[], void *allocators[])
{
	struct hevc_ctx *ctx;
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	struct hevc_raw_info *raw;
	void *alloc_ctx1;
	void *alloc_ctx2;
	void *capture_ctx;
	int i;

	hevc_debug_enter();

	if (!vq) {
		hevc_err("no vb2_queue info\n");
		return -EINVAL;
	}

	ctx = vq->drv_priv;
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	raw = &ctx->raw_buf;
	alloc_ctx1 = ctx->dev->alloc_ctx[HEVC_BANK_A_ALLOC_CTX];
	alloc_ctx2 = ctx->dev->alloc_ctx[HEVC_BANK_B_ALLOC_CTX];

	/* Video output for decoding (source)
	 * this can be set after getting an instance */
	if (ctx->state == HEVCINST_GOT_INST &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		hevc_debug(2, "setting for VIDEO output\n");
		/* A single plane is required for input */
		*plane_count = 1;
		if (*buf_count < 1)
			*buf_count = 1;
		if (*buf_count > HEVC_MAX_BUFFERS)
			*buf_count = HEVC_MAX_BUFFERS;
	/* Video capture for decoding (destination)
	 * this can be set after the header was parsed */
	} else if (ctx->state == HEVCINST_HEAD_PARSED &&
		   vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		hevc_debug(2, "setting for VIDEO capture\n");
		/* Output plane count is different by the pixel format */
		*plane_count = raw->num_planes;
		/* Setup buffer count */
		if (*buf_count < ctx->dpb_count)
			*buf_count = ctx->dpb_count;
		if (*buf_count > HEVC_MAX_BUFFERS)
			*buf_count = HEVC_MAX_BUFFERS;
	} else {
		hevc_err("State seems invalid. State = %d, vq->type = %d\n",
							ctx->state, vq->type);
		return -EINVAL;
	}
	hevc_debug(2, "buffer count=%d, plane count=%d type=0x%x\n",
					*buf_count, *plane_count, vq->type);

	if (ctx->state == HEVCINST_HEAD_PARSED &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->is_drm)
			capture_ctx = ctx->dev->alloc_ctx_drm;
		else
			capture_ctx = alloc_ctx1;

		for (i = 0; i < raw->num_planes; i++) {
			psize[i] = raw->plane_size[i];
			allocators[i] = capture_ctx;
		}

	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
		   ctx->state == HEVCINST_GOT_INST) {
		psize[0] = dec->src_buf_size;

		if (ctx->is_drm)
			allocators[0] = ctx->dev->alloc_ctx_drm;
		else
			allocators[0] = alloc_ctx1;

	} else {
		hevc_err("Currently only decoding is supported. Decoding not initalised.\n");
		return -EINVAL;
	}

	hevc_debug(2, "plane=0, size=%d\n", psize[0]);
	hevc_debug(2, "plane=1, size=%d\n", psize[1]);

	hevc_debug_leave();

	return 0;
}

static void hevc_unlock(struct vb2_queue *q)
{
	struct hevc_ctx *ctx = q->drv_priv;
	struct hevc_dev *dev;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	mutex_unlock(&dev->hevc_mutex);
}

static void hevc_lock(struct vb2_queue *q)
{
	struct hevc_ctx *ctx = q->drv_priv;
	struct hevc_dev *dev;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	mutex_lock(&dev->hevc_mutex);
}

static int hevc_buf_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct hevc_ctx *ctx = vq->drv_priv;
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	struct hevc_buf *buf = vb_to_hevc_buf(vb);
	int i;
	unsigned long flags;

	hevc_debug_enter();
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (!dec->is_dynamic_dpb &&
				(ctx->capture_state == QUEUE_BUFS_MMAPED)) {
			hevc_debug_leave();
			return 0;
		}
		for (i = 0; i < ctx->dst_fmt->num_planes; i++) {
			if (hevc_mem_plane_addr(ctx, vb, i) == 0) {
				hevc_err("Plane mem not allocated.\n");
				return -EINVAL;
			}
			buf->planes.raw[i] = hevc_mem_plane_addr(ctx, vb, i);
		}

		spin_lock_irqsave(&dev->irqlock, flags);
		list_add_tail(&buf->list, &dec->dpb_queue);
		dec->dpb_queue_cnt++;
		spin_unlock_irqrestore(&dev->irqlock, flags);

		if (call_cop(ctx, init_buf_ctrls, ctx, HEVC_CTRL_TYPE_DST,
					vb->v4l2_buf.index) < 0)
			hevc_err("failed in init_buf_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (hevc_mem_plane_addr(ctx, vb, 0) == 0) {
			hevc_err("Plane memory not allocated.\n");
			return -EINVAL;
		}
		buf->planes.stream = hevc_mem_plane_addr(ctx, vb, 0);

		if (call_cop(ctx, init_buf_ctrls, ctx, HEVC_CTRL_TYPE_SRC,
					vb->v4l2_buf.index) < 0)
			hevc_err("failed in init_buf_ctrls\n");
	} else {
		hevc_err("hevc_buf_init: unknown queue type.\n");
		return -EINVAL;
	}

	hevc_debug_leave();

	return 0;
}

static int hevc_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct hevc_ctx *ctx = vq->drv_priv;
	struct hevc_dec *dec;
	struct hevc_raw_info *raw;
	unsigned int index = vb->v4l2_buf.index;
	int i;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		raw = &ctx->raw_buf;
		for (i = 0; i < raw->num_planes; i++) {
			if (vb2_plane_size(vb, i) < raw->plane_size[i]) {
				hevc_err("Capture plane[%d] is too small\n", i);
				hevc_err("%lu is smaller than %d\n",
						vb2_plane_size(vb, i),
						raw->plane_size[i]);
				return -EINVAL;
			} else {
				hevc_debug(2, "Plane[%d] size = %lu\n",
						i, vb2_plane_size(vb, i));
			}
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		hevc_debug(2, "Plane size: %ld, ctx->dec_src_buf_size: %d\n",
				vb2_plane_size(vb, 0), dec->src_buf_size);

		if (vb2_plane_size(vb, 0) < dec->src_buf_size) {
			hevc_err("Plane buffer (OUTPUT) is too small.\n");
			return -EINVAL;
		}

		if (call_cop(ctx, to_buf_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			hevc_err("failed in to_buf_ctrls\n");
	}

	hevc_mem_prepare(vb);

	return 0;
}

static int hevc_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct hevc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (call_cop(ctx, to_ctx_ctrls, ctx, &ctx->dst_ctrls[index]) < 0)
			hevc_err("failed in to_ctx_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (call_cop(ctx, to_ctx_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			hevc_err("failed in to_ctx_ctrls\n");
	}

	hevc_mem_finish(vb);

	return 0;
}

static void hevc_buf_cleanup(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct hevc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;

	hevc_debug_enter();
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (call_cop(ctx, cleanup_buf_ctrls, ctx,
					HEVC_CTRL_TYPE_DST, index) < 0)
			hevc_err("failed in cleanup_buf_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (call_cop(ctx, cleanup_buf_ctrls, ctx,
					HEVC_CTRL_TYPE_SRC, index) < 0)
			hevc_err("failed in cleanup_buf_ctrls\n");
	} else {
		hevc_err("hevc_buf_cleanup: unknown queue type.\n");
	}

	hevc_debug_leave();
}

static int hevc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct hevc_ctx *ctx = q->drv_priv;
	struct hevc_dev *dev;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	if (ctx->state == HEVCINST_FINISHING || ctx->state == HEVCINST_FINISHED)
		ctx->state = HEVCINST_RUNNING;

	/* If context is ready then dev = work->data;schedule it to run */
	if (hevc_dec_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}

	hevc_try_run(dev);

	return 0;
}

static void cleanup_ref_queue(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec = ctx->dec_priv;
	struct hevc_buf *ref_buf;
	dma_addr_t ref_addr;
	int i;

	/* move buffers in ref queue to src queue */
	while (!list_empty(&dec->ref_queue)) {
		ref_buf = list_entry((&dec->ref_queue)->next,
						struct hevc_buf, list);

		for (i = 0; i < 2; i++) {
			ref_addr = hevc_mem_plane_addr(ctx, &ref_buf->vb, i);
			hevc_debug(2, "dec ref[%d] addr: 0x%08lx", i,
						(unsigned long)ref_addr);
		}

		list_del(&ref_buf->list);
		dec->ref_queue_cnt--;
	}

	hevc_debug(2, "Dec ref-count: %d\n", dec->ref_queue_cnt);

	BUG_ON(dec->ref_queue_cnt);
}

static void cleanup_assigned_fd(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec;
	int i;

	dec = ctx->dec_priv;

	for (i = 0; i < HEVC_MAX_DPBS; i++)
		dec->assigned_fd[i] = HEVC_INFO_INIT_FD;
}

#define need_to_wait_frame_start(ctx)		\
	(((ctx->state == HEVCINST_FINISHING) ||	\
	  (ctx->state == HEVCINST_RUNNING)) &&	\
	 test_bit(ctx->num, &ctx->dev->hw_lock))
#define need_to_dpb_flush(ctx)		\
	((ctx->state == HEVCINST_FINISHING) ||	\
	  (ctx->state == HEVCINST_RUNNING))
static int hevc_stop_streaming(struct vb2_queue *q)
{
	unsigned long flags;
	struct hevc_ctx *ctx = q->drv_priv;
	struct hevc_dec *dec;
	struct hevc_dev *dev;
	int aborted = 0;
	int index = 0;
	int prev_state;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		hevc_err("no hevc decoder to run\n");
		return -EINVAL;
	}

	if (need_to_wait_frame_start(ctx)) {
		ctx->state = HEVCINST_ABORT;
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_FRAME_DONE_RET))
			hevc_cleanup_timeout(ctx);

		aborted = 1;
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (dec->is_dynamic_dpb) {
			cleanup_assigned_fd(ctx);
			cleanup_ref_queue(ctx);
			dec->dynamic_used = 0;
		}

		hevc_cleanup_queue(&ctx->dst_queue, &ctx->vq_dst);
		INIT_LIST_HEAD(&ctx->dst_queue);
		ctx->dst_queue_cnt = 0;
		ctx->is_dpb_realloc = 0;
		dec->dpb_flush = 1;
		dec->dpb_status = 0;

		INIT_LIST_HEAD(&dec->dpb_queue);
		dec->dpb_queue_cnt = 0;

		while (index < HEVC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->dst_ctrls_avail,
					HEVC_MAX_BUFFERS, index);
			if (index < HEVC_MAX_BUFFERS)
				__dec_reset_buf_ctrls(&ctx->dst_ctrls[index]);
			index++;
		}
		if (ctx->wait_state == WAIT_INITBUF_DONE) {
			ctx->wait_state = WAIT_NONE;
			hevc_debug(2, "Decoding can be started now\n");
		}
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		hevc_cleanup_queue(&ctx->src_queue, &ctx->vq_src);
		INIT_LIST_HEAD(&ctx->src_queue);
		ctx->src_queue_cnt = 0;

		while (index < HEVC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->src_ctrls_avail,
					HEVC_MAX_BUFFERS, index);
			if (index < HEVC_MAX_BUFFERS)
				__dec_reset_buf_ctrls(&ctx->src_ctrls[index]);
			index++;
		}
	}

	if (aborted)
		ctx->state = HEVCINST_RUNNING;

	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
			need_to_dpb_flush(ctx)) {
		prev_state = ctx->state;
		ctx->state = HEVCINST_DPB_FLUSHING;
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		hevc_try_run(dev);
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_DPB_FLUSH_RET))
			hevc_cleanup_timeout(ctx);
		ctx->state = prev_state;
	}
	return 0;
}


static void hevc_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct hevc_ctx *ctx = vq->drv_priv;
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	unsigned long flags;
	struct hevc_buf *buf = vb_to_hevc_buf(vb);
	struct hevc_buf *dpb_buf, *tmp_buf;
	int wait_flag = 0;
	int remove_flag = 0;
	int index;
	int skip_add = 0;

	hevc_debug_enter();
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

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		buf->used = 0;
		hevc_debug(2, "Src queue: %p\n", &ctx->src_queue);
		hevc_debug(2, "Adding to src: %p (0x%08lx, 0x%08lx)\n", vb,
			(unsigned long)hevc_mem_plane_addr(ctx, vb, 0),
			(unsigned long)buf->planes.stream);
		spin_lock_irqsave(&dev->irqlock, flags);
		list_add_tail(&buf->list, &ctx->src_queue);
		ctx->src_queue_cnt++;
		spin_unlock_irqrestore(&dev->irqlock, flags);
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = 0;
		index = vb->v4l2_buf.index;
		hevc_debug(2, "Dst queue: %p\n", &ctx->dst_queue);
		hevc_debug(2, "Adding to dst: %p (0x%08lx)\n", vb,
			(unsigned long)hevc_mem_plane_addr(ctx, vb, 0));
		hevc_debug(2, "ADDING Flag before: %lx (%d)\n",
					dec->dpb_status, index);
		/* Mark destination as available for use by HEVC */
		spin_lock_irqsave(&dev->irqlock, flags);
		if (!list_empty(&dec->dpb_queue)) {
			remove_flag = 0;
			list_for_each_entry_safe(dpb_buf, tmp_buf, &dec->dpb_queue, list) {
				if (dpb_buf == buf) {
					list_del(&dpb_buf->list);
					remove_flag = 1;
					break;
				}
			}
			if (remove_flag == 0) {
				hevc_err("Can't find buf(0x%08lx)\n",
					(unsigned long)buf->planes.raw[0]);
				spin_unlock_irqrestore(&dev->irqlock, flags);
				return;
			}
		}
		if (dec->is_dynamic_dpb) {
			dec->assigned_fd[index] = vb->v4l2_planes[0].m.fd;
			hevc_debug(2, "Assigned FD[%d] = %d\n", index,
						dec->assigned_fd[index]);
			if (dec->dynamic_used & (1 << index)) {
				/* This buffer is already referenced */
				hevc_debug(2, "Already ref[%d], fd = %d\n",
						index, dec->assigned_fd[index]);
				list_add_tail(&buf->list, &dec->ref_queue);
				dec->ref_queue_cnt++;
				skip_add = 1;
			}
		} else {
			set_bit(index, &dec->dpb_status);
			hevc_debug(2, "ADDING Flag after: %lx\n",
							dec->dpb_status);
		}
		if (!skip_add) {
			list_add_tail(&buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;
		}
		spin_unlock_irqrestore(&dev->irqlock, flags);
		if ((dec->dst_memtype == V4L2_MEMORY_USERPTR || dec->dst_memtype == V4L2_MEMORY_DMABUF) &&
				ctx->dst_queue_cnt == dec->total_dpb_count)
			ctx->capture_state = QUEUE_BUFS_MMAPED;
	} else {
		hevc_err("Unsupported buffer type (%d)\n", vq->type);
	}

	if (hevc_dec_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		if (ctx->state == HEVCINST_HEAD_PARSED &&
			ctx->capture_state == QUEUE_BUFS_MMAPED)
			wait_flag = 1;
	}
	hevc_try_run(dev);
	if (wait_flag) {
		if (hevc_wait_for_done_ctx(ctx,
				HEVC_R2H_CMD_INIT_BUFFERS_RET))
			hevc_cleanup_timeout(ctx);
	}

	hevc_debug_leave();
}

static struct vb2_ops hevc_dec_qops = {
	.queue_setup		= hevc_queue_setup,
	.wait_prepare		= hevc_unlock,
	.wait_finish		= hevc_lock,
	.buf_init		= hevc_buf_init,
	.buf_prepare		= hevc_buf_prepare,
	.buf_finish		= hevc_buf_finish,
	.buf_cleanup		= hevc_buf_cleanup,
	.start_streaming	= hevc_start_streaming,
	.stop_streaming		= hevc_stop_streaming,
	.buf_queue		= hevc_buf_queue,
};

const struct v4l2_ioctl_ops *hevc_get_dec_v4l2_ioctl_ops(void)
{
	return &hevc_dec_ioctl_ops;
}

int hevc_init_dec_ctx(struct hevc_ctx *ctx)
{
	struct hevc_dec *dec;
	int ret = 0;
	int i;

	dec = kzalloc(sizeof(struct hevc_dec), GFP_KERNEL);
	if (!dec) {
		hevc_err("failed to allocate decoder private data\n");
		return -ENOMEM;
	}
	ctx->dec_priv = dec;

	ctx->inst_no = HEVC_NO_INSTANCE_SET;

	INIT_LIST_HEAD(&ctx->src_queue);
	INIT_LIST_HEAD(&ctx->dst_queue);
	ctx->src_queue_cnt = 0;
	ctx->dst_queue_cnt = 0;

	for (i = 0; i < HEVC_MAX_BUFFERS; i++) {
		INIT_LIST_HEAD(&ctx->src_ctrls[i]);
		INIT_LIST_HEAD(&ctx->dst_ctrls[i]);
	}
	ctx->src_ctrls_avail = 0;
	ctx->dst_ctrls_avail = 0;

	ctx->capture_state = QUEUE_FREE;
	ctx->output_state = QUEUE_FREE;

	ctx->state = HEVCINST_INIT;
	ctx->type = HEVCINST_DECODER;
	ctx->c_ops = &decoder_codec_ops;
	ctx->src_fmt = &formats[DEF_SRC_FMT];
	ctx->dst_fmt = &formats[DEF_DST_FMT];

	ctx->framerate = DEC_MAX_FPS;
	ctx->qos_ratio = 100;
#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	INIT_LIST_HEAD(&ctx->qos_list);
#endif

	INIT_LIST_HEAD(&dec->dpb_queue);
	dec->dpb_queue_cnt = 0;
	INIT_LIST_HEAD(&dec->ref_queue);
	dec->ref_queue_cnt = 0;

	dec->display_delay = -1;
	dec->is_packedpb = 0;
	dec->is_interlaced = 0;
	dec->immediate_display = 0;
	dec->is_dts_mode = 0;

	dec->is_dynamic_dpb = 0;
	dec->dynamic_used = 0;
	cleanup_assigned_fd(ctx);
	dec->sh_handle.fd = -1;
	dec->ref_info = kzalloc(
		(sizeof(struct dec_dpb_ref_info) * HEVC_MAX_DPBS), GFP_KERNEL);
	if (!dec->ref_info) {
		hevc_err("failed to allocate decoder information data\n");
		return -ENOMEM;
	}

	/* Init videobuf2 queue for OUTPUT */
	ctx->vq_src.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ctx->vq_src.drv_priv = ctx;
	ctx->vq_src.buf_struct_size = sizeof(struct hevc_buf);
	ctx->vq_src.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_src.ops = &hevc_dec_qops;
	ctx->vq_src.mem_ops = hevc_mem_ops();
	ctx->vq_src.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_src);
	if (ret) {
		hevc_err("Failed to initialize videobuf2 queue(output)\n");
		return ret;
	}
	/* Init videobuf2 queue for CAPTURE */
	ctx->vq_dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->vq_dst.drv_priv = ctx;
	ctx->vq_dst.buf_struct_size = sizeof(struct hevc_buf);
	ctx->vq_dst.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_dst.ops = &hevc_dec_qops;
	ctx->vq_dst.mem_ops = hevc_mem_ops();
	ctx->vq_dst.timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_dst);
	if (ret) {
		hevc_err("Failed to initialize videobuf2 queue(capture)\n");
		return ret;
	}

	dec->remained = 0;

	return ret;
}
