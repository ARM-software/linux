/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * HW control file for Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "g2d.h"
#include "g2d-regs.h"

static const int a8_rgbcolor		= (int)0x0;
static const int msk_oprmode		= (int)MSK_ARGB;

/* (A+1)*B) >> 8 */
static const int premult_round_mode	= (int)PREMULT_ROUND_1;

/* (A+1)*B) >> 8 */
static const int blend_round_mode	= (int)BLEND_ROUND_0;

int g2d_hwset_src_image_format(struct g2d_dev *g2d, u32 pixelformat)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_SRC_COLOR_MODE_REG);

	switch (pixelformat) {
	case V4L2_PIX_FMT_RGB565:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_RGB_565;
		break;
	case V4L2_PIX_FMT_RGB555X:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_1555;
		break;
	case V4L2_PIX_FMT_RGB444:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_4444;
		break;
	case V4L2_PIX_FMT_RGB32:
		cfg |= FIMG2D_RGB_ORDER_AXBGR;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_8888;
		break;
	case V4L2_PIX_FMT_BGR32:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_8888;
		break;
	case V4L2_PIX_FMT_YUYV:
		cfg |= FIMG2D_YCBCR_ORDER_P1_CRY1CBY0;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_UYVY:
		cfg |= FIMG2D_YCBCR_ORDER_P1_Y1CRY0CB;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_YVYU:
		cfg |= FIMG2D_YCBCR_ORDER_P1_CBY1CRY0;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_VYUY:
		cfg |= FIMG2D_YCBCR_ORDER_P1_Y1CBY0CR;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_NV12:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV21:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV12M:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV21M:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV16:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_NV61:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_NV24:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_444;
		break;
	case V4L2_PIX_FMT_NV42:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_444;
		break;
	/* TODO: add L8A8 and L8 source format */
	default:
		dev_err(g2d->dev, "invalid pixelformat type\n");
		return -EINVAL;
	}
	writel(cfg, g2d->regs + FIMG2D_SRC_COLOR_MODE_REG);
	return 0;
}

int g2d_hwset_dst_image_format(struct g2d_dev *g2d, u32 pixelformat)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_DST_COLOR_MODE_REG);

	switch (pixelformat) {
	case V4L2_PIX_FMT_RGB565:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_RGB_565;
		break;
	case V4L2_PIX_FMT_RGB555X:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_1555;
		break;
	case V4L2_PIX_FMT_RGB444:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_4444;
		break;
	case V4L2_PIX_FMT_RGB32:
		cfg |= FIMG2D_RGB_ORDER_AXBGR;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_8888;
		break;
	case V4L2_PIX_FMT_BGR32:
		cfg |= FIMG2D_RGB_ORDER_AXRGB;
		cfg |= FIMG2D_COLOR_FORMAT_ARGB_8888;
		break;
	case V4L2_PIX_FMT_YUYV:
		cfg |= FIMG2D_YCBCR_ORDER_P1_CRY1CBY0;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_UYVY:
		cfg |= FIMG2D_YCBCR_ORDER_P1_Y1CRY0CB;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_YVYU:
		cfg |= FIMG2D_YCBCR_ORDER_P1_CBY1CRY0;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_VYUY:
		cfg |= FIMG2D_YCBCR_ORDER_P1_Y1CBY0CR;
		cfg |= FIMG2D_YCBCR_1PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_NV12:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV21:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV12M:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV21M:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_420;
		break;
	case V4L2_PIX_FMT_NV16:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_NV61:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_422;
		break;
	case V4L2_PIX_FMT_NV24:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CRCB;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_444;
		break;
	case V4L2_PIX_FMT_NV42:
		cfg |= FIMG2D_YCBCR_ORDER_P2_CBCR;
		cfg |= FIMG2D_YCBCR_2PLANE;
		cfg |= FIMG2D_COLOR_FORMAT_YCBCR_444;
		break;
	/* TODO: add L8A8 and L8 source format */
	default:
		dev_err(g2d->dev, "invalid pixelformat type\n");
		return -EINVAL;
	}
	writel(cfg, g2d->regs + FIMG2D_DST_COLOR_MODE_REG);
	return 0;
}

void g2d_hwset_src_type(struct g2d_dev *g2d, enum image_sel type)
{
	unsigned long cfg;

	if (type == IMG_MEMORY)
		cfg = FIMG2D_IMAGE_TYPE_MEMORY;
	else if (type == IMG_FGCOLOR)
		cfg = FIMG2D_IMAGE_TYPE_FGCOLOR;
	else
		cfg = FIMG2D_IMAGE_TYPE_BGCOLOR;

	writel(cfg, g2d->regs + FIMG2D_SRC_SELECT_REG);
}

void g2d_hwset_dst_type(struct g2d_dev *g2d, enum image_sel type)
{
	unsigned long cfg;

	if (type == IMG_MEMORY)
		cfg = FIMG2D_IMAGE_TYPE_MEMORY;
	else if (type == IMG_FGCOLOR)
		cfg = FIMG2D_IMAGE_TYPE_FGCOLOR;
	else
		cfg = FIMG2D_IMAGE_TYPE_BGCOLOR;

	writel(cfg, g2d->regs + FIMG2D_DST_SELECT_REG);
}

void g2d_hwset_pre_multi_format(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_PREMULT_ALL;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

void g2d_hwset_set_max_burst_length(struct g2d_dev *g2d, enum g2d_max_burst_len len)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_AXI_MODE_REG);

	cfg &=  ~FIMG2D_MAX_BURST_LEN_MASK;
	cfg |= len << FIMG2D_MAX_BURST_LEN_SHIFT;
	writel(cfg, g2d->regs + FIMG2D_AXI_MODE_REG);
}

