/* linux/drivers/media/video/exynos/gsc/gsc-core.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series G-scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/clk-private.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/v4l2-mediabus.h>
#include <linux/exynos_iovmm.h>
#include <media/v4l2-ioctl.h>

#include "gsc-core.h"
static char *gsc_clocks[GSC_MAX_CLOCKS] = {
	"gate_gscl", "mout_aclk_gscl_333_user", "aclk_gscl_333",
	"mout_aclk_gscl_111_user", "aclk_gscl_111"
};
int gsc_dbg = 6;
module_param(gsc_dbg, int, 0644);

static struct gsc_fmt gsc_formats[] = {
	{
		.name		= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.depth		= { 16 },
		.num_planes	= 1,
		.nr_comp	= 1,
	}, {
		.name		= "XRGB-8-8-8-8, 32 bpp",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.depth		= { 32 },
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_XRGB8888_4X8_LE,
	}, {
		.name		= "XBGR-8-8-8-8, 32 bpp",
		.pixelformat	= V4L2_PIX_FMT_BGR32,
		.depth		= { 32 },
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_XRGB8888_4X8_LE,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.depth		= { 16 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.depth		= { 16 },
		.yorder		= GSC_LSB_C,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_UYVY8_2X8,
	}, {
		.name		= "YUV 4:2:2 packed, CrYCbY",
		.pixelformat	= V4L2_PIX_FMT_VYUY,
		.depth		= { 16 },
		.yorder		= GSC_LSB_C,
		.corder		= GSC_CRCB,
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_VYUY8_2X8,
	}, {
		.name		= "YUV 4:2:2 packed, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.depth		= { 16 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CRCB,
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_YVYU8_2X8,
	}, {
		.name		= "YUV 4:4:4 planar, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUV32,
		.depth		= { 32 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 1,
		.mbus_code	= V4L2_MBUS_FMT_YUV8_1X24,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.depth		= { 16 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 3,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.depth		= { 16 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 2,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.depth		= { 16 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CRCB,
		.num_planes	= 1,
		.nr_comp	= 2,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.depth		= { 12 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 3,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.depth		= { 12 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CRCB,
		.num_planes	= 1,
		.nr_comp	= 3,

	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.depth		= { 12 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 1,
		.nr_comp	= 2,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.depth		= { 12 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CRCB,
		.num_planes	= 1,
		.nr_comp	= 2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.depth		= { 8, 4 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 2,
		.nr_comp	= 2,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.depth		= { 8, 4 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CRCB,
		.num_planes	= 2,
		.nr_comp	= 2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.depth		= { 8, 2, 2 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 3,
		.nr_comp	= 3,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.depth		= { 8, 2, 2 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CRCB,
		.num_planes	= 3,
		.nr_comp	= 3,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr, tiled",
		.pixelformat	= V4L2_PIX_FMT_NV12MT_16X16,
		.depth		= { 8, 4 },
		.yorder		= GSC_LSB_Y,
		.corder		= GSC_CBCR,
		.num_planes	= 2,
		.nr_comp	= 2,
	},
};

struct gsc_fmt *get_format(int index)
{
	return &gsc_formats[index];
}

struct gsc_fmt *find_format(u32 *pixelformat, u32 *mbus_code, int index)
{
	struct gsc_fmt *fmt, *def_fmt = NULL;
	unsigned int i;

	if (index >= ARRAY_SIZE(gsc_formats))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(gsc_formats); ++i) {
		fmt = get_format(i);
		if (pixelformat && fmt->pixelformat == *pixelformat)
			return fmt;
		if (mbus_code && fmt->mbus_code == *mbus_code)
			return fmt;
	}
	return def_fmt;

}

void gsc_set_frame_size(struct gsc_frame *frame, int width, int height)
{
	frame->f_width	= width;
	frame->f_height	= height;
	frame->crop.width = width;
	frame->crop.height = height;
	frame->crop.left = 0;
	frame->crop.top = 0;
}

int gsc_cal_prescaler_ratio(struct gsc_variant *var, u32 src, u32 dst, u32 *ratio)
{
	if ((dst > src) || (dst >= src / var->poly_sc_down_max)) {
		*ratio = 1;
		return 0;
	}

	if ((src / var->poly_sc_down_max / var->pre_sc_down_max) > dst) {
		gsc_err("scale ratio exceeded maximun scale down ratio(1/16)");
		return -EINVAL;
	}

	*ratio = (dst > (src / 8)) ? 2 : 4;

	return 0;
}

void gsc_get_prescaler_shfactor(u32 hratio, u32 vratio, u32 *sh)
{
	if (hratio == 4 && vratio == 4)
		*sh = 4;
	else if ((hratio == 4 && vratio == 2) ||
		 (hratio == 2 && vratio == 4))
		*sh = 3;
	else if ((hratio == 4 && vratio == 1) ||
		 (hratio == 1 && vratio == 4) ||
		 (hratio == 2 && vratio == 2))
		*sh = 2;
	else if (hratio == 1 && vratio == 1)
		*sh = 0;
	else
		*sh = 1;
}

void gsc_check_src_scale_info(struct gsc_variant *var,
		struct gsc_frame *s_frame, u32 *wratio,
		u32 tx, u32 ty, u32 *hratio)
{
	int remainder = 0, walign, halign;

	if (is_rgb(s_frame->fmt->pixelformat)) {
		walign = *wratio;
		halign = *hratio;
	} else {
		walign = *wratio << 1;
		halign = *hratio << 1;
	}

	remainder = s_frame->crop.width % walign;
	if (remainder) {
		s_frame->crop.width -= remainder;
		gsc_cal_prescaler_ratio(var, s_frame->crop.width, tx, wratio);
		gsc_info("cropped src width size is recalculated from %d to %d",
			s_frame->crop.width + remainder, s_frame->crop.width);
	}

	remainder = s_frame->crop.height % halign;
	if (remainder) {
		s_frame->crop.height -= remainder;
		gsc_cal_prescaler_ratio(var, s_frame->crop.height, ty, hratio);
		gsc_info("cropped src height size is recalculated from %d to %d",
			s_frame->crop.height + remainder, s_frame->crop.height);
	}
}

int gsc_enum_fmt_mplane(struct v4l2_fmtdesc *f)
{
	struct gsc_fmt *fmt;

	fmt = find_format(NULL, NULL, f->index);
	if (!fmt)
		return -EINVAL;

	strncpy(f->description, fmt->name, sizeof(f->description) - 1);
	f->pixelformat = fmt->pixelformat;

	return 0;
}

u32 get_plane_size(struct gsc_frame *frame, unsigned int plane)
{
	if (!frame || plane >= frame->fmt->num_planes) {
		gsc_err("Invalid argument");
		return 0;
	}

	return frame->payload[plane];
}

u32 get_plane_info(struct gsc_frame frm, u32 addr, u32 *index)
{
	if (frm.addr.y == addr) {
		*index = 0;
		return frm.addr.y;
	} else if (frm.addr.cb == addr) {
		*index = 1;
		return frm.addr.cb;
	} else if (frm.addr.cr == addr) {
		*index = 2;
		return frm.addr.cr;
	} else {
		gsc_err("Plane address is wrong");
		return -EINVAL;
	}
}

void gsc_set_prefbuf(struct gsc_dev *gsc, struct gsc_frame frm)
{
	u32 f_chk_addr, f_chk_len, s_chk_addr, s_chk_len;
	f_chk_addr = f_chk_len = s_chk_addr = s_chk_len = 0;

	f_chk_addr = frm.addr.y;
	f_chk_len = frm.payload[0];
	if (frm.fmt->num_planes == 2) {
		s_chk_addr = frm.addr.cb;
		s_chk_len = frm.payload[1];
	} else if (frm.fmt->num_planes == 3) {
		u32 low_addr, low_plane, mid_addr, mid_plane, high_addr, high_plane;
		u32 t_min, t_max;

		t_min = min3(frm.addr.y, frm.addr.cb, frm.addr.cr);
		low_addr = get_plane_info(frm, t_min, &low_plane);
		t_max = max3(frm.addr.y, frm.addr.cb, frm.addr.cr);
		high_addr = get_plane_info(frm, t_max, &high_plane);

		mid_plane = 3 - (low_plane + high_plane);
		if (mid_plane == 0)
			mid_addr = frm.addr.y;
		else if (mid_plane == 1)
			mid_addr = frm.addr.cb;
		else if (mid_plane == 2)
			mid_addr = frm.addr.cr;
		else
			return;

		f_chk_addr = low_addr;
		if (mid_addr + frm.payload[mid_plane] - low_addr >
		    high_addr + frm.payload[high_plane] - mid_addr) {
			f_chk_len = frm.payload[low_plane];
			s_chk_addr = mid_addr;
			s_chk_len = high_addr + frm.payload[high_plane] - mid_addr;
		} else {
			f_chk_len = mid_addr + frm.payload[mid_plane] - low_addr;
			s_chk_addr = high_addr;
			s_chk_len = frm.payload[high_plane];
		}
	}

	gsc_dbg("f_addr = 0x%08x, f_len = %d, s_addr = 0x%08x, s_len = %d\n",
		f_chk_addr, f_chk_len, s_chk_addr, s_chk_len);
}

int gsc_try_fmt_mplane(struct gsc_ctx *ctx, struct v4l2_format *f)
{
	struct gsc_dev *gsc = ctx->gsc_dev;
	struct gsc_variant *variant = gsc->variant;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct gsc_fmt *fmt;
	u32 max_w, max_h, mod_x, mod_y;
	u32 min_w, min_h, tmp_w, tmp_h;
	u32 ffs_org_w, ffs_org_h;
	int i;

	gsc_dbg("user put w: %d, h: %d", pix_mp->width, pix_mp->height);

	fmt = find_format(&pix_mp->pixelformat, NULL, 0);
	if (!fmt) {
		gsc_err("pixelformat format (0x%X) invalid\n", pix_mp->pixelformat);
		return -EINVAL;
	}

	if (pix_mp->field == V4L2_FIELD_ANY)
		pix_mp->field = V4L2_FIELD_NONE;
	else if (pix_mp->field != V4L2_FIELD_NONE) {
		gsc_err("Not supported field order(%d)\n", pix_mp->field);
		return -EINVAL;
	}

	max_w = variant->pix_max->org_w;
	max_h = variant->pix_max->org_h;

	ffs_org_w = ffs(variant->pix_align->org_w);
	ffs_org_h = ffs(variant->pix_align->org_h);
	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		if (is_rgb(fmt->pixelformat)) {
			if (is_rgb32(fmt->pixelformat)) {
				mod_x = ffs_org_w - 3;
				mod_y = ffs_org_h - 3;
			} else {
				mod_x = ffs_org_w - 2;
				mod_y = ffs_org_h - 2;
			}
			min_w = variant->pix_min->org_w / 2;
			min_h = variant->pix_min->org_h / 2;
		} else {
			if (is_tiled(fmt)) {
				mod_x = ffs_org_w + 1;
				mod_y = ffs_org_h + 1;
			} else {
				mod_x = ffs_org_w - 1;
				mod_y = ffs_org_h - 1;
			}
			min_w = variant->pix_min->org_w;
			min_h = variant->pix_min->org_h;
		}
	} else {
		if (is_rgb(fmt->pixelformat)) {
			mod_x = ffs_org_w - 3;
			mod_y = ffs_org_h - 3;
			min_w = variant->pix_min->target_w / 2;
			min_h = variant->pix_min->target_h / 2;
		} else {
			mod_x = ffs_org_w - 2;
			mod_y = ffs_org_h - 2;
			min_w = variant->pix_min->target_w;
			min_h = variant->pix_min->target_h;
		}
	}

	gsc_dbg("org_w: %d", variant->pix_align->org_w);
	gsc_dbg("mod_x: %d, mod_y: %d, max_w: %d, max_h = %d",
	     mod_x, mod_y, max_w, max_h);
	/* To check if image size is modified to adjust parameter against
	   hardware abilities */
	tmp_w = pix_mp->width;
	tmp_h = pix_mp->height;

	v4l_bound_align_image(&pix_mp->width, min_w, max_w, mod_x,
		&pix_mp->height, min_h, max_h, mod_y, 0);
	if (tmp_w != pix_mp->width || tmp_h != pix_mp->height)
		gsc_info("Image size has been modified from %dx%d to %dx%d",
			 tmp_w, tmp_h, pix_mp->width, pix_mp->height);

	pix_mp->num_planes = fmt->num_planes;

	if (ctx->gsc_ctrls.csc_eq_mode->val)
		ctx->gsc_ctrls.csc_eq->val =
		(pix_mp->width >= 1280) ?
		V4L2_COLORSPACE_REC709 : V4L2_COLORSPACE_SMPTE170M;

	if (is_csc_eq_709) /* HD */
		pix_mp->colorspace = V4L2_COLORSPACE_REC709;
	else	/* SD */
		pix_mp->colorspace = V4L2_COLORSPACE_SMPTE170M;

	for (i = 0; i < pix_mp->num_planes; ++i) {
		int bpl = (pix_mp->width * fmt->depth[i]) >> 3;
		pix_mp->plane_fmt[i].bytesperline = bpl;
		if (is_AYV12(fmt->pixelformat))
			pix_mp->plane_fmt[i].sizeimage =
			(pix_mp->width * pix_mp->height) +
			((ALIGN(pix_mp->width >> 1, 16) *
			  (pix_mp->height >> 1) * 2));
		else
			pix_mp->plane_fmt[i].sizeimage = bpl * pix_mp->height;

		gsc_dbg("[%d]: bpl: %d, sizeimage: %d",
		    i, bpl, pix_mp->plane_fmt[i].sizeimage);
	}

	return 0;
}

