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

#ifndef _COPROC_H_
#define _COPROC_H_

struct drm_mode_modeinfo;
struct coproc_client;

struct coproc_client_callbacks {
	/* when modeset is done, this callback will be called */
	int (*modeset_cb)(struct coproc_client *client,
				const struct drm_mode_modeinfo *drminfo);
	/* When DPMS is changed, this callback will be called */
	int (*dpms_cb)(struct coproc_client *client, u8 state);
	/* this callback will be called when a new frame with
	 * the data is arriving.
	 */
	int (*frame_data_cb)(struct coproc_client *client,
				const void *data, size_t size);
	/* this callback will be called when need to query the coproc data info.*/
	int (*coproc_query)(struct coproc_client *client,
				void *data, const size_t size);
	/* this callback will be called when need to set the coproc data info.*/
	int (*coproc_prepare)(struct coproc_client *client,
				const void *data, const size_t size);
};

/* --Host API-- */
#ifdef CONFIG_COPROC_SERVER
/*
 * notify function when modeset is done
 * @co_client: a pointer to client.
 * @drminfo: a pointer to drm_mode_modeinfo
 *
 * @return: 0 on success, negative value for fail
 *
 * After modeset operation is done, host will call this function to notice
 * coprocessor server.
 */
int coproc_modeset_notify(struct coproc_client *co_client,
			  const struct drm_mode_modeinfo *drminfo);
/*
 * notify function when DPMS is changed
 * @co_client: a pointer to client
 * @state: DPMS state should be DRM_MODE_DPMS_ON, DRM_MODE_DPMS_STANDBY,
 *         DRM_MODE_DPMS_SUSPEND or DRM_MODE_DPMS_OFF.
 *
 * @return: 0 on success, negative value for fail.
 *
 * When diplay DPMS is changed, the host will this function to notice
 * coprocessor server.
 */
int coproc_dpms_notify(struct coproc_client *co_client, const u8 state);
/*
 * find the client via device node
 * @nd: device tree node of the co-processor
 *
 * @return: NULL for fail, or a pointer to co-processor client
 *
 * Host calls this function to get a reference to co-processor client.
 */
struct coproc_client *of_find_coproc_client_by_node(struct device_node *nd);

/*
 * a function for transfering frame related data
 * @co_client: a pointer to client
 * @data: a point to frame related data
 * @size: the count of the data (bytes)
 *
 * @return: 0 on success, negative value for fail.
 *
 * DP driver calls this function to send frame related data to client driver.
 */
int coproc_frame_data(struct coproc_client *co_client,
		      const void *data, size_t size);

/*
 * prepare the coproc data info to client structure
 * @co_client: the pointer to co-processor client
 * @data_info: a pointer to the coproc data info
 * @size: the size of the data info
 * @return: 0 on success, negative value for fail.
 *
 * Client driver could prepare  the coproc data info via calling this function
 */
int coproc_prepare(struct coproc_client *co_client,
                   const void *data_info, const size_t size);

/*
 * query the coproc data info from client structure
 * @co_client: the pointer to co-processor client
 * @data_info: the pointer of  the data info
 * @size: the size of the data info
 * @return: 0 on success, negative value for fail.
 *
 * Client driver query the coproc data info from client structure.
 */
int coproc_query(struct coproc_client *co_client,
                 void *data_info, const size_t size);
#else
static inline
int coproc_modeset_notify(struct coproc_client *co_client,
			  const struct drm_mode_modeinfo *drminfo)
{
	return 0;
}

static inline
int coproc_dpms_notify(struct coproc_client *co_client, const u8 state)
{
	return 0;
}

static inline
struct coproc_client *of_find_coproc_client_by_node(struct device_node *nd)
{
	return NULL;
}

static inline
int coproc_frame_data(struct coproc_client *co_client,
		      const void *data, size_t size)
{
	return 0;
}

static inline
int coproc_prepare(struct coproc_client *co_client,
                   const void *data_info, const size_t size)
{
	return 0;
}

static inline
int coproc_query(struct coproc_client *co_client,
                 void *data_info, const size_t size)
{
	return 0;
}
#endif

/* --co-processor client API-- */
/*
 * register co-processor client
 * @dev: the device of co-processor which will be registered
 * @client_cb: a callback function set
 *
 * @return: a pointer to co-processor client structure. NULL means fail
 *
 * co-processor driver calls this function to register itself to server driver,
 * the returned pointer will be used in the notify functions.
 */
struct coproc_client *coproc_register_client(struct device *dev,
			struct coproc_client_callbacks *client_cb);
/*
 * unregister client
 * @co_client: the pointer to co-processor client
 *
 * When client driver will be removed from driver stack, it calls this function
 * to unregister itself from co-processor server.
 */
void coproc_unregister_client(struct coproc_client *co_client);
/*
 * set driver data to client structure
 * @co_client: the pointer to co-processor client
 * @p: a pointer to driver data
 *
 * Client driver could store its data via calling this function
 */
void coproc_set_drvdata(struct coproc_client *co_client, void *p);
/*
 * get driver data from client structure
 * @co_client: the pointer to co-processor client
 *
 * @return: a pointer to driver data
 *
 * Client driver get its data from client structure.
 */
void *coproc_get_drvdata(struct coproc_client *co_client);

#endif /* _COPROC_H_ */
