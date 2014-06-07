/*
 * drivers/media/video/exynos/hevc/hevc_opr.c
 *
 * Samsung HEVC driver
 * This file contains hw related functions.
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 e This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/jiffies.h>

#include <linux/firmware.h>
#include <linux/err.h>
#include <linux/sched.h>

#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <config/exynos/iovmm.h>
#endif

#include "hevc_common.h"

#include "hevc_cmd.h"
#include "hevc_mem.h"
#include "hevc_intr.h"
#include "hevc_inst.h"
#include "hevc_pm.h"
#include "hevc_debug.h"
#include "hevc_dec.h"
#include "hevc_opr.h"
#include "regs-hevc.h"
#include "hevc_reg.h"

/* #define HEVC_DEBUG_REGWRITE  */
#ifdef HEVC_DEBUG_REGWRITE
#undef writel
#define writel(v, r)								\
	do {									\
		printk(KERN_ERR "HEVCWRITE(%p): %08x\n", r, (unsigned int)v);	\
	__raw_writel(v, r);							\
	} while (0)
#endif /* HEVC_DEBUG_REGWRITE */

#define READL(offset)		readl(dev->regs_base + (offset))
#define WRITEL(data, offset)	writel((data), dev->regs_base + (offset))
#define OFFSETA(x)		(((x) - dev->port_a) >> HEVC_MEM_OFFSET)
#define OFFSETB(x)		(((x) - dev->port_b) >> HEVC_MEM_OFFSET)

/* Allocate codec buffers */
int hevc_alloc_codec_buffers(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	unsigned int mb_width, mb_height;
	void *alloc_ctx;
	int i;
	int lcu_width, lcu_height;

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
	alloc_ctx = dev->alloc_ctx[HEVC_BANK_A_ALLOC_CTX];

	mb_width = mb_width(ctx->img_width);
	mb_height = mb_height(ctx->img_height);

	if (ctx->type == HEVCINST_DECODER) {
		for (i = 0; i < ctx->raw_buf.num_planes; i++)
			hevc_debug(2, "Plane[%d] size:%d\n",
					i, ctx->raw_buf.plane_size[i]);
		hevc_debug(2, "MV size: %d, Totals bufs: %d\n",
				ctx->mv_size, dec->total_dpb_count);
	} else {
		return -EINVAL;
	}

	dec->mv_count = dec->total_dpb_count;

	hevc_info("ctx->lcu_size : %d\n", ctx->lcu_size);
	lcu_width = ALIGN(ctx->img_width, ctx->lcu_size);
	lcu_height = ALIGN(ctx->img_height, ctx->lcu_size);

	ctx->scratch_buf_size =
		DEC_HEVC_SCRATCH_SIZE(lcu_width , lcu_height);

	ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
	ctx->port_a_size =
		ctx->scratch_buf_size +
		(dec->mv_count * ctx->mv_size);

	if (ctx->is_drm)
		alloc_ctx = dev->alloc_ctx_drm;

	/* Allocate only if memory from bank 1 is necessary */
	if (ctx->port_a_size > 0) {

		ctx->port_a_buf = hevc_mem_alloc_priv(
				alloc_ctx, ctx->port_a_size);
		if (IS_ERR(ctx->port_a_buf)) {
			ctx->port_a_buf = 0;
			printk(KERN_ERR
			       "Buf alloc for decoding failed (port A).\n");
			return -ENOMEM;
		}
		ctx->port_a_phys = hevc_mem_daddr_priv(ctx->port_a_buf);

	}

	hevc_debug_leave();

	return 0;
}

/* Release buffers allocated for codec */
void hevc_release_codec_buffers(struct hevc_ctx *ctx)
{
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}
	if (ctx->port_a_buf) {
		hevc_mem_free_priv(ctx->port_a_buf);
		ctx->port_a_buf = 0;
		ctx->port_a_phys = 0;
		ctx->port_a_size = 0;
	}
}

