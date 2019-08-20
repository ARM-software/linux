// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include "mali_aeu_hw.h"
#include "mali_aeu_io.h"
#include "mali_aeu_reg_dump.h"

#define MALI_AEU_VALID_TIMEOUT	5000

#define DS_OUPUT_FAKE_ADDRESS	0x400000

#define MALI_AEU_HW_EVENT_DS_ERR	(1 << 0)
#define MALI_AEU_HW_EVENT_DS_CVAL	(1 << 1)
#define MALI_AEU_HW_EVENT_DS_PL		(1 << 2)
#define MALI_AEU_HW_EVENT_AES_EOW	(1 << 3)
#define MALI_AEU_HW_EVENT_AES_CFGS	(1 << 4)
#define MALI_AEU_HW_EVENT_AES_ERR	(1 << 5)
#define MALI_AEU_HW_EVENT_AES_TERR	(1 << 6)

struct mali_aeu_hw_device {
	void __iomem	*reg;
	struct device	*dev;
	struct v4l2_m2m_dev *m2mdev;
	/* protect for access registers and status */
	spinlock_t	reglock;
	u32	irq_on: 1,
		th_on:  1,
		reserved: 30;
	mali_aeu_hw_ctx_t* (*get_current_hw_ctx_cb)(struct v4l2_m2m_dev *mdev);
};

struct mali_aeu_hw_ctx {
	struct mali_aeu_hw_device *hw_dev;

	u16 input_size_h, input_size_v;
	dma_addr_t input_p0_addr;
	dma_addr_t input_p1_addr;
	dma_addr_t input_p2_addr;
	int input_p0_stride, input_p1_stride;

	dma_addr_t output_p0_addr;
	dma_addr_t output_p1_addr;

	enum aeu_hw_ds_format ds_format;
	enum aeu_hw_aes_format aes_format;
	u16	afbc_fmt_flags;
	/* aes_<h, v> is calculated by input_size */
	u16 aes_h, aes_v;

	wait_queue_head_t	wq;
	atomic_t	event;

#ifdef CONFIG_VIDEO_ADV_DEBUG
	struct mali_aeu_reg_file *reg_file;
#endif
};

bool mali_aeu_hw_job_done(struct mali_aeu_hw_ctx *hw_ctx)
{
	u32 flag = atomic_read(&hw_ctx->event);

	return (flag & MALI_AEU_HW_EVENT_AES_EOW) ? true : false;
}

static const struct mali_aeu_hw_info aeu_hw_info = {
	.min_width		= 4,
	.min_height		= 4,
	.max_width		= 4096,
	.max_height		= 4096,
	.raddr_align		= 16,
	.waddr_align		= 16,
	.waddr_align_afbc12	= 4096
};

struct mali_aeu_hw_pixfmt_item {
	enum aeu_hw_ds_format input_format;
	enum aeu_hw_aes_format convert_format; /* for aes */
	u16 nplanes;
	u16 bpp; /* = (output_bpp << 8) | input_bpp */
	bool native_supported;
};

#define BPP(in, out)	((in) | ((out) << 8))
/* bpp of input and output formats are same */
#define BPP_1(in)	BPP(in, in)
#define INPUT_BPP(x)	((x) & 0xFF)
#define OUTPUT_BPP(x)	(0xFF & ((x) >> 8))

