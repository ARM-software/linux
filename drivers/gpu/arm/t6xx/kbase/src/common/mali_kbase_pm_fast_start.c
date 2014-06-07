/*
 *
 * (C) COPYRIGHT 2010-2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file mali_kbase_pm_fast_start.c
 * A 'fast start' demand based power policy. This will power on all shader
 * cores when GPU power is enabled, before any jobs are submitted, but will
 * then behave as a demand policy.
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_pm.h>

static u64 fast_start_get_core_mask(struct kbase_device *kbdev)
{	
	struct kbasep_pm_policy_fast_start *data = &kbdev->pm.pm_policy_data.fast_start;

	u64 desired = kbdev->shader_needed_bitmap | kbdev->shader_inuse_bitmap;

	if (0 == kbdev->pm.active_count)
		return 0;

	if (0 != kbdev->shader_needed_bitmap)
		data->fast_start = MALI_FALSE;

	if (data->fast_start != MALI_FALSE)
		return kbdev->shader_present_bitmap;
		
	return desired;
}

static mali_bool fast_start_get_core_active(struct kbase_device *kbdev)
{
	struct kbasep_pm_policy_fast_start *data = &kbdev->pm.pm_policy_data.fast_start;

	if (0 != kbdev->pm.active_count && data->active_state == MALI_FALSE)
		data->fast_start = MALI_TRUE;

	if (0 == kbdev->pm.active_count)
	{
		data->fast_start = MALI_FALSE;
		data->active_state = MALI_FALSE;
		return MALI_FALSE;
	}

	data->active_state = MALI_TRUE;
	return MALI_TRUE;
}

static void fast_start_init(struct kbase_device *kbdev)
{
	struct kbasep_pm_policy_fast_start *data = &kbdev->pm.pm_policy_data.fast_start;

	data->fast_start = MALI_FALSE;
	data->active_state = MALI_FALSE;
}

static void fast_start_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

/** The @ref kbase_pm_policy structure for the demand power policy.
 *
 * This is the static structure that defines the demand power policy's callback and name.
 */
const kbase_pm_policy kbase_pm_fast_start_policy_ops = {
	"fast_start",			/* name */
	fast_start_init,		/* init */
	fast_start_term,		/* term */
	fast_start_get_core_mask,	/* get_core_mask */
	fast_start_get_core_active,	/* get_core_active */
	0u,				/* flags */
	KBASE_PM_POLICY_ID_FAST_START,	/* id */
};

KBASE_EXPORT_TEST_API(kbase_pm_fast_start_policy_ops)
