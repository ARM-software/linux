// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_print.h>
#include "komeda_dev.h"
#include "komeda_kms.h"
#include "komeda_framebuffer.h"
#include "komeda_color_mgmt.h"
#include "malidp_math.h"

static int
komeda_plane_init_data_flow(struct drm_plane_state *st,
			    struct komeda_crtc_state *kcrtc_st,
			    struct komeda_data_flow_cfg *dflow)
{
	struct komeda_plane *kplane = to_kplane(st->plane);
	struct komeda_plane_state *kplane_st = to_kplane_st(st);
	struct drm_framebuffer *fb = st->fb;
	const struct komeda_format_caps *caps = to_kfb(fb)->format_caps;
	struct komeda_pipeline *pipe = kplane->layer->base.pipeline;

	memset(dflow, 0, sizeof(*dflow));

	dflow->blending_zorder = st->normalized_zpos;
	if (pipe == to_kcrtc(st->crtc)->master)
		dflow->blending_zorder -= kcrtc_st->max_slave_zorder;
	if (dflow->blending_zorder < 0) {
		DRM_DEBUG_ATOMIC("%s zorder:%d < max_slave_zorder: %d.\n",
				 st->plane->name, st->normalized_zpos,
				 kcrtc_st->max_slave_zorder);
		return -EINVAL;
	}

	dflow->pixel_blend_mode = st->pixel_blend_mode;
	dflow->layer_alpha = st->alpha >> 8;

	dflow->out_x = st->crtc_x;
	dflow->out_y = st->crtc_y;
	dflow->out_w = st->crtc_w;
	dflow->out_h = st->crtc_h;

	dflow->in_x = st->src_x >> 16;
	dflow->in_y = st->src_y >> 16;
	dflow->in_w = st->src_w >> 16;
	dflow->in_h = st->src_h >> 16;

	dflow->rot = drm_rotation_simplify(st->rotation, caps->supported_rots);
	if (!has_bits(dflow->rot, caps->supported_rots)) {
		DRM_DEBUG_ATOMIC("rotation(0x%x) isn't supported by %s.\n",
				 dflow->rot,
				 komeda_get_format_name(caps->fourcc,
							fb->modifier));
		return -EINVAL;
	}

	dflow->en_atu = !!kplane_st->vp_outrect;
	dflow->pixel_blend_mode = st->pixel_blend_mode;
	komeda_complete_data_flow_cfg(kplane->layer, dflow, fb);

	return 0;
}

/**
 * komeda_plane_atomic_check - build input data flow
 * @plane: DRM plane
 * @state: the plane state object
 *
 * RETURNS:
 * Zero for success or -errno
 */
static int
komeda_plane_atomic_check(struct drm_plane *plane,
			  struct drm_plane_state *state)
{
	struct komeda_plane *kplane = to_kplane(plane);
	struct komeda_plane_state *kplane_st = to_kplane_st(state);
	struct komeda_layer *layer = kplane->layer;
	struct drm_crtc_state *crtc_st;
	struct komeda_crtc *kcrtc;
	struct komeda_crtc_state *kcrtc_st;
	struct komeda_data_flow_cfg dflow;
	int err;

	if (!state->crtc || !state->fb)
		return 0;

	crtc_st = drm_atomic_get_crtc_state(state->state, state->crtc);
	if (IS_ERR(crtc_st) || !crtc_st->enable) {
		DRM_DEBUG_ATOMIC("Cannot update plane on a disabled CRTC.\n");
		return -EINVAL;
	}

	/* crtc is inactive, skip the resource assignment */
	if (!crtc_st->active)
		return 0;

	kcrtc = to_kcrtc(crtc_st->crtc);
	kcrtc_st = to_kcrtc_st(crtc_st);

	err = komeda_plane_init_data_flow(state, kcrtc_st, &dflow);
	if (err)
		return err;

	if (kcrtc->side_by_side)
		err = komeda_build_layer_sbs_data_flow(layer,
				kplane_st, kcrtc_st, &dflow);
	else if (dflow.en_split)
		err = komeda_build_layer_split_data_flow(layer,
				kplane_st, kcrtc_st, &dflow);
	else
		err = komeda_build_layer_data_flow(layer,
				kplane_st, kcrtc_st, &dflow);

