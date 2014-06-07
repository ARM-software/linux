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

#ifndef _DRIVERS_CPU_LOAD_METRIC_H
#define _DRIVERS_CPU_LOAD_METRIC_H

#include <linux/cpufreq.h>
#include <linux/cpumask.h>

struct cluster_stats
{
	int util;
	int utils[NR_CPUS];
	int freq;
	cpumask_var_t mask;
};

void update_cpu_metric(int cpu, u64 now, u64 delta_idle, u64 delta_time, struct cpufreq_policy *policy);

void cpu_load_metric_get(int *load, int *freq);
void get_cluster_stats(struct cluster_stats *clstats);

#endif /* _DRIVERS_CPU_LOAD_METRIC_H */
