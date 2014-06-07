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
 * @file mali_kbase_pm_ca_random.h
 * A core availability policy implementing random core rotation
 */

#ifndef MALI_KBASE_PM_CA_RANDOM_H
#define MALI_KBASE_PM_CA_RANDOM_H

/**
 * Private structure for policy instance data.
 *
 * This contains data that is private to the particular core availability 
 * policy that is active.
 */
typedef struct kbasep_pm_ca_policy_random {
	/** Cores that the policy wants to be available */
	u64 cores_desired;
	/** Cores that the policy is currently returning as available */
	u64 cores_enabled;
	/** Cores currently powered or transitioning */
	u64 cores_used;
	/** Timer for changing desired core mask */
	struct timer_list core_change_timer;
} kbasep_pm_ca_policy_random;

#endif /* MALI_KBASE_PM_CA_RANDOM_H */