int gsc_g_fmt_mplane(struct gsc_ctx *ctx, struct v4l2_format *f)
{
	struct gsc_frame *frame;
	struct v4l2_pix_format_mplane *pix_mp;
	int i;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	pix_mp = &f->fmt.pix_mp;

	pix_mp->width		= frame->f_width;
	pix_mp->height		= frame->f_height;
	pix_mp->field		= V4L2_FIELD_NONE;
	pix_mp->pixelformat	= frame->fmt->pixelformat;
	pix_mp->colorspace	= V4L2_COLORSPACE_JPEG;
	pix_mp->num_planes	= frame->fmt->num_planes;

	for (i = 0; i < pix_mp->num_planes; ++i) {
		pix_mp->plane_fmt[i].bytesperline = (frame->f_width *
			frame->fmt->depth[i]) / 8;
		if (is_AYV12(pix_mp->pixelformat))
			pix_mp->plane_fmt[i].sizeimage =
			(pix_mp->width * pix_mp->height) +
			((ALIGN(pix_mp->width >> 1, 16) *
			  (pix_mp->height >> 1) * 2));
		else
			pix_mp->plane_fmt[i].sizeimage =
				pix_mp->plane_fmt[i].bytesperline * frame->f_height;
	}

	return 0;
}

void gsc_check_crop_change(u32 tmp_w, u32 tmp_h, u32 *w, u32 *h)
{
	if (tmp_w != *w || tmp_h != *h) {
		gsc_info("Image cropped size has been modified from %dx%d to %dx%d",
				*w, *h, tmp_w, tmp_h);
		*w = tmp_w;
		*h = tmp_h;
	}
}