/* fimg2d4x_enable_msk() */
void g2d_hwset_enable_msk(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_ENABLE_NORMAL_MSK;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

/* fimg2d4x_set_msk_image() */
void g2d_hwset_msk_image(struct g2d_dev *g2d, struct fimg2d_image *m)
{
	unsigned long cfg;

	writel(FIMG2D_ADDR(m->addr.start), g2d->regs + FIMG2D_MSK_BASE_ADDR_REG);
	writel(FIMG2D_STRIDE(m->stride), g2d->regs + FIMG2D_MSK_STRIDE_REG);

	cfg = m->order << FIMG2D_MSK_ORDER_SHIFT;
	cfg |= (m->fmt - CF_MSK_1BIT) << FIMG2D_MSK_FORMAT_SHIFT;

	/* 16, 32bit mask only */
	if (m->fmt >= CF_MSK_16BIT_565) {
		if (msk_oprmode == MSK_ALPHA)
			cfg |= FIMG2D_MSK_TYPE_ALPHA;
		else if (msk_oprmode == MSK_ARGB)
			cfg |= FIMG2D_MSK_TYPE_ARGB;
		else
			cfg |= FIMG2D_MSK_TYPE_MIXED;
	}

	writel(cfg, g2d->regs + FIMG2D_MSK_MODE_REG);
}

/* fimg2d4x_set_msk_rect() */
void g2d_hwset_msk_rect(struct g2d_dev *g2d, struct fimg2d_rect *r)
{
	writel(FIMG2D_OFFSET(r->x1, r->y1), g2d->regs + FIMG2D_MSK_LEFT_TOP_REG);
	writel(FIMG2D_OFFSET(r->x2, r->y2), g2d->regs + FIMG2D_MSK_RIGHT_BOTTOM_REG);
}

/* fimg2d4x_set_color_fill() */
/**
 * If solid color fill is enabled, other blit command is ignored.
 * Color format of solid color is considered to be
 *	the same as destination color format
 * Channel order of solid color is A-R-G-B or Y-Cb-Cr
 */
void g2d_hwset_color_fill(struct g2d_dev *g2d, unsigned long color)
{
	writel(FIMG2D_SOLID_FILL, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);

	/* sf color */
	writel(color, g2d->regs + FIMG2D_SF_COLOR_REG);

	/* set 16 burst for performance */
	g2d_hwset_set_max_burst_length(g2d, MAX_BURST_16);
}

/* fimg2d4x_set_premultiplied() */
/**
 * set alpha-multiply mode for src, dst, pat read (pre-bitblt)
 * set alpha-demultiply for dst write (post-bitblt)
 */
void g2d_hwset_premultiplied(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_PREMULT_ALL;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

/* fimg2d4x_src_premultiply() */
void g2d_hwsrc_premultiply(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_SRC_PREMULT;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

/* fimg2d4x_dst_premultiply() */
void g2d_hwset_dst_premultiply(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_DST_RD_PREMULT;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

/* void fimg2d4x_dst_depremultiply() */
void g2d_hwset_dst_depremultiply(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_DST_WR_DEPREMULT;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

/* fimg2d4x_set_bluescreen() */
/**
 * set transp/bluscr mode, bs color, bg color
 */
void g2d_hwset_bluescreen(struct g2d_dev *g2d,
		struct fimg2d_bluscr *bluscr)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);

	if (bluscr->mode == TRANSP)
		cfg |= FIMG2D_TRANSP_MODE;
	else if (bluscr->mode == BLUSCR)
		cfg |= FIMG2D_BLUSCR_MODE;
	else	/* opaque: initial value */
		return;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);

	/* bs color */
	if (bluscr->bs_color)
		writel(bluscr->bs_color, g2d->regs + FIMG2D_BS_COLOR_REG);

	/* bg color */
	if (bluscr->mode == BLUSCR && bluscr->bg_color)
		writel(bluscr->bg_color, g2d->regs + FIMG2D_BG_COLOR_REG);
}

/* fimg2d4x_enable_clipping() */
/**
 * @c: destination clipping region
 */
void g2d_hwset_enable_clipping(struct g2d_dev *g2d,
				struct v4l2_rect *clip)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_ENABLE_CW;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);

	g2d_dbg("left:%d, top:%d, width:%d, height:%d\n"
			, clip->left, clip->top, clip->width, clip->height);
	writel(FIMG2D_OFFSET(clip->left, clip->top), g2d->regs + FIMG2D_CW_LT_REG);
	writel(FIMG2D_OFFSET(clip->left + clip->width, clip->top + clip->height)
			, g2d->regs + FIMG2D_CW_RB_REG);
}

/* fimg2d4x_enable_dithering() */
void g2d_hwset_enable_dithering(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_ENABLE_DITHER;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
}

#define MAX_PRECISION		16
#define DEFAULT_SCALE_RATIO	0x10000

/**
 * scale_factor_to_fixed16 - convert scale factor to fixed pint 16
 * @n: numerator
 * @d: denominator
 */
static inline unsigned long scale_factor_to_fixed16(int n, int d)
{
	int i;
	u32 fixed16;

	if (!d)
		return DEFAULT_SCALE_RATIO;

	fixed16 = (n/d) << 16;
	n %= d;

	for (i = 0; i < MAX_PRECISION; i++) {
		if (!n)
			break;
		n <<= 1;
		if (n/d)
			fixed16 |= 1 << (15-i);
		n %= d;
	}

	return fixed16;
}

/* fimg2d4x_set_src_scaling() */
void g2d_hwset_src_scaling(struct g2d_dev *g2d,
				struct fimg2d_scale *scl,
				struct fimg2d_repeat *rep)
{
	unsigned long wcfg, hcfg;
	unsigned long mode;
	//int src_w, dst_w, src_h, dst_h;

