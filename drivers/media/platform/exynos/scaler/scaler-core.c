/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS Scaler driver
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
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>

#include <media/v4l2-ioctl.h>
#include <mach/videonode.h>
#include <plat/cpu.h>

#include "scaler.h"

int sc_log_level;
module_param_named(sc_log_level, sc_log_level, uint, 0644);

/*
 * If true, writes the latency of H/W operation to v4l2_buffer.reserved2
 * in the unit of nano seconds.  It must not be enabled with real use-case
 * because v4l2_buffer.reserved may be used for other purpose.
 * The latency is written to the destination buffer.
 */
int __measure_hw_latency;
module_param_named(measure_hw_latency, __measure_hw_latency, int, 0644);

static struct sc_fmt sc_formats[] = {
	{
		.name		= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_RGB,
	}, {
		.name		= "RGB1555",
		.pixelformat	= V4L2_PIX_FMT_RGB555X,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_RGB,
	}, {
		.name		= "ARGB4444",
		.pixelformat	= V4L2_PIX_FMT_RGB444,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_RGB,
	}, {
		.name		= "RGBA8888",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 32 },
		.color		= SC_COLOR_RGB,
	}, {
		.name		= "BGRA8888",
		.pixelformat	= V4L2_PIX_FMT_BGR32,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 32 },
		.color		= SC_COLOR_RGB,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 12 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 12 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 8, 4 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 8, 4 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr, tiled",
		.pixelformat	= V4L2_PIX_FMT_NV12MT_16X16,
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 8, 4 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,	/* I420 */
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 12 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YVU 4:2:0 contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420,	/* YV12 */
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 12 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 8, 2, 2 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
		.bitperpixel	= { 8, 2, 2 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 packed, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.num_planes	= 1,
		.num_comp	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:2:2 contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
		.bitperpixel	= { 16 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:4:4 contiguous Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV24,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 24 },
		.color		= SC_COLOR_YUV,
	}, {
		.name		= "YUV 4:4:4 contiguous Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV42,
		.num_planes	= 1,
		.num_comp	= 2,
		.bitperpixel	= { 24 },
		.color		= SC_COLOR_YUV,
	},
};

#define SCALE_RATIO(x, y)	((65536 * x) / y)

static struct sc_variant variant = {
	.limit_input = {
		.min_w		= 16,
		.min_h		= 16,
		.max_w		= 8192,
		.max_h		= 8192,
		.align_w	= 0,
		.align_h	= 0,
	},
	.limit_output = {
		.min_w		= 4,
		.min_h		= 4,
		.max_w		= 8192,
		.max_h		= 8192,
		.align_w	= 0,
		.align_h	= 0,
	},
	.sc_up_max		= SCALE_RATIO(1, 16),
	.sc_down_min		= SCALE_RATIO(4, 1),
	.sc_down_swmin		= SCALE_RATIO(16, 1),
};

/* Find the matches format */
static struct sc_fmt *sc_find_format(struct sc_dev *sc, struct v4l2_format *f)
{
	struct sc_fmt *sc_fmt;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sc_formats); ++i) {
		sc_fmt = &sc_formats[i];
		if (sc_fmt->pixelformat == f->fmt.pix_mp.pixelformat) {
			if (!V4L2_TYPE_IS_OUTPUT(f->type) &&
				sc_fmt->pixelformat == V4L2_PIX_FMT_NV12MT_16X16)
				return NULL;
			else
				return &sc_formats[i];
		}
	}

	return NULL;
}

static int sc_v4l2_querycap(struct file *file, void *fh,
			     struct v4l2_capability *cap)
{
	strncpy(cap->driver, MODULE_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, MODULE_NAME, sizeof(cap->card) - 1);

	cap->bus_info[0] = 0;
	cap->capabilities = V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;

	return 0;
}

static int sc_v4l2_enum_fmt_mplane(struct file *file, void *fh,
			     struct v4l2_fmtdesc *f)
{
	struct sc_fmt *sc_fmt;

	if (f->index >= ARRAY_SIZE(sc_formats))
		return -EINVAL;

	sc_fmt = &sc_formats[f->index];
	strncpy(f->description, sc_fmt->name, sizeof(f->description) - 1);
	f->pixelformat = sc_fmt->pixelformat;

	return 0;
}

static int sc_v4l2_g_fmt_mplane(struct file *file, void *fh,
			  struct v4l2_format *f)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct sc_fmt *sc_fmt;
	struct sc_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int i;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	sc_fmt = frame->sc_fmt;

	pixm->width		= frame->width;
	pixm->height		= frame->height;
	pixm->pixelformat	= frame->pixelformat;
	pixm->field		= V4L2_FIELD_NONE;
	pixm->num_planes	= frame->sc_fmt->num_planes;
	pixm->colorspace	= 0;

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width *
				sc_fmt->bitperpixel[i]) >> 3;
		if (sc_fmt_is_ayv12(sc_fmt->pixelformat)) {
			unsigned int y_size, c_span;
			y_size = pixm->width * pixm->height;
			c_span = ALIGN(pixm->width >> 1, 16);
			pixm->plane_fmt[i].sizeimage =
				y_size + (c_span * pixm->height >> 1) * 2;
		} else {
			pixm->plane_fmt[i].sizeimage =
				pixm->plane_fmt[i].bytesperline * pixm->height;
		}

		v4l2_dbg(1, sc_log_level, &ctx->sc_dev->m2m.v4l2_dev,
				"[%d] plane: bytesperline %d, sizeimage %d\n",
				i, pixm->plane_fmt[i].bytesperline,
				pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int sc_v4l2_try_fmt_mplane(struct file *file, void *fh,
			    struct v4l2_format *f)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct sc_fmt *sc_fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct sc_size_limit *limit;
	int i;

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
				"not supported v4l2 type\n");
		return -EINVAL;
	}

	sc_fmt = sc_find_format(ctx->sc_dev, f);
	if (!sc_fmt) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
				"not supported format type\n");
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type))
		limit = &ctx->sc_dev->variant->limit_input;
	else
		limit = &ctx->sc_dev->variant->limit_output;

	/*
	 * Y_SPAN - should even in interleaved YCbCr422
	 * C_SPAN - should even in YCbCr420 and YCbCr422
	 */
	if (sc_fmt_is_yuv422(sc_fmt->pixelformat) ||
			sc_fmt_is_yuv420(sc_fmt->pixelformat))
		limit->align_w = 1;

	/* Bound an image to have width and height in limit */
	v4l_bound_align_image(&pixm->width, limit->min_w, limit->max_w,
			limit->align_w, &pixm->height, limit->min_h,
			limit->max_h, limit->align_h, 0);

	pixm->num_planes = sc_fmt->num_planes;
	pixm->colorspace = 0;

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width *
				sc_fmt->bitperpixel[i]) >> 3;
		if (sc_fmt_is_ayv12(sc_fmt->pixelformat)) {
			unsigned int y_size, c_span;
			y_size = pixm->width * pixm->height;
			c_span = ALIGN(pixm->width >> 1, 16);
			pixm->plane_fmt[i].sizeimage =
				y_size + (c_span * pixm->height >> 1) * 2;
		} else {
			pixm->plane_fmt[i].sizeimage =
				pixm->plane_fmt[i].bytesperline * pixm->height;
		}

		v4l2_dbg(1, sc_log_level, &ctx->sc_dev->m2m.v4l2_dev,
				"[%d] plane: bytesperline %d, sizeimage %d\n",
				i, pixm->plane_fmt[i].bytesperline,
				pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int sc_v4l2_s_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)

{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	struct sc_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct sc_size_limit *limitout = &ctx->sc_dev->variant->limit_input;
	struct sc_size_limit *limitcap = &ctx->sc_dev->variant->limit_output;
	int i, ret = 0;

	if (vb2_is_streaming(vq)) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev, "device is busy\n");
		return -EBUSY;
	}

	ret = sc_v4l2_try_fmt_mplane(file, fh, f);
	if (ret < 0)
		return ret;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	set_bit(CTX_PARAMS, &ctx->flags);

	frame->sc_fmt = sc_find_format(ctx->sc_dev, f);
	if (!frame->sc_fmt) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
				"not supported format values\n");
		return -EINVAL;
	}

	for (i = 0; i < frame->sc_fmt->num_planes; i++)
		frame->bytesused[i] = pixm->plane_fmt[i].sizeimage;

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width > limitout->max_w) ||
			 (pixm->height > limitout->max_h))) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too large\n",
			pixm->width, pixm->height);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width > limitcap->max_w) ||
			 (pixm->height > limitcap->max_h))) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
			"%dx%d of target image is not supported: too large\n",
			pixm->width, pixm->height);
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width < limitout->min_w) ||
			 (pixm->height < limitout->min_h))) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too small\n",
			pixm->width, pixm->height);
		return -EINVAL;
	}

	if (!V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width < limitcap->min_w) ||
			 (pixm->height < limitcap->min_h))) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
			"%dx%d of target image is not supported: too small\n",
			pixm->width, pixm->height);
		return -EINVAL;
	}

	frame->width = pixm->width;
	frame->height = pixm->height;
	frame->pixelformat = pixm->pixelformat;

	frame->crop.width = pixm->width;
	frame->crop.height = pixm->height;

	return 0;
}

