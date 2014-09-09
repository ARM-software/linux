/* linux/drivers/media/video/videobuf2-ion.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Implementation of Android ION memory allocator for videobuf2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/dma-buf.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-ion.h>

#include <linux/exynos_iovmm.h>
#include <plat/cpu.h>

struct vb2_ion_context {
	struct device		*dev;
	struct ion_client	*client;
	unsigned long		alignment;
	long			flags;

	/* protects iommu_active_cnt and protected */
	struct mutex		lock;
	int			iommu_active_cnt;
	bool			protected;
};

struct vb2_ion_buf {
	struct vb2_ion_context		*ctx;
	struct vb2_vmarea_handler	handler;
	struct vm_area_struct		*vma;
	struct ion_handle		*handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	enum dma_data_direction		direction;
	void				*kva;
	unsigned long			size;
	atomic_t			ref;
	bool				cached;
	struct vb2_ion_cookie		cookie;
};

#define CACHE_FLUSH_ALL_SIZE	SZ_8M
#define DMA_SYNC_SIZE		SZ_512K
#define OUTER_FLUSH_ALL_SIZE	SZ_1M

#define ctx_cached(ctx) (!(ctx->flags & VB2ION_CTX_UNCACHED))
#define ctx_iommu(ctx) (!!(ctx->flags & VB2ION_CTX_IOMMU))
#define ctx_coherent(ctx) (!!(ctx->flags & VB2ION_CTX_COHERENT))
#define need_kaddr(ctx, size, cached)					\
			(!!(ctx->flags & VB2ION_CTX_KVA_STATIC)) ||	\
			(!(ctx->flags & VB2ION_CTX_KVA_ONDEMAND) &&	\
			(size < CACHE_FLUSH_ALL_SIZE) && !!(cached))

void vb2_ion_set_cached(void *ctx, bool cached)
{
	struct vb2_ion_context *vb2ctx = ctx;

	if (cached)
		vb2ctx->flags &= ~VB2ION_CTX_UNCACHED;
	else
		vb2ctx->flags |= VB2ION_CTX_UNCACHED;
}
EXPORT_SYMBOL(vb2_ion_set_cached);

/*
 * when a context is protected, we cannot use the IOMMU since
 * secure world is in charge.
 */
void vb2_ion_set_protected(void *ctx, bool ctx_protected)
{
	struct vb2_ion_context *vb2ctx = ctx;

	mutex_lock(&vb2ctx->lock);

	if (vb2ctx->protected == ctx_protected)
		goto out;
	vb2ctx->protected = ctx_protected;

	if (ctx_protected) {
		if (vb2ctx->iommu_active_cnt) {
			dev_dbg(vb2ctx->dev, "detaching active MMU\n");
			iovmm_deactivate(vb2ctx->dev);
		}
	} else {
		if (vb2ctx->iommu_active_cnt) {
			int ret;
			dev_dbg(vb2ctx->dev, "re-attaching active MMU\n");
			ret = iovmm_activate(vb2ctx->dev);
			if (ret) {
				dev_err(vb2ctx->dev,
				"Failed to activate IOMMU with err %d\n", ret);
				BUG();
			}
		}
	}

out:
	mutex_unlock(&vb2ctx->lock);
}
EXPORT_SYMBOL(vb2_ion_set_protected);

int vb2_ion_set_alignment(void *ctx, size_t alignment)
{
	struct vb2_ion_context *vb2ctx = ctx;

	if ((alignment != 0) && (alignment < PAGE_SIZE))
		return -EINVAL;

	if (alignment & ~alignment)
		return -EINVAL;

	if (alignment == 0)
		vb2ctx->alignment = PAGE_SIZE;
	else
		vb2ctx->alignment = alignment;

	return 0;
}
EXPORT_SYMBOL(vb2_ion_set_alignment);

void *vb2_ion_create_context(struct device *dev, size_t alignment, long flags)
{
	struct vb2_ion_context *ctx;

	 /* non-contigous memory without H/W virtualization is not supported */
	if ((flags & VB2ION_CTX_VMCONTIG) && !(flags & VB2ION_CTX_IOMMU))
		return ERR_PTR(-EINVAL);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->dev = dev;
	ctx->client = ion_client_create(ion_exynos, dev_name(dev));
	if (IS_ERR(ctx->client)) {
		void *retp = ctx->client;
		kfree(ctx);
		return retp;
	}

	vb2_ion_set_alignment(ctx, alignment);
	ctx->flags = flags;
	mutex_init(&ctx->lock);

	return ctx;
}
EXPORT_SYMBOL(vb2_ion_create_context);