	//src_w = s_frame->pix_mp.width;
	//dst_w = d_frame->pix_mp.width;
	//src_h = s_frame->pix_mp.height;
	//dst_h = d_frame->pix_mp.height;

	g2d_dbg("src_w:%d, src_h:%d, dst_w:%d, dst_h:%d\n"
			, scl->src_w, scl->src_h, scl->dst_w, scl->dst_h);

	/*
	 * scaling ratio in pixels
	 * e.g scale-up: src(1,1)-->dst(2,2), src factor: 0.5 (0x000080000)
	 *     scale-down: src(2,2)-->dst(1,1), src factor: 2.0 (0x000200000)
	 */

	/* inversed scaling factor: src is numerator */
	wcfg = scale_factor_to_fixed16(scl->src_w, scl->dst_w);
	hcfg = scale_factor_to_fixed16(scl->src_h, scl->dst_h);

	g2d_dbg("wcfg:%ld, hcfg:%ld, src_w:%d, src_h:%d, dst_w:%d, dst_h:%d\n"
			, wcfg, hcfg, scl->src_w, scl->src_h, scl->dst_w, scl->dst_h);

	if (wcfg == DEFAULT_SCALE_RATIO && hcfg == DEFAULT_SCALE_RATIO)
		return;

	writel(wcfg, g2d->regs + FIMG2D_SRC_XSCALE_REG);
	writel(hcfg, g2d->regs + FIMG2D_SRC_YSCALE_REG);


	/* scaling algorithm */
	if (scl->mode == SCALING_NEAREST)
		mode = FIMG2D_SCALE_MODE_NEAREST;
	else {
		/* 0x3: ignore repeat mode at boundary */
		if (rep->mode == REPEAT_PAD || rep->mode == REPEAT_CLAMP)
			mode = 0x3;	/* hidden */
		else
			mode = FIMG2D_SCALE_MODE_BILINEAR;
	}

	g2d_dbg("scale_mode:%d, mode:0x%lx\n", scl->mode, mode);

	writel(mode, g2d->regs + FIMG2D_SRC_SCALE_CTRL_REG);
}

/* fimg2d4x_set_msk_scaling() */
void g2d_hwset_msk_scaling(struct g2d_dev *g2d,
				struct g2d_frame *s_frame,
				struct g2d_frame *d_frame,
				enum scaling scale_mode,
				enum repeat repeat_mode)
{
	unsigned long wcfg, hcfg;
	unsigned long mode;
	int src_w, dst_w, src_h, dst_h;

	src_w = s_frame->pix_mp.width;
	dst_w = d_frame->pix_mp.width;
	src_h = s_frame->pix_mp.height;
	dst_h = d_frame->pix_mp.height;

	/*
	 * scaling ratio in pixels
	 * e.g scale-up: src(1,1)-->dst(2,2), msk factor: 0.5 (0x000080000)
	 *     scale-down: src(2,2)-->dst(1,1), msk factor: 2.0 (0x000200000)
	 */

	/* inversed scaling factor: src is numerator */
	wcfg = scale_factor_to_fixed16(src_w, dst_w);
	hcfg = scale_factor_to_fixed16(src_h, dst_h);

	if (wcfg == DEFAULT_SCALE_RATIO && hcfg == DEFAULT_SCALE_RATIO)
		return;

	writel(wcfg, g2d->regs + FIMG2D_MSK_XSCALE_REG);
	writel(hcfg, g2d->regs + FIMG2D_MSK_YSCALE_REG);

	/* scaling algorithm */
	if (scale_mode == SCALING_NEAREST)
		mode = FIMG2D_SCALE_MODE_NEAREST;
	else {
		/* 0x3: ignore repeat mode at boundary */
		if (repeat_mode == REPEAT_PAD || repeat_mode == REPEAT_CLAMP)
			mode = 0x3;	/* hidden */
		else
			mode = FIMG2D_SCALE_MODE_BILINEAR;
	}

	writel(mode, g2d->regs + FIMG2D_MSK_SCALE_CTRL_REG);
}

/* fimg2d4x_set_src_repeat() */
void g2d_hwset_src_repeat(struct g2d_dev *g2d,
				struct fimg2d_repeat *rep)
{
	unsigned long cfg;

	if (rep->mode == NO_REPEAT)
		return;

	g2d_dbg("repeat_mode:%d, pad_color:%ld\n", rep->mode, rep->pad_color);

	cfg = (rep->mode - REPEAT_NORMAL) << FIMG2D_SRC_REPEAT_SHIFT;

	writel(cfg, g2d->regs + FIMG2D_SRC_REPEAT_MODE_REG);

	/* src pad color */
	if (rep->mode == REPEAT_PAD)
		writel(rep->pad_color, g2d->regs + FIMG2D_SRC_PAD_VALUE_REG);
}

/* fimg2d4x_set_msk_repeat() */
void g2d_hwset_msk_repeat(struct g2d_dev *g2d,
				enum repeat repeat_mode,
				unsigned long pad_color)
{
	unsigned long cfg;

	if (repeat_mode == NO_REPEAT)
		return;

	cfg = (repeat_mode - REPEAT_NORMAL) << FIMG2D_MSK_REPEAT_SHIFT;

	writel(cfg, g2d->regs + FIMG2D_MSK_REPEAT_MODE_REG);

	/* mask pad color */
	if (repeat_mode == REPEAT_PAD)
		writel(pad_color, g2d->regs + FIMG2D_MSK_PAD_VALUE_REG);
}

