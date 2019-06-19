/*
 * Copyright (C) 2016 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * Dummy encoder and connector that use the OF to "discover" the attached
 * display timings. Can be used in situations where the encoder and connector's
 * functionality are emulated and no setup steps are needed, or to describe
 * attached panels for which no driver exists but can be used without
 * additional hardware setup.
 *
 * The encoder also uses the component framework so that it can be a quick
 * replacement for existing drivers when testing in an emulated environment.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <linux/component.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

struct drm_virt_priv {
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct display_timings *timings;
};

#define connector_to_drm_virt_priv(x) \
	container_of(x, struct drm_virt_priv, connector)

#define encoder_to_drm_virt_priv(x) \
	container_of(x, struct drm_virt_priv, encoder)

static void drm_virtcon_destroy(struct drm_connector *connector)
{
	struct drm_virt_priv *conn = connector_to_drm_virt_priv(connector);

	drm_connector_cleanup(connector);
	display_timings_release(conn->timings);
}

static enum drm_connector_status
drm_virtcon_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static const struct drm_connector_funcs drm_virtcon_funcs = {
	.dpms = drm_helper_connector_dpms,
	.reset = drm_atomic_helper_connector_reset,
	.detect	= drm_virtcon_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_virtcon_destroy,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int drm_virtcon_get_modes(struct drm_connector *connector)
{
	struct drm_virt_priv *conn = connector_to_drm_virt_priv(connector);
	struct display_timings *timings = conn->timings;
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

static int drm_virtcon_mode_valid(struct drm_connector *connector,
				   struct drm_display_mode *mode)
{
	return MODE_OK;
}

struct drm_encoder *drm_virtcon_best_encoder(struct drm_connector *connector)
{
	struct drm_virt_priv *priv = connector_to_drm_virt_priv(connector);

	return &priv->encoder;
}

struct drm_encoder *
drm_virtcon_atomic_best_encoder(struct drm_connector *connector,
				 struct drm_connector_state *connector_state)
{
	struct drm_virt_priv *priv = connector_to_drm_virt_priv(connector);

	return &priv->encoder;
}

static const struct drm_connector_helper_funcs drm_virtcon_helper_funcs = {
	.get_modes = drm_virtcon_get_modes,
	.mode_valid = drm_virtcon_mode_valid,
	.best_encoder = drm_virtcon_best_encoder,
	.atomic_best_encoder = drm_virtcon_atomic_best_encoder,
};

static void drm_vencoder_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs drm_vencoder_funcs = {
	.destroy = drm_vencoder_destroy,
};

static void drm_vencoder_dpms(struct drm_encoder *encoder, int mode)
{
	/* nothing needed */
}

static bool drm_vencoder_mode_fixup(struct drm_encoder *encoder,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	/* nothing needed */
	return true;
}

static void drm_vencoder_prepare(struct drm_encoder *encoder)
{
	drm_vencoder_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void drm_vencoder_commit(struct drm_encoder *encoder)
{
	drm_vencoder_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void drm_vencoder_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	/* nothing needed */
}

static const struct drm_encoder_helper_funcs drm_vencoder_helper_funcs = {
	.dpms		= drm_vencoder_dpms,
	.mode_fixup	= drm_vencoder_mode_fixup,
	.prepare	= drm_vencoder_prepare,
	.commit		= drm_vencoder_commit,
	.mode_set	= drm_vencoder_mode_set,
};

static int drm_vencoder_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct drm_encoder *encoder;
	struct drm_virt_priv *con;
	struct drm_connector *connector;
	struct drm_device *drm = data;
	struct drm_bridge *bridge = NULL;
	struct device_node *np = NULL;
	u32 crtcs = 0;
	int ret;

	con = devm_kzalloc(dev, sizeof(*con), GFP_KERNEL);
	if (!con)
		return -ENOMEM;

	dev_set_drvdata(dev, con);
	connector = &con->connector;
	encoder = &con->encoder;

	np = dev->of_node;

	if (np) {
		crtcs = drm_of_find_possible_crtcs(drm, np);
		bridge = of_drm_find_bridge(np);
		con->timings = of_get_display_timings(np);
		if (!con->timings) {
			dev_err(dev, "failed to get display panel timings\n");
			return ENXIO;
		}
	}

	/* If no CRTCs were found, fall back to the old encoder's behaviour */
	if (crtcs == 0) {
		dev_warn(dev, "Falling back to first CRTC\n");
		crtcs = 1 << 0;
	}

	encoder->possible_crtcs = crtcs ? crtcs : 1;
	encoder->possible_clones = 0;

	ret = drm_encoder_init(drm, encoder, &drm_vencoder_funcs,
			       DRM_MODE_ENCODER_VIRTUAL, "virtual-encoder");
	if (ret)
		goto encoder_init_err;

	if (bridge) {
		ret = drm_bridge_attach(encoder, bridge, NULL);
		if (ret) {
			DRM_ERROR("Failed to initialize bridge\n");
			goto connector_init_err;
		}
		encoder->bridge = bridge;
	}

	drm_encoder_helper_add(encoder, &drm_vencoder_helper_funcs);

	/* bogus values, pretend we're a 24" screen for DPI calculations */
	connector->display_info.width_mm = 519;
	connector->display_info.height_mm = 324;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->polled = 0;

	if (of_property_read_bool(np, "command_mode"))
		connector->display_info.command_mode = true;

	ret = drm_connector_init(drm, connector, &drm_virtcon_funcs,
				 DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret)
		goto connector_init_err;

	drm_connector_helper_add(connector, &drm_virtcon_helper_funcs);

	drm_connector_register(connector);

	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret)
		goto attach_err;

	return ret;

attach_err:
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
connector_init_err:
	drm_encoder_cleanup(encoder);
encoder_init_err:
	display_timings_release(con->timings);

	return ret;
};

static void drm_vencoder_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct drm_virt_priv *con = dev_get_drvdata(dev);

	drm_connector_unregister(&con->connector);
	drm_connector_cleanup(&con->connector);
	drm_encoder_cleanup(&con->encoder);
	display_timings_release(con->timings);
}

static const struct component_ops drm_vencoder_ops = {
	.bind = drm_vencoder_bind,
	.unbind = drm_vencoder_unbind,
};

static int drm_vencoder_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &drm_vencoder_ops);
}

static int drm_vencoder_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &drm_vencoder_ops);
	return 0;
}

static const struct of_device_id drm_vencoder_of_match[] = {
	{ .compatible = "drm,virtual-encoder", },
	{},
};
MODULE_DEVICE_TABLE(of, drm_vencoder_of_match);

static struct platform_driver drm_vencoder_driver = {
	.probe = drm_vencoder_probe,
	.remove = drm_vencoder_remove,
	.driver = {
		.name = "drm_vencoder",
		.of_match_table = drm_vencoder_of_match,
	},
};

module_platform_driver(drm_vencoder_driver);

MODULE_AUTHOR("Liviu Dudau");
MODULE_DESCRIPTION("Virtual DRM Encoder");
MODULE_LICENSE("GPL v2");