void vb2_ion_destroy_context(void *ctx)
{
	struct vb2_ion_context *vb2ctx = ctx;

	mutex_destroy(&vb2ctx->lock);
	ion_client_destroy(vb2ctx->client);
	kfree(vb2ctx);
}
EXPORT_SYMBOL(vb2_ion_destroy_context);

void *vb2_ion_private_alloc(void *alloc_ctx, size_t size, int write, int plane)
{
	struct vb2_ion_context *ctx = alloc_ctx;
	struct vb2_ion_buf *buf;
	int heapflags = ion_heapflag(ctx->flags);
	int flags = ion_flag(ctx->flags);
	int ret = 0;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	size = PAGE_ALIGN(size);

	flags |= ctx_cached(ctx) ? ION_FLAG_CACHED : 0;
	buf->handle = ion_alloc(ctx->client, size, ctx->alignment,
				heapflags, flags);
	if (IS_ERR(buf->handle)) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	buf->cookie.sgt = ion_sg_table(ctx->client, buf->handle);

	buf->ctx = ctx;
	buf->size = size;
	buf->cached = ctx_cached(ctx);
	buf->direction = write ? DMA_FROM_DEVICE: DMA_TO_DEVICE;

	if (need_kaddr(ctx, size, buf->cached)) {
		buf->kva = ion_map_kernel(ctx->client, buf->handle);
		if (IS_ERR_OR_NULL(buf->kva)) {
			ret = (buf->kva == NULL) ? -ENOMEM : PTR_ERR(buf->kva);
			buf->kva = NULL;
			goto err_map_kernel;
		}
	}

	mutex_lock(&ctx->lock);
	if (ctx_iommu(ctx) && !ctx->protected) {
		buf->cookie.ioaddr = iovmm_map(ctx->dev,
					       buf->cookie.sgt->sgl, 0,
					       buf->size, buf->direction, plane);
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			ret = (int)buf->cookie.ioaddr;
			mutex_unlock(&ctx->lock);
			goto err_ion_map_io;
		}
	}
	mutex_unlock(&ctx->lock);

	return &buf->cookie;

err_ion_map_io:
	if (buf->kva)
		ion_unmap_kernel(ctx->client, buf->handle);
err_map_kernel:
	ion_free(ctx->client, buf->handle);
err_alloc:
	kfree(buf);

	pr_err("%s: Error occured while allocating\n", __func__);
	return ERR_PTR(ret);
}

void vb2_ion_private_free(void *cookie)
{
	struct vb2_ion_buf *buf =
			container_of(cookie, struct vb2_ion_buf, cookie);
	struct vb2_ion_context *ctx;

	if (WARN_ON(IS_ERR_OR_NULL(cookie)))
		return;

	ctx = buf->ctx;
	mutex_lock(&ctx->lock);
	if (ctx_iommu(ctx) && !ctx->protected)
		iovmm_unmap(ctx->dev, buf->cookie.ioaddr);
	mutex_unlock(&ctx->lock);

	if (buf->kva)
		ion_unmap_kernel(ctx->client, buf->handle);
	ion_free(ctx->client, buf->handle);

	kfree(buf);
}

static void vb2_ion_put(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (atomic_dec_and_test(&buf->ref))
		vb2_ion_private_free(&buf->cookie);
}

static void *vb2_ion_alloc(void *alloc_ctx, unsigned long size, int write,
			   int plane, gfp_t gfp_flags)
{
	struct vb2_ion_buf *buf;
	void *cookie;

	cookie = vb2_ion_private_alloc(alloc_ctx, size, write, plane);
	if (IS_ERR(cookie))
		return cookie;

	buf = container_of(cookie, struct vb2_ion_buf, cookie);

	buf->handler.refcount = &buf->ref;
	buf->handler.put = vb2_ion_put;
	buf->handler.arg = buf;
	atomic_set(&buf->ref, 1);

	return buf;
}