/* Allocate memory for instance data buffer */
int hevc_alloc_instance_buffer(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_buf_size_v6 *buf_size;
	void *alloc_ctx;

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
	buf_size = dev->variant->buf_size->buf;
	alloc_ctx = dev->alloc_ctx[HEVC_BANK_A_ALLOC_CTX];

	ctx->ctx_buf_size = buf_size->dec_ctx;

	if (ctx->is_drm)
		alloc_ctx = dev->alloc_ctx_drm;

	ctx->ctx.alloc = hevc_mem_alloc_priv(alloc_ctx, ctx->ctx_buf_size);
	if (IS_ERR(ctx->ctx.alloc)) {
		hevc_err("Allocating context buffer failed.\n");
		return PTR_ERR(ctx->ctx.alloc);
	}

	ctx->ctx.ofs = hevc_mem_daddr_priv(ctx->ctx.alloc);
	ctx->ctx.virt = hevc_mem_vaddr_priv(ctx->ctx.alloc);
	if (!ctx->ctx.virt) {
		hevc_mem_free_priv(ctx->ctx.alloc);
		ctx->ctx.alloc = NULL;
		ctx->ctx.ofs = 0;
		ctx->ctx.virt = NULL;

		hevc_err("Remapping context buffer failed.\n");
		return -ENOMEM;
	}

	hevc_debug_leave();

	return 0;
}

/* Release instance buffer */
void hevc_release_instance_buffer(struct hevc_ctx *ctx)
{
	hevc_debug_enter();
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	if (ctx->ctx.alloc) {
		hevc_mem_free_priv(ctx->ctx.alloc);
		ctx->ctx.alloc = NULL;
		ctx->ctx.ofs = 0;
		ctx->ctx.virt = NULL;
	}

	hevc_debug_leave();
}

/* Allocate context buffers for SYS_INIT */
int hevc_alloc_dev_context_buffer(struct hevc_dev *dev)
{
	struct hevc_buf_size_v6 *buf_size;
	void *alloc_ctx;

	hevc_debug_enter();
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	buf_size = dev->variant->buf_size->buf;
	alloc_ctx = dev->alloc_ctx[HEVC_BANK_A_ALLOC_CTX];

	hevc_info("dev context buf_size : %d\n", buf_size->dev_ctx);

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->num_drm_inst)
		alloc_ctx = dev->alloc_ctx_drm;
#endif
	dev->ctx_buf.alloc =
			hevc_mem_alloc_priv(alloc_ctx, buf_size->dev_ctx);
	if (IS_ERR(dev->ctx_buf.alloc)) {
		hevc_err("Allocating DESC buffer failed.\n");
		return PTR_ERR(dev->ctx_buf.alloc);
	}

	dev->ctx_buf.ofs = hevc_mem_daddr_priv(dev->ctx_buf.alloc);
	dev->ctx_buf.virt = hevc_mem_vaddr_priv(dev->ctx_buf.alloc);
	if (!dev->ctx_buf.virt) {
		hevc_mem_free_priv(dev->ctx_buf.alloc);
		dev->ctx_buf.alloc = NULL;
		dev->ctx_buf.ofs = 0;

		hevc_err("Remapping DESC buffer failed.\n");
		return -ENOMEM;
	}

	hevc_debug_leave();

	return 0;
}

/* Release context buffers for SYS_INIT */
void hevc_release_dev_context_buffer(struct hevc_dev *dev)
{
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}
	if (dev->ctx_buf.alloc) {
		hevc_mem_free_priv(dev->ctx_buf.alloc);
		dev->ctx_buf.alloc = NULL;
		dev->ctx_buf.ofs = 0;
		dev->ctx_buf.virt = NULL;
	}

}

static int calc_plane(int width, int height, int is_tiled)
{
	int mbX, mbY;

	mbX = (width + 15)/16;
	mbY = (height + 15)/16;

	/* Alignment for interlaced processing */
	if (is_tiled)
		mbY = (mbY + 1) / 2 * 2;

	return (mbX * 16) * (mbY * 16);
}

