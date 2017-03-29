/*
 * (C) COPYRIGHT 2016 ARM Limited. All rights reserved.
 * Author: Brian Starkey <brian.starkey@arm.com>
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 */

#ifndef __MALIDP_MW_H__
#define __MALIDP_MW_H__

int malidp_mw_connector_init(struct drm_device *drm);
void malidp_mw_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *old_state);
#endif
