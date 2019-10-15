// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/component.h>
#include <drm/drm_device.h>
#include "ad_coprocessor_defs.h"
#include "coproc.h"

static struct {
	struct list_head client_list;
	/* Protect for accessing client list */
	struct mutex mutex;
} coproc_srv;

struct coproc_client {
	struct ad_coprocessor base;
	struct ad_coprocessor_funcs funcs;
	struct list_head list;
	struct coproc_client_callbacks *client_callbacks;
	void *private;
};

#define ad_to_co_client(p) container_of(p, struct coproc_client, base)

static int coproc_mode_set(struct ad_coprocessor *ad,
			   struct drm_display_mode *mode)
{
	struct coproc_client *co_client = ad_to_co_client(ad);
	struct drm_mode_modeinfo m;

	m.clock = mode->crtc_clock;
	m.hdisplay = mode->crtc_hdisplay;
	m.vdisplay = mode->crtc_vdisplay;
	m.hsync_start = mode->crtc_hsync_start;
	m.hsync_end = mode->crtc_hsync_end;
	m.htotal = mode->crtc_htotal;
	m.hskew = mode->crtc_hskew;
	m.vsync_start = mode->crtc_vsync_start;
	m.vsync_end = mode->crtc_vsync_end;
	m.vtotal = mode->crtc_vtotal;
	m.vscan = mode->vscan;
	m.vrefresh = mode->vrefresh;
	m.flags = mode->flags;
	m.type = mode->type;
	sprintf(m.name, "%s", mode->name);

	co_client->client_callbacks->modeset_cb(co_client, &m);

	return 0;
}

static int coproc_enable(struct ad_coprocessor *ad)
{
	struct coproc_client *co_client = ad_to_co_client(ad);

	co_client->client_callbacks->dpms_cb(co_client, DRM_MODE_DPMS_ON);

	return 0;
}

static int coproc_disable(struct ad_coprocessor *ad)
{
	struct coproc_client *co_client = ad_to_co_client(ad);

	co_client->client_callbacks->dpms_cb(co_client, DRM_MODE_DPMS_OFF);

	return 0;
}

static int coproc_frame_data(struct ad_coprocessor *ad,
			     const void *data, size_t size)
{
	struct coproc_client *co_client = ad_to_co_client(ad);

	co_client->client_callbacks->frame_data_cb(co_client, data, size);

	return 0;
}

static int coproc_prepare(struct ad_coprocessor *ad,
			  const void *data_info, const size_t size)
{
	struct coproc_client *co_client = ad_to_co_client(ad);

	co_client->client_callbacks->coproc_prepare(co_client, data_info, size);

	return 0;
}

static int coproc_query(struct ad_coprocessor *ad,
			void *data_info, const size_t size)
{
	struct coproc_client *co_client = ad_to_co_client(ad);

	co_client->client_callbacks->coproc_query(co_client, data_info, size);

	return 0;
}

static int coproc_client_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct ad_list *ad_head = drm->dev_private;
	struct coproc_client *co_client = NULL, *c;

	mutex_lock(&coproc_srv.mutex);
	list_for_each_entry(c, &coproc_srv.client_list, list) {
		if (c->base.dev == dev) {
			co_client = c;
			break;
		}
	}
	mutex_unlock(&coproc_srv.mutex);

	if (co_client)
		list_add_tail(&co_client->base.ad_node, &ad_head->head);
	else
		WARN(1, "Could not find the coproc-client in list.\n");

	return 0;
}

static void coproc_client_unbind(struct device *dev,
				 struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct ad_list *ad_head = drm->dev_private;
	struct ad_coprocessor *ad;

	list_for_each_entry(ad, &ad_head->head, ad_node) {
		if (ad->dev == dev) {
			list_del_init(&ad->ad_node);
			break;
		}
	}
}

static const struct component_ops coproc_client_component_ops = {
	.bind   = coproc_client_bind,
	.unbind = coproc_client_unbind,
};

/** Called from coprocessor client driver **/
struct coproc_client *
coproc_register_client(struct device *dev,
		       struct coproc_client_callbacks *client_cb)
{
	struct coproc_client *co_client;
	struct ad_coprocessor_funcs *ad_funcs;

	if (!dev || !client_cb)
		return ERR_PTR(EINVAL);

	mutex_lock(&coproc_srv.mutex);
	list_for_each_entry(co_client, &coproc_srv.client_list, list) {
		if (co_client->base.dev == dev) {
			mutex_unlock(&coproc_srv.mutex);
			pr_warning("client device has already been registered\n");
			return co_client;
		}
	}
	mutex_unlock(&coproc_srv.mutex);

	co_client = kzalloc(sizeof(*co_client), GFP_KERNEL);
	if (!co_client)
		return NULL;

	INIT_LIST_HEAD(&co_client->list);
	co_client->client_callbacks = client_cb;

	mutex_lock(&coproc_srv.mutex);
	list_add_tail(&co_client->list, &coproc_srv.client_list);
	mutex_unlock(&coproc_srv.mutex);

	/* set ad_coprocessor funcs */
	ad_funcs = &co_client->funcs;
	if (client_cb->modeset_cb)
		ad_funcs->mode_set = coproc_mode_set;
	if (client_cb->dpms_cb) {
		ad_funcs->enable = coproc_enable;
		ad_funcs->disable = coproc_disable;
	}
	if (client_cb->frame_data_cb)
		ad_funcs->frame_data = coproc_frame_data;
	if (client_cb->coproc_query)
		ad_funcs->coproc_query = coproc_query;
	if (client_cb->coproc_prepare)
		ad_funcs->coproc_prepare = coproc_prepare;

	co_client->base.dev = dev;
	co_client->base.funcs = ad_funcs;

	INIT_LIST_HEAD(&co_client->base.ad_node);

	component_add(dev, &coproc_client_component_ops);

	return co_client;
}
EXPORT_SYMBOL_GPL(coproc_register_client);

void coproc_unregister_client(struct coproc_client *co_client)
{
	struct coproc_client *c;

	mutex_lock(&coproc_srv.mutex);
	list_for_each_entry(c, &coproc_srv.client_list, list) {
		if (c->base.dev == co_client->base.dev) {
			list_del_init(&c->list);
			component_del(c->base.dev, &coproc_client_component_ops);
			if (!list_empty(&co_client->base.ad_node))
				pr_warning("co_client is still referenced by others, can not unregister the client.\n");
			else
				kfree(co_client);

			break;
		}
	}
	mutex_unlock(&coproc_srv.mutex);
}
EXPORT_SYMBOL_GPL(coproc_unregister_client);

void coproc_set_drvdata(struct coproc_client *co_client, void *p)
{
	co_client->private = p;
}
EXPORT_SYMBOL_GPL(coproc_set_drvdata);

void *coproc_get_drvdata(struct coproc_client *co_client)
{
	if (WARN_ON(co_client == NULL))
		return NULL;

	return co_client->private;
}
EXPORT_SYMBOL_GPL(coproc_get_drvdata);

static int __init coproc_srv_init(void)
{
	INIT_LIST_HEAD(&coproc_srv.client_list);
	mutex_init(&coproc_srv.mutex);

	return 0;
}
module_init(coproc_srv_init);

static void __exit coproc_srv_exit(void)
{
	if (!list_empty(&coproc_srv.client_list))
		pr_warning("Client still active while server will exit!\n");

}
module_exit(coproc_srv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Arm Mali display co-processor server");