static void set_linear_stride_size(struct hevc_ctx *ctx, struct hevc_fmt *fmt)
{
	struct hevc_raw_info *raw;
	int i;

	raw = &ctx->raw_buf;

	switch (fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		ctx->raw_buf.stride[0] = ctx->img_width;
		ctx->raw_buf.stride[1] = ctx->img_width >> 1;
		ctx->raw_buf.stride[2] = ctx->img_width >> 1;
		break;
	case V4L2_PIX_FMT_NV12MT_16X16:
	case V4L2_PIX_FMT_NV12MT:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		raw->stride[0] = ctx->img_width;
		raw->stride[1] = ctx->img_width;
		raw->stride[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB24:
		ctx->raw_buf.stride[0] = ctx->img_width * 3;
		ctx->raw_buf.stride[1] = 0;
		ctx->raw_buf.stride[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB565:
		ctx->raw_buf.stride[0] = ctx->img_width * 2;
		ctx->raw_buf.stride[1] = 0;
		ctx->raw_buf.stride[2] = 0;
		break;
	case V4L2_PIX_FMT_RGB32X:
	case V4L2_PIX_FMT_BGR32:
		ctx->raw_buf.stride[0] = ctx->img_width * 4;
		ctx->raw_buf.stride[1] = 0;
		ctx->raw_buf.stride[2] = 0;
		break;
	default:
		break;
	}

	/* Decoder needs multiple of 16 alignment for stride */
	if (ctx->type == HEVCINST_DECODER) {
		for (i = 0; i < 3; i++)
			ctx->raw_buf.stride[i] =
				ALIGN(ctx->raw_buf.stride[i], 16);
	}
}

void hevc_dec_calc_dpb_size(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	struct hevc_raw_info *raw;
	int i;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}

	dev = ctx->dev;
	raw = &ctx->raw_buf;

	dec = ctx->dec_priv;

	ctx->buf_width = ALIGN(ctx->img_width, 16);
	ctx->buf_height = ALIGN(ctx->img_height, 16);

	hevc_info("SEQ Done: Movie dimensions %dx%d, "
			"buffer dimensions: %dx%d\n", ctx->img_width,
			ctx->img_height, ctx->buf_width, ctx->buf_height);

	switch (ctx->dst_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		raw->plane_size[0] = ctx->buf_width*ctx->img_height;
		raw->plane_size[1] = ctx->buf_width*(ctx->img_height >> 1);
		raw->plane_size[2] = 0;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		raw->plane_size[0] =
			calc_plane(ctx->img_width, ctx->img_height, 0);
		raw->plane_size[1] =
			calc_plane(ctx->img_width >> 1, ctx->img_height >> 1, 0);
		raw->plane_size[2] =
			calc_plane(ctx->img_width >> 1, ctx->img_height >> 1, 0);
		break;
	default:
		raw->plane_size[0] = 0;
		raw->plane_size[1] = 0;
		raw->plane_size[2] = 0;
		hevc_err("Invalid pixelformat : %s\n", ctx->dst_fmt->name);
		break;
	}

	set_linear_stride_size(ctx, ctx->dst_fmt);

	for (i = 0; i < raw->num_planes; i++)
		hevc_debug(2, "Plane[%d] size = %d, stride = %d\n",
			i, raw->plane_size[i], raw->stride[i]);

	switch (ctx->dst_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		raw->plane_size[0] += 64;
		raw->plane_size[1] += 64;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		raw->plane_size[0] += 64;
		raw->plane_size[1] += 64;
		raw->plane_size[2] += 64;
		break;
	default:
		break;
	}

	hevc_info("codec mode : %d\n", ctx->codec_mode);

	if (ctx->codec_mode == EXYNOS_CODEC_HEVC_DEC) {
		ctx->mv_size = hevc_dec_mv_size(ctx->img_width,
				ctx->img_height);
		ctx->mv_size = ALIGN(ctx->mv_size, 32);
	} else {
		ctx->mv_size = 0;
	}
}

/* Set registers for decoding stream buffer */
int hevc_set_dec_stream_buffer(struct hevc_ctx *ctx, dma_addr_t buf_addr,
		  unsigned int start_num_byte, unsigned int strm_size)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	size_t cpb_buf_size;

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
		hevc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	cpb_buf_size = ALIGN(dec->src_buf_size, HEVC_NV12M_HALIGN);

	hevc_debug(2, "inst_no: %d, buf_addr: 0x%x\n", ctx->inst_no, buf_addr);
	hevc_debug(2, "strm_size: 0x%08x cpb_buf_size: 0x%x\n",
			 strm_size, cpb_buf_size);

	WRITEL(strm_size, HEVC_D_STREAM_DATA_SIZE);
	WRITEL(buf_addr, HEVC_D_CPB_BUFFER_ADDR);
	WRITEL(cpb_buf_size, HEVC_D_CPB_BUFFER_SIZE);
	WRITEL(start_num_byte, HEVC_D_CPB_BUFFER_OFFSET);

	hevc_debug_leave();
	return 0;
}

/* Set display buffer through shared memory at INIT_BUFFER */
int hevc_set_dec_stride_buffer(struct hevc_ctx *ctx, struct list_head *buf_queue)
{
	struct hevc_dev *dev = ctx->dev;
	int i;

	for (i = 0; i < ctx->raw_buf.num_planes; i++) {
		WRITEL(ctx->raw_buf.stride[i],
			HEVC_D_FIRST_PLANE_DPB_STRIDE_SIZE + (i * 4));
		hevc_debug(2, "# plane%d.size = %d, stride = %d\n", i,
			ctx->raw_buf.plane_size[i], ctx->raw_buf.stride[i]);
	}

	return 0;
}