int gsc_g_crop(struct gsc_ctx *ctx, struct v4l2_crop *cr)
{
	struct gsc_frame *frame;

	frame = ctx_get_frame(ctx, cr->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	memcpy(&cr->c, &frame->crop, sizeof(struct v4l2_rect));

	return 0;
}

int gsc_try_crop(struct gsc_ctx *ctx, struct v4l2_crop *cr)
{
	struct gsc_frame *f;
	struct gsc_dev *gsc = ctx->gsc_dev;
	struct gsc_variant *var = gsc->variant;
	u32 mod_x = 0, mod_y = 0, tmp_w, tmp_h;
	u32 min_w, min_h, max_w, max_h;
	u32 offset_w, offset_h;

	if (cr->c.top < 0 || cr->c.left < 0) {
		gsc_err("doesn't support negative values for top & left\n");
		return -EINVAL;
	}
	gsc_dbg("user put w: %d, h: %d", cr->c.width, cr->c.height);

	if (cr->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		f = &ctx->d_frame;
	else if (cr->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		f = &ctx->s_frame;
	else
		return -EINVAL;

	tmp_w = cr->c.width;
	tmp_h = cr->c.height;

	if (V4L2_TYPE_IS_OUTPUT(cr->type)) {
		if (is_rotation) {
			max_w = min(f->f_width, (u32)var->pix_max->target_w);
			max_h = min(f->f_height, (u32)var->pix_max->target_h);
			if (is_rgb(f->fmt->pixelformat)) {
				min_w = min_h = var->pix_min->real_w / 2;
				offset_w = offset_h = 2;
			} else {
				min_w = min_h = var->pix_min->real_w;
				offset_w = offset_h = 4;
			}
		} else {
			max_w = min(f->f_width, (u32)var->pix_max->real_w);
			max_h = min(f->f_height, (u32)var->pix_max->real_h);
			if (is_rgb(f->fmt->pixelformat)) {
				min_w = var->pix_min->real_w / 2;
				min_h = var->pix_min->real_h / 2;
				offset_w = 2;
				offset_h = 1;
			} else {
				min_w = var->pix_min->real_w;
				min_h = var->pix_min->real_h;
				offset_w = 4;
				offset_h = 1;
			}
		}
	} else {
		if (is_rotation) {
			max_w = min(f->f_width, (u32)var->pix_max->real_w);
			max_h = min(f->f_height, (u32)var->pix_max->real_h);
		} else {
			max_w = min(f->f_width, (u32)var->pix_max->target_w);
			max_h = min(f->f_height, (u32)var->pix_max->target_h);
		}
		if (is_rgb(f->fmt->pixelformat)) {
			min_w = var->pix_min->target_w / 2;
			min_h = var->pix_min->target_h / 2;
			mod_x = ffs(var->pix_align->target_w) - 2;
			mod_y = ffs(var->pix_align->target_h) - 2;
			offset_w = offset_h = 1;
		} else {
			min_w = var->pix_min->target_w;
			min_h = var->pix_min->target_h;
			mod_x = ffs(var->pix_align->target_w) - 1;
			mod_y = ffs(var->pix_align->target_h) - 1;
			offset_w = offset_h = 2;
		}
	}

	gsc_dbg("mod_x: %d, mod_y: %d, min_w: %d, min_h = %d,\
		tmp_w : %d, tmp_h : %d",
		mod_x, mod_y, min_w, min_h, tmp_w, tmp_h);

	v4l_bound_align_image(&tmp_w, min_w, max_w, mod_x,
			      &tmp_h, min_h, max_h, mod_y, 0);

	gsc_check_crop_change(tmp_w, tmp_h, &cr->c.width, &cr->c.height);

	/* adjust left/top if cropping rectangle is out of bounds */
	/* Need to add code to algin left value with 2's multiple */
	if (cr->c.left + tmp_w > max_w)
		cr->c.left = max_w - tmp_w;
	if (cr->c.top + tmp_h > max_h)
		cr->c.top = max_h - tmp_h;

	cr->c.left -= (cr->c.left % offset_w);
	cr->c.top -= (cr->c.top % offset_h);

	gsc_dbg("Aligned l:%d, t:%d, w:%d, h:%d, f_w: %d, f_h: %d",
	    cr->c.left, cr->c.top, cr->c.width, cr->c.height, max_w, max_h);

	return 0;
}

int gsc_check_rotation_size(struct gsc_ctx *ctx)
{
	struct gsc_dev *gsc = ctx->gsc_dev;
	struct gsc_variant *var = gsc->variant;
	struct exynos_platform_gscaler *pdata = gsc->pdata;
	if (use_input_rotator) {
		struct gsc_frame *s_frame = &ctx->s_frame;
		if ((s_frame->crop.width > (u32)var->pix_max->rot_w) ||
		(s_frame->crop.height > (u32)var->pix_max->rot_h)) {
			gsc_err("Not supported source crop size");
			return -EINVAL;
		}
	} else {
		struct gsc_frame *d_frame = &ctx->d_frame;
		if ((d_frame->crop.width > (u32)var->pix_max->rot_w) ||
		(d_frame->crop.height > (u32)var->pix_max->rot_h)) {
			gsc_err("Not supported source crop size");
			return -EINVAL;
		}
	}

	return 0;
}

int gsc_check_scaler_ratio(struct gsc_ctx *ctx,
			struct gsc_variant *var, int sw, int sh, int dw,
			int dh, int rot, int out_path)
{
	int tmp_w, tmp_h, sc_down_max;
	sc_down_max =
		(out_path == GSC_DMA) ? var->sc_down_max : var->local_sc_down;
	if (is_rotation) {
		tmp_w = dh;
		tmp_h = dw;
	} else {
		tmp_w = dw;
		tmp_h = dh;
	}

	if ((sw > (tmp_w * sc_down_max)) ||
	    (sh > (tmp_h * sc_down_max)) ||
	    (tmp_w > (sw * var->sc_up_max)) ||
	    (tmp_h > (sh * var->sc_up_max)))
		return -EINVAL;

	return 0;
}

int gsc_set_scaler_info(struct gsc_ctx *ctx)
{
	struct gsc_scaler *sc = &ctx->scaler;
	struct gsc_frame *s_frame = &ctx->s_frame;
	struct gsc_frame *d_frame = &ctx->d_frame;
	struct gsc_variant *variant = ctx->gsc_dev->variant;
	int tx, ty;
	int ret;

	ret = gsc_check_scaler_ratio(ctx, variant, s_frame->crop.width,
		s_frame->crop.height, d_frame->crop.width,
		d_frame->crop.height, ctx->gsc_ctrls.rotate->val,
		ctx->out_path);
	if (ret) {
		gsc_err("out of scaler range");
		return ret;
	}

	if (is_rotation) {
		ty = d_frame->crop.width;
		tx = d_frame->crop.height;
	} else {
		tx = d_frame->crop.width;
		ty = d_frame->crop.height;
	}

	ret = gsc_cal_prescaler_ratio(variant, s_frame->crop.width,
				      tx, &sc->pre_hratio);
	if (ret) {
		gsc_err("Horizontal scale ratio is out of range");
		return ret;
	}

	ret = gsc_cal_prescaler_ratio(variant, s_frame->crop.height,
				      ty, &sc->pre_vratio);
	if (ret) {
		gsc_err("Vertical scale ratio is out of range");
		return ret;
	}

	gsc_check_src_scale_info(variant, s_frame, &sc->pre_hratio,
				 tx, ty, &sc->pre_vratio);

	gsc_get_prescaler_shfactor(sc->pre_hratio, sc->pre_vratio,
				   &sc->pre_shfactor);

	sc->main_hratio = (s_frame->crop.width << 16) / tx;
	sc->main_vratio = (s_frame->crop.height << 16) / ty;

	gsc_dbg("scaler input/output size : sx = %d, sy = %d, tx = %d, ty = %d",
		s_frame->crop.width, s_frame->crop.height, tx, ty);
	gsc_dbg("scaler ratio info : pre_shfactor : %d, pre_h : %d, pre_v :%d,\
		main_h : %ld, main_v : %ld", sc->pre_shfactor, sc->pre_hratio,
		sc->pre_vratio, sc->main_hratio, sc->main_vratio);

	return 0;
}

int gsc_pipeline_s_stream(struct gsc_dev *gsc, bool on)
{
	struct gsc_pipeline *p = &gsc->pipeline;
	struct exynos_entity_data md_data;
	int ret = 0;

	/* If gscaler subdev calls the mixer's s_stream, the gscaler must
	   inform the mixer subdev pipeline started from gscaler */
	if (gsc->out.ctx->out_path == GSC_MIXER) {
		md_data.mxr_data_from = FROM_GSC_SD;
		v4l2_set_subdevdata(p->disp, &md_data);
	}

	ret = v4l2_subdev_call(p->disp, video, s_stream, on);
	if (ret)
		gsc_err("Display s_stream on failed\n");

	return ret;
}

int gsc_out_link_validate(const struct media_pad *source,
			  const struct media_pad *sink)
{
	struct v4l2_subdev_format src_fmt;
	struct v4l2_subdev_crop dst_crop;
	struct v4l2_subdev *sd;
	struct gsc_dev *gsc;
	struct gsc_frame *f;
	int ret;

	if (media_entity_type(source->entity) != MEDIA_ENT_T_V4L2_SUBDEV ||
	    media_entity_type(sink->entity) != MEDIA_ENT_T_V4L2_SUBDEV) {
		gsc_err("media entity type isn't subdev\n");
		return 0;
	}

	sd = media_entity_to_v4l2_subdev(source->entity);
	gsc = entity_data_to_gsc(v4l2_get_subdevdata(sd));
	f = &gsc->out.ctx->d_frame;

	src_fmt.format.width = f->crop.width;
	src_fmt.format.height = f->crop.height;
	src_fmt.format.code = f->fmt->mbus_code;

	sd = media_entity_to_v4l2_subdev(sink->entity);
	/* To check if G-Scaler destination size and Mixer destinatin size
	   are the same */
	dst_crop.pad = sink->index;
	dst_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(sd, pad, get_crop, NULL, &dst_crop);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		gsc_err("subdev get_fmt is failed\n");
		return -EPIPE;
	}

	if (src_fmt.format.width != dst_crop.rect.width ||
	    src_fmt.format.height != dst_crop.rect.height) {
		gsc_err("sink and source format is different\
			src_fmt.w = %d, src_fmt.h = %d,\
			dst_crop.w = %d, dst_crop.h = %d, rotation = %d",
			src_fmt.format.width, src_fmt.format.height,
			dst_crop.rect.width, dst_crop.rect.height,
			gsc->out.ctx->gsc_ctrls.rotate->val);
		return -EINVAL;
	}

	return 0;
}

/*
 * Set alpha blending for all layers of mixer when gscaler is connected
 * to mixer only
 */
static int gsc_s_ctrl_to_mxr(struct v4l2_ctrl *ctrl)
{
	struct gsc_ctx *ctx = ctrl_to_ctx(ctrl);
	struct media_pad *pad = &ctx->gsc_dev->out.sd_pads[GSC_PAD_SOURCE];
	struct v4l2_subdev *sd, *gsc_sd;
	struct v4l2_control control;

	pad = media_entity_remote_source(pad);
	if (IS_ERR(pad)) {
		gsc_err("No sink pad conncted with a gscaler source pad");
		return PTR_ERR(pad);
	}

	sd = media_entity_to_v4l2_subdev(pad->entity);
	gsc_sd = ctx->gsc_dev->out.sd;
	gsc_dbg("%s is connected to %s\n", gsc_sd->name, sd->name);
	if (strcmp(sd->name, "s5p-mixer0") && strcmp(sd->name, "s5p-mixer1")) {
		gsc_err("%s is not connected to mixer\n", gsc_sd->name);
		return -ENODEV;
	}

	switch (ctrl->id) {
	case V4L2_CID_TV_LAYER_BLEND_ENABLE:
	case V4L2_CID_TV_LAYER_BLEND_ALPHA:
	case V4L2_CID_TV_PIXEL_BLEND_ENABLE:
	case V4L2_CID_TV_CHROMA_ENABLE:
	case V4L2_CID_TV_CHROMA_VALUE:
	case V4L2_CID_TV_LAYER_PRIO:
		control.id = ctrl->id;
		control.value = ctrl->val;
		v4l2_subdev_call(sd, core, s_ctrl, &control);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * V4L2 controls handling
 */

static int gsc_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gsc_ctx *ctx = ctrl_to_ctx(ctrl);
	struct gsc_dev *gsc = ctx->gsc_dev;

	switch (ctrl->id) {
	case V4L2_CID_M2M_CTX_NUM:
		update_ctrl_value(ctx->gsc_ctrls.m2m_ctx_num, gsc->m2m.refcnt);
		break;

	default:
		gsc_err("Invalid control\n");
		return -EINVAL;
	}
	return 0;
}

static int gsc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct gsc_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		update_ctrl_value(ctx->gsc_ctrls.hflip, ctrl->val);
		break;

	case V4L2_CID_VFLIP:
		update_ctrl_value(ctx->gsc_ctrls.vflip, ctrl->val);
		break;

	case V4L2_CID_ROTATE:
		update_ctrl_value(ctx->gsc_ctrls.rotate, ctrl->val);
		break;

	case V4L2_CID_GLOBAL_ALPHA:
		update_ctrl_value(ctx->gsc_ctrls.global_alpha, ctrl->val);
		break;

	case V4L2_CID_CACHEABLE:
		update_ctrl_value(ctx->gsc_ctrls.cacheable, ctrl->val);
		break;

	case V4L2_CID_CSC_EQ_MODE:
		update_ctrl_value(ctx->gsc_ctrls.csc_eq_mode, ctrl->val);
		break;

	case V4L2_CID_CSC_EQ:
		update_ctrl_value(ctx->gsc_ctrls.csc_eq, ctrl->val);
		break;

	case V4L2_CID_CSC_RANGE:
		update_ctrl_value(ctx->gsc_ctrls.csc_range, ctrl->val);
		break;

	case V4L2_CID_CONTENT_PROTECTION:
		update_ctrl_value(ctx->gsc_ctrls.drm_en, ctrl->val);
		break;
	default:
		ret = gsc_s_ctrl_to_mxr(ctrl);
		if (ret) {
			gsc_err("Invalid control\n");
			return ret;
		}
	}

	if (gsc_m2m_opened(ctx->gsc_dev))
		gsc_ctx_state_lock_set(GSC_PARAMS, ctx);

	return 0;
}

const struct v4l2_ctrl_ops gsc_ctrl_ops = {
	.g_volatile_ctrl = gsc_g_ctrl,
	.s_ctrl = gsc_s_ctrl,
};

static const struct v4l2_ctrl_config gsc_custom_ctrl[] = {
	{
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_GLOBAL_ALPHA,
		.name = "Set RGB alpha",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = 0,
		.max = 255,
		.step = 1,
		.def = 0,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_CACHEABLE,
		.name = "Set cacheable",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.def = true,
		.min = false,
		.max = true,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_TV_LAYER_BLEND_ENABLE,
		.name = "Enable layer alpha blending",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.def = false,
		.min = false,
		.max = true,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_TV_LAYER_BLEND_ALPHA,
		.name = "Set alpha for layer blending",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = 0,
		.max = 255,
		.step = 1,
		.def = 0,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_TV_PIXEL_BLEND_ENABLE,
		.name = "Enable pixel alpha blending",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.def = false,
		.min = false,
		.max = true,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_TV_CHROMA_ENABLE,
		.name = "Enable chromakey",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.def = false,
		.min = false,
		.max = true,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_TV_CHROMA_VALUE,
		.name = "Set chromakey value",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = 0,
		.max = 255,
		.step = 1,
		.def = 0,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_TV_LAYER_PRIO,
		.name = "Set layer priority",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.min = 0,
		.max = 15,
		.def = 1,
		.step = 1,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_CSC_EQ_MODE,
		.name = "Set CSC equation mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.max = true,
		.min = false,
		.step = 1,
		.def = true,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_CSC_EQ,
		.name = "Set CSC equation",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.max = V4L2_COLORSPACE_SRGB,
		.min = 1,
		.step = 1,
		.def = V4L2_COLORSPACE_REC709,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_CSC_RANGE,
		.name = "Set CSC range",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.def = true,
		.step = 1,
		.max = true,
		.min = false,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_CONTENT_PROTECTION,
		.name = "Enable content protection",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.def = false,
		.step = 1,
		.max = true,
		.min = false,
	}, {
		.ops = &gsc_ctrl_ops,
		.id = V4L2_CID_M2M_CTX_NUM,
		.name = "Get number of m2m context",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_SLIDER,
		.step = 1,
		.min = 0,
		.max = 255,
		.def = 0,
	},
};

int gsc_ctrls_create(struct gsc_ctx *ctx)
{
	if (ctx->ctrls_rdy) {
		gsc_err("Control handler of this context was created already");
		return 0;
	}

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, GSC_MAX_CTRL_NUM);

	ctx->gsc_ctrls.rotate = v4l2_ctrl_new_std(&ctx->ctrl_handler,
				&gsc_ctrl_ops, V4L2_CID_ROTATE, 0, 270, 90, 0);
	ctx->gsc_ctrls.hflip = v4l2_ctrl_new_std(&ctx->ctrl_handler,
				&gsc_ctrl_ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	ctx->gsc_ctrls.vflip = v4l2_ctrl_new_std(&ctx->ctrl_handler,
				&gsc_ctrl_ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	ctx->gsc_ctrls.global_alpha = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[0], NULL);

	ctx->gsc_ctrls.cacheable = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[1], NULL);
	/* for mixer control */
	ctx->gsc_ctrls.layer_blend_en = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[2], NULL);
	ctx->gsc_ctrls.layer_alpha = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[3], NULL);
	ctx->gsc_ctrls.pixel_blend_en = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[4], NULL);
	ctx->gsc_ctrls.chroma_en = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[5], NULL);
	ctx->gsc_ctrls.chroma_val = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[6], NULL);
	ctx->gsc_ctrls.prio = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[7], NULL);

	/* for CSC equation */
	ctx->gsc_ctrls.csc_eq_mode = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[8], NULL);
	ctx->gsc_ctrls.csc_eq = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[9], NULL);
	ctx->gsc_ctrls.csc_range = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[10], NULL);

	ctx->gsc_ctrls.drm_en = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[11], NULL);

	/* for gscaler m2m context information */
	ctx->gsc_ctrls.m2m_ctx_num = v4l2_ctrl_new_custom(&ctx->ctrl_handler,
					&gsc_custom_ctrl[12], NULL);

	if (ctx->gsc_ctrls.m2m_ctx_num)
		ctx->gsc_ctrls.m2m_ctx_num->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctx->ctrls_rdy = ctx->ctrl_handler.error == 0;

	if (ctx->ctrl_handler.error) {
		int err = ctx->ctrl_handler.error;
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		gsc_err("Failed to gscaler control hander create");
		return err;
	}

	return 0;
}

