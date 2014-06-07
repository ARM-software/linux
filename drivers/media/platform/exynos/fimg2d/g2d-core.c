/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS FIMG2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/of.h>

#include <media/v4l2-ioctl.h>
#include <mach/videonode.h>
#include <plat/fimg2d.h>

#include "g2d.h"

int g2d_log_level;
module_param_named(g2d_log_level, g2d_log_level, uint, 0644);


static struct g2d_fmt g2d_formats[] = {
	{
		.name		= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_RGB,
	}, {
		.name		= "RGB1555",
		.pixelformat	= V4L2_PIX_FMT_RGB555X,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_RGB,
	}, {
		.name		= "ARGB4444",
		.pixelformat	= V4L2_PIX_FMT_RGB444,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_RGB,
	}, {
		.name		= "BGRA8888",
		.pixelformat	= V4L2_PIX_FMT_BGR32,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 32 },
		.color		= G2D_COLOR_RGB,
	}, {
		.name		= "RGBA8888",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 32 },
		.color		= G2D_COLOR_RGB,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 packed, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 packed, CrYCbY",
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 12 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 12 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2,
		.num_comp	= 2,
		.bitperpixel	= { 8, 4 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2,
		.num_comp	= 2,
		.bitperpixel	= { 8, 4 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 16 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:4:4 contiguous Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV24,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 24 },
		.color		= G2D_COLOR_YUV,
	}, {
		.name		= "YUV 4:4:4 contiguous Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV42,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 24 },
		.color		= G2D_COLOR_YUV,
	},
};


static struct g2d_variant variant = {
	.limit_input = {
		.min_w		= 2,
		.min_h		= 2,
		.max_w		= 8000,
		.max_h		= 8000,
		.align_w	= 0,
		.align_h	= 0,
	},
	.limit_output = {
		.min_w		= 2,
		.min_h		= 2,
		.max_w		= 8000,
		.max_h		= 8000,
		.align_w	= 0,
		.align_h	= 0,
	},
	.g2d_up_max		= 16,
	.g2d_down_max		= 4,
};

int g2d_ip_version(struct g2d_dev *g2d)
{
	int ret;

	ret = g2d->ver;
	g2d_dbg("ver:%d\n", ret);

	return ret;
}

/* Find the matches format */
static struct g2d_fmt *g2d_find_format(struct v4l2_format *f)
{
	struct g2d_fmt *g2d_fmt;
	unsigned int i;


	for (i = 0; i < ARRAY_SIZE(g2d_formats); ++i) {
		g2d_fmt = &g2d_formats[i];
		g2d_dbg("i:%d, g2d_fmt->pixelformat:0x%x,\
				f->fmt.pix_mp.pixelformat:0x%x\n"
				, i , g2d_fmt->pixelformat, f->fmt.pix_mp.pixelformat);
		if (g2d_fmt->pixelformat == f->fmt.pix_mp.pixelformat)
			return &g2d_formats[i];
	}

	return NULL;
}
static int g2d_v4l2_querycap(struct file *file, void *fh,
			     struct v4l2_capability *cap)
{
	g2d_dbg("enter!!!!!!\n");
	strncpy(cap->driver, MODULE_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, MODULE_NAME, sizeof(cap->card) - 1);

	cap->bus_info[0] = 0;
	cap->capabilities = V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;

	return 0;
}

static int g2d_v4l2_enum_fmt_mplane(struct file *file, void *fh,
			     struct v4l2_fmtdesc *f)
{
	struct g2d_fmt *g2d_fmt;

	g2d_dbg("f->index:%d, f->type:%d, f->description:%s\n"
			, f->index, f->type, f->description);

	if (f->index >= ARRAY_SIZE(g2d_formats)) {
		g2d_dbg("f->index:%d, ARRAY_SIZE(g2d_formats):%d\n"
				, f->index, ARRAY_SIZE(g2d_formats));
		return -EINVAL;
	}

	g2d_fmt = &g2d_formats[f->index];
	strncpy(f->description, g2d_fmt->name, sizeof(f->description) - 1);
	f->pixelformat = g2d_fmt->pixelformat;

	return 0;
}

static int g2d_v4l2_g_fmt_mplane(struct file *file, void *fh,
			  struct v4l2_format *f)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct g2d_fmt *g2d_fmt;
	struct g2d_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int i;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	g2d_fmt = frame->g2d_fmt;

	pixm->width		= frame->pix_mp.width;
	pixm->height		= frame->pix_mp.height;
	pixm->pixelformat	= frame->pix_mp.pixelformat;
	pixm->field		= V4L2_FIELD_NONE;
	pixm->num_planes	= frame->g2d_fmt->num_planes;
	pixm->colorspace	= 0;

	g2d_dbg("width:%d, height:%d, pixelformat:%d, field:%d, num_planes:%d\n"
			, pixm->width, pixm->height, pixm->pixelformat
			, pixm->field, pixm->num_planes);

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width *
				g2d_fmt->bitperpixel[i]) >> 3;
		pixm->plane_fmt[i].sizeimage = pixm->plane_fmt[i].bytesperline
				* pixm->height;

		v4l2_dbg(1, g2d_log_level, &ctx->g2d_dev->m2m.v4l2_dev,
				"[%d] plane: bytesperline %d, sizeimage %d\n",
				i, pixm->plane_fmt[i].bytesperline,
				pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

int g2d_v4l2_try_fmt_mplane(struct file *file, void *fh,
			    struct v4l2_format *f)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct g2d_fmt *g2d_fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct g2d_frame *frame;
	struct g2d_size_limit *limit;
	u32 tmp_w, tmp_h;
	int i;

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"not supported v4l2 type\n");
		return -EINVAL;
	}

	g2d_fmt = g2d_find_format(f);
	g2d_dbg("g2d_fmt:0x%p\n", g2d_fmt);

	if (!g2d_fmt) {
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"not supported format type\n");
		return -EINVAL;
	}

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (frame == &ctx->s_frame) {
		limit = &ctx->g2d_dev->variant->limit_input;
/* rDW : G2D TODO - limitation in ratation */
#if 0
		/* rotation max source size is 4Kx4K */
		if (ctx->rotation == 90 || ctx->rotation == 270) {
			limit->max_w = 4096;
			limit->max_h = 4096;
		}
#endif
	} else {
		limit = &ctx->g2d_dev->variant->limit_output;
	}

	/*
	 * Y_SPAN - should even in interleaved YCbCr422
	 * C_SPAN - should even in YCbCr420 and YCbCr422
	 */
	if (g2d_fmt_is_yuv422(g2d_fmt->pixelformat) ||
			g2d_fmt_is_yuv420(g2d_fmt->pixelformat))
		limit->align_w = 1;

	/* To check if image size is modified to adjust parameter against
	   hardware abilities */
	tmp_w = pixm->width;
	tmp_h = pixm->height;

	/* Bound an image to have width and height in limit */
	v4l_bound_align_image(&pixm->width, limit->min_w, limit->max_w,
			limit->align_w, &pixm->height, limit->min_h,
			limit->max_h, limit->align_h, 0);

	if (tmp_w != pixm->width || tmp_h != pixm->height)
		g2d_dbg("Image size has been modified\
				from %dx%d to pixm->width x pixm->height %dx%d",
			 tmp_w, tmp_h, pixm->width, pixm->height);

	pixm->num_planes = g2d_fmt->num_planes;
	pixm->colorspace = 0;

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width *
				g2d_fmt->bitperpixel[i]) >> 3;
		pixm->plane_fmt[i].sizeimage = pixm->plane_fmt[i].bytesperline
				* pixm->height;

		v4l2_dbg(1, g2d_log_level, &ctx->g2d_dev->m2m.v4l2_dev,
				"[%d] plane: bytesperline %d, sizeimage %d\n",
				i, pixm->plane_fmt[i].bytesperline,
				pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int g2d_v4l2_s_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)

