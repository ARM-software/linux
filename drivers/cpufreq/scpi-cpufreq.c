/*
 * SCPI CPUFreq Interface driver
 *
 * It provides necessary ops to arm_big_little cpufreq driver.
 *
 * Copyright (C) 2014 ARM Ltd.
 * Sudeep Holla <sudeep.holla@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/scpi_protocol.h>
#include <linux/types.h>

#include "arm_big_little.h"

static int scpi_init_opp_table(struct device *cpu_dev)
{
	u8 domain = topology_physical_package_id(cpu_dev->id);
	struct scpi_opp *opp;
	int idx, ret = 0, max_opp;
	u32 *freqs;

	opp = scpi_dvfs_get_opps(domain);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	freqs = opp->freqs;
	max_opp = opp->count;
	for (idx = 0; idx < max_opp; idx++, freqs++) {
		ret = dev_pm_opp_add(cpu_dev, *freqs, 900000000 /* TODO */);
		if (ret) {
			dev_warn(cpu_dev, "failed to add opp %u\n", *freqs);
			return ret;
		}
	}
	return ret;
}

static int scpi_get_transition_latency(struct device *cpu_dev)
{
	u8 domain = topology_physical_package_id(cpu_dev->id);
	struct scpi_opp *opp;

	opp = scpi_dvfs_get_opps(domain);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	return opp->latency * 1000; /* SCPI returns in uS */
}

static struct cpufreq_arm_bL_ops scpi_cpufreq_ops = {
	.name	= "scpi",
	.get_transition_latency = scpi_get_transition_latency,
	.init_opp_table = scpi_init_opp_table,
};

static int scpi_cpufreq_probe(struct platform_device *pdev)
{
	return bL_cpufreq_register(&scpi_cpufreq_ops);
}

static int scpi_cpufreq_remove(struct platform_device *pdev)
{
	bL_cpufreq_unregister(&scpi_cpufreq_ops);
	return 0;
}

static struct of_device_id scpi_cpufreq_of_match[] = {
	{ .compatible = "arm,scpi-cpufreq" },
	{},
};
MODULE_DEVICE_TABLE(of, scpi_cpufreq_of_match);

static struct platform_driver scpi_cpufreq_platdrv = {
	.driver = {
		.name	= "scpi-cpufreq",
		.owner	= THIS_MODULE,
		.of_match_table = scpi_cpufreq_of_match,
	},
	.probe		= scpi_cpufreq_probe,
	.remove		= scpi_cpufreq_remove,
};
module_platform_driver(scpi_cpufreq_platdrv);

MODULE_LICENSE("GPL");