/* Set decoding frame buffer */
int hevc_set_dec_frame_buffer(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	unsigned int i, frame_size_mv;
	size_t buf_addr1;
	int buf_size1;
	int align_gap;
	struct hevc_buf *buf;
	struct hevc_raw_info *raw;
	struct list_head *buf_queue;
	unsigned char *dpb_vir;
	int j;

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
	buf_addr1 = ctx->port_a_phys;
	buf_size1 = ctx->port_a_size;

	hevc_debug(2, "Buf1: %p (%d)\n", (void *)buf_addr1, buf_size1);
	hevc_info("Total DPB COUNT: %d\n", dec->total_dpb_count);
	hevc_debug(2, "Setting display delay to %d\n", dec->display_delay);
	hevc_debug(2, "ctx->scratch_buf_size %d\n", ctx->scratch_buf_size);

	WRITEL(dec->total_dpb_count, HEVC_D_NUM_DPB);

	hevc_debug(2, "raw->num_planes %d\n", raw->num_planes);
	for (i = 0; i < raw->num_planes; i++) {
		hevc_debug(2, "raw->plane_size[%d]= %d\n", i, raw->plane_size[i]);
		WRITEL(raw->plane_size[i], HEVC_D_FIRST_PLANE_DPB_SIZE + i*4);
	}

	if (dec->is_dynamic_dpb)
		WRITEL((0x1 << HEVC_D_OPT_DYNAMIC_DPB_SET_SHIFT), HEVC_D_INIT_BUFFER_OPTIONS);

	WRITEL(buf_addr1, HEVC_D_SCRATCH_BUFFER_ADDR);
	WRITEL(ctx->scratch_buf_size, HEVC_D_SCRATCH_BUFFER_SIZE);
	buf_addr1 += ctx->scratch_buf_size;
	buf_size1 -= ctx->scratch_buf_size;

	WRITEL(ctx->mv_size, HEVC_D_MV_BUFFER_SIZE);

	frame_size_mv = ctx->mv_size;
	hevc_debug(2, "Frame size: %d, %d, %d, mv: %d\n",
			raw->plane_size[0], raw->plane_size[1],
			raw->plane_size[2], frame_size_mv);

	if (dec->dst_memtype == V4L2_MEMORY_USERPTR || dec->dst_memtype == V4L2_MEMORY_DMABUF)
		buf_queue = &ctx->dst_queue;
	else
		buf_queue = &dec->dpb_queue;

	i = 0;
	list_for_each_entry(buf, buf_queue, list) {
		/* Do not setting DPB */
		if (dec->is_dynamic_dpb)
			break;
		for (j = 0; j < raw->num_planes; j++) {
			hevc_debug(2, "buf->planes.raw[%d] = 0x%x\n", j, buf->planes.raw[j]);
			WRITEL(buf->planes.raw[j], HEVC_D_FIRST_PLANE_DPB0 + (j*0x100 + i*4));
		}

		if ((i == 0) && (!ctx->is_drm)) {
			int j, color[3] = { 0x0, 0x80, 0x80 };
			for (j = 0; j < raw->num_planes; j++) {
				dpb_vir = vb2_plane_vaddr(&buf->vb, j);
				if (dpb_vir)
					memset(dpb_vir, color[j],
							raw->plane_size[j]);
			}
			hevc_mem_clean_vb(&buf->vb, j);
		}
		i++;
	}

	hevc_set_dec_stride_buffer(ctx, buf_queue);

	WRITEL(dec->mv_count, HEVC_D_NUM_MV);

	for (i = 0; i < dec->mv_count; i++) {
		/* To test alignment */
		align_gap = buf_addr1;
		buf_addr1 = ALIGN(buf_addr1, 16);
		align_gap = buf_addr1 - align_gap;
		buf_size1 -= align_gap;

		hevc_debug(2, "\tBuf1: %x, size: %d\n", buf_addr1, buf_size1);
		WRITEL(buf_addr1, HEVC_D_MV_BUFFER0 + i * 4);
		buf_addr1 += frame_size_mv;
		buf_size1 -= frame_size_mv;
	}

	hevc_debug(2, "Buf1: %u, buf_size1: %d (frames %d)\n",
			buf_addr1, buf_size1, dec->total_dpb_count);
	if (buf_size1 < 0) {
		hevc_debug(2, "Not enough memory has been allocated.\n");
		return -ENOMEM;
	}

	WRITEL(ctx->inst_no, HEVC_INSTANCE_ID);
	hevc_cmd_host2risc(HEVC_CH_INIT_BUFS, NULL);

	hevc_debug(2, "After setting buffers.\n");

	return 0;
}