/* fimg2d4x_set_rotation() */
void g2d_hwset_rotation(struct g2d_dev *g2d, u32 direction, int degree)
{
	int rev_rot90;	/* counter clockwise, 4.1 specific */
	unsigned long cfg;
	enum addressing dirx, diry;

	g2d_dbg("direction : %d, degree : %d\n", direction, degree);

	rev_rot90 = 0;
	dirx = diry = FORWARD_ADDRESSING;

	if (direction & G2D_VFLIP)
		diry = REVERSE_ADDRESSING;
	if (direction & G2D_HFLIP)
		dirx = REVERSE_ADDRESSING;

	if (degree == 90) {
		rev_rot90 = 1;	/* fall through */
		dirx = REVERSE_ADDRESSING;
		diry = REVERSE_ADDRESSING;
	}
	else if (degree == 180) {
		dirx = REVERSE_ADDRESSING;
		diry = REVERSE_ADDRESSING;
	} else if (degree == 270)
		rev_rot90 = 1;

	/* destination direction */
	if (dirx == REVERSE_ADDRESSING || diry == REVERSE_ADDRESSING) {
		cfg = readl(g2d->regs + FIMG2D_DST_PAT_DIRECT_REG);

		if (dirx == REVERSE_ADDRESSING)
			cfg |= FIMG2D_DST_X_DIR_NEGATIVE;

		if (diry == REVERSE_ADDRESSING)
			cfg |= FIMG2D_DST_Y_DIR_NEGATIVE;

		writel(cfg, g2d->regs + FIMG2D_DST_PAT_DIRECT_REG);
	}

	/* rotation -90 */
	if (rev_rot90) {
		cfg = readl(g2d->regs + FIMG2D_ROTATE_REG);
		cfg |= FIMG2D_SRC_ROTATE_90;
		cfg |= FIMG2D_MSK_ROTATE_90;

		writel(cfg, g2d->regs + FIMG2D_ROTATE_REG);
	}
}


/* fimg2d4x_set_fgcolor() */
void g2d_hwset_fgcolor(struct g2d_dev *g2d, unsigned long fg)
{
	writel(fg, g2d->regs + FIMG2D_FG_COLOR_REG);
}

/* fimg2d4x_set_bgcolor() */
void g2d4x_hwset_bgcolor(struct g2d_dev *g2d, unsigned long bg)
{
	writel(bg, g2d->regs + FIMG2D_BG_COLOR_REG);
}

/* fimg2d4x_enable_alpha() */
void g2d_hwset_enable_alpha(struct g2d_dev *g2d, unsigned char g_alpha)
{
	unsigned long cfg;

	/* enable alpha */
	cfg = readl(g2d->regs + FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_ALPHA_BLEND_MODE;

	writel(cfg, g2d->regs + FIMG2D_BITBLT_COMMAND_REG);

	/*
	 * global(constant) alpha
	 * ex. if global alpha is 0x80, must set 0x80808080
	 */
	cfg = g_alpha;
	cfg |= g_alpha << 8;
	cfg |= g_alpha << 16;
	cfg |= g_alpha << 24;
	writel(cfg, g2d->regs + FIMG2D_ALPHA_REG);
}

void g2d_hwset_blend(struct g2d_dev *sc, enum g2d_blend_op bl_op, bool pre_multi)
{
	enum image_sel srcsel, dstsel;

	srcsel = dstsel = IMG_MEMORY;
}

void g2d_hwset_set_src_type(struct g2d_dev *g2d, enum image_sel type)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_SRC_SELECT_REG);

	if (type == IMG_MEMORY)
		cfg = FIMG2D_IMAGE_TYPE_MEMORY;
	else if (type == IMG_FGCOLOR)
		cfg = FIMG2D_IMAGE_TYPE_FGCOLOR;
	else
		cfg = FIMG2D_IMAGE_TYPE_BGCOLOR;

	writel(cfg, g2d->regs + FIMG2D_SRC_SELECT_REG);
}

/* fimg2d4x_set_src_image */
void g2d_hwset_src_addr(struct g2d_dev *g2d, struct g2d_frame *s_frame)
{
	unsigned long cfg;

	cfg = FIMG2D_ADDR(s_frame->addr.y);
	writel(cfg, g2d->regs + FIMG2D_SRC_BASE_ADDR_REG);

	cfg = FIMG2D_ADDR(s_frame->addr.c);
	writel(cfg, g2d->regs + FIMG2D_SRC_PLANE2_BASE_ADDR_REG);

	//if (s->fmt == CF_A8)
	//	writel(a8_rgbcolor, g2d->regs + FIMG2D_SRC_A8_RGB_EXT_REG);
}

void g2d_hwset_src_stride(struct g2d_dev *g2d, int stride)
{
	unsigned long cfg;

	g2d_dbg("src stride:%d\n", stride);
	cfg = FIMG2D_STRIDE(stride);
	writel(cfg, g2d->regs + FIMG2D_SRC_STRIDE_REG);
}

void g2d_hwset_src_rect(struct g2d_dev *g2d, struct v4l2_rect *rect)
{
	unsigned long cfg;

	g2d_dbg("left:%d, top:%d, width:%d, height:%d\n"
			, rect->left, rect->top, rect->width, rect->height);
	cfg = FIMG2D_OFFSET(rect->left, rect->top);
	writel(cfg, g2d->regs + FIMG2D_SRC_LEFT_TOP_REG);

	cfg = FIMG2D_OFFSET((rect->left + rect->width), (rect->top + rect->height));
	writel(cfg, g2d->regs + FIMG2D_SRC_RIGHT_BOTTOM_REG);
}