static const struct mali_aeu_hw_pixfmt_item mali_aeu_hw_pixfmt_table[] = {
	{ ds_argb_2101010, aes_rgba_1010102, 1, BPP_1(32), false },
	{ ds_abgr_2101010, aes_rgba_1010102, 1, BPP_1(32), false },
	{ ds_rgba_1010102, aes_rgba_1010102, 1, BPP_1(32), true },
	{ ds_bgra_1010102, aes_rgba_1010102, 1, BPP_1(32), false },

	{ ds_argb_8888, aes_rgba_8888, 1, BPP_1(32), false },
	{ ds_abgr_8888, aes_rgba_8888, 1, BPP_1(32), false },
	{ ds_rgba_8888, aes_rgba_8888, 1, BPP_1(32), true },
	{ ds_bgra_8888, aes_rgba_8888, 1, BPP_1(32), false },

	{ ds_xrgb_8888, aes_rgb_888, 1, BPP(32, 24), false },
	{ ds_xbgr_8888, aes_rgb_888, 1, BPP(32, 24), false },
	{ ds_rgbx_8888, aes_rgb_888, 1, BPP(32, 24), true },
	{ ds_bgrx_8888, aes_rgb_888, 1, BPP(32, 24), false },

	{ ds_rgba_5551, aes_rgba_5551, 1, BPP_1(16), true },
	{ ds_abgr_1555, aes_rgba_5551, 1, BPP_1(16), false },

	{ ds_rgb_565, aes_rgb_565, 1, BPP_1(16), true },
	{ ds_bgr_565, aes_rgb_565, 1, BPP_1(16), false },

	{ ds_vyuv_422_p1_8, aes_yuyv, 1, BPP_1(16), true },
	{ ds_yvyu_422_p1_8, aes_yuyv, 1, BPP_1(16), false },

	{ ds_yuv_420_p2_8, aes_yuv_420_p2_8, 2, BPP_1(8), true },
	{ ds_yuv_420_p3_8, aes_yuv_420_p2_8, 3, BPP_1(8), false },

	{ ds_yuv_420_p2_10, aes_yuv_420_p2_10, 2, BPP_1(16), true }
};

#define HW_FMT_TABLE_SIZE \
	sizeof(mali_aeu_hw_pixfmt_table)/sizeof(struct mali_aeu_hw_pixfmt_item)

static u32 mali_aeu_hw_ds_fmt_index(enum aeu_hw_ds_format ifmt)
{
	u32 idx;

	for (idx = 0; idx < HW_FMT_TABLE_SIZE; idx++) {
		if (ifmt == mali_aeu_hw_pixfmt_table[idx].input_format)
			return idx;
	}

	return 0xFFFFFFFF;
}

static u32 mali_aeu_hw_aes_fmt_index(enum aeu_hw_aes_format ofmt)
{
	u32 idx;

	for (idx = 0; idx < HW_FMT_TABLE_SIZE; idx++) {
		if (ofmt == mali_aeu_hw_pixfmt_table[idx].convert_format)
			return idx;
	}

	return 0xFFFFFFFF;
}

enum aeu_hw_aes_format mali_aeu_hw_convert_fmt(enum aeu_hw_ds_format ifmt)
{
	u32 i = mali_aeu_hw_ds_fmt_index(ifmt);

	if (i == 0xFFFFFFFF)
		return i;
	return mali_aeu_hw_pixfmt_table[i].convert_format;
}

u16 mali_aeu_hw_pix_fmt_planes(enum aeu_hw_ds_format ifmt)
{
	u32 i = mali_aeu_hw_ds_fmt_index(ifmt);;

	if (i == 0xFFFFFFFF)
		return 0xFFFF;
	return mali_aeu_hw_pixfmt_table[i].nplanes;
}

bool mali_aeu_hw_pix_fmt_native(enum aeu_hw_ds_format ifmt)
{
	u32 i = mali_aeu_hw_ds_fmt_index(ifmt);
	if (i == 0xFFFFFFFF)
		return false;
	return mali_aeu_hw_pixfmt_table[i].native_supported;
}

static void reset_hw(struct mali_aeu_hw_device *hw_dev)
{
	unsigned int v;

	mali_aeu_write(hw_dev->reg, AEU_CONTROL, AEU_CTRL_SRST);
	do {
		v = mali_aeu_read(hw_dev->reg, AEU_CONTROL);
	} while (v & AEU_CTRL_SRST);

	hw_dev->irq_on = 0;
	hw_dev->th_on = 0;

	mali_aeu_write(hw_dev->reg, AEU_DS_CONTROL, DS_CTRL_EN);
	mali_aeu_write(hw_dev->reg, AEU_AES_CONTROL, AES_CTRL_EN);
}


