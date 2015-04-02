/*
 * Copyright (C) 2013,2014 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/i2c.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include "hdlcd_drv.h"

struct hdlcd_connector {
	struct drm_connector	connector;
	struct display_timings	*timings;
};

#define conn_to_hdlcd(x) container_of(x, struct hdlcd_connector, connector)

static void hdlcd_connector_destroy(struct drm_connector *connector)
{
	struct hdlcd_connector *hdlcd = conn_to_hdlcd(connector);

	drm_connector_cleanup(connector);
	kfree(hdlcd);
}

static enum drm_connector_status hdlcd_connector_detect(
		struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static const struct drm_connector_funcs hdlcd_connector_funcs = {
	.destroy	= hdlcd_connector_destroy,
	.dpms		= drm_helper_connector_dpms,
	.detect		= hdlcd_connector_detect,
	.fill_modes	= drm_helper_probe_single_connector_modes,
};

static int hdlcd_connector_get_modes(struct drm_connector *connector)
{
	struct hdlcd_connector *hdlcd = conn_to_hdlcd(connector);
	struct display_timings *timings = hdlcd->timings;
	int i;

	for (i = 0; i < timings->num_timings; i++) {
		struct drm_display_mode *mode = drm_mode_create(connector->dev);
		struct videomode vm;

		if (videomode_from_timings(timings, &vm, i))
			break;

		drm_display_mode_from_videomode(&vm, mode);
		mode->type = DRM_MODE_TYPE_DRIVER;
		if (timings->native_mode == i)
			mode->type = DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);
		drm_mode_probed_add(connector, mode);
	}

	return i;
}

static int hdlcd_connector_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	return MODE_OK;
}

static const struct drm_connector_helper_funcs hdlcd_virt_con_helper_funcs = {
	.get_modes	= hdlcd_connector_get_modes,
	.mode_valid	= hdlcd_connector_mode_valid,
	.best_encoder	= hdlcd_connector_best_encoder,
};

static void hdlcd_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
	kfree(encoder);
}

static const struct drm_encoder_funcs hdlcd_encoder_funcs = {
	.destroy = hdlcd_encoder_destroy,
};

static void hdlcd_encoder_dpms(struct drm_encoder *encoder, int mode)
{
}

static bool hdlcd_encoder_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	/* nothing needed */
	return true;
}

static void hdlcd_encoder_prepare(struct drm_encoder *encoder)
{
	hdlcd_encoder_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void hdlcd_encoder_commit(struct drm_encoder *encoder)
{
	hdlcd_encoder_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void hdlcd_encoder_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	/* nothing needed */
}

static const struct drm_encoder_helper_funcs hdlcd_encoder_helper_funcs = {
	.dpms		= hdlcd_encoder_dpms,
	.mode_fixup	= hdlcd_encoder_mode_fixup,
	.prepare	= hdlcd_encoder_prepare,
	.commit		= hdlcd_encoder_commit,
	.mode_set	= hdlcd_encoder_mode_set,
};

int hdlcd_create_virtual_connector(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	struct hdlcd_connector *hdlcdc;
	struct drm_connector *connector;
	int ret;

	encoder = kzalloc(sizeof(*encoder), GFP_KERNEL);
	if (!encoder)
		return -ENOMEM;

	encoder->possible_crtcs = 1;
	encoder->possible_clones = 0;

	ret = drm_encoder_init(dev, encoder, &hdlcd_encoder_funcs,
			DRM_MODE_ENCODER_VIRTUAL);
	if (ret)
		goto encoder_init_err;

	drm_encoder_helper_add(encoder, &hdlcd_encoder_helper_funcs);

	hdlcdc = kzalloc(sizeof(*hdlcdc), GFP_KERNEL);
	if (!hdlcdc) {
		ret = -ENOMEM;
		goto connector_alloc_err;
	}

	hdlcdc->timings = of_get_display_timings(dev->platformdev->dev.of_node);
	if (!hdlcdc->timings) {
		DRM_ERROR("failed to get display panel timings\n");
		ret = -ENXIO;
		goto connector_init_err;
	}

	connector = &hdlcdc->connector;

	/* bogus values, pretend we're a 24" screen */
	connector->display_info.width_mm = 519;
	connector->display_info.height_mm = 324;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->polled = 0;
	ret = drm_connector_init(dev, connector, &hdlcd_connector_funcs,
				DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret)
		goto connector_init_err;

	drm_connector_helper_add(connector, &hdlcd_virt_con_helper_funcs);

	connector->encoder = encoder;
	ret = drm_mode_connector_attach_encoder(connector, encoder);
	if (ret)
		goto attach_err;


	return ret;

attach_err:
	drm_connector_cleanup(connector);
connector_init_err:
	kfree(hdlcdc);
connector_alloc_err:
	drm_encoder_cleanup(encoder);
encoder_init_err:
	kfree(encoder);

	return ret;
};