/* fimg2d4x_set_dst_rect() */
void g2d_hwset_dst_rect(struct g2d_dev *g2d, struct v4l2_rect *rect)
{
	unsigned long cfg;

	g2d_dbg("left:%d, top:%d, width:%d, height:%d\n"
			, rect->left, rect->top, rect->width, rect->height);
	cfg = FIMG2D_OFFSET(rect->left, rect->top);
	writel(cfg, g2d->regs + FIMG2D_DST_LEFT_TOP_REG);

	cfg = FIMG2D_OFFSET((rect->left + rect->width), (rect->top + rect->height));
	writel(cfg, g2d->regs + FIMG2D_DST_RIGHT_BOTTOM_REG);
}
/* fimg2d4x_set_dst_image() */
void g2d_hwset_dst_addr(struct g2d_dev *g2d, struct g2d_frame *d_frame)
{
	unsigned long cfg;

	cfg = FIMG2D_ADDR(d_frame->addr.y);
	writel(cfg, g2d->regs + FIMG2D_DST_BASE_ADDR_REG);

	cfg = FIMG2D_ADDR(d_frame->addr.c);
	writel(cfg, g2d->regs + FIMG2D_DST_PLANE2_BASE_ADDR_REG);

	//if (s->fmt == CF_A8)
	//	writel(a8_rgbcolor, g2d->regs + FIMG2D_SRC_A8_RGB_EXT_REG);
}

void g2d_hwset_dst_stride(struct g2d_dev *g2d, int stride)
{
	unsigned long cfg;

	g2d_dbg("dst stride:%d\n", stride);
	cfg = FIMG2D_STRIDE(stride);
	writel(cfg, g2d->regs + FIMG2D_DST_STRIDE_REG);
}

void g2d_hwset_start_blit(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_BITBLT_START_REG);

	cfg |= FIMG2D_START_BITBLT;
	writel(cfg, g2d->regs + FIMG2D_BITBLT_START_REG);
}

int g2d_hwset_blit_done_status(struct g2d_dev *g2d)
{
	volatile unsigned long cfg;

	/* read twice */
	cfg = readl(g2d->regs + FIMG2D_FIFO_STAT_REG);
	cfg = readl(g2d->regs + FIMG2D_FIFO_STAT_REG);
	cfg &= FIMG2D_BLIT_FINISHED;

	return (int)cfg;
}

int g2d_hwset_is_blit_done(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_INTC_PEND_REG);

	cfg &= FIMG2D_BLIT_INT_FLAG;
	return cfg;
}

void g2d_hwset_int_enable(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_INTEN_REG);

	cfg |= FIMG2D_BLIT_INT_ENABLE;
	writel(cfg, g2d->regs + FIMG2D_INTEN_REG);
}

void g2d_hwset_int_disable(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_INTEN_REG);

	cfg &= ~(FIMG2D_BLIT_INT_ENABLE);
	writel(cfg, g2d->regs + FIMG2D_INTEN_REG);
}

void g2d_hwset_int_clear(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_INTC_PEND_REG);

	cfg |= FIMG2D_BLIT_INT_FLAG;
	writel(cfg, g2d->regs + FIMG2D_INTC_PEND_REG);
}

int g2d_hwset_version(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_VERSION_REG);

	g2d_dbg("This G2D version is 0x%lut\n", cfg);
	return cfg & 0xffff;
}

void g2d_hwset_soft_reset(struct g2d_dev *g2d)
{
	unsigned long cfg = readl(g2d->regs + FIMG2D_SOFT_RESET_REG);

	cfg |= FIMG2D_SOFT_RESET;
	writel(cfg, g2d->regs + FIMG2D_SOFT_RESET_REG);
	g2d_dbg("done soft reset\n");
}

void g2d_hwset_init(struct g2d_dev *g2d)
{
	unsigned long cfg;

	/* sfr clear */
	cfg = readl(g2d->regs + FIMG2D_SOFT_RESET_REG);
	cfg |= FIMG2D_SFR_CLEAR;
	writel(cfg, g2d->regs + FIMG2D_SOFT_RESET_REG);

	/* turn off wince option */
	cfg = 0x0;
	writel(cfg, g2d->regs + FIMG2D_BLEND_FUNCTION_REG);

	/* set default repeat mode to reflect(mirror)*/
	cfg = FIMG2D_SRC_REPEAT_REFLECT;
	writel(cfg, g2d->regs + FIMG2D_SRC_REPEAT_MODE_REG);
	cfg = FIMG2D_MSK_REPEAT_REFLECT;
	writel(cfg, g2d->regs + FIMG2D_MSK_REPEAT_MODE_REG);
}

void g2d_hwset_cci_on(struct g2d_dev *g2d)
{
	unsigned long cfg;

	cfg = readl(g2d->regs + FIMG2D_AXI_MODE_REG);
	cfg |= (0xf << FIMG2D_AXI_AWCACHE_SHIFT) |
		(0xf << FIMG2D_AXI_ARCACHE_SHIFT);
	writel(cfg, g2d->regs + FIMG2D_AXI_MODE_REG);

	/* printk("[%s:%d] done g2d cci setting!!!\n", __func__, __LINE__); */
}