void gsc_ctrls_delete(struct gsc_ctx *ctx)
{
	if (ctx->ctrls_rdy) {
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		ctx->ctrls_rdy = false;
	}
}

/* The color format (nr_comp, num_planes) must be already configured. */
int gsc_prepare_addr(struct gsc_ctx *ctx, struct vb2_buffer *vb,
		     struct gsc_frame *frame, struct gsc_addr *addr)
{
	struct gsc_dev *gsc = ctx->gsc_dev;
	int ret = 0;
	u32 pix_size;

	if (IS_ERR(vb) || IS_ERR(frame)) {
		gsc_err("Invalid argument");
		return -EINVAL;
	}

	pix_size = frame->f_width * frame->f_height;

	gsc_dbg("num_planes= %d, nr_comp= %d, pix_size= %d",
		frame->fmt->num_planes, frame->fmt->nr_comp, pix_size);

	addr->y = gsc->vb2->plane_addr(vb, 0);

	if (frame->fmt->num_planes == 1) {
		switch (frame->fmt->nr_comp) {
		case 1:
			addr->cb = 0;
			addr->cr = 0;
			break;
		case 2:
			/* decompose Y into Y/Cb */
			addr->cb = (dma_addr_t)(addr->y + pix_size);
			addr->cr = 0;
			break;
		case 3:
			if (is_AYV12(frame->fmt->pixelformat)) {
				addr->cb = (dma_addr_t)(addr->y + pix_size);
				addr->cr = (dma_addr_t)(addr->cb + (ALIGN(frame->f_width >> 1, 16)) *
						(frame->f_height >> 1));
			} else {
				addr->cb = (dma_addr_t)(addr->y + pix_size);
				addr->cr = (dma_addr_t)(addr->cb + (pix_size >> 2));
			}
			break;
		default:
			gsc_err("Invalid the number of color planes");
			return -EINVAL;
		}
	} else {
		if (frame->fmt->num_planes >= 2)
			addr->cb = gsc->vb2->plane_addr(vb, 1);
		if (frame->fmt->num_planes == 3)
			addr->cr = gsc->vb2->plane_addr(vb, 2);
	}

