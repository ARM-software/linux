// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Julien Yin <Julien.Yin@arm.com>
 *
 */

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_of.h>
#include <linux/component.h>
#include <video/videomode.h>

static struct drm_display_mode real_connector_mode = {
	.clock 		= 25175,
	.hdisplay 	= 640,
	.hsync_start 	= 656,
	.hsync_end 	= 752,
	.htotal 	= 800,
	.hskew 		= 0,
	.vdisplay 	= 480,
	.vsync_start 	= 490,
	.vsync_end 	= 492,
	.vtotal 	= 525,
	.vscan 		= 0,
	.vrefresh 	= 60,
	.flags 		= DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
	.type 		= DRM_MODE_TYPE_PREFERRED,
	.status 	= MODE_OK,
};

struct drm_split_connector {
	struct drm_connector connector;
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct drm_connector *slave_connector[2];
	struct drm_encoder *slave_encoder[2];
	struct drm_device *drm;
	struct device *dev;
	void __iomem *regs;
};

#define connector_to_drm_split_connector(x) \
	container_of(x, struct drm_split_connector, connector)

#define encoder_to_drm_split_connector(x) \
	container_of(x, struct drm_split_connector, encoder)

#define bridge_to_drm_split_connector(x) \
	container_of(x, struct drm_split_connector, bridge)

static void split_connector_destroy(struct drm_connector *connector)
{
	struct drm_split_connector *conn = connector_to_drm_split_connector(connector);
	int i = 0;

	for (; i < 2; i++) {
		if (conn->slave_connector[i] != NULL &&
		    conn->slave_connector[i]->funcs->destroy != NULL) {
			conn->slave_connector[i]->funcs->destroy(conn->slave_connector[i]);
		}
	}
	drm_connector_cleanup(connector);
}

static enum drm_connector_status
split_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static const struct drm_connector_funcs split_connector_funcs = {
	.dpms		= drm_helper_connector_dpms,
	.reset		= drm_atomic_helper_connector_reset,
	.detect		= split_connector_detect,
	.fill_modes	= drm_helper_probe_single_connector_modes,
	.destroy	= split_connector_destroy,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_connector_destroy_state,
};

static void get_advertised_mode(struct drm_display_mode *advertised_mode)
{
	struct videomode vm;

	drm_display_mode_to_videomode(&real_connector_mode, &vm);

	vm.hactive *= 2;
	vm.hfront_porch *= 2;
	vm.hback_porch *= 2;
	vm.hsync_len *= 2;
	vm.pixelclock *= 2;

	drm_display_mode_from_videomode(&vm, advertised_mode);
}

static int split_connector_get_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode = drm_mode_create(connector->dev);

	get_advertised_mode(mode);

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	return 1;
}

static int split_connector_mode_valid(struct drm_connector *connector,
				   struct drm_display_mode *mode)
{
	if (mode) {
		struct drm_display_mode supported_mode;

		get_advertised_mode(&supported_mode);
		if ((supported_mode.hdisplay != mode->hdisplay) ||
		    (supported_mode.vdisplay != mode->vdisplay)) {
			return MODE_ERROR;
		}
	}

	return MODE_OK;
}

static struct drm_encoder *split_connector_best_encoder(struct drm_connector *connector)
{
	struct drm_split_connector *conn = connector_to_drm_split_connector(connector);

	return &conn->encoder;
}

static struct drm_encoder *
split_connector_atomic_best_encoder(struct drm_connector *connector,
				 struct drm_connector_state *connector_state)
{
	struct drm_split_connector *conn = connector_to_drm_split_connector(connector);

	return &conn->encoder;
}

static const struct drm_connector_helper_funcs split_connector_helper_funcs = {
	.get_modes = split_connector_get_modes,
	.mode_valid = split_connector_mode_valid,
	.best_encoder = split_connector_best_encoder,
	.atomic_best_encoder = split_connector_atomic_best_encoder,
};

static void split_encoder_destroy(struct drm_encoder *encoder)
{
	struct drm_split_connector *conn = encoder_to_drm_split_connector(encoder);
	int i = 0;

	for (; i < 2; i++) {
		if (conn->slave_encoder[i] != NULL &&
		    conn->slave_encoder[i]->funcs->destroy != NULL) {
			conn->slave_encoder[i]->funcs->destroy(conn->slave_encoder[i]);
		}
	}
	drm_encoder_cleanup(encoder);
}

static const struct drm_encoder_funcs split_encoder_funcs = {
	.destroy = split_encoder_destroy,
};

static enum drm_mode_status split_encoder_bridge_mode_valid(struct drm_bridge *bridge,
				     const struct drm_display_mode *mode)
{
	struct drm_split_connector *conn = bridge_to_drm_split_connector(bridge);
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(conn->slave_encoder); i++) {
		if (!conn->slave_encoder[i])
			continue;

		ret = drm_bridge_mode_valid(conn->slave_encoder[i]->bridge,
					    &real_connector_mode);

		if (ret  != MODE_OK) {
			DRM_ERROR("Encoder %d, doesn't support the requested"
				  "split mode  \n", i);
			return ret;
		}
	}

	return MODE_OK;
}