void *vb2_ion_private_vaddr(void *cookie)
{
	struct vb2_ion_buf *buf =
			container_of(cookie, struct vb2_ion_buf, cookie);
	if (WARN_ON(IS_ERR_OR_NULL(cookie)))
		return NULL;

	if (!buf->kva) {
		buf->kva = ion_map_kernel(buf->ctx->client, buf->handle);
		if (IS_ERR_OR_NULL(buf->kva))
			buf->kva = NULL;

		buf->kva += buf->cookie.offset;
	}

	return buf->kva;
}

static void *vb2_ion_cookie(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return NULL;

	return (void *)&buf->cookie;
}

static void *vb2_ion_vaddr(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return NULL;

	if (buf->kva != NULL)
		return buf->kva;

	if (buf->handle)
		return vb2_ion_private_vaddr(&buf->cookie);

	if (dma_buf_begin_cpu_access(buf->dma_buf,
		0, buf->size, buf->direction))
		return NULL;

	buf->kva = dma_buf_kmap(buf->dma_buf, buf->cookie.offset / PAGE_SIZE);

	if (buf->kva == NULL)
		dma_buf_end_cpu_access(buf->dma_buf, 0,
			buf->size, buf->direction);
	else
		buf->kva += buf->cookie.offset & ~PAGE_MASK;

	return buf->kva;
}

static unsigned int vb2_ion_num_users(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return 0;

	return atomic_read(&buf->ref);
}

static int vb2_ion_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_ion_buf *buf = buf_priv;
	unsigned long vm_start = vma->vm_start;
	unsigned long vm_end = vma->vm_end;
	struct scatterlist *sg = buf->cookie.sgt->sgl;
	unsigned long size;
	int ret = -EINVAL;

	if (buf->size  < (vm_end - vm_start))
		return ret;

	if (!buf->cached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	size = min_t(size_t, vm_end - vm_start, sg_dma_len(sg));

	ret = remap_pfn_range(vma, vm_start, page_to_pfn(sg_page(sg)),
				size, vma->vm_page_prot);

	for (sg = sg_next(sg), vm_start += size;
			!ret && sg && (vm_start < vm_end);
			vm_start += size, sg = sg_next(sg)) {
		size = min_t(size_t, vm_end - vm_start, sg_dma_len(sg));
		ret = remap_pfn_range(vma, vm_start, page_to_pfn(sg_page(sg)),
						size, vma->vm_page_prot);
	}

	if (ret)
		return ret;

	if (vm_start < vm_end)
		return -EINVAL;

	vma->vm_flags		|= VM_DONTEXPAND;
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	return ret;
}

static int vb2_ion_map_dmabuf(void *mem_priv, int plane)
{
	struct vb2_ion_buf *buf = mem_priv;
	struct vb2_ion_context *ctx = buf->ctx;

	if (WARN_ON(!buf->attachment)) {
		pr_err("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->cookie.sgt)) {
		pr_err("dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	buf->cookie.sgt = dma_buf_map_attachment(buf->attachment,
		ctx_coherent(ctx) ? DMA_COHERENT : buf->direction);
	if (IS_ERR_OR_NULL(buf->cookie.sgt)) {
		pr_err("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	vb2_ion_sync_for_device((void *) &buf->cookie, 0, buf->size,
			ctx_coherent(ctx) ? DMA_COHERENT : buf->direction);

	buf->cookie.offset = 0;
	/* buf->kva = NULL; */

	mutex_lock(&ctx->lock);
	if (ctx_iommu(ctx) && !ctx->protected && buf->cookie.ioaddr == 0) {
		buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
					buf->size, buf->direction, plane);
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			pr_err("buf->cookie.ioaddr is error: %d\n",
					buf->cookie.ioaddr);
			mutex_unlock(&ctx->lock);
			dma_buf_unmap_attachment(buf->attachment,
					buf->cookie.sgt, buf->direction);
			return (int)buf->cookie.ioaddr;
		}
	}
	mutex_unlock(&ctx->lock);

	return 0;
}

static void vb2_ion_unmap_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (WARN_ON(!buf->attachment)) {
		pr_err("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!buf->cookie.sgt)) {
		pr_err("dmabuf buffer is already unpinned\n");
		return;
	}

	dma_buf_unmap_attachment(buf->attachment, buf->cookie.sgt,
		ctx_coherent(buf->ctx) ? DMA_COHERENT : buf->direction);

	vb2_ion_sync_for_cpu((void *) &buf->cookie, 0, buf->size,
			ctx_coherent(buf->ctx) ? DMA_COHERENT : buf->direction);

	buf->cookie.sgt = NULL;
}

static void vb2_ion_detach_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;
	struct vb2_ion_context *ctx = buf->ctx;

	mutex_lock(&ctx->lock);
	if (buf->cookie.ioaddr && ctx_iommu(ctx) && !ctx->protected ) {
		ion_iovmm_unmap(buf->attachment, buf->cookie.ioaddr);
		buf->cookie.ioaddr = 0;
	}
	mutex_unlock(&ctx->lock);

	if (buf->kva != NULL) {
		dma_buf_kunmap(buf->dma_buf, 0, buf->kva);
		dma_buf_end_cpu_access(buf->dma_buf, 0, buf->size, 0);
	}

	/* detach this attachment */
	dma_buf_detach(buf->dma_buf, buf->attachment);
	kfree(buf);
}