static int sc_v4l2_reqbufs(struct file *file, void *fh,
			    struct v4l2_requestbuffers *reqbufs)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct sc_dev *sc = ctx->sc_dev;

	sc->vb2->set_cacheable(sc->alloc_ctx, ctx->cacheable);

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int sc_v4l2_querybuf(struct file *file, void *fh,
			     struct v4l2_buffer *buf)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	return v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);
}

static int sc_v4l2_qbuf(struct file *file, void *fh,
			 struct v4l2_buffer *buf)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}

static int sc_v4l2_dqbuf(struct file *file, void *fh,
			  struct v4l2_buffer *buf)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int sc_v4l2_streamon(struct file *file, void *fh,
			     enum v4l2_buf_type type)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int sc_v4l2_streamoff(struct file *file, void *fh,
			      enum v4l2_buf_type type)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	return v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);
}

static int sc_v4l2_cropcap(struct file *file, void *fh,
			    struct v4l2_cropcap *cr)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct sc_frame *frame;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	cr->bounds.left		= 0;
	cr->bounds.top		= 0;
	cr->bounds.width	= frame->width;
	cr->bounds.height	= frame->height;
	cr->defrect		= cr->bounds;

	return 0;
}

static int sc_v4l2_g_crop(struct file *file, void *fh, struct v4l2_crop *cr)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct sc_frame *frame;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	cr->c = frame->crop;

	return 0;
}

static int sc_v4l2_s_crop(struct file *file, void *fh, struct v4l2_crop *cr)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(fh);
	struct sc_dev *sc = ctx->sc_dev;
	struct sc_frame *frame;
	struct sc_size_limit *limit = NULL;
	int x_align = 0, y_align = 0;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!test_bit(CTX_PARAMS, &ctx->flags)) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
				"color format is not set\n");
		return -EINVAL;
	}

	if (cr->c.left < 0 || cr->c.top < 0 ||
			cr->c.width < 0 || cr->c.height < 0) {
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
				"crop value is negative\n");
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(cr->type)) {
		limit = &sc->variant->limit_input;
		set_bit(CTX_SRC_FMT, &ctx->flags);
	} else {
		limit = &sc->variant->limit_output;
		set_bit(CTX_DST_FMT, &ctx->flags);
	}

	if (sc_fmt_is_yuv422(frame->sc_fmt->pixelformat)) {
		limit->align_w = 1;
	} else if (sc_fmt_is_yuv420(frame->sc_fmt->pixelformat)) {
		limit->align_w = 1;
		limit->align_h = 1;
	}

	/* Bound an image to have crop width and height in limit */
	v4l_bound_align_image(&cr->c.width, limit->min_w, limit->max_w,
			limit->align_w, &cr->c.height, limit->min_h,
			limit->max_h, limit->align_h, 0);

	if (V4L2_TYPE_IS_OUTPUT(cr->type)) {
		if (sc_fmt_is_yuv422(frame->sc_fmt->pixelformat))
			x_align = 1;
	} else {
		if (sc_fmt_is_yuv422(frame->sc_fmt->pixelformat)) {
			x_align = 1;
		} else if (sc_fmt_is_yuv420(frame->sc_fmt->pixelformat)) {
			x_align = 1;
			y_align = 1;
		}
	}

	/* Bound an image to have crop position in limit */
	v4l_bound_align_image(&cr->c.left, 0, frame->width - cr->c.width,
			x_align, &cr->c.top, 0, frame->height - cr->c.height,
			y_align, 0);

	frame->crop.top = cr->c.top;
	frame->crop.left = cr->c.left;
	frame->crop.height = cr->c.height;
	frame->crop.width = cr->c.width;

	return 0;
}

static const struct v4l2_ioctl_ops sc_v4l2_ioctl_ops = {
	.vidioc_querycap		= sc_v4l2_querycap,

	.vidioc_enum_fmt_vid_cap_mplane	= sc_v4l2_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_out_mplane	= sc_v4l2_enum_fmt_mplane,

	.vidioc_g_fmt_vid_cap_mplane	= sc_v4l2_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= sc_v4l2_g_fmt_mplane,

	.vidioc_try_fmt_vid_cap_mplane	= sc_v4l2_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= sc_v4l2_try_fmt_mplane,

	.vidioc_s_fmt_vid_cap_mplane	= sc_v4l2_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= sc_v4l2_s_fmt_mplane,

	.vidioc_reqbufs			= sc_v4l2_reqbufs,
	.vidioc_querybuf		= sc_v4l2_querybuf,

	.vidioc_qbuf			= sc_v4l2_qbuf,
	.vidioc_dqbuf			= sc_v4l2_dqbuf,

	.vidioc_streamon		= sc_v4l2_streamon,
	.vidioc_streamoff		= sc_v4l2_streamoff,

	.vidioc_g_crop			= sc_v4l2_g_crop,
	.vidioc_s_crop			= sc_v4l2_s_crop,
	.vidioc_cropcap			= sc_v4l2_cropcap
};

static int sc_ctx_stop_req(struct sc_ctx *ctx)
{
	struct sc_ctx *curr_ctx;
	struct sc_dev *sc = ctx->sc_dev;
	int ret = 0;

	curr_ctx = v4l2_m2m_get_curr_priv(sc->m2m.m2m_dev);
	if (!test_bit(CTX_RUN, &ctx->flags) || (curr_ctx != ctx))
		return 0;

	set_bit(CTX_ABORT, &ctx->flags);

	ret = wait_event_timeout(sc->wait,
			!test_bit(CTX_RUN, &ctx->flags), SC_TIMEOUT);

	/* TODO: How to handle case of timeout event */
	if (ret == 0) {
		dev_err(sc->dev, "device failed to stop request\n");
		ret = -EBUSY;
	}

	return ret;
}

static void sc_calc_intbufsize(struct sc_dev *sc, struct sc_int_frame *int_frame)
{
	struct sc_frame *frame = &int_frame->frame;
	unsigned int pixsize, bytesize;

	pixsize = frame->width * frame->height;
	bytesize = (pixsize * frame->sc_fmt->bitperpixel[0]) >> 3;

	switch (frame->sc_fmt->num_comp) {
	case 1:
		frame->addr.ysize = bytesize;
		break;
	case 2:
		if (frame->sc_fmt->num_planes == 1) {
			frame->addr.ysize = pixsize;
			frame->addr.cbsize = bytesize - pixsize;
		} else if (frame->sc_fmt->num_planes == 2) {
			frame->addr.ysize =
				(pixsize * frame->sc_fmt->bitperpixel[0]) / 8;
			frame->addr.cbsize =
				(pixsize * frame->sc_fmt->bitperpixel[1]) / 8;
		}
		break;
	case 3:
		if (frame->sc_fmt->num_planes == 1) {
			if (sc_fmt_is_ayv12(frame->sc_fmt->pixelformat)) {
				unsigned int c_span;
				c_span = ALIGN(frame->width >> 1, 16);
				frame->addr.ysize = pixsize;
				frame->addr.cbsize = c_span * (frame->height >> 1);
				frame->addr.crsize = frame->addr.cbsize;
			} else {
				frame->addr.ysize = pixsize;
				frame->addr.cbsize = (bytesize - pixsize) / 2;
				frame->addr.crsize = frame->addr.cbsize;
			}
		} else if (frame->sc_fmt->num_planes == 3) {
			frame->addr.ysize =
				(pixsize * frame->sc_fmt->bitperpixel[0]) / 8;
			frame->addr.cbsize =
				(pixsize * frame->sc_fmt->bitperpixel[1]) / 8;
			frame->addr.crsize =
				(pixsize * frame->sc_fmt->bitperpixel[2]) / 8;
		} else {
			dev_err(sc->dev, "Please check the num of comp\n");
		}

		break;
	default:
		break;
	}

	memcpy(&int_frame->src_addr, &frame->addr, sizeof(int_frame->src_addr));
	memcpy(&int_frame->dst_addr, &frame->addr, sizeof(int_frame->dst_addr));
}

