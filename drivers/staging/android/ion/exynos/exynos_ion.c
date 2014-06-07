/*
 * drivers/gpu/exynos/exynos_ion.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/exynos_ion.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/dma-contiguous.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/bitops.h>
#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/plist.h>

#include <asm/pgtable.h>

#include "../ion_priv.h"

struct ion_device *ion_exynos;

static int num_heaps;
static struct ion_heap **heaps;

/* IMBUFS stands for "InterMediate BUFfer Storage" */
#define IMBUFS_SHIFT	6
#define IMBUFS_ENTRIES	(1 << IMBUFS_SHIFT)
#define IMBUFS_MASK	(IMBUFS_ENTRIES - 1)	/* masking lower bits */
#define MAX_LV0IMBUFS	IMBUFS_ENTRIES
#define MAX_LV1IMBUFS	(IMBUFS_ENTRIES + IMBUFS_ENTRIES * IMBUFS_ENTRIES)
#define MAX_IMBUFS	(MAX_LV1IMBUFS + (IMBUFS_ENTRIES << (IMBUFS_SHIFT * 2)))

#define LV1IDX(lv1base)		((lv1base) >> IMBUFS_SHIFT)
#define LV2IDX1(lv2base)	((lv2base) >> (IMBUFS_SHIFT * 2))
#define LV2IDX2(lv2base)	(((lv2base) >> (IMBUFS_SHIFT)) & IMBUFS_MASK)

static int orders[] = {PAGE_SHIFT + 8, PAGE_SHIFT + 4, PAGE_SHIFT, 0};

static inline phys_addr_t *get_imbufs_and_free(int idx,
		phys_addr_t *lv0imbufs, phys_addr_t **lv1pimbufs,
		phys_addr_t ***lv2ppimbufs)
{
	if (idx < MAX_LV0IMBUFS) {
		return lv0imbufs;
	} else if (idx < MAX_LV1IMBUFS) {
		phys_addr_t *imbufs;
		idx -= MAX_LV0IMBUFS;
		imbufs = lv1pimbufs[LV1IDX(idx)];
		if ((LV1IDX(idx) == (IMBUFS_ENTRIES - 1)) ||
			(lv1pimbufs[LV1IDX(idx) + 1] == NULL))
			kfree(lv1pimbufs);
		return imbufs;
	} else if (idx < MAX_IMBUFS) {
		int baseidx;
		phys_addr_t *imbufs;
		baseidx = idx - MAX_LV1IMBUFS;
		imbufs = lv2ppimbufs[LV2IDX1(baseidx)][LV2IDX2(baseidx)];
		if ((LV2IDX2(baseidx) == (IMBUFS_ENTRIES - 1)) ||
			(lv2ppimbufs[LV2IDX1(baseidx)][LV2IDX2(baseidx) + 1]
				== NULL)) {
			kfree(lv2ppimbufs[LV2IDX1(baseidx)]);
			if ((LV2IDX1(baseidx) == (IMBUFS_ENTRIES - 1)) ||
				(lv2ppimbufs[LV2IDX1(baseidx) + 1] == NULL))
				kfree(lv2ppimbufs);
		}
		return imbufs;

	}
	return NULL;
}