/**
 * Four channels of the image are computed with:
 *	R = [ coeff(S)*Sc  + coeff(D)*Dc ]
 *	where
 *	Rc is result color or alpha
 *	Sc is source color or alpha
 *	Dc is destination color or alpha
 *
 * Caution: supposed that Sc and Dc are perpixel-alpha-premultiplied value
 *
 * MODE:             Formula
 * ----------------------------------------------------------------------------
 * FILL:
 * CLEAR:	     R = 0
 * SRC:		     R = Sc
 * DST:		     R = Dc
 * SRC_OVER:         R = Sc + (1-Sa)*Dc
 * DST_OVER:         R = (1-Da)*Sc + Dc
 * SRC_IN:	     R = Da*Sc
 * DST_IN:           R = Sa*Dc
 * SRC_OUT:          R = (1-Da)*Sc
 * DST_OUT:          R = (1-Sa)*Dc
 * SRC_ATOP:         R = Da*Sc + (1-Sa)*Dc
 * DST_ATOP:         R = (1-Da)*Sc + Sa*Dc
 * XOR:              R = (1-Da)*Sc + (1-Sa)*Dc
 * ADD:              R = Sc + Dc
 * MULTIPLY:         R = Sc*Dc
 * SCREEN:           R = Sc + (1-Sc)*Dc
 * DARKEN:           R = (Da*Sc<Sa*Dc)? Sc+(1-Sa)*Dc : (1-Da)*Sc+Dc
 * LIGHTEN:          R = (Da*Sc>Sa*Dc)? Sc+(1-Sa)*Dc : (1-Da)*Sc+Dc
 * DISJ_SRC_OVER:    R = Sc + (min(1,(1-Sa)/Da))*Dc
 * DISJ_DST_OVER:    R = (min(1,(1-Da)/Sa))*Sc + Dc
 * DISJ_SRC_IN:      R = (max(1-(1-Da)/Sa,0))*Sc
 * DISJ_DST_IN:      R = (max(1-(1-Sa)/Da,0))*Dc
 * DISJ_SRC_OUT:     R = (min(1,(1-Da)/Sa))*Sc
 * DISJ_DST_OUT:     R = (min(1,(1-Sa)/Da))*Dc
 * DISJ_SRC_ATOP:    R = (max(1-(1-Da)/Sa,0))*Sc + (min(1,(1-Sa)/Da))*Dc
 * DISJ_DST_ATOP:    R = (min(1,(1-Da)/Sa))*Sc + (max(1-(1-Sa)/Da,0))*Dc
 * DISJ_XOR:         R = (min(1,(1-Da)/Sa))*Sc + (min(1,(1-Sa)/Da))*Dc
 * CONJ_SRC_OVER:    R = Sc + (max(1-Sa/Da,0))*Dc
 * CONJ_DST_OVER:    R = (max(1-Da/Sa,0))*Sc + Dc
 * CONJ_SRC_IN:      R = (min(1,Da/Sa))*Sc
 * CONJ_DST_IN:      R = (min(1,Sa/Da))*Dc
 * CONJ_SRC_OUT:     R = (max(1-Da/Sa,0)*Sc
 * CONJ_DST_OUT:     R = (max(1-Sa/Da,0))*Dc
 * CONJ_SRC_ATOP:    R = (min(1,Da/Sa))*Sc + (max(1-Sa/Da,0))*Dc
 * CONJ_DST_ATOP:    R = (max(1-Da/Sa,0))*Sc + (min(1,Sa/Da))*Dc
 * CONJ_XOR:         R = (max(1-Da/Sa,0))*Sc + (max(1-Sa/Da,0))*Dc
 */
static struct fimg2d_blend_coeff const coeff_table[MAX_FIMG2D_BLIT_OP] = {
	{ 0, 0, 0, 0 },		/* FILL */
	{ 0, COEFF_ZERO,	0, COEFF_ZERO },	/* CLEAR */
	{ 0, COEFF_ONE,		0, COEFF_ZERO },	/* SRC */
	{ 0, COEFF_ZERO,	0, COEFF_ONE },		/* DST */
	{ 0, COEFF_ONE,		1, COEFF_SA },		/* SRC_OVER */
	{ 1, COEFF_DA,		0, COEFF_ONE },		/* DST_OVER */
	{ 0, COEFF_DA,		0, COEFF_ZERO },	/* SRC_IN */
	{ 0, COEFF_ZERO,	0, COEFF_SA },		/* DST_IN */
	{ 1, COEFF_DA,		0, COEFF_ZERO },	/* SRC_OUT */
	{ 0, COEFF_ZERO,	1, COEFF_SA },		/* DST_OUT */
	{ 0, COEFF_DA,		1, COEFF_SA },		/* SRC_ATOP */
	{ 1, COEFF_DA,		0, COEFF_SA },		/* DST_ATOP */
	{ 1, COEFF_DA,		1, COEFF_SA },		/* XOR */
	{ 0, COEFF_ONE,		0, COEFF_ONE },		/* ADD */
	{ 0, COEFF_DC,		0, COEFF_ZERO },	/* MULTIPLY */
	{ 0, COEFF_ONE,		1, COEFF_SC },		/* SCREEN */
	{ 0, 0, 0, 0 },		/* DARKEN */
	{ 0, 0, 0, 0 },		/* LIGHTEN */
	{ 0, COEFF_ONE,		0, COEFF_DISJ_S },	/* DISJ_SRC_OVER */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },		/* DISJ_DST_OVER */
	{ 1, COEFF_DISJ_D,	0, COEFF_ZERO },	/* DISJ_SRC_IN */
	{ 0, COEFF_ZERO,	1, COEFF_DISJ_S },	/* DISJ_DST_IN */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },		/* DISJ_SRC_OUT */
	{ 0, COEFF_ZERO,	0, COEFF_DISJ_S },	/* DISJ_DST_OUT */
	{ 1, COEFF_DISJ_D,	0, COEFF_DISJ_S },	/* DISJ_SRC_ATOP */
	{ 0, COEFF_DISJ_D,	1, COEFF_DISJ_S },	/* DISJ_DST_ATOP */
	{ 0, COEFF_DISJ_D,	0, COEFF_DISJ_S },	/* DISJ_XOR */
	{ 0, COEFF_ONE,		1, COEFF_DISJ_S },	/* CONJ_SRC_OVER */
	{ 1, COEFF_DISJ_D,	0, COEFF_ONE },		/* CONJ_DST_OVER */
	{ 0, COEFF_CONJ_D,	0, COEFF_ONE },		/* CONJ_SRC_IN */
	{ 0, COEFF_ZERO,	0, COEFF_CONJ_S },	/* CONJ_DST_IN */
	{ 1, COEFF_CONJ_D,	0, COEFF_ZERO },	/* CONJ_SRC_OUT */
	{ 0, COEFF_ZERO,	1, COEFF_CONJ_S },	/* CONJ_DST_OUT */
	{ 0, COEFF_CONJ_D,	1, COEFF_CONJ_S },	/* CONJ_SRC_ATOP */
	{ 1, COEFF_CONJ_D,	0, COEFF_CONJ_D },	/* CONJ_DST_ATOP */
	{ 1, COEFF_CONJ_D,	1, COEFF_CONJ_S },	/* CONJ_XOR */
	{ 0, 0, 0, 0 },		/* USER */
	{ 1, COEFF_GA,		1, COEFF_ZERO },	/* USER_SRC_GA */
};