/* Initialize decoding */
int hevc_init_decode(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	unsigned int reg = 0, pix_val;
	int fmo_aso_ctrl = 0;

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
	hevc_debug(2, "InstNo: %d/%d\n", ctx->inst_no, HEVC_CH_SEQ_HEADER);
	hevc_debug(2, "BUFs: %08x %08x %08x\n",
		  READL(HEVC_D_CPB_BUFFER_ADDR),
		  READL(HEVC_D_CPB_BUFFER_ADDR),
		  READL(HEVC_D_CPB_BUFFER_ADDR));

	reg |= (dec->idr_decoding << HEVC_D_OPT_IDR_DECODING_SHFT);
	/* FMO_ASO_CTRL - 0: Enable, 1: Disable */
	reg |= (fmo_aso_ctrl << HEVC_D_OPT_FMO_ASO_CTRL_MASK);
	/* When user sets desplay_delay to 0,
	 * It works as "display_delay enable" and delay set to 0.
	 * If user wants display_delay disable, It should be
	 * set to negative value. */
	if (dec->display_delay >= 0) {
		reg |= (0x1 << HEVC_D_OPT_DDELAY_EN_SHIFT);
		WRITEL(dec->display_delay, HEVC_D_DISPLAY_DELAY);
	}

	if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16)
		reg |= (0x1 << HEVC_D_OPT_TILE_MODE_SHIFT);

	hevc_debug(2, "HEVC_D_DEC_OPTIONS : 0x%x\n", reg);

	WRITEL(0x20, HEVC_D_DEC_OPTIONS);

	switch (ctx->dst_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12MT_16X16:
		pix_val = 0;
		break;
	case V4L2_PIX_FMT_NV21M:
		pix_val = 1;
		break;
	case V4L2_PIX_FMT_YVU420M:
		pix_val = 2;
		break;
	case V4L2_PIX_FMT_YUV420M:
		pix_val = 3;
		break;
	default:
		pix_val = 0;
		break;
	}

	hevc_debug(2, "pixel format: %d\n", pix_val);
	WRITEL(pix_val, HEVC_PIXEL_FORMAT);

	/* sei parse */
	reg = dec->sei_parse;
	hevc_debug(2, "sei parse: %d\n", dec->sei_parse);
	/* Enable realloc interface if SEI is enabled */
	if (dec->sei_parse)
		reg |= (0x1 << HEVC_D_SEI_NEED_INIT_BUFFER_SHIFT);
	WRITEL(reg, HEVC_D_SEI_ENABLE);

	WRITEL(ctx->inst_no, HEVC_INSTANCE_ID);
	WRITEL(0xffffffff, HEVC_D_AVAILABLE_DPB_FLAG_UPPER);
	WRITEL(0xffffffff, HEVC_D_AVAILABLE_DPB_FLAG_LOWER);
	hevc_cmd_host2risc(HEVC_CH_SEQ_HEADER, NULL);

	hevc_debug_leave();
	return 0;
}

/* Decode a single frame */
int hevc_decode_one_frame(struct hevc_ctx *ctx, int last_frame)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;

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
	hevc_debug(2, "Setting flags to %08lx (free:%d WTF:%d)\n",
				dec->dpb_status, ctx->dst_queue_cnt,
						dec->dpb_queue_cnt);
	if (dec->is_dynamic_dpb) {
		hevc_debug(2, "Dynamic:0x%08x, Available:0x%08lx\n",
					dec->dynamic_set, dec->dpb_status);
		WRITEL(dec->dynamic_set, HEVC_D_DYNAMIC_DPB_FLAG_LOWER);
		WRITEL(0x0, HEVC_D_DYNAMIC_DPB_FLAG_UPPER);
	}

	WRITEL(dec->dpb_status, HEVC_D_AVAILABLE_DPB_FLAG_LOWER);
	WRITEL(0x0, HEVC_D_AVAILABLE_DPB_FLAG_UPPER);
	WRITEL(dec->slice_enable, HEVC_D_SLICE_IF_ENABLE);

	hevc_debug(2, "dec->slice_enable : %d\n", dec->slice_enable);
	hevc_debug(2, "inst_no : %d last_frame : %d\n", ctx->inst_no, last_frame);
	WRITEL(ctx->inst_no, HEVC_INSTANCE_ID);
	/* Issue different commands to instance basing on whether it
	 * is the last frame or not. */

	switch (last_frame) {
	case 0:
		hevc_cmd_host2risc(HEVC_CH_FRAME_START, NULL);
		break;
	case 1:
		hevc_cmd_host2risc(HEVC_CH_LAST_FRAME, NULL);
		break;
	}

	hevc_debug(2, "Decoding a usual frame.\n");
	return 0;
}