static void *vb2_ion_attach_dmabuf(void *alloc_ctx, struct dma_buf *dbuf,
				  unsigned long size, int write)
{
	struct vb2_ion_buf *buf;
	struct dma_buf_attachment *attachment;

	if (dbuf->size < size) {
		WARN(1, "dbuf->size(%d) is smaller than size(%ld)\n",
				dbuf->size, size);
		return ERR_PTR(-EFAULT);
	}

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf) {
		pr_err("out of memory\n");
		return ERR_PTR(-ENOMEM);
	}

	buf->ctx = alloc_ctx;
	/* create attachment for the dmabuf with the user device */
	attachment = dma_buf_attach(dbuf, buf->ctx->dev);
	if (IS_ERR(attachment)) {
		pr_err("failed to attach dmabuf\n");
		kfree(buf);
		return attachment;
	}

	buf->direction = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf->size = size;
	buf->dma_buf = dbuf;
	buf->attachment = attachment;

	return buf;
}

/***** V4L2_MEMORY_USERPTR support *****/

struct vb2_ion_dmabuf_data {
	struct sg_table sgt;
	bool is_pfnmap;
	int write;
	void *kva;
};

static struct sg_table *vb2_ion_map_dmabuf_userptr(
		struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	struct vb2_ion_dmabuf_data *priv = attach->dmabuf->priv;
	return &priv->sgt;
}

static void vb2_ion_unmap_dmabuf_userptr(struct dma_buf_attachment *attach,
						struct sg_table *sgt,
						enum dma_data_direction dir)
{
}

static void vb2_ion_release_dmabuf_userptr(struct dma_buf *dbuf)
{
	struct vb2_ion_dmabuf_data *priv = dbuf->priv;
	int i;
	struct scatterlist *sg;

	vfree(priv->kva);

	if (!priv->is_pfnmap) {
		if (priv->write) {
			for_each_sg(priv->sgt.sgl, sg,
					priv->sgt.orig_nents, i) {
				set_page_dirty_lock(sg_page(sg));
				put_page(sg_page(sg));
			}
		} else {
			for_each_sg(priv->sgt.sgl, sg,
						priv->sgt.orig_nents, i)
				put_page(sg_page(sg));
		}
	}

	sg_free_table(&priv->sgt);
	kfree(priv);
}

static void *vb2_ion_kmap_dmabuf_userptr(struct dma_buf *dbuf,
					unsigned long page_num)
{
	struct page **pages, **tmp_pages;
	struct scatterlist *sgl;
	int num_pages, i;
	void *vaddr;
	struct vb2_ion_dmabuf_data *priv = dbuf->priv;

	num_pages = PAGE_ALIGN(
			offset_in_page(sg_phys(priv->sgt.sgl)) + dbuf->size)
				>> PAGE_SHIFT;

	pages = vmalloc(sizeof(*pages) * num_pages);
	if (!pages)
		return NULL;

	tmp_pages = pages;
	for_each_sg(priv->sgt.sgl, sgl, priv->sgt.orig_nents, i) {
		struct page *page = sg_page(sgl);
		unsigned int n =
			PAGE_ALIGN(sgl->offset + sg_dma_len(sgl)) >> PAGE_SHIFT;

		for (; n > 0; n--)
			*(tmp_pages++) = page++;
	}

	vaddr = vmap(pages, num_pages, VM_USERMAP | VM_MAP, PAGE_KERNEL);

	vfree(pages);

	return (vaddr) ? vaddr + offset_in_page(sg_phys(priv->sgt.sgl)) : 0;
}