	return err;
}

/* plane doesn't represent a real HW, so there is no HW update for plane.
 * komeda handles all the HW update in crtc->atomic_flush
 */
static void
komeda_plane_atomic_update(struct drm_plane *plane,
			   struct drm_plane_state *old_state)
{
}

static const struct drm_plane_helper_funcs komeda_plane_helper_funcs = {
	.atomic_check	= komeda_plane_atomic_check,
	.atomic_update	= komeda_plane_atomic_update,
};

static void komeda_plane_destroy(struct drm_plane *plane)
{
	drm_plane_cleanup(plane);

	kfree(to_kplane(plane));
}

static void komeda_plane_reset(struct drm_plane *plane)
{
	struct komeda_plane_state *state;
	struct komeda_plane *kplane = to_kplane(plane);

	if (plane->state)
		__drm_atomic_helper_plane_destroy_state(plane->state);

	kfree(plane->state);
	plane->state = NULL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state) {
		state->base.rotation = DRM_MODE_ROTATE_0;
		state->base.pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;
		state->base.alpha = DRM_BLEND_ALPHA_OPAQUE;
		state->base.color_encoding = DRM_COLOR_YCBCR_BT601;
		state->base.color_range = DRM_COLOR_YCBCR_LIMITED_RANGE;
		if (kplane->layer)
			state->base.zpos = kplane->layer->base.id;
		else
			state->base.zpos = kplane->atu->base.id -
					   KOMEDA_COMPONENT_ATU0;
		plane->state = &state->base;
		plane->state->plane = plane;
	}
}

static struct drm_plane_state *
komeda_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct komeda_plane_state *new;

	if (WARN_ON(!plane->state))
		return NULL;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &new->base);

	return &new->base;
}

static void
komeda_plane_atomic_destroy_state(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	__drm_atomic_helper_plane_destroy_state(state);
	kfree(to_kplane_st(state));
}

static int
komeda_plane_atomic_get_property(struct drm_plane *plane,
				 const struct drm_plane_state *state,
				 struct drm_property *property,
				 uint64_t *val)
{
	struct drm_device *drm = plane->dev;
	struct komeda_kms_dev *kms = to_kdev(drm);
	struct komeda_plane_state *st = to_kplane_st(state);
	struct komeda_plane *kplane = to_kplane(plane);

	if (property == kms->prop_spline_coeff_r)
		*val = (st->spline_coeff_r) ? st->spline_coeff_r->base.id : 0;
	else if (property == kms->prop_spline_coeff_g)
		*val = (st->spline_coeff_g) ? st->spline_coeff_g->base.id : 0;
	else if (property == kms->prop_spline_coeff_b)
		*val = (st->spline_coeff_b) ? st->spline_coeff_b->base.id : 0;
	else if (property == kplane->prop_viewport_outrect)
		*val = (st->vp_outrect) ? st->vp_outrect->base.id : 0;
	else if (property == kplane->prop_viewport_trans)
		*val = (st->viewport_trans) ? st->viewport_trans->base.id : 0;
	else if (property == kplane->prop_layer_projection)
		*val = (st->layer_project) ? st->layer_project->base.id : 0;
	else if (property == kplane->prop_layer_quad)
		*val = (st->layer_quad) ? st->layer_quad->base.id : 0;

	return 0;
}

