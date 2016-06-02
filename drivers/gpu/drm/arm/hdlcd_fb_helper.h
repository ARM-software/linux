#ifndef __DRM_FB_CMA_HELPER_H__
#define __DRM_FB_CMA_HELPER_H__

struct hdlcd_drm_fbdev;
struct drm_gem_cma_object;

struct drm_framebuffer;
struct drm_device;
struct drm_file;
struct drm_mode_fb_cmd2;

struct hdlcd_drm_fbdev *hdlcd_drm_fbdev_init(struct drm_device *dev,
	unsigned int preferred_bpp, unsigned int num_crtc,
	unsigned int max_conn_count);
void hdlcd_drm_fbdev_fini(struct hdlcd_drm_fbdev *hdlcd_fbdev);

void hdlcd_drm_fbdev_restore_mode(struct hdlcd_drm_fbdev *hdlcd_fbdev);
void hdlcd_drm_fbdev_hotplug_event(struct hdlcd_drm_fbdev *hdlcd_fbdev);

struct drm_framebuffer *hdlcd_fb_create(struct drm_device *dev,
	struct drm_file *file_priv, struct drm_mode_fb_cmd2 *mode_cmd);

struct drm_gem_cma_object *hdlcd_fb_get_gem_obj(struct drm_framebuffer *fb,
	unsigned int plane);

#ifdef CONFIG_DEBUG_FS
int hdlcd_fb_debugfs_show(struct seq_file *m, void *arg);
#endif

#endif

