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
#include <drm/drm_encoder_slave.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <linux/vexpress.h>

#include "hdlcd_drv.h"

/*
 * use the functionality of vexpress-config to set the output mode
 * for HDLCD on Versatile Express boards that lack proper control
 * of the DDC i2c chip.
 */

//static struct vexpress_config_func *vconfig_func;

/*
 * Predefined modes that are available through the VExpress micro
 */
static const struct {
	int hsize, vsize, vrefresh, dvimode;
} vexpress_dvimodes[] = {
	{  640,  480, 60, 0 }, /* VGA */
	{  800,  600, 60, 1 }, /* SVGA */
	{ 1024,  768, 60, 2 }, /* XGA */
	{ 1280, 1024, 60, 3 }, /* SXGA */
	{ 1600, 1200, 60, 4 }, /* UXGA */
	{ 1920, 1080, 60, 5 }, /* HD1080 */
};

static void hdlcd_connector_destroy(struct drm_connector *connector)
{
}

static enum drm_connector_status
hdlcd_connector_detect(struct drm_connector *connector, bool force)
{
	//if (vconfig_func)
		return connector_status_connected;
		//return connector_status_disconnected;
}

static const struct drm_connector_funcs hdlcd_connector_funcs = {
	.destroy = hdlcd_connector_destroy,
	.dpms = drm_helper_connector_dpms,
	.detect = hdlcd_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
};

static int hdlcd_vexpress_con_get_modes(struct drm_connector *connector)
{
	int i;
	struct drm_display_mode *mode;

	/* Add the predefined modes */
	for (i = 0; i < ARRAY_SIZE(vexpress_dvimodes); i++) {
		mode = drm_mode_find_dmt(connector->dev,
					vexpress_dvimodes[i].hsize,
					vexpress_dvimodes[i].vsize,
					vexpress_dvimodes[i].vrefresh, false);
		if (!mode)
			continue;
		/* prefer the 1280x1024 mode */
		if (i == 3)
			mode->type |= DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
	}

	return i;
}

/*
 * mode valid is only called for detected modes and we know that
 * the restricted list is correct ;)
 */
static int hdlcd_vexpress_con_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	return MODE_OK;
}

static const struct drm_connector_helper_funcs hdlcd_vexpress_con_helper_funcs = {
	.get_modes	= hdlcd_vexpress_con_get_modes,
	.mode_valid	= hdlcd_vexpress_con_mode_valid,
	.best_encoder	= hdlcd_connector_best_encoder,
};


static void hdlcd_vexpress_encoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
	kfree(encoder);
	/*if (vconfig_func)
	  vexpress_config_func_put(vconfig_func);*/
}

static const struct drm_encoder_funcs hdlcd_vexpress_encoder_funcs = {
	.destroy = hdlcd_vexpress_encoder_destroy,
};

static void hdlcd_vexpress_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	/* VExpress micro has no support for DPMS */
}

static bool hdlcd_vexpress_encoder_mode_fixup(struct drm_encoder *encoder,
					const struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	/* nothing needs to be done here */
	return true;
}

static void hdlcd_vexpress_encoder_prepare(struct drm_encoder *encoder)
{
	hdlcd_vexpress_encoder_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void hdlcd_vexpress_encoder_commit(struct drm_encoder *encoder)
{
	hdlcd_vexpress_encoder_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void hdlcd_vexpress_encoder_mode_set(struct drm_encoder *encoder,
					struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	/*if (vconfig_func) {
		int i, vrefresh = drm_mode_vrefresh(mode);

		for (i = 0; i < ARRAY_SIZE(vexpress_dvimodes); i++) {
			if (vexpress_dvimodes[i].hsize != mode->hdisplay)
				continue;
			if (vexpress_dvimodes[i].vsize != mode->vdisplay)
				continue;
			if (vexpress_dvimodes[i].vrefresh != vrefresh)
				continue;

			vexpress_config_write(vconfig_func, 0,
					vexpress_dvimodes[i].dvimode);
			return;
		}
	}*/
}

static const struct drm_encoder_helper_funcs
hdlcd_vexpress_encoder_helper_funcs = {
	.dpms		= hdlcd_vexpress_encoder_dpms,
	.mode_fixup	= hdlcd_vexpress_encoder_mode_fixup,
	.prepare	= hdlcd_vexpress_encoder_prepare,
	.commit		= hdlcd_vexpress_encoder_commit,
	.mode_set	= hdlcd_vexpress_encoder_mode_set,
};

static const struct of_device_id vexpress_dvi_match[] = {
	{ .compatible = "arm,vexpress-dvimode" },
	{}
};

int hdlcd_create_vexpress_connector(struct drm_device *dev,
				struct hdlcd_drm_private *hdlcd)
{
	int err;
	struct drm_connector *connector;
	struct device_node *node;
	struct drm_encoder *encoder;

	node = of_find_matching_node(NULL, vexpress_dvi_match);
	if (!node)
		return -ENXIO;

	/*
	vconfig_func = vexpress_config_func_get_by_node(node);
	if (!vconfig_func) {
		DRM_ERROR("failed to get an output connector\n");
		return -ENXIO;
	}*/

	encoder = kzalloc(sizeof(*encoder), GFP_KERNEL);
	if (!encoder) {
		err = -ENOMEM;
		goto encoder_alloc_fail;
	}

	encoder->possible_crtcs = 1;
	encoder->possible_clones = 0;
	err = drm_encoder_init(dev, encoder, &hdlcd_vexpress_encoder_funcs,
			DRM_MODE_ENCODER_TMDS);
	if (err)
		goto encoder_init_fail;

	drm_encoder_helper_add(encoder, &hdlcd_vexpress_encoder_helper_funcs);

	connector = kzalloc(sizeof(*connector), GFP_KERNEL);
	if (!connector) {
		err = -ENOMEM;
		goto connector_alloc_err;
	}

	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->polled = 0;
	err = drm_connector_init(dev, connector, &hdlcd_connector_funcs,
						DRM_MODE_CONNECTOR_DVID);
	if (err)
		goto connector_init_err;

	drm_connector_helper_add(connector, &hdlcd_vexpress_con_helper_funcs);

	connector->encoder = encoder;
	err = drm_mode_connector_attach_encoder(connector, encoder);
	if (err)
		goto connector_attach_err;

	return 0;

connector_attach_err:
	drm_connector_cleanup(connector);
connector_init_err:
	kfree(connector);
connector_alloc_err:
	drm_encoder_cleanup(encoder);
encoder_init_fail:
	kfree(encoder);
encoder_alloc_fail:
	//vexpress_config_func_put(vconfig_func);

	return err;
}
