/*
 * Copyright (C) 2013-2015 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  ARM HDLCD Driver
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_graph.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_of.h>

#include "hdlcd_drv.h"
#include "hdlcd_regs.h"

static void hdlcd_setup_mode_config(struct drm_device *dev);

static int compare_dev(struct device *dev, void *data)
{
	return dev->of_node == data;
}

struct drm_encoder *
hdlcd_connector_best_encoder(struct drm_connector *connector)
{
	int enc_id = connector->encoder_ids[0];
	struct drm_mode_object *obj;
	struct drm_encoder *encoder;

	if (connector->encoder)
		return connector->encoder;

	if (enc_id) {
		obj = drm_mode_object_find(connector->dev, enc_id,
					DRM_MODE_OBJECT_ENCODER);
		if (obj) {
			encoder = obj_to_encoder(obj);
			return encoder;
		}
	}
	return NULL;

}

static int hdlcd_unload(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;

	drm_kms_helper_poll_fini(dev);
	if (hdlcd->fbdev)
		drm_fbdev_cma_fini(hdlcd->fbdev);

	drm_vblank_cleanup(dev);
	drm_mode_config_cleanup(dev);

	drm_irq_uninstall(dev);

	if (!IS_ERR(hdlcd->clk))
		clk_put(hdlcd->clk);

	dma_release_declared_memory(dev->dev);
	platform_set_drvdata(dev->platformdev, NULL);
	dev->dev_private = NULL;

	return 0;
}

static int hdlcd_load(struct drm_device *dev, unsigned long flags)
{
	struct platform_device *pdev = dev->platformdev;
	struct hdlcd_drm_private *hdlcd;
	struct resource *res;
	u32 version;
	int ret;

	hdlcd = devm_kzalloc(dev->dev, sizeof(*hdlcd), GFP_KERNEL);
	if (!hdlcd)
		return -ENOMEM;

#ifdef CONFIG_DEBUG_FS
	atomic_set(&hdlcd->buffer_underrun_count, 0);
	atomic_set(&hdlcd->bus_error_count, 0);
	atomic_set(&hdlcd->vsync_count, 0);
	atomic_set(&hdlcd->dma_end_count, 0);
#endif
	platform_set_drvdata(pdev, dev);
	dev->dev_private = hdlcd;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hdlcd->mmio = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hdlcd->mmio)) {
		DRM_ERROR("failed to map control registers area\n");
		ret = PTR_ERR(hdlcd->mmio);
		goto fail;
	}

	version = hdlcd_read(hdlcd, HDLCD_REG_VERSION);
	if ((version & HDLCD_PRODUCT_MASK) != HDLCD_PRODUCT_ID) {
		DRM_ERROR("unknown product id: 0x%x\n", version);
		ret = -EINVAL;
		goto fail;
	}
	DRM_INFO("found ARM HDLCD version r%dp%d\n",
		(version & HDLCD_VERSION_MAJOR_MASK) >> 8,
		version & HDLCD_VERSION_MINOR_MASK);

	/* Get the optional coherent memory resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		ret = dma_declare_coherent_memory(dev->dev, res->start, res->start,
					resource_size(res), DMA_MEMORY_MAP);
		if ((ret & DMA_MEMORY_MAP) == 0) {
			DRM_ERROR("failed to declare coherent device memory\n");
			ret = -ENXIO;
			goto fail;
		}
	}

	hdlcd_setup_mode_config(dev);
	ret = hdlcd_setup_crtc(dev);
	if (ret < 0) {
		DRM_ERROR("failed to create crtc\n");
		goto fail;
	}

	ret = component_bind_all(dev->dev, dev);
	if (ret) {
		DRM_ERROR("Failed to bind all components\n");
		goto fail;
	}

	drm_kms_helper_poll_init(dev);
	drm_mode_config_reset(dev);

	ret = drm_irq_install(dev, platform_get_irq(pdev, 0));
	if (ret < 0) {
		DRM_ERROR("failed to install IRQ handler\n");
		goto fail;
	}

	dev->irq_enabled = true;
	dev->vblank_disable_allowed = true;

	ret = drm_vblank_init(dev, dev->mode_config.num_crtc);
	if (ret < 0) {
		DRM_ERROR("failed to initialise vblank\n");
		goto fail;
	}
	hdlcd->fbdev = drm_fbdev_cma_init(dev, 32,
					dev->mode_config.num_crtc,
					dev->mode_config.num_connector);

	return 0;

fail:
	dev->dev_private = NULL;
	platform_set_drvdata(dev->platformdev, NULL);
	return ret;
}

static void hdlcd_fb_output_poll_changed(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	if (hdlcd->fbdev) {
		drm_fbdev_cma_hotplug_event(hdlcd->fbdev);
	}
}

static const struct drm_mode_config_funcs hdlcd_mode_config_funcs = {
	.fb_create = drm_fb_cma_create,
	.output_poll_changed = hdlcd_fb_output_poll_changed,
};

static void hdlcd_setup_mode_config(struct drm_device *dev)
{
	drm_mode_config_init(dev);
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = HDLCD_MAX_XRES;
	dev->mode_config.max_height = HDLCD_MAX_YRES;
	dev->mode_config.funcs = &hdlcd_mode_config_funcs;
}

static void hdlcd_preclose(struct drm_device *dev, struct drm_file *file)
{
}

static void hdlcd_lastclose(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	drm_fbdev_cma_restore_mode(hdlcd->fbdev);
}

static irqreturn_t hdlcd_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	unsigned long irq_status;

	irq_status = hdlcd_read(hdlcd, HDLCD_REG_INT_STATUS);

#ifdef CONFIG_DEBUG_FS
	if (irq_status & HDLCD_INTERRUPT_UNDERRUN) {
		atomic_inc(&hdlcd->buffer_underrun_count);
	}
	if (irq_status & HDLCD_INTERRUPT_DMA_END) {
		atomic_inc(&hdlcd->dma_end_count);
	}
	if (irq_status & HDLCD_INTERRUPT_BUS_ERROR) {
		atomic_inc(&hdlcd->bus_error_count);
	}
	if (irq_status & HDLCD_INTERRUPT_VSYNC) {
		atomic_inc(&hdlcd->vsync_count);
	}
#endif
	if (irq_status & HDLCD_INTERRUPT_VSYNC) {
		struct drm_pending_vblank_event *event;
		unsigned long flags;

		hdlcd_set_scanout(hdlcd);

		drm_handle_vblank(dev, 0);

		spin_lock_irqsave(&dev->event_lock, flags);
		if (hdlcd->event) {
			event = hdlcd->event;
			hdlcd->event = NULL;
			drm_send_vblank_event(dev, 0, event);
			drm_vblank_put(dev, 0);
		}
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	/* acknowledge interrupt(s) */
	hdlcd_write(hdlcd, HDLCD_REG_INT_CLEAR, irq_status);

	return IRQ_HANDLED;
}