{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	struct g2d_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int i, ret = 0;

	if (vb2_is_streaming(vq)) {
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev, "device is busy\n");
		return -EBUSY;
	}

	ret = g2d_v4l2_try_fmt_mplane(file, fh, f);
	if (ret < 0)
		return ret;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	set_bit(CTX_PARAMS, &ctx->flags);

	frame->g2d_fmt = g2d_find_format(f);
	if (!frame->g2d_fmt) {
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"not supported format values\n");
		return -EINVAL;
	}

	frame->pix_mp.pixelformat = pixm->pixelformat;
	frame->pix_mp.width	= pixm->width;
	frame->pix_mp.height	= pixm->height;

	/*
	 * Shouldn't call s_crop or g_crop before called g_fmt or s_fmt.
	 * Let's assume that we can keep the order.
	 */
	frame->crop.width	= pixm->width;
	frame->crop.height	= pixm->height;

	g2d_dbg("pixelformat:%d, width:%d, height:%d, crop.width:%d, crop.height:%d\n"
			, frame->pix_mp.pixelformat, frame->pix_mp.width
			, frame->pix_mp.height, frame->crop.width, frame->crop.height);

	for (i = 0; i < frame->g2d_fmt->num_planes; ++i)
		frame->bytesused[i] = (pixm->width * pixm->height *
				frame->g2d_fmt->bitperpixel[i]) >> 3;

	return 0;
}

static int g2d_v4l2_cropcap(struct file *file, void *fh,
			    struct v4l2_cropcap *cr)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct g2d_frame *frame;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	cr->bounds.left		= 0;
	cr->bounds.top		= 0;
	cr->bounds.width	= frame->pix_mp.width;
	cr->bounds.height	= frame->pix_mp.height;
	cr->defrect		= cr->bounds;

	return 0;
}

static int g2d_v4l2_s_crop(struct file *file, void *fh, struct v4l2_crop *cr)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct g2d_dev *g2d = ctx->g2d_dev;
	struct v4l2_pix_format_mplane *pixm;
	struct g2d_frame *frame;
	struct g2d_size_limit *limit = NULL;
	u32 tmp_w, tmp_h;
	int x_align = 0, y_align = 0;
	int i;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!test_bit(CTX_PARAMS, &ctx->flags)) {
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"color format is not set\n");
		return -EINVAL;
	}

	if (cr->c.left < 0 || cr->c.top < 0 ||
			cr->c.width < 0 || cr->c.height < 0) {
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"crop value is negative\n");
		return -EINVAL;
	}

	pixm = &frame->pix_mp;
	if (frame == &ctx->s_frame) {
		limit = &g2d->variant->limit_input;
		set_bit(CTX_SRC_FMT, &ctx->flags);
	} else {
		limit = &g2d->variant->limit_output;
		set_bit(CTX_DST_FMT, &ctx->flags);
	}

	if (g2d_fmt_is_yuv422(frame->g2d_fmt->pixelformat)) {
		limit->align_w = 1;
	} else if (g2d_fmt_is_yuv420(frame->g2d_fmt->pixelformat)) {
		limit->align_w = 1;
		limit->align_h = 1;
	}

	/* To check if image size is modified to adjust parameter against
	   hardware abilities */
	tmp_w = cr->c.width;
	tmp_h = cr->c.height;

	/* Bound an image to have crop width and height in limit */
	v4l_bound_align_image(&cr->c.width, limit->min_w, limit->max_w,
			limit->align_w, &cr->c.height, limit->min_h,
			limit->max_h, limit->align_h, 0);

	if (tmp_w != cr->c.width || tmp_h != cr->c.height)
		g2d_dbg("Image size has been modified\
				from %dx%d to cr->c.width x cr->c.height %dx%d",
			 tmp_w, tmp_h, cr->c.width, cr->c.height);

	if (frame == &ctx->s_frame) {
		if (g2d_fmt_is_yuv422(frame->g2d_fmt->pixelformat))
			x_align = 1;
	} else {
		if (g2d_fmt_is_yuv422(frame->g2d_fmt->pixelformat)) {
			x_align = 1;
		} else if (g2d_fmt_is_yuv420(frame->g2d_fmt->pixelformat)) {
			x_align = 1;
			y_align = 1;
		}
	}

	/* To check if image size is modified to adjust parameter against
	   hardware abilities */
	tmp_w = cr->c.left;
	tmp_h = cr->c.top;

	/* Bound an image to have crop position in limit */
	v4l_bound_align_image(&cr->c.left, 0, pixm->width - cr->c.width,
			x_align, &cr->c.top, 0, pixm->height - cr->c.height,
			y_align, 0);

	if (tmp_w != cr->c.left || tmp_h != cr->c.top)
		g2d_dbg("Image size has been modified\
				from %dx%d to cr->c.left x cr->c.top %dx%d",
			 tmp_w, tmp_h, cr->c.left, cr->c.top);

	frame->crop = cr->c;

	for (i = 0; i < frame->g2d_fmt->num_planes; ++i)
		frame->bytesused[i] = (cr->c.width * cr->c.height *
				frame->g2d_fmt->bitperpixel[i]) >> 3;

	g2d_dbg("c.left:%d, c.top:%d, c.width:%d, c.height:%d\n"
			, cr->c.left, cr->c.top
			, cr->c.width, cr->c.height);
	return 0;
}

static int g2d_v4l2_g_crop(struct file *file, void *fh, struct v4l2_crop *cr)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct g2d_frame *frame;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	cr->c = frame->crop;

	return 0;
}

/* This function use only Dst frame */
static int g2d_v4l2_s_clip(struct g2d_ctx *ctx, struct v4l2_rect *clip)
{
	struct g2d_frame *frame;
	struct g2d_dev *g2d = ctx->g2d_dev;
	struct v4l2_pix_format_mplane *pixm;
	struct g2d_size_limit *limit = NULL;
	u32 tmp_w, tmp_h;
	int x_align = 0, y_align = 0;
	int i;

	frame = &ctx->d_frame;
	pixm = &frame->pix_mp;

	limit = &g2d->variant->limit_output;
	set_bit(CTX_DST_FMT, &ctx->flags);

	if (g2d_fmt_is_yuv422(frame->g2d_fmt->pixelformat)) {
		x_align = 1;
	} else if (g2d_fmt_is_yuv420(frame->g2d_fmt->pixelformat)) {
		x_align = 1;
		y_align = 1;
	}

	/* To check if image size is modified to adjust parameter against
	   hardware abilities */
	tmp_w = clip->left;
	tmp_h = clip->top;

	/* Bound an image to have crop position in limit */
	v4l_bound_align_image(&clip->left, 0, pixm->width,
			x_align, &clip->top, 0, pixm->height,
			y_align, 0);

	if (tmp_w != clip->left || tmp_h != clip->top)
		g2d_dbg("Image size has been modified\
				from %dx%d to clip->left x clip->top %dx%d",
			 tmp_w, tmp_h, clip->left, clip->top);

	for (i = 0; i < frame->g2d_fmt->num_planes; ++i)
		frame->bytesused[i] = (clip->width * clip->height *
				frame->g2d_fmt->bitperpixel[i]) >> 3;

	g2d_dbg("left:%d, top:%d, width:%d, height:%d\n"
			, clip->left, clip->top
			, clip->width, clip->height);

	return 0;
}

static int g2d_v4l2_reqbufs(struct file *file, void *fh,
			    struct v4l2_requestbuffers *reqbufs)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	struct g2d_dev *g2d = ctx->g2d_dev;

	g2d_dbg("call.\n");

	if (reqbufs->count == 0) {
		ctx->cci_on = G2D_CCI_OFF;
		reqbufs->reserved[1] = ((g2d->end_time)-(g2d->start_time));
	}

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int g2d_v4l2_querybuf(struct file *file, void *fh,
			     struct v4l2_buffer *buf)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	return v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);
}