void vb2_ion_kunmap_dmabuf_userptr(struct dma_buf *dbuf, unsigned long page_num,
								void *vaddr)
{
	vunmap((void *)((unsigned long)vaddr & PAGE_MASK));
}

static int vb2_ion_mmap_dmabuf_userptr(struct dma_buf *dbuf,
					struct vm_area_struct *vma)
{
	return -ENOSYS;
}

static struct dma_buf_ops vb2_ion_dmabuf_ops = {
	.map_dma_buf = vb2_ion_map_dmabuf_userptr,
	.unmap_dma_buf = vb2_ion_unmap_dmabuf_userptr,
	.release = vb2_ion_release_dmabuf_userptr,
	.kmap_atomic = vb2_ion_kmap_dmabuf_userptr,
	.kmap = vb2_ion_kmap_dmabuf_userptr,
	.kunmap_atomic = vb2_ion_kunmap_dmabuf_userptr,
	.kunmap = vb2_ion_kunmap_dmabuf_userptr,
	.mmap = vb2_ion_mmap_dmabuf_userptr,
};

static int pfnmap_digger(struct sg_table *sgt, unsigned long addr, int nr_pages,
			off_t offset)
{
	/* If the given user address is not normal mapping,
	   It must be contiguous physical mapping */
	struct vm_area_struct *vma;
	unsigned long *pfns;
	int i, ipfn, pi, ret;
	struct scatterlist *sg;
	unsigned int contigs;
	unsigned long pfn;

	vma = find_vma(current->mm, addr);

	if ((vma == NULL) ||
			(vma->vm_end < (addr + (nr_pages << PAGE_SHIFT))))
		return -EINVAL;

	pfns = kmalloc(sizeof(*pfns) * nr_pages, GFP_KERNEL);
	if (!pfns)
		return -ENOMEM;

	ret = follow_pfn(vma, addr, &pfns[0]); /* no side effect */
	if (ret)
		goto err_follow_pfn;

	if (!pfn_valid(pfns[0])) {
		ret = -EINVAL;
		goto err_follow_pfn;
	}

	addr += PAGE_SIZE;

	/* An element of pfns consists of
	 * - higher 20 bits: page frame number (pfn)
	 * - lower  12 bits: number of contiguous pages from the pfn
	 * Maximum size of a contiguous chunk: 16MB (4096 pages)
	 * contigs = 0 indicates no adjacent page is found yet.
	 * Thus, contigs = x means (x + 1) pages are contiguous.
	 */
	for (i = 1, pi = 0, ipfn = 0, contigs = 0; i < nr_pages; i++) {
		ret = follow_pfn(vma, addr, &pfn);
		if (ret)
			break;

		if ((pfns[ipfn] == (pfn - (i - pi))) &&
				!((contigs + 1) & PAGE_MASK)) {
			contigs++;
		} else {
			pfns[ipfn] <<= PAGE_SHIFT;
			pfns[ipfn] |= contigs;
			ipfn++;
			pi = i;
			contigs = 0;
			pfns[ipfn] = pfn;
		}

		addr += PAGE_SIZE;
	}

	if (i == nr_pages) {
		pfns[ipfn] <<= PAGE_SHIFT;
		pfns[ipfn] |= contigs;

		nr_pages = ipfn + 1;
	} else {
		ret = -EINVAL;
		goto err_follow_pfn;
	}

	ret = sg_alloc_table(sgt, nr_pages, GFP_KERNEL);
	if (ret)
		goto err_follow_pfn;

	for_each_sg(sgt->sgl, sg, nr_pages, i) {
		sg_set_page(sg, phys_to_page(pfns[i]),
			(((pfns[i] & ~PAGE_MASK) + 1) << PAGE_SHIFT) - offset,
			offset);
		offset = 0;
	}
err_follow_pfn:
	kfree(pfns);

	return ret;
}