extern struct ion_device *ion_exynos;

static void free_intermediate_frame(struct sc_ctx *ctx)
{

	if (ctx->i_frame == NULL)
		return;

	if (!ctx->i_frame->handle[0])
		return;

	ion_free(ctx->i_frame->client, ctx->i_frame->handle[0]);

	if (ctx->i_frame->handle[1])
		ion_free(ctx->i_frame->client, ctx->i_frame->handle[1]);
	if (ctx->i_frame->handle[2])
		ion_free(ctx->i_frame->client, ctx->i_frame->handle[2]);

	if (ctx->i_frame->src_addr.y)
		iovmm_unmap(ctx->sc_dev->dev, ctx->i_frame->src_addr.y);
	if (ctx->i_frame->src_addr.cb)
		iovmm_unmap(ctx->sc_dev->dev, ctx->i_frame->src_addr.cb);
	if (ctx->i_frame->src_addr.cr)
		iovmm_unmap(ctx->sc_dev->dev, ctx->i_frame->src_addr.cr);
	if (ctx->i_frame->dst_addr.y)
		iovmm_unmap(ctx->sc_dev->dev, ctx->i_frame->dst_addr.y);
	if (ctx->i_frame->dst_addr.cb)
		iovmm_unmap(ctx->sc_dev->dev, ctx->i_frame->dst_addr.cb);
	if (ctx->i_frame->dst_addr.cr)
		iovmm_unmap(ctx->sc_dev->dev, ctx->i_frame->dst_addr.cr);

	memset(&ctx->i_frame->handle, 0, sizeof(struct ion_handle *) * 3);
	memset(&ctx->i_frame->src_addr, 0, sizeof(ctx->i_frame->src_addr));
	memset(&ctx->i_frame->dst_addr, 0, sizeof(ctx->i_frame->dst_addr));
}

static void destroy_intermediate_frame(struct sc_ctx *ctx)
{
	if (ctx->i_frame) {
		free_intermediate_frame(ctx);
		ion_client_destroy(ctx->i_frame->client);
		kfree(ctx->i_frame);
		ctx->i_frame = NULL;
		clear_bit(CTX_INT_FRAME, &ctx->flags);
	}
}

static bool initialize_initermediate_frame(struct sc_ctx *ctx)
{
	struct sc_frame *frame;
	struct sc_dev *sc = ctx->sc_dev;
	struct sg_table *sgt;

	frame = &ctx->i_frame->frame;

	frame->crop.top = 0;
	frame->crop.left = 0;
	frame->width = frame->crop.width;
	frame->height = frame->crop.height;

	/*
	 * Check if intermeidate frame is already initialized by a previous
	 * frame. If it is already initialized, intermediate buffer is no longer
	 * needed to be initialized because image setting is never changed
	 * while streaming continues.
	 */
	if (ctx->i_frame->handle[0])
		return true;

	sc_calc_intbufsize(sc, ctx->i_frame);

	if (frame->addr.ysize) {
		ctx->i_frame->handle[0] = ion_alloc(ctx->i_frame->client,
				frame->addr.ysize, 0, ION_HEAP_SYSTEM_MASK, 0);
		if (IS_ERR(ctx->i_frame->handle[0])) {
			dev_err(sc->dev,
			"Failed to allocate intermediate y buffer (err %ld)",
				PTR_ERR(ctx->i_frame->handle[0]));
			ctx->i_frame->handle[0] = NULL;
			goto err_ion_alloc;
		}

		sgt = ion_sg_table(ctx->i_frame->client,
				   ctx->i_frame->handle[0]);
		if (IS_ERR(sgt)) {
			dev_err(sc->dev,
			"Failed to get sg_table from ion_handle of y (err %ld)",
			PTR_ERR(sgt));
			goto err_ion_alloc;
		}

		ctx->i_frame->src_addr.y = iovmm_map(sc->dev, sgt->sgl, 0,
					frame->addr.ysize, DMA_TO_DEVICE, 0);
		if (IS_ERR_VALUE(ctx->i_frame->src_addr.y)) {
			dev_err(sc->dev,
				"Failed to allocate iova of y (err %d)",
				ctx->i_frame->src_addr.y);
			ctx->i_frame->src_addr.y = 0;
			goto err_ion_alloc;
		}

		ctx->i_frame->dst_addr.y = iovmm_map(sc->dev, sgt->sgl, 0,
					frame->addr.ysize, DMA_FROM_DEVICE, 0);
		if (IS_ERR_VALUE(ctx->i_frame->dst_addr.y)) {
			dev_err(sc->dev,
				"Failed to allocate iova of y (err %d)",
				ctx->i_frame->dst_addr.y);
			ctx->i_frame->dst_addr.y = 0;
			goto err_ion_alloc;
		}

		frame->addr.y = ctx->i_frame->dst_addr.y;
	}

	if (frame->addr.cbsize) {
		ctx->i_frame->handle[1] = ion_alloc(ctx->i_frame->client,
				frame->addr.cbsize, 0, ION_HEAP_SYSTEM_MASK, 0);
		if (IS_ERR(ctx->i_frame->handle[1])) {
			dev_err(sc->dev,
			"Failed to allocate intermediate cb buffer (err %ld)",
				PTR_ERR(ctx->i_frame->handle[1]));
			ctx->i_frame->handle[1] = NULL;
			goto err_ion_alloc;
		}

		sgt = ion_sg_table(ctx->i_frame->client,
				   ctx->i_frame->handle[1]);
		if (IS_ERR(sgt)) {
			dev_err(sc->dev,
			"Failed to get sg_table from ion_handle of cb(err %ld)",
			PTR_ERR(sgt));
			goto err_ion_alloc;
		}

		ctx->i_frame->src_addr.cb = iovmm_map(sc->dev, sgt->sgl, 0,
					frame->addr.cbsize, DMA_TO_DEVICE, 1);
		if (IS_ERR_VALUE(ctx->i_frame->src_addr.cb)) {
			dev_err(sc->dev,
				"Failed to allocate iova of cb (err %d)",
				ctx->i_frame->src_addr.cb);
			ctx->i_frame->src_addr.cb = 0;
			goto err_ion_alloc;
		}

		ctx->i_frame->dst_addr.cb = iovmm_map(sc->dev, sgt->sgl, 0,
					frame->addr.cbsize, DMA_FROM_DEVICE, 1);
		if (IS_ERR_VALUE(ctx->i_frame->dst_addr.cb)) {
			dev_err(sc->dev,
				"Failed to allocate iova of cb (err %d)",
				ctx->i_frame->dst_addr.cb);
			ctx->i_frame->dst_addr.cb = 0;
			goto err_ion_alloc;
		}

		frame->addr.cb = ctx->i_frame->dst_addr.cb;
	}

	if (frame->addr.crsize) {
		ctx->i_frame->handle[2] = ion_alloc(ctx->i_frame->client,
				frame->addr.crsize, 0, ION_HEAP_SYSTEM_MASK, 0);
		if (IS_ERR(ctx->i_frame->handle[2])) {
			dev_err(sc->dev,
			"Failed to allocate intermediate cr buffer (err %ld)",
				PTR_ERR(ctx->i_frame->handle[2]));
			ctx->i_frame->handle[2] = NULL;
			goto err_ion_alloc;
		}

		sgt = ion_sg_table(ctx->i_frame->client,
				   ctx->i_frame->handle[2]);
		if (IS_ERR(sgt)) {
			dev_err(sc->dev,
			"Failed to get sg_table from ion_handle of cr(err %ld)",
			PTR_ERR(sgt));
			goto err_ion_alloc;
		}

		ctx->i_frame->src_addr.cr = iovmm_map(sc->dev, sgt->sgl, 0,
					frame->addr.crsize, DMA_TO_DEVICE, 2);
		if (IS_ERR_VALUE(ctx->i_frame->src_addr.cr)) {
			dev_err(sc->dev,
				"Failed to allocate iova of cr (err %d)",
				ctx->i_frame->src_addr.cr);
			ctx->i_frame->src_addr.cr = 0;
			goto err_ion_alloc;
		}

		ctx->i_frame->dst_addr.cr = iovmm_map(sc->dev, sgt->sgl, 0,
					frame->addr.crsize, DMA_FROM_DEVICE, 2);
		if (IS_ERR_VALUE(ctx->i_frame->dst_addr.cr)) {
			dev_err(sc->dev,
				"Failed to allocate iova of cr (err %d)",
				ctx->i_frame->dst_addr.cr);
			ctx->i_frame->dst_addr.cr = 0;
			goto err_ion_alloc;
		}

		frame->addr.cr = ctx->i_frame->dst_addr.cr;
	}

	return true;

err_ion_alloc:
	free_intermediate_frame(ctx);
	return false;
}