static int g2d_v4l2_qbuf(struct file *file, void *fh,
			 struct v4l2_buffer *buf)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);

	g2d_dbg("call.\n");

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}
static int g2d_v4l2_dqbuf(struct file *file, void *fh,
			  struct v4l2_buffer *buf)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);
	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int g2d_v4l2_streamon(struct file *file, void *fh,
			     enum v4l2_buf_type type)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);

	g2d_dbg("call.\n");
	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int g2d_v4l2_streamoff(struct file *file, void *fh,
			      enum v4l2_buf_type type)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(fh);

	g2d_dbg("type:%d\n", type);
	return v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);
}

static const struct v4l2_ioctl_ops g2d_v4l2_ioctl_ops = {
	.vidioc_querycap		= g2d_v4l2_querycap,

	.vidioc_enum_fmt_vid_cap_mplane	= g2d_v4l2_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_out_mplane	= g2d_v4l2_enum_fmt_mplane,

	.vidioc_g_fmt_vid_cap_mplane	= g2d_v4l2_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= g2d_v4l2_g_fmt_mplane,

	.vidioc_try_fmt_vid_cap_mplane	= g2d_v4l2_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= g2d_v4l2_try_fmt_mplane,

	.vidioc_s_fmt_vid_cap_mplane	= g2d_v4l2_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= g2d_v4l2_s_fmt_mplane,

	.vidioc_reqbufs			= g2d_v4l2_reqbufs,
	.vidioc_querybuf		= g2d_v4l2_querybuf,

	.vidioc_qbuf			= g2d_v4l2_qbuf,
	.vidioc_dqbuf			= g2d_v4l2_dqbuf,

	.vidioc_streamon		= g2d_v4l2_streamon,
	.vidioc_streamoff		= g2d_v4l2_streamoff,

	.vidioc_g_crop			= g2d_v4l2_g_crop,
	.vidioc_s_crop			= g2d_v4l2_s_crop,
	.vidioc_cropcap			= g2d_v4l2_cropcap
};

static int g2d_ctx_stop_req(struct g2d_ctx *ctx)
{
	struct g2d_ctx *curr_ctx;
	struct g2d_dev *g2d = ctx->g2d_dev;
	int ret = 0;

	curr_ctx = v4l2_m2m_get_curr_priv(g2d->m2m.m2m_dev);
	if (!test_bit(CTX_RUN, &ctx->flags) || (curr_ctx != ctx))
		return 0;

	set_bit(CTX_ABORT, &ctx->flags);

	ret = wait_event_timeout(g2d->wait,
			!test_bit(CTX_RUN, &ctx->flags), G2D_TIMEOUT);

	/* TODO: How to handle case of timeout event */
	if (ret == 0) {
		dev_err(g2d->dev, "device failed to stop request\n");
		ret = -EBUSY;
	}

	return ret;
}

static int g2d_vb2_queue_setup(struct vb2_queue *vq,
		const struct v4l2_format *fmt, unsigned int *num_buffers,
		unsigned int *num_planes, unsigned int sizes[],
		void *allocators[])
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vq);
	struct g2d_frame *frame;
	int i;
	int ret;
	unsigned int total = 0;

	frame = ctx_get_frame(ctx, vq->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	/* Get number of planes from format_list in driver */
	*num_planes = frame->g2d_fmt->num_planes;
	for (i = 0; i < frame->g2d_fmt->num_planes; i++) {
		sizes[i] = (frame->pix_mp.width * frame->pix_mp.height *
				frame->g2d_fmt->bitperpixel[i]) >> 3;
		total = total + sizes[i];
	}

#if defined(CONFIG_FIMG2D_CCI_SNOOP)
	if ((is_cci_on(ctx, total)) | (ctx->cci_on)) { /* cci on */
		for (i = 0; i < frame->g2d_fmt->num_planes; i++)
			allocators[i] = ctx->g2d_dev->alloc_ctx_cci;
		ctx->cci_on = G2D_CCI_ON;
		/* printk("[%s:%d] set as alloc_ctx_cci, total:%d\n"
				, __func__, __LINE__, total); */
	} else {	/* cci off */
		for (i = 0; i < frame->g2d_fmt->num_planes; i++)
			allocators[i] = ctx->g2d_dev->alloc_ctx;
		ctx->cci_on = G2D_CCI_OFF;
		/* printk("[%s:%d] set as alloc_ctx(do not cci), total:%d\n"
				, __func__, __LINE__, total); */
	}
#else
	for (i = 0; i < frame->g2d_fmt->num_planes; i++) {
		allocators[i] = ctx->g2d_dev->alloc_ctx;
	}
	ctx->cci_on = G2D_CCI_OFF;
#endif

	ret = vb2_queue_init(vq);
	if (ret) {
		g2d_dbg("failed to init vb2_queue");
		return ret;
	}

	return 0;
}

static int g2d_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct g2d_frame *frame;
	int i;

	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < frame->g2d_fmt->num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->bytesused[i]);
	}

	return g2d_buf_sync_prepare(vb);
}

static void g2d_fence_work(struct work_struct *work)
{
	struct g2d_ctx *ctx = container_of(work, struct g2d_ctx, fence_work);
	struct v4l2_m2m_buffer *buffer;
	struct sync_fence *fence;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ctx->slock, flags);

	while (!list_empty(&ctx->fence_wait_list)) {
		buffer = list_first_entry(&ctx->fence_wait_list,
					  struct v4l2_m2m_buffer, wait);
		list_del(&buffer->wait);
		spin_unlock_irqrestore(&ctx->slock, flags);

		fence = buffer->vb.acquire_fence;
		if (fence) {
			buffer->vb.acquire_fence = NULL;
			ret = sync_fence_wait(fence, 1000);
			if (ret == -ETIME) {
				dev_warn(ctx->g2d_dev->dev,
					"sync_fence_wait() timeout\n");
				ret = sync_fence_wait(fence, 10 * MSEC_PER_SEC);
			}
			if (ret)
				dev_warn(ctx->g2d_dev->dev,
					"sync_fence_wait() error\n");
			sync_fence_put(fence);
		}

		if (ctx->m2m_ctx) {
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &buffer->vb);
			v4l2_m2m_try_schedule(ctx->m2m_ctx);
		}

		spin_lock_irqsave(&ctx->slock, flags);
	}

	spin_unlock_irqrestore(&ctx->slock, flags);
}

static void g2d_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_m2m_buffer *b =
		container_of(vb, struct v4l2_m2m_buffer, vb);
	struct sync_fence *fence;
	unsigned long flags;

	fence = vb->acquire_fence;
	if (fence) {
		spin_lock_irqsave(&ctx->slock, flags);
		list_add_tail(&b->wait, &ctx->fence_wait_list);
		spin_unlock_irqrestore(&ctx->slock, flags);

		queue_work(ctx->fence_wq, &ctx->fence_work);
	} else {
		if (ctx->m2m_ctx)
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);
	}
}

static void g2d_vb2_lock(struct vb2_queue *vq)
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_lock(&ctx->g2d_dev->lock);
}

static void g2d_vb2_unlock(struct vb2_queue *vq)
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_unlock(&ctx->g2d_dev->lock);
}

static int g2d_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vq);
	set_bit(CTX_STREAMING, &ctx->flags);

	return 0;
}

static int g2d_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct g2d_ctx *ctx = vb2_get_drv_priv(vq);
	int ret;

	ret = g2d_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->g2d_dev->dev, "wait timeout\n");

	clear_bit(CTX_STREAMING, &ctx->flags);

	return ret;
}

static struct vb2_ops g2d_vb2_ops = {
	.queue_setup		 = g2d_vb2_queue_setup,
	.buf_prepare		 = g2d_vb2_buf_prepare,
	.buf_finish		 = g2d_buf_sync_finish,
	.buf_queue		 = g2d_vb2_buf_queue,
	.wait_finish		 = g2d_vb2_lock,
	.wait_prepare		 = g2d_vb2_unlock,
	.start_streaming	 = g2d_vb2_start_streaming,
	.stop_streaming		 = g2d_vb2_stop_streaming,
};