static int
komeda_plane_atomic_set_property(struct drm_plane *plane,
				 struct drm_plane_state *state,
				 struct drm_property *property,
				 uint64_t val)
{
	struct drm_device *drm = plane->dev;
	struct komeda_plane *kplane = to_kplane(plane);
	struct komeda_kms_dev *kms = to_kdev(drm);
	struct komeda_plane_state *kplane_st = to_kplane_st(state);
	bool replaced = false;
	int ret = 0;

	if (property == kms->prop_spline_coeff_r) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->spline_coeff_r,
					val,
					KOMEDA_SPLINE_COEFF_SIZE * sizeof(u32),
					KOMEDA_SPLINE_COEFF_SIZE, &replaced);
		kplane_st->spline_coeff_r_changed = replaced;
	} else if (property == kms->prop_spline_coeff_g) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->spline_coeff_g,
					val,
					KOMEDA_SPLINE_COEFF_SIZE * sizeof(u32),
					KOMEDA_SPLINE_COEFF_SIZE, &replaced);
		kplane_st->spline_coeff_g_changed = replaced;
	} else if (property == kms->prop_spline_coeff_b) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->spline_coeff_b,
					val,
					KOMEDA_SPLINE_COEFF_SIZE * sizeof(u32),
					KOMEDA_SPLINE_COEFF_SIZE, &replaced);
		kplane_st->spline_coeff_b_changed = replaced;
	} else if (property == kplane->prop_viewport_outrect) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->vp_outrect,
					val,
					4 * sizeof(uint32_t),
					-1, &replaced);
		kplane_st->vp_rect_changed = replaced;
	} else if (property == kplane->prop_viewport_trans) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->viewport_trans,
					val,
					sizeof(struct malidp_matrix4),
					-1, &replaced);
		kplane_st->mat_coeff_changed = replaced;
	} else if (property == kplane->prop_layer_projection) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->layer_project,
					val,
					sizeof(struct malidp_matrix4),
					-1, &replaced);
		kplane_st->mat_coeff_changed = replaced;
	} else if (property == kplane->prop_layer_quad) {
		ret = drm_property_replace_blob_from_id(drm,
					&kplane_st->layer_quad,
					val,
					sizeof(struct malidp_matrix4),
					-1, &replaced);
		kplane_st->mat_coeff_changed = replaced;
	} else if (property == kplane->prop_viewport_clamp) {
		kplane_st->viewport_clamp = !!val;
	}

	return ret;
}

static bool
komeda_plane_format_mod_supported(struct drm_plane *plane,
				  u32 format, u64 modifier)
{
	struct komeda_dev *mdev = plane->dev->dev_private;
	struct komeda_plane *kplane = to_kplane(plane);
	u32 layer_type = kplane->layer ?
			 kplane->layer->layer_type : kplane->atu->layer_type;

	return komeda_format_mod_supported(&mdev->fmt_tbl, layer_type,
					   format, modifier, 0);
}

static const struct drm_plane_funcs komeda_plane_funcs = {
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= komeda_plane_destroy,
	.reset			= komeda_plane_reset,
	.atomic_duplicate_state	= komeda_plane_atomic_duplicate_state,
	.atomic_destroy_state	= komeda_plane_atomic_destroy_state,
	.atomic_get_property	= komeda_plane_atomic_get_property,
	.atomic_set_property	= komeda_plane_atomic_set_property,
	.format_mod_supported	= komeda_plane_format_mod_supported,
};

/* for komeda, which is pipeline can be share between crtcs */
static u32 get_possible_crtcs(struct komeda_kms_dev *kms,
			      struct komeda_pipeline *pipe)
{
	struct komeda_crtc *crtc;
	u32 possible_crtcs = 0;
	int i;

	for (i = 0; i < kms->n_crtcs; i++) {
		crtc = &kms->crtcs[i];

		if ((pipe == crtc->master) || (pipe == crtc->slave))
			possible_crtcs |= BIT(i);
	}

	return possible_crtcs;
}

static void
komeda_set_crtc_plane_mask(struct komeda_kms_dev *kms,
			   struct komeda_pipeline *pipe,
			   struct drm_plane *plane)
{
	struct komeda_crtc *kcrtc;
	int i;

	for (i = 0; i < kms->n_crtcs; i++) {
		kcrtc = &kms->crtcs[i];

		if (pipe == kcrtc->slave)
			kcrtc->slave_planes |= BIT(drm_plane_index(plane));
	}
}

/* use Layer0 as primary */
static u32 get_plane_type(struct komeda_kms_dev *kms,
			  struct komeda_component *c)
{
	bool is_primary = (c->id == KOMEDA_COMPONENT_LAYER0);

	return is_primary ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
}

static int
attach_atu_property_to_plane(struct komeda_kms_dev *kms,
			     struct komeda_plane *kplane,
			     struct komeda_atu *atu)
{
	struct drm_plane *plane = &kplane->base;

