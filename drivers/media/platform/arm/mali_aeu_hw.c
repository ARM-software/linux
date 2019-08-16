// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include "mali_aeu_hw.h"

#define MALI_AEU_VALID_TIMEOUT	5000

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
	/* protect for access registers */
	spinlock_t	reglock;
	mali_aeu_hw_ctx_t* (*get_current_hw_ctx_cb)(struct v4l2_m2m_dev *mdev);
};

struct mali_aeu_hw_ctx {
	struct mali_aeu_hw_device *hw_dev;

	u16 input_size_h, input_size_v;
	dma_addr_t input_p0_addr;
	dma_addr_t input_p1_addr;
	dma_addr_t input_p2_addr;
	u32 input_p0_sride, input_p1_stride;

	dma_addr_t output_p0_addr;
	dma_addr_t output_p1_addr;

	enum aeu_hw_ds_format input_ds_format;
	enum aeu_hw_aes_format input_aes_format;

	wait_queue_head_t	wq;
	atomic_t	event;
};

static const struct mali_aeu_hw_info aeu_hw_info = {
	.min_width		= 4,
	.min_height		= 4,
	.max_width		= 4096,
	.max_height		= 4096,
	.raddr_align		= 16,
	.waddr_align		= 16,
	.waddr_align_afbc12	= 4096
};

/* internal IO routines */
static u32 mali_aeu_read(void __iomem *reg, u32 offset)
{
	return readl(reg + offset);
}

static void mali_aeu_write(void __iomem *reg, u32 offset, u32 val)
{
	writel(val, reg + offset);
}

#define SOFT_RESET(hw_dev) {						\
	unsigned int v = mali_aeu_read((hw_dev)->reg, AEU_CONTROL);	\
	v |= AEU_CTRL_SRST;						\
	mali_aeu_write((hw_dev)->reg, AEU_CONTROL, v);			\
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
		if (val & AES_IRQ_STATUS_CFGS)
			break;
		mdelay(1);
	}

	if (!count) {
		dev_err(hw_dev->dev, "CFCG is not asserted\n");
		ret = -1;
		goto exit_reset;
	}
	mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_CLEAR, AES_IRQ_CFGS);

	SOFT_RESET(hw_dev);

	count = 1000;
	ret = -1;
	do {
		val = mali_aeu_read(hw_dev->reg, AEU_CONTROL);
		if (!(val & AEU_CTRL_SRST)) {
			ret = 0;
			break;
		}
		mdelay(1);
	} while (--count);
	mali_aeu_write(hw_dev->reg, AEU_DS_CONTROL, 0);
exit_reset:
	spin_unlock_irqrestore(&hw_dev->reglock, flags);
	return ret;
}

static void mali_aeu_hw_enable_irq(struct mali_aeu_hw_device *hw_dev)
{
	unsigned long flags;
	u32 v;

	spin_lock_irqsave(&hw_dev->reglock, flags);
	v = AES_IRQ_EOW | AES_IRQ_CFGS | AES_IRQ_ERR | AES_IRQ_TERR;
	mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, v);

	v = DS_IRQ_ERR | DS_IRQ_CVAL | DS_IRQ_PL;
	mali_aeu_write(hw_dev->reg, AEU_DS_IRQ_MASK, v);
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
	mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, 0);
	mali_aeu_write(hw_dev->reg, AEU_DS_IRQ_MASK, 0);
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

	if (!hw_dev->get_current_hw_ctx_cb) {
		dev_err(hw_dev->dev, "m2m device is not connected\n");
		return IRQ_NONE;
	}

	hw_ctx = hw_dev->get_current_hw_ctx_cb(hw_dev->m2mdev);
	if (!hw_ctx) {
		dev_err(hw_dev->dev, "hw context error (irq)!\n");
		mali_aeu_hw_disable_irq(hw_dev);
	}

	aeu_irq = mali_aeu_read(hw_dev->reg, AEU_IRQ_STATUS);
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
		if (val & DS_IRQ_CVAL)
			mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_CLEAR,
					AES_IRQ_CFGS);
		spin_unlock_irqrestore(&hw_dev->reglock, flags);
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
		}

		spin_lock_irqsave(&hw_dev->reglock, flags);
		mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_CLEAR, val);
		if (ievnt & MALI_AEU_HW_EVENT_AES_CFGS) {
			val = mali_aeu_read(hw_dev->reg, AEU_AES_IRQ_MASK);
			val &= ~AES_IRQ_CFGS;
			mali_aeu_write(hw_dev->reg, AEU_AES_IRQ_MASK, val);
		}
		spin_unlock_irqrestore(&hw_dev->reglock, flags);
	}

	if (hw_ctx)
		atomic_or(ievnt, &hw_ctx->event);

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

	return hw_ctx;
}

void mali_aeu_hw_free_ctx(mali_aeu_hw_ctx_t *hw_ctx)
{
	devm_kfree(hw_ctx->hw_dev->dev, hw_ctx);
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

