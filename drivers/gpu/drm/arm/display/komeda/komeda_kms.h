/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#ifndef _KOMEDA_KMS_H_
#define _KOMEDA_KMS_H_

#include <linux/list.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_writeback.h>
#include <drm/drm_print.h>

#include "komeda_pipeline.h"
#include "komeda_framebuffer.h"

/**
 * struct komeda_plane - komeda instance of drm_plane
 */
struct komeda_plane {
	/** @base: &drm_plane */
	struct drm_plane base;

	/**
	 * @force_layer_split:
	 *
	 * debug flag, if true, force enable layer_split when this plane
	 * has been enabled.
	 */
	bool force_layer_split;

	/**
	 * @layer:
	 *
	 * represents available layer input pipelines for this plane.
	 *
	 * NOTE:
	 * the layer is not for a specific Layer, but indicate a group of
	 * Layers with same capabilities.
	 */
	struct komeda_layer *layer;

	/**
	 * @atu: represents which ATU this plane belongs to.
	 * if it's not a ATU plane, leave this field to be NULL;
	 */
	struct komeda_atu *atu;
	/**
	 * @atu_vp_buddy: points to its viewport buddy.
	 *
	 * Note:
	 * every ATU has 2 vp(viewports), and each vp is binded with a plane.
	 * and these 2 vp are buddies.
	 */
	struct komeda_plane *atu_vp_buddy; /* point to it's vp buddy */

	/** @prop_viewport_outrect: describe a viewport rect start point and size */
	struct drm_property *prop_viewport_outrect;
	/** @prop_viewport_trans: represents head position when gpu render the framebuffer */
	struct drm_property *prop_viewport_trans;
	/** @prop_layer_projection: represents head position of XR devices in reprojection mode*/
	struct drm_property *prop_layer_projection;
	/** @prop_layer_quad: represents head position of XR devices in quad mode*/
	struct drm_property *prop_layer_quad;
	/** @prop_viewport_clamp: represents clamp effect */
	struct drm_property *prop_viewport_clamp;
	/** @prop_channel_scaling: represents channel scaling*/
	struct drm_property *prop_channel_scaling;
};

/**
 * struct komeda_plane_state
 *
 * The plane_state can be split into two data flow (left/right) and handled
 * by two layers &komeda_plane.layer and &komeda_plane.layer.right
 */
struct komeda_plane_state {
	/** @base: &drm_plane_state */
	struct drm_plane_state base;
	/** @zlist_node: zorder list node */
	struct list_head zlist_node;

	/** @dflow: the input data flow configuration computed by state */
	struct komeda_data_flow_cfg dflow;

	bool viewport_clamp;

	/** @spline_coeff_r_changed: the value in "spline_coeff_r" changed or not */
	u8 spline_coeff_r_changed : 1,
	/** @spline_coeff_g_changed: the value in "spline_coeff_r" changed or not */
	   spline_coeff_g_changed : 1,
	/** @spline_coeff_b_changed: the value in "spline_coeff_r" changed or not */
	   spline_coeff_b_changed : 1,
	/** @mat_coeff_changed: any matrix props value changed or not */
		mat_coeff_changed : 1,
	/** @vp_rect_changed: the value in "vp_outrect" changed or not */
		  vp_rect_changed : 1;

	u32 channel_scaling;

	struct drm_property_blob *spline_coeff_r;
	struct drm_property_blob *spline_coeff_g;
	struct drm_property_blob *spline_coeff_b;
	struct drm_property_blob *vp_outrect;
	struct drm_property_blob *viewport_trans;
	struct drm_property_blob *layer_project;
	struct drm_property_blob *layer_quad;
};

/**
 * struct komeda_wb_connector
 */
struct komeda_wb_connector {
	/** @base: &drm_writeback_connector */
	struct drm_writeback_connector base;

	/** @wb_layer: represents associated writeback pipeline of komeda */
	struct komeda_layer *wb_layer;

	/** @expected_pipes: pipelines are used for the writeback job */
	u32 expected_pipes;
	/** @complete_pipes: pipelines which have finished writeback */
	u32 complete_pipes;
	/**
	 * @color_encoding_property: enum property for specifying color encoding
	 * for non RGB formats for writeback layer.
	 */
	struct drm_property *color_encoding_property;
	/**
	 * @color_range_property: enum property for specifying color range for
	 * non RGB formats for writeback layer.
	 */
	struct drm_property *color_range_property;
};

/**
 * struct komeda_wb_connector_state
 */
struct komeda_wb_connector_state {
	struct drm_connector_state base;

	enum drm_color_encoding color_encoding;
	enum drm_color_range color_range;
};

/**
 * struct komeda_crtc
 */
struct komeda_crtc {
	/** @base: &drm_crtc */
	struct drm_crtc base;
	/** @master: only master has display output */
	struct komeda_pipeline *master;
	/**
	 * @slave: optional
	 *
	 * Doesn't have its own display output, the handled data flow will
	 * merge into the master.
	 */
	struct komeda_pipeline *slave;

	/** @side_by_side: if the master and slave works on side by side mode */
	bool side_by_side;

	/** @slave_planes: komeda slave planes mask */
	u32 slave_planes;

	/** @wb_conn: komeda write back connector */
	struct komeda_wb_connector *wb_conn;

	/** @disable_done: this flip_done is for tracing the disable */
	struct completion *disable_done;

	/* protected mode property */
	struct drm_property *protected_mode_property;
	/* coprocessor property */
	struct drm_property *coproc_property;
	/* assertive display properties */
	struct drm_property *assertiveness_property;
	struct drm_property *strength_limit_property;
	struct drm_property *drc_property;
	/* ATU sensor buffer properties */
	struct drm_property *sensor_buf_property;
	struct komeda_sensor_buff s_buff;
};