static int queue_init(void *priv, struct vb2_queue *src_vq,
		      struct vb2_queue *dst_vq)
{
	struct g2d_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->ops = &g2d_vb2_ops;
	src_vq->mem_ops = ctx->g2d_dev->vb2->ops;
	src_vq->drv_priv = ctx;
	src_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);

	ret = vb2_queue_init(src_vq);
	if (ret) {
		dev_err(ctx->g2d_dev->dev, "failed to init vb2_queue");
		return ret;
	}

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->ops = &g2d_vb2_ops;
	dst_vq->mem_ops = ctx->g2d_dev->vb2->ops;
	dst_vq->drv_priv = ctx;
	dst_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);

	return vb2_queue_init(dst_vq);
}

static int g2d_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct g2d_ctx *ctx;
	struct g2d_frame *d_frame;

	g2d_dbg("ctrl ID:%d, value:0x%x\n", ctrl->id, ctrl->val);
	ctx = container_of(ctrl->handler, struct g2d_ctx, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			ctx->flip |= G2D_VFLIP;
		else
			ctx->flip &= ~G2D_VFLIP;
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			ctx->flip |= G2D_HFLIP;
		else
			ctx->flip &= ~G2D_HFLIP;
		break;
	case V4L2_CID_ROTATE:
		ctx->rotation = ctrl->val;
		break;
	case V4L2_CID_CACHEABLE:
		ctx->cacheable = (bool)ctrl->val;
		break;
	case V4L2_CID_GLOBAL_ALPHA:
		ctx->g_alpha = ctrl->val;
		break;
	case V4L2_CID_2D_BLEND_OP:
		ctx->op = ctrl->val;
		break;
	case V4L2_CID_2D_COLOR_FILL:
		ctx->color_fill = (bool)ctrl->val;
		break;
	case V4L2_CID_2D_SRC_COLOR:
		ctx->color = ctrl->val;
		break;
	case V4L2_CID_2D_FMT_PREMULTI:
		ctx->pre_multi = ctrl->val;
		break;
	case V4L2_CID_2D_DITH:
		ctx->dith = ctrl->val;
		break;
	case V4L2_CID_2D_SCALE_MODE:
		ctx->scale.mode = ctrl->val;
		break;
	case V4L2_CID_2D_SCALE_WIDTH:
		ctx->scale.src_w = (ctrl->val >> 16) & (0xFFFF);
		ctx->scale.dst_w = (ctrl->val) & (0xFFFF);

		if ((ctx->scale.src_w > 8000) || (ctx->scale.dst_w > 8000)) {
			v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"g2d_s_ctrl : V4L2_CID_2D_SCALE_WIDTH failed,\
				scaled.src_w:%d, scale.dst_w:%d\n"
				, ctx->scale.src_w, ctx->scale.dst_w);
			return -EINVAL;
		}
		g2d_dbg("scale.src_w:%d, scale.dst_w:%d\n"
				, ctx->scale.src_w, ctx->scale.dst_w);
		break;
	case V4L2_CID_2D_SCALE_HEIGHT:
		ctx->scale.src_h = (ctrl->val >> 16) & (0xFFFF);
		ctx->scale.dst_h = (ctrl->val) & (0xFFFF);

		if ((ctx->scale.src_h > 8000) || (ctx->scale.dst_h > 8000)) {
			v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"g2d_s_ctrl : V4L2_CID_2D_SCALE_HEIGHT failed,\
				scaled.src_h:%d, scale.dst_h:%d\n"
				, ctx->scale.src_h, ctx->scale.dst_h);
			return -EINVAL;
		}

		g2d_dbg("scale.src_h:%d, scale.dst_h:%d, scale.mode:%d\n"
				, ctx->scale.src_h, ctx->scale.dst_h
				, ctx->scale.mode);
		break;
	case V4L2_CID_2D_CLIP:
		d_frame = &ctx->d_frame;
		if (ctrl->val) {
			if (copy_from_user(&d_frame->clip
					, (struct v4l2_rect *)(ctrl->val)
					, sizeof(struct v4l2_rect))) {
					return -EFAULT;
			}
			d_frame->clip_enable = 1;
			g2d_dbg("d_frame->clip->left=%d, top:%d,\
					width:%d, height:%d\n"
				, d_frame->clip.left, d_frame->clip.top
				, d_frame->clip.width, d_frame->clip.height);
		} else
			d_frame->clip_enable = 0;

		break;
	case V4L2_CID_2D_REPEAT:
		ctx->rep.mode = ctrl->val;
		break;
	case V4L2_CID_2D_BLUESCREEN:
		ctx->bluesc.mode = ctrl->val;
		break;
	case V4L2_CID_2D_BG_COLOR:
		ctx->bluesc.bg_color = ctrl->val;
		break;
	case V4L2_CID_CSC_EQ_MODE:
		ctx->csc.csc_mode = ctrl->val;
		break;
	case V4L2_CID_CSC_EQ:
		ctx->csc.csc_eq = ctrl->val;
		break;
	case V4L2_CID_CSC_RANGE:
		ctx->csc.csc_range = ctrl->val;
		break;
	}
	return 0;
}

static const struct v4l2_ctrl_ops g2d_ctrl_ops = {
	.s_ctrl = g2d_s_ctrl,
};

static const struct v4l2_ctrl_config g2d_custom_ctrl[] = {
	{
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_CACHEABLE,
		.name = "set cacheable",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = true,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_GLOBAL_ALPHA,
		.name = "Set constant src alpha",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,
		.max = 255,
		.def = 255,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_BLEND_OP,
		.name = "set blend op",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,
		.max = BLIT_OP_ADD,
		.def = BLIT_OP_SRC,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_COLOR_FILL,
		.name = "set color fill",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = false,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_SRC_COLOR,
		.name = "set color fill value",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		/* 'step' mush be 0 for V4L2_CTRL_TYPE_BITMASK */
		.step = 0,
		.max = 0xffffffff,
		.min = 0,
		.def = 0,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_DITH,
		.name = "set dithering",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = false,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_CLIP,
		.name = "set clip",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 0,
		.max = 0xffffffff,
		.min = 0,
		.def = 0,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_SCALE_MODE,
		.name = "set scale mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,
		.max = SCALING_BILINEAR,
		.def = 0,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_SCALE_WIDTH,
		.name = "set scale width",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.max = ((8000 << 16) | (8000 << 0)),
		.min = 0x00010001,
		.def = 0x00010001,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_SCALE_HEIGHT,
		.name = "set scale height",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.max = ((8000 << 16) | (8000 << 0)),
		.min = 0x00010001,
		.def = 0x00010001,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_REPEAT,
		.name = "set repeat mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.max = REPEAT_CLAMP,
		.min = NO_REPEAT,
		.def = NO_REPEAT,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_FMT_PREMULTI,
		.name = "set pre-multiplied format",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = true,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_CSC_EQ_MODE,
		.name = "Set CSC equation mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = false,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_CSC_EQ,
		.name = "Set CSC equation",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = G2D_CSC_709,
		.def = G2D_CSC_601,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_CSC_RANGE,
		.name = "Set CSC range",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = G2D_CSC_WIDE,
		.def = G2D_CSC_NARROW,
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_BLUESCREEN,
		.name = "set bluescreen mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,		/* OPAQUE */
		.max = (BS_END - 1),	/* BLUSCR */
		.def = 0,		/* OPAQUE */
	}, {
		.ops = &g2d_ctrl_ops,
		.id = V4L2_CID_2D_BG_COLOR,
		.name = "set bluescreen BG color",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		/* 'step' mush be 0 for V4L2_CTRL_TYPE_BITMASK */
		.step = 0,
		.max = 0xffffffff,
		.min = 0,
		.def = 0,
	}
};