	if (frame->fmt->pixelformat == V4L2_PIX_FMT_YVU420 ||
	    frame->fmt->pixelformat == V4L2_PIX_FMT_YVU420M) {
		u32 t_cb = addr->cb;
		addr->cb = addr->cr;
		addr->cr = t_cb;
	}

	gsc_dbg("ADDR: y= 0x%X  cb= 0x%X cr= 0x%X ret= %d",
		addr->y, addr->cb, addr->cr, ret);

	return ret;
}

void gsc_cap_irq_handler(struct gsc_dev *gsc)
{
	int done_index;

	done_index = gsc_hw_get_done_output_buf_index(gsc);
	gsc_dbg("done_index : %d", done_index);
	if (done_index < 0) {
		gsc_err("All buffers are masked\n");
		return;
	}
	test_bit(ST_CAPT_RUN, &gsc->state) ? :
		set_bit(ST_CAPT_RUN, &gsc->state);
	vb2_buffer_done(gsc->cap.vbq.bufs[done_index], VB2_BUF_STATE_DONE);
}

static irqreturn_t gsc_irq_handler(int irq, void *priv)
{
	struct gsc_dev *gsc = priv;
	int gsc_irq;

#ifdef GSC_PERF
	gsc->end_time = sched_clock();
	gsc_dbg("OPERATION-TIME: %llu\n", gsc->end_time - gsc->start_time);
#endif
	gsc_irq = gsc_hw_get_irq_status(gsc);
	gsc_hw_clear_irq(gsc, gsc_irq);

	if (!(gsc_irq & GSC_IRQ_STATUS_FRM_DONE)) {
		gsc_err("Error interrupt(0x%x) occured", gsc_irq);
		exynos_sysmmu_show_status(&gsc->pdev->dev);
		gsc_hw_set_sw_reset(gsc);
		return IRQ_HANDLED;
	}

	spin_lock(&gsc->slock);

	if (test_and_clear_bit(ST_M2M_RUN, &gsc->state)) {
		struct vb2_buffer *src_vb, *dst_vb;
		struct gsc_ctx *ctx =
			v4l2_m2m_get_curr_priv(gsc->m2m.m2m_dev);

		if (!ctx || !ctx->m2m_ctx)
			goto isr_unlock;

		del_timer(&ctx->op_timer);
		src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		if (src_vb && dst_vb) {
			v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
			v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);

			if (test_and_clear_bit(ST_STOP_REQ, &gsc->state))
				wake_up(&gsc->irq_queue);
			else
				v4l2_m2m_job_finish(gsc->m2m.m2m_dev, ctx->m2m_ctx);

			/* wake_up job_abort, stop_streaming */
			spin_lock(&ctx->slock);
			if (ctx->state & GSC_CTX_STOP_REQ) {
				ctx->state &= ~GSC_CTX_STOP_REQ;
				wake_up(&gsc->irq_queue);
			}
			spin_unlock(&ctx->slock);
		}
		pm_runtime_put(&gsc->pdev->dev);
	} else if (test_bit(ST_OUTPUT_STREAMON, &gsc->state)) {
		if (!list_empty(&gsc->out.active_buf_q) &&
		    !list_is_singular(&gsc->out.active_buf_q)) {
			struct gsc_input_buf *done_buf;
			done_buf = active_queue_pop(&gsc->out, gsc);
			if (done_buf->idx != gsc_hw_get_curr_in_buf_idx(gsc)) {
				gsc_hw_set_input_buf_masking(gsc, done_buf->idx, true);
				gsc_out_set_pp_pending_bit(gsc, done_buf->idx, true);
				vb2_buffer_done(&done_buf->vb, VB2_BUF_STATE_DONE);
				list_del(&done_buf->list);
			}
		}
	} else if (test_bit(ST_CAPT_PEND, &gsc->state)) {
		gsc_cap_irq_handler(gsc);
	}

