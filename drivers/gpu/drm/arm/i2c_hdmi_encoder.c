/*
 * Copyright (C) 2013,2014 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

/*
 * Theory of operation:
 *
 * The DRM framework expects the CRTC -> Encoder -> Connector chain,
 * where the CRTC is reading from framebuffer, passes data to the
 * encoder and that formats the signals to something usable by the
 * attached connector(s). Connectors can use i2c links to talk with
 * attached monitors.
 *
 * The HDMI transmitter is a different beast: it is both and encoder
 * and a connector in DRM parlance *and* can only be reached via i2c.
 * It implements an i2c pass through mode for the situation where one
 * wants to talk with the attached monitor. To complicate things
 * even further, the VExpress boards that have the SiI9022 chip share
 * the i2c line between the on-board microcontroller and the CoreTiles.
 * This leads to a situation where the microcontroller might be able to
 * talk with the SiI9022 transmitter, but not the CoreTile. And the
 * micro has a very small brain and a list of hardcoded modes that
 * it can program into the HDMI transmitter, so only a limited set
 * of resolutions will be valid.
 *
 * This file handles only the case where the i2c connection is available
 * to the kernel. For the case where we have to ask the microcontroller
 * to do the modesetting for us see the hdlcd_vexpress_encoder.c file.
 */

#include <linux/i2c.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder_slave.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include "hdlcd_drv.h"

static inline struct drm_encoder_slave *
hdlcd_get_slave_encoder(struct drm_connector * connector)
{
	return to_encoder_slave(hdlcd_connector_best_encoder(connector));
}

static void hdlcd_connector_destroy(struct drm_connector *connector)
{
}

static enum drm_connector_status
hdlcd_connector_detect(struct drm_connector *connector, bool force)
{
	struct drm_encoder_slave *slave;
	if (!connector->encoder)
		return connector_status_unknown;

	slave = hdlcd_get_slave_encoder(connector);
	if (!slave || !slave->slave_funcs)
		return connector_status_unknown;

	return slave->slave_funcs->detect(connector->encoder, connector);
}

static const struct drm_connector_funcs hdlcd_connector_funcs = {
	.destroy = hdlcd_connector_destroy,
	.dpms = drm_helper_connector_dpms,
	.detect = hdlcd_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
};

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

static int hdlcd_hdmi_con_get_modes(struct drm_connector *connector)
{
	struct drm_encoder_slave *slave = hdlcd_get_slave_encoder(connector);

	if (slave && slave->slave_funcs)
		return slave->slave_funcs->get_modes(&slave->base, connector);

	return 0;
}

static int hdlcd_hdmi_con_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	struct drm_encoder_slave *slave = hdlcd_get_slave_encoder(connector);

	if (slave && slave->slave_funcs)
		return slave->slave_funcs->mode_valid(connector->encoder, mode);

	return MODE_ERROR;
}

static const struct drm_connector_helper_funcs hdlcd_hdmi_con_helper_funcs = {
	.get_modes	= hdlcd_hdmi_con_get_modes,
	.mode_valid	= hdlcd_hdmi_con_mode_valid,
	.best_encoder	= hdlcd_connector_best_encoder,
};


static struct drm_encoder_funcs hdlcd_encoder_funcs = {
	.destroy	= drm_i2c_encoder_destroy,
};

static struct drm_encoder_helper_funcs hdlcd_encoder_helper_funcs = {
	.dpms		= drm_i2c_encoder_dpms,
	.save		= drm_i2c_encoder_save,
	.restore	= drm_i2c_encoder_restore,
	.mode_fixup	= drm_i2c_encoder_mode_fixup,
	.prepare	= drm_i2c_encoder_prepare,
	.commit		= drm_i2c_encoder_commit,
	.mode_set	= drm_i2c_encoder_mode_set,
	.detect		= drm_i2c_encoder_detect,
};

int hdlcd_create_digital_connector(struct drm_device *dev,
				struct hdlcd_drm_private *hdlcd)
{
	int err;
	struct i2c_board_info i2c_info;
	struct drm_encoder_slave *slave;
	struct drm_connector *connector;
	struct device_node *node = hdlcd->slave_node;

	slave = kzalloc(sizeof(*slave), GFP_KERNEL);
	if (!slave)
		return -ENOMEM;

	slave->base.possible_crtcs = 1;
	slave->base.possible_clones = 0;

	err = drm_encoder_init(dev, &slave->base, &hdlcd_encoder_funcs,
			DRM_MODE_ENCODER_TMDS);
	if (err)
		goto encoder_init_err;

	drm_encoder_helper_add(&slave->base, &hdlcd_encoder_helper_funcs);

	/* get the driver for the i2c slave node */
	i2c_info.of_node = node;
	err = of_modalias_node(node, i2c_info.type, sizeof(i2c_info.type));
	if (err < 0) {
		dev_err(dev->dev, "failed to get a module alias for node %s\n",
			node->full_name);
	}

	err = drm_i2c_encoder_init(dev, slave, NULL, &i2c_info);
	of_node_put(node);
	if (err)
		goto connector_alloc_err;

	connector = kzalloc(sizeof(*connector), GFP_KERNEL);
	if (!connector) {
		err = -ENOMEM;
		goto connector_alloc_err;
	}

	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->polled = DRM_CONNECTOR_POLL_CONNECT |
			DRM_CONNECTOR_POLL_DISCONNECT;
	err = drm_connector_init(dev, connector, &hdlcd_connector_funcs,
						DRM_MODE_CONNECTOR_DVID);
	if (err)
		goto connector_init_err;

	drm_connector_helper_add(connector, &hdlcd_hdmi_con_helper_funcs);

	connector->encoder = &slave->base;
	err = drm_mode_connector_attach_encoder(connector, &slave->base);
	if (err) {
		goto connector_attach_err;
	}

	return err;

connector_attach_err:
	drm_connector_cleanup(connector);
connector_init_err:
	kfree(connector);
connector_alloc_err:
	drm_encoder_cleanup(&slave->base);
encoder_init_err:
	kfree(slave);

	return err;
}