static int g2d_add_ctrls(struct g2d_ctx *ctx)
{
	int i;
	int err;

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, G2D_MAX_CTRL_NUM);
	v4l2_ctrl_new_std(&ctx->ctrl_handler, &g2d_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ctx->ctrl_handler, &g2d_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ctx->ctrl_handler, &g2d_ctrl_ops,
			V4L2_CID_ROTATE, 0, 270, 90, 0);

	for (i = 0; i < ARRAY_SIZE(g2d_custom_ctrl); i++)
		v4l2_ctrl_new_custom(&ctx->ctrl_handler,
				&g2d_custom_ctrl[i], NULL);
	if (ctx->ctrl_handler.error) {
		err = ctx->ctrl_handler.error;
		v4l2_err(&ctx->g2d_dev->m2m.v4l2_dev,
				"v4l2_ctrl_handler_init failed %d\n", err);
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		return err;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_handler);

	return 0;
}

static int g2d_open(struct file *file)
{
	struct g2d_dev *g2d = video_drvdata(file);
	struct g2d_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(g2d->dev, "no memory for open context\n");
		return -ENOMEM;
	}

	atomic_inc(&g2d->m2m.in_use);
	ctx->g2d_dev = g2d;

	v4l2_fh_init(&ctx->fh, g2d->m2m.vfd);
	ret = g2d_add_ctrls(ctx);
	if (ret)
		goto err_fh;

	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	/* Default color format */
	ctx->s_frame.g2d_fmt = &g2d_formats[0];
	ctx->d_frame.g2d_fmt = &g2d_formats[0];
	init_waitqueue_head(&g2d->wait);
	spin_lock_init(&ctx->slock);

	INIT_LIST_HEAD(&ctx->fence_wait_list);
	INIT_WORK(&ctx->fence_work, g2d_fence_work);
	ctx->fence_wq = create_singlethread_workqueue("g2d_wq");
	if (&ctx->fence_wq == NULL) {
		dev_err(g2d->dev, "failed to create work queue\n");
		goto err_wq;
	}

	/* Setup the device context for mem2mem mode. */
	ctx->m2m_ctx = v4l2_m2m_ctx_init(g2d->m2m.m2m_dev, ctx, queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		ret = -EINVAL;
		goto err_ctx;
	}

	return 0;

err_ctx:
	if (&ctx->fence_wq)
		destroy_workqueue(ctx->fence_wq);
err_wq:
	v4l2_fh_del(&ctx->fh);
err_fh:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&g2d->m2m.in_use);
	kfree(ctx);

	return ret;
}

static int g2d_release(struct file *file)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(file->private_data);
	struct g2d_dev *g2d = ctx->g2d_dev;

	g2d_dbg("refcnt= %d", atomic_read(&g2d->m2m.in_use));

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	if (&ctx->fence_wq)
		destroy_workqueue(ctx->fence_wq);
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&g2d->m2m.in_use);
	kfree(ctx);

	return 0;
}

static unsigned int g2d_poll(struct file *file,
			     struct poll_table_struct *wait)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(file->private_data);

	return v4l2_m2m_poll(file, ctx->m2m_ctx, wait);
}

static int g2d_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct g2d_ctx *ctx = fh_to_g2d_ctx(file->private_data);

	return v4l2_m2m_mmap(file, ctx->m2m_ctx, vma);
}

static const struct v4l2_file_operations g2d_v4l2_fops = {
	.owner		= THIS_MODULE,
	.open		= g2d_open,
	.release	= g2d_release,
	.poll		= g2d_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= g2d_mmap,
};

static void g2d_clock_gating(struct g2d_dev *g2d, enum g2d_clk_status status)
{
	if (status == G2D_CLK_ON) {
		atomic_inc(&g2d->clk_cnt);
		clk_enable(g2d->clk);
		g2d_dbg("clock enable\n");
	} else if (status == G2D_CLK_OFF) {
		int clk_cnt = atomic_dec_return(&g2d->clk_cnt);
		if (clk_cnt < 0) {
			dev_err(g2d->dev, "G2D clock control is wrong!!\n");
			atomic_set(&g2d->clk_cnt, 0);
		} else {
			clk_disable(g2d->clk);
			g2d_dbg("clock disable\n");
		}
	}
}

#ifdef CONFIG_FIMG2D_CCI_SNOOP
static void g2d_set_cci_snoop(struct g2d_ctx *ctx)
{
	enum g2d_shared_val val;
	enum g2d_shared_sel sel;

	if (ctx->cci_on) {
		/* G2D CCI on */
		val = SHAREABLE_PATH;
		sel = SHARED_G2D_SEL;
		g2d_cci_snoop_control(IP_VER_G2D_5H, val, sel);
	} else {
		/* G2D CCI off */
		val = NON_SHAREABLE_PATH;
		sel = SHARED_FROM_SYSMMU;
		g2d_cci_snoop_control(IP_VER_G2D_5H, val, sel);
	}
}
#endif

static void g2d_clock_resume(struct g2d_dev *g2d)
{
	if (clk_set_parent(g2d->clk_chld1, g2d->clk_parn1))
		dev_err(g2d->dev, "Unable to set chld1, parn1 clock\n");
	if (clk_set_parent(g2d->clk_chld2, g2d->clk_parn2))
		dev_err(g2d->dev, "Unable to set chld2, parn2 clock\n");

	g2d_dbg("clock resume\n");
}

static void g2d_watchdog(unsigned long arg)
{
}

static irqreturn_t g2d_irq_handler(int irq, void *priv)
{
	struct g2d_dev *g2d = priv;
	struct g2d_ctx *ctx;
	struct vb2_buffer *src_vb, *dst_vb;

	g2d->end_time = sched_clock();
#ifdef G2D_PERF
	printk("[%s:%d] OPERATION-TIME: %llu\n"
			, __func__, __LINE__,
			(g2d->end_time)-(g2d->start_time));
#endif
	g2d_dbg("start irq handler\n");

	spin_lock(&g2d->slock);

	clear_bit(DEV_RUN, &g2d->state);
	if (timer_pending(&g2d->wdt.timer))
		del_timer(&g2d->wdt.timer);

	g2d_hwset_int_clear(g2d);
	g2d_clock_gating(g2d, G2D_CLK_OFF);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put(g2d->dev);
#endif

	ctx = v4l2_m2m_get_curr_priv(g2d->m2m.m2m_dev);
	if (!ctx || !ctx->m2m_ctx) {
		dev_err(g2d->dev, "current ctx is NULL\n");
		goto isr_unlock;
	}

	clear_bit(CTX_RUN, &ctx->flags);

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	if (src_vb && dst_vb) {
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);

		if (test_bit(DEV_SUSPEND, &g2d->state)) {
			g2d_dbg("wake up blocked process by suspend\n");
			wake_up(&g2d->wait);
		} else {
			v4l2_m2m_job_finish(g2d->m2m.m2m_dev, ctx->m2m_ctx);
		}

		/* Wake up from CTX_ABORT state */
		/* if (test_and_clear_bit(CTX_ABORT, &ctx->flags))
			wake_up(&g2d->wait);
		 */
	} else {
		dev_err(g2d->dev, "failed to get the buffer done\n");
	}

isr_unlock:
	spin_unlock(&g2d->slock);


	return IRQ_HANDLED;
}

