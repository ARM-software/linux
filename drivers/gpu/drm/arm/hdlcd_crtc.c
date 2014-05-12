/*
 * Copyright (C) 2013,2014 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  Implementation of a CRTC class for the HDLCD driver.
 */

#include <linux/clk.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "hdlcd_drv.h"
#include "hdlcd_regs.h"

/*
 * The HDLCD controller is a dumb RGB streamer that gets connected to
 * a single HDMI transmitter or in the case of the ARM Models it gets
 * emulated by the software that does the actual rendering.
 *
 */
static void hdlcd_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

void hdlcd_set_scanout(struct hdlcd_drm_private *hdlcd)
{
	struct drm_framebuffer *fb = hdlcd->crtc.primary->fb;
	struct drm_gem_cma_object *gem;
	unsigned int depth, bpp;
	dma_addr_t scanout_start;

	drm_fb_get_bpp_depth(fb->pixel_format, &depth, &bpp);
	gem = drm_fb_cma_get_gem_obj(fb, 0);

	scanout_start = gem->paddr + fb->offsets[0] +
		(hdlcd->crtc.y * fb->pitches[0]) + (hdlcd->crtc.x * bpp/8);

	hdlcd_write(hdlcd, HDLCD_REG_FB_BASE, scanout_start);
}

static int hdlcd_crtc_page_flip(struct drm_crtc *crtc,
			struct drm_framebuffer *fb,
			struct drm_pending_vblank_event *event,
			uint32_t flags)
{
	struct hdlcd_drm_private *hdlcd = crtc_to_hdlcd_priv(crtc);

	if (hdlcd->dpms == DRM_MODE_DPMS_ON) {
		/* don't schedule any page flipping if one is in progress */
		if (hdlcd->event)
			return -EBUSY;

		hdlcd->event = event;
		drm_vblank_get(crtc->dev, 0);
	}

	crtc->primary->fb = fb;

	if (hdlcd->dpms != DRM_MODE_DPMS_ON) {
		unsigned long flags;

		/* not active, update registers immediately */
		hdlcd_set_scanout(hdlcd);
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		if (event)
			drm_send_vblank_event(crtc->dev, 0, event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}

	return 0;
}

static const struct drm_crtc_funcs hdlcd_crtc_funcs = {
	.destroy	= hdlcd_crtc_destroy,
	.set_config	= drm_crtc_helper_set_config,
	.page_flip	= hdlcd_crtc_page_flip,
};

static void hdlcd_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct hdlcd_drm_private *hdlcd = crtc_to_hdlcd_priv(crtc);

	hdlcd->dpms = mode;
	if (mode == DRM_MODE_DPMS_ON)
		hdlcd_write(hdlcd, HDLCD_REG_COMMAND, 1);
	else
		hdlcd_write(hdlcd, HDLCD_REG_COMMAND, 0);
}