static struct dma_buf *vb2_ion_get_user_pages(unsigned long start,
						   unsigned long len,
						   int write)
{
	size_t last_size = 0;
	struct page **pages;
	int nr_pages;
	int ret = 0, i;
	off_t start_off;
	struct vb2_ion_dmabuf_data *priv;
	struct scatterlist *sgl;
	struct dma_buf *dbuf;

	last_size = (start + len) & ~PAGE_MASK;
	if (last_size == 0)
		last_size = PAGE_SIZE;

	start_off = offset_in_page(start);

	start = round_down(start, PAGE_SIZE);

	nr_pages = PFN_DOWN(PAGE_ALIGN(len + start_off));

	pages = vzalloc(nr_pages * sizeof(*pages));
	if (!pages)
		return ERR_PTR(-ENOMEM);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_privdata;
	}

	ret = get_user_pages(current, current->mm, start,
				nr_pages, write, 0, pages, NULL);

	if (ret < 0) {
		ret = pfnmap_digger(&priv->sgt, start, nr_pages, start_off);
		if (ret)
			goto err_pfnmap;

		priv->is_pfnmap = true;
	} else {
		if (ret != nr_pages) {
			nr_pages = ret;
			ret = -EFAULT;
			goto err_alloc_sg;
		}

		ret = sg_alloc_table(&priv->sgt, nr_pages, GFP_KERNEL);
		if (ret)
			goto err_alloc_sg;

		sgl = priv->sgt.sgl;

		sg_set_page(sgl, pages[0],
				(nr_pages == 1) ? len : PAGE_SIZE - start_off,
				start_off);

		sgl = sg_next(sgl);

		/* nr_pages == 1 if sgl == NULL here */
		for (i = 1; i < (nr_pages - 1); i++) {
			sg_set_page(sgl, pages[i], PAGE_SIZE, 0);
			sgl = sg_next(sgl);
		}

		if (sgl)
			sg_set_page(sgl, pages[i], last_size, 0);

		priv->is_pfnmap = false;
	}

	priv->write = write;

	dbuf = dma_buf_export(priv, &vb2_ion_dmabuf_ops, len, O_RDWR);
	if (IS_ERR(dbuf)) {
		sg_free_table(&priv->sgt);
		ret = PTR_ERR(dbuf);
		goto err_alloc_sg;
	}
	vfree(pages);

	return dbuf;
err_alloc_sg:
	for (i = 0; i < nr_pages; i++)
		put_page(pages[i]);
err_pfnmap:
	kfree(priv);
err_privdata:
	vfree(pages);
	return ERR_PTR(ret);
}

