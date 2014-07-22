/*
 * Copyright (C) 2013,2014 ARM Limited
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
#include <linux/completion.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "hdlcd_drv.h"
#include "hdlcd_regs.h"


static int hdlcd_unload(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;

	drm_kms_helper_poll_fini(dev);
	if (hdlcd->fb_helper)
		drm_fb_helper_fini(hdlcd->fb_helper);

	drm_vblank_cleanup(dev);
	drm_mode_config_cleanup(dev);

	drm_irq_uninstall(dev);

	if (!IS_ERR(hdlcd->clk))
		clk_put(hdlcd->clk);

	platform_set_drvdata(dev->platformdev, NULL);

	if (hdlcd->mmio)
		iounmap(hdlcd->mmio);

	dev->dev_private = NULL;
	kfree(hdlcd);

	return 0;
}

static int hdlcd_load(struct drm_device *dev, unsigned long flags)
{
	struct platform_device *pdev = dev->platformdev;
	struct hdlcd_drm_private *hdlcd;
	struct resource *res;
	phandle slave_phandle;
	u32 version;
	int ret;

	hdlcd = kzalloc(sizeof(*hdlcd), GFP_KERNEL);
	if (!hdlcd) {
		dev_err(dev->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

#ifdef CONFIG_DEBUG_FS
	atomic_set(&hdlcd->buffer_underrun_count, 0);
	atomic_set(&hdlcd->bus_error_count, 0);
	atomic_set(&hdlcd->vsync_count, 0);
	atomic_set(&hdlcd->dma_end_count, 0);
#endif
	hdlcd->initialised = false;
	dev->dev_private = hdlcd;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev->dev, "failed to get memory resource\n");
		ret = -EINVAL;
		goto fail;
	}

	hdlcd->mmio = ioremap_nocache(res->start, resource_size(res));
	if (!hdlcd->mmio) {
		dev_err(dev->dev, "failed to map control registers area\n");
		ret = -ENOMEM;
		goto fail;
	}

	hdlcd->clk = clk_get(dev->dev, "pxlclk");
	if (IS_ERR(hdlcd->clk)) {
		dev_err(dev->dev, "unable to get an usable clock\n");
		ret = PTR_ERR(hdlcd->clk);
		goto fail;
	}

	if (of_property_read_u32(pdev->dev.of_node, "i2c-slave", &slave_phandle)) {
		dev_warn(dev->dev, "no i2c-slave handle provided, disabling physical connector\n");
		hdlcd->slave_node = NULL;
	} else
		hdlcd->slave_node = of_find_node_by_phandle(slave_phandle);

	version = hdlcd_read(hdlcd, HDLCD_REG_VERSION);
		if ((version & HDLCD_PRODUCT_MASK) != HDLCD_PRODUCT_ID) {
		dev_err(dev->dev, "unknown product id: 0x%x\n", version);
		ret = -EINVAL;
		goto fail;
	}
	dev_info(dev->dev, "found ARM HDLCD version r%dp%d\n",
		(version & HDLCD_VERSION_MAJOR_MASK) >> 8,
		version & HDLCD_VERSION_MINOR_MASK);

	ret = hdlcd_setup_crtc(dev);
	if (ret < 0) {
		dev_err(dev->dev, "failed to create crtc\n");
		goto fail;
	}

	/*
	 * It only makes sense to create the virtual connector if we don't have
	 * a physical way of controlling output
	 */
	if (hdlcd->slave_node) {
		ret = hdlcd_create_digital_connector(dev, hdlcd);
		if (ret < 0) {
			dev_err(dev->dev, "failed to create digital connector, trying board setup: %d\n", ret);
			ret = hdlcd_create_vexpress_connector(dev, hdlcd);
		}

		if (ret < 0) {
			dev_err(dev->dev, "failed to create board connector: %d\n", ret);
			goto fail;
		}
	} else {
		ret = hdlcd_create_virtual_connector(dev);
		if (ret < 0) {
			dev_err(dev->dev, "failed to create virtual connector: %d\n", ret);
			goto fail;
		}
	}

	ret = hdlcd_fbdev_init(dev);
	if (ret < 0) {
		dev_err(dev->dev, "failed to init the framebuffer (%d)\n", ret);
		goto fail;
	}

	platform_set_drvdata(pdev, dev);

	ret = drm_irq_install(dev);
	if (ret < 0) {
		dev_err(dev->dev, "failed to install IRQ handler\n");
		goto fail;
	}

	init_completion(&hdlcd->frame_completion);
	ret = drm_vblank_init(dev, 1);
	if (ret < 0) {
		dev_err(dev->dev, "failed to initialise vblank\n");
		goto fail;
	} else {
		dev_info(dev->dev, "initialised vblank\n");
	}

	drm_kms_helper_poll_init(dev);

	return 0;