static inline int hevc_get_new_ctx(struct hevc_dev *dev)
{
	int new_ctx;
	int cnt;

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	hevc_debug(2, "Previous context: %d (bits %08lx)\n", dev->curr_ctx,
							dev->ctx_work_bits);

	if (dev->preempt_ctx > HEVC_NO_INSTANCE_SET)
		return dev->preempt_ctx;
	else
		new_ctx = (dev->curr_ctx + 1) % HEVC_NUM_CONTEXTS;

	cnt = 0;
	while (!test_bit(new_ctx, &dev->ctx_work_bits)) {
		new_ctx = (new_ctx + 1) % HEVC_NUM_CONTEXTS;
		cnt++;
		if (cnt > HEVC_NUM_CONTEXTS) {
			/* No contexts to run */
			return -EAGAIN;
		}
	}

	return new_ctx;
}

static int hevc_set_dynamic_dpb(struct hevc_ctx *ctx, struct hevc_buf *dst_vb)
{
	struct hevc_dev *dev = ctx->dev;
	struct hevc_dec *dec = ctx->dec_priv;
	struct hevc_raw_info *raw = &ctx->raw_buf;
	int dst_index;
	int i;

	dst_index = dst_vb->vb.v4l2_buf.index;
	dec->dynamic_set = 1 << dst_index;
	dst_vb->used = 1;
	set_bit(dst_index, &dec->dpb_status);
	hevc_debug(2, "ADDING Flag after: %lx\n", dec->dpb_status);
	hevc_debug(2, "Dst addr [%d] = 0x%x\n", dst_index,
			dst_vb->planes.raw[0]);

	for (i = 0; i < raw->num_planes; i++) {
		WRITEL(raw->plane_size[i], HEVC_D_FIRST_PLANE_DPB_SIZE + i*4);
		WRITEL(dst_vb->planes.raw[i],
			HEVC_D_FIRST_PLANE_DPB0 + (i*0x100 + dst_index*4));
	}

	return 0;
}

