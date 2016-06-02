/*
 * Copied from drivers/gpu/drm/drm_fb_cma_helper.c which has the following
 * copyright and notices...
 *
 * Copyright (C) 2012 Analog Device Inc.
 *   Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 * Based on udl_fbdev.c
 *  Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include "hdlcd_fb_helper.h"
#include <linux/dma-buf.h>
#include <linux/module.h>
#include "hdlcd_drv.h"

#define MAX_FRAMES 2

static int hdlcd_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);

/******************************************************************************
 * Code copied from drivers/gpu/drm/drm_fb_helper.c as of Linux 3.18
 ******************************************************************************/

/**
 * Copy of drm_fb_helper_check_var modified to allow MAX_FRAMES * height
 */
int hdlcd_fb_helper_check_var(struct fb_var_screeninfo *var,
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

/******************************************************************************
 * Code copied from drivers/gpu/drm/drm_fb_cma_helper.c as of Linux 3.18
 ******************************************************************************/

struct hdlcd_fb {
	struct drm_framebuffer		fb;
	struct drm_gem_cma_object	*obj[4];
};

struct hdlcd_drm_fbdev {
	struct drm_fb_helper	fb_helper;
	struct hdlcd_fb	*fb;
};

static inline struct hdlcd_drm_fbdev *to_hdlcd_fbdev(struct drm_fb_helper *helper)
{
	return container_of(helper, struct hdlcd_drm_fbdev, fb_helper);
}

static inline struct hdlcd_fb *to_hdlcd_fb(struct drm_framebuffer *fb)
{
	return container_of(fb, struct hdlcd_fb, fb);
}

static void hdlcd_fb_destroy(struct drm_framebuffer *fb)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);
	int i;

	for (i = 0; i < 4; i++) {
		if (hdlcd_fb->obj[i])
			drm_gem_object_unreference_unlocked(&hdlcd_fb->obj[i]->base);
	}

	drm_framebuffer_cleanup(fb);
	kfree(hdlcd_fb);
}

static int hdlcd_fb_create_handle(struct drm_framebuffer *fb,
	struct drm_file *file_priv, unsigned int *handle)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);

	return drm_gem_handle_create(file_priv,
			&hdlcd_fb->obj[0]->base, handle);
}

static struct drm_framebuffer_funcs hdlcd_fb_funcs = {
	.destroy	= hdlcd_fb_destroy,
	.create_handle	= hdlcd_fb_create_handle,
};

static struct hdlcd_fb *hdlcd_fb_alloc(struct drm_device *dev,
	struct drm_mode_fb_cmd2 *mode_cmd, struct drm_gem_cma_object **obj,
	unsigned int num_planes)
{
	struct hdlcd_fb *hdlcd_fb;
	int ret;
	int i;

	hdlcd_fb = kzalloc(sizeof(*hdlcd_fb), GFP_KERNEL);
	if (!hdlcd_fb)
		return ERR_PTR(-ENOMEM);

	drm_helper_mode_fill_fb_struct(&hdlcd_fb->fb, mode_cmd);

	for (i = 0; i < num_planes; i++)
		hdlcd_fb->obj[i] = obj[i];

	ret = drm_framebuffer_init(dev, &hdlcd_fb->fb, &hdlcd_fb_funcs);
	if (ret) {
		dev_err(dev->dev, "Failed to initialize framebuffer: %d\n", ret);
		kfree(hdlcd_fb);
		return ERR_PTR(ret);
	}

	return hdlcd_fb;
}

/**
 * hdlcd_fb_create() - (struct drm_mode_config_funcs *)->fb_create callback function
 *
 * If your hardware has special alignment or pitch requirements these should be
 * checked before calling this function.
 */
