// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#include <drm/drm_print.h>

#include "malidp_utils.h"
#include "komeda_color_mgmt.h"

/* 10bit precision YUV2RGB matrix */
static const s32 yuv2rgb_bt601_narrow[KOMEDA_N_YUV2RGB_COEFFS] = {
	1192,    0, 1634,
	1192, -401, -832,
	1192, 2066,    0,
	  64,  512,  512
};

static const s32 yuv2rgb_bt601_wide[KOMEDA_N_YUV2RGB_COEFFS] = {
	1024,    0, 1436,
	1024, -352, -731,
	1024, 1815,    0,
	   0,  512,  512
};

static const s32 yuv2rgb_bt709_narrow[KOMEDA_N_YUV2RGB_COEFFS] = {
	1192,    0, 1836,
	1192, -218, -546,
	1192, 2163,    0,
	  64,  512,  512
};

static const s32 yuv2rgb_bt709_wide[KOMEDA_N_YUV2RGB_COEFFS] = {
	1024,    0, 1613,
	1024, -192, -479,
	1024, 1900,    0,
	   0,  512,  512
};

static const s32 yuv2rgb_bt2020[KOMEDA_N_YUV2RGB_COEFFS] = {
	1024,    0, 1476,
	1024, -165, -572,
	1024, 1884,    0,
	   0,  512,  512
};

const s32 *komeda_select_yuv2rgb_coeffs(u32 color_encoding, u32 color_range)
{
	bool narrow = color_range == DRM_COLOR_YCBCR_LIMITED_RANGE;
	const s32 *coeffs;

	switch (color_encoding) {
	case DRM_COLOR_YCBCR_BT709:
		coeffs = narrow ? yuv2rgb_bt709_narrow : yuv2rgb_bt709_wide;
		break;
	case DRM_COLOR_YCBCR_BT601:
		coeffs = narrow ? yuv2rgb_bt601_narrow : yuv2rgb_bt601_wide;
		break;
	case DRM_COLOR_YCBCR_BT2020:
		coeffs = yuv2rgb_bt2020;
		break;
	default:
		coeffs = NULL;
		break;
	}

	return coeffs;
}

struct gamma_curve_sector {
	u32 boundary_start;
	u32 num_of_segments;
	u32 segment_width;
};

struct gamma_curve_segment {
	u32 start;
	u32 end;
};

static struct gamma_curve_sector sector_tbl[] = {
	{ 0,    4,  4   },
	{ 16,   4,  4   },
	{ 32,   4,  8   },
	{ 64,   4,  16  },
	{ 128,  4,  32  },
	{ 256,  4,  64  },
	{ 512,  16, 32  },
	{ 1024, 24, 128 },
};

static struct gamma_curve_sector igamma_sector_tbl[] = {
	{0, 64, 64},
};

static void
drm_lut_to_coeffs(struct drm_property_blob *lut_blob, u32 *coeffs,
		  struct gamma_curve_sector *sector_tbl, u32 num_sectors)
{
	struct drm_color_lut *lut;
	u32 i, j, in, num = 0;

	if (!lut_blob)
		return;

	lut = lut_blob->data;

	for (i = 0; i < num_sectors; i++) {
		for (j = 0; j < sector_tbl[i].num_of_segments; j++) {
			in = sector_tbl[i].boundary_start +
			     j * sector_tbl[i].segment_width;

			coeffs[num++] = drm_color_lut_extract(lut[in].red,
						KOMEDA_COLOR_PRECISION);
		}
	}

	coeffs[num] = BIT(KOMEDA_COLOR_PRECISION);
}

void drm_lut_to_igamma_coeffs(struct drm_property_blob *lut_blob, u32 *coeffs)
{
	drm_lut_to_coeffs(lut_blob, coeffs,
			  igamma_sector_tbl, ARRAY_SIZE(igamma_sector_tbl));
}

void drm_lut_to_fgamma_coeffs(struct drm_property_blob *lut_blob, u32 *coeffs)
{
	drm_lut_to_coeffs(lut_blob, coeffs,
			  sector_tbl, ARRAY_SIZE(sector_tbl));
}

void drm_ctm_to_coeffs(struct drm_property_blob *ctm_blob, u32 *coeffs)
{
	struct drm_color_ctm *ctm;
	u32 i;

	if (!ctm_blob)
		return;

	ctm = ctm_blob->data;

	for (i = 0; i < KOMEDA_N_CTM_COEFFS; ++i) {
		/* Convert from S31.32 to Q3.12. */
		s64 v = ctm->matrix[i];

		coeffs[i] = clamp_val(v, 1 - (1LL << 34), (1LL << 34) - 1) >> 20;
	}
}

void komeda_color_duplicate_state(struct komeda_color_state *new,
				  struct komeda_color_state *old)
{
	new->igamma = komeda_coeffs_get(old->igamma);
	new->fgamma = komeda_coeffs_get(old->fgamma);
}

void komeda_color_cleanup_state(struct komeda_color_state *color_st)
{
	komeda_coeffs_put(color_st->igamma);
	komeda_coeffs_put(color_st->fgamma);
}

int komeda_color_validate(struct komeda_color_manager *mgr,
			  struct komeda_color_state *st,
			  struct drm_property_blob *igamma_blob,
			  struct drm_property_blob *fgamma_blob)
{
	u32 coeffs[KOMEDA_N_GAMMA_COEFFS];

	komeda_color_cleanup_state(st);

	if (igamma_blob) {
		drm_lut_to_igamma_coeffs(igamma_blob, coeffs);
		st->igamma = komeda_coeffs_request(mgr->igamma_mgr, coeffs);
		if (!st->igamma) {
			DRM_DEBUG_ATOMIC("request igamma table failed.\n");
			return -EBUSY;
		}
	}

	if (fgamma_blob) {
		drm_lut_to_fgamma_coeffs(fgamma_blob, coeffs);
		st->fgamma = komeda_coeffs_request(mgr->fgamma_mgr, coeffs);
		if (!st->fgamma) {
			DRM_DEBUG_ATOMIC("request fgamma table failed.\n");
			return -EBUSY;
		}
	}

	return 0;
}