isr_unlock:
	spin_unlock(&gsc->slock);
	return IRQ_HANDLED;
}

static int gsc_get_media_info(struct device *dev, void *p)
{
	struct exynos_md **mdev = p;
	struct platform_device *pdev = to_platform_device(dev);

	mdev[pdev->id] = dev_get_drvdata(dev);

	if (!mdev[pdev->id])
		return -ENODEV;

	return 0;
}

void gsc_pm_qos_ctrl(struct gsc_dev *gsc, enum gsc_qos_status status,
			int mem_val, int int_val)
{
	int qos_cnt;

	if (status == GSC_QOS_ON) {
		qos_cnt = atomic_inc_return(&gsc->qos_cnt);
		gsc_dbg("mif val : %d, int val : %d", mem_val, int_val);
		if (qos_cnt == 1) {
			pm_qos_add_request(&gsc->exynos5_gsc_mif_qos,
					PM_QOS_BUS_THROUGHPUT, mem_val);
			pm_qos_add_request(&gsc->exynos5_gsc_int_qos,
					PM_QOS_DEVICE_THROUGHPUT, int_val);
		} else {
			pm_qos_update_request(&gsc->exynos5_gsc_mif_qos,
					mem_val);
			pm_qos_update_request(&gsc->exynos5_gsc_int_qos,
					int_val);
		}
	} else if (status == GSC_QOS_OFF) {
		qos_cnt = atomic_dec_return(&gsc->qos_cnt);
		if (qos_cnt == 0) {
			pm_qos_remove_request(&gsc->exynos5_gsc_mif_qos);
			pm_qos_remove_request(&gsc->exynos5_gsc_int_qos);
		}
	}
}

static void gsc_clk_put(struct gsc_dev *gsc)
{
	int i;
	for (i = 0; i < GSC_MAX_CLOCKS; i++) {
		if (IS_ERR_OR_NULL(gsc->clock[i]))
			continue;
		clk_unprepare(gsc->clock[i]);
		clk_put(gsc->clock[i]);
		gsc->clock[i] = NULL;
	}
}