struct drm_framebuffer *hdlcd_fb_create(struct drm_device *dev,
	struct drm_file *file_priv, struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct hdlcd_fb *hdlcd_fb;
	struct drm_gem_cma_object *objs[4];
	struct drm_gem_object *obj;
	unsigned int hsub;
	unsigned int vsub;
	int ret;
	int i;

	hsub = drm_format_horz_chroma_subsampling(mode_cmd->pixel_format);
	vsub = drm_format_vert_chroma_subsampling(mode_cmd->pixel_format);

	for (i = 0; i < drm_format_num_planes(mode_cmd->pixel_format); i++) {
		unsigned int width = mode_cmd->width / (i ? hsub : 1);
		unsigned int height = mode_cmd->height / (i ? vsub : 1);
		unsigned int min_size;

		obj = drm_gem_object_lookup(dev, file_priv, mode_cmd->handles[i]);
		if (!obj) {
			dev_err(dev->dev, "Failed to lookup GEM object\n");
			ret = -ENXIO;
			goto err_gem_object_unreference;
		}

		min_size = (height - 1) * mode_cmd->pitches[i]
			 + width * drm_format_plane_cpp(mode_cmd->pixel_format, i)
			 + mode_cmd->offsets[i];

		if (obj->size < min_size) {
			drm_gem_object_unreference_unlocked(obj);
			ret = -EINVAL;
			goto err_gem_object_unreference;
		}
		objs[i] = to_drm_gem_cma_obj(obj);
	}

	hdlcd_fb = hdlcd_fb_alloc(dev, mode_cmd, objs, i);
	if (IS_ERR(hdlcd_fb)) {
		ret = PTR_ERR(hdlcd_fb);
		goto err_gem_object_unreference;
	}

	return &hdlcd_fb->fb;

err_gem_object_unreference:
	for (i--; i >= 0; i--)
		drm_gem_object_unreference_unlocked(&objs[i]->base);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(hdlcd_fb_create);

/**
 * hdlcd_fb_get_gem_obj() - Get CMA GEM object for framebuffer
 * @fb: The framebuffer
 * @plane: Which plane
 *
 * Return the CMA GEM object for given framebuffer.
 *
 * This function will usually be called from the CRTC callback functions.
 */
struct drm_gem_cma_object *hdlcd_fb_get_gem_obj(struct drm_framebuffer *fb,
	unsigned int plane)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);

	if (plane >= 4)
		return NULL;

	return hdlcd_fb->obj[plane];
}
EXPORT_SYMBOL_GPL(hdlcd_fb_get_gem_obj);

#ifdef CONFIG_DEBUG_FS
/*
 * hdlcd_fb_describe() - Helper to dump information about a single
 * CMA framebuffer object
 */
static void hdlcd_fb_describe(struct drm_framebuffer *fb, struct seq_file *m)
{
	struct hdlcd_fb *hdlcd_fb = to_hdlcd_fb(fb);
	int i, n = drm_format_num_planes(fb->pixel_format);

	seq_printf(m, "fb: %dx%d@%4.4s\n", fb->width, fb->height,
			(char *)&fb->pixel_format);

	for (i = 0; i < n; i++) {
		seq_printf(m, "   %d: offset=%d pitch=%d, obj: ",
				i, fb->offsets[i], fb->pitches[i]);
		drm_gem_cma_describe(hdlcd_fb->obj[i], m);
	}
}

/**
 * hdlcd_fb_debugfs_show() - Helper to list CMA framebuffer objects
 * in debugfs.
 */
