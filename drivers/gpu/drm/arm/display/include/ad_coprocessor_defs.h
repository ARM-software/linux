// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 *
 */
#ifndef _AD_COPROCESSOR_DEFS_H_
#define _AD_COPROCESSOR_DEFS_H_
#include <drm/drm_modes.h>

#define AD_MAX_ASSERTIVENESS	255
#define AD_MAX_STRENGTH_LIMIT	255
#define AD_MAX_DRC		65535

struct ad_coprocessor;

struct ad_coprocessor_funcs {
	/* send drm mode information to AD */
	int (*mode_set)(struct ad_coprocessor *ad, struct drm_display_mode *mode);
	/* enable AD. */
	int (*enable)(struct ad_coprocessor *ad);
	/* disable AD. */
	int (*disable)(struct ad_coprocessor *ad);
	/* set assertiveness to AD. (optional) */
	int (*assertiveness_set)(struct ad_coprocessor *ad, u32 assertiveness);
	/* set strength limit to AD. (optinal) */
	int (*strength_set)(struct ad_coprocessor *ad, u32 strength_limit);
	/* set dynamic-range compression to AD. (optional) */
	int (*drc_set)(struct ad_coprocessor *ad, u16 drc);
};

struct ad_coprocessor {
	struct device *dev;
	const struct ad_coprocessor_funcs *funcs;
	struct list_head ad_node;
};

struct ad_list {
	struct list_head head;
};

static inline void init_ad_list(struct ad_list *list)
{
	INIT_LIST_HEAD(&list->head);
}

static inline struct ad_coprocessor *
of_find_ad_coprocessor_by_node(struct ad_list *list, struct device_node *np)
{
	struct ad_coprocessor *ad = NULL;

	list_for_each_entry(ad, &list->head, ad_node)
		if (ad->dev->of_node == np)
			return ad;

	return NULL;
}

#endif
