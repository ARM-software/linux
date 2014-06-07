/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *              Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS_H
#define __DEVFREQ_EXYNOS_H __FILE__

#include <linux/clk.h>

#include <mach/pm_domains.h>

#define VOLT_STEP	(12500)

struct devfreq_opp_table {
	unsigned int idx;
	unsigned long freq;
	unsigned long volt;
};

struct devfreq_clk_state {
	int clk_idx;
	int parent_clk_idx;
};

struct devfreq_clk_states {
	struct devfreq_clk_state *state;
	unsigned int state_count;
};

struct devfreq_clk_value {
	unsigned int reg;
	unsigned int set_value;
	unsigned int clr_value;
};

struct devfreq_clk_info {
	unsigned int idx;
	unsigned long freq;
	int pll;
	struct devfreq_clk_states *states;
};

struct devfreq_clk_list {
	const char *clk_name;
	struct clk *clk;
};

struct exynos_devfreq_platdata {
	unsigned long default_qos;
};

struct devfreq_info {
	unsigned int old;
	unsigned int new;
};

struct devfreq_pm_domain_link {
	const char *pm_domain_name;
	struct exynos_pm_domain *pm_domain;
};

struct devfreq_dynamic_clkgate {
	unsigned long	paddr;
	unsigned long	vaddr;
	unsigned int	bit;
	unsigned long	freq;
};

#endif /* __DEVFREQ_EXYNOS_H */
