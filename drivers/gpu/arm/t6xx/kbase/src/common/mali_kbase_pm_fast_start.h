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
 * @file mali_kbase_pm_fast_start.h
 * A 'fast start' demand based power policy
 */

#ifndef MALI_KBASE_PM_FAST_START_H
#define MALI_KBASE_PM_FAST_START_H

/**
 * The fast start power management policy has the following characteristics:
 * - When KBase indicates that the GPU will be powered up, but we don't yet
 *   know which Job Chains are to be run:
 *  - All Shader Cores are powered up, regardless of whether or not they will
 *    be needed later.
 * - When KBase indicates that a set of Shader Cores are needed to submit the
 *   currently queued Job Chains:
 *  - Only those Shader Cores are powered up
 * - When KBase indicates that the GPU need not be powered:
 *  - The Shader Cores are powered off, and the GPU itself is powered off too.
 *
 * @note:
 * - KBase indicates the GPU will be powered up when it has a User Process that
 *   has just started to submit Job Chains.
 * - KBase indicates the GPU need not be powered when all the Job Chains from
 *   User Processes have finished, and it is waiting for a User Process to
 *   submit some more Job Chains.
 */

typedef struct kbasep_pm_policy_fast_start {
	/* Set to MALI_TRUE when powering on the GPU, in order to force all
	 * shader cores on. Set to MALI_FALSE otherwise. */
	mali_bool fast_start;
	/* Set if there was an active context on the device when 
	   fast_start_get_core_active was last called. */
	mali_bool active_state;
} kbasep_pm_policy_fast_start;

#endif 				/* MALI_KBASE_PM_FAST_START_H */