static bool allocate_intermediate_frame(struct sc_ctx *ctx)
{
	if (ctx->i_frame == NULL) {
		ctx->i_frame = kzalloc(sizeof(*ctx->i_frame), GFP_KERNEL);
		if (ctx->i_frame == NULL) {
			dev_err(ctx->sc_dev->dev,
				"Failed to allocate intermediate frame\n");
			return false;
		}

		ctx->i_frame->client = ion_client_create(ion_exynos,
							"scaler-int");
		if (IS_ERR(ctx->i_frame->client)) {
			dev_err(ctx->sc_dev->dev,
			"Failed to create ION client for int.buf.(err %ld)\n",
				PTR_ERR(ctx->i_frame->client));
			ctx->i_frame->client = NULL;
			kfree(ctx->i_frame);
			ctx->i_frame = NULL;
			return false;
		}
	}

	return true;
}

static int sc_find_scaling_ratio(struct sc_ctx *ctx)
{
	__s32 src_width, src_height;
	unsigned int h_ratio, v_ratio;
	struct sc_dev *sc = ctx->sc_dev;

	if ((ctx->s_frame.crop.width == 0) ||
			(ctx->d_frame.crop.width == 0))
		return 0; /* s_fmt is not complete */

	src_width = ctx->s_frame.crop.width;
	src_height = ctx->s_frame.crop.height;
	if ((ctx->rotation % 180) == 90)
		swap(src_width, src_height);

	h_ratio = SCALE_RATIO(src_width, ctx->d_frame.crop.width);
	v_ratio = SCALE_RATIO(src_height, ctx->d_frame.crop.height);

	if ((h_ratio > sc->variant->sc_down_swmin) ||
			(h_ratio < sc->variant->sc_up_max)) {
		dev_err(sc->dev, "Width scaling is out of range(%d -> %d)\n",
			src_width, ctx->d_frame.crop.width);
		return -EINVAL;
	}

	if ((v_ratio > sc->variant->sc_down_swmin) ||
			(v_ratio < sc->variant->sc_up_max)) {
		dev_err(sc->dev, "Height scaling is out of range(%d -> %d)\n",
			src_height, ctx->d_frame.crop.height);
		return -EINVAL;
	}

	if ((h_ratio > sc->variant->sc_down_min) ||
				(v_ratio > sc->variant->sc_down_min)) {
		struct v4l2_rect crop = ctx->d_frame.crop;
		struct sc_size_limit *limit;
		unsigned int halign = 0, walign = 0;
		__u32 pixfmt;
		struct sc_fmt *target_fmt = ctx->d_frame.sc_fmt;

		if (!allocate_intermediate_frame(ctx))
			return -ENOMEM;

		if (v_ratio > sc->variant->sc_down_min)
			crop.height = ((src_height + 7) / 8) * 2;

		if (h_ratio > sc->variant->sc_down_min)
			crop.width = ((src_width + 7) / 8) * 2;

		pixfmt = target_fmt->pixelformat;

		if (sc_fmt_is_yuv422(pixfmt)) {
			walign = 1;
		} else if (sc_fmt_is_yuv420(pixfmt)) {
			walign = 1;
			halign = 1;
		}

		limit = &sc->variant->limit_output;
		v4l_bound_align_image(&crop.width, limit->min_w, limit->max_w,
				walign, &crop.height, limit->min_h,
				limit->max_h, halign, 0);

		h_ratio = SCALE_RATIO(src_width, crop.width);
		v_ratio = SCALE_RATIO(src_height, crop.height);

		limit = &sc->variant->limit_output;
		v4l_bound_align_image(&crop.width, limit->min_w, limit->max_w,
				walign, &crop.height, limit->min_h,
				limit->max_h, halign, 0);

		h_ratio = SCALE_RATIO(src_width, crop.width);
		v_ratio = SCALE_RATIO(src_height, crop.height);

		if ((ctx->i_frame->frame.sc_fmt != ctx->d_frame.sc_fmt) ||
		    memcmp(&crop, &ctx->i_frame->frame.crop, sizeof(crop))) {
			memcpy(&ctx->i_frame->frame, &ctx->d_frame,
					sizeof(ctx->d_frame));
			memcpy(&ctx->i_frame->frame.crop, &crop, sizeof(crop));
			free_intermediate_frame(ctx);
			if (!initialize_initermediate_frame(ctx)) {
				free_intermediate_frame(ctx);
				return -ENOMEM;
			}
		}
	} else {
		destroy_intermediate_frame(ctx);
	}

	ctx->h_ratio = h_ratio;
	ctx->v_ratio = v_ratio;

	return 0;
}

static int sc_vb2_queue_setup(struct vb2_queue *vq,
		const struct v4l2_format *fmt, unsigned int *num_buffers,
		unsigned int *num_planes, unsigned int sizes[],
		void *allocators[])
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vq);
	struct sc_frame *frame;
	int ret;
	int i;

	frame = ctx_get_frame(ctx, vq->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	/* Get number of planes from format_list in driver */
	*num_planes = frame->sc_fmt->num_planes;
	for (i = 0; i < frame->sc_fmt->num_planes; i++) {
		sizes[i] = frame->bytesused[i];
		allocators[i] = ctx->sc_dev->alloc_ctx;
	}

	ret = sc_find_scaling_ratio(ctx);
	if (ret)
		return ret;

	return vb2_queue_init(vq);
}

static int sc_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct sc_frame *frame;
	int i;

	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < frame->sc_fmt->num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->bytesused[i]);
	}

	return sc_buf_sync_prepare(vb);
}

static int sc_vb2_buf_finish(struct vb2_buffer *vb)
{
	return sc_buf_sync_finish(vb);
}

static void sc_fence_work(struct work_struct *work)
{
	struct sc_ctx *ctx = container_of(work, struct sc_ctx, fence_work);
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
				dev_warn(ctx->sc_dev->dev,
					"sync_fence_wait() timeout\n");
				ret = sync_fence_wait(fence, 10 * MSEC_PER_SEC);
			}
			if (ret)
				dev_warn(ctx->sc_dev->dev,
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

static void sc_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
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

static void sc_vb2_lock(struct vb2_queue *vq)
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_lock(&ctx->sc_dev->lock);
}

static void sc_vb2_unlock(struct vb2_queue *vq)
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vq);
	mutex_unlock(&ctx->sc_dev->lock);
}

static int sc_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vq);
	set_bit(CTX_STREAMING, &ctx->flags);

	return 0;
}

static int sc_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct sc_ctx *ctx = vb2_get_drv_priv(vq);
	int ret;

	ret = sc_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->sc_dev->dev, "wait timeout\n");

	clear_bit(CTX_STREAMING, &ctx->flags);

	return ret;
}

static struct vb2_ops sc_vb2_ops = {
	.queue_setup		 = sc_vb2_queue_setup,
	.buf_prepare		 = sc_vb2_buf_prepare,
	.buf_finish		 = sc_vb2_buf_finish,
	.buf_queue		 = sc_vb2_buf_queue,
	.wait_finish		 = sc_vb2_lock,
	.wait_prepare		 = sc_vb2_unlock,
	.start_streaming	 = sc_vb2_start_streaming,
	.stop_streaming		 = sc_vb2_stop_streaming,
};

struct vb2_scaler_buffer {
	struct v4l2_m2m_buffer mb;
	unsigned long long hw_latency;
};

static int queue_init(void *priv, struct vb2_queue *src_vq,
		      struct vb2_queue *dst_vq)
{
	struct sc_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->ops = &sc_vb2_ops;
	src_vq->mem_ops = ctx->sc_dev->vb2->ops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct vb2_scaler_buffer);
	src_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->ops = &sc_vb2_ops;
	dst_vq->mem_ops = ctx->sc_dev->vb2->ops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct vb2_scaler_buffer);
	dst_vq->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	return vb2_queue_init(dst_vq);
}

