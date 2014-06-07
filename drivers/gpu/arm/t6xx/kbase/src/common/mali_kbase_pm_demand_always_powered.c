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
 * @file mali_kbase_pm_demand_always_powered.c
 * A demand based power management policy which keeps overall GPU power on at 
 * all times, but turns off individual shader cores when not required.
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_pm.h>

static u64 demand_always_powered_get_core_mask(struct kbase_device *kbdev)
{
	u64 desired = kbdev->shader_needed_bitmap | kbdev->shader_inuse_bitmap;

	if (0 == kbdev->pm.active_count)
		return 0;

	return desired;
}

static mali_bool demand_always_powered_get_core_active(struct kbase_device *kbdev)
{
	return MALI_TRUE;
}

static void demand_always_powered_init(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

static void demand_always_powered_term(struct kbase_device *kbdev)
{
	CSTD_UNUSED(kbdev);
}

/** The @ref kbase_pm_policy structure for the demand power policy.
 *
 * This is the static structure that defines the demand power policy's callback and name.
 */
const kbase_pm_policy kbase_pm_demand_always_powered_policy_ops = {
	"demand_always_powered",			/* name */
	demand_always_powered_init,			/* init */
	demand_always_powered_term,			/* term */
	demand_always_powered_get_core_mask,		/* get_core_mask */
	demand_always_powered_get_core_active,		/* get_core_active */
	0u,						/* flags */
	KBASE_PM_POLICY_ID_DEMAND_ALWAYS_POWERED,	/* id */
};

KBASE_EXPORT_TEST_API(kbase_pm_demand_always_powered_policy_ops)