static int ion_exynos_heap_allocate(struct ion_heap *heap,
		struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	int *cur_order = orders;
	int alloc_chunks = 0;
	int ret = 0;
	phys_addr_t *im_phys_bufs = NULL;
	phys_addr_t **pim_phys_bufs = NULL;
	phys_addr_t ***ppim_phys_bufs = NULL;
	phys_addr_t *cur_bufs = NULL;
	int copied = 0;
	struct scatterlist *sgl;
	struct sg_table *sgtable;

	while (size && *cur_order) {
		struct page *page;

		if (size < (1 << *cur_order)) {
			cur_order++;
			continue;
		}

		page = alloc_pages(GFP_HIGHUSER | __GFP_COMP |
						__GFP_NOWARN | __GFP_NORETRY,
						*cur_order - PAGE_SHIFT);
		if (!page) {
			cur_order++;
			continue;
		}

		if (alloc_chunks & IMBUFS_MASK) {
			cur_bufs++;
		} else if (alloc_chunks < MAX_LV0IMBUFS) {
			if (!im_phys_bufs)
				im_phys_bufs = kzalloc(
					sizeof(*im_phys_bufs) * IMBUFS_ENTRIES,
					GFP_KERNEL);
			if (!im_phys_bufs)
				break;

			cur_bufs = im_phys_bufs;
		} else if (alloc_chunks < MAX_LV1IMBUFS) {
			int lv1idx = LV1IDX(alloc_chunks - MAX_LV0IMBUFS);

			if (!pim_phys_bufs) {
				pim_phys_bufs = kzalloc(
					sizeof(*pim_phys_bufs) * IMBUFS_ENTRIES,
					GFP_KERNEL);
				if (!pim_phys_bufs)
					break;
			}

			if (!pim_phys_bufs[lv1idx]) {
				pim_phys_bufs[lv1idx] = kzalloc(
					sizeof(*cur_bufs) * IMBUFS_ENTRIES,
					GFP_KERNEL);
				if (!pim_phys_bufs[lv1idx])
					break;
			}

			cur_bufs = pim_phys_bufs[lv1idx];
		} else if (alloc_chunks < MAX_IMBUFS) {
			phys_addr_t **pcur_bufs;
			int lv2base = alloc_chunks - MAX_LV1IMBUFS;

			if (!ppim_phys_bufs) {
				ppim_phys_bufs = kzalloc(
					sizeof(*ppim_phys_bufs) * IMBUFS_ENTRIES
					, GFP_KERNEL);
				if (!ppim_phys_bufs)
					break;
			}

			if (!ppim_phys_bufs[LV2IDX1(lv2base)]) {
				ppim_phys_bufs[LV2IDX1(lv2base)] = kzalloc(
					sizeof(*pcur_bufs) * IMBUFS_ENTRIES,
					GFP_KERNEL);
				if (!ppim_phys_bufs[LV2IDX1(lv2base)])
					break;
			}
			pcur_bufs = ppim_phys_bufs[LV2IDX1(lv2base)];

			if (!pcur_bufs[LV2IDX2(lv2base)]) {
				pcur_bufs[LV2IDX2(lv2base)] = kzalloc(
					sizeof(*cur_bufs) * IMBUFS_ENTRIES,
					GFP_KERNEL);
				if (!pcur_bufs[LV2IDX2(lv2base)])
					break;
			}
			cur_bufs = pcur_bufs[LV2IDX2(lv2base)];
		} else {
			break;
		}

		*cur_bufs = page_to_phys(page) | *cur_order;

		size = size - (1 << *cur_order);
		alloc_chunks++;
	}

	if (size) {
		ret = -ENOMEM;
		goto alloc_error;
	}

	sgtable = kmalloc(sizeof(*sgtable), GFP_KERNEL);
	if (!sgtable) {
		ret = -ENOMEM;
		goto alloc_error;
	}

	if (sg_alloc_table(sgtable, alloc_chunks, GFP_KERNEL)) {
		ret = -ENOMEM;
		kfree(sgtable);
		goto alloc_error;
	}

	sgl = sgtable->sgl;
	while (copied < alloc_chunks) {
		int i;
		cur_bufs = get_imbufs_and_free(copied, im_phys_bufs,
						pim_phys_bufs, ppim_phys_bufs);
		BUG_ON(!cur_bufs);
		for (i = 0; (i < IMBUFS_ENTRIES) && cur_bufs[i]; i++) {
			phys_addr_t phys;
			int order;

			phys = cur_bufs[i];
			order = phys & ~PAGE_MASK;
			sg_set_page(sgl, phys_to_page(phys), 1 << order, 0);
			sgl = sg_next(sgl);
			copied++;
		}

		kfree(cur_bufs);
	}

	buffer->priv_virt = sgtable;
	buffer->flags = flags;

	return 0;
alloc_error:
	copied = 0;
	while (copied < alloc_chunks) {
		int i;
		cur_bufs = get_imbufs_and_free(copied, im_phys_bufs,
				pim_phys_bufs, ppim_phys_bufs);
		for (i = 0; (i < IMBUFS_ENTRIES) && cur_bufs[i]; i++) {
			phys_addr_t phys;
			int gfp_order;

			phys = cur_bufs[i];
			gfp_order = (phys & ~PAGE_MASK) - PAGE_SHIFT;
			phys = phys & PAGE_MASK;
			__free_pages(phys_to_page(phys), gfp_order);
		}

		kfree(cur_bufs);
		copied += IMBUFS_ENTRIES;
	}

	return ret;
}

static void ion_exynos_heap_free(struct ion_buffer *buffer)
{
	struct scatterlist *sg;
	int i;
	struct sg_table *sgtable = buffer->priv_virt;

	for_each_sg(sgtable->sgl, sg, sgtable->orig_nents, i)
		__free_pages(sg_page(sg), __ffs(sg_dma_len(sg)) - PAGE_SHIFT);

	sg_free_table(sgtable);
	kfree(sgtable);
}

static struct sg_table *ion_exynos_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_exynos_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
}

static void *ion_exynos_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	struct page **pages, **tmp_pages;
	struct sg_table *sgt;
	struct scatterlist *sgl;
	int num_pages, i;
	void *vaddr;
	pgprot_t pgprot;

	sgt = buffer->priv_virt;
	num_pages = PAGE_ALIGN(offset_in_page(sg_phys(sgt->sgl)) + buffer->size)
								>> PAGE_SHIFT;

	pages = vmalloc(sizeof(*pages) * num_pages);
	if (!pages)
		return ERR_PTR(-ENOMEM);

	tmp_pages = pages;
	for_each_sg(sgt->sgl, sgl, sgt->orig_nents, i) {
		struct page *page = sg_page(sgl);
		unsigned int n =
			PAGE_ALIGN(sgl->offset + sg_dma_len(sgl)) >> PAGE_SHIFT;

		for (; n > 0; n--)
			*(tmp_pages++) = page++;
	}

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	vaddr = vmap(pages, num_pages, VM_USERMAP | VM_MAP, pgprot);

	vfree(pages);

	if (vaddr == NULL)
		return ERR_PTR(-ENOMEM);

	return vaddr + offset_in_page(sg_phys(sgt->sgl));
}

static void ion_exynos_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
	struct sg_table *sgt = buffer->priv_virt;

	vunmap(buffer->vaddr - offset_in_page(sg_phys(sgt->sgl)));
}

static int ion_exynos_heap_map_user(struct ion_heap *heap,
			struct ion_buffer *buffer, struct vm_area_struct *vma)
{
	struct sg_table *sgt = buffer->priv_virt;
	struct scatterlist *sgl;
	unsigned long pgoff;
	int i;
	unsigned long start;
	int map_pages;

	if (buffer->kmap_cnt)
		return remap_vmalloc_range(vma, buffer->vaddr, vma->vm_pgoff);

	pgoff = vma->vm_pgoff;
	start = vma->vm_start;
	map_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;

