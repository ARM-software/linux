/*
 * drivers/media/platform/s5p-mfc/s5p_mfc_opr.c
 *
 * Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains hw related functions.
 *
 * Kamil Debski, Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "s5p_mfc_debug.h"
#include "s5p_mfc_opr.h"
#include "s5p_mfc_opr_v5.h"
#include "s5p_mfc_opr_v6.h"
#include "s5p_mfc_ctrl.h"

void s5p_mfc_init_hw_ops(struct s5p_mfc_dev *dev)
{
	if (IS_MFCV6(dev))
		dev->mfc_ops = s5p_mfc_init_hw_ops_v6();
	else
		dev->mfc_ops = s5p_mfc_init_hw_ops_v5();
}

void s5p_mfc_init_regs(struct s5p_mfc_dev *dev)
{
	if (IS_MFCV6(dev))
		dev->mfc_regs = s5p_mfc_init_regs_v6_plus(dev);
}

void s5p_mfc_init_hw_ctrls(struct s5p_mfc_dev *dev)
{
	if (IS_MFCV6(dev))
		dev->mfc_ctrl_ops = s5p_mfc_init_hw_ctrl_ops_v6_plus();
	else
		dev->mfc_ctrl_ops = s5p_mfc_init_hw_ctrl_ops_v5();
}

int s5p_mfc_alloc_priv_buf(struct device *dev, struct s5p_mfc_priv_buf *b,
				size_t size, bool needs_cpu_access)
{
	void *token;
	dma_addr_t dma;
	DEFINE_DMA_ATTRS(attrs);

	mfc_debug(3, "Allocating priv buffer of size: %d\n", size);

	/* We shouldn't normally allocate on top of previously-allocated buffer,
	 * but if we happen to, at least free it first. */
	WARN_ON(b->token);
	s5p_mfc_release_priv_buf(dev, b);

	if (!needs_cpu_access)
		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);

	token = dma_alloc_attrs(dev, size, &dma, GFP_KERNEL, &attrs);
	if (!token) {
		mfc_err("Allocating private buffer of size %d failed\n", size);
		return -ENOMEM;
	}

	if (needs_cpu_access)
		b->virt = token;

	b->dma = dma;
	b->size = size;
	b->attrs = attrs;
	b->token = token;

	mfc_debug(3, "Allocated dma_addr %08x mapped to %p\n", b->dma, b->virt);
	return 0;
}

void s5p_mfc_release_priv_buf(struct device *dev, struct s5p_mfc_priv_buf *b)
{
	if (b->token)
		dma_free_attrs(dev, b->size, b->token, b->dma, &b->attrs);

	memset(b, 0, sizeof(*b));
}