static int mali_aeu_soft_reset(struct mali_aeu_hw_device *hw_dev)
{
	unsigned long flags;
	unsigned int count, val;
	int ret = 0;

	spin_lock_irqsave(&hw_dev->reglock, flags);
	mali_aeu_write(hw_dev->reg, AEU_AES_CONTROL, 0);
	mali_aeu_write(hw_dev->reg, AEU_AES_COMMAND, 1);

	count = 100;
	while (--count) {
		val = mali_aeu_read(hw_dev->reg, AEU_AES_IRQ_RAW_STATUS);
		if (val & AES_IRQ_CFGS)
			break;
		mdelay(1);
	}

	if (!count) {
		dev_err(hw_dev->dev, "CFCG is not asserted\n");
		ret = -1;
		goto exit_reset;
	}
	mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_CLEAR, AES_IRQ_CFGS);

	reset_hw(hw_dev);

exit_reset:
	spin_unlock_irqrestore(&hw_dev->reglock, flags);
	return ret;
}

static void mali_aeu_hw_enable_irq(struct mali_aeu_hw_device *hw_dev)
{
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&hw_dev->reglock, flags);
	if (!hw_dev->irq_on) {
		v = AES_IRQ_EOW | AES_IRQ_CFGS | AES_IRQ_ERR | AES_IRQ_TERR;
		mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, v);

		v = DS_IRQ_ERR | DS_IRQ_CVAL | DS_IRQ_PL;
		mali_aeu_write(hw_dev->reg, AEU_DS_IRQ_MASK, v);
		hw_dev->irq_on = 1;
	}
	spin_unlock_irqrestore(&hw_dev->reglock, flags);
}

struct mali_aeu_hw_device *
mali_aeu_hw_init(void __iomem *r, struct device *dev,
		 struct mali_aeu_hw_info *hw_info)
{
	struct mali_aeu_hw_device *hw_dev = NULL;
	u32 v;

	BUG_ON(r == NULL);
	/* read registers to ensure adu */
	v = mali_aeu_read(r, AEU_BLOCK_INFO);
	if ((v & 0xFFFF) != 0x402)
		return NULL;

	hw_dev = devm_kzalloc(dev, sizeof(*hw_dev), GFP_KERNEL);
	if (!hw_dev)
		return NULL;

	hw_dev->dev = dev;
	hw_dev->reg = r;
	spin_lock_init(&hw_dev->reglock);

	if (mali_aeu_soft_reset(hw_dev)) {
		devm_kfree(dev, hw_dev);
		dev_err(dev, "%s: hardware reset error!\n", __func__);
		return NULL;
	}

	if (hw_info)
		*hw_info = aeu_hw_info;

	mali_aeu_hw_enable_irq(hw_dev);

	return hw_dev;
}

static void mali_aeu_hw_disable_irq(struct mali_aeu_hw_device *hw_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&hw_dev->reglock, flags);
	if (hw_dev->irq_on) {
		mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, 0);
		mali_aeu_write(hw_dev->reg, AEU_DS_IRQ_MASK, 0);
		hw_dev->irq_on = 0;
	}
	spin_unlock_irqrestore(&hw_dev->reglock, flags);
}

void mali_aeu_hw_exit(struct mali_aeu_hw_device *hw_dev)
{
	if (hw_dev) {
		/* TODO: clear status */
		mali_aeu_hw_disable_irq(hw_dev);
		devm_kfree(hw_dev->dev, hw_dev);
	}
}