static void hdlcd_irq_preinstall(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	/* Ensure interrupts are disabled */
	hdlcd_write(hdlcd, HDLCD_REG_INT_MASK, 0);
	hdlcd_write(hdlcd, HDLCD_REG_INT_CLEAR, ~0);
}

static int hdlcd_irq_postinstall(struct drm_device *dev)
{
#ifdef CONFIG_DEBUG_FS
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	unsigned long irq_mask = hdlcd_read(hdlcd, HDLCD_REG_INT_MASK);

	/* enable debug interrupts */
	irq_mask |= HDLCD_DEBUG_INT_MASK;

	hdlcd_write(hdlcd, HDLCD_REG_INT_MASK, irq_mask);
#endif
	return 0;
}

static void hdlcd_irq_uninstall(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	/* disable all the interrupts that we might have enabled */
	unsigned long irq_mask = hdlcd_read(hdlcd, HDLCD_REG_INT_MASK);

#ifdef CONFIG_DEBUG_FS
	/* disable debug interrupts */
	irq_mask &= ~HDLCD_DEBUG_INT_MASK;
#endif

	/* disable vsync interrupts */
	irq_mask &= ~HDLCD_INTERRUPT_VSYNC;

	hdlcd_write(hdlcd, HDLCD_REG_INT_MASK, irq_mask);
}

static int hdlcd_enable_vblank(struct drm_device *dev, int crtc)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	unsigned int mask = hdlcd_read(hdlcd, HDLCD_REG_INT_MASK);

	hdlcd_write(hdlcd, HDLCD_REG_INT_MASK, mask | HDLCD_INTERRUPT_VSYNC);

	return 0;
}

static void hdlcd_disable_vblank(struct drm_device *dev, int crtc)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	unsigned int mask = hdlcd_read(hdlcd, HDLCD_REG_INT_MASK);

	hdlcd_write(hdlcd, HDLCD_REG_INT_MASK, mask & ~HDLCD_INTERRUPT_VSYNC);
}

#ifdef CONFIG_DEBUG_FS
static int hdlcd_show_underrun_count(struct seq_file *m, void *arg)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct hdlcd_drm_private *hdlcd = dev->dev_private;

	seq_printf(m, "underrun : %d\n", atomic_read(&hdlcd->buffer_underrun_count));
	seq_printf(m, "dma_end  : %d\n", atomic_read(&hdlcd->dma_end_count));
	seq_printf(m, "bus_error: %d\n", atomic_read(&hdlcd->bus_error_count));
	seq_printf(m, "vsync    : %d\n", atomic_read(&hdlcd->vsync_count));
	return 0;
}

static struct drm_info_list hdlcd_debugfs_list[] = {
	{ "interrupt_count", hdlcd_show_underrun_count, 0 },
};

static int hdlcd_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(hdlcd_debugfs_list,
		ARRAY_SIZE(hdlcd_debugfs_list),	minor->debugfs_root, minor);
}