static int gsc_clk_get(struct gsc_dev *gsc)
{
	int i, ret;

	for (i = 0; i < GSC_MAX_CLOCKS; i++)
		gsc->clock[i] = ERR_PTR(-EINVAL);

	for (i = 0; i < GSC_MAX_CLOCKS; i++) {
		gsc->clock[i] = __clk_lookup(gsc_clocks[i]);
		if (IS_ERR(gsc->clock[i])) {
			ret = PTR_ERR(gsc->clock[i]);
			goto err;
		}
		ret = clk_prepare(gsc->clock[i]);
		if (ret < 0) {
			clk_put(gsc->clock[i]);
			gsc->clock[i] = ERR_PTR(-EINVAL);
			goto err;
		}
	}
	return 0;
err:
	gsc_clk_put(gsc);
	dev_err(&gsc->pdev->dev, "failed to get clock: %s\n",
		gsc_clocks[i]);
	return -ENXIO;
}

void gsc_clock_gating(struct gsc_dev *gsc, enum gsc_clk_status status)
{
	int clk_cnt;

	if (status == GSC_CLK_ON) {
		clk_cnt = atomic_inc_return(&gsc->clk_cnt);
		if (clk_cnt == 1) {
			set_bit(ST_PWR_ON, &gsc->state);
		}
		clk_enable(gsc->clock[CLK_GATE]);
	} else if (status == GSC_CLK_OFF) {
		clk_cnt = atomic_dec_return(&gsc->clk_cnt);
		if (clk_cnt < 0) {
			gsc_err("clock count is out of range");
			atomic_set(&gsc->clk_cnt, 0);
		} else {
			clk_disable(gsc->clock[CLK_GATE]);
			if (clk_cnt == 0)
				clear_bit(ST_PWR_ON, &gsc->state);
		}
	}
}

int gsc_set_protected_content(struct gsc_dev *gsc, bool enable)
{
	if (gsc->protected_content == enable)
		return 0;

	if (enable)
		pm_runtime_get_sync(&gsc->pdev->dev);

	gsc->vb2->set_protected(gsc->alloc_ctx, enable);

	if (!enable)
		pm_runtime_put_sync(&gsc->pdev->dev);

	gsc->protected_content = enable;

	return 0;
}

void gsc_dump_registers(struct gsc_dev *gsc)
{
	pr_err("dumping registers\n");
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, gsc->regs,
			0x0280, false);
	pr_err("End of GSC_SFR DUMP\n");
}

static int gsc_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gsc_dev *gsc = (struct gsc_dev *)platform_get_drvdata(pdev);

	gsc_clock_gating(gsc, GSC_CLK_OFF);
	if (gsc_m2m_opened(gsc))
		gsc->m2m.ctx = NULL;

	return 0;
}

static int gsc_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gsc_dev *gsc = (struct gsc_dev *)platform_get_drvdata(pdev);

	gsc_hw_set_dynamic_clock_gating(gsc);

	if (clk_set_parent(gsc->clock[CLK_CHILD],
			gsc->clock[CLK_PARENT])) {
		dev_err(dev, "Unable to set parent %s of clock %s.\n",
			gsc_clocks[CLK_CHILD], gsc_clocks[CLK_PARENT]);
		return -EINVAL;
	}

	if (clk_set_parent(gsc->clock[CLK_S_CHILD],
			gsc->clock[CLK_S_PARENT])) {
		dev_err(dev, "Unable to set parent %s of clock %s.\n",
			gsc_clocks[CLK_S_CHILD],
			gsc_clocks[CLK_S_PARENT]);
		return -EINVAL;
	}

	gsc_clock_gating(gsc, GSC_CLK_ON);

	return 0;
}

static void gsc_pm_runtime_enable(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(dev);
#else
	gsc_runtime_resume(dev);
#endif
}

static void gsc_pm_runtime_disable(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(dev);
#else
	gsc_runtime_suspend(dev);
#endif
}
#ifdef CONFIG_OF
static void gsc_parse_dt(struct device_node *np, struct gsc_dev *gsc)
{
	struct exynos_platform_gscaler *pdata = gsc->pdata;

	if (!np)
		return;

	of_property_read_u32(np, "ip_ver", &pdata->ip_ver);
	of_property_read_u32(np, "mif_min", &pdata->mif_min);
	of_property_read_u32(np, "int_min", &pdata->int_min);
}
#else
static void gsc_parse_dt(struct device_node *np, struct gsc_dev *gsc)
{
	return;
}
#endif

struct gsc_pix_max gsc_v_max = {
	.org_w		= 4800,
	.org_h		= 3344,
	.real_w		= 4800,
	.real_h		= 3344,
	.target_w	= 4800,
	.target_h	= 3344,
	.rot_w		= 2047,
	.rot_h		= 2047,
	.otf_w		= 2560,
	.otf_h		= 1600,
};

struct gsc_pix_min gsc_v_min = {
	.org_w		= 64,
	.org_h		= 32,
	.real_w		= 64,
	.real_h		= 32,
	.target_w	= 32,
	.target_h	= 16,
	.otf_w		= 64,
	.otf_h		= 64,
};

struct gsc_pix_align gsc_v_align = {
		.org_w			= 4,
		.org_h			= 4,
		.real_w			= 1,
		.real_h			= 1,
		.target_w		= 2,
		.target_h		= 2,
};

struct gsc_variant gsc_variant = {
	.pix_max		= &gsc_v_max,
	.pix_min		= &gsc_v_min,
	.pix_align		= &gsc_v_align,
	.in_buf_cnt		= 4,
	.out_buf_cnt		= 16,
	.sc_up_max		= 8,
	.sc_down_max		= 16,
	.poly_sc_down_max	= 4,
	.pre_sc_down_max	= 4,
	.local_sc_down		= 4,
};

static struct gsc_driverdata gsc_drvdata = {
	.variant = {
		[0] = &gsc_variant,
		[1] = &gsc_variant,
		[2] = &gsc_variant,
		[3] = &gsc_variant,
	},
	.num_entities = 4,
};

static struct platform_device_id gsc_driver_ids[] = {
	{
		.name		= "exynos-gsc",
		.driver_data	= (unsigned long)&gsc_drvdata,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, gsc_driver_ids);

static const struct of_device_id exynos_gsc_match[] = {
	{
		.compatible = "samsung,exynos5-gsc",
		.data = &gsc_drvdata,
	},
	{},
};

MODULE_DEVICE_TABLE(of, exynos_gsc_match);

static void *gsc_get_drv_data(struct platform_device *pdev)
{
	struct gsc_driverdata *driver_data = NULL;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(of_match_ptr(exynos_gsc_match),
					pdev->dev.of_node);
		if (match)
			driver_data = (struct gsc_driverdata *)match->data;
	} else {
		driver_data = (struct gsc_driverdata *)
			platform_get_device_id(pdev)->driver_data;
	}

	return driver_data;
}