fail:
	hdlcd_unload(dev);
	return ret;
}

static void hdlcd_preclose(struct drm_device *dev, struct drm_file *file)
{
}

static void hdlcd_lastclose(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;

	drm_modeset_lock_all(dev);
	if (hdlcd->fb_helper)
		drm_fb_helper_restore_fbdev_mode(hdlcd->fb_helper);
	drm_modeset_unlock_all(dev);
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
	if (irq_status & HDLCD_INTERRUPT_DMA_END) {
		// send completion when reading the frame has finished
		complete_all(&hdlcd->frame_completion);
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
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	unsigned int irq_mask = hdlcd_read(hdlcd, HDLCD_REG_INT_MASK);

#ifdef CONFIG_DEBUG_FS
	/* enable debug interrupts */
	irq_mask |= HDLCD_DEBUG_INT_MASK;
#endif

	/* enable DMA completion interrupts */
	irq_mask |= HDLCD_INTERRUPT_DMA_END;
	hdlcd_write(hdlcd, HDLCD_REG_INT_MASK, irq_mask);

	return 0;
}

static void hdlcd_irq_uninstall(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	/* disable all the interrupts that we might have enabled */
	unsigned int irq_mask = hdlcd_read(hdlcd, HDLCD_REG_INT_MASK);

#ifdef CONFIG_DEBUG_FS
	/* disable debug interrupts */
	irq_mask &= ~HDLCD_DEBUG_INT_MASK;
#endif

	/* disable vsync and dma interrupts */
	irq_mask &= ~(HDLCD_INTERRUPT_VSYNC | HDLCD_INTERRUPT_DMA_END);

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

static const struct file_operations hdlcd_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.mmap		= drm_gem_cma_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.llseek		= no_llseek,
};

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
	.gem_prime_get_sg_table	= drm_gem_cma_prime_get_sg_table,
	.gem_prime_vmap		= drm_gem_cma_prime_vmap,
	.gem_prime_vunmap	= drm_gem_cma_prime_vunmap,
	.gem_prime_mmap		= drm_gem_cma_prime_mmap,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init		= hdlcd_debugfs_init,
	.debugfs_cleanup	= hdlcd_debugfs_cleanup,
#endif
	.fops			= &hdlcd_fops,

	.name			= "hdlcd",
	.desc			= "ARM HDLCD Controller DRM",
	.date			= "20130505",
	.major			= 1,
	.minor			= 0,
};


static int hdlcd_probe(struct platform_device *pdev)
{
	return drm_platform_init(&hdlcd_driver, pdev);
}

static int hdlcd_remove(struct platform_device *pdev)
{
	drm_put_dev(platform_get_drvdata(pdev));
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
	return platform_driver_register(&hdlcd_platform_driver);
}

static void __exit hdlcd_exit(void)
{
	platform_driver_unregister(&hdlcd_platform_driver);
}

module_init(hdlcd_init);
module_exit(hdlcd_exit);

MODULE_AUTHOR("Liviu Dudau");
MODULE_DESCRIPTION("ARM HDLCD DRM driver");
MODULE_LICENSE("GPL v2");
