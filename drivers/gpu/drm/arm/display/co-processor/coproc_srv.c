/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <uapi/drm/drm_mode.h>

#include "coproc.h"

static struct {
	struct list_head client_list;
	/* Protect for accessing client list */
	struct mutex mutex;
} coproc_srv;

struct coproc_client {
	struct list_head list;
	struct device *dev;
	struct coproc_client_callbacks *client_callbacks;
	void *private;
};

/** Called from display driver **/
int coproc_modeset_notify(struct coproc_client *co_client,
		const struct drm_mode_modeinfo *drminfo)
{
	if (co_client == NULL)
		return -ENODEV;

	if (co_client->client_callbacks &&
			co_client->client_callbacks->modeset_cb)
		co_client->client_callbacks->modeset_cb(co_client, drminfo);

	return 0;
}
EXPORT_SYMBOL_GPL(coproc_modeset_notify);

int coproc_dpms_notify(struct coproc_client *co_client, const u8 status)
{
	if (co_client == NULL)
		return -ENODEV;

	if (co_client->client_callbacks &&
			co_client->client_callbacks->dpms_cb)
		co_client->client_callbacks->dpms_cb(co_client, status);

	return 0;
}
EXPORT_SYMBOL_GPL(coproc_dpms_notify);

int coproc_frame_data(struct coproc_client *co_client, const void *data,
			size_t size)
{
	if (co_client == NULL)
		return -ENODEV;

	if (co_client->client_callbacks &&
		co_client->client_callbacks->frame_data_cb)
		co_client->client_callbacks->frame_data_cb(co_client, data, size);

	return 0;
}
EXPORT_SYMBOL_GPL(coproc_frame_data);

struct coproc_client *of_find_coproc_client_by_node(struct device_node *nd)
{
	struct coproc_client *co_client = NULL, *c;
	struct device *dev;
	struct i2c_client *i2c;
	struct platform_device *pdev;

	i2c = of_find_i2c_device_by_node(nd);
	pdev = of_find_device_by_node(nd);
	if (i2c)
		dev = &i2c->dev;
	else if (pdev)
		dev = &pdev->dev;
	else {
		pr_err("%s: could not find a co-processor device matching the dt node\n",
			__func__);
		return NULL;
	}

	mutex_lock(&coproc_srv.mutex);
	list_for_each_entry(c, &coproc_srv.client_list, list) {
		if (c->dev == dev) {
			co_client = c;
			break;
		}
	}
	mutex_unlock(&coproc_srv.mutex);

	return co_client;
}
EXPORT_SYMBOL_GPL(of_find_coproc_client_by_node);

/** Called from coprocessor client driver **/
struct coproc_client *coproc_register_client(struct device *dev,
		struct coproc_client_callbacks *client_cb)
{
	struct coproc_client *co_client;

	if (!dev)
		return ERR_PTR(EINVAL);

	mutex_lock(&coproc_srv.mutex);
	list_for_each_entry(co_client, &coproc_srv.client_list, list) {
		if (co_client->dev == dev) {
			mutex_unlock(&coproc_srv.mutex);
			pr_warning("client device has already been registered\n");
			return co_client;
		}
	}
	mutex_unlock(&coproc_srv.mutex);

	co_client = kzalloc(sizeof(*co_client), GFP_KERNEL);
	if (!co_client)
		return NULL;

	co_client->dev = dev;
	INIT_LIST_HEAD(&co_client->list);
	co_client->client_callbacks = client_cb;

	mutex_lock(&coproc_srv.mutex);
	list_add_tail(&co_client->list, &coproc_srv.client_list);
	mutex_unlock(&coproc_srv.mutex);
	return co_client;
}
EXPORT_SYMBOL_GPL(coproc_register_client);

void coproc_unregister_client(struct coproc_client *co_client)
{
	struct coproc_client *c;

	mutex_lock(&coproc_srv.mutex);
	list_for_each_entry(c, &coproc_srv.client_list, list) {
		if (c->dev == co_client->dev) {
			list_del(&c->list);
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

int coproc_prepare(struct coproc_client *co_client,
				const void *data_info, const size_t size)
{

	if (WARN_ON(co_client == NULL))
		return -ENODEV;

	if (co_client->client_callbacks &&
		co_client->client_callbacks->coproc_prepare)
		co_client->client_callbacks->coproc_prepare(co_client, data_info, size);
	return 0;
}
EXPORT_SYMBOL_GPL(coproc_prepare);

int coproc_query(struct coproc_client *co_client,
				void *data_info, const size_t size)
{
	if (WARN_ON(co_client == NULL))
		return -ENODEV;

	if (co_client->client_callbacks &&
		co_client->client_callbacks->coproc_query)
		co_client->client_callbacks->coproc_query(co_client, data_info, size);
	return 0;
}
EXPORT_SYMBOL_GPL(coproc_query);

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
