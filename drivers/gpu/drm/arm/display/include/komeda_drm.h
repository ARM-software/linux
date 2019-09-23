/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 */

#ifndef _KOMEDA_DRM_
#define _KOMEDA_DRM_

enum komeda_hdr_eotf {
	MALIDP_HDR_NONE,
	MALIDP_HDR_ST2084,
	MALIDP_HDR_HLG,
};

struct komeda_hdr_roi {
	__u32 left, top;
	__u32 width, height;
};

struct komeda_primaries {
	__u16 x;
	__u16 y;
};

struct komeda_hdr_metadata {
	struct komeda_hdr_roi roi;
	enum komeda_hdr_eotf eotf;

	/* metadata defined in SMPTE ST 2086:2014 */

	struct komeda_primaries display_primaries_red;
	struct komeda_primaries display_primaries_green;
	struct komeda_primaries display_primaries_blue;
	struct komeda_primaries white_point;

	__u16 max_display_mastering_lum;
	__u16 min_display_mastering_lum;
	__u16 max_content_light_level;
	__u16 max_frame_average_light_level;
};
#endif /*_KOMEDA_DRM_*/