	for_each_sg(sgt->sgl, sgl, sgt->orig_nents, i) {
		unsigned long sg_pgnum = sg_dma_len(sgl) >> PAGE_SHIFT;

		if (sg_pgnum <= pgoff) {
			pgoff -= sg_pgnum;
		} else {
			struct page *page = sg_page(sgl) + pgoff;
			int i;

			sg_pgnum -= pgoff;

			for (i = 0; (map_pages > 0) && (i < sg_pgnum); i++) {
				int ret;
				ret = vm_insert_page(vma, start, page);
				if (ret)
					return ret;
				start += PAGE_SIZE;
				page++;
				map_pages--;
			}

			pgoff = 0;

			if (map_pages == 0)
				break;
		}
	}

	return 0;
}

static struct ion_heap_ops vmheap_ops = {
	.allocate = ion_exynos_heap_allocate,
	.free = ion_exynos_heap_free,
	.map_dma = ion_exynos_heap_map_dma,
	.unmap_dma = ion_exynos_heap_unmap_dma,
	.map_kernel = ion_exynos_heap_map_kernel,
	.unmap_kernel = ion_exynos_heap_unmap_kernel,
	.map_user = ion_exynos_heap_map_user,
};

static struct ion_heap *ion_exynos_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &vmheap_ops;
	heap->type = ION_HEAP_TYPE_EXYNOS;
	return heap;
}

static void ion_exynos_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

struct ion_exynos_contig_heap {
	struct ion_heap heap;
	struct device *dev;		/* misc device of ION */
	unsigned int enabled_mask;	/* regions are available? */
};

#define MAX_CONTIG_NAME 11

struct ion_exynos_cmadata {
	int id;
	char name[MAX_CONTIG_NAME + 1];
};

static int ion_cma_device_name_match(struct device *dev, void *data)
{
	struct ion_exynos_cmadata *cmadata = dev_get_drvdata(dev);

	return (strncmp(data, cmadata->name, MAX_CONTIG_NAME) == 0) ? -1 : 0;
}

static int ion_exynos_dev_match(struct device *dev, void *data)
{
	return (dev_get_drvdata(dev) == data) ? -1 : 0;
}

unsigned int ion_exynos_contig_region_mask(char *region_name)
{
	struct device *dev_ion, *dev;
	struct ion_exynos_cmadata *cmadata;

	dev_ion = bus_find_device(&platform_bus_type,
				  NULL, ion_exynos, ion_exynos_dev_match);
	if (!dev_ion) {
		pr_err("%s: Unable to find device for ION\n", __func__);
		return EXYNOS_CONTIG_REGION_NOMASK;
	}

	dev = device_find_child(dev_ion, region_name,
				ion_cma_device_name_match);
	if (!dev) {
		pr_err("%s: Unable to find CMA region '%s'\n",
			__func__, region_name);
		return EXYNOS_CONTIG_REGION_NOMASK;
	}

	cmadata = dev_get_drvdata(dev);

	return MAKE_CONTIG_FLAG(cmadata->id);
}
EXPORT_SYMBOL(ion_exynos_contig_region_mask);

static int ion_cma_device_id_match(struct device *dev, void *data)
{
	int *id = data;
	struct ion_exynos_cmadata *cmadata = dev_get_drvdata(dev);

	return (*id == cmadata->id) ? -1 : 0;
}

static struct device *ion_exynos_contig_dev(int region_id)
{
	struct device *dev;

	dev = bus_find_device(&platform_bus_type,
				NULL, ion_exynos, ion_exynos_dev_match);
	if (!dev) {
		pr_err("%s: Unable to find device for ION\n", __func__);
		return NULL;
	}

	dev = device_find_child(dev, &region_id, ion_cma_device_id_match);
	if (!dev) {
		pr_err("%s: Unable to find contiguous region of ID %d\n",
			__func__, region_id);
		return NULL;
	}

	return dev;
}

int ion_exynos_contig_heap_info(int region_id, phys_addr_t *phys, size_t *size)
{
	struct device *dev;
	struct cma_info info;
	struct ion_exynos_cmadata *cmadata;

	dev = ion_exynos_contig_dev(region_id);
	if (!dev)
		return -ENODEV;

	if (dma_contiguous_info(dev, &info)) {
		cmadata = dev_get_drvdata(dev);
		pr_err("%s: Unable to find region, '%s(%d)' from CMA\n",
			__func__, cmadata->name, region_id);
		return -ENODEV;
	}

	if (phys)
		*phys = info.base;
	if (size)
		*size = info.size;

	return 0;
}
EXPORT_SYMBOL(ion_exynos_contig_heap_info);

int ion_exynos_contig_heap_isolate(int region_id)
{
	struct device *dev = ion_exynos_contig_dev(region_id);
	if (!dev)
		return -ENODEV;

	return dma_contiguous_isolate(dev);
}

void ion_exynos_contig_heap_deisolate(int region_id)
{
	struct device *dev = ion_exynos_contig_dev(region_id);
	if (dev)
		dma_contiguous_deisolate(dev);
}