static void g2d_set_frame_addr(struct g2d_ctx *ctx)
{
	struct vb2_buffer *src_vb, *dst_vb;
	struct g2d_frame *s_frame, *d_frame;
	struct g2d_dev *g2d = ctx->g2d_dev;
	unsigned int s_size, d_size;
	unsigned int s_stride, d_stride;
	/* TODO */
	/* struct sysmmu_prefbuf prebuf[G2D_MAX_PBUF]; */

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	/* get source buffer address */
	src_vb = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	s_size = s_frame->pix_mp.width * s_frame->pix_mp.height;
	s_frame->addr.y = g2d->vb2->plane_addr(src_vb, 0);
	s_frame->addr.c = 0;

	g2d_dbg("s_frame - num_comp:%d, num_planes:%d\n"
			, s_frame->g2d_fmt->num_comp, s_frame->g2d_fmt->num_planes);
	g2d_dbg("s_frame - pix_mp.width:%d, pix_mp,height:%d\n"
			, s_frame->pix_mp.width, s_frame->pix_mp.height);

	if (s_frame->g2d_fmt->num_comp == 1) {
		s_size = s_size * (s_frame->g2d_fmt->bitperpixel[0] >> 3);
	} else if (s_frame->g2d_fmt->num_comp == 2) {
		if (s_frame->g2d_fmt->num_planes == 1)
			s_frame->addr.c = s_frame->addr.y + s_size;
		else if (s_frame->g2d_fmt->num_planes == 2)
			s_frame->addr.c = g2d->vb2->plane_addr(src_vb, 1);
		else
			dev_err(g2d->dev, "Please check the num of planes\n");
	}

	if (s_frame->g2d_fmt->color == G2D_COLOR_YUV)
		s_stride = s_frame->pix_mp.width;
	else
		s_stride = (s_frame->pix_mp.width) * (s_frame->g2d_fmt->bitperpixel[0] >> 3);

	g2d_dbg("s_stride : %d, s_size:%d,\
			s_frame->g2d_fmt->bitperpixel[0] : %d, pixelformat : %d\n"
			, s_stride, s_size, s_frame->g2d_fmt->bitperpixel[0]
			, s_frame->g2d_fmt->pixelformat);

	/* get destination buffer address */
	dst_vb = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	d_size = d_frame->pix_mp.width * d_frame->pix_mp.height;
	d_frame->addr.y = g2d->vb2->plane_addr(dst_vb, 0);
	d_frame->addr.c = 0;

	g2d_dbg("d_frame - num_comp:%d, num_planes:%d\n"
			, d_frame->g2d_fmt->num_comp, d_frame->g2d_fmt->num_planes);
	g2d_dbg("d_frame - pix_mp.width:%d, pix_mp,height:%d\n"
			, d_frame->pix_mp.width, d_frame->pix_mp.height);

	if (d_frame->g2d_fmt->num_comp == 1) {
		d_size = d_size * (d_frame->g2d_fmt->bitperpixel[0] >> 3);
	} else if (d_frame->g2d_fmt->num_comp == 2) {
		if (d_frame->g2d_fmt->num_planes == 1)
			d_frame->addr.c = d_frame->addr.y + d_size;
		else if (d_frame->g2d_fmt->num_planes == 2)
			d_frame->addr.c = g2d->vb2->plane_addr(dst_vb, 1);
		else
			dev_err(g2d->dev, "Please check the num of planes\n");
	}

	if (d_frame->g2d_fmt->color == G2D_COLOR_YUV)
		d_stride = d_frame->pix_mp.width;
	else
		d_stride = (d_frame->pix_mp.width) * (d_frame->g2d_fmt->bitperpixel[0] >> 3);
	g2d_dbg("d_stride : %d, d_size:%d,\
			d_frame->g2d_fmt->bitperpixel[0] : %d, pixelformat : %d\n"
			, d_stride, d_size, d_frame->g2d_fmt->bitperpixel[0]
			, d_frame->g2d_fmt->pixelformat);

	g2d_dbg("s_frame->addr.y : 0x%x s_frame->add.c : 0x%x\n"
			, s_frame->addr.y, s_frame->addr.c);

	/* set buffer base address */
	/* FIMG2D_SRC_BASE_ADDR_REG */
	/* FIMG2D_SRC_PLANE2_BASE_ADDR_REG */
	g2d_hwset_src_addr(g2d, s_frame);
	/* FIMG2D_SRC_STRIDE_REG */
	g2d_hwset_src_stride(g2d, s_stride);

	g2d_dbg("d_frame->addr.y : 0x%x d_frame->add.c : 0x%x\n"
			, d_frame->addr.y, d_frame->addr.c);
	/* FIMG2D_DST_BASE_ADDR_REG */
	/* FIMG2D_DST_PLANE2_BASE_ADDR_REG */
	g2d_hwset_dst_addr(g2d, d_frame);
	/* FIMG2D_DST_STRIDE_REG */
	g2d_hwset_dst_stride(g2d, d_stride);
#if 0
	/* set sysmmu prefetch buffer */
	prebuf[0].base = s_frame->addr.y;
	prebuf[0].size = s_size;

	if (s_frame->g2d_fmt->num_comp == 2) {
		prebuf[1].base = s_frame->addr.c;
		prebuf[1].size = s_size;
	} else {
		prebuf[1].base = d_frame->addr.y;
		prebuf[1].size = d_size;
	}

	exynos_sysmmu_set_pbuf(g2d->dev, G2D_MAX_PBUF, prebuf);
#endif
	g2d_dbg("Done\n");
}

static void g2d_m2m_device_run(void *priv)
{
	enum image_sel srcsel, dstsel;
	struct g2d_ctx *ctx = priv;
	struct g2d_dev *g2d;
	struct g2d_frame *s_frame, *d_frame;

	g2d = ctx->g2d_dev;

	g2d_dbg("enter\n");

	if (test_bit(DEV_RUN, &g2d->state)) {
		dev_err(g2d->dev, "G2D is already in progress\n");
		return;
	}

	if (test_bit(DEV_SUSPEND, &g2d->state)) {
		dev_err(g2d->dev, "G2D is in suspend state\n");
		return;
	}

	if (test_bit(CTX_ABORT, &ctx->flags)) {
		dev_err(g2d->dev, "aborted G2D device run\n");
		return;
	}

#ifdef CONFIG_PM_RUNTIME
	if (in_irq())
		pm_runtime_get(g2d->dev);
	else
		pm_runtime_get_sync(g2d->dev);
#endif

	g2d_clock_gating(g2d, G2D_CLK_ON);

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	g2d_hwset_init(g2d);

#ifdef CONFIG_FIMG2D_CCI_SNOOP
	g2d_set_cci_snoop(ctx);
	g2d_hwset_cci_on(g2d);
#endif

	/* FIMG2D_SRC_COLOR_MODE_REG */
	g2d_hwset_src_image_format(g2d, s_frame->g2d_fmt->pixelformat);
	/* FIMG2D_DST_COLOR_MODE_REG */
	g2d_hwset_dst_image_format(g2d, d_frame->g2d_fmt->pixelformat);

	/* FIMG2D_SRC_BASE_ADDR_REG */
	/* FIMG2D_SRC_STRIDE_REG */
	/* FIMG2D_SRC_PLANE2_BASE_ADDR_REG */
	/* FIMG2D_DST_BASE_ADDR_REG */
	/* FIMG2D_DST_STRIDE_REG */
	/* FIMG2D_DST_PLANE2_BASE_ADDR_REG */
	g2d_set_frame_addr(ctx);

	/* FIMG2D_SRC_LEFT_TOP_REG */
	/* FIMG2D_SRC_RIGHT_BOTTOM_REG */
	g2d_hwset_src_rect(g2d, &s_frame->crop);

	/* FIMG2D_DST_LEFT_TOP_REG */
	/* FIMG2D_DST_RIGHT_BOTTOM_REG */
	g2d_hwset_dst_rect(g2d, &d_frame->crop);

	if (d_frame->clip_enable) {
		g2d_v4l2_s_clip(ctx, &d_frame->clip);
		g2d_hwset_enable_clipping(g2d, &d_frame->clip);
	} else {
		d_frame->clip_enable = 1;
		d_frame->clip.left = 0;
		d_frame->clip.width  = d_frame->pix_mp.width;
		d_frame->clip.height = d_frame->pix_mp.height;
		g2d_v4l2_s_clip(ctx, &d_frame->clip);
		g2d_hwset_enable_clipping(g2d, &d_frame->clip);
	}

	if (ctx->rep.mode)
		g2d_hwset_src_repeat(g2d, &ctx->rep);

	if (ctx->scale.mode)
		g2d_hwset_src_scaling(g2d, &ctx->scale, &ctx->rep);

	if (ctx->bluesc.mode) {
		g2d_hwset_bluescreen(g2d, &ctx->bluesc);
	}

	g2d_hwset_rotation(g2d, ctx->flip, ctx->rotation);
	if (ctx->dith)
		g2d_hwset_enable_dithering(g2d);

	g2d_dbg("ctx->op:%d\n", ctx->op);

	/* src and dst select */
	srcsel = dstsel = IMG_MEMORY;

	switch (ctx->op) {
	case BLIT_OP_CLR:
		srcsel = dstsel = IMG_FGCOLOR;
		/* FIMG2D_BITBLT_COMMAND_REG */
		/* FIMG2D_SF_COLOR_REG */
		/* FIMG2D_AXI_MODE_REG */
		g2d_hwset_color_fill(g2d, 0);
		break;
	case BLIT_OP_DST:
		srcsel = dstsel = IMG_FGCOLOR;
		break;
	default:
		if (ctx->color_fill) {
			srcsel = IMG_FGCOLOR;
			g2d_dbg("srcsel:%d, ctx->color:%d\n"
					, srcsel, ctx->color);
			/* FIMG2D_FG_COLOR_REG */
			g2d_hwset_fgcolor(g2d, ctx->color);
		}

		if (ctx->op == BLIT_OP_SRC)
			dstsel = IMG_FGCOLOR;

		/* FIMG2D_BITBLT_COMMAND_REG */
		/* FIMG2D_ALPHA_REG */
		g2d_hwset_enable_alpha(g2d, ctx->g_alpha);

		/* FIMG2D_BLEND_FUNCTION_REG */
		/* FIMG2D_ROUND_MODE_REG */
		g2d_hwset_alpha_composite(g2d, ctx->op, ctx->g_alpha);

		if (!ctx->pre_multi)
			/* FIMG2D_BITBLT_COMMAND_REG */
			g2d_hwset_premultiplied(g2d);
	}

	/* FIMG2D_SRC_SELECT_REG */
	g2d_hwset_src_type(g2d, srcsel);
	/* FIMG2D_DST_SELECT_REG */
	g2d_hwset_dst_type(g2d, dstsel);

	g2d_hwset_int_enable(g2d);

	g2d_dbg("after g2d_hwset_int_enable\n");

	set_bit(DEV_RUN, &g2d->state);
	set_bit(CTX_RUN, &ctx->flags);

	g2d_dbg("before g2d_hwset_start blt()\n");
	if (g2d_log_level)
		g2d_hwset_dump_regs(g2d);

	g2d->start_time = sched_clock();

	g2d_hwset_start_blit(g2d);
	/* g2d->wdt.timer.expires = jiffies + G2D_TIMEOUT; */
	/* add_timer(&g2d->wdt.timer); */
}

