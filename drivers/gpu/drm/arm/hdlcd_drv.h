/*
 *  ARM HDLCD Controller register definition
 */

#ifndef __HDLCD_DRV_H__
#define __HDLCD_DRV_H__

struct hdlcd_drm_private {
	void __iomem			*mmio;
	struct clk			*clk;
	struct hdlcd_drm_fbdev		*fbdev;
	struct drm_framebuffer		*fb;
	struct drm_pending_vblank_event	*event;
	struct drm_crtc			crtc;
	struct completion		frame_completion;
#ifdef CONFIG_DEBUG_FS
	atomic_t buffer_underrun_count;
	atomic_t bus_error_count;
	atomic_t vsync_count;
	atomic_t dma_end_count;
#endif
	int dpms;
};

#define crtc_to_hdlcd_priv(x)	container_of(x, struct hdlcd_drm_private, crtc)

static inline void
hdlcd_write(struct hdlcd_drm_private *hdlcd, unsigned int reg, u32 value)
{
	writel(value, hdlcd->mmio + reg);
}

static inline u32 hdlcd_read(struct hdlcd_drm_private *hdlcd, unsigned int reg)
{
	return readl(hdlcd->mmio + reg);
}

/*
 * Developers using HDLCD may wish to enable these settings if
 * display disruption is apparent and you suspect HDLCD
 * access to RAM may be starved.
 *
 * Turn HDLCD default color to red instead of default black so
 * that it's easier to see data underruns (compared to other
 * visual disruptions)
 */
//#define HDLCD_SHOW_UNDERRUN

/* setup the crtc subclass */
int hdlcd_setup_crtc(struct drm_device *dev);

/* functions for creating a suitable connector */
extern int hdlcd_create_digital_connector(struct drm_device *dev,
					struct hdlcd_drm_private *hdlcd);
extern int hdlcd_create_vexpress_connector(struct drm_device *dev,
					struct hdlcd_drm_private *hdlcd);
#ifdef CONFIG_DRM_VIRTUAL_HDLCD
extern int hdlcd_create_virtual_connector(struct drm_device *dev);
#else
static inline int hdlcd_create_virtual_connector(struct drm_device *dev)
{
	return -ENXIO;
}
#endif

/* common function used by all connectors */
extern struct drm_encoder *hdlcd_connector_best_encoder(struct drm_connector *con);

#endif /* __HDLCD_DRV_H__ */