static void *vb2_ion_get_userptr(void *alloc_ctx, unsigned long vaddr,
				unsigned long size, int write, int plane)
{
	struct vb2_ion_context *ctx = alloc_ctx;
	struct vb2_ion_buf *buf = NULL;
	struct vm_area_struct *vma;
	void *p_ret;

	vma = find_vma(current->mm, vaddr);
	if (!vma || (vaddr < vma->vm_start) || (size > (vma->vm_end - vaddr))) {
		dev_err(ctx->dev, "%s: Incorrect user buffer @ %#lx/%#lx\n",
				__func__, vaddr, size);
		return ERR_PTR(-EINVAL);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		dev_err(ctx->dev, "%s: Not enough memory\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	if (!vma->vm_file || !is_dma_buf_file(vma->vm_file)) {
		buf->dma_buf = vb2_ion_get_user_pages(vaddr, size, write);

		if (IS_ERR(buf->dma_buf)) {
			dev_err(ctx->dev,
			"%s: User buffer @ %#lx/%#lx is not allocated by ION\n",
				__func__, vaddr, size);
			p_ret = buf->dma_buf;
			goto err_get_user_pages;
		}
	} else {
		buf->dma_buf = vma->vm_file->private_data; /* ad-hoc */
		buf->cookie.offset = vaddr - vma->vm_start;
		get_dma_buf(buf->dma_buf);
	}

	buf->ctx = ctx;
	buf->direction = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf->size = size;

	buf->attachment = dma_buf_attach(buf->dma_buf, ctx->dev);
	if (IS_ERR(buf->attachment)) {
		dev_err(ctx->dev, "%s: Failed to pin user buffer @ %#lx/%#lx\n",
				__func__, vaddr, size);
		p_ret = buf->attachment;
		goto err_attach;
	}

	buf->vma = vb2_get_vma(vma);
	if (IS_ERR(buf->vma)) {
		dev_err(ctx->dev,
			"%s: Failed to holding user buffer @ %#lx/%#lx\n",
			__func__, vaddr, size);
		p_ret = buf->vma;
		goto err_get_vma;
	}

	buf->cookie.sgt = dma_buf_map_attachment(buf->attachment,
						buf->direction);
	if (IS_ERR(buf->cookie.sgt)) {
		dev_err(ctx->dev,
			"%s: Failed to get sgt of user buffer @ %#lx/%#lx\n",
			__func__, vaddr, size);
		p_ret = buf->cookie.sgt;
		goto err_map_attachment;
	}

	if (ctx_iommu(ctx))
		buf->cookie.ioaddr = iovmm_map(ctx->dev, buf->cookie.sgt->sgl,
					buf->cookie.offset, size, buf->direction,
					plane);

	if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
		dev_err(ctx->dev,
			"%s: Failed to alloc IOVA of user buffer @ %#lx/%#lx\n",
			__func__, vaddr, size);
		p_ret = (void *)buf->cookie.ioaddr;
		goto err_iovmm;
	}

	if ((pgprot_noncached(buf->vma->vm_page_prot)
				== buf->vma->vm_page_prot)
			|| (pgprot_writecombine(buf->vma->vm_page_prot)
				== buf->vma->vm_page_prot))
		buf->cached = false;
	else
		buf->cached = true;

	if (need_kaddr(ctx, size, buf->cached)) {
		 /* ION maps entire buffer at once in the kernel space */
		p_ret = (void *)dma_buf_begin_cpu_access(buf->dma_buf,
				buf->cookie.offset, size, DMA_FROM_DEVICE);
		if (p_ret) {
			dev_err(ctx->dev,
			"%s: No kernel mapping for user buffer @ %#lx/%#lx\n",
				__func__, vaddr, size);
			goto err_begin_cpu;
		}

		buf->kva = dma_buf_kmap(buf->dma_buf,
					buf->cookie.offset / PAGE_SIZE);
		if (!buf->kva) {
			dev_err(ctx->dev,
			"%s: No space in kernel for user buffer @ %#lx/%#lx\n",
				__func__, vaddr, size);
			p_ret = ERR_PTR(-ENOMEM);
			goto err_kmap;
		}

		buf->kva += buf->cookie.offset & ~PAGE_MASK;
	}

	return buf;
err_kmap:
	dma_buf_end_cpu_access(buf->dma_buf, buf->cookie.offset, size,
					DMA_FROM_DEVICE);
err_begin_cpu:
	if (ctx_iommu(ctx))
		iovmm_unmap(ctx->dev, buf->cookie.ioaddr);
err_iovmm:
	dma_buf_unmap_attachment(buf->attachment, buf->cookie.sgt,
				buf->direction);
err_map_attachment:
	vb2_put_vma(buf->vma);
err_get_vma:
	dma_buf_detach(buf->dma_buf, buf->attachment);
err_attach:
	dma_buf_put(buf->dma_buf);
err_get_user_pages:
	kfree(buf);

	return p_ret;
}

static void vb2_ion_put_userptr(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (buf->kva) {
		dma_buf_kunmap(buf->dma_buf, buf->cookie.offset / PAGE_SIZE,
				buf->kva - (buf->cookie.offset & ~PAGE_SIZE));
		dma_buf_end_cpu_access(buf->dma_buf, buf->cookie.offset,
					buf->size, DMA_FROM_DEVICE);
	}

	if (ctx_iommu(buf->ctx))
		iovmm_unmap(buf->ctx->dev, buf->cookie.ioaddr);

	dma_buf_unmap_attachment(buf->attachment, buf->cookie.sgt,
				buf->direction);
	vb2_put_vma(buf->vma);
	dma_buf_detach(buf->dma_buf, buf->attachment);
	dma_buf_put(buf->dma_buf);
	kfree(buf);
}

