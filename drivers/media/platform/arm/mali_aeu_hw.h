// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#ifndef _MALI_AEU_HW_H_
#define _MALI_AEU_HW_H_

#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>

#define AEU_OFFSET	0x08000
#define AEU_DS_OFFSET	0x08200
#define AEU_AES_OFFSET	0x08400
#define AEU_TRUSTED	0x18000

#define _AES_REG(offset)	(AEU_AES_OFFSET + offset)
#define _DS_REG(offset)		(AEU_DS_OFFSET + offset)
#define _TRUSTED_REG(offset)	(AEU_TRUSTED + offset)

#define _BLK_INFO(base)		(base + 0x000)
#define _PPL_INFO(base)		(base + 0x004)
#define _IRQ_STATUS(base)	(base + 0x010)
#define _STATUS(base)		(base + 0x0B0)
#define _CONTROL(base)		(base + 0x0D0)

#define AEU_BLOCK_INFO		_BLK_INFO(AEU_OFFSET)
#define AEU_PIPELINE_INFO	_PPL_INFO(AEU_OFFSET)
#define AEU_IRQ_STATUS		_IRQ_STATUS(AEU_OFFSET)
#define AEU_STATUS		_STATUS(AEU_OFFSET)
#define AEU_CONTROL		_CONTROL(AEU_OFFSET)

#define AEU_IRQ_DS		(1 << 0)
#define AEU_IRQ_AES		(1 << 1)

#define AEU_CTRL_SRST		(1 << 16)
#define AEU_CTRL_PM		(1 << 20)

#define AES_IRQ_EOW		(1 << 0)
#define AES_IRQ_CFGS		(1 << 1)
#define AES_IRQ_ERR		(1 << 2)
#define AES_IRQ_TERR		(1 << 3)

#define AES_CTRL_EN		(1 << 0)
#define AES_CMD_DS		(1 << 0)

#define AEU_AES_BLOCK_INFO	_BLK_INFO(AEU_AES_OFFSET)
#define AEU_AES_PIPELINE_INFO	_PPL_INFO(AEU_AES_OFFSET)
#define AEU_AES_BLOCK_ID	_AES_REG(0x100)
#define AEU_AES_IRQ_RAW_STATUS	_AES_REG(0x104)
#define AEU_AES_IRQ_CLEAR	_AES_REG(0x108)
#define AEU_AES_IRQ_MASK	_AES_REG(0x10C)
#define AEU_AES_IRQ_STATUS	_AES_REG(0x110)
#define AEU_AES_COMMAND		_AES_REG(0x114)
#define AEU_AES_STATUS		_AES_REG(0x118)
#define AEU_AES_CONTROL		_AES_REG(0x11C)

#define AES_IRQ_STATUS_EOW	(1 << 0)
#define AES_IRQ_STATUS_CFGS	(1 << 1)
#define AES_IRQ_STATUS_ERR	(1 << 2)
#define AES_IRQ_STATUS_TERR	(1 << 3)

#define AEU_DS_BLOCK_INFO	_BLK_INFO(AEU_DS_OFFSET)
#define AEU_DS_PIPELINE_INFO	_PPL_INFO(AEU_DS_OFFSET)
#define AEU_DS_IRQ_RAW_STATUS	_DS_REG(0x0A0)
#define AEU_DS_IRQ_CLEAR	_DS_REG(0x0A4)
#define AEU_DS_IRQ_MASK		_DS_REG(0x0A8)
#define AEU_DS_IRQ_STATUS	_DS_REG(0x0AC)
#define AEU_DS_CONTROL		_DS_REG(0x0D0)
#define ADU_DS_CONFIG_VALID	_DS_REG(0x0D4)
#define ADU_DS_PROG_LINE	_DS_REG(0x0D8)

#define DS_IRQ_ERR		(1 << 2)
#define DS_IRQ_CVAL		(1 << 8)
#define DS_IRQ_PL		(1 << 9)

enum aeu_hw_ds_format {
	ds_argb_2101010 = 0,
	ds_abgr_2101010,
	ds_rgba_1010102,
	ds_bgra_1010102,

	ds_argb_8888 = 8,
	ds_abgr_8888,
	ds_rgba_8888,
	ds_bgra_8888,

	ds_xrgb_8888 = 16,
	ds_xbgr_8888,
	ds_rgbx_8888,
	ds_bgrx_8888,

	ds_rgba_5551 = 32,
	ds_abgr_1555,
	ds_rgb_565,
	ds_bgr_565,

	ds_yuv_422_p2_8 = 41,
	ds_vyuv_422_p1_8,

	ds_yuv_420_p2_8 = 46,
	ds_yuv_420_p3_8,

	ds_yuv_420_p2_10 = 55
};

enum aeu_hw_aes_format {
	aes_rgb_565 = 0,
	aes_rgba_5551,
	ase_rgba_1010102,
	aes_yuv_420_p2_10,
	aes_rgb_888,
	aes_rgba_8888,
	aes_rgba_4444,
	aes_r_8,
	aes_rg_88,
	aes_yuv_420_p2_8,
	ase_yuyv = 11,
	aes_y210 = 14,
};

struct mali_aeu_hw_info {
	u32 min_width, min_height, max_width, max_height;
	u32 raddr_align;	/* read */
	u32 waddr_align;	/* write afbc 1.0/1.1 */
	u32 waddr_align_afbc12;	/* write afbc 1.2 */
};

typedef struct mali_aeu_hw_ctx mali_aeu_hw_ctx_t;
struct mali_aeu_hw_device;

struct mali_aeu_hw_device *
mali_aeu_hw_init(void __iomem *r, struct device *dev,
		 struct mali_aeu_hw_info *hw_info);
void mali_aeu_hw_exit(struct mali_aeu_hw_device *hw_dev);

irqreturn_t mali_aeu_hw_irq_handler(int irq, void *data);

mali_aeu_hw_ctx_t *
mali_aeu_hw_init_ctx(struct mali_aeu_hw_device *hw_dev);
void mali_aeu_hw_free_ctx(mali_aeu_hw_ctx_t *hw_ctx);

void mali_aeu_hw_connect_m2m_device(struct mali_aeu_hw_device *hw_dev,
		struct v4l2_m2m_dev *m2mdev,
		mali_aeu_hw_ctx_t* (*cb)(struct v4l2_m2m_dev *));
struct v4l2_m2m_dev *
mali_aeu_hw_get_m2m_device(struct mali_aeu_hw_device *hw_dev);
#endif
