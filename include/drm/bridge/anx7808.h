/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DRM_BRIDGE_ANX7808_H_
#define _DRM_BRIDGE_ANX7808_H_

struct drm_encoder;

#ifdef CONFIG_DRM_ANX7808

extern int anx7808_init(struct drm_encoder *encoder);

#else

static inline int anx7808_init(struct drm_encoder *encoder)
{
	return 0;
}

#endif

#endif