static int ion_exynos_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	int id = MAKE_CONTIG_ID(flags);
	struct ion_exynos_contig_heap *contig_heap =
			container_of(heap, struct ion_exynos_contig_heap, heap);
	struct device *dev;
	struct page **pages;
	size_t pages_size = (len / PAGE_SIZE) * sizeof(struct page *);
	size_t cur;
	void *p;
	int ret = 0;

	buffer->flags = flags;

	/* fixup of old DRM flags */
	if (flags & (ION_EXYNOS_FIMD_VIDEO_MASK | ION_EXYNOS_MFC_OUTPUT_MASK))
		id = ION_EXYNOS_ID_VIDEO;

	dev = device_find_child(contig_heap->dev, &id, ion_cma_device_id_match);
	if (!dev) {
		pr_err("%s: Unable to find contiguous region for flag %#lx\n",
			__func__, flags);
		return -EINVAL;
	}

	/* In order to avoid passing '0' to get_order() */
	if (!align)
		align = PAGE_SIZE;

	buffer->priv_virt = dma_alloc_from_contiguous(dev, len >> PAGE_SHIFT,
					      get_order(align));
	if (buffer->priv_virt == NULL) {
		struct cma_info info;
		pr_err("%s: Failed to allocate %#lx from '%s'\n",
			__func__, len, dev_name(dev));

		if (dma_contiguous_info(dev, &info) == 0)
			pr_err("%s:     base: %#x, size: %#x, free: %#x\n",
				__func__, info.base, info.size, info.free);
		else
			WARN(1, "%s: No contiguous region is declared for %s\n",
				__func__, dev_name(dev));
		return -ENOMEM;
	}

	if (pages_size > PAGE_SIZE)
		pages = vmalloc(pages_size);
	else
		pages = (struct page **)__get_free_page(GFP_KERNEL);

	if (!pages) {
		pr_err("%s: Failed to allocate page descriptors\n",
			__func__);
		ret = -ENOMEM;
		goto err_alloc_pages;
	}

	for (cur = 0; cur < (pages_size / sizeof(struct page *)); cur++)
		pages[cur] = (struct page *)buffer->priv_virt + cur;

	p = vmap(pages, cur, VM_MAP, PAGE_KERNEL);
	if (!p) {
		pr_err("%s: Failed to vmap to clean allocated buffer\n",
			__func__);
		ret = -ENOMEM;
		goto err_vmap;
	}

	if (!(flags & ION_FLAG_NOZEROED))
		memset(p, 0, len);

	dmac_flush_range(p, p + len);

	vunmap(p);

	ion_buffer_set_ready(buffer);
	ion_buffer_set_clean(buffer);

err_vmap:
	if (pages_size > PAGE_SIZE)
		vfree(pages);
	else
		free_pages((unsigned long)pages, 0);

err_alloc_pages:
	if (ret)
		dma_release_from_contiguous(dev,
				buffer->priv_virt, buffer->size >> PAGE_SHIFT);

	return ret;
}

static void ion_exynos_contig_heap_free(struct ion_buffer *buffer)
{
	int id = MAKE_CONTIG_ID(buffer->flags);
	struct ion_exynos_contig_heap *contig_heap =
		container_of(buffer->heap, struct ion_exynos_contig_heap, heap);
	struct device *dev;

	/* fixup of old DRM flags */
	if (buffer->flags & (ION_EXYNOS_FIMD_VIDEO_MASK | ION_EXYNOS_MFC_OUTPUT_MASK))
		id = ION_EXYNOS_ID_VIDEO;

	dev = device_find_child(contig_heap->dev, &id, ion_cma_device_id_match);
	if (!dev) {
		pr_err("%s: Unable to find contiguous region for flag %#lx\n",
			__func__, buffer->flags);
		return;
	} else {
		bool ret = dma_release_from_contiguous(
			dev, buffer->priv_virt, buffer->size >> PAGE_SHIFT);
		WARN(!ret, "%s: %#x@%#x is not allocated region by %s\n",
			__func__, buffer->size,
			page_to_phys((struct page *)buffer->priv_virt),
			dev_name(dev));
	}
}

static int ion_exynos_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	*addr = page_to_phys((struct page *)buffer->priv_virt);
	*len = buffer->size;
	return 0;
}

static struct sg_table *ion_exynos_contig_heap_map_dma(struct ion_heap *heap,
						   struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, buffer->priv_virt, buffer->size, 0);
	return table;
}

static void ion_exynos_contig_heap_unmap_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
	if (buffer->sg_table)
		sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static int ion_exynos_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma)
{
	unsigned long pfn = page_to_pfn((struct page *)buffer->priv_virt);

	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);

}

static void *ion_exynos_contig_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	int i;
	pgprot_t pgprot;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < npages; i++)
		pages[i] = (struct page *)buffer->priv_virt + i;

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	buffer->vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (buffer->vaddr == NULL)
		return ERR_PTR(-ENOMEM);

	return buffer->vaddr;
}

static void ion_exynos_contig_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
	vunmap(buffer->vaddr);
}

static struct ion_heap_ops contig_heap_ops = {
	.allocate = ion_exynos_contig_heap_allocate,
	.free = ion_exynos_contig_heap_free,
	.phys = ion_exynos_contig_heap_phys,
	.map_dma = ion_exynos_contig_heap_map_dma,
	.unmap_dma = ion_exynos_contig_heap_unmap_dma,
	.map_kernel = ion_exynos_contig_heap_map_kernel,
	.unmap_kernel = ion_exynos_contig_heap_unmap_kernel,
	.map_user = ion_exynos_contig_heap_map_user,
};

static int ion_exynos_contig_region_show(struct device *dev, void *data)
{
	struct seq_file *s = data;
	struct cma_info info;
	struct ion_exynos_cmadata *cmadata = dev_get_drvdata(dev);

	if (dma_contiguous_info(dev, &info) != 0)
		return -EINVAL;

	seq_printf(s, "[%2.d] %11.s %#18.x %#10.x %#10.x\n",
			cmadata->id, cmadata->name,
			info.base, info.size, info.free);

	return 0;
}