static void g2d_m2m_job_abort(void *priv)
{
	struct g2d_ctx *ctx = priv;
	int ret;

	ret = g2d_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->g2d_dev->dev, "wait timeout\n");
}

static struct v4l2_m2m_ops g2d_m2m_ops = {
	.device_run	= g2d_m2m_device_run,
	.job_abort	= g2d_m2m_job_abort,
};

static int g2d_register_m2m_device(struct g2d_dev *g2d)
{
	struct v4l2_device *v4l2_dev;
	struct device *dev;
	struct video_device *vfd;
	int ret = 0;

	if (!g2d)
		return -ENODEV;

	dev = g2d->dev;
	v4l2_dev = &g2d->m2m.v4l2_dev;

	snprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s.m2m",
			MODULE_NAME);

	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret) {
		dev_err(g2d->dev, "failed to register v4l2 device\n");
		return ret;
	}

	vfd = video_device_alloc();
	if (!vfd) {
		dev_err(g2d->dev, "failed to allocate video device\n");
		goto err_v4l2_dev;
	}

	vfd->fops	= &g2d_v4l2_fops;
	vfd->ioctl_ops	= &g2d_v4l2_ioctl_ops;
	vfd->release	= video_device_release;
	vfd->lock	= &g2d->lock;
	vfd->vfl_dir	= VFL_DIR_M2M;
	snprintf(vfd->name, sizeof(vfd->name), "%s:m2m", MODULE_NAME);

	video_set_drvdata(vfd, g2d);

	g2d->m2m.vfd = vfd;
	g2d->m2m.m2m_dev = v4l2_m2m_init(&g2d_m2m_ops);
	if (IS_ERR(g2d->m2m.m2m_dev)) {
		dev_err(g2d->dev, "failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(g2d->m2m.m2m_dev);
		goto err_dev_alloc;
	}

	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
						EXYNOS_VIDEONODE_FIMG2D);
	if (ret) {
		dev_err(g2d->dev, "failed to register video device\n");
		goto err_m2m_dev;
	}

	return 0;

err_m2m_dev:
	v4l2_m2m_release(g2d->m2m.m2m_dev);
err_dev_alloc:
	video_device_release(g2d->m2m.vfd);
err_v4l2_dev:
	v4l2_device_unregister(v4l2_dev);

	return ret;
}

static int g2d_runtime_suspend(struct device *dev)
{
	return 0;
}

static int g2d_runtime_resume(struct device *dev)
{
	struct g2d_dev *g2d = dev_get_drvdata(dev);
	int ret = 0;

	g2d_clock_resume(g2d);

	ret = g2d_dynamic_clock_gating(IP_VER_G2D_5H);
	if (ret)
		dev_err(g2d->dev, "failed to g2d dynamic clock gating\n");

	return 0;
}

static int g2d_suspend(struct device *dev)
{
	struct g2d_dev *g2d = dev_get_drvdata(dev);
	int ret;

	set_bit(DEV_SUSPEND, &g2d->state);

	ret = wait_event_timeout(g2d->wait,
			!test_bit(DEV_RUN, &g2d->state), G2D_TIMEOUT);
	if (ret == 0)
		dev_err(g2d->dev, "wait timeout\n");

	return 0;
}

static int g2d_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops g2d_pm_ops = {
	.suspend		= g2d_suspend,
	.resume			= g2d_resume,
	.runtime_suspend	= g2d_runtime_suspend,
	.runtime_resume		= g2d_runtime_resume,
};

static void g2d_clk_put(struct g2d_dev *g2d)
{
	clk_unprepare(g2d->clk);
	clk_put(g2d->clk);
}