static inline int hevc_run_dec_last_frames(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_buf *temp_vb, *dst_vb;
	struct hevc_dec *dec;

	unsigned long flags;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->irqlock, flags);

	if ((dec->is_dynamic_dpb) && (ctx->dst_queue_cnt == 0)) {
		hevc_debug(2, "No dst buffer\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	/* Frames are being decoded */
	if (list_empty(&ctx->src_queue)) {
		hevc_debug(2, "No src buffers.\n");
		hevc_set_dec_stream_buffer(ctx, 0, 0, 0);
	} else {
		/* Get the next source buffer */
		temp_vb = list_entry(ctx->src_queue.next,
					struct hevc_buf, list);
		temp_vb->used = 1;
		hevc_set_dec_stream_buffer(ctx,
			hevc_mem_plane_addr(ctx, &temp_vb->vb, 0), 0, 0);
	}

	if (dec->is_dynamic_dpb) {
		dst_vb = list_entry(ctx->dst_queue.next,
						struct hevc_buf, list);
		hevc_set_dynamic_dpb(ctx, dst_vb);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);
	hevc_decode_one_frame(ctx, 1);

	return 0;
}

static inline int hevc_run_dec_frame(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_buf *temp_vb, *dst_vb;
	struct hevc_dec *dec;
	unsigned long flags;
	int last_frame = 0;
	unsigned int index;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->irqlock, flags);

	/* Frames are being decoded */
	if (list_empty(&ctx->src_queue)) {
		hevc_debug(2, "No src buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}
	if ((dec->is_dynamic_dpb && ctx->dst_queue_cnt == 0) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt < ctx->dpb_count)) {
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	/* Get the next source buffer */
	temp_vb = list_entry(ctx->src_queue.next, struct hevc_buf, list);
	temp_vb->used = 1;
	hevc_debug(2, "Temp vb: %p\n", temp_vb);
	hevc_debug(2, "Src Addr: 0x%08lx\n",
		(unsigned long)hevc_mem_plane_addr(ctx, &temp_vb->vb, 0));
	hevc_set_dec_stream_buffer(ctx,
			hevc_mem_plane_addr(ctx, &temp_vb->vb, 0),
			0, temp_vb->vb.v4l2_planes[0].bytesused);

	index = temp_vb->vb.v4l2_buf.index;
	if (call_cop(ctx, set_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
		hevc_err("failed in set_buf_ctrls_val\n");

	if (dec->is_dynamic_dpb) {
		dst_vb = list_entry(ctx->dst_queue.next,
						struct hevc_buf, list);
		hevc_set_dynamic_dpb(ctx, dst_vb);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);

	if (temp_vb->vb.v4l2_planes[0].bytesused == 0 ||
		temp_vb->vb.v4l2_buf.reserved2 == FLAG_LAST_FRAME) {
		last_frame = 1;
		hevc_debug(2, "Setting ctx->state to FINISHING\n");
		ctx->state = HEVCINST_FINISHING;
	}
	hevc_decode_one_frame(ctx, last_frame);

	return 0;
}

static inline void hevc_run_init_dec(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	unsigned long flags;
	struct hevc_buf *temp_vb;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}
	/* Initializing decoding - parsing header */
	spin_lock_irqsave(&dev->irqlock, flags);
	hevc_info("Preparing to init decoding.\n");
	temp_vb = list_entry(ctx->src_queue.next, struct hevc_buf, list);
	hevc_info("Header size: %d\n", temp_vb->vb.v4l2_planes[0].bytesused);
	hevc_set_dec_stream_buffer(ctx,
			hevc_mem_plane_addr(ctx, &temp_vb->vb, 0),
			0, temp_vb->vb.v4l2_planes[0].bytesused);
	spin_unlock_irqrestore(&dev->irqlock, flags);
	dev->curr_ctx = ctx->num;
	hevc_debug(2, "Header addr: 0x%08lx\n",
		(unsigned long)hevc_mem_plane_addr(ctx, &temp_vb->vb, 0));
	hevc_clean_ctx_int_flags(ctx);
	hevc_init_decode(ctx);
}

static inline int hevc_run_init_dec_buffers(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev;
	struct hevc_dec *dec;
	int ret;

	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	dev = ctx->dev;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}
	/* Initializing decoding - parsing header */
	/* Header was parsed now starting processing
	 * First set the output frame buffers
	 * hevc_alloc_dec_buffers(ctx); */

	if (!dec->is_dynamic_dpb && (ctx->capture_state != QUEUE_BUFS_MMAPED)) {
		hevc_err("It seems that not all destionation buffers were "
			"mmaped.\nHEVC requires that all destination are mmaped "
			"before starting processing.\n");
		return -EAGAIN;
	}

	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);
	ret = hevc_set_dec_frame_buffer(ctx);
	if (ret) {
		hevc_err("Failed to alloc frame mem.\n");
		ctx->state = HEVCINST_ERROR;
	}
	return ret;
}

static inline int hevc_abort_inst(struct hevc_ctx *ctx)
{
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

	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);

	WRITEL(ctx->inst_no, HEVC_INSTANCE_ID);
	hevc_cmd_host2risc(HEVC_CH_NAL_ABORT, NULL);

	return 0;
}

static inline int hevc_dec_dpb_flush(struct hevc_ctx *ctx)
{
	struct hevc_dev *dev = ctx->dev;

	dev->curr_ctx = ctx->num;
	hevc_clean_ctx_int_flags(ctx);

	WRITEL(ctx->inst_no, HEVC_INSTANCE_ID);
	hevc_cmd_host2risc(HEVC_H2R_CMD_FLUSH, NULL);

	return 0;
}

static inline int hevc_ctx_ready(struct hevc_ctx *ctx)
{
	if (ctx->type == HEVCINST_DECODER)
		return hevc_dec_ctx_ready(ctx);

	return 0;
}

