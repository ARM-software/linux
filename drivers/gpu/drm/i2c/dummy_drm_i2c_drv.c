/*
 * This file was originally based on tda998x_drv.c which has the following
 * copyright and licence...
 *
 * Copyright (C) 2012 Texas Instruments
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/component.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder_slave.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>

struct dummy_priv {
	struct drm_encoder *encoder;
};

struct dummy_encoder_params {
};

#define to_dummy_priv(x)  ((struct dummy_priv *)to_encoder_slave(x)->slave_priv)

/* DRM encoder functions */

static void dummy_encoder_set_config(struct dummy_priv *priv,
				       const struct dummy_encoder_params *p)
{
}

static void dummy_encoder_dpms(struct dummy_priv *priv, int mode)
{
}

static void
dummy_encoder_save(struct drm_encoder *encoder)
{
}

static void
dummy_encoder_restore(struct drm_encoder *encoder)
{
}

static bool
dummy_encoder_mode_fixup(struct drm_encoder *encoder,
			  const struct drm_display_mode *mode,
			  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int dummy_encoder_mode_valid(struct dummy_priv *priv,
				      struct drm_display_mode *mode)
{
	return MODE_OK;
}

static void
dummy_encoder_mode_set(struct dummy_priv *priv,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode)
{
}

static enum drm_connector_status
dummy_encoder_detect(struct dummy_priv *priv)
{
	return connector_status_connected;
}


static const u8 edid_1024x768[] = {
	/*
	 * These values are a copy of Documentation/EDID/1024x768.c
	 * produced by executing "make -C Documentation/EDID"
	 */
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x31, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x16, 0x01, 0x03, 0x6d, 0x23, 0x1a, 0x78,
	0xea, 0x5e, 0xc0, 0xa4, 0x59, 0x4a, 0x98, 0x25,
	0x20, 0x50, 0x54, 0x00, 0x08, 0x00, 0x61, 0x40,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x64, 0x19,
	0x00, 0x40, 0x41, 0x00, 0x26, 0x30, 0x08, 0x90,
	0x36, 0x00, 0x63, 0x0a, 0x11, 0x00, 0x00, 0x18,
	0x00, 0x00, 0x00, 0xff, 0x00, 0x4c, 0x69, 0x6e,
	0x75, 0x78, 0x20, 0x23, 0x30, 0x0a, 0x20, 0x20,
	0x20, 0x20, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x3b,
	0x3d, 0x2f, 0x31, 0x07, 0x00, 0x0a, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
	0x00, 0x4c, 0x69, 0x6e, 0x75, 0x78, 0x20, 0x58,
	0x47, 0x41, 0x0a, 0x20, 0x20, 0x20, 0x00, 0x55
};

static int
dummy_encoder_get_modes(struct dummy_priv *priv,
			  struct drm_connector *connector)
{
	struct edid *edid = (struct edid *)edid_1024x768;
	int n;

	drm_mode_connector_update_edid_property(connector, edid);
	n = drm_add_edid_modes(connector, edid);

	return n;
}

static int
dummy_encoder_set_property(struct drm_encoder *encoder,
			    struct drm_connector *connector,
			    struct drm_property *property,
			    uint64_t val)
{
	return 0;
}

static void dummy_destroy(struct dummy_priv *priv)
{
}

/* Slave encoder support */

static void
dummy_encoder_slave_set_config(struct drm_encoder *encoder, void *params)
{
	dummy_encoder_set_config(to_dummy_priv(encoder), params);
}

static void dummy_encoder_slave_destroy(struct drm_encoder *encoder)
{
	struct dummy_priv *priv = to_dummy_priv(encoder);

	dummy_destroy(priv);
	drm_i2c_encoder_destroy(encoder);
	kfree(priv);
}

static void dummy_encoder_slave_dpms(struct drm_encoder *encoder, int mode)
{
	dummy_encoder_dpms(to_dummy_priv(encoder), mode);
}

static int dummy_encoder_slave_mode_valid(struct drm_encoder *encoder,
					    struct drm_display_mode *mode)
{
	return dummy_encoder_mode_valid(to_dummy_priv(encoder), mode);
}

static void
dummy_encoder_slave_mode_set(struct drm_encoder *encoder,
			       struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	dummy_encoder_mode_set(to_dummy_priv(encoder), mode, adjusted_mode);
}

static enum drm_connector_status
dummy_encoder_slave_detect(struct drm_encoder *encoder,
			     struct drm_connector *connector)
{
	return dummy_encoder_detect(to_dummy_priv(encoder));
}

static int dummy_encoder_slave_get_modes(struct drm_encoder *encoder,
					   struct drm_connector *connector)
{
	return dummy_encoder_get_modes(to_dummy_priv(encoder), connector);
}

static int
dummy_encoder_slave_create_resources(struct drm_encoder *encoder,
				       struct drm_connector *connector)
{
	return 0;
}

static struct drm_encoder_slave_funcs dummy_encoder_slave_funcs = {
	.set_config = dummy_encoder_slave_set_config,
	.destroy = dummy_encoder_slave_destroy,
	.dpms = dummy_encoder_slave_dpms,
	.save = dummy_encoder_save,
	.restore = dummy_encoder_restore,
	.mode_fixup = dummy_encoder_mode_fixup,
	.mode_valid = dummy_encoder_slave_mode_valid,
	.mode_set = dummy_encoder_slave_mode_set,
	.detect = dummy_encoder_slave_detect,
	.get_modes = dummy_encoder_slave_get_modes,
	.create_resources = dummy_encoder_slave_create_resources,
	.set_property = dummy_encoder_set_property,
};

/* I2C driver functions */

static int dummy_encoder_init(struct i2c_client *client,
				struct drm_device *dev,
				struct drm_encoder_slave *encoder_slave)
{
	struct dummy_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->encoder = &encoder_slave->base;

	encoder_slave->slave_priv = priv;
	encoder_slave->slave_funcs = &dummy_encoder_slave_funcs;

	return 0;
}

struct dummy_priv2 {
	struct dummy_priv base;
	struct drm_encoder encoder;
	struct drm_connector connector;
};

#define conn_to_dummy_priv2(x) \
	container_of(x, struct dummy_priv2, connector);

#define enc_to_dummy_priv2(x) \
	container_of(x, struct dummy_priv2, encoder);

static void dummy_encoder2_dpms(struct drm_encoder *encoder, int mode)
{
	struct dummy_priv2 *priv = enc_to_dummy_priv2(encoder);

	dummy_encoder_dpms(&priv->base, mode);
}

static void dummy_encoder_prepare(struct drm_encoder *encoder)
{
	dummy_encoder2_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void dummy_encoder_commit(struct drm_encoder *encoder)
{
	dummy_encoder2_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void dummy_encoder2_mode_set(struct drm_encoder *encoder,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adjusted_mode)
{
	struct dummy_priv2 *priv = enc_to_dummy_priv2(encoder);

	dummy_encoder_mode_set(&priv->base, mode, adjusted_mode);
}

static const struct drm_encoder_helper_funcs dummy_encoder_helper_funcs = {
	.dpms = dummy_encoder2_dpms,
	.save = dummy_encoder_save,
	.restore = dummy_encoder_restore,
	.mode_fixup = dummy_encoder_mode_fixup,
	.prepare = dummy_encoder_prepare,
	.commit = dummy_encoder_commit,
	.mode_set = dummy_encoder2_mode_set,
};

static void dummy_encoder_destroy(struct drm_encoder *encoder)
{
	struct dummy_priv2 *priv = enc_to_dummy_priv2(encoder);

	dummy_destroy(&priv->base);
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs dummy_encoder_funcs = {
	.destroy = dummy_encoder_destroy,
};

static int dummy_connector_get_modes(struct drm_connector *connector)
{
	struct dummy_priv2 *priv = conn_to_dummy_priv2(connector);

	return dummy_encoder_get_modes(&priv->base, connector);
}

static int dummy_connector_mode_valid(struct drm_connector *connector,
					struct drm_display_mode *mode)
{
	struct dummy_priv2 *priv = conn_to_dummy_priv2(connector);

	return dummy_encoder_mode_valid(&priv->base, mode);
}

static struct drm_encoder *
dummy_connector_best_encoder(struct drm_connector *connector)
{
	struct dummy_priv2 *priv = conn_to_dummy_priv2(connector);

	return &priv->encoder;
}

static
const struct drm_connector_helper_funcs dummy_connector_helper_funcs = {
	.get_modes = dummy_connector_get_modes,
	.mode_valid = dummy_connector_mode_valid,
	.best_encoder = dummy_connector_best_encoder,
};

static enum drm_connector_status
dummy_connector_detect(struct drm_connector *connector, bool force)
{
	struct dummy_priv2 *priv = conn_to_dummy_priv2(connector);

	return dummy_encoder_detect(&priv->base);
}

static void dummy_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs dummy_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.detect = dummy_connector_detect,
	.destroy = dummy_connector_destroy,
};

static int dummy_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct dummy_priv2 *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);

	priv->base.encoder = &priv->encoder;
	priv->encoder.possible_crtcs = 1 << 0;

	drm_encoder_helper_add(&priv->encoder, &dummy_encoder_helper_funcs);
	ret = drm_encoder_init(drm, &priv->encoder, &dummy_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS);
	if (ret)
		goto err_encoder;

	drm_connector_helper_add(&priv->connector,
				 &dummy_connector_helper_funcs);
	ret = drm_connector_init(drm, &priv->connector,
				 &dummy_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		goto err_connector;

	ret = drm_connector_register(&priv->connector);
	if (ret)
		goto err_sysfs;

	priv->connector.encoder = &priv->encoder;
	drm_mode_connector_attach_encoder(&priv->connector, &priv->encoder);

	return 0;

err_sysfs:
	drm_connector_cleanup(&priv->connector);
err_connector:
	drm_encoder_cleanup(&priv->encoder);
err_encoder:
	dummy_destroy(&priv->base);
	return ret;
}

static void dummy_unbind(struct device *dev, struct device *master,
			   void *data)
{
	struct dummy_priv2 *priv = dev_get_drvdata(dev);

	drm_connector_cleanup(&priv->connector);
	drm_encoder_cleanup(&priv->encoder);
	dummy_destroy(&priv->base);
}

static const struct component_ops dummy_ops = {
	.bind = dummy_bind,
	.unbind = dummy_unbind,
};

static int
dummy_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	return component_add(&client->dev, &dummy_ops);
}

static int dummy_remove(struct i2c_client *client)
{
	component_del(&client->dev, &dummy_ops);
	return 0;
}

static const struct of_device_id dummy_of_ids[] = {
	{ .compatible = "sil,sii9022-tpi", },
	{ }
};
MODULE_DEVICE_TABLE(of, dummy_of_ids);

static struct i2c_device_id dummy_ids[] = {
	{ }
};
MODULE_DEVICE_TABLE(i2c, dummy_ids);

static struct drm_i2c_encoder_driver dummy_driver = {
	.i2c_driver = {
		.probe = dummy_probe,
		.remove = dummy_remove,
		.driver = {
			.name = "dummy_drm_i2c",
			.of_match_table = dummy_of_ids
		},
		.id_table = dummy_ids,
	},
	.encoder_init = dummy_encoder_init,
};

/* Module initialization */

static int __init
dummy_init(void)
{
	return drm_i2c_encoder_register(THIS_MODULE, &dummy_driver);
}

static void __exit
dummy_exit(void)
{
	drm_i2c_encoder_unregister(&dummy_driver);
}

MODULE_LICENSE("GPL");

module_init(dummy_init);
module_exit(dummy_exit);