static bool hdlcd_crtc_mode_fixup(struct drm_crtc *crtc,
			const struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void hdlcd_crtc_prepare(struct drm_crtc *crtc)
{
	hdlcd_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
}

static void hdlcd_crtc_commit(struct drm_crtc *crtc)
{
	drm_vblank_post_modeset(crtc->dev, 0);
	hdlcd_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
}

static int hdlcd_crtc_mode_set(struct drm_crtc *crtc,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode,
			int x, int y, struct drm_framebuffer *oldfb)
{
	struct hdlcd_drm_private *hdlcd = crtc_to_hdlcd_priv(crtc);
	unsigned int depth, bpp, polarities;
	unsigned char red_width = 0, green_width = 0, blue_width = 0, alpha_width = 0;
	unsigned int default_color = 0x00000000;

#ifdef HDLCD_SHOW_UNDERRUN
	default_color = 0x00ff000000;
#endif

	drm_vblank_pre_modeset(crtc->dev, 0);

	/* Preset the number of bits per colour */
	drm_fb_get_bpp_depth(crtc->primary->fb->pixel_format, &depth, &bpp);
	switch (depth) {
	case 32:
		alpha_width = 8;
	case 24:
	case 8:	 /* pseudocolor */
		red_width = 8; green_width = 8; blue_width = 8;
		break;
	case 16: /* 565 format */
		red_width = 5; green_width = 6; blue_width = 5;
		break;
	}

	/* switch to using the more useful bytes per pixel */
	bpp = (bpp + 7) / 8;

	polarities = HDLCD_POLARITY_DATAEN | HDLCD_POLARITY_DATA;

	if (adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC)
		polarities |= HDLCD_POLARITY_HSYNC;
	if (adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC)
		polarities |= HDLCD_POLARITY_VSYNC;

	/* Allow max number of outstanding requests and largest burst size */
	hdlcd_write(hdlcd, HDLCD_REG_BUS_OPTIONS,
		    HDLCD_BUS_MAX_OUTSTAND | HDLCD_BUS_BURST_16);

	hdlcd_write(hdlcd, HDLCD_REG_PIXEL_FORMAT, (bpp - 1) << 3);

	hdlcd_write(hdlcd, HDLCD_REG_FB_LINE_LENGTH, crtc->primary->fb->width * bpp);
	hdlcd_write(hdlcd, HDLCD_REG_FB_LINE_COUNT, crtc->primary->fb->height - 1);
	hdlcd_write(hdlcd, HDLCD_REG_FB_LINE_PITCH, crtc->primary->fb->width * bpp);
	hdlcd_write(hdlcd, HDLCD_REG_V_BACK_PORCH,
				mode->vtotal - mode->vsync_end - 1);
	hdlcd_write(hdlcd, HDLCD_REG_V_FRONT_PORCH,
				mode->vsync_start - mode->vdisplay - 1);
	hdlcd_write(hdlcd, HDLCD_REG_V_SYNC,
				mode->vsync_end - mode->vsync_start - 1);
	hdlcd_write(hdlcd, HDLCD_REG_V_DATA, mode->vdisplay - 1);
	hdlcd_write(hdlcd, HDLCD_REG_H_BACK_PORCH,
				mode->htotal - mode->hsync_end - 1);
	hdlcd_write(hdlcd, HDLCD_REG_H_FRONT_PORCH,
				mode->hsync_start - mode->hdisplay - 1);
	hdlcd_write(hdlcd, HDLCD_REG_H_SYNC,
				mode->hsync_end - mode->hsync_start - 1);
	hdlcd_write(hdlcd, HDLCD_REG_H_DATA, mode->hdisplay - 1);
	hdlcd_write(hdlcd, HDLCD_REG_POLARITIES, polarities);

	/*
	 * The format of the HDLCD_REG_<color>_SELECT register is:
	 *   - bits[23:16] - default value for that color component
	 *   - bits[11:8]  - number of bits to extract for each color component
	 *   - bits[4:0]   - index of the lowest bit to extract
	 *
	 * The default color value is used when bits[11:8] read zero, when the
	 * pixel is outside the visible frame area or when there is a
	 * buffer underrun.
	 */
	hdlcd_write(hdlcd, HDLCD_REG_BLUE_SELECT, default_color |
		alpha_width |   /* offset */
		(blue_width & 0xf) << 8);
	hdlcd_write(hdlcd, HDLCD_REG_GREEN_SELECT, default_color |
		(blue_width + alpha_width) |  /* offset */
		((green_width & 0xf) << 8));
	hdlcd_write(hdlcd, HDLCD_REG_RED_SELECT, default_color |
		(blue_width + green_width + alpha_width) |  /* offset */
		((red_width & 0xf) << 8));

	clk_prepare(hdlcd->clk);
	clk_set_rate(hdlcd->clk, mode->crtc_clock * 1000);
	clk_enable(hdlcd->clk);

	hdlcd_set_scanout(hdlcd);

	return 0;
}

static void hdlcd_crtc_load_lut(struct drm_crtc *crtc)
{
}

static const struct drm_crtc_helper_funcs hdlcd_crtc_helper_funcs = {
	.dpms		= hdlcd_crtc_dpms,
	.mode_fixup	= hdlcd_crtc_mode_fixup,
	.prepare	= hdlcd_crtc_prepare,
	.commit		= hdlcd_crtc_commit,
	.mode_set	= hdlcd_crtc_mode_set,
	.load_lut	= hdlcd_crtc_load_lut,
};

static void hdlcd_fb_output_poll_changed(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	if (hdlcd->fbdev)
		drm_fbdev_cma_hotplug_event(hdlcd->fbdev);
}

static const struct drm_mode_config_funcs hdlcd_mode_config_funcs = {
	.fb_create = drm_fb_cma_create,
	.output_poll_changed = hdlcd_fb_output_poll_changed,
};

int hdlcd_setup_crtc(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	int ret;

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = HDLCD_MAX_XRES;
	dev->mode_config.max_height = HDLCD_MAX_YRES;
	dev->mode_config.funcs = &hdlcd_mode_config_funcs;

	ret = drm_crtc_init(dev, &hdlcd->crtc, &hdlcd_crtc_funcs);
	if (ret < 0)
		goto crtc_setup_err;

	drm_crtc_helper_add(&hdlcd->crtc, &hdlcd_crtc_helper_funcs);

	return 0;

crtc_setup_err:
	drm_mode_config_cleanup(dev);

	return ret;
}

