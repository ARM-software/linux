/*
 *  ARM Intelligent Power Allocation
 *
 *  Copyright (C) 2013 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DRIVERS_THERMAL_IPA_H
#define _DRIVERS_THERMAL_IPA_H

#include <linux/cpufreq.h>

#define THERMAL_NEW_MAX_FREQ 0

struct ipa_sensor_conf {
	int (*read_soc_temperature)(void *data);
	int (*read_skin_temperature)(void);
	void *private_data;
};

struct thermal_limits {
	int max_freq;
	int cur_freq;
	cpumask_t cpus;
};

#ifdef CONFIG_CPU_THERMAL_IPA

void check_switch_ipa_on(int temp);
void ipa_cpufreq_requested(struct cpufreq_policy *p, unsigned int *freqs);
int ipa_register_thermal_sensor(struct ipa_sensor_conf *);
int thermal_register_notifier(struct notifier_block *nb);
int thermal_unregister_notifier(struct notifier_block *nb);

#else

static inline void check_switch_ipa_on(int t)
{
}

static void __maybe_unused ipa_cpufreq_requested(struct cpufreq_policy *p, unsigned int *freqs)
{
}

static inline int ipa_register_thermal_sensor(struct ipa_sensor_conf *p)
{
	return 0;
}

static inline int thermal_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int thermal_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}

#endif

#endif /* _DRIVERS_THERMAL_IPA_H */
