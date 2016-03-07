/*
 * (C) COPYRIGHT 2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * ARM Mali DP driver (crtc operations)
 */

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <linux/clk.h>
#include <video/videomode.h>

#include "malidp_drv.h"
#include "malidp_hw.h"

static bool malidp_crtc_mode_fixup(struct drm_crtc *crtc,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);
	struct malidp_hw_device *hwdev = malidp->dev;

	/* check that the hardware can drive the required rate clock */
	long rate, req_rate = mode->crtc_clock * 1000;

	rate = clk_round_rate(hwdev->mclk, req_rate);
	if (rate < req_rate) {
		DRM_INFO("mclk clock unable to reach %d kHz\n", mode->crtc_clock);
		return false;
	}

	rate = clk_round_rate(hwdev->pxlclk, req_rate);
	if (rate != req_rate) {
		DRM_INFO("pxlclk clock rate %ld not supported: can do %ld\n",
			 req_rate, rate);
		return false;
	}

	adjusted_mode->crtc_clock = rate / 1000;

	return true;
}

static void malidp_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);
	struct malidp_hw_device *hwdev = malidp->dev;
	struct videomode vm;

	drm_display_mode_to_videomode(&crtc->state->adjusted_mode, &vm);

	/* mclk needs to be set to the same or higher rate than pxlclk */
	clk_set_rate(hwdev->mclk, crtc->state->adjusted_mode.crtc_clock * 1000);
	clk_set_rate(hwdev->pxlclk, crtc->state->adjusted_mode.crtc_clock * 1000);

	hwdev->modeset(hwdev, &vm);
}

void malidp_crtc_enable(struct drm_crtc *crtc)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);
	struct malidp_hw_device *hwdev = malidp->dev;

	clk_prepare_enable(hwdev->pxlclk);
	hwdev->leave_config_mode(hwdev);
}

void malidp_crtc_disable(struct drm_crtc *crtc)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);
	struct malidp_hw_device *hwdev = malidp->dev;

	hwdev->enter_config_mode(hwdev);
	clk_disable_unprepare(hwdev->pxlclk);
}

static int malidp_crtc_atomic_check(struct drm_crtc *crtc,
				    struct drm_crtc_state *state)
{
	/* ToDo: check for shared resources used by planes */
	return 0;
}

static void malidp_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_state)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);

	if (crtc->state->event) {
		struct drm_pending_vblank_event *event = crtc->state->event;
		unsigned long flags;

		crtc->state->event = NULL;
		event->pipe = drm_crtc_index(crtc);

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		list_add_tail(&event->base.link, &malidp->event_list);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void malidp_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_crtc_state *old_state)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);
	struct drm_device *drm = crtc->dev;
	int ret = malidp_wait_config_valid(drm);

	if (!ret) {
		unsigned long flags;
		struct drm_pending_vblank_event *e;

		spin_lock_irqsave(&drm->event_lock, flags);
		e = list_first_entry_or_null(&malidp->event_list, typeof(*e),
					     base.link);
		if (e) {
			list_del(&e->base.link);
			drm_crtc_send_vblank_event(&malidp->crtc, e);
			drm_crtc_vblank_put(&malidp->crtc);
		}
		spin_unlock_irqrestore(&drm->event_lock, flags);
	} else {
		DRM_DEBUG_DRIVER("timed out waiting for updated configuration\n");
	}
}

static const struct drm_crtc_helper_funcs malidp_crtc_helper_funcs = {
	.mode_fixup = malidp_crtc_mode_fixup,
	.mode_set = drm_helper_crtc_mode_set,
	.mode_set_base = drm_helper_crtc_mode_set_base,
	.mode_set_nofb = malidp_crtc_mode_set_nofb,
	.enable = malidp_crtc_enable,
	.disable = malidp_crtc_disable,
	.atomic_check = malidp_crtc_atomic_check,
	.atomic_begin = malidp_crtc_atomic_begin,
	.atomic_flush = malidp_crtc_atomic_flush,
};

static void malidp_crtc_destroy(struct drm_crtc *crtc)
{
	struct malidp_drm *malidp = crtc_to_malidp_device(crtc);
	struct malidp_hw_device *hwdev = malidp->dev;

	clk_disable_unprepare(hwdev->pxlclk);
	clk_disable_unprepare(hwdev->mclk);
	drm_crtc_cleanup(crtc);
}

static const struct drm_crtc_funcs malidp_crtc_funcs = {
	.destroy = malidp_crtc_destroy,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

int malidp_crtc_init(struct drm_device *drm)
{
	struct malidp_drm *malidp = drm->dev_private;
	struct drm_plane *primary = NULL, *plane;
	int ret;

	drm_for_each_plane(plane, drm) {
		if (plane->type == DRM_PLANE_TYPE_PRIMARY) {
			primary = plane;
			break;
		}
	}
	if (!primary) {
		DRM_ERROR("no primary plane found\n");
		return -EINVAL;
	}

	ret = drm_crtc_init_with_planes(drm, &malidp->crtc, primary, NULL,
					&malidp_crtc_funcs, NULL);

	if (ret) {
		malidp_de_planes_destroy(drm);
		return ret;
	}

	drm_crtc_helper_add(&malidp->crtc, &malidp_crtc_helper_funcs);
	return 0;
}