/*
 * coefficient table with global (constant) alpha
 * replace COEFF_ONE with COEFF_GA
 *
 * MODE:             Formula with Global Alpha (Ga is multiplied to both Sc and Sa)
 * ----------------------------------------------------------------------------
 * FILL:
 * CLEAR:	     R = 0
 * SRC:		     R = Ga*Sc
 * DST:		     R = Dc
 * SRC_OVER:         R = Ga*Sc + (1-Sa*Ga)*Dc
 * DST_OVER:         R = (1-Da)*Ga*Sc + Dc --> (W/A) 1st:Ga*Sc, 2nd:DST_OVER
 * SRC_IN:	     R = Da*Ga*Sc
 * DST_IN:           R = Sa*Ga*Dc
 * SRC_OUT:          R = (1-Da)*Ga*Sc --> (W/A) 1st: Ga*Sc, 2nd:SRC_OUT
 * DST_OUT:          R = (1-Sa*Ga)*Dc
 * SRC_ATOP:         R = Da*Ga*Sc + (1-Sa*Ga)*Dc
 * DST_ATOP:         R = (1-Da)*Ga*Sc + Sa*Ga*Dc --> (W/A) 1st: Ga*Sc, 2nd:DST_ATOP
 * XOR:              R = (1-Da)*Ga*Sc + (1-Sa*Ga)*Dc --> (W/A) 1st: Ga*Sc, 2nd:XOR
 * ADD:              R = Ga*Sc + Dc
 * MULTIPLY:         R = Ga*Sc*Dc --> (W/A) 1st: Ga*Sc, 2nd: MULTIPLY
 * SCREEN:           R = Ga*Sc + (1-Ga*Sc)*Dc --> (W/A) 1st: Ga*Sc, 2nd: SCREEN
 * DARKEN:           R = (W/A) 1st: Ga*Sc, 2nd: OP
 * LIGHTEN:          R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_XOR:         R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_XOR:         R = (W/A) 1st: Ga*Sc, 2nd: OP
 */
static struct fimg2d_blend_coeff const ga_coeff_table[MAX_FIMG2D_BLIT_OP] = {
	{ 0, 0, 0, 0 },		/* FILL */
	{ 0, COEFF_ZERO,	0, COEFF_ZERO },	/* CLEAR */
	{ 0, COEFF_GA,		0, COEFF_ZERO },	/* SRC */
	{ 0, COEFF_ZERO,	0, COEFF_ONE },		/* DST */
	{ 0, COEFF_GA,		1, COEFF_SA },		/* SRC_OVER */
	{ 1, COEFF_DA,		0, COEFF_ONE },		/* DST_OVER (use W/A) */
	{ 0, COEFF_DA,		0, COEFF_ZERO },	/* SRC_IN */
	{ 0, COEFF_ZERO,	0, COEFF_SA },		/* DST_IN */
	{ 1, COEFF_DA,		0, COEFF_ZERO },	/* SRC_OUT (use W/A) */
	{ 0, COEFF_ZERO,	1, COEFF_SA },		/* DST_OUT */
	{ 0, COEFF_DA,		1, COEFF_SA },		/* SRC_ATOP */
	{ 1, COEFF_DA,		0, COEFF_SA },		/* DST_ATOP (use W/A) */
	{ 1, COEFF_DA,		1, COEFF_SA },		/* XOR (use W/A) */
	{ 0, COEFF_GA,		0, COEFF_ONE },		/* ADD */
	{ 0, COEFF_DC,		0, COEFF_ZERO },	/* MULTIPLY (use W/A) */
	{ 0, COEFF_ONE,		1, COEFF_SC },		/* SCREEN (use W/A) */
	{ 0, 0, 0, 0 },		/* DARKEN (use W/A) */
	{ 0, 0, 0, 0 },		/* LIGHTEN (use W/A) */
	{ 0, COEFF_ONE,		0, COEFF_DISJ_S },	/* DISJ_SRC_OVER (use W/A) */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },		/* DISJ_DST_OVER (use W/A) */
	{ 1, COEFF_DISJ_D,	0, COEFF_ZERO },	/* DISJ_SRC_IN (use W/A) */
	{ 0, COEFF_ZERO,	1, COEFF_DISJ_S },	/* DISJ_DST_IN (use W/A) */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },		/* DISJ_SRC_OUT (use W/A) */
	{ 0, COEFF_ZERO,	0, COEFF_DISJ_S },	/* DISJ_DST_OUT (use W/A) */
	{ 1, COEFF_DISJ_D,	0, COEFF_DISJ_S },	/* DISJ_SRC_ATOP (use W/A) */
	{ 0, COEFF_DISJ_D,	1, COEFF_DISJ_S },	/* DISJ_DST_ATOP (use W/A) */
	{ 0, COEFF_DISJ_D,	0, COEFF_DISJ_S },	/* DISJ_XOR (use W/A) */
	{ 0, COEFF_ONE,		1, COEFF_DISJ_S },	/* CONJ_SRC_OVER (use W/A) */
	{ 1, COEFF_DISJ_D,	0, COEFF_ONE },		/* CONJ_DST_OVER (use W/A) */
	{ 0, COEFF_CONJ_D,	0, COEFF_ONE },		/* CONJ_SRC_IN (use W/A) */
	{ 0, COEFF_ZERO,	0, COEFF_CONJ_S },	/* CONJ_DST_IN (use W/A) */
	{ 1, COEFF_CONJ_D,	0, COEFF_ZERO },	/* CONJ_SRC_OUT (use W/A) */
	{ 0, COEFF_ZERO,	1, COEFF_CONJ_S },	/* CONJ_DST_OUT (use W/A) */
	{ 0, COEFF_CONJ_D,	1, COEFF_CONJ_S },	/* CONJ_SRC_ATOP (use W/A) */
	{ 1, COEFF_CONJ_D,	0, COEFF_CONJ_D },	/* CONJ_DST_ATOP (use W/A) */
	{ 1, COEFF_CONJ_D,	1, COEFF_CONJ_S },	/* CONJ_XOR (use W/A) */
	{ 0, 0, 0, 0 },		/* USER */
	{ 1, COEFF_GA,		1, COEFF_ZERO },	/* USER_SRC_GA */
};