	kplane->prop_viewport_outrect = drm_property_create(plane->dev,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"VIEWPORT_OUTRECT", 0);

	if (!kplane->prop_viewport_outrect)
		return -ENOMEM;

	kplane->prop_viewport_trans = drm_property_create(plane->dev,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"VIEWPORT_TRANS", 0);
	if (!kplane->prop_viewport_trans)
		return -ENOMEM;

	kplane->prop_layer_projection = drm_property_create(plane->dev,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"LAYER_PROJECTION", 0);

	if (!kplane->prop_layer_projection)
		return -ENOMEM;

	kplane->prop_layer_quad = drm_property_create(plane->dev,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"LAYER_QUAD", 0);

	if (!kplane->prop_layer_quad)
		return -ENOMEM;

	/* property: layer clamp */
	kplane->prop_viewport_clamp = drm_property_create_range(plane->dev,
			DRM_MODE_PROP_ATOMIC, "VIEWPORT_CLAMP", 0, 1);
	if (!kplane->prop_viewport_clamp)
		return -ENOMEM;

	/* other property */
	drm_object_attach_property(&plane->base, kplane->prop_viewport_outrect, 0);
	drm_object_attach_property(&plane->base, kplane->prop_viewport_trans, 0);
	drm_object_attach_property(&plane->base, kplane->prop_layer_projection, 0);
	drm_object_attach_property(&plane->base, kplane->prop_layer_quad, 0);
	drm_object_attach_property(&plane->base, kplane->prop_viewport_clamp, 0);
	drm_object_attach_property(&plane->base, kms->prop_spline_coeff_r, 0);
	drm_object_attach_property(&plane->base, kms->prop_spline_coeff_g, 0);
	drm_object_attach_property(&plane->base, kms->prop_spline_coeff_b, 0);

	return 0;
}

static int komeda_plane_add(struct komeda_kms_dev *kms,
			    struct komeda_layer *layer, struct komeda_atu *atu)
{
	struct komeda_dev *mdev = kms->base.dev_private;
	struct komeda_component *master_comp;
	struct komeda_color_manager *color_mgr;
	struct komeda_plane *kplane;
	struct drm_plane *plane;
	u32 *formats, layer_type, n_formats = 0, zpos;
	int err;

	kplane = kzalloc(sizeof(*kplane), GFP_KERNEL);
	if (!kplane)
		return -ENOMEM;

	plane = &kplane->base;
	kplane->layer = layer;
	kplane->atu  = atu;

	master_comp = layer ? &layer->base : &atu->base;
	layer_type = layer ? layer->layer_type : atu->layer_type;
	zpos = layer ? layer->base.id : atu->base.id - KOMEDA_COMPONENT_ATU0;

	formats = komeda_get_layer_fourcc_list(&mdev->fmt_tbl,
					       layer_type, &n_formats);

	err = drm_universal_plane_init(&kms->base, plane,
			get_possible_crtcs(kms, master_comp->pipeline),
			&komeda_plane_funcs,
			formats, n_formats, komeda_supported_modifiers,
			get_plane_type(kms, master_comp),
			"%s", master_comp->name);

	komeda_put_fourcc_list(formats);

	if (err)
		goto cleanup;

	if (atu)
		attach_atu_property_to_plane(kms, kplane, atu);

	drm_plane_helper_add(plane, &komeda_plane_helper_funcs);

	err = drm_plane_create_alpha_property(plane);
	if (err)
		goto cleanup;

	err = drm_plane_create_blend_mode_property(plane,
			BIT(DRM_MODE_BLEND_PIXEL_NONE) |
			BIT(DRM_MODE_BLEND_PREMULTI)   |
			BIT(DRM_MODE_BLEND_COVERAGE));
	if (err)
		goto cleanup;

	err = drm_plane_create_color_properties(plane,
			BIT(DRM_COLOR_YCBCR_BT601) |
			BIT(DRM_COLOR_YCBCR_BT709) |
			BIT(DRM_COLOR_YCBCR_BT2020),
			BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
			BIT(DRM_COLOR_YCBCR_FULL_RANGE),
			DRM_COLOR_YCBCR_BT601,
			DRM_COLOR_YCBCR_LIMITED_RANGE);
	if (err)
		goto cleanup;

