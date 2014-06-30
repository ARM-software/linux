/*
 * Copyright (C) 2013,2014 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 * Implementation of the DRM fbdev compatibility mode for the HDLCD driver.
 * Mainly used for doing dumb double buffering as expected by the ARM Mali driver.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <linux/dma-buf.h>

#include "hdlcd_drv.h"
#include "hdlcd_regs.h"

#define MAX_FRAMES 2

#define to_hdlcd_fb(x) container_of(x, struct hdlcd_fb, fb)

struct hdlcd_dmabuf_attachment {
	struct sg_table *sgt;
	enum dma_data_direction dir;
};

static struct drm_framebuffer *hdlcd_fb_alloc(struct drm_device *dev,
					struct drm_mode_fb_cmd2 *mode_cmd);

static int hdlcd_dmabuf_attach(struct dma_buf *dma_buf,
		struct device *target_dev, struct dma_buf_attachment *attach)
{
	struct hdlcd_dmabuf_attachment *hdlcd_attach;

	hdlcd_attach = kzalloc(sizeof(*hdlcd_attach), GFP_KERNEL);
	if (!hdlcd_attach)
		return -ENOMEM;

	hdlcd_attach->dir = DMA_NONE;
	attach->priv = hdlcd_attach;

	return 0;
}

static void hdlcd_dmabuf_detach(struct dma_buf *dma_buf,
				struct dma_buf_attachment *attach)
{
	struct hdlcd_dmabuf_attachment *hdlcd_attach = attach->priv;
	struct sg_table *sgt;

	if (!hdlcd_attach)
		return;

	sgt = hdlcd_attach->sgt;
	if (sgt) {
		sg_free_table(sgt);
	}

	kfree(sgt);
	kfree(hdlcd_attach);
	attach->priv = NULL;
}

static void hdlcd_dmabuf_release(struct dma_buf *dma_buf)
{
	struct hdlcd_drm_private *hdlcd = dma_buf->priv;
	struct drm_gem_object *obj = &hdlcd->bo->gem;

	drm_gem_object_unreference_unlocked(obj);
}

static struct sg_table *hdlcd_map_dma_buf(struct dma_buf_attachment *attach,
					  enum dma_data_direction dir)
{
	struct hdlcd_dmabuf_attachment *hdlcd_attach = attach->priv;
	struct hdlcd_drm_private *hdlcd = attach->dmabuf->priv;
	struct sg_table *sgt;
	int size, ret;

	if (dir == DMA_NONE || !hdlcd_attach)
		return ERR_PTR(-EINVAL);

	/* return the cached mapping when possible */
	if (hdlcd_attach->dir == dir)
		return hdlcd_attach->sgt;

	/* don't allow two different directions for the same attachment */
	if (hdlcd_attach->dir != DMA_NONE)
		return ERR_PTR(-EBUSY);

	size = hdlcd->bo->gem.size;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		DRM_ERROR("Failed to allocate sg_table\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret < 0) {
		DRM_ERROR("Failed to allocate page table\n");
		kfree(sgt);
		return ERR_PTR(-ENOMEM);
	}

	sg_dma_len(sgt->sgl) = size;
	sg_set_page(sgt->sgl, pfn_to_page(PFN_DOWN(hdlcd->bo->dma_addr)), size, 0);
	sg_dma_address(sgt->sgl) = hdlcd->bo->dma_addr;
	ret = dma_map_sg(attach->dev, sgt->sgl, sgt->orig_nents, dir);
	if (!ret) {
		DRM_ERROR("failed to map sgl with IOMMU\n");
		sg_free_table(sgt);
		kfree(sgt);
		return ERR_PTR(-EIO);
	}
	hdlcd_attach->sgt = sgt;
	hdlcd_attach->dir = dir;

	return sgt;
}

static void hdlcd_unmap_dma_buf(struct dma_buf_attachment *attach,
				struct sg_table *sgt, enum dma_data_direction dir)
{
	struct hdlcd_dmabuf_attachment *hdlcd_attach = attach->priv;