/* Try running an operation on hardware */
void hevc_try_run(struct hevc_dev *dev)
{
	struct hevc_ctx *ctx;
	int new_ctx;
	unsigned int ret = 0;

	hevc_debug(1, "Try run dev: %p\n", dev);
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}

	spin_lock_irq(&dev->condlock);
	/* Check whether hardware is not running */
	if (dev->hw_lock != 0) {
		spin_unlock_irq(&dev->condlock);
		/* This is perfectly ok, the scheduled ctx should wait */
		hevc_debug(1, "Couldn't lock HW.\n");
		return;
	}

	/* Choose the context to run */
	new_ctx = hevc_get_new_ctx(dev);
	if (new_ctx < 0) {
		/* No contexts to run */
		spin_unlock_irq(&dev->condlock);
		hevc_debug(1, "No ctx is scheduled to be run.\n");
		return;
	}

	ctx = dev->ctx[new_ctx];
	if (!ctx) {
		spin_unlock_irq(&dev->condlock);
		hevc_err("no hevc context to run\n");
		return;
	}

	if (test_and_set_bit(ctx->num, &dev->hw_lock) != 0) {
		spin_unlock_irq(&dev->condlock);
		hevc_err("Failed to lock hardware.\n");
		return;
	}
	spin_unlock_irq(&dev->condlock);

	hevc_debug(1, "New context: %d\n", new_ctx);
	hevc_debug(1, "Seting new context to %p\n", ctx);

	/* Got context to run in ctx */
	hevc_debug(1, "ctx->dst_queue_cnt=%d ctx->dpb_count=%d ctx->src_queue_cnt=%d\n",
		ctx->dst_queue_cnt, ctx->dpb_count, ctx->src_queue_cnt);
	hevc_debug(1, "ctx->state=%d\n", ctx->state);
	/* Last frame has already been sent to HEVC
	 * Now obtaining frames from HEVC buffer */

	dev->curr_ctx_drm = ctx->is_drm;

	hevc_clock_on();

	if (ctx->type == HEVCINST_DECODER) {
		switch (ctx->state) {
		case HEVCINST_FINISHING:
			ret = hevc_run_dec_last_frames(ctx);
			break;
		case HEVCINST_RUNNING:
			ret = hevc_run_dec_frame(ctx);
			break;
		case HEVCINST_INIT:
			ret = hevc_open_inst(ctx);
			break;
		case HEVCINST_RETURN_INST:
			ret = hevc_close_inst(ctx);
			break;
		case HEVCINST_GOT_INST:
			hevc_run_init_dec(ctx);
			break;
		case HEVCINST_HEAD_PARSED:
			ret = hevc_run_init_dec_buffers(ctx);
			break;
		case HEVCINST_RES_CHANGE_INIT:
			ret = hevc_run_dec_last_frames(ctx);
			break;
		case HEVCINST_RES_CHANGE_FLUSH:
			ret = hevc_run_dec_last_frames(ctx);
			break;
		case HEVCINST_RES_CHANGE_END:
			hevc_debug(2, "Finished remaining frames after resolution change.\n");
			ctx->capture_state = QUEUE_FREE;
			hevc_debug(2, "Will re-init the codec`.\n");
			hevc_run_init_dec(ctx);
			break;
		case HEVCINST_DPB_FLUSHING:
			ret = hevc_dec_dpb_flush(ctx);
			break;
		default:
			ret = -EAGAIN;
		}
	} else {
		hevc_err("invalid context type: %d\n", ctx->type);
		ret = -EAGAIN;
	}

	if (ret) {
		/* Check again the ctx condition and clear work bits
		 * if ctx is not available. */
		if (hevc_ctx_ready(ctx) == 0) {
			spin_lock_irq(&dev->condlock);
			clear_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}

		/* Free hardware lock */
		if (hevc_clear_hw_bit(ctx) == 0)
			hevc_err("Failed to unlock hardware.\n");

		hevc_clock_off();

		/* Trigger again if other instance's work is waiting */
		spin_lock_irq(&dev->condlock);
		if (dev->ctx_work_bits)
			queue_work(dev->sched_wq, &dev->sched_work);
		spin_unlock_irq(&dev->condlock);
	}
}

void hevc_cleanup_queue(struct list_head *lh, struct vb2_queue *vq)
{
	struct hevc_buf *b;
	int i;

	while (!list_empty(lh)) {
		b = list_entry(lh->next, struct hevc_buf, list);
		for (i = 0; i < b->vb.num_planes; i++)
			vb2_set_plane_payload(&b->vb, i, 0);
		vb2_buffer_done(&b->vb, VB2_BUF_STATE_ERROR);
		list_del(&b->list);
	}
}

void hevc_write_info(struct hevc_ctx *ctx, unsigned int data, unsigned int ofs)
{
	struct hevc_dev *dev = ctx->dev;

	if (dev->hw_lock) {
		WRITEL(data, ofs);
	} else {
		hevc_clock_on();
		WRITEL(data, ofs);
		hevc_clock_off();
	}
}

unsigned int hevc_read_info(struct hevc_ctx *ctx, unsigned int ofs)
{
	struct hevc_dev *dev = ctx->dev;
	int ret;

	if (dev->hw_lock) {
		ret = READL(ofs);
	} else {
		hevc_clock_on();
		ret = READL(ofs);
		hevc_clock_off();
	}

	return ret;
}