	err = drm_plane_color_create_prop(plane->dev, plane);
	if (err)
		goto cleanup;

	color_mgr = atu ? & atu->color_mgr : &layer->color_mgr;
	drm_plane_enable_color_mgmt(plane,
			color_mgr->igamma_mgr ? KOMEDA_COLOR_LUT_SIZE : 0,
			color_mgr->has_ctm,
			color_mgr->fgamma_mgr ? KOMEDA_COLOR_LUT_SIZE : 0);

	err = drm_plane_create_zpos_property(plane, zpos, 0, 8);
	if (err)
		goto cleanup;

	komeda_set_crtc_plane_mask(kms, master_comp->pipeline, plane);

	/* below is the normal layer properties */
	if (!layer)
		return 0;

	err = drm_plane_create_rotation_property(plane, DRM_MODE_ROTATE_0,
						 layer->supported_rots);
	if (err)
		goto cleanup;

	return 0;
cleanup:
	komeda_plane_destroy(plane);
	return err;
}

int komeda_kms_create_plane_properties(struct komeda_kms_dev *kms,
				       struct komeda_dev *mdev)
{
	struct drm_device *drm = &kms->base;
	struct drm_property *prop;
	bool has_atu = !!mdev->pipelines[0]->n_atus;

	if (!has_atu)
		return 0;

	prop = drm_property_create(drm,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"SPLINE_COEFF_R", 0);
	if (!prop)
		return -ENOMEM;
	kms->prop_spline_coeff_r = prop;

	prop = drm_property_create(drm,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"SPLINE_COEFF_G", 0);
	if (!prop)
		return -ENOMEM;
	kms->prop_spline_coeff_g = prop;

	prop = drm_property_create(drm,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"SPLINE_COEFF_B", 0);
	if (!prop)
		return -ENOMEM;
	kms->prop_spline_coeff_b = prop;

	return 0;
}

static struct komeda_atu *
get_atu_by_layer(struct komeda_pipeline *ppl, struct komeda_layer *layer)
{
	int i;

	for (i = 0; i < ppl->n_atus; i++)
		if (ppl->atu[i]->slave_resource == layer->base.id)
			return ppl->atu[i];

	return NULL;
}

static struct komeda_plane *
get_atu_vp_buddy(struct komeda_kms_dev *kms, struct komeda_plane *kplane)
{
	struct komeda_plane *node;
	struct drm_plane *plane;

	drm_for_each_plane(plane, &kms->base) {
		node = to_kplane(plane);
		if (!node->atu || node == kplane)
      			continue;
		if (node->atu == kplane->atu)
			return node;
	}
	return NULL;
}

int komeda_kms_add_planes(struct komeda_kms_dev *kms, struct komeda_dev *mdev)
{
	struct komeda_pipeline *pipe;
	struct komeda_plane *kplane;
	struct drm_plane *plane;
	int i, j, err;

	for (i = 0; i < mdev->n_pipelines; i++) {
		pipe = mdev->pipelines[i];

		for (j = 0; j < pipe->n_layers; j++) {
			struct komeda_layer *layer = pipe->layers[j];
			struct komeda_atu *atu = get_atu_by_layer(pipe, layer);
			err = komeda_plane_add(kms, layer, atu);
			if (err)
				return err;
		}
	}
	/* Add ATU*_VP1 planes */
	for (i = 0; i < mdev->n_pipelines; i++) {
		pipe = mdev->pipelines[i];

		for (j = 0; j < pipe->n_atus; j++) {
			if (pipe->atu[j]->n_vp < 2)
				continue;
			err = komeda_plane_add(kms, NULL, pipe->atu[j]);
			if (err)
				return err;
		}
	}

	drm_for_each_plane(plane, &kms->base) {
		kplane = to_kplane(plane);
		if (!kplane->atu)
			continue;
		kplane->atu_vp_buddy = get_atu_vp_buddy(kms, kplane);
	}

	return 0;
}