const struct vb2_mem_ops vb2_ion_memops = {
	.alloc		= vb2_ion_alloc,
	.put		= vb2_ion_put,
	.cookie		= vb2_ion_cookie,
	.vaddr		= vb2_ion_vaddr,
	.mmap		= vb2_ion_mmap,
	.map_dmabuf	= vb2_ion_map_dmabuf,
	.unmap_dmabuf	= vb2_ion_unmap_dmabuf,
	.attach_dmabuf	= vb2_ion_attach_dmabuf,
	.detach_dmabuf	= vb2_ion_detach_dmabuf,
	.get_userptr	= vb2_ion_get_userptr,
	.put_userptr	= vb2_ion_put_userptr,
	.num_users	= vb2_ion_num_users,
};
EXPORT_SYMBOL_GPL(vb2_ion_memops);

void vb2_ion_sync_for_device(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	dev_dbg(buf->ctx->dev, "syncing for device, dmabuf: %p, kva: %p, "
		"size: %d, dir: %d\n", buf->dma_buf, buf->kva, size, dir);

	if (buf->dma_buf && !buf->vma) {
		exynos_ion_sync_dmabuf_for_device(buf->ctx->dev,
						buf->dma_buf, size, dir);
	} else if (buf->kva) {
		if ((offset + size) > buf->size)
			size -= (offset + size) - buf->size;
		exynos_ion_sync_vaddr_for_device(buf->ctx->dev,
						buf->kva, size, offset, dir);
	} else {
		exynos_ion_sync_sg_for_device(buf->ctx->dev,
						buf->cookie.sgt, dir);
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_sync_for_device);

void vb2_ion_sync_for_cpu(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	dev_dbg(buf->ctx->dev, "syncing for cpu, dmabuf: %p, kva: %p, "
		"size: %d, dir: %d\n", buf->dma_buf, buf->kva, size, dir);

	if (buf->dma_buf && !buf->vma) {
		exynos_ion_sync_dmabuf_for_cpu(buf->ctx->dev,
						buf->dma_buf, size, dir);
	} else if (buf->kva) {
		if ((offset + size) > buf->size)
			size -= (offset + size) - buf->size;
		exynos_ion_sync_vaddr_for_cpu(buf->ctx->dev,
						buf->kva, size, offset, dir);
	} else {
		exynos_ion_sync_sg_for_cpu(buf->ctx->dev,
						buf->cookie.sgt, dir);
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_sync_for_cpu);

int vb2_ion_buf_prepare(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;
		if (buf->dma_buf && !buf->vma) /* dmabuf type */
			return 0;
		vb2_ion_sync_for_device((void *) &buf->cookie, 0,
							buf->size, dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_prepare);

int vb2_ion_buf_finish(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;
		if (buf->dma_buf && !buf->vma) /* dmabuf type */
			return 0;
		vb2_ion_sync_for_cpu((void *) &buf->cookie, 0,
						buf->size, dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_finish);

void vb2_ion_detach_iommu(void *alloc_ctx)
{
	struct vb2_ion_context *ctx = alloc_ctx;

	if (!ctx_iommu(ctx))
		return;

	mutex_lock(&ctx->lock);
	BUG_ON(ctx->iommu_active_cnt == 0);

	if (--ctx->iommu_active_cnt == 0 && !ctx->protected)
		iovmm_deactivate(ctx->dev);
	mutex_unlock(&ctx->lock);
}
EXPORT_SYMBOL_GPL(vb2_ion_detach_iommu);

int vb2_ion_attach_iommu(void *alloc_ctx)
{
	struct vb2_ion_context *ctx = alloc_ctx;
	int ret = 0;

	if (!ctx_iommu(ctx))
		return -ENOENT;

	mutex_lock(&ctx->lock);
	if (ctx->iommu_active_cnt == 0 && !ctx->protected)
		ret = iovmm_activate(ctx->dev);
	if (!ret)
		ctx->iommu_active_cnt++;
	mutex_unlock(&ctx->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(vb2_ion_attach_iommu);

MODULE_AUTHOR("Cho KyongHo <pullip.cho@samsung.com>");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("Android ION allocator handling routines for videobuf2");
MODULE_LICENSE("GPL");
