/* SPDX-License-Identifier: GPL-2.0 */

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
