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

#ifndef _DRM_BRIDGE_PS8622_H_
#define _DRM_BRIDGE_PS8622_H_

struct drm_encoder;

#ifdef CONFIG_DRM_PS8622

extern int ps8622_init(struct drm_encoder *encoder);

#else

static inline int ps8622_init(struct drm_encoder *encoder)
{
	return -ENODEV;
}

#endif

#endif