/**
 * struct komeda_crtc_state
 */
struct komeda_crtc_state {
	/** @base: &drm_crtc_state */
	struct drm_crtc_state base;

	/* private properties */

	/* computed state which are used by validate/check */
	/**
	 * @affected_pipes:
	 * the affected pipelines in once display instance
	 */
	u32 affected_pipes;
	/**
	 * @active_pipes:
	 * the active pipelines in once display instance
	 */
	u32 active_pipes;

	/** @max_slave_zorder: the maximum of slave zorder */
	u32 max_slave_zorder;

	bool en_coproc;
	bool en_protected_mode;
	u32 assertiveness;
	u32 strength_limit;
	u16 drc;
	union {
		u32 assertive_changed : 1,
		     strength_changed : 1,
			  drc_changed : 1;
		u32 cfg_changed;
	};

	/** @pl3: pl3 irq*/
	u32 pl3;
};

/** struct komeda_kms_dev - for gather KMS related things */
struct komeda_kms_dev {
	/** @base: &drm_device */
	struct drm_device base;

	/** @n_crtcs: valid numbers of crtcs in &komeda_kms_dev.crtcs */
	int n_crtcs;
	/** @crtcs: crtcs list */
	struct komeda_crtc crtcs[KOMEDA_MAX_PIPELINES];

	/** @prop_spline_coeff_r: ATU LDC spline coefficients prop for red channel */
	struct drm_property *prop_spline_coeff_r;
	/** @prop_spline_coeff_g: ATU LDC spline coefficients prop for green channel */
	struct drm_property *prop_spline_coeff_g;
	/** @prop_spline_coeff_b: ATU LDC spline coefficients prop for blue channel */
	struct drm_property *prop_spline_coeff_b;
};

#define to_kplane(p)	container_of(p, struct komeda_plane, base)
#define to_kplane_st(p)	container_of(p, struct komeda_plane_state, base)
#define to_kconn(p)	container_of(p, struct komeda_wb_connector, base)
#define to_kcrtc(p)	container_of(p, struct komeda_crtc, base)
#define to_kcrtc_st(p)	container_of(p, struct komeda_crtc_state, base)
#define to_kdev(p)	container_of(p, struct komeda_kms_dev, base)
#define to_wb_conn(x)	container_of(x, struct drm_writeback_connector, base)
#define to_kconn_st(p)	container_of(p, struct komeda_wb_connector_state, base)

#define _drm_conn_to_kconn(c)   to_kconn(to_wb_conn((c)))

static inline bool is_writeback_only(struct drm_crtc_state *st)
{
	struct komeda_wb_connector *wb_conn = to_kcrtc(st->crtc)->wb_conn;
	struct drm_connector *conn = wb_conn ? &wb_conn->base.base : NULL;

	return conn && (st->connector_mask == BIT(drm_connector_index(conn)));
}

static inline bool
is_only_changed_connector(struct drm_crtc_state *st, struct drm_connector *conn)
{
	struct drm_crtc_state *old_st;
	u32 changed_connectors;

	old_st = drm_atomic_get_old_crtc_state(st->state, st->crtc);
	changed_connectors = st->connector_mask ^ old_st->connector_mask;

	return BIT(drm_connector_index(conn)) == changed_connectors;
}

static inline bool has_flip_h(u32 rot)
{
	u32 rotation = drm_rotation_simplify(rot,
					     DRM_MODE_ROTATE_0 |
					     DRM_MODE_ROTATE_90 |
					     DRM_MODE_REFLECT_MASK);

	if (rotation & DRM_MODE_ROTATE_90)
		return !!(rotation & DRM_MODE_REFLECT_Y);
	else
		return !!(rotation & DRM_MODE_REFLECT_X);
}

void komeda_crtc_get_color_config(struct drm_crtc_state *crtc_st,
				  u32 *color_depths, u32 *color_formats);
unsigned long komeda_crtc_get_aclk(struct komeda_crtc_state *kcrtc_st);

static inline struct drm_property_blob *
komeda_drm_blob_get(struct drm_property_blob *blob)
{
	if (!blob)
		return NULL;

	return drm_property_blob_get(blob);
}

int komeda_kms_setup_crtcs(struct komeda_kms_dev *kms, struct komeda_dev *mdev);

int komeda_kms_add_crtcs(struct komeda_kms_dev *kms, struct komeda_dev *mdev);
int komeda_kms_add_planes(struct komeda_kms_dev *kms, struct komeda_dev *mdev);
int komeda_kms_add_private_objs(struct komeda_kms_dev *kms,
				struct komeda_dev *mdev);
int komeda_kms_add_wb_connectors(struct komeda_kms_dev *kms,
				 struct komeda_dev *mdev);
void komeda_kms_cleanup_private_objs(struct komeda_kms_dev *kms);
int komeda_kms_create_plane_properties(struct komeda_kms_dev *kms,
				       struct komeda_dev *mdev);
void komeda_crtc_handle_event(struct komeda_crtc   *kcrtc,
			      struct komeda_events *evts);

int komeda_kms_init(struct komeda_kms_dev *kms, struct komeda_dev *mdev);
void komeda_kms_fini(struct komeda_kms_dev *kms);
int komeda_plane_init_data_flow(struct drm_plane_state *st,
				struct komeda_crtc_state *kcrtc_st,
				struct komeda_data_flow_cfg *dflow);
int komeda_kms_crtcs_add_ad_properties(struct komeda_kms_dev *kms,
				       struct komeda_dev *mdev);

int komeda_plane_prepare(struct drm_plane *plane,
			 struct drm_plane_state *state);
#endif /*_KOMEDA_KMS_H_*/