static int gsc_probe(struct platform_device *pdev)
{
	struct gsc_dev *gsc;
	struct resource *res;
	struct gsc_driverdata *drv_data = gsc_get_drv_data(pdev);
	struct device *dev = &pdev->dev;
	struct device_driver *driver;
	struct exynos_md *mdev[MDEV_MAX_NUM] = {NULL,};
	struct exynos_platform_gscaler *pdata = NULL;
	int ret = 0;
	char workqueue_name[WORKQUEUE_NAME_SIZE];

	gsc = devm_kzalloc(dev, sizeof(struct gsc_dev), GFP_KERNEL);
	if (!gsc)
		return -ENOMEM;

	if (dev->of_node) {
		gsc->id = of_alias_get_id(pdev->dev.of_node, "gsc");
	} else {
		gsc->id = pdev->id;
		pdata = dev->platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data\n");
			return -EINVAL;
		}
	}

	if (gsc->id >= drv_data->num_entities) {
		dev_err(dev, "Invalid platform device id: %d\n", gsc->id);
		return -EINVAL;
	}

	gsc->variant = drv_data->variant[gsc->id];
	gsc->pdev = pdev;

	gsc->pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!gsc->pdata) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	if (pdata) {
		memcpy(gsc->pdata, pdata, sizeof(*pdata));
	} else {
		gsc_parse_dt(dev->of_node, gsc);
		pdata = gsc->pdata;
	}

	init_waitqueue_head(&gsc->irq_queue);
	spin_lock_init(&gsc->slock);
	mutex_init(&gsc->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gsc->regs = devm_request_and_ioremap(dev, res);
	if (!gsc->regs) {
		dev_err(dev, "failed to map registers\n");
		return -ENOENT;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "failed to get IRQ resource\n");
		return -ENXIO;
	}

	ret = gsc_clk_get(gsc);
	if (ret)
		return ret;

	ret = devm_request_irq(dev, res->start, gsc_irq_handler,
				0, pdev->name, gsc);
	if (ret) {
		dev_err(dev, "failed to install irq (%d)\n", ret);
		goto err_clk_put;
	}

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
	gsc->vb2 = &gsc_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
	gsc->vb2 = &gsc_vb2_ion;
#endif

	platform_set_drvdata(pdev, gsc);

	ret = gsc_register_m2m_device(gsc);
	if (ret)
		goto err_clk_put;

	/* find media device */
	driver = driver_find(MDEV_MODULE_NAME, &platform_bus_type);

	if (!driver)
		goto err_m2m;

	ret = driver_for_each_device(driver, NULL, &mdev[0],
			gsc_get_media_info);
	if (ret)
		goto err_m2m;

	gsc->mdev[MDEV_OUTPUT] = mdev[MDEV_OUTPUT];
	gsc->mdev[MDEV_CAPTURE] = mdev[MDEV_CAPTURE];

	gsc_info("mdev->mdev[%d] = 0x%08x, mdev->mdev[%d] = 0x%08x",
		 MDEV_OUTPUT, (u32)gsc->mdev[MDEV_OUTPUT], MDEV_CAPTURE,
		 (u32)gsc->mdev[MDEV_CAPTURE]);

	ret = gsc_register_output_device(gsc);
	if (ret)
		goto err_m2m;

	snprintf(workqueue_name, WORKQUEUE_NAME_SIZE,
			"gsc%d_irq_wq_name", gsc->id);
	gsc->irq_workqueue = create_singlethread_workqueue(workqueue_name);
	if (gsc->irq_workqueue == NULL) {
		dev_err(&pdev->dev, "failed to create workqueue for gsc\n");
		goto err_out;
	}

	gsc->alloc_ctx = gsc->vb2->init(gsc);
	if (IS_ERR(gsc->alloc_ctx)) {
		ret = PTR_ERR(gsc->alloc_ctx);
		goto err_m2m;
	}

	exynos_create_iovmm(&pdev->dev, 3, 3);
	gsc->vb2->resume(gsc->alloc_ctx);

	gsc_hw_set_dynamic_clock_gating(gsc);

	gsc_pm_runtime_enable(&pdev->dev);

	gsc_info("gsc-%d registered successfully", gsc->id);

	return 0;
err_out:
	gsc_unregister_output_device(gsc);
err_m2m:
	gsc_unregister_m2m_device(gsc);
err_clk_put:
	gsc_clk_put(gsc);

	return ret;
}

static int gsc_remove(struct platform_device *pdev)
{
	struct gsc_dev *gsc =
		(struct gsc_dev *)platform_get_drvdata(pdev);

	gsc_unregister_m2m_device(gsc);
	gsc_unregister_output_device(gsc);
	gsc_unregister_capture_device(gsc);

	gsc->vb2->cleanup(gsc->alloc_ctx);
	gsc_pm_runtime_disable(&pdev->dev);
	gsc_clk_put(gsc);
	gsc->vb2->suspend(gsc->alloc_ctx);

	kfree(gsc);

	dev_info(&pdev->dev, "%s driver unloaded\n", pdev->name);
	return 0;
}

static int gsc_suspend(struct device *dev)
{
	struct platform_device *pdev;
	struct gsc_dev *gsc;
	int ret = 0;

	pdev = to_platform_device(dev);
	gsc = (struct gsc_dev *)platform_get_drvdata(pdev);

	if (gsc_m2m_run(gsc)) {
		set_bit(ST_STOP_REQ, &gsc->state);
		ret = wait_event_timeout(gsc->irq_queue,
				!test_bit(ST_STOP_REQ, &gsc->state),
				GSC_SHUTDOWN_TIMEOUT);
		if (ret == 0)
			dev_err(&gsc->pdev->dev, "wait timeout : %s\n",
				__func__);
	}
	if (gsc_cap_active(gsc)) {
		gsc_err("capture device is running!!");
		return -EINVAL;
	}
#ifndef CONFIG_PM_RUNTIME
	gsc_clock_gating(gsc, GSC_CLK_OFF);
#endif
	return ret;
}

static int gsc_resume(struct device *dev)
{
	struct platform_device *pdev;
	struct gsc_driverdata *drv_data;
	struct gsc_dev *gsc;
	struct gsc_ctx *ctx;

	pdev = to_platform_device(dev);
	gsc = (struct gsc_dev *)platform_get_drvdata(pdev);
	drv_data = (struct gsc_driverdata *)
		platform_get_device_id(pdev)->driver_data;

#ifndef CONFIG_PM_RUNTIME
	gsc_clock_gating(gsc, GSC_CLK_ON);
#endif
	if (gsc_m2m_opened(gsc)) {
		ctx = v4l2_m2m_get_curr_priv(gsc->m2m.m2m_dev);
		if (ctx != NULL) {
			gsc->m2m.ctx = NULL;
			v4l2_m2m_job_finish(gsc->m2m.m2m_dev, ctx->m2m_ctx);
		}
	}

	return 0;
}

static const struct dev_pm_ops gsc_pm_ops = {
	.suspend		= gsc_suspend,
	.resume			= gsc_resume,
	.runtime_suspend	= gsc_runtime_suspend,
	.runtime_resume		= gsc_runtime_resume,
};

static struct platform_driver gsc_driver = {
	.probe		= gsc_probe,
	.remove		= gsc_remove,
	.id_table	= gsc_driver_ids,
	.driver = {
		.name	= GSC_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &gsc_pm_ops,
		.of_match_table = exynos_gsc_match,
	}
};

module_platform_driver(gsc_driver);

MODULE_AUTHOR("Hyunwong Kim <khw0178.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS5 Soc series G-Scaler driver");
MODULE_LICENSE("GPL");