int hdlcd_fb_debugfs_show(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_framebuffer *fb;
	int ret;

	ret = mutex_lock_interruptible(&dev->mode_config.mutex);
	if (ret)
		return ret;

	ret = mutex_lock_interruptible(&dev->struct_mutex);
	if (ret) {
		mutex_unlock(&dev->mode_config.mutex);
		return ret;
	}

	list_for_each_entry(fb, &dev->mode_config.fb_list, head)
		hdlcd_fb_describe(fb, m);

	mutex_unlock(&dev->struct_mutex);
	mutex_unlock(&dev->mode_config.mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(hdlcd_fb_debugfs_show);
#endif

static struct fb_ops hdlcd_drm_fbdev_ops = {
	.owner		= THIS_MODULE,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_check_var	= hdlcd_fb_helper_check_var,
	.fb_set_par	= drm_fb_helper_set_par,
	.fb_blank	= drm_fb_helper_blank,
	.fb_pan_display	= drm_fb_helper_pan_display,
	.fb_setcmap	= drm_fb_helper_setcmap,
	.fb_ioctl	= hdlcd_fb_ioctl,
	.fb_compat_ioctl= hdlcd_fb_ioctl,
};

static int hdlcd_drm_fbdev_create(struct drm_fb_helper *helper,
	struct drm_fb_helper_surface_size *sizes)
{
	struct hdlcd_drm_fbdev *hdlcd_fbdev = to_hdlcd_fbdev(helper);
	struct drm_mode_fb_cmd2 mode_cmd = { 0 };
	struct drm_device *dev = helper->dev;
	struct drm_gem_cma_object *obj;
	struct drm_framebuffer *fb;
	unsigned int bytes_per_pixel;
	unsigned long offset;
	struct fb_info *fbi;
	size_t size;
	int ret;

	DRM_DEBUG_KMS("surface width(%d), height(%d) and bpp(%d)\n",
			sizes->surface_width, sizes->surface_height,
			sizes->surface_bpp);

	bytes_per_pixel = DIV_ROUND_UP(sizes->surface_bpp, 8);

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = sizes->surface_width * bytes_per_pixel;
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
		sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height * MAX_FRAMES;
	obj = drm_gem_cma_create(dev, size);
	if (IS_ERR(obj))
		return -ENOMEM;

	fbi = framebuffer_alloc(0, dev->dev);
	if (!fbi) {
		dev_err(dev->dev, "Failed to allocate framebuffer info.\n");
		ret = -ENOMEM;
		goto err_drm_gem_cma_free_object;
	}

	hdlcd_fbdev->fb = hdlcd_fb_alloc(dev, &mode_cmd, &obj, 1);
	if (IS_ERR(hdlcd_fbdev->fb)) {
		dev_err(dev->dev, "Failed to allocate DRM framebuffer.\n");
		ret = PTR_ERR(hdlcd_fbdev->fb);
		goto err_framebuffer_release;
	}

	fb = &hdlcd_fbdev->fb->fb;
	helper->fb = fb;
	helper->fbdev = fbi;

	fbi->par = helper;
	fbi->flags = FBINFO_FLAG_DEFAULT;
	fbi->fbops = &hdlcd_drm_fbdev_ops;

	ret = fb_alloc_cmap(&fbi->cmap, 256, 0);
	if (ret) {
		dev_err(dev->dev, "Failed to allocate color map.\n");
		goto err_hdlcd_fb_destroy;
	}

	drm_fb_helper_fill_fix(fbi, fb->pitches[0], fb->depth);
	drm_fb_helper_fill_var(fbi, helper, fb->width, fb->height);

	offset = fbi->var.xoffset * bytes_per_pixel;
	offset += fbi->var.yoffset * fb->pitches[0];

	dev->mode_config.fb_base = (resource_size_t)obj->paddr;
	fbi->screen_base = obj->vaddr + offset;
	fbi->fix.smem_start = (unsigned long)(obj->paddr + offset);
	fbi->screen_size = size;
	fbi->fix.smem_len = size;
	fbi->var.yres_virtual = fbi->var.yres * MAX_FRAMES;

	return 0;

err_hdlcd_fb_destroy:
	drm_framebuffer_unregister_private(fb);
	hdlcd_fb_destroy(fb);
err_framebuffer_release:
	framebuffer_release(fbi);
err_drm_gem_cma_free_object:
	drm_gem_cma_free_object(&obj->base);
	return ret;
}

static const struct drm_fb_helper_funcs hdlcd_fb_helper_funcs = {
	.fb_probe = hdlcd_drm_fbdev_create,
};

/**
 * hdlcd_drm_fbdev_init() - Allocate and initializes a hdlcd_drm_fbdev struct
 * @dev: DRM device
 * @preferred_bpp: Preferred bits per pixel for the device
 * @num_crtc: Number of CRTCs
 * @max_conn_count: Maximum number of connectors
 *
 * Returns a newly allocated hdlcd_drm_fbdev struct or a ERR_PTR.
 */
struct hdlcd_drm_fbdev *hdlcd_drm_fbdev_init(struct drm_device *dev,
	unsigned int preferred_bpp, unsigned int num_crtc,
	unsigned int max_conn_count)
{
	struct hdlcd_drm_fbdev *hdlcd_fbdev;
	struct drm_fb_helper *helper;
	int ret;

	hdlcd_fbdev = kzalloc(sizeof(*hdlcd_fbdev), GFP_KERNEL);
	if (!hdlcd_fbdev) {
		dev_err(dev->dev, "Failed to allocate drm fbdev.\n");
		return ERR_PTR(-ENOMEM);
	}

	helper = &hdlcd_fbdev->fb_helper;

	drm_fb_helper_prepare(dev, helper, &hdlcd_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, helper, num_crtc, max_conn_count);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to initialize drm fb helper.\n");
		goto err_free;
	}

	ret = drm_fb_helper_single_add_all_connectors(helper);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to add connectors.\n");
		goto err_drm_fb_helper_fini;

	}

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	ret = drm_fb_helper_initial_config(helper, preferred_bpp);
	if (ret < 0) {
		dev_err(dev->dev, "Failed to set initial hw configuration.\n");
		goto err_drm_fb_helper_fini;
	}

	return hdlcd_fbdev;

err_drm_fb_helper_fini:
	drm_fb_helper_fini(helper);
err_free:
	kfree(hdlcd_fbdev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_init);

/**
 * hdlcd_drm_fbdev_fini() - Free hdlcd_drm_fbdev struct
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct
 */
void hdlcd_drm_fbdev_fini(struct hdlcd_drm_fbdev *hdlcd_fbdev)
{
	if (hdlcd_fbdev->fb_helper.fbdev) {
		struct fb_info *info;
		int ret;

		info = hdlcd_fbdev->fb_helper.fbdev;
		ret = unregister_framebuffer(info);
		if (ret < 0)
			DRM_DEBUG_KMS("failed unregister_framebuffer()\n");

		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);

		framebuffer_release(info);
	}

	if (hdlcd_fbdev->fb) {
		drm_framebuffer_unregister_private(&hdlcd_fbdev->fb->fb);
		hdlcd_fb_destroy(&hdlcd_fbdev->fb->fb);
	}

	drm_fb_helper_fini(&hdlcd_fbdev->fb_helper);
	kfree(hdlcd_fbdev);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_fini);