static int ion_exynos_contig_heap_debug_show(struct ion_heap *heap,
					     struct seq_file *s,
					     void *unused)
{
	struct ion_exynos_contig_heap *contig_heap =
			container_of(heap, struct ion_exynos_contig_heap, heap);
	int ret;

	seq_printf(s, "\nexynos-ion contiguous regions:\n");
	seq_printf(s, " id  %11.s   %16.s %10.s %10.s\n",
			"name", "base", "size", "free");
	seq_printf(s,
		"----------------------------------------------------------\n");

	ret = device_for_each_child(contig_heap->dev, s,
				    ion_exynos_contig_region_show);
	if (ret)
		pr_err("%s: Error while retrieving contiguous regions\n",
			__func__);

	return 0;
}

static struct ion_exynos_contig_heap *__init_contig_heap __initdata;

static struct ion_heap *ion_exynos_contig_heap_create(struct device *dev)
{
	__init_contig_heap = kzalloc(sizeof(*__init_contig_heap), GFP_KERNEL);
	if (!__init_contig_heap)
		return ERR_PTR(-ENOMEM);

	__init_contig_heap->heap.ops = &contig_heap_ops;
	__init_contig_heap->heap.type = ION_HEAP_TYPE_EXYNOS_CONTIG;
	__init_contig_heap->heap.debug_show = ion_exynos_contig_heap_debug_show;
	__init_contig_heap->dev = dev;

	return &__init_contig_heap->heap;
}

static void ion_exynos_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(container_of(heap, struct ion_exynos_contig_heap, heap));
}

static struct ion_heap *__ion_heap_create(struct ion_platform_heap *heap_data,
					  struct device *dev)
{
	struct ion_heap *heap = NULL;

	switch ((int)heap_data->type) {
	case ION_HEAP_TYPE_EXYNOS:
		heap = ion_exynos_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_EXYNOS_CONTIG:
		heap = ion_exynos_contig_heap_create(dev);
		break;
	default:
		return ion_heap_create(heap_data);
	}

	if (IS_ERR_OR_NULL(heap)) {
		pr_err("%s: error creating heap %s type %d base %lu size %u\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		return ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;

	ion_heap_reserve_vm(heap);

	return heap;
}

void __ion_heap_destroy(struct ion_heap *heap)
{
	if (!heap)
		return;

	switch ((int)heap->type) {
	case ION_HEAP_TYPE_EXYNOS:
		free_vm_area(heap->reserved_vm_area);
		ion_exynos_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_EXYNOS_CONTIG:
		free_vm_area(heap->reserved_vm_area);
		ion_exynos_contig_heap_destroy(heap);
		break;
	default:
		ion_heap_destroy(heap);
	}
}

void exynos_ion_sync_dmabuf_for_device(struct device *dev,
					struct dma_buf *dmabuf,
					size_t size,
					enum dma_data_direction dir)
{
	struct ion_buffer *buffer = (struct ion_buffer *) dmabuf->priv;

	if (dir == DMA_COHERENT)
		return;

	if (IS_ERR_OR_NULL(buffer))
		BUG();

	mutex_lock(&buffer->lock);
	ion_buffer_make_ready(buffer);

	if (ion_buffer_fault_user_mappings(buffer))
		goto out;

	if (!ion_buffer_dirty(buffer))
		goto out;

	pr_debug("%s: syncing for device %s, buffer: %p, size: %d\n",
			__func__, dev ? dev_name(dev) : "null", buffer, size);

	if (ion_buffer_need_flush_all(buffer)) {
		flush_all_cpu_caches();
	} else if (!IS_ERR_OR_NULL(buffer->vaddr)) {
		dmac_map_area(buffer->vaddr, size, dir);
	} else if (buffer->heap->reserved_vm_area) {
		ion_heap_sync(buffer->heap, buffer->sg_table,
					dir, dmac_map_area, false);
	} else {
		dma_sync_sg_for_device(dev, buffer->sg_table->sgl,
					buffer->sg_table->orig_nents, dir);
	}

out:
	mutex_unlock(&buffer->lock);
}
EXPORT_SYMBOL(exynos_ion_sync_dmabuf_for_device);

void exynos_ion_sync_vaddr_for_device(struct device *dev,
					void *vaddr,
					size_t size,
					off_t offset,
					enum dma_data_direction dir)
{
	if (dir == DMA_COHERENT)
		return;

	pr_debug("%s: syncing for device %s, vaddr: %p, size: %d, offset: %ld\n",
			__func__, dev ? dev_name(dev) : "null",
			vaddr, size, offset);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		flush_all_cpu_caches();
	else if (!IS_ERR_OR_NULL(vaddr))
		dmac_map_area(vaddr + offset, size, dir);
	else
		BUG();
}
EXPORT_SYMBOL(exynos_ion_sync_vaddr_for_device);

void exynos_ion_sync_sg_for_device(struct device *dev,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	if (dir == DMA_COHERENT)
		return;

	dma_sync_sg_for_device(dev, sgt->sgl, sgt->orig_nents, dir);
}
EXPORT_SYMBOL(exynos_ion_sync_sg_for_device);

void exynos_ion_sync_dmabuf_for_cpu(struct device *dev,
					struct dma_buf *dmabuf,
					size_t size,
					enum dma_data_direction dir)
{
	struct ion_buffer *buffer = (struct ion_buffer *) dmabuf->priv;

	if (dir == DMA_COHERENT)
		return;

	if (IS_ERR_OR_NULL(buffer))
		BUG();

	mutex_lock(&buffer->lock);
	ion_buffer_make_ready(buffer);

	if (ion_buffer_fault_user_mappings(buffer))
		goto out;

	if (!ion_buffer_dirty(buffer))
		goto out;

	pr_debug("%s: syncing for cpu %s, buffer: %p, size: %d\n",
			__func__, dev ? dev_name(dev) : "null", buffer, size);

	if (ion_buffer_need_flush_all(buffer)) {
		flush_all_cpu_caches();
	} else if (!IS_ERR_OR_NULL(buffer->vaddr)) {
		dmac_unmap_area(buffer->vaddr, size, dir);
	} else if (buffer->heap->reserved_vm_area) {
		ion_heap_sync(buffer->heap, buffer->sg_table,
					dir, dmac_unmap_area, false);
	} else {
		dma_sync_sg_for_cpu(dev, buffer->sg_table->sgl,
					buffer->sg_table->orig_nents, dir);
	}

	if (!ion_buffer_cached(buffer) || !ion_buffer_cpumapped(buffer)) {
		pr_debug("%s: set clean for buffer %p, cached: %d, "
				"mapped: %d\n", __func__,
				buffer, ion_buffer_cached(buffer),
				ion_buffer_cpumapped(buffer));
		ion_buffer_set_clean(buffer);
	}

out:
	mutex_unlock(&buffer->lock);
}
EXPORT_SYMBOL(exynos_ion_sync_dmabuf_for_cpu);

void exynos_ion_sync_vaddr_for_cpu(struct device *dev,
					void *vaddr,
					size_t size,
					off_t offset,
					enum dma_data_direction dir)
{
	if (dir == DMA_COHERENT)
		return;

	pr_debug("%s: syncing for cpu %s, vaddr: %p, size: %d, offset: %ld\n",
			__func__, dev ? dev_name(dev) : "null",
			vaddr, size, offset);

	if (size >= ION_FLUSH_ALL_HIGHLIMIT)
		flush_all_cpu_caches();
	else if (!IS_ERR_OR_NULL(vaddr))
		dmac_unmap_area(vaddr + offset, size, dir);
	else
		BUG();
}
EXPORT_SYMBOL(exynos_ion_sync_vaddr_for_cpu);

void exynos_ion_sync_sg_for_cpu(struct device *dev,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	if (dir == DMA_COHERENT)
		return;

	dma_sync_sg_for_cpu(dev, sgt->sgl, sgt->orig_nents, dir);
}
EXPORT_SYMBOL(exynos_ion_sync_sg_for_cpu);

#ifdef CONFIG_ION_EXYNOS_OF
static int exynos_ion_populate_heaps(struct platform_device *pdev,
				     struct ion_device *ion_dev)
{
	struct ion_platform_heap heap_data;
	struct device_node *np = NULL;
	int i = 0, ret;

	num_heaps = of_get_child_count(pdev->dev.of_node);

	heaps = kzalloc(sizeof(struct ion_heap *) * num_heaps, GFP_KERNEL);
	if (!heaps)
		return -ENOMEM;

	for_each_child_of_node(pdev->dev.of_node, np) {
		heap_data.name = np->name;

		ret = of_property_read_u32_index(np, "id-type", 0,
						 &heap_data.id);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to get id of '%s': skip\n",
				__func__, heap_data.name);
			continue;
		}

		ret = of_property_read_u32_index(np, "id-type", 1,
						 &heap_data.type);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to get type of '%s': skip\n",
				__func__, heap_data.name);
			continue;
		}

		BUG_ON(i >= num_heaps);

		heaps[i] = __ion_heap_create(&heap_data, &pdev->dev);
		if (IS_ERR_OR_NULL(heaps[i])) {
			ret = PTR_ERR(heaps[i]);
			goto err;
		}

		ion_device_add_heap(ion_exynos, heaps[i]);

		i++;
	}

	return 0;
