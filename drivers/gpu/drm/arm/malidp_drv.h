/*
 * (C) COPYRIGHT 2013-2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * ARM Mali DP500/DP550 KMS/DRM driver structures
 */

#ifndef __MALIDP_5X0__
#define __MALIDP_5X0__

#include <linux/mutex.h>
#include <linux/wait.h>
#include "malidp_hw.h"

struct malidp_drm {
	struct malidp_hw_device *dev;
	struct drm_fbdev_cma *fbdev;
	struct list_head event_list;
	struct drm_crtc crtc;
	wait_queue_head_t wq;
	atomic_t config_valid;
};

#define crtc_to_malidp_device(x) container_of(x, struct malidp_drm, crtc)

struct malidp_plane {
	struct drm_plane base;
	struct malidp_hw_device *hwdev;
	const struct malidp_layer *layer;
};

#define to_malidp_plane(x) container_of(x, struct malidp_plane, base)

int malidp_wait_config_valid(struct drm_device *drm);
int malidp_de_planes_init(struct drm_device *drm);
void malidp_de_planes_destroy(struct drm_device *drm);
int malidp_crtc_init(struct drm_device *drm);
void malidp_crtc_enable(struct drm_crtc *crtc);
void malidp_crtc_disable(struct drm_crtc *crtc);

#endif  /* __MALIDP_5X0__ */
