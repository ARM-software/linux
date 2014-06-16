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

#ifndef _EXYNOS_DRM_DEBUGFS_H_
#define _EXYNOS_DRM_DEBUGFS_H_

int exynos_drm_debugfs_init(struct drm_minor *minor);
void exynos_drm_debugfs_cleanup(struct drm_minor *minor);

#endif