static int sc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc_ctx *ctx;

	sc_dbg("ctrl ID:%d, value:%d\n", ctrl->id, ctrl->val);
	ctx = container_of(ctrl->handler, struct sc_ctx, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		if (ctrl->val)
			ctx->flip |= SC_VFLIP;
		else
			ctx->flip &= ~SC_VFLIP;
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->val)
			ctx->flip |= SC_HFLIP;
		else
			ctx->flip &= ~SC_HFLIP;
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
		ctx->bl_op = ctrl->val;
		break;
	case V4L2_CID_2D_FMT_PREMULTI:
		ctx->pre_multi = ctrl->val;
		break;
	case V4L2_CID_2D_DITH:
		ctx->dith = ctrl->val;
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

static const struct v4l2_ctrl_ops sc_ctrl_ops = {
	.s_ctrl = sc_s_ctrl,
};

static const struct v4l2_ctrl_config sc_custom_ctrl[] = {
	{
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_CACHEABLE,
		.name = "set cacheable",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = true,
	}, {
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_GLOBAL_ALPHA,
		.name = "Set constant src alpha",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,
		.max = 255,
		.def = 255,
	}, {
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_2D_BLEND_OP,
		.name = "set blend op",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,
		.max = BL_OP_ADD,
		.def = 0,
	}, {
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_2D_DITH,
		.name = "set dithering",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = false,
	}, {
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_2D_FMT_PREMULTI,
		.name = "set pre-multiplied format",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = false,
		.max = true,
		.def = false,
	}, {
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_CSC_EQ,
		.name = "Set CSC equation",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = SC_CSC_601,
		.max = SC_CSC_709,
		.def = SC_CSC_601,
	}, {
		.ops = &sc_ctrl_ops,
		.id = V4L2_CID_CSC_RANGE,
		.name = "Set CSC range",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = SC_CSC_NARROW,
		.max = SC_CSC_WIDE,
		.def = SC_CSC_NARROW,
	}
};

static int sc_add_ctrls(struct sc_ctx *ctx)
{
	int i;

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, SC_MAX_CTRL_NUM);
	v4l2_ctrl_new_std(&ctx->ctrl_handler, &sc_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ctx->ctrl_handler, &sc_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ctx->ctrl_handler, &sc_ctrl_ops,
			V4L2_CID_ROTATE, 0, 270, 90, 0);

	for (i = 0; i < ARRAY_SIZE(sc_custom_ctrl); i++)
		v4l2_ctrl_new_custom(&ctx->ctrl_handler,
				&sc_custom_ctrl[i], NULL);
	if (ctx->ctrl_handler.error) {
		int err = ctx->ctrl_handler.error;
		v4l2_err(&ctx->sc_dev->m2m.v4l2_dev,
				"v4l2_ctrl_handler_init failed %d\n", err);
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		return err;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_handler);

	return 0;
}

static int sc_open(struct file *file)
{
	struct sc_dev *sc = video_drvdata(file);
	struct sc_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(sc->dev, "no memory for open context\n");
		return -ENOMEM;
	}

	atomic_inc(&sc->m2m.in_use);
	ctx->sc_dev = sc;

	v4l2_fh_init(&ctx->fh, sc->m2m.vfd);
	ret = sc_add_ctrls(ctx);
	if (ret)
		goto err_fh;

	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	/* Default color format */
	ctx->s_frame.sc_fmt = &sc_formats[0];
	ctx->d_frame.sc_fmt = &sc_formats[0];
	init_waitqueue_head(&sc->wait);
	spin_lock_init(&ctx->slock);

	INIT_LIST_HEAD(&ctx->fence_wait_list);
	INIT_WORK(&ctx->fence_work, sc_fence_work);
	ctx->fence_wq = create_singlethread_workqueue("sc_wq");
	if (ctx->fence_wq == NULL) {
		dev_err(sc->dev, "failed to create work queue\n");
		goto err_wq;
	}

	/* Setup the device context for mem2mem mode. */
	ctx->m2m_ctx = v4l2_m2m_ctx_init(sc->m2m.m2m_dev, ctx, queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		ret = -EINVAL;
		goto err_ctx;
	}

	return 0;

err_ctx:
	if (ctx->fence_wq)
		destroy_workqueue(ctx->fence_wq);
err_wq:
	v4l2_fh_del(&ctx->fh);
err_fh:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&sc->m2m.in_use);
	kfree(ctx);

	return ret;
}

static int sc_release(struct file *file)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(file->private_data);
	struct sc_dev *sc = ctx->sc_dev;

	sc_dbg("refcnt= %d", atomic_read(&sc->m2m.in_use));

	destroy_intermediate_frame(ctx);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	if (ctx->fence_wq)
		destroy_workqueue(ctx->fence_wq);
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&sc->m2m.in_use);
	kfree(ctx);

	return 0;
}

static unsigned int sc_poll(struct file *file,
			     struct poll_table_struct *wait)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(file->private_data);

	return v4l2_m2m_poll(file, ctx->m2m_ctx, wait);
}

static int sc_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct sc_ctx *ctx = fh_to_sc_ctx(file->private_data);

	return v4l2_m2m_mmap(file, ctx->m2m_ctx, vma);
}

static const struct v4l2_file_operations sc_v4l2_fops = {
	.owner		= THIS_MODULE,
	.open		= sc_open,
	.release	= sc_release,
	.poll		= sc_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= sc_mmap,
};

static int sc_clock_gating(struct sc_dev *sc, enum sc_clk_status status)
{
	if (status == SC_CLK_ON) {
		if (sc->aclk) {
			int ret = clk_enable(sc->aclk);
			if (ret) {
				dev_err(sc->dev,
					"%s: Failed to enable clock: %d\n",
					__func__, ret);
				return ret;
			}
		}

		atomic_inc(&sc->clk_cnt);
		dev_dbg(sc->dev, "clock enabled\n");
	} else if (status == SC_CLK_OFF) {
		if (WARN_ON(atomic_dec_return(&sc->clk_cnt) < 0)) {
			dev_err(sc->dev, "scaler clock control is wrong!!\n");
			atomic_set(&sc->clk_cnt, 0);
		} else {
			if (sc->aclk)
				clk_disable(sc->aclk);
			dev_dbg(sc->dev, "clock disabled\n");
		}
	}

	return 0;
}

static void sc_watchdog(unsigned long arg)
{
	struct sc_dev *sc = (struct sc_dev *)arg;
	struct sc_ctx *ctx;
	unsigned long flags;
	struct vb2_buffer *src_vb, *dst_vb;

	sc_dbg("timeout watchdog\n");
	if (atomic_read(&sc->wdt.cnt) >= SC_WDT_CNT) {
		sc_clock_gating(sc, SC_CLK_OFF);
		pm_runtime_put(sc->dev);

		sc_dbg("wakeup blocked process\n");
		atomic_set(&sc->wdt.cnt, 0);
		clear_bit(DEV_RUN, &sc->state);

		ctx = v4l2_m2m_get_curr_priv(sc->m2m.m2m_dev);
		if (!ctx || !ctx->m2m_ctx) {
			dev_err(sc->dev, "current ctx is NULL\n");
			return;
		}
		spin_lock_irqsave(&sc->slock, flags);
		clear_bit(CTX_RUN, &ctx->flags);
		src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

		if (src_vb && dst_vb) {
			v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
			v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

			v4l2_m2m_job_finish(sc->m2m.m2m_dev, ctx->m2m_ctx);
		}
		spin_unlock_irqrestore(&sc->slock, flags);
		return;
	}

	if (test_bit(DEV_RUN, &sc->state)) {
		atomic_inc(&sc->wdt.cnt);
		dev_err(sc->dev, "scaler is still running\n");
		sc->wdt.timer.expires = jiffies + SC_TIMEOUT;
		add_timer(&sc->wdt.timer);
	} else {
		sc_dbg("scaler finished job\n");
	}
}

static void sc_set_csc_coef(struct sc_ctx *ctx)
{
	struct sc_frame *s_frame, *d_frame;
	struct sc_dev *sc;
	enum sc_csc_idx idx;

	sc = ctx->sc_dev;
	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	if (s_frame->sc_fmt->color == d_frame->sc_fmt->color)
		idx = NO_CSC;
	else if (sc_fmt_is_rgb(s_frame->sc_fmt->color))
		idx = CSC_R2Y;
	else
		idx = CSC_Y2R;

	sc_hwset_csc_coef(sc, idx, &ctx->csc);
}