static void split_encoder_bridge_enable(struct drm_bridge *bridge)
{
	struct drm_split_connector *conn = bridge_to_drm_split_connector(bridge);
	int i;

	for (i = 0; i < ARRAY_SIZE(conn->slave_encoder); i++) {
		if (conn->slave_encoder[i])
			drm_bridge_enable(conn->slave_encoder[i]->bridge);
	}
}

static void split_encoder_bridge_disable(struct drm_bridge *bridge)
{
	struct drm_split_connector *conn = bridge_to_drm_split_connector(bridge);
	int i;

	for (i = 0; i < ARRAY_SIZE(conn->slave_encoder); i++) {
		if (conn->slave_encoder[i])
			drm_bridge_disable(conn->slave_encoder[i]->bridge);
	}
}

static void split_encoder_bridge_mode_set(struct drm_bridge *bridge,
				    const struct drm_display_mode *mode,
				    const struct drm_display_mode *adjusted_mode)
{
	struct drm_split_connector *conn = bridge_to_drm_split_connector(bridge);
	int i;

	for (i = 0; i < ARRAY_SIZE(conn->slave_encoder); i++) {
		if (conn->slave_encoder[i])
			drm_bridge_mode_set(conn->slave_encoder[i]->bridge,
					&real_connector_mode, &real_connector_mode);
	}
}

static const struct drm_bridge_funcs split_encoder_bridge_funcs = {
	.mode_valid = split_encoder_bridge_mode_valid,
	.disable = split_encoder_bridge_disable,
	.mode_set = split_encoder_bridge_mode_set,
	.enable = split_encoder_bridge_enable,
};

static int split_connector_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct drm_encoder *encoder;
	struct drm_split_connector *con;
	struct drm_connector *connector;
	struct drm_device *drm = data;
	struct drm_bridge *bridge;
	u32 crtcs = 0;
	int ret;

	con = dev_get_drvdata(dev);
	connector = &con->connector;
	encoder = &con->encoder;
	bridge = &con->bridge;
	INIT_LIST_HEAD(&bridge->list);
	bridge->funcs = &split_encoder_bridge_funcs;
#ifdef CONFIG_OF
	bridge->of_node = dev->of_node;
#endif
	drm_bridge_add(bridge);

	if (dev->of_node) {
		crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	}

	/* If no CRTCs were found, fall back to the old encoder's behaviour */
	if (crtcs == 0) {
		dev_warn(dev, "Falling back to first CRTC\n");
		crtcs = 1 << 0;
	}

	encoder->possible_crtcs = crtcs ? crtcs : 1;
	encoder->possible_clones = 0;

	ret = drm_encoder_init(drm, encoder, &split_encoder_funcs,
			       DRM_MODE_ENCODER_VIRTUAL, "virtual-encoder");
	if (ret)
		goto encoder_init_err;

	ret = drm_bridge_attach(encoder, bridge, NULL);
	if (ret) {
		DRM_ERROR("Failed to initialize bridge\n");
		goto connector_init_err;
	}

	/* bogus values, pretend we're a 24" screen for DPI calculations */
	connector->display_info.width_mm = 519;
	connector->display_info.height_mm = 324;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->polled = 0;

	ret = drm_connector_init(drm, connector, &split_connector_funcs,
				 DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret)
		goto connector_init_err;

	drm_connector_helper_add(connector, &split_connector_helper_funcs);

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

	return ret;
};

static void split_connector_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct drm_split_connector *conn = dev_get_drvdata(dev);

	drm_connector_unregister(&conn->connector);
	drm_connector_cleanup(&conn->connector);
	drm_encoder_cleanup(&conn->encoder);
}

static const struct component_ops split_connector_ops = {
	.bind = split_connector_bind,
	.unbind = split_connector_unbind,
};

static struct drm_mode_config_funcs linked_drm_config_funcs = {
	.fb_create = NULL,
	.atomic_check = NULL,
	.atomic_commit = NULL,
};

static struct drm_driver linked_drm_driver = {
	.driver_features = 0,
};

