/*
 * drivers/media/video/exynos/hevc/hevc_opr.h
 *
 * Header file for Samsung HEVC driver
 * Contains declarations of hw related functions.
 *
 * Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef HEVC_OPR_H_
#define HEVC_OPR_H_

#include "hevc_mem.h"

#define HEVC_CTRL_MODE_CUSTOM	HEVC_CTRL_MODE_SFR

int hevc_init_decode(struct hevc_ctx *ctx);

int hevc_set_dec_frame_buffer(struct hevc_ctx *ctx);
int hevc_set_dec_stream_buffer(struct hevc_ctx *ctx,
		dma_addr_t buf_addr,
		unsigned int start_num_byte,
		unsigned int buf_size);

int hevc_decode_one_frame(struct hevc_ctx *ctx, int last_frame);

/* Memory allocation */
int hevc_alloc_dec_temp_buffers(struct hevc_ctx *ctx);
void hevc_set_dec_desc_buffer(struct hevc_ctx *ctx);
void hevc_release_dec_desc_buffer(struct hevc_ctx *ctx);

int hevc_alloc_codec_buffers(struct hevc_ctx *ctx);
void hevc_release_codec_buffers(struct hevc_ctx *ctx);

int hevc_alloc_instance_buffer(struct hevc_ctx *ctx);
void hevc_release_instance_buffer(struct hevc_ctx *ctx);
int hevc_alloc_dev_context_buffer(struct hevc_dev *dev);
void hevc_release_dev_context_buffer(struct hevc_dev *dev);

void hevc_dec_calc_dpb_size(struct hevc_ctx *ctx);

#define hevc_get_dspl_status()	readl(dev->regs_base + \
						HEVC_D_DISPLAY_STATUS)
#define hevc_get_decoded_status()	readl(dev->regs_base + \
						HEVC_D_DECODED_STATUS)
#define hevc_get_dec_frame_type()	(readl(dev->regs_base + \
						HEVC_D_DECODED_FRAME_TYPE) \
						& HEVC_DECODED_FRAME_MASK)
#define hevc_get_disp_frame_type()	(readl(ctx->dev->regs_base + \
						HEVC_D_DISPLAY_FRAME_TYPE) \
						& HEVC_DISPLAY_FRAME_MASK)
#define hevc_get_consumed_stream()	readl(dev->regs_base + \
						HEVC_D_DECODED_NAL_SIZE)
#define hevc_get_int_reason()	(readl(dev->regs_base + \
					HEVC_RISC2HOST_CMD))
#define hevc_get_int_err()		readl(dev->regs_base + \
						HEVC_ERROR_CODE)
#define hevc_err_dec(x)		(((x) & HEVC_ERR_DEC_MASK) >> \
						HEVC_ERR_DEC_SHIFT)
#define hevc_err_dspl(x)		(((x) & HEVC_ERR_DSPL_MASK) >> \
						HEVC_ERR_DSPL_SHIFT)
#define hevc_get_img_width()		readl(dev->regs_base + \
						HEVC_D_DISPLAY_FRAME_WIDTH)
#define hevc_get_img_height()	readl(dev->regs_base + \
						HEVC_D_DISPLAY_FRAME_HEIGHT)
#define hevc_get_dpb_count()		readl(dev->regs_base + \
						HEVC_D_MIN_NUM_DPB)
#define hevc_get_lcu_size()		readl(dev->regs_base + \
						HEVC_D_HEVC_INFO)
#define hevc_get_mv_count()		readl(dev->regs_base + \
						HEVC_D_MIN_NUM_MV)
#define hevc_get_inst_no()		readl(dev->regs_base + \
						HEVC_RET_INSTANCE_ID)
#define hevc_get_sei_avail_status()	readl(dev->regs_base + \
						HEVC_D_FRAME_PACK_SEI_AVAIL)
#define hevc_is_interlace_picture()	((readl(dev->regs_base + \
					HEVC_D_DECODED_STATUS) & \
					HEVC_DEC_STATUS_INTERLACE_MASK) == \
					HEVC_DEC_STATUS_INTERLACE)

#define hevc_get_dec_status()	(readl(dev->regs_base + \
						HEVC_D_DECODED_STATUS) \
						& HEVC_DECODED_FRAME_MASK)

#define hevc_get_dec_frame()		(readl(dev->regs_base + \
						HEVC_D_DECODED_FRAME_TYPE) \
						& HEVC_DECODED_FRAME_MASK)
#define hevc_get_dec_used_flag()		readl(dev->regs_base + \
						HEVC_D_USED_DPB_FLAG_LOWER)

#define mb_width(x_size)		((x_size + 15) / 16)
#define mb_height(y_size)		((y_size + 15) / 16)

#define hevc_dec_mv_size(x, y)		\
			(((x + 63) / 64) * ((y + 63) / 64) * 256 + 512 + 1024)

#define hevc_clear_int_flags()				\
	do {							\
		hevc_write_reg(0, HEVC_RISC2HOST_CMD);	\
		hevc_write_reg(0, HEVC_RISC2HOST_INT);	\
	} while (0)

/* Definitions for shared memory compatibility */
#define PIC_TIME_TOP		HEVC_D_RET_PICTURE_TAG_TOP
#define PIC_TIME_BOT		HEVC_D_RET_PICTURE_TAG_BOT
#define CROP_INFO_H		HEVC_D_DISPLAY_CROP_INFO1
#define CROP_INFO_V		HEVC_D_DISPLAY_CROP_INFO2

#define DEC_HEVC_SCRATCH_SIZE(x, y)			\
			((((x + 15) / 16) + 1) * 64 + ((x + 63) / 64) * 256 + \
			((y + 63) / 64) * 256 + (x + 64) * 10 + y * 128)

void hevc_try_run(struct hevc_dev *dev);

void hevc_cleanup_queue(struct list_head *lh, struct vb2_queue *vq);

void hevc_write_info(struct hevc_ctx *ctx, unsigned int data, unsigned int ofs);
unsigned int hevc_read_info(struct hevc_ctx *ctx, unsigned int ofs);

#endif /* HEVC_OPR_H_ */