static int sc_get_scale_filter(unsigned int ratio)
{
	int filter;

	if (ratio <= 65536)
		filter = 0;	/* 8:8 or zoom-in */
	else if (ratio <= 74898)
		filter = 1;	/* 8:7 zoom-out */
	else if (ratio <= 87381)
		filter = 2;	/* 8:6 zoom-out */
	else if (ratio <= 104857)
		filter = 3;	/* 8:5 zoom-out */
	else if (ratio <= 131072)
		filter = 4;	/* 8:4 zoom-out */
	else if (ratio <= 174762)
		filter = 5;	/* 8:3 zoom-out */
	else
		filter = 6;	/* 8:2 zoom-out */

	return filter;
}

static void sc_set_scale_coef(struct sc_dev *sc, unsigned int h_ratio,
				unsigned int v_ratio)
{
	int h_coef, v_coef;

	h_coef = sc_get_scale_filter(h_ratio);
	v_coef = sc_get_scale_filter(v_ratio);

	sc_hwset_hcoef(sc, h_coef);
	sc_hwset_vcoef(sc, v_coef);
}

static void sc_set_scale_ratio(struct sc_dev *sc,
				unsigned int h_ratio, unsigned int v_ratio)
{
	sc_set_scale_coef(sc, h_ratio, v_ratio);

	sc_hwset_hratio(sc, h_ratio);
	sc_hwset_vratio(sc, v_ratio);
}

static bool sc_process_2nd_stage(struct sc_dev *sc, struct sc_ctx *ctx)
{
	struct sc_frame *s_frame, *d_frame;
	struct sc_size_limit *limit;
	unsigned int halign = 0, walign = 0;

	if (!test_bit(CTX_INT_FRAME, &ctx->flags))
		return false;

	s_frame = &ctx->i_frame->frame;
	d_frame = &ctx->d_frame;

	s_frame->addr.y = ctx->i_frame->src_addr.y;
	s_frame->addr.cb = ctx->i_frame->src_addr.cb;
	s_frame->addr.cr = ctx->i_frame->src_addr.cr;

	if (sc_fmt_is_yuv422(d_frame->sc_fmt->pixelformat)) {
		walign = 1;
	} else if (sc_fmt_is_yuv420(d_frame->sc_fmt->pixelformat)) {
		walign = 1;
		halign = 1;
	}

	limit = &sc->variant->limit_input;
	v4l_bound_align_image(&s_frame->crop.width, limit->min_w, limit->max_w,
			walign, &s_frame->crop.height, limit->min_h,
			limit->max_h, halign, 0);

	sc_set_scale_ratio(sc,
		SCALE_RATIO(s_frame->crop.width, d_frame->crop.width),
		SCALE_RATIO(s_frame->crop.height, d_frame->crop.height));

	sc_hwset_src_image_format(sc, s_frame->sc_fmt->pixelformat);
	sc_hwset_dst_image_format(sc, d_frame->sc_fmt->pixelformat);
	sc_hwset_src_imgsize(sc, s_frame);
	sc_hwset_dst_imgsize(sc, d_frame);
	sc_hwset_src_crop(sc, &s_frame->crop, s_frame->sc_fmt);
	sc_hwset_dst_crop(sc, &d_frame->crop);

	sc_hwset_src_addr(sc, &s_frame->addr);
	sc_hwset_dst_addr(sc, &d_frame->addr);

	sc_hwset_flip_rotation(sc, 0, 0);

	sc_hwset_start(sc);

	clear_bit(CTX_INT_FRAME, &ctx->flags);

	return true;
}

static irqreturn_t sc_irq_handler(int irq, void *priv)
{
	struct sc_dev *sc = priv;
	struct sc_ctx *ctx;
	struct vb2_buffer *src_vb, *dst_vb;
	int val;

	spin_lock(&sc->slock);

	clear_bit(DEV_RUN, &sc->state);

	if (timer_pending(&sc->wdt.timer))
		del_timer(&sc->wdt.timer);

	val = sc_hwget_int_status(sc);
	sc_hwset_int_clear(sc);

	ctx = v4l2_m2m_get_curr_priv(sc->m2m.m2m_dev);
	if (!ctx || !ctx->m2m_ctx) {
		sc_clock_gating(sc, SC_CLK_OFF);
		pm_runtime_put(sc->dev);
		dev_err(sc->dev, "current ctx is NULL\n");
		goto isr_unlock;
	}

	if (sc_process_2nd_stage(sc, ctx))
		goto isr_unlock;

	sc_clock_gating(sc, SC_CLK_OFF);
	pm_runtime_put(sc->dev);

	clear_bit(CTX_RUN, &ctx->flags);

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	if (src_vb && dst_vb) {
		if (__measure_hw_latency) {
			struct v4l2_m2m_buffer *mb =
					container_of(dst_vb, typeof(*mb), vb);
			struct vb2_scaler_buffer *svb =
					container_of(mb, typeof(*svb), mb);

			dst_vb->v4l2_buf.reserved2 =
				(__u32)(sched_clock() - svb->hw_latency);
		}

		if (val & SCALER_INT_STATUS_FRAME_END) {
			v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
			v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);
		} else {
			dev_err(sc->dev, "illegal setting 0x%x err!!!\n", val);
			v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
			v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);
		}

		if (test_bit(DEV_SUSPEND, &sc->state)) {
			sc_dbg("wake up blocked process by suspend\n");
			wake_up(&sc->wait);
		} else {
			v4l2_m2m_job_finish(sc->m2m.m2m_dev, ctx->m2m_ctx);
		}

		/* Wake up from CTX_ABORT state */
		if (test_and_clear_bit(CTX_ABORT, &ctx->flags))
			wake_up(&sc->wait);
	} else {
		dev_err(sc->dev, "failed to get the buffer done\n");
	}

isr_unlock:
	spin_unlock(&sc->slock);

	return IRQ_HANDLED;
}

static int sc_get_bufaddr(struct sc_dev *sc, struct vb2_buffer *vb2buf,
		struct sc_frame *frame)
{
	int ret;
	unsigned int pixsize, bytesize;
	void *cookie;

	pixsize = frame->width * frame->height;
	bytesize = (pixsize * frame->sc_fmt->bitperpixel[0]) >> 3;

	cookie = vb2_plane_cookie(vb2buf, 0);
	if (!cookie)
		return -EINVAL;

	ret = sc_get_dma_address(cookie, &frame->addr.y);
	if (ret != 0)
		return ret;

	frame->addr.cb = 0;
	frame->addr.cr = 0;
	frame->addr.cbsize = 0;
	frame->addr.crsize = 0;

	switch (frame->sc_fmt->num_comp) {
	case 1: /* rgb, yuyv */
		frame->addr.ysize = bytesize;
		break;
	case 2:
		if (frame->sc_fmt->num_planes == 1) {
			frame->addr.cb = frame->addr.y + pixsize;
			frame->addr.ysize = pixsize;
			frame->addr.cbsize = bytesize - pixsize;
		} else if (frame->sc_fmt->num_planes == 2) {
			cookie = vb2_plane_cookie(vb2buf, 1);
			if (!cookie)
				return -EINVAL;

			ret = sc_get_dma_address(cookie, &frame->addr.cb);
			if (ret != 0)
				return ret;
			frame->addr.ysize =
				pixsize * frame->sc_fmt->bitperpixel[0] >> 3;
			frame->addr.cbsize =
				pixsize * frame->sc_fmt->bitperpixel[1] >> 3;
		}
		break;
	case 3:
		if (frame->sc_fmt->num_planes == 1) {
			if (sc_fmt_is_ayv12(frame->sc_fmt->pixelformat)) {
				unsigned int c_span;
				c_span = ALIGN(frame->width >> 1, 16);
				frame->addr.ysize = pixsize;
				frame->addr.cbsize = c_span * (frame->height >> 1);
				frame->addr.crsize = frame->addr.cbsize;
				frame->addr.cb = frame->addr.y + pixsize;
				frame->addr.cr = frame->addr.cb + frame->addr.cbsize;
			} else {
				frame->addr.ysize = pixsize;
				frame->addr.cbsize = (bytesize - pixsize) / 2;
				frame->addr.crsize = frame->addr.cbsize;
				frame->addr.cb = frame->addr.y + pixsize;
				frame->addr.cr = frame->addr.cb + frame->addr.cbsize;
			}
		} else if (frame->sc_fmt->num_planes == 3) {
			cookie = vb2_plane_cookie(vb2buf, 1);
			if (!cookie)
				return -EINVAL;
			ret = sc_get_dma_address(cookie, &frame->addr.cb);
			if (ret != 0)
				return ret;
			cookie = vb2_plane_cookie(vb2buf, 2);
			if (!cookie)
				return -EINVAL;
			ret = sc_get_dma_address(cookie, &frame->addr.cr);
			if (ret != 0)
				return ret;
			frame->addr.ysize =
				pixsize * frame->sc_fmt->bitperpixel[0] >> 3;
			frame->addr.cbsize =
				pixsize * frame->sc_fmt->bitperpixel[1] >> 3;
			frame->addr.crsize =
				pixsize * frame->sc_fmt->bitperpixel[2] >> 3;
		} else {
			dev_err(sc->dev, "Please check the num of comp\n");
		}
		break;
	default:
		break;
	}