static int linked_conn_bind(struct device *dev)
{
	struct drm_device *drm;
	struct drm_mode_config *config;
	struct drm_split_connector *conn;
	int ret;
	int i = 0;

	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct drm_encoder *encoder;

	conn = dev_get_drvdata(dev);

	drm = drm_dev_alloc(&linked_drm_driver, dev);
	if (!drm)
	{
		drm_dev_put(drm);
		return -ENOMEM;
	}

	drm->dev_private = drm;
	conn->drm = drm;

	drm_mode_config_init(drm);
	config = &drm->mode_config;
	config->funcs = &linked_drm_config_funcs;

	ret = drm_dev_register(drm, 0);
	if (ret)
		return -EINVAL;

	component_bind_all(dev, drm);

	drm_connector_list_iter_begin(drm, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		conn->slave_connector[i] = connector;
		i++;
		if (i >= 2)
			break;
	}

	if (i != ARRAY_SIZE(conn->slave_connector)) {
		dev_err(dev, "Expected to find just two connectors");
		return -EINVAL;
	}

	i = 0;
	drm_for_each_encoder(encoder, drm) {
		conn->slave_encoder[i] = encoder;
		i++;
		if (i >= 2)
			break;
	}

	if (i != ARRAY_SIZE(conn->slave_connector)) {
		dev_err(dev, "Expected to find just two encoders");
		return -EINVAL;
	}

	return 0;
}

static void linked_conn_unbind(struct device *dev)
{
	struct drm_split_connector *conn = dev_get_drvdata(dev);
	struct drm_device *drm = conn->drm;

	component_unbind_all(dev, drm);
	drm_mode_config_cleanup(drm);
	drm_dev_unregister(drm);
	drm_dev_put(drm);
}

static const struct component_master_ops drm_linked_conn_ops = {
	.bind	= linked_conn_bind,
	.unbind	= linked_conn_unbind,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int split_conn_parse_dt(struct drm_split_connector *conn)
{
	struct component_match *match = NULL;
	struct device_node *np;
	struct device_node *of_inputs[2] = { NULL, NULL };
	struct device_node *of_outputs[2] = { NULL, NULL };
	int i, ret = 0;

	np = conn->dev->of_node;

	for (i = 0; i < 2; i++) {
		of_inputs[i] = of_graph_get_remote_node(np, 0, i);
		if (!of_inputs[i]) {
			dev_err(conn->dev, "Couldn't find remote-endpoint for "
					   "port 0 endpoint %d\n", i);
			ret = -EINVAL;
			goto cleanup;
		}
		of_outputs[i] = of_graph_get_remote_node(np, 1, i);
		if (!of_outputs[i]) {
			dev_err(conn->dev, "Couldn't find remote-endpoint for "
					   "port 1 endpoint %d\n", i);
			ret = -EINVAL;
			goto cleanup;

		}
		dev_info(conn->dev, "Link%d input: %s output%s", i, of_inputs[i]->full_name,
			 of_outputs[i]->full_name);
	}
	if (of_inputs[0] != of_inputs[1]) {
		dev_err(conn->dev, "Invalid inputs, inputs need to be endpoints "
				   "of the same remote-port");
		ret = -EINVAL;
		goto cleanup;
	}

	component_match_add(conn->dev, &match, compare_of, of_outputs[0]);
	component_match_add(conn->dev, &match, compare_of, of_outputs[1]);

	if (IS_ERR(match)) {
		dev_err(conn->dev, "Couldn't add outputs to component match\n");
		ret = PTR_ERR(match);
		goto cleanup;;
	}

	ret = component_master_add_with_match(conn->dev, &drm_linked_conn_ops, match);

cleanup:
	for (i = 0; i < 2; i++) {
		of_node_put(of_inputs[i]);
		of_node_put(of_outputs[i]);
	}

	return ret;
}


static int drm_split_conn_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct drm_split_connector *conn;
	struct resource *res;
	int ret = 0;

	dev = &pdev->dev;
	conn = devm_kzalloc(dev, sizeof(*conn), GFP_KERNEL);
	if (!conn)
		return -ENOMEM;

	dev_set_drvdata(dev, conn);
	conn->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Could not get registers resource\n");
		return -ENOMEM;
	}
	conn->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(conn->regs))
		return -ENOMEM;

	ret = split_conn_parse_dt(conn);
	if (ret) {
		dev_err(&pdev->dev, "Failed to parse device tree");
		return ret;
	}

	component_add(dev, &split_connector_ops);

	//always enable display split in split connector
	writel(1, conn->regs);
	return 0;
}

static int drm_split_conn_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &drm_linked_conn_ops);
	component_del(&pdev->dev, &split_connector_ops);
	return 0;
}

static const struct of_device_id drm_split_conn_of_match[] = {
	{ .compatible = "drm,split-connector", },
	{},
};
MODULE_DEVICE_TABLE(of, drm_split_conn_of_match);

static struct platform_driver drm_split_conn_driver = {
	.probe = drm_split_conn_probe,
	.remove = drm_split_conn_remove,
	.driver = {
		.name = "drm_split_conn",
		.of_match_table = drm_split_conn_of_match,
	},
};

module_platform_driver(drm_split_conn_driver);

MODULE_AUTHOR("Arm limited");
MODULE_DESCRIPTION("Split DRM connector");
MODULE_LICENSE("GPL v2");