err:
	while (i-- > 0)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);

	return ret;
}
#else /* !CONFIG_ION_EXYNOS_OF */
static int exynos_ion_populate_heaps(struct platform_device *pdev,
				     struct ion_device *ion_dev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;
	int i;

	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);
	if (!heaps)
		return -ENOMEM;

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = __ion_heap_create(heap_data, &pdev->dev);
		if (IS_ERR_OR_NULL(heaps[i])) {
			ret = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(ion_exynos, heaps[i]);
	}

	return 0;
err:
	while (i-- > 0)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);

	return ret;
}
#endif /* CONFIG_ION_EXYNOS_OF */

static int __init exynos_ion_probe(struct platform_device *pdev)
{
	ion_exynos = ion_device_create(NULL);
	if (IS_ERR_OR_NULL(ion_exynos)) {
		kfree(heaps);
		return PTR_ERR(ion_exynos);
	}

	platform_set_drvdata(pdev, ion_exynos);

	return exynos_ion_populate_heaps(pdev, ion_exynos);
}

static int __exit exynos_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		__ion_heap_destroy(heaps[i]);
	kfree(heaps);
	return 0;
}

#ifdef CONFIG_ION_EXYNOS_OF
static struct of_device_id exynos_ion_of_match[] __initconst = {
	{ .compatible	= "samsung,exynos5430-ion", },
	{ },
};

static int ion_device_register(void)
{
	return 0;
}
#else
static struct ion_platform_heap exynos_ion_heaps[] __initconst = {
	{	.type = ION_HEAP_TYPE_SYSTEM,
		.name = "ion_noncontig_heap",
		.id = EXYNOS_ION_HEAP_SYSTEM_ID,
	},
	{	.type = ION_HEAP_TYPE_EXYNOS,
		.name = "exynos_noncontig_heap",
		.id = EXYNOS_ION_HEAP_EXYNOS_ID,
	},
	{	.type = ION_HEAP_TYPE_EXYNOS_CONTIG,
		.name = "exynos_contig_heap",
		.id = EXYNOS_ION_HEAP_EXYNOS_CONTIG_ID,
	},
};