	if (frame->sc_fmt->pixelformat == V4L2_PIX_FMT_YVU420 ||
			frame->sc_fmt->pixelformat == V4L2_PIX_FMT_YVU420M) {
		u32 t_cb = frame->addr.cb;
		frame->addr.cb = frame->addr.cr;
		frame->addr.cr = t_cb;
	}

	sc_dbg("y addr 0x%x y size 0x%x\n", frame->addr.y, frame->addr.ysize);
	sc_dbg("cb addr 0x%x cb size 0x%x\n", frame->addr.cb, frame->addr.cbsize);
	sc_dbg("cr addr 0x%x cr size 0x%x\n", frame->addr.cr, frame->addr.crsize);

	return 0;
}

static void sc_set_dithering(struct sc_ctx *ctx)
{
	struct sc_dev *sc = ctx->sc_dev;
	unsigned int val = 0;

	if (ctx->dith)
		val = sc_dith_val(1, 1, 1);

	sc_dbg("dither value is 0x%x\n", val);
	sc_hwset_dith(sc, val);
}

/*
 * 'Prefetch' is not required by Scaler
 * because fetch larger region is more beneficial for rotation
 */
#define SC_SRC_PBCONFIG	(SYSMMU_PBUFCFG_TLB_UPDATE |		\
			SYSMMU_PBUFCFG_ASCENDING | SYSMMU_PBUFCFG_READ)
#define SC_DST_PBCONFIG	(SYSMMU_PBUFCFG_TLB_UPDATE |		\
			SYSMMU_PBUFCFG_ASCENDING | SYSMMU_PBUFCFG_WRITE)

static void sc_set_prefetch_buffers(struct device *dev, struct sc_ctx *ctx)
{
	struct sc_frame *s_frame = &ctx->s_frame;
	struct sc_frame *d_frame = &ctx->d_frame;
	struct sysmmu_prefbuf pb_reg[6];
	unsigned int i = 0;

	pb_reg[i].base = s_frame->addr.y;
	pb_reg[i].size = s_frame->addr.ysize;
	pb_reg[i++].config = SC_SRC_PBCONFIG;
	if (s_frame->sc_fmt->num_comp >= 2) {
		pb_reg[i].base = s_frame->addr.cb;
		pb_reg[i].size = s_frame->addr.cbsize;
		pb_reg[i++].config = SC_SRC_PBCONFIG;
	}
	if (s_frame->sc_fmt->num_comp >= 3) {
		pb_reg[i].base = s_frame->addr.cr;
		pb_reg[i].size = s_frame->addr.crsize;
		pb_reg[i++].config = SC_SRC_PBCONFIG;
	}

	pb_reg[i].base = d_frame->addr.y;
	pb_reg[i].size = d_frame->addr.ysize;
	pb_reg[i++].config = SC_DST_PBCONFIG;
	if (d_frame->sc_fmt->num_comp >= 2) {
		pb_reg[i].base = d_frame->addr.cb;
		pb_reg[i].size = d_frame->addr.cbsize;
		pb_reg[i++].config = SC_DST_PBCONFIG;
	}
	if (d_frame->sc_fmt->num_comp >= 3) {
		pb_reg[i].base = d_frame->addr.cr;
		pb_reg[i].size = d_frame->addr.crsize;
		pb_reg[i++].config = SC_DST_PBCONFIG;
	}

	sysmmu_set_prefetch_buffer_by_region(dev, pb_reg, i);
}

static void sc_m2m_device_run(void *priv)
{
	struct sc_ctx *ctx = priv;
	struct sc_dev *sc;
	struct sc_frame *s_frame, *d_frame;
	int ret;

	sc = ctx->sc_dev;

	if (test_bit(DEV_RUN, &sc->state)) {
		dev_err(sc->dev, "Scaler is already in progress\n");
		return;
	}

	if (test_bit(DEV_SUSPEND, &sc->state)) {
		dev_err(sc->dev, "Scaler is in suspend state\n");
		return;
	}

	if (test_bit(CTX_ABORT, &ctx->flags)) {
		dev_err(sc->dev, "aborted scaler device run\n");
		return;
	}

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	sc_get_bufaddr(sc, v4l2_m2m_next_src_buf(ctx->m2m_ctx), s_frame);
	sc_get_bufaddr(sc, v4l2_m2m_next_dst_buf(ctx->m2m_ctx), d_frame);

	if (in_irq())
		ret = pm_runtime_get(sc->dev);
	else
		ret = pm_runtime_get_sync(sc->dev);
	if (ret < 0) {
		dev_err(sc->dev,
			"%s=%d: Failed to enable local power\n", __func__, ret);
		return;
	}

	ret = sc_clock_gating(sc, SC_CLK_ON);
	if (ret < 0) {
		pm_runtime_put(sc->dev);
		dev_err(sc->dev,
			"%s=%d: Failed to enable clock\n", __func__, ret);
		return;
	}

	sc_hwset_soft_reset(sc);

	sc_set_scale_ratio(sc, ctx->h_ratio, ctx->v_ratio);
	if (ctx->i_frame)
		set_bit(CTX_INT_FRAME, &ctx->flags);

	if (test_bit(CTX_INT_FRAME, &ctx->flags))
		d_frame = &ctx->i_frame->frame;

	sc_set_csc_coef(ctx);

	sc_hwset_src_image_format(sc, s_frame->sc_fmt->pixelformat);
	sc_hwset_dst_image_format(sc, d_frame->sc_fmt->pixelformat);
	if (ctx->pre_multi)
		sc_hwset_pre_multi_format(sc);

	sc_hwset_src_imgsize(sc, s_frame);
	sc_hwset_dst_imgsize(sc, d_frame);
	sc_hwset_src_crop(sc, &s_frame->crop, s_frame->sc_fmt);
	sc_hwset_dst_crop(sc, &d_frame->crop);

	sc_hwset_src_addr(sc, &s_frame->addr);
	sc_hwset_dst_addr(sc, &d_frame->addr);

	sc_set_dithering(ctx);

	if (ctx->bl_op)
		sc_hwset_blend(sc, ctx->bl_op, ctx->pre_multi, ctx->g_alpha);

	sc_hwset_flip_rotation(sc, ctx->flip, ctx->rotation);
	sc_hwset_int_en(sc, 1);

	sc->wdt.timer.expires = jiffies + SC_TIMEOUT;
	add_timer(&sc->wdt.timer);

	set_bit(DEV_RUN, &sc->state);
	set_bit(CTX_RUN, &ctx->flags);

	sc_set_prefetch_buffers(sc->dev, ctx);

	if (__measure_hw_latency) {
		struct vb2_buffer *vb = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
		struct v4l2_m2m_buffer *mb = container_of(vb, typeof(*mb), vb);
		struct vb2_scaler_buffer *svb =
					container_of(mb, typeof(*svb), mb);

		svb->hw_latency = sched_clock();
	}

	sc_hwset_start(sc);
}

static void sc_m2m_job_abort(void *priv)
{
	struct sc_ctx *ctx = priv;
	int ret;

	ret = sc_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->sc_dev->dev, "wait timeout\n");
}

static struct v4l2_m2m_ops sc_m2m_ops = {
	.device_run	= sc_m2m_device_run,
	.job_abort	= sc_m2m_job_abort,
};