/* fimg2d4x_set_alpha_composite() */
void g2d_hwset_alpha_composite(struct g2d_dev *g2d,
		enum blit_op op, unsigned char g_alpha)
{
	int alphamode;
	unsigned long cfg = 0;
	struct fimg2d_blend_coeff const *tbl;

	switch (op) {
	case BLIT_OP_SOLID_FILL:
	case BLIT_OP_CLR:
		/* nop */
		return;
	case BLIT_OP_DARKEN:
		cfg |= FIMG2D_DARKEN;
		break;
	case BLIT_OP_LIGHTEN:
		cfg |= FIMG2D_LIGHTEN;
		break;
	case BLIT_OP_USER_COEFF:
		/* TODO */
		return;
	default:
		if (g_alpha < 0xff) {	/* with global alpha */
			tbl = &ga_coeff_table[op];
			alphamode = ALPHA_PERPIXEL_MUL_GLOBAL;
		} else {
			tbl = &coeff_table[op];
			alphamode = ALPHA_PERPIXEL;
		}

		/* src coefficient */
		cfg |= tbl->s_coeff << FIMG2D_SRC_COEFF_SHIFT;

		cfg |= alphamode << FIMG2D_SRC_COEFF_SA_SHIFT;
		cfg |= alphamode << FIMG2D_SRC_COEFF_DA_SHIFT;

		if (tbl->s_coeff_inv)
			cfg |= FIMG2D_INV_SRC_COEFF;

		/* dst coefficient */
		cfg |= tbl->d_coeff << FIMG2D_DST_COEFF_SHIFT;

		cfg |= alphamode << FIMG2D_DST_COEFF_DA_SHIFT;
		cfg |= alphamode << FIMG2D_DST_COEFF_SA_SHIFT;

		if (tbl->d_coeff_inv)
			cfg |= FIMG2D_INV_DST_COEFF;

		break;
	}

	//printk("[%s] s_coeff_inv:%d, s_coeff:%d. d_coeff_inv:%d, d_coeff:%d, g_alpha:%d\n", __func__,
	//		tbl->s_coeff_inv, tbl->s_coeff, tbl->d_coeff_inv, tbl->d_coeff, g_alpha);
	g2d_dbg("cfg = 0x%lx in FIMG2D_BLEND_FUMCTION_REG\n", cfg);

	writel(cfg, g2d->regs + FIMG2D_BLEND_FUNCTION_REG);

	/* round mode: depremult round mode is not used */
	cfg = readl(g2d->regs + FIMG2D_ROUND_MODE_REG);

	/* premult */
	cfg &= ~FIMG2D_PREMULT_ROUND_MASK;
	cfg |= premult_round_mode << FIMG2D_PREMULT_ROUND_SHIFT;

	/* blend */
	cfg &= ~FIMG2D_BLEND_ROUND_MASK;
	cfg |= blend_round_mode << FIMG2D_BLEND_ROUND_SHIFT;

	writel(cfg, g2d->regs + FIMG2D_ROUND_MODE_REG);
}

void g2d_hwset_dump_regs(struct g2d_dev *g2d)
{
	int i, offset;
	unsigned long table[][2] = {
		/* start, end */
		{0x0000, 0x0030},	/* general */
		{0x0080, 0x00a0},	/* host dma */
		{0x0100, 0x0110},	/* commands */
		{0x0200, 0x0210},	/* rotation & direction */
		{0x0300, 0x0340},	/* source */
		{0x0400, 0x0420},	/* dest */
		{0x0500, 0x0550},	/* pattern & mask */
		{0x0600, 0x0710},	/* clip, rop, alpha and color */
		{0x0, 0x0}
	};

	printk("[%s] print sfr!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", __func__);
	for (i = 0; table[i][1] != 0x0; i++) {
		offset = table[i][0];
		do {
			printk(KERN_INFO "[0x%04x] 0x%08x 0x%08x 0x%08x 0x%08x\n", offset,
				readl(g2d->regs + offset),
				readl(g2d->regs + offset + 0x4),
				readl(g2d->regs + offset + 0x8),
				readl(g2d->regs + offset + 0xC));
			offset += 0x10;
		} while (offset < table[i][1]);
	}
}