	if (hdlcd_attach->dir != DMA_NONE)
		dma_unmap_sg(attach->dev, sgt->sgl, sgt->nents, dir);
	sg_free_table(sgt);
	kfree(sgt);
	hdlcd_attach->sgt = NULL;
	hdlcd_attach->dir = DMA_NONE;
}

static void *hdlcd_dmabuf_kmap(struct dma_buf *dma_buf, unsigned long page_num)
{
	return NULL;
}

static void hdlcd_dmabuf_kunmap(struct dma_buf *dma_buf,
				unsigned long page_num, void *addr)
{
}

static int hdlcd_dmabuf_mmap(struct dma_buf *dma_buf, struct vm_area_struct *vma)
{
	struct hdlcd_drm_private *hdlcd = dma_buf->priv;
	struct drm_gem_object *obj = &hdlcd->bo->gem;
	struct hdlcd_bo *bo = to_hdlcd_bo_obj(obj);
	int ret;

	mutex_lock(&obj->dev->struct_mutex);
	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	mutex_unlock(&obj->dev->struct_mutex);
	if (ret < 0)
		return ret;

	/*
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(), and set the
	 * vm_pgoff (used as a fake buffer offset by DRM) to 0 as we want to map
	 * the whole buffer.
	 */
	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_pgoff = 0;
	ret = dma_mmap_writecombine(obj->dev->dev, vma, bo->cpu_addr, bo->dma_addr,
				    vma->vm_end - vma->vm_start);
	if (ret)
		drm_gem_vm_close(vma);

	return 0;
}

static void *hdlcd_dmabuf_vmap(struct dma_buf *dma_buf)
{
	return ERR_PTR(-EINVAL);
}

static void hdlcd_dmabuf_vunmap(struct dma_buf *dma_buf, void *cpu_addr)
{
}

struct dma_buf_ops hdlcd_buf_ops = {
	.attach = hdlcd_dmabuf_attach,
	.detach = hdlcd_dmabuf_detach,
	.map_dma_buf = hdlcd_map_dma_buf,
	.unmap_dma_buf = hdlcd_unmap_dma_buf,
	.release = hdlcd_dmabuf_release,
	.kmap = hdlcd_dmabuf_kmap,
	.kmap_atomic = hdlcd_dmabuf_kmap,
	.kunmap = hdlcd_dmabuf_kunmap,
	.kunmap_atomic = hdlcd_dmabuf_kunmap,
	.mmap = hdlcd_dmabuf_mmap,
	.vmap = hdlcd_dmabuf_vmap,
	.vunmap = hdlcd_dmabuf_vunmap,
};

/*
 * Used for sharing buffers with Mali userspace
 */
struct fb_dmabuf_export {
	uint32_t fd;
	uint32_t flags;
};

#define FBIOGET_DMABUF       _IOR('F', 0x21, struct fb_dmabuf_export)

static int hdlcd_get_dmabuf_ioctl(struct fb_info *info, unsigned int cmd,
				  unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct fb_dmabuf_export ebuf;
	struct drm_fb_helper *helper = info->par;
	struct hdlcd_drm_private *hdlcd = helper->dev->dev_private;
	struct dma_buf			*dma_buf;
	uint32_t			fd;

	if (copy_from_user(&ebuf, argp, sizeof(ebuf)))
		return -EFAULT;

	dma_buf = dma_buf_export(hdlcd, &hdlcd_buf_ops,
				info->screen_size, ebuf.flags | O_RDWR);
	if (!dma_buf) {
		dev_info(info->dev, "Failed to export DMA buffer\n");
		goto err_export;
	}

	fd = dma_buf_fd(dma_buf, O_CLOEXEC);
	if (fd < 0) {
		dev_info(info->dev, "Failed to get file descriptor for DMA buffer\n");
		goto err_export_fd;
	}
	ebuf.fd = fd;

	if (copy_to_user(argp, &ebuf, sizeof(ebuf)))
		goto err_export_fd;

	return 0;

err_export_fd:
	dma_buf_put(dma_buf);
err_export:
	return -EFAULT;
}