irqreturn_t mali_aeu_hw_irq_handler(int irq, void *data)
{
	struct mali_aeu_hw_device *hw_dev = data;
	unsigned int aeu_irq, val;
	unsigned long flags;
	mali_aeu_hw_ctx_t *hw_ctx;
	irqreturn_t iret = IRQ_HANDLED;
	u32 ievnt = 0;

	aeu_irq = mali_aeu_read(hw_dev->reg, AEU_IRQ_STATUS);
	if (!aeu_irq)
		return IRQ_NONE;

	if (!hw_dev->get_current_hw_ctx_cb) {
		dev_err(hw_dev->dev, "m2m device is not connected\n");
		return IRQ_NONE;
	}

	hw_ctx = hw_dev->get_current_hw_ctx_cb(hw_dev->m2mdev);
	if (!hw_ctx) {
		dev_err(hw_dev->dev, "hw context error (irq)!\n");
		mali_aeu_hw_disable_irq(hw_dev);
		return iret;
	}

	if (aeu_irq & AEU_IRQ_AES) {
		val = mali_aeu_read(hw_dev->reg, AEU_AES_IRQ_STATUS);
		if (val & AES_IRQ_EOW) {
			dev_info(hw_dev->dev, "AES EOW (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_AES_EOW;
			iret = IRQ_WAKE_THREAD;
		}
		if (val & AES_IRQ_CFGS) {
			dev_info(hw_dev->dev, "AES CFGS (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_AES_CFGS;
			val &= ~AES_IRQ_CFGS;
		}
		if (val & AES_IRQ_ERR) {
			dev_err(hw_dev->dev, "AES Err (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_AES_ERR;
		}
		if (val & AES_IRQ_TERR) {
			dev_err(hw_dev->dev, "AES TErr (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_AES_TERR;
			iret = IRQ_WAKE_THREAD;
			spin_lock_irqsave(&hw_dev->reglock, flags);
			reset_hw(hw_dev);
			spin_unlock_irqrestore(&hw_dev->reglock, flags);
			goto irq_done;
		}

		spin_lock_irqsave(&hw_dev->reglock, flags);
#ifdef CONFIG_VIDEO_ADV_DEBUG
		if (!(ievnt & MALI_AEU_HW_EVENT_AES_CFGS) && hw_ctx)
			mali_aeu_reg_dump(hw_ctx->reg_file, REG_FILE_AFT);
#endif
		mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_CLEAR, val);
		if (ievnt & MALI_AEU_HW_EVENT_AES_CFGS) {
			val = mali_aeu_read(hw_dev->reg, AEU_AES_IRQ_MASK);
			val &= ~AES_IRQ_CFGS;
			mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, val);
		}
		spin_unlock_irqrestore(&hw_dev->reglock, flags);
	}

	if (aeu_irq & AEU_IRQ_DS) {
		val = mali_aeu_read(hw_dev->reg, AEU_DS_IRQ_STATUS);
		if (val & DS_IRQ_ERR) {
			dev_err(hw_dev->dev, "DS Err (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_DS_ERR;
		}
		if (val & DS_IRQ_CVAL) {
			dev_info(hw_dev->dev, "DS CVAL (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_DS_CVAL;
		}
		if (val & DS_IRQ_PL) {
			dev_info(hw_dev->dev, "DS PL (irq)\n");
			ievnt |= MALI_AEU_HW_EVENT_DS_PL;
		}

		spin_lock_irqsave(&hw_dev->reglock, flags);
		mali_aeu_write(hw_dev->reg, AEU_DS_IRQ_CLEAR, val);
		if (val & DS_IRQ_CVAL) {
			mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_CLEAR,
					AES_IRQ_CFGS);
			val = mali_aeu_read(hw_dev->reg, AEU_AES_IRQ_MASK);
			val |= AES_IRQ_CFGS;
			mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, val);
		}
		spin_unlock_irqrestore(&hw_dev->reglock, flags);
	}
irq_done:
	atomic_or(ievnt, &hw_ctx->event);
	wake_up(&hw_ctx->wq);

	return iret;
}

mali_aeu_hw_ctx_t *
mali_aeu_hw_init_ctx(struct mali_aeu_hw_device *hw_dev)
{
	mali_aeu_hw_ctx_t *hw_ctx;

	hw_ctx = devm_kzalloc(hw_dev->dev, sizeof(*hw_ctx), GFP_KERNEL);
	if (!hw_ctx)
		return NULL;

	hw_ctx->hw_dev = hw_dev;
	init_waitqueue_head(&hw_ctx->wq);

#ifdef CONFIG_VIDEO_ADV_DEBUG
	hw_ctx->reg_file = mali_aeu_init_reg_file(hw_dev->reg);
	if (!hw_ctx->reg_file)
		dev_err(hw_dev->dev, "%s: dumping register is disabled!\n",
			__func__);
#endif
	return hw_ctx;
}

void mali_aeu_hw_free_ctx(mali_aeu_hw_ctx_t *hw_ctx)
{
#ifdef CONFIG_VIDEO_ADV_DEBUG
	mali_aeu_free_reg_file(hw_ctx->reg_file);
#endif
	devm_kfree(hw_ctx->hw_dev->dev, hw_ctx);
}

static int aeu_hw_ceil(u32 x, u32 y)
{
	WARN_ON(y == 0);
	return (x + y - 1)/y;
}

static void mali_aeu_hw_set_input_size(mali_aeu_hw_ctx_t *hw_ctx, u32 h, u32 w)
{
	hw_ctx->input_size_h = w;
	hw_ctx->input_size_v = h;

	hw_ctx->aes_h = 128; /* it is fixed size */
	hw_ctx->aes_v = aeu_hw_ceil(hw_ctx->input_size_h, 128) *
			aeu_hw_ceil(hw_ctx->input_size_v, 16);
	hw_ctx->aes_v <<= 4;
}

void mali_aeu_hw_set_buffer_fmt(mali_aeu_hw_ctx_t *hw_ctx,
			struct mali_aeu_hw_buf_fmt *in_fmt,
			struct mali_aeu_hw_buf_fmt *out_fmt)
{
	WARN_ON(in_fmt->buf_type != AEU_HW_INPUT_BUF);
	WARN_ON(out_fmt->buf_type != AEU_HW_OUTPUT_BUF);

	hw_ctx->ds_format = in_fmt->input_format;
	hw_ctx->aes_format = out_fmt->output_format;
	hw_ctx->afbc_fmt_flags = in_fmt->afbc_fmt_flags |
				 out_fmt->afbc_fmt_flags;
	mali_aeu_hw_set_input_size(hw_ctx, in_fmt->buf_h, in_fmt->buf_w);
}

static u32 input_buf_width_adjust(u32 w, enum aeu_hw_ds_format f, u32 plane)
{
	u32 s = 1;

	if (plane == 0)
		return w;

	switch (f) {
	case ds_yuv_422_p2_8:
	case ds_vyuv_422_p1_8:
	case ds_yvyu_422_p1_8:
	case ds_yuv_420_p2_8:
	case ds_yuv_420_p3_8:
	case ds_yuv_420_p2_10:
		s = 2;
		break;
	default:
		return w;
	}

	return w / s;
}

static void aeu_ds_output_stride(enum aeu_hw_ds_format f, u32 *p0_s, u32 *p1_s)
{
	u32 idx = mali_aeu_hw_ds_fmt_index(f);
	u32 stride;

	stride = INPUT_BPP(mali_aeu_hw_pixfmt_table[idx].bpp) >> 3;
	*p0_s = stride * 128;

	*p1_s = 0;
	if (mali_aeu_hw_pixfmt_table[idx].nplanes > 1) {
		u32 w = input_buf_width_adjust(128, f, 1);
		if (f != ds_yuv_420_p3_8)
			stride <<= 1;
		*p1_s = ALIGN_UP(stride * w, 128);
	}
}

int mali_aeu_hw_ctx_commit(mali_aeu_hw_ctx_t *hw_ctx)
{
	struct mali_aeu_hw_device *hw_dev = hw_ctx->hw_dev;
	void __iomem *reg = hw_dev->reg;
	unsigned long flags;
	u32 val, fake_addr = 0;
	u32 aes_p0_stride, aes_p1_stride;
	long ret;

	aeu_ds_output_stride(hw_ctx->ds_format,
			&aes_p0_stride, &aes_p1_stride);

	atomic_set(&hw_ctx->event, 0);

	mali_aeu_hw_enable_irq(hw_dev);

	if (hw_ctx->input_p1_addr)
		fake_addr = DS_OUPUT_FAKE_ADDRESS + 0x90000000;

	spin_lock_irqsave(&hw_dev->reglock, flags);
	/* configure AES first */
	mali_aeu_write(reg, AEU_AES_IN_HSIZE, hw_ctx->aes_h);
	mali_aeu_write(reg, AEU_AES_IN_VSIZE, hw_ctx->aes_v);
	mali_aeu_write(reg, AEU_AES_BBOX_X_START, 0);
	mali_aeu_write(reg, AEU_AES_BBOX_X_END, hw_ctx->aes_h - 1);
	mali_aeu_write(reg, AEU_AES_BBOX_Y_START, 0);
	mali_aeu_write(reg, AEU_AES_BBOX_Y_END, hw_ctx->aes_v - 1);

	val = hw_ctx->aes_format;
	val |= hw_ctx->afbc_fmt_flags << 8;
	mali_aeu_write(reg, AEU_AES_FORMAT, val);

	mali_aeu_write(reg, AEU_AES_OUT_P0_PTR_LOW,
			lower_32_bits(hw_ctx->output_p0_addr));
	mali_aeu_write(reg, AEU_AES_OUT_P0_PTR_HIGH,
			upper_32_bits(hw_ctx->output_p0_addr));
	mali_aeu_write(reg, AEU_AES_OUT_P1_PTR_LOW,
			lower_32_bits(hw_ctx->output_p1_addr));
	mali_aeu_write(reg, AEU_AES_OUT_P1_PTR_HIGH,
			upper_32_bits(hw_ctx->output_p1_addr));
	mali_aeu_write(reg, AEU_AES_IN_P0_STRIDE, aes_p0_stride);
	mali_aeu_write(reg, AEU_AES_IN_P1_STRIDE, aes_p1_stride);
	mali_aeu_write(reg, AEU_AES_IN_P0_PTR_LOW, DS_OUPUT_FAKE_ADDRESS);
	mali_aeu_write(reg, AEU_AES_IN_P1_PTR_LOW, fake_addr);

	/* Enable and direct swap */
	mali_aeu_write(reg, AEU_AES_COMMAND, AES_CMD_DS);
	spin_unlock_irqrestore(&hw_dev->reglock, flags);

	dev_dbg(hw_dev->dev, "ase_h: %u, aes_v: %u\n",
				hw_ctx->aes_h, hw_ctx->aes_v);
	dev_dbg(hw_dev->dev, "output address (p0): %p\n",
				(void*)hw_ctx->output_p0_addr);
	dev_dbg(hw_dev->dev, "output address (p1): %p\n",
				(void*)hw_ctx->output_p1_addr);
	dev_dbg(hw_dev->dev, "output stride (p0): %u\n", aes_p0_stride);

	/* configure DS */
	spin_lock_irqsave(&hw_dev->reglock, flags);
	mali_aeu_write(reg, AEU_DS_IN_SIZE,
		DS_INPUT_SIZE(hw_ctx->input_size_h, hw_ctx->input_size_v));
	mali_aeu_write(reg, AEU_DS_FORMAT, hw_ctx->ds_format);

	mali_aeu_write(reg, AEU_DS_IN_P0_PTR_LOW,
			lower_32_bits(hw_ctx->input_p0_addr));
	mali_aeu_write(reg, AEU_DS_IN_P0_PTR_HIGH,
			upper_32_bits(hw_ctx->input_p0_addr));
	mali_aeu_write(reg, AEU_DS_IN_P1_PTR_LOW,
			lower_32_bits(hw_ctx->input_p1_addr));
	mali_aeu_write(reg, AEU_DS_IN_P1_PTR_HIGH,
			upper_32_bits(hw_ctx->input_p1_addr));
	mali_aeu_write(reg, AEU_DS_IN_P2_PTR_LOW,
			lower_32_bits(hw_ctx->input_p2_addr));
	mali_aeu_write(reg, AEU_DS_IN_P2_PTR_HIGH,
			upper_32_bits(hw_ctx->input_p2_addr));
	mali_aeu_write(reg, AEU_DS_IN_P0_STRIDE, hw_ctx->input_p0_stride);
	mali_aeu_write(reg, AEU_DS_IN_P1_STRIDE, hw_ctx->input_p1_stride);

	mali_aeu_write(reg, AEU_DS_OUT_P0_PTR_LOW, DS_OUPUT_FAKE_ADDRESS);
	mali_aeu_write(reg, AEU_DS_OUT_P1_PTR_LOW, fake_addr);
	mali_aeu_write(reg, AEU_DS_OUT_P0_STRIDE, aes_p0_stride);
	mali_aeu_write(reg, AEU_DS_OUT_P1_STRIDE, aes_p1_stride);

	/* Enable DS */
	val = !!(hw_ctx->afbc_fmt_flags & MALI_AEU_HW_AFBC_TH);
	if (val != hw_dev->th_on) {
		hw_dev->th_on = val;
		val = (val) ? (DS_CTRL_EN | DS_CTRL_TH) : DS_CTRL_EN;
		mali_aeu_write(reg, AEU_DS_CONTROL, val);
	}
	mali_aeu_write(reg, AEU_DS_CONFIG_VALID, DS_CTRL_CVAL);
#ifdef CONFIG_VIDEO_ADV_DEBUG
	mali_aeu_reg_dump(hw_ctx->reg_file, REG_FILE_BEF);
#endif
	spin_unlock_irqrestore(&hw_dev->reglock, flags);

	dev_dbg(hw_dev->dev, "input h: %u, input v: %u\n",
				hw_ctx->input_size_h, hw_ctx->input_size_v);
	dev_dbg(hw_dev->dev, "input address (p0): %p\n",
				(void*)hw_ctx->input_p0_addr);
	dev_dbg(hw_dev->dev, "input address (p1): %p\n",
				(void*)hw_ctx->input_p1_addr);
	dev_dbg(hw_dev->dev, "input address (p2): %p\n",
				(void*)hw_ctx->input_p2_addr);

	ret = wait_event_timeout(hw_ctx->wq,
		(atomic_read(&hw_ctx->event) & MALI_AEU_HW_EVENT_AES_EOW) != 0,
		msecs_to_jiffies(MALI_AEU_VALID_TIMEOUT));
	if (ret <= 0) {
		dev_err(hw_dev->dev, "waiting EOW error!\n");
		return -1;
	}

	return 0;
}

void mali_aeu_hw_set_buf_addr(mali_aeu_hw_ctx_t *hw_ctx,
		struct mali_aeu_hw_buf_addr *addr, u32 type)
{
	switch (type) {
	case AEU_HW_INPUT_BUF:
		hw_ctx->input_p0_addr = addr->p0_addr;
		hw_ctx->input_p1_addr = addr->p1_addr;
		hw_ctx->input_p2_addr = addr->p2_addr;
		hw_ctx->input_p0_stride = addr->p0_stride;
		hw_ctx->input_p1_stride = addr->p1_stride;
		break;
	case AEU_HW_OUTPUT_BUF:
		hw_ctx->output_p0_addr = addr->p0_addr;
		hw_ctx->output_p1_addr = addr->p1_addr;
		break;
	default:
		WARN_ON(1);
	}
}

void mali_aeu_hw_connect_m2m_device(struct mali_aeu_hw_device *hw_dev,
			struct v4l2_m2m_dev *m2mdev,
			mali_aeu_hw_ctx_t* (*cb)(struct v4l2_m2m_dev *))
{
	hw_dev->m2mdev = m2mdev;
	hw_dev->get_current_hw_ctx_cb = cb;
}

struct v4l2_m2m_dev *
mali_aeu_hw_get_m2m_device(struct mali_aeu_hw_device *hw_dev)
{
	return hw_dev->m2mdev;
}

static u32 input_buf_height_subsampling(enum aeu_hw_ds_format f)
{
	u32 s;

	switch (f) {
	case ds_yuv_420_p2_8:
	case ds_yuv_420_p3_8:
	case ds_yuv_420_p2_10:
		s = 2;
		break;
	case ds_yuv_422_p2_8:
	case ds_vyuv_422_p1_8:
	case ds_yvyu_422_p1_8:
	default:
		s = 1;
	}
	return s;
}

static u32 output_buf_width_adjust(u32 w, enum aeu_hw_aes_format f, u32 plane)
{
	u32 s = 1;

	if (plane == 0)
		return w;

	switch (f) {
	case aes_yuv_420_p2_10:
	case aes_yuv_420_p2_8:
	case aes_y210:
	case aes_yuyv:
		s = 2;
		break;
	default:
		return w;
	}

	return w / s;
}

static u32 output_buf_height_subsampling(enum aeu_hw_aes_format f)
{
	u32 s;

	switch (f) {
	case aes_yuv_420_p2_10:
	case aes_yuv_420_p2_8:
	case aes_y210:
		s = 2;
		break;
	case aes_yuyv:
	default:
		s = 1;
	}

	return s;
}

u32 mali_aeu_hw_plane_stride(struct mali_aeu_hw_buf_fmt *bf, u32 n)
{
	u32 stride = 0;
	u32 align, idx;
	bool is_yuv420_p3 = false;
	u32 w = bf->buf_w;

	if (bf->buf_type == AEU_HW_INPUT_BUF) {
		idx = mali_aeu_hw_ds_fmt_index(bf->input_format);
		align = aeu_hw_info.raddr_align;
		if (bf->input_format == ds_yuv_420_p3_8)
			is_yuv420_p3 = true;
		w = input_buf_width_adjust(bf->buf_w, bf->input_format, n);
		stride = INPUT_BPP(mali_aeu_hw_pixfmt_table[idx].bpp);
	} else {
		idx = mali_aeu_hw_aes_fmt_index(bf->output_format);
		if (bf->afbc_fmt_flags & MALI_AEU_HW_AFBC_TH)
			align = aeu_hw_info.waddr_align_afbc12;
		else
			align = aeu_hw_info.waddr_align;
		w = output_buf_width_adjust(bf->buf_w, bf->output_format, n);
		stride = OUTPUT_BPP(mali_aeu_hw_pixfmt_table[idx].bpp);
	}

	if (idx == 0xFFFFFFFF)
		return 0;

	if (n >= mali_aeu_hw_pixfmt_table[idx].nplanes)
		return 0;

	stride >>= 3;
	if (n && !is_yuv420_p3)
		stride <<= 1;

	stride *= w;
	return ALIGN_UP(stride, align);
}

u32 mali_aeu_hw_plane_size(struct mali_aeu_hw_buf_fmt *bf, u32 n)
{
	u32 h = bf->buf_h;

	if (n == 0)
		goto calc_size;

	if (bf->buf_type == AEU_HW_INPUT_BUF)
		h /= input_buf_height_subsampling(bf->input_format);
	else
		h /= output_buf_height_subsampling(bf->output_format);
calc_size:
	return h * mali_aeu_hw_plane_stride(bf, n);
}

u32 mali_aeu_hw_g_reg(mali_aeu_hw_ctx_t *hw_ctx, u32 table, u32 reg)
{
#ifdef CONFIG_VIDEO_ADV_DEBUG
	u32 v;

	if (mali_aeu_g_reg(hw_ctx->reg_file, table, reg, &v))
		v = 0xDEADDEAD;
	return v;
#else
	return 0;
#endif
}
