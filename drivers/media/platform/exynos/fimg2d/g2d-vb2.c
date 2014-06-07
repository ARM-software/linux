/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Videobuf2 bridge driver file for EXYNOS FIMG2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include "g2d.h"

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
void *g2d_cma_init(struct g2d_dev *g2d)
{
	return vb2_cma_phys_init(g2d->dev, NULL, 0, false);
}

int g2d_cma_resume(void *alloc_ctx) {}
void g2d_cma_suspend(void *alloc_ctx) {}
void g2d_cma_set_cacheable(void *alloc_ctx, bool cacheable) {}
int g2d_cma_cache_flush(struct vb2_buffer *vb, u32 plane_no) { return 0; }

const struct g2d_vb2 g2d_vb2_cma = {
	.ops		= &vb2_cma_phys_memops,
	.init		= g2d_cma_init,
	.cleanup	= vb2_cma_phys_cleanup,
	.plane_addr	= vb2_cma_phys_plane_paddr,
	.resume		= g2d_cma_resume,
	.suspend	= g2d_cma_suspend,
	.cache_flush	= g2d_cma_cache_flush,
	.set_cacheable	= g2d_cma_set_cacheable,
};

#elif defined(CONFIG_VIDEOBUF2_ION)
void *g2d_ion_init(struct g2d_dev *g2d, bool is_use_cci)
{
	long flags = 0;

#if defined(CONFIG_FIMG2D_CCI_SNOOP)
	if (is_use_cci)
		flags = (VB2ION_CTX_COHERENT | VB2ION_CTX_VMCONTIG
				| VB2ION_CTX_IOMMU | VB2ION_CTX_UNCACHED);
	else
		flags = (VB2ION_CTX_VMCONTIG | VB2ION_CTX_IOMMU
				| VB2ION_CTX_UNCACHED);
#else
	flags = (VB2ION_CTX_VMCONTIG | VB2ION_CTX_IOMMU | VB2ION_CTX_UNCACHED);
#endif
	return vb2_ion_create_context(g2d->dev, SZ_4K, flags);
}

static unsigned long g2d_vb2_plane_addr(struct vb2_buffer *vb, u32 plane_no)
{
	void *cookie = vb2_plane_cookie(vb, plane_no);
	dma_addr_t dma_addr = 0;

	WARN_ON(vb2_ion_dma_address(cookie, &dma_addr) != 0);

	return (unsigned long)dma_addr;
}

const struct g2d_vb2 g2d_vb2_ion = {
	.ops		= &vb2_ion_memops,
	.init		= g2d_ion_init,
	.cleanup	= vb2_ion_destroy_context,
	.plane_addr	= g2d_vb2_plane_addr,
	.resume		= vb2_ion_attach_iommu,
	.suspend	= vb2_ion_detach_iommu,
	.set_cacheable	= vb2_ion_set_cached,
};
#endif