static int hdlcd_wait_for_vsync(struct fb_info *info)
{
#if 0
	struct drm_fb_helper *helper = info->par;
	struct hdlcd_drm_private *hdlcd = helper->dev->dev_private;
	int ret;

	drm_crtc_vblank_on(&hdlcd->crtc);
	ret = wait_for_completion_interruptible_timeout(&hdlcd->vsync_completion,
							msecs_to_jiffies(100));
	drm_crtc_vblank_off(&hdlcd->crtc);
	if (ret)
		return -ETIMEDOUT;
#endif

	return 0;
}

static int hdlcd_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FBIOGET_DMABUF:
		return hdlcd_get_dmabuf_ioctl(info, cmd, arg);
	case FBIO_WAITFORVSYNC:
		return hdlcd_wait_for_vsync(info);
	default:
		printk(KERN_INFO "HDLCD FB does not handle ioctl 0x%x\n", cmd);
	}

	return -EFAULT;
}

static int hdlcd_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct drm_fb_helper *helper = info->par;
	struct hdlcd_drm_private *hdlcd = helper->dev->dev_private;
	struct drm_gem_object *obj = &hdlcd->bo->gem;
	struct hdlcd_bo *bo = to_hdlcd_bo_obj(obj);
	DEFINE_DMA_ATTRS(attrs);
	size_t vm_size;

	dma_set_attr(DMA_ATTR_WRITE_COMBINE, &attrs);

	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
	vm_size = vma->vm_end - vma->vm_start;

	if (vm_size > bo->gem.size)
		return -EINVAL;

	return dma_mmap_attrs(helper->dev->dev, vma, bo->cpu_addr,
			bo->dma_addr, bo->gem.size, &attrs);
}

static int hdlcd_fb_check_var(struct fb_var_screeninfo *var,
			struct fb_info *info)
{
	struct drm_fb_helper *fb_helper = info->par;
	struct drm_framebuffer *fb = fb_helper->fb;
	int depth;

	if (var->pixclock != 0 || in_dbg_master())
		return -EINVAL;

	/* Need to resize the fb object !!! */
	if (var->bits_per_pixel > fb->bits_per_pixel ||
	    var->xres > fb->width || var->yres > fb->height ||
	    var->xres_virtual > fb->width || var->yres_virtual > fb->height * MAX_FRAMES) {
		DRM_DEBUG("fb userspace requested width/height/bpp is greater than current fb "
			  "request %dx%d-%d (virtual %dx%d) > %dx%d-%d\n",
			  var->xres, var->yres, var->bits_per_pixel,
			  var->xres_virtual, var->yres_virtual,
			  fb->width, fb->height, fb->bits_per_pixel);
		return -EINVAL;
	}

	switch (var->bits_per_pixel) {
	case 16:
		depth = (var->green.length == 6) ? 16 : 15;
		break;
	case 32:
		depth = (var->transp.length > 0) ? 32 : 24;
		break;
	default:
		depth = var->bits_per_pixel;
		break;
	}

	switch (depth) {
	case 8:
		var->red.offset = 0;
		var->green.offset = 0;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 15:
		var->red.offset = 10;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.length = 1;
		var->transp.offset = 15;
		break;
	case 16:
		var->red.offset = 11;
		var->green.offset = 5;
		var->blue.offset = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 24:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 0;
		var->transp.offset = 0;
		break;
	case 32:
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.length = 8;
		var->transp.offset = 24;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct fb_ops hdlcd_fb_ops = {
	.owner	= THIS_MODULE,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
	.fb_check_var = hdlcd_fb_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_setcmap = drm_fb_helper_setcmap,
	.fb_ioctl = hdlcd_fb_ioctl,
	.fb_mmap = hdlcd_fb_mmap,
};

static struct hdlcd_bo *hdlcd_fb_bo_create(struct drm_device *drm, size_t size)
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
		dev_err(drm->dev, "failed to allocate buffer of size %zu\n", size);
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

#if 0
err_map:
	dma_free_attrs(drm->dev, size, bo->cpu_addr, bo->dma_addr, &attrs);
#endif
err_dma:
	drm_gem_object_release(&bo->gem);
err_init:
	kfree(bo);

	return ERR_PTR(err);
}

void hdlcd_fb_bo_free(struct hdlcd_bo *bo)
{
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_WRITE_COMBINE, &attrs);