static void hdlcd_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(hdlcd_debugfs_list,
		ARRAY_SIZE(hdlcd_debugfs_list), minor);
}
#endif

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
	.mmap		= drm_gem_cma_mmap,
};

struct sg_table *hdlcd_gem_cma_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct sg_table *sgt;

	sgt = drm_gem_cma_prime_get_sg_table(obj);
	if (sgt) {
		struct drm_gem_cma_object *cma_obj;

		cma_obj = to_drm_gem_cma_obj(obj);
		sg_dma_address(sgt->sgl) = cma_obj->paddr;
		sg_set_page(sgt->sgl, pfn_to_page(PFN_DOWN(cma_obj->paddr)),
				PAGE_ALIGN(obj->size), 0);
	}

	return sgt;
}

static struct drm_driver hdlcd_driver = {
	.driver_features	= DRIVER_HAVE_IRQ | DRIVER_GEM |
					DRIVER_MODESET | DRIVER_PRIME,
	.load			= hdlcd_load,
	.unload			= hdlcd_unload,
	.preclose		= hdlcd_preclose,
	.lastclose		= hdlcd_lastclose,
	.irq_handler		= hdlcd_irq,
	.irq_preinstall		= hdlcd_irq_preinstall,
	.irq_postinstall	= hdlcd_irq_postinstall,
	.irq_uninstall		= hdlcd_irq_uninstall,
	.get_vblank_counter	= drm_vblank_count,
	.enable_vblank		= hdlcd_enable_vblank,
	.disable_vblank		= hdlcd_disable_vblank,
	.gem_free_object	= drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
	.dumb_create		= drm_gem_cma_dumb_create,
	.dumb_map_offset	= drm_gem_cma_dumb_map_offset,
	.dumb_destroy		= drm_gem_dumb_destroy,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_export	= drm_gem_prime_export,
	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_get_sg_table	= hdlcd_gem_cma_prime_get_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init		= hdlcd_debugfs_init,
	.debugfs_cleanup	= hdlcd_debugfs_cleanup,
#endif
	.fops			= &fops,
	.name			= "hdlcd",
	.desc			= "ARM HDLCD Controller DRM",
	.date			= "20130505",
	.major			= 1,
	.minor			= 0,
};

static int hdlcd_add_components(struct device *dev, struct master *master)
{
	struct device_node *port, *ep = NULL;
	int ret = -ENXIO;

	if (!dev->of_node)
		return -ENODEV;

	do {
		ep = of_graph_get_next_endpoint(dev->of_node, ep);
		if (!ep)
			break;

		if (!of_device_is_available(ep)) {
			of_node_put(ep);
			continue;
		}

		port = of_graph_get_remote_port_parent(ep);
		of_node_put(ep);
		if (!port || !of_device_is_available(port)) {
			of_node_put(port);
			continue;
		}

		ret = component_master_add_child(master, compare_dev, port);
		of_node_put(port);
	} while (1);

	return ret;
}

static int hdlcd_drm_bind(struct device *dev)
{
	return drm_platform_init(&hdlcd_driver, to_platform_device(dev));
}

static void hdlcd_drm_unbind(struct device *dev)
{
	drm_put_dev(dev_get_drvdata(dev));
}

static const struct component_master_ops hdlcd_master_ops = {
	.add_components	= hdlcd_add_components,
	.bind		= hdlcd_drm_bind,
	.unbind		= hdlcd_drm_unbind,
};

static int hdlcd_probe(struct platform_device *pdev)
{
	return component_master_add(&pdev->dev, &hdlcd_master_ops);
}

static int hdlcd_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &hdlcd_master_ops);
	return 0;
}

static struct of_device_id  hdlcd_of_match[] = {
	{ .compatible	= "arm,hdlcd" },
	{},
};
MODULE_DEVICE_TABLE(of, hdlcd_of_match);

static struct platform_driver hdlcd_platform_driver = {
	.probe		= hdlcd_probe,
	.remove		= hdlcd_remove,
	.driver	= {
		.name		= "hdlcd",
		.owner		= THIS_MODULE,
		.of_match_table	= hdlcd_of_match,
	},
};

static int __init hdlcd_init(void)
{
	int err = platform_driver_register(&hdlcd_platform_driver);

#ifdef HDLCD_COUNT_BUFFERUNDERRUNS
	if (!err)
		hdlcd_underrun_init();
#endif

	return err;
}

static void __exit hdlcd_exit(void)
{
#ifdef HDLCD_COUNT_BUFFERUNDERRUNS
	hdlcd_underrun_close();
#endif
	platform_driver_unregister(&hdlcd_platform_driver);
}

module_init(hdlcd_init);
module_exit(hdlcd_exit);

MODULE_AUTHOR("Liviu Dudau");
MODULE_DESCRIPTION("ARM HDLCD DRM driver");
MODULE_LICENSE("GPL v2");