static struct ion_platform_data exynos_ion_pdata __initconst = {
	.nr = ARRAY_SIZE(exynos_ion_heaps),
	.heaps = exynos_ion_heaps,
};

struct platform_device exynos_device_ion __refdata = {
	.name		= "ion-exynos",
	.id		= -1,
	.dev		= {
		.platform_data = &exynos_ion_pdata,
	}
};

static int ion_device_register(void)
{
	return platform_device_register(&exynos_device_ion);
}
#endif	/* CONFIG_ION_EXYNOS_OF */

static struct platform_driver ion_driver __refdata = {
	.probe	= exynos_ion_probe,
	.remove	= exynos_ion_remove,
	.driver	= {
		.owner		= THIS_MODULE,
		.name		= "ion-exynos",
#ifdef CONFIG_ION_EXYNOS_OF
		.of_match_table	= of_match_ptr(exynos_ion_of_match),
#endif
	}
};

static int __init ion_init(void)
{
	int ret = ion_device_register();
	if (ret) {
		pr_err("ION: Failed to register ION platform device\n");
		return ret;
	}

	return platform_driver_register(&ion_driver);
}

subsys_initcall(ion_init);

struct exynos_ion_contig_region {
	char name[MAX_CONTIG_NAME + 1];
	int id;
	size_t size;
	phys_addr_t base;
	struct device dev;
};

static int contig_region_cursor __initdata;

#ifdef CONFIG_ION_EXYNOS_OF
static struct exynos_ion_contig_region
	exynos_ion_contig_region[EXYNOS_ION_CONTIG_ID_NUM] __initdata;

static int __init __fdt_init_exynos_ion(unsigned long node, const char *uname,
				      int depth, void *data)
{
	__be32 *prop;
	char *pch, *cch;
	unsigned long len = 0;
	int i = 0;

	if (!of_flat_dt_is_compatible(node, "samsung,exynos5430-ion"))
		return 0;

	prop = of_get_flat_dt_prop(node, "contig-names", &len);
	if (!prop)
		return 0;

	pch = (char *)prop;
	cch = pch + len;
	/* NULL separated list of character strings */
	while (pch != cch) {
		strncpy(exynos_ion_contig_region[i++].name,
			pch, MAX_CONTIG_NAME);
		if (i == EXYNOS_ION_CONTIG_ID_NUM) {
			pr_err("%s: Too many region names\n", __func__);
			return 0;
		}
		pch += strlen(pch) + 1; /* number of char plus NULL */
	}

	prop = of_get_flat_dt_prop(node, "contig", &len);

	/* <id size base> */
	if (!prop || (len != (unsigned long)i * 3 * sizeof(long))) {
		pr_err("%s: Different number in 'contig-names' and 'contig'!\n",
			__func__);
		return 0;
	}

	len /= sizeof(long);
	for (i = 0; (unsigned long)i < len; i += 3) {
		if (be32_to_cpu(prop[i]) >= EXYNOS_ION_CONTIG_ID_NUM) {
			pr_err("%s: Too big region ID %d\n",
				__func__, be32_to_cpu(prop[i]));
			return 0;
		}

		exynos_ion_contig_region[contig_region_cursor].id =
						be32_to_cpu(prop[i]);
		exynos_ion_contig_region[contig_region_cursor].size =
						be32_to_cpu(prop[i + 1]);
		exynos_ion_contig_region[contig_region_cursor].base =
						be32_to_cpu(prop[i + 2]);
		pr_info("ION: Contiguous %#x bytes @ %#x defined for %d:%s\n",
			exynos_ion_contig_region[contig_region_cursor].size,
			exynos_ion_contig_region[contig_region_cursor].base,
			exynos_ion_contig_region[contig_region_cursor].id,
			exynos_ion_contig_region[contig_region_cursor].name);

		contig_region_cursor++;
	}

	return 0;
}