static int g2d_clk_get(struct g2d_dev *g2d)
{
	int ret;
	char *parn1_clkname, *chld1_clkname;
	char *parn2_clkname, *chld2_clkname;
	char *gate_clkname;

	of_property_read_string_index(g2d->dev->of_node,
		"clock-names", G2D_GATE_CLK, (const char **)&gate_clkname);
	of_property_read_string_index(g2d->dev->of_node,
		"clock-names", G2D_PARN1_CLK, (const char **)&parn1_clkname);
	of_property_read_string_index(g2d->dev->of_node,
		"clock-names", G2D_CHLD1_CLK, (const char **)&chld1_clkname);
	of_property_read_string_index(g2d->dev->of_node,
		"clock-names", G2D_PARN2_CLK, (const char **)&parn2_clkname);
	of_property_read_string_index(g2d->dev->of_node,
		"clock-names", G2D_CHLD2_CLK, (const char **)&chld2_clkname);


	g2d_dbg("clknames: parent1 %s, child1 %s, parent2 %s, child2 %s, gate %s\n"
			, parn1_clkname, chld1_clkname
			, parn2_clkname, chld2_clkname, gate_clkname);

	g2d->clk_parn1 = clk_get(g2d->dev, parn1_clkname);
	if (IS_ERR(g2d->clk_parn1)) {
		dev_err(g2d->dev, "failed to get parent1 clk\n");
		goto err_clk_get_parn1;
	}

	g2d->clk_chld1 = clk_get(g2d->dev, chld1_clkname);
	if (IS_ERR(g2d->clk_chld1)) {
		dev_err(g2d->dev, "failed to get child1 clk\n");
		goto err_clk_get_chld1;
	}

	g2d->clk_parn2 = clk_get(g2d->dev, parn2_clkname);
	if (IS_ERR(g2d->clk_parn2)) {
		dev_err(g2d->dev, "failed to get parent2 clk\n");
		goto err_clk_get_parn2;
	}

	g2d->clk_chld2 = clk_get(g2d->dev, chld2_clkname);
	if (IS_ERR(g2d->clk_chld2)) {
		dev_err(g2d->dev, "failed to get child2 clk\n");
		goto err_clk_get_chld2;
	}

	/* clock for gating */
	g2d->clk = clk_get(g2d->dev, "fimg2d");
	if (IS_ERR(g2d->clk)) {
		dev_err(g2d->dev, "failed to get gate clk\n");
		goto err_clk_get;
	}

	ret = clk_prepare(g2d->clk);
	if (ret < 0) {
		dev_err(g2d->dev, "failed to prepare gate clk\n");
		goto err_clk_prepare;
	}

	g2d_dbg("Done sclk_fimg2d\n");
	return 0;

err_clk_prepare:
	clk_put(g2d->clk);
err_clk_get:
	clk_put(g2d->clk_chld2);
err_clk_get_chld2:
	clk_put(g2d->clk_parn2);
err_clk_get_parn2:
	clk_put(g2d->clk_chld1);
err_clk_get_chld1:
	clk_put(g2d->clk_parn1);
err_clk_get_parn1:
	return -ENXIO;
}

#ifdef CONFIG_OF
static void g2d_parse_dt(struct device_node *np, struct g2d_dev *g2d)
{
	struct fimg2d_platdata *pdata = g2d->pdata;

	if (!np)
		return;

	of_property_read_u32(np, "ip_ver", &pdata->ip_ver);
	of_property_read_u32(np, "cpu_min", &pdata->cpu_min);
	of_property_read_u32(np, "mif_min", &pdata->mif_min);
	of_property_read_u32(np, "int_min", &pdata->int_min);

	g2d_dbg("ip_ver:%x cpu_min:%d, mif_min:%d, int_min:%d\n"
			, pdata->ip_ver, pdata->cpu_min
			, pdata->mif_min, pdata->int_min);
}
#else
static void g2d_parse_dt(struct device_node *np, struct g2d_dev *gsc)
{
	return;
}
#endif

static const struct of_device_id exynos_fimg2d_match[] = {
	{
		.compatible = "samsung,s5p-fimg2d",
	},
	{},
};

MODULE_DEVICE_TABLE(of, exynos_fimg2d_match);

static int g2d_probe(struct platform_device *pdev)
{
	struct g2d_dev *g2d;
	struct resource *res;
	struct fimg2d_platdata *pdata = NULL;
	struct device *dev = &pdev->dev;
	int ret = 0;

	dev_info(dev, "++%s\n", __func__);

	g2d = devm_kzalloc(dev, sizeof(struct g2d_dev), GFP_KERNEL);
	if (!g2d) {
		dev_err(dev, "no memory for g2d device\n");
		return -ENOMEM;
	}

	if (dev->of_node) {
		g2d->id = of_alias_get_id(pdev->dev.of_node, "fimg2d");
	} else {
		g2d->id = pdev->id;
		pdata = dev->platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data\n");
			return -EINVAL;
		}
	}

	if (g2d->id < 0) {
		dev_err(dev, "Invalid platform device id: %d\n", g2d->id);
		return -EINVAL;
	}

	g2d->dev = &pdev->dev;

	g2d->pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!g2d->pdata) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	if (pdata) {
		memcpy(g2d->pdata, pdata, sizeof(*pdata));
	} else {
		g2d_parse_dt(dev->of_node, g2d);
		pdata = g2d->pdata;
	}

	spin_lock_init(&g2d->slock);
	mutex_init(&g2d->lock);

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	g2d->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (g2d->regs == NULL) {
		dev_err(&pdev->dev, "failed to claim register region\n");
		return -ENOENT;
	}

	g2d_dbg("res->start : 0x%x\n", res->start);

	/* Get IRQ resource and register IRQ handler. */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get IRQ resource\n");
		return -ENXIO;
	}

	g2d_dbg("res->start(irq) : %d\n", res->start);

	ret = devm_request_irq(&pdev->dev, res->start
			, g2d_irq_handler, 0, pdev->name, g2d);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq\n");
		return ret;
	}

	atomic_set(&g2d->wdt.cnt, 0);
	setup_timer(&g2d->wdt.timer, g2d_watchdog, (unsigned long)g2d);

	ret = g2d_clk_get(g2d);
	if (ret)
		return ret;

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	g2d->vb2 = &g2d_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
	g2d->vb2 = &g2d_vb2_ion;
#endif

	g2d->alloc_ctx = g2d->vb2->init(g2d, G2D_CCI_OFF);
	if (IS_ERR_OR_NULL(g2d->alloc_ctx)) {
		ret = PTR_ERR(g2d->alloc_ctx);
		dev_err(&pdev->dev, "failed to alloc_ctx\n");
		goto err_clk_put;
	}

	g2d->alloc_ctx_cci = g2d->vb2->init(g2d, G2D_CCI_ON);
	if (IS_ERR_OR_NULL(g2d->alloc_ctx_cci)) {
		ret = PTR_ERR(g2d->alloc_ctx_cci);
		dev_err(&pdev->dev, "failed to alloc_ctx_cci\n");
		goto err_clk_put;
	}

	exynos_create_iovmm(&pdev->dev, 3, 3);
	g2d->vb2->resume(g2d->alloc_ctx);
	g2d->vb2->resume(g2d->alloc_ctx_cci);

	platform_set_drvdata(pdev, g2d);

	ret = g2d_register_m2m_device(g2d);
	if (ret) {
		dev_err(&pdev->dev, "failed to register m2m device\n");
		ret = -EPERM;
		goto err_clk_put;
	}

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&pdev->dev);
#endif

	g2d_clock_gating(g2d, G2D_CLK_ON);
	g2d_clock_gating(g2d, G2D_CLK_OFF);

	ret = g2d_cci_snoop_init(IP_VER_G2D_5H);
	if (ret) {
		dev_err(&pdev->dev, "failed to init g2d cci snoop\n");
		goto err_clk_put;
	}

	ret = g2d_dynamic_clock_gating(IP_VER_G2D_5H);
	if (ret) {
		dev_err(&pdev->dev, "failed to g2d dynamic clock gating\n");
		goto err_clk_put;
	}

	g2d->variant = &variant;

	dev_info(&pdev->dev, "G2D registered successfully\n");

	return 0;

err_clk_put:
	g2d_clk_put(g2d);

	return ret;
}

static int g2d_remove(struct platform_device *pdev)
{
	struct g2d_dev *g2d =
		(struct g2d_dev *)platform_get_drvdata(pdev);

	g2d->vb2->cleanup(g2d->alloc_ctx);
	g2d->vb2->cleanup(g2d->alloc_ctx_cci);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#endif
	g2d_clk_put(g2d);
	g2d_cci_snoop_remove(IP_VER_G2D_5H);

	g2d->vb2->suspend(g2d->alloc_ctx);
	g2d->vb2->suspend(g2d->alloc_ctx_cci);

	if (timer_pending(&g2d->wdt.timer))
		del_timer(&g2d->wdt.timer);

	return 0;
}

static struct platform_device_id g2d_driver_ids[] = {
	{
		.name	= MODULE_NAME,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, g2d_driver_ids);

static struct platform_driver g2d_driver = {
	.probe		= g2d_probe,
	.remove		= g2d_remove,
	.id_table	= g2d_driver_ids,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &g2d_pm_ops,
		.of_match_table = exynos_fimg2d_match,
	}
};

module_platform_driver(g2d_driver);