static int sc_register_m2m_device(struct sc_dev *sc)
{
	struct v4l2_device *v4l2_dev;
	struct device *dev;
	struct video_device *vfd;
	int ret = 0;

	if (!sc)
		return -ENODEV;

	dev = sc->dev;
	v4l2_dev = &sc->m2m.v4l2_dev;

	snprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s.m2m",
			MODULE_NAME);

	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret) {
		dev_err(sc->dev, "failed to register v4l2 device\n");
		return ret;
	}

	vfd = video_device_alloc();
	if (!vfd) {
		dev_err(sc->dev, "failed to allocate video device\n");
		goto err_v4l2_dev;
	}

	vfd->fops	= &sc_v4l2_fops;
	vfd->ioctl_ops	= &sc_v4l2_ioctl_ops;
	vfd->release	= video_device_release;
	vfd->lock	= &sc->lock;
	vfd->vfl_dir	= VFL_DIR_M2M;
	snprintf(vfd->name, sizeof(vfd->name), "%s:m2m", MODULE_NAME);

	video_set_drvdata(vfd, sc);

	sc->m2m.vfd = vfd;
	sc->m2m.m2m_dev = v4l2_m2m_init(&sc_m2m_ops);
	if (IS_ERR(sc->m2m.m2m_dev)) {
		dev_err(sc->dev, "failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(sc->m2m.m2m_dev);
		goto err_dev_alloc;
	}

	ret = video_register_device(vfd, VFL_TYPE_GRABBER,
				EXYNOS_VIDEONODE_SCALER(sc->id));
	if (ret) {
		dev_err(sc->dev, "failed to register video device\n");
		goto err_m2m_dev;
	}

	return 0;

err_m2m_dev:
	v4l2_m2m_release(sc->m2m.m2m_dev);
err_dev_alloc:
	video_device_release(sc->m2m.vfd);
err_v4l2_dev:
	v4l2_device_unregister(v4l2_dev);

	return ret;
}

static int sc_clk_get(struct sc_dev *sc)
{
	sc->aclk = devm_clk_get(sc->dev, "gate");
	if (IS_ERR(sc->aclk)) {
		if (PTR_ERR(sc->aclk) == -ENOENT)
			/* clock is not present */
			sc->aclk = NULL;
		else
			return PTR_ERR(sc->aclk);
		dev_info(sc->dev, "'gate' clock is not present\n");
	}

	sc->clk_chld = devm_clk_get(sc->dev, "mux_user");
	if (IS_ERR(sc->clk_chld)) {
		if (PTR_ERR(sc->clk_chld) == -ENOENT)
			/* clock is not present */
			sc->clk_chld = NULL;
		else
			return PTR_ERR(sc->clk_chld);
		dev_info(sc->dev, "'mux_user' clock is not present\n");
	}

	if (sc->clk_chld) {
		sc->clk_parn = devm_clk_get(sc->dev, "mux_src");
		if (IS_ERR(sc->clk_parn)) {
			if (PTR_ERR(sc->clk_parn) == -ENOENT)
				/* clock is not present */
				sc->clk_parn = NULL;
			else
				return PTR_ERR(sc->clk_parn);
		dev_info(sc->dev, "'mux_src' clock is not present\n");
		}
	}

	return clk_prepare(sc->aclk);
}

static void sc_clk_put(struct sc_dev *sc)
{
	if (sc->aclk) {
		clk_unprepare(sc->aclk);
		clk_put(sc->aclk);
	}

	if (sc->clk_chld)
		clk_put(sc->clk_chld);

	if (sc->clk_parn)
		clk_put(sc->clk_parn);
}

#ifdef CONFIG_PM_SLEEP
static int sc_suspend(struct device *dev)
{
	struct sc_dev *sc = dev_get_drvdata(dev);
	int ret;

	set_bit(DEV_SUSPEND, &sc->state);

	ret = wait_event_timeout(sc->wait,
			!test_bit(DEV_RUN, &sc->state), SC_TIMEOUT);
	if (ret == 0)
		dev_err(sc->dev, "wait timeout\n");

	return 0;
}

static int sc_resume(struct device *dev)
{
	struct sc_dev *sc = dev_get_drvdata(dev);

	clear_bit(DEV_SUSPEND, &sc->state);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sc_runtime_resume(struct device *dev)
{
	struct sc_dev *sc = dev_get_drvdata(dev);
	if (sc->clk_chld && sc->clk_parn) {
		int ret = clk_set_parent(sc->clk_chld, sc->clk_parn);
		if (ret) {
			dev_err(sc->dev, "%s: Failed to setup MUX: %d\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}
#endif

static const struct dev_pm_ops sc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc_suspend, sc_resume)
	SET_RUNTIME_PM_OPS(NULL, sc_runtime_resume, NULL)
};

static int sc_probe(struct platform_device *pdev)
{
	struct sc_dev *sc;
	struct resource *res;
	int ret = 0;

	dev_info(&pdev->dev, "++%s\n", __func__);

	sc = devm_kzalloc(&pdev->dev, sizeof(struct sc_dev), GFP_KERNEL);
	if (!sc) {
		dev_err(&pdev->dev, "no memory for scaler device\n");
		return -ENOMEM;
	}

	sc->dev = &pdev->dev;

	if (pdev->dev.of_node)
		sc->id = of_alias_get_id(pdev->dev.of_node, "scaler");
	else
		sc->id = pdev->id;

	spin_lock_init(&sc->slock);
	mutex_init(&sc->lock);

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	sc->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (sc->regs == NULL) {
		dev_err(&pdev->dev, "failed to claim register region\n");
		return -ENOENT;
	}

	/* Get IRQ resource and register IRQ handler. */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get IRQ resource\n");
		return -ENXIO;
	}

	ret = devm_request_irq(&pdev->dev, res->start, sc_irq_handler, 0,
			pdev->name, sc);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq\n");
		return ret;
	}

	atomic_set(&sc->wdt.cnt, 0);
	setup_timer(&sc->wdt.timer, sc_watchdog, (unsigned long)sc);

	ret = sc_clk_get(sc);
	if (ret)
		return ret;

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	sc->vb2 = &sc_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
	sc->vb2 = &sc_vb2_ion;
#endif

	sc->alloc_ctx = sc->vb2->init(sc);
	if (IS_ERR_OR_NULL(sc->alloc_ctx)) {
		ret = PTR_ERR(sc->alloc_ctx);
		dev_err(&pdev->dev, "failed to alloc_ctx\n");
		goto err_clk;
	}

	platform_set_drvdata(pdev, sc);

	exynos_create_iovmm(&pdev->dev, 3, 3);
	sc->vb2->resume(sc->alloc_ctx);

	pm_runtime_enable(&pdev->dev);

	ret = pm_runtime_get_sync(sc->dev);
	if (ret < 0)
		goto err_clk;

	ret = sc_clock_gating(sc, SC_CLK_ON);
	if (ret < 0) {
		pm_runtime_put_sync(sc->dev);
		goto err_clk;
	}

	sc->ver = sc_hwget_version(sc);
	dev_info(&pdev->dev, "scaler version is 0x%08x\n", sc->ver);

	sc_clock_gating(sc, SC_CLK_OFF);

	pm_runtime_put_sync(sc->dev);

	sc->variant = &variant;

	ret = sc_register_m2m_device(sc);
	if (ret) {
		dev_err(&pdev->dev, "failed to register m2m device\n");
		ret = -EPERM;
		goto err_clk;
	}

	dev_info(&pdev->dev, "scaler registered successfully\n");

	return 0;

err_clk:
	sc_clk_put(sc);
	return ret;
}

static int sc_remove(struct platform_device *pdev)
{
	struct sc_dev *sc = platform_get_drvdata(pdev);

	sc->vb2->suspend(sc->alloc_ctx);

	sc_clk_put(sc);

	if (timer_pending(&sc->wdt.timer))
		del_timer(&sc->wdt.timer);

	return 0;
}

static const struct of_device_id exynos_sc_match[] = {
	{
		.compatible = "samsung,exynos5-scaler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_sc_match);

static struct platform_driver sc_driver = {
	.probe		= sc_probe,
	.remove		= sc_remove,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &sc_pm_ops,
		.of_match_table = of_match_ptr(exynos_sc_match),
	}
};

module_platform_driver(sc_driver);

MODULE_AUTHOR("Sunyoung, Kang <sy0816.kang@samsung.com>");
MODULE_DESCRIPTION("EXYNOS m2m scaler driver");
MODULE_LICENSE("GPL");
