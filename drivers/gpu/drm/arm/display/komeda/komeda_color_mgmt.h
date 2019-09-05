/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#ifndef _KOMEDA_COLOR_MGMT_H_
#define _KOMEDA_COLOR_MGMT_H_

#include <drm/drm_color_mgmt.h>
#include "komeda_coeffs.h"

#define KOMEDA_N_YUV2RGB_COEFFS		12
#define KOMEDA_N_RGB2YUV_COEFFS		12
#define KOMEDA_COLOR_PRECISION		12
#define KOMEDA_N_GAMMA_COEFFS		65
#define KOMEDA_COLOR_LUT_SIZE		BIT(KOMEDA_COLOR_PRECISION)
#define KOMEDA_N_CTM_COEFFS		9

struct komeda_color_manager {
	struct komeda_coeffs_manager *igamma_mgr;
	struct komeda_coeffs_manager *fgamma_mgr;
	bool has_ctm;
};

struct komeda_color_state {
	struct komeda_coeffs_table *igamma;
	struct komeda_coeffs_table *fgamma;
};

void komeda_color_duplicate_state(struct komeda_color_state *new,
				  struct komeda_color_state *old);
void komeda_color_cleanup_state(struct komeda_color_state *color_st);
int komeda_color_validate(struct komeda_color_manager *mgr,
			  struct komeda_color_state *st,
			  struct drm_property_blob *igamma_blob,
			  struct drm_property_blob *fgamma_blob);

void drm_lut_to_igamma_coeffs(struct drm_property_blob *lut_blob, u32 *coeffs);
void drm_lut_to_fgamma_coeffs(struct drm_property_blob *lut_blob, u32 *coeffs);
void drm_ctm_to_coeffs(struct drm_property_blob *ctm_blob, u32 *coeffs);

const s32 *komeda_select_yuv2rgb_coeffs(u32 color_encoding, u32 color_range);
const s32 *komeda_select_rgb2yuv_coeffs(u32 color_encoding, u32 color_range);

#endif /*_KOMEDA_COLOR_MGMT_H_*/