/**
 * hdlcd_drm_fbdev_restore_mode() - Restores initial framebuffer mode
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct, may be NULL
 *
 * This function is usually called from the DRM drivers lastclose callback.
 */
void hdlcd_drm_fbdev_restore_mode(struct hdlcd_drm_fbdev *hdlcd_fbdev)
{
	if (hdlcd_fbdev)
		drm_fb_helper_restore_fbdev_mode_unlocked(&hdlcd_fbdev->fb_helper);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_restore_mode);

/**
 * hdlcd_drm_fbdev_hotplug_event() - Poll for hotpulug events
 * @hdlcd_fbdev: The hdlcd_drm_fbdev struct, may be NULL
 *
 * This function is usually called from the DRM drivers output_poll_changed
 * callback.
 */
void hdlcd_drm_fbdev_hotplug_event(struct hdlcd_drm_fbdev *hdlcd_fbdev)
{
	if (hdlcd_fbdev)
		drm_fb_helper_hotplug_event(&hdlcd_fbdev->fb_helper);
}
EXPORT_SYMBOL_GPL(hdlcd_drm_fbdev_hotplug_event);


/******************************************************************************
 * IOCTL Interface
 ******************************************************************************/

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
	struct drm_gem_cma_object *obj = hdlcd->fbdev->fb->obj[0];
	struct dma_buf *dma_buf;
	uint32_t fd;

	if (copy_from_user(&ebuf, argp, sizeof(ebuf)))
		return -EFAULT;

	/*
	 * We need a reference on the gem object. This will be released by
	 * drm_gem_dmabuf_release when the file descriptor is closed.
	 */
	drm_gem_object_reference(&obj->base);

	dma_buf = drm_gem_prime_export(helper->dev, &obj->base, ebuf.flags | O_RDWR);
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
	drm_gem_object_unreference(&obj->base);
	return -EFAULT;
}

static int hdlcd_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case FBIOGET_DMABUF:
		return hdlcd_get_dmabuf_ioctl(info, cmd, arg);
	case FBIO_WAITFORVSYNC:
		return 0; /* Nothing to do as we wait when page flipping anyway */
	default:
		printk(KERN_INFO "HDLCD FB does not handle ioctl 0x%x\n", cmd);
	}

	return -EFAULT;
}