	drm_gem_object_release(&bo->gem);
	dma_free_attrs(bo->gem.dev->dev, bo->gem.size, bo->cpu_addr, bo->dma_addr, &attrs);
	kfree(bo);
}

static int hdlcd_fb_probe(struct drm_fb_helper *helper,
			  struct drm_fb_helper_surface_size *sizes)
{
	struct hdlcd_drm_private *hdlcd = helper->dev->dev_private;
	struct fb_info *info;
	struct hdlcd_bo *bo = hdlcd->bo;
	struct drm_mode_fb_cmd2 cmd = { 0 };
	unsigned int bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);
	unsigned long offset;
	size_t size;
	int err;

	cmd.width = sizes->surface_width;
	cmd.height = sizes->surface_height;
	cmd.pitches[0] = sizes->surface_width * bytes_per_pixel;
	cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
						     sizes->surface_depth);

	size = cmd.pitches[0] * cmd.height * MAX_FRAMES;

	bo = hdlcd_fb_bo_create(helper->dev, size);
	if (IS_ERR(bo))
		return PTR_ERR(bo);

	info = framebuffer_alloc(0, helper->dev->dev);
	if (!info) {
		dev_err(helper->dev->dev, "failed to allocate framebuffer info\n");
		hdlcd_fb_bo_free(bo);
		return -ENOMEM;
	}

	helper->fb = hdlcd_fb_alloc(helper->dev, &cmd);
	helper->fbdev = info;

	info->par = helper;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->fbops = &hdlcd_fb_ops;

	err = fb_alloc_cmap(&info->cmap, 256, 0);
	if (err < 0) {
		dev_err(helper->dev->dev, "failed to allocate color map: %d\n", err);
		goto cleanup;
	}

	drm_fb_helper_fill_fix(info, helper->fb->pitches[0], helper->fb->depth);
	drm_fb_helper_fill_var(info, helper, helper->fb->width, sizes->surface_height);

	offset = info->var.xoffset * bytes_per_pixel +
			info->var.yoffset * helper->fb->pitches[0];

	helper->dev->mode_config.fb_base = (resource_size_t)bo->dma_addr;
	info->screen_base = (void __iomem *)bo->cpu_addr + offset;
	info->screen_size = size;
	info->var.yres_virtual = info->var.yres * MAX_FRAMES;
	info->fix.smem_start = (unsigned long)bo->dma_addr + offset;
	info->fix.smem_len = size;

	return 0;

cleanup:
	drm_framebuffer_unregister_private(helper->fb);
	hdlcd_fb_bo_free(bo);
	framebuffer_release(info);
	return err;
}

static struct drm_fb_helper_funcs hdlcd_fb_helper_funcs = {
	.fb_probe	= hdlcd_fb_probe,
};

static void hdlcd_fb_destroy(struct drm_framebuffer *fb)
{
	struct hdlcd_drm_private *hdlcd = fb->dev->dev_private;
	drm_gem_object_unreference_unlocked(&hdlcd->bo->gem);
	drm_framebuffer_cleanup(fb);
	kfree(hdlcd->bo);
}

static int hdlcd_fb_create_handle(struct drm_framebuffer *fb,
				  struct drm_file *file_priv,
				  unsigned int *handle)
{
	struct hdlcd_drm_private *hdlcd = fb->dev->dev_private;
	return drm_gem_handle_create(file_priv, &hdlcd->bo->gem, handle);
}

