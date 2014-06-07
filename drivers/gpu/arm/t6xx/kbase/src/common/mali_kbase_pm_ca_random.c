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
 * @file mali_kbase_pm_ca_random.c
 * A core availability policy implementing random core rotation.
 *
 * This policy periodically selects a new core mask at random. This new mask is
 * applied in two stages. It initially powers off all undesired cores, by
 * removing them from the core availability mask. Once it is confirmed that
 * these cores are powered off, then the desired cores are powered on.
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_pm.h>
#include <kbase/src/common/mali_kbase_pm_ca_random_test.h>
#include <linux/random.h>

STATIC void random_timer_callback(unsigned long cb)
{
	struct kbase_device *kbdev = (struct kbase_device *) cb;
	struct kbasep_pm_ca_policy_random *data = &kbdev->pm.ca_policy_data.random;
	unsigned long flags;

	spin_lock_irqsave(&kbdev->pm.power_change_lock, flags);

	/* Select new core mask, ensuring that core group 0 is not powered off */
	do {
		data->cores_desired = random32() & kbdev->shader_present_bitmap;
	} while (!(data->cores_desired & kbdev->gpu_props.props.coherency_info.group[0].core_mask));

	/* Disable any cores that are now unwanted */
	data->cores_enabled &= data->cores_desired;

	kbdev->pm.ca_in_transition = MALI_TRUE;

	/* If there are no cores to be powered off then power on desired cores */
	if (!(data->cores_used & ~data->cores_desired)) {
		data->cores_enabled = data->cores_desired;
		kbdev->pm.ca_in_transition = MALI_FALSE;
	}

	data->core_change_timer.expires = jiffies + 5 * HZ;

	add_timer(&data->core_change_timer);

	kbase_pm_update_cores_state_nolock(kbdev);

	spin_unlock_irqrestore(&kbdev->pm.power_change_lock, flags);

	KBASE_DEBUG_PRINT_INFO(KBASE_PM, "Random policy : new core mask=%llX %llX\n", data->cores_desired, data->cores_enabled);
}

KBASE_EXPORT_TEST_API(random_timer_callback);

static void random_init(struct kbase_device *kbdev)
{
	struct kbasep_pm_ca_policy_random *data = &kbdev->pm.ca_policy_data.random;

	data->cores_enabled = kbdev->shader_present_bitmap;
	data->cores_desired = kbdev->shader_present_bitmap;
	data->cores_used = 0;
	kbdev->pm.ca_in_transition = MALI_FALSE;

	init_timer(&data->core_change_timer);

	data->core_change_timer.function = random_timer_callback;
	data->core_change_timer.data = (unsigned long) kbdev;
	data->core_change_timer.expires = jiffies + 5 * HZ;

	add_timer(&data->core_change_timer);
}

static void random_term(struct kbase_device *kbdev)
{
	struct kbasep_pm_ca_policy_random *data = &kbdev->pm.ca_policy_data.random;

	del_timer(&data->core_change_timer);
}

static u64 random_get_core_mask(struct kbase_device *kbdev)
{
	return kbdev->pm.ca_policy_data.random.cores_enabled;
}

static void random_update_core_status(struct kbase_device *kbdev, u64 cores_ready, u64 cores_transitioning)
{
	struct kbasep_pm_ca_policy_random *data = &kbdev->pm.ca_policy_data.random;

	lockdep_assert_held(&kbdev->pm.power_change_lock);

	data->cores_used = cores_ready | cores_transitioning;

	/* If in desired state then clear transition flag */
	if (data->cores_enabled == data->cores_desired)
		kbdev->pm.ca_in_transition = MALI_FALSE;

	/* If all undesired cores are now off then power on desired cores.
	 * The direct comparison against cores_enabled limits potential 
	 * recursion to one level */
	if (!(data->cores_used & ~data->cores_desired) && data->cores_enabled != data->cores_desired) {
		data->cores_enabled = data->cores_desired;

		kbase_pm_update_cores_state_nolock(kbdev);

		kbdev->pm.ca_in_transition = MALI_FALSE;
	}
}

/** The @ref kbase_pm_ca_policy structure for the random core availability
 * policy.
 *
 * This is the static structure that defines the random core availability power
 * policy's callback and name.
 */
const kbase_pm_ca_policy kbase_pm_ca_random_policy_ops = {
	"random",			/* name */
	random_init,			/* init */
	random_term,			/* term */
	random_get_core_mask,		/* get_core_mask */
	random_update_core_status,	/* update_core_status */
	0u,				/* flags */
	KBASE_PM_CA_POLICY_ID_RANDOM,	/* id */
};

KBASE_EXPORT_TEST_API(kbase_pm_ca_random_policy_ops)
