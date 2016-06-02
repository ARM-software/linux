/*
 * Copyright (C) 2013-2015 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  Implementation of a CRTC class for the HDLCD driver.
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include "hdlcd_fb_helper.h"
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_plane_helper.h>
#include <linux/clk.h>
#include <linux/of_graph.h>
#include <linux/platform_data/simplefb.h>

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
	struct hdlcd_drm_private *hdlcd = crtc_to_hdlcd_priv(crtc);

	of_node_put(hdlcd->crtc.port);
	drm_crtc_cleanup(crtc);
}

void hdlcd_set_scanout(struct hdlcd_drm_private *hdlcd, bool wait)
{
	struct drm_framebuffer *fb = hdlcd->crtc.primary->fb;
	struct drm_gem_cma_object *gem;
	unsigned int depth, bpp;
	dma_addr_t scanout_start;

	drm_fb_get_bpp_depth(fb->pixel_format, &depth, &bpp);
	gem = hdlcd_fb_get_gem_obj(fb, 0);

	scanout_start = gem->paddr + fb->offsets[0] +
		(hdlcd->crtc.y * fb->pitches[0]) + (hdlcd->crtc.x * bpp/8);

	hdlcd_write(hdlcd, HDLCD_REG_FB_BASE, scanout_start);

	if (wait && hdlcd->dpms == DRM_MODE_DPMS_ON) {
		drm_vblank_get(fb->dev, 0);
		/* Clear any interrupt that may be from before we changed scanout */
		hdlcd_write(hdlcd, HDLCD_REG_INT_CLEAR, HDLCD_INTERRUPT_DMA_END);
		/* Wait for next interrupt so we know scanout change is live */
		reinit_completion(&hdlcd->frame_completion);
		wait_for_completion_interruptible(&hdlcd->frame_completion);

		drm_vblank_put(fb->dev, 0);
	}
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

	if (hdlcd->dpms == DRM_MODE_DPMS_ON) {
		hdlcd_set_scanout(hdlcd, true);
	} else {
		unsigned long flags;

		/* not active, update registers immediately */
		hdlcd_set_scanout(hdlcd, false);
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

static struct simplefb_format supported_formats[] = SIMPLEFB_FORMATS;

static int hdlcd_crtc_colour_set(struct hdlcd_drm_private *hdlcd,
				uint32_t pixel_format)
{
	unsigned int depth, bpp;
	unsigned int default_color = 0x00000000;
	struct simplefb_format *format = NULL;
	int i;

#ifdef HDLCD_SHOW_UNDERRUN
	default_color = 0x00ff0000;	/* show underruns in red */
#endif

	/* Calculate each colour's number of bits */
	drm_fb_get_bpp_depth(pixel_format, &depth, &bpp);

	for (i = 0; i < ARRAY_SIZE(supported_formats); i++) {
		if (supported_formats[i].fourcc == pixel_format)
			format = &supported_formats[i];
	}

	if (!format) {
		DRM_ERROR("Format not supported: 0x%x\n", pixel_format);
		return -EINVAL;
	}

	/* HDLCD uses 'bytes per pixel' */
	bpp = (bpp + 7) / 8;
	hdlcd_write(hdlcd, HDLCD_REG_PIXEL_FORMAT, (bpp - 1) << 3);

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
	if(!config_enabled(CONFIG_ARM)) {
		hdlcd_write(hdlcd, HDLCD_REG_RED_SELECT, default_color |
			format->red.offset | (format->red.length & 0xf) << 8);
		hdlcd_write(hdlcd, HDLCD_REG_GREEN_SELECT, default_color |
			format->green.offset | (format->green.length & 0xf) << 8);
		hdlcd_write(hdlcd, HDLCD_REG_BLUE_SELECT, default_color |
			format->blue.offset | (format->blue.length & 0xf) << 8);
	} else {
		/*
		 * This is a hack to swap read and blue when building for
		 * 32-bit ARM, because Versatile Express motherboard seems
		 * to be wired up differently.
		 */
		hdlcd_write(hdlcd, HDLCD_REG_BLUE_SELECT, default_color |
			format->red.offset | (format->red.length & 0xf) << 8);
		hdlcd_write(hdlcd, HDLCD_REG_GREEN_SELECT, default_color |
			format->green.offset | (format->green.length & 0xf) << 8);
		hdlcd_write(hdlcd, HDLCD_REG_RED_SELECT, default_color |
			format->blue.offset | (format->blue.length & 0xf) << 8);
	}
	return 0;
}

static int hdlcd_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
	struct drm_framebuffer *old_fb)
{
	struct hdlcd_drm_private *hdlcd = crtc_to_hdlcd_priv(crtc);
	hdlcd_set_scanout(hdlcd, true);
	return 0;
}


static int hdlcd_crtc_mode_set(struct drm_crtc *crtc,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode,
			int x, int y, struct drm_framebuffer *oldfb)
{
	struct hdlcd_drm_private *hdlcd = crtc_to_hdlcd_priv(crtc);
	unsigned int depth, bpp, polarities, line_length, err;

	drm_vblank_pre_modeset(crtc->dev, 0);

	polarities = HDLCD_POLARITY_DATAEN | HDLCD_POLARITY_DATA;

	if (adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC)
		polarities |= HDLCD_POLARITY_HSYNC;
	if (adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC)
		polarities |= HDLCD_POLARITY_VSYNC;

	drm_fb_get_bpp_depth(crtc->primary->fb->pixel_format, &depth, &bpp);
	/* switch to the more useful 'bytes per pixel' HDLCD needs */
	bpp = (bpp + 7) / 8;
	line_length = crtc->primary->fb->width * bpp;

	/* Allow max number of outstanding requests and largest burst size */
	hdlcd_write(hdlcd, HDLCD_REG_BUS_OPTIONS,
		    HDLCD_BUS_MAX_OUTSTAND | HDLCD_BUS_BURST_16);

	hdlcd_write(hdlcd, HDLCD_REG_FB_LINE_LENGTH, line_length);
	hdlcd_write(hdlcd, HDLCD_REG_FB_LINE_COUNT,
				crtc->primary->fb->height - 1);
	hdlcd_write(hdlcd, HDLCD_REG_FB_LINE_PITCH, line_length);
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

	err = hdlcd_crtc_colour_set(hdlcd, crtc->primary->fb->pixel_format);
	if (err)
		return err;

	clk_prepare(hdlcd->clk);
	clk_set_rate(hdlcd->clk, mode->crtc_clock * 1000);
	clk_enable(hdlcd->clk);

	hdlcd_set_scanout(hdlcd, false);

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
	.mode_set_base	= hdlcd_crtc_mode_set_base,
	.load_lut	= hdlcd_crtc_load_lut,
};

int hdlcd_setup_crtc(struct drm_device *dev)
{
	struct hdlcd_drm_private *hdlcd = dev->dev_private;
	int ret;

	hdlcd->crtc.port = of_graph_get_next_endpoint(dev->platformdev->dev.of_node, NULL);
	if (!hdlcd->crtc.port)
		return -ENXIO;

	ret = drm_crtc_init(dev, &hdlcd->crtc, &hdlcd_crtc_funcs);
	if (ret < 0) {
		of_node_put(hdlcd->crtc.port);
		return ret;
	}

	drm_crtc_helper_add(&hdlcd->crtc, &hdlcd_crtc_helper_funcs);
	return 0;
}