static int hdlcd_fb_dirty(struct drm_framebuffer *fb,
			  struct drm_file *file_priv, unsigned flags,
			  unsigned color, struct drm_clip_rect *clips,
			  unsigned num_clips)
{
	return 0;
}

static struct drm_framebuffer_funcs hdlcd_fb_funcs = {
	.destroy	= hdlcd_fb_destroy,
	.create_handle	= hdlcd_fb_create_handle,
	.dirty		= hdlcd_fb_dirty,
};

static struct drm_framebuffer *hdlcd_fb_alloc(struct drm_device *dev,
					      struct drm_mode_fb_cmd2 *mode_cmd)
{
	int err;
	struct drm_framebuffer *fb;

	dev_info(dev->dev, "Linux is here %s", __func__);
	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(fb, mode_cmd);

	err = drm_framebuffer_init(dev, fb, &hdlcd_fb_funcs);
	if (err) {
		dev_err(dev->dev, "failed to initialize framebuffer.\n");
		kfree(fb);
		return ERR_PTR(err);
	}

	return fb;
}

static struct drm_framebuffer *hdlcd_fb_create(struct drm_device *dev,
					       struct drm_file *file_priv,
					       struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *obj;
	struct hdlcd_bo *bo;
	struct hdlcd_drm_private *hdlcd = dev->dev_private;

	obj = drm_gem_object_lookup(dev, file_priv, mode_cmd->handles[0]);
	if (!obj) {
		dev_err(dev->dev, "failed to lookup GEM object\n");
		return ERR_PTR(-ENXIO);
	}

	bo = to_hdlcd_bo_obj(obj);
	hdlcd->fb_helper->fb = hdlcd_fb_alloc(dev, mode_cmd);
	if (IS_ERR(hdlcd->fb_helper->fb)) {
		dev_err(dev->dev, "failed to allocate DRM framebuffer\n");
	}
	hdlcd->bo = bo;

	return hdlcd->fb_helper->fb;
}

int hdlcd_fbdev_init(struct drm_device *dev)
{
	int ret;
	struct hdlcd_drm_private *hdlcd = dev->dev_private;

	hdlcd->fb_helper = kzalloc(sizeof(*hdlcd->fb_helper), GFP_KERNEL);
	if (!hdlcd->fb_helper)
		return -ENOMEM;

	hdlcd->fb_helper->funcs = &hdlcd_fb_helper_funcs;
	ret = drm_fb_helper_init(dev, hdlcd->fb_helper, dev->mode_config.num_crtc,
				 dev->mode_config.num_connector);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to initialize DRM fb helper.\n");
		goto err_free;
	}

	ret = drm_fb_helper_single_add_all_connectors(hdlcd->fb_helper);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to add connectors.\n");
		goto err_helper_fini;
	}

	drm_helper_disable_unused_functions(dev);

	/* disable all the possible outputs/crtcs before entering KMS mode */
	ret = drm_fb_helper_initial_config(hdlcd->fb_helper, 32);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to set initial hw configuration.\n");
		goto err_helper_fini;
	}

	return 0;

err_helper_fini:
	drm_fb_helper_fini(hdlcd->fb_helper);

err_free:
	kfree(hdlcd->fb_helper);
	hdlcd->fb_helper = NULL;

	return ret;
}

static void hdlcd_fb_output_poll_changed(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	struct drm_fb_helper *fb_helper = hdlcd->fb_helper;

	drm_fb_helper_hotplug_event(fb_helper);
}

static const struct drm_mode_config_funcs hdlcd_mode_config_funcs = {
	.fb_create = hdlcd_fb_create,
	.output_poll_changed = hdlcd_fb_output_poll_changed,
};


void hdlcd_drm_mode_config_init(struct drm_device *dev)
{
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = HDLCD_MAX_XRES;
	dev->mode_config.max_height = HDLCD_MAX_YRES;
	dev->mode_config.funcs = &hdlcd_mode_config_funcs;
}
