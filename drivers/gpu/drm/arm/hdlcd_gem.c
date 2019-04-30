/*
 * Copyright (C) 2013,2014 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <drm/drmP.h>
#include <linux/dma-buf.h>

#include "hdlcd_drv.h"

struct hdlcd_bo *hdlcd_bo_create(struct drm_device *drm, unsigned int size)
{
	int err;
	struct hdlcd_bo *bo;
	struct hdlcd_drm_private *hdlcd = drm->dev_private;
	DEFINE_DMA_ATTRS(attrs);

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (!bo)
		return ERR_PTR(-ENOMEM);

	size = round_up(size, PAGE_SIZE);
	err = drm_gem_object_init(drm, &bo->gem, size);
	if (err)
		goto err_init;

	dma_set_attr(DMA_ATTR_WRITE_COMBINE, &attrs);
	bo->cpu_addr = dma_alloc_attrs(drm->dev, size, &bo->dma_addr,
				    GFP_KERNEL | __GFP_NOWARN, &attrs);

	if (!bo->cpu_addr) {
		DRM_ERROR("failed to allocate buffer of size %u\n", size);
		err = -ENOMEM;
		goto err_dma;
	}

#if 0
	err = drm_gem_create_mmap_offset(&bo->gem);
	if (err)
		goto err_map;
#endif

	hdlcd->bo = bo;
	return bo;

err_map:
	dma_free_attrs(drm->dev, size, bo->cpu_addr, bo->dma_addr, &attrs);
err_dma:
	drm_gem_object_release(&bo->gem);
err_init:
	kfree(bo);

	return ERR_PTR(err);
}

void hdlcd_bo_free(struct hdlcd_bo *bo)
{
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_WRITE_COMBINE, &attrs);

	drm_gem_object_release(&bo->gem);
	dma_free_attrs(bo->gem.dev->dev, bo->gem.size, bo->cpu_addr, bo->dma_addr, &attrs);
	kfree(bo);
}

const struct vm_operations_struct hdlcd_gem_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

int hdlcd_gem_dumb_create(struct drm_file *file_priv,
		struct drm_device *drm, struct drm_mode_create_dumb *args)
{
	struct hdlcd_bo *bo;
	int ret, bytes_pp = DIV_ROUND_UP(args->bpp, 8);

	args->pitch = ALIGN(args->width * bytes_pp, 64);
	args->size = PAGE_ALIGN(args->pitch * args->height);
	bo = hdlcd_bo_create(drm, args->size);
	if (IS_ERR(bo))
		return PTR_ERR(bo);

	ret = drm_gem_handle_create(file_priv, &bo->gem, &args->handle);
	if (ret != 0) {
		DRM_ERROR("failed to create GEM handle\n");
		hdlcd_bo_free(bo);
		return ret;
	}

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(&bo->gem);

	return 0;
}

void hdlcd_gem_free_object(struct drm_gem_object *gem_obj)
{
}

struct sg_table *hdlcd_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct hdlcd_bo *bo = to_hdlcd_bo_obj(obj);
	struct sg_table *sgt;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	ret = dma_get_sgtable(obj->dev->dev, sgt, bo->cpu_addr, bo->dma_addr, obj->size);
	if (ret < 0) {
		kfree(sgt);
		return NULL;
	}

	return sgt;
}

void *hdlcd_gem_prime_vmap(struct drm_gem_object *obj)
{
	struct hdlcd_bo *bo = to_hdlcd_bo_obj(obj);

	return bo->cpu_addr;
}

void hdlcd_gem_prime_vunmap(struct drm_gem_object *obj, void *cpu_addr)
{
	/* nothing to do */
}

int hdlcd_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct hdlcd_bo *bo = to_hdlcd_bo_obj(obj);
	struct drm_device *dev = obj->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	mutex_unlock(&dev->struct_mutex);
	if (ret < 0)
		return ret;

	/*
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(), and set the
	 * vm_pgoff (used as a fake buffer offset by DRM) to 0 as we want to map
	 * the whole buffer.
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;
	ret = dma_mmap_writecombine(dev->dev, vma, bo->cpu_addr, bo->dma_addr,
				    vma->vm_end - vma->vm_start);
	if (ret)
		drm_gem_vm_close(vma);

	return 0;
}

struct drm_gem_object *hdlcd_gem_prime_import(struct drm_device *dev,
					      struct dma_buf *dma_buf)
{
	return NULL;
}

struct dma_buf *hdlcd_gem_prime_export(struct drm_device *dev,
				      struct drm_gem_object *obj, int flags)
{
	return  NULL;
}

int hdlcd_gem_mmap(struct file *file_priv, struct vm_area_struct *vma)
{
	return -ENXIO;
}