int __init init_exynos_ion_contig_heap(void)
{
	if (!of_scan_flat_dt(__fdt_init_exynos_ion, NULL) &&
					(contig_region_cursor > 0)) {
		int i, ret;
		for (i = 0; i < contig_region_cursor; i++) {
			ret = dma_declare_contiguous(
				&exynos_ion_contig_region[i].dev,
				exynos_ion_contig_region[i].size,
				exynos_ion_contig_region[i].base,
				-1);
			if (ret)
				break;
		}

		if (i < contig_region_cursor) {
			pr_err("%s: Failed to reserve %#x bytes for %s\n",
				__func__,
				exynos_ion_contig_region[i].size,
				exynos_ion_contig_region[i].name);
			return ret;
		}
	}

	return 0;
}
#else /* !CONFIG_ION_EXYNOS_OF */
static struct exynos_ion_contig_region
		exynos_ion_contig_region[EXYNOS_ION_CONTIG_ID_NUM] __initconst =
{
#if defined(CONFIG_ION_EXYNOS_SIZE_COMMON) &&  \
	(CONFIG_ION_EXYNOS_SIZE_COMMON > 0)
	{
		.name = "common",
		.id = ION_EXYNOS_ID_COMMON,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_COMMON * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_MFC_SH) &&  \
	(CONFIG_ION_EXYNOS_SIZE_MFC_SH > 0)
	{
		.name = "mfc_sh",
		.id = ION_EXYNOS_ID_MFC_SH,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_MFC_SH * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_MSGBOX_SH) &&  \
	(CONFIG_ION_EXYNOS_SIZE_MSGBOX_SH > 0)
	{
		.name = "msgbox_sh",
		.id = ION_EXYNOS_ID_MSGBOX_SH,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_MSGBOX_SH * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_FIMD_VIDEO) &&  \
	(CONFIG_ION_EXYNOS_SIZE_FIMD_VIDEO > 0)
	{
		.name = "fimd_video",
		.id = ION_EXYNOS_ID_FIMD_VIDEO,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_FIMD_VIDEO * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_GSC) &&  \
	(CONFIG_ION_EXYNOS_SIZE_GSC > 0)
	{
		.name = "gsc",
		.id = ION_EXYNOS_ID_GSC,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_GSC * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_MFC_OUTPUT) &&  \
	(CONFIG_ION_EXYNOS_SIZE_MFC_OUTPUT > 0)
	{
		.name = "mfc_output",
		.id = ION_EXYNOS_ID_MFC_OUTPUT,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_MFC_OUTPUT * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_MFC_INPUT) &&  \
	(CONFIG_ION_EXYNOS_SIZE_MFC_INPUT > 0)
	{
		.name = "mfc_input",
		.id = ION_EXYNOS_ID_MFC_INPUT,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_MFC_INPUT * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_MFC_FW) &&	\
	(CONFIG_ION_EXYNOS_SIZE_MFC_FW > 0)
	{
		.name = "mfc_fw",
		.id = ION_EXYNOS_ID_MFC_FW,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_MFC_FW * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_SECTBL) &&	\
	(CONFIG_ION_EXYNOS_SIZE_SECTBL > 0)
	{
		.name = "sectbl",
		.id = ION_EXYNOS_ID_SECTBL,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_SECTBL * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_G2D_WFD) &&	\
	(CONFIG_ION_EXYNOS_SIZE_G2D_WFD > 0)
	{
		.name = "g2d_wfd",
		.id = ION_EXYNOS_ID_G2D_WFD,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_G2D_WFD * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_VIDEO) &&	\
	(CONFIG_ION_EXYNOS_SIZE_VIDEO > 0)
	{
		.name = "video",
		.id = ION_EXYNOS_ID_VIDEO,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_VIDEO * SZ_1K),
	},
#endif
#if defined(CONFIG_ION_EXYNOS_SIZE_MFC_NFW) &&	\
	(CONFIG_ION_EXYNOS_SIZE_MFC_NFW > 0)
	{
		.name = "mfc_nfw",
		.id = ION_EXYNOS_ID_MFC_NFW,
		.size = PAGE_ALIGN(CONFIG_ION_EXYNOS_SIZE_MFC_NFW * SZ_1K),
	},
#endif
};

int __init init_exynos_ion_contig_heap(void)
{
	int i, ret;

	for (i = 0; i < EXYNOS_ION_CONTIG_ID_NUM; i++) {
		if (exynos_ion_contig_region[i].size == 0)
			break;

		ret = dma_declare_contiguous(&exynos_ion_contig_region[i].dev,
					     exynos_ion_contig_region[i].size,
					     exynos_ion_contig_region[i].base,
					     -1);
		if (ret) {
			pr_err("%s: Failed to reserve %#x bytes for %s\n",
				__func__,
				exynos_ion_contig_region[i].size,
				exynos_ion_contig_region[i].name);
			return ret;
		}

		contig_region_cursor++;
	}

	return 0;
}
#endif	/* CONFIG_ION_EXYNOS_OF */
static struct class *ion_cma_class;

static ssize_t region_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ion_exynos_cmadata *cmadata = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n", cmadata->name);
}

static ssize_t region_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ion_exynos_cmadata *cmadata = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u\n", cmadata->id);
}

static struct device_attribute cma_regname_attr = __ATTR_RO(region_name);
static struct device_attribute cma_regid_attr = __ATTR_RO(region_id);

static int __init ion_exynos_contigheap_init(void)
{
	int i;

	if (__init_contig_heap == NULL)
		return 0;

	ion_cma_class = class_create(THIS_MODULE, "ion_cma");
	if (IS_ERR(ion_cma_class)) {
		pr_err("%s: failed to create 'ion_cma' class\n", __func__);
		return PTR_ERR(ion_cma_class);
	}

	for (i = 0; i < contig_region_cursor; i++) {
		struct device *dev;
		struct ion_exynos_cmadata *drvdata;
		int ret;

		if ((exynos_ion_contig_region[i].dev.cma_area == NULL) ||
			(exynos_ion_contig_region[i].size == 0))
			continue;

		drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
		if (!drvdata) {
			pr_err("%s: failed to allocate cma device data\n",
				__func__);
			return -ENOMEM;
		}

		drvdata->id = exynos_ion_contig_region[i].id;
		strncpy(drvdata->name, exynos_ion_contig_region[i].name,
			MAX_CONTIG_NAME);

		dev = device_create(ion_cma_class,
				__init_contig_heap->dev, 0, drvdata,
				"ion_%s", exynos_ion_contig_region[i].name);
		if (IS_ERR(dev)) {
			kfree(drvdata);
			pr_err("%s: failed to create device of '%s'\n",
				__func__, exynos_ion_contig_region[i].name);
			continue;
		}

		dev_dbg(dev, "%s: Registered (region %d)\n",
			__func__, drvdata->id);

		dev->cma_area = exynos_ion_contig_region[i].dev.cma_area;

		ret = device_create_file(dev, &cma_regid_attr);
		if (ret)
			dev_err(dev, "%s: Failed to create '%s' file. (%d)\n",
				__func__, cma_regid_attr.attr.name, ret);

		ret = device_create_file(dev, &cma_regname_attr);
		if (ret)
			dev_err(dev, "%s: Failed to create '%s' file. (%d)\n",
				__func__, cma_regname_attr.attr.name, ret);
	}

	return 0;
}
subsys_initcall_sync(ion_exynos_contigheap_init);
