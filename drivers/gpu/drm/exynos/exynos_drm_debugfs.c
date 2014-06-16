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

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <drm/drmP.h>
#include <drm/drm.h>
#include "exynos_drm_debugfs.h"
#include "exynos_drm_drv.h"

static int exynos_drm_gem_object_info(struct seq_file *m, void* data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct exynos_drm_private *dev_priv = dev->dev_private;

	mutex_lock(&dev->struct_mutex);

	seq_printf(m, "%u objects, %u bytes\n",
		   atomic_read(&dev_priv->mm.object_count),
		   atomic_read(&dev_priv->mm.object_memory));

	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static struct drm_info_list exynos_drm_debugfs_list[] = {
	{"exynos_gem_objects", exynos_drm_gem_object_info, 0},
};
#define EXYNOS_DRM_DEBUGFS_ENTRIES ARRAY_SIZE(exynos_drm_debugfs_list)

int exynos_drm_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(exynos_drm_debugfs_list,
					EXYNOS_DRM_DEBUGFS_ENTRIES,
					minor->debugfs_root, minor);
}

void exynos_drm_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(exynos_drm_debugfs_list,
				 EXYNOS_DRM_DEBUGFS_ENTRIES, minor);
}
