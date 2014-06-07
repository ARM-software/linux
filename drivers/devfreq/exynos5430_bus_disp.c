#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <mach/asv-exynos.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(317000)
#define DEVFREQ_POLLING_PERIOD	(100)

enum devfreq_disp_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV_COUNT,
};

enum devfreq_disp_clk {
	DOUT_ACLK_DISP_333,
	DOUT_SCLK_DSD,
	CLK_COUNT,
};

struct devfreq_data_disp {
	struct device *dev;
	struct devfreq *devfreq;

	struct mutex lock;

	unsigned int use_dvfs;
};

struct devfreq_clk_list devfreq_disp_clk[CLK_COUNT] = {
	{"dout_aclk_disp_333",},
	{"dout_sclk_dsd",},
};

struct devfreq_opp_table devfreq_disp_opp_list[] = {
	{LV0,	317000,	0},
	{LV1,	211000,	0},
	{LV2,	159000, 0},
	{LV3,	127000,	0},
};

struct devfreq_clk_info aclk_disp_333[] = {
	{LV0,	317000000,	0,	NULL},
	{LV1,	211000000,	0,	NULL},
	{LV2,	159000000,	0,	NULL},
	{LV3,	127000000,	0,	NULL},
};

struct devfreq_clk_info sclk_dsd[] = {
	{LV0,	317000000,	0,	NULL},
	{LV1,	317000000,	0,	NULL},
	{LV2,	317000000,	0,	NULL},
	{LV3,	317000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_disp_info_list[] = {
	aclk_disp_333,
	sclk_dsd,
};

enum devfreq_disp_clk devfreq_clk_disp_info_idx[] = {
	DOUT_ACLK_DISP_333,
	DOUT_SCLK_DSD,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_disp_pm_domain[] = {
	{"pd-disp",},
	{"pd-disp",},
};
#endif

static struct devfreq_simple_ondemand_data exynos5_devfreq_disp_governor_data = {
	.pm_qos_class		= PM_QOS_DISPLAY_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 317000,
};

static struct exynos_devfreq_platdata exynos5430_qos_disp = {
	.default_qos		= 127000,
};

static struct ppmu_info ppmu_disp[] = {
	{
		.base = (void __iomem *)PPMU_D0_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_RT_ADDR,
	},
};

static struct devfreq_exynos devfreq_disp_exynos = {
	.ppmu_list = ppmu_disp,
	.ppmu_count = ARRAY_SIZE(ppmu_disp),
};

static struct pm_qos_request exynos5_disp_qos;
static struct pm_qos_request boot_disp_qos;
static struct pm_qos_request min_disp_thermal_qos;
static struct pm_qos_request exynos5_disp_bts_qos;
static struct devfreq_data_disp *data_disp;

extern void exynos5_update_district_int_level(int aclk_disp_333_freq);

void exynos5_update_district_disp_level(unsigned int idx)
{
	if (!pm_qos_request_active(&exynos5_disp_bts_qos))
		return;

	if (idx != LV3)
		pm_qos_update_request(&exynos5_disp_bts_qos, devfreq_disp_opp_list[idx].freq);
	else
		pm_qos_update_request(&exynos5_disp_bts_qos, exynos5430_qos_disp.default_qos);
}

static inline int exynos5_devfreq_disp_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; ++i) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

static int exynos5_devfreq_disp_set_clk(struct devfreq_data_disp *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info->states;

	if (clk_get_rate(clk) < clk_info->freq) {
		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_disp_clk[clk_states->state[i].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info->freq != 0)
			clk_set_rate(clk, clk_info->freq);
	} else {
		if (clk_info->freq != 0)
			clk_set_rate(clk, clk_info->freq);

		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_disp_clk[clk_states->state[i].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info->freq != 0)
			clk_set_rate(clk, clk_info->freq);
	}

	return 0;
}

void exynos5_disp_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on ||
		!data_disp->use_dvfs)
		return;

	mutex_lock(&data_disp->lock);
	cur_freq_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
			ARRAY_SIZE(devfreq_disp_opp_list),
			data_disp->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_disp->lock);
		pr_err("DEVFREQ(INT) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_pm_domain); ++i) {
		if (devfreq_disp_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_disp_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos5_devfreq_disp_set_clk(data_disp,
				cur_freq_idx,
				devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk,
				devfreq_clk_disp_info_list[i]);
	}
	mutex_unlock(&data_disp->lock);
}

static int exynos5_devfreq_disp_set_freq(struct devfreq_data_disp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;
#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_disp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;
#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_disp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_disp *disp_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_disp = disp_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long old_freq;

	mutex_lock(&disp_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&disp_data->lock);
		dev_err(dev, "DEVFREQ(DISP) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	rcu_read_unlock();

	target_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
						ARRAY_SIZE(devfreq_disp_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
						ARRAY_SIZE(devfreq_disp_opp_list),
						devfreq_disp->previous_freq);
	old_freq = devfreq_disp->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_update_district_int_level(target_idx);
		exynos5_devfreq_disp_set_freq(disp_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_disp_set_freq(disp_data, target_idx, old_idx);
		exynos5_update_district_int_level(target_idx);
	}
out:
	mutex_unlock(&disp_data->lock);

	return ret;
}

static int exynos5_devfreq_disp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_disp *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	if (ppmu_count_total(devfreq_disp_exynos.ppmu_list,
			devfreq_disp_exynos.ppmu_count,
			&devfreq_disp_exynos.val_ccnt,
			&devfreq_disp_exynos.val_pmcnt)) {
		pr_err("DEVFREQ(DISP) : can't calculate bus status\n");
		return -EINVAL;
	}

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_disp_exynos.val_pmcnt;
	stat->total_time = devfreq_disp_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_disp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_disp_target,
	.get_dev_status	= exynos5_devfreq_disp_get_dev_status,
	.max_state	= LV_COUNT,
};

static int exynos5_devfreq_disp_init_ppmu(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_init(&ppmu_disp[i]))
			return -EINVAL;
	}

	return 0;
}

static int exynos5_devfreq_disp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_clk); ++i) {
		devfreq_disp_clk[i].clk = __clk_lookup(devfreq_disp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_disp_clk[i].clk)) {
			pr_err("DEVFREQ(DISP) : %s can't get clock\n", devfreq_disp_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_disp_init_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	int i;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd = platform_get_drvdata(pdev);

		for (i = 0; i < ARRAY_SIZE(devfreq_disp_pm_domain); ++i) {
			if (devfreq_disp_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_disp_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_disp_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

static int exynos5_init_disp_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_opp_list); ++i) {
		freq = devfreq_disp_opp_list[i].freq;

		exynos5_devfreq_disp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, 0);
		if (ret) {
			pr_err("DEVFREQ(DISP) : Failed to add opp entries %uKhz\n", freq);
			return ret;
		} else {
			pr_info("DEVFREQ(DISP) : %uKhz\n", freq);
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_disp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_disp_reboot_notifier,
};

static int exynos5_devfreq_disp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_disp *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;

	if (exynos5_devfreq_disp_init_ppmu()) {
		ret = -EINVAL;
		goto err_data;
	}

	if (exynos5_devfreq_disp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_disp_init_pm_domain()) {
		ret = -EINVAL;
		goto err_data;
	}
#endif

	data = kzalloc(sizeof(struct devfreq_data_disp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5_devfreq_disp_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_disp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_disp_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	data_disp = data;
	mutex_init(&data->lock);

	data->dev = &pdev->dev;
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_disp_profile,
						"simple_ondemand",
						&exynos5_devfreq_disp_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_disp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_disp_reboot_notifier);

	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(exynos5_devfreq_disp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_disp_remove(struct platform_device *pdev)
{
	struct devfreq_data_disp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_disp_thermal_qos);
	pm_qos_remove_request(&exynos5_disp_qos);
	pm_qos_remove_request(&boot_disp_qos);
	pm_qos_remove_request(&exynos5_disp_bts_qos);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_disp_suspend(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_disable(&ppmu_disp[i]))
			return -EINVAL;
	}

	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_disp_resume(struct device *dev)
{
	int i;
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, pdata->default_qos);

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_reset(&ppmu_disp[i]))
			return -EINVAL;
	}

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_disp_pm = {
	.suspend	= exynos5_devfreq_disp_suspend,
	.resume		= exynos5_devfreq_disp_resume,
};

static struct platform_driver exynos5_devfreq_disp_driver = {
	.probe	= exynos5_devfreq_disp_probe,
	.remove	= exynos5_devfreq_disp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-disp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_disp_pm,
	},
};

static struct platform_device exynos5_devfreq_disp_device = {
	.name	= "exynos5-devfreq-disp",
	.id	= -1,
};

static int __init exynos5_devfreq_disp_qos_init(void)
{
	pm_qos_add_request(&exynos5_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_add_request(&min_disp_thermal_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_add_request(&boot_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_add_request(&exynos5_disp_bts_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_update_request_timeout(&exynos5_disp_qos,
					exynos5_devfreq_disp_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_disp_qos_init);

static int __init exynos5_devfreq_disp_init(void)
{
	int ret;

	exynos5_devfreq_disp_device.dev.platform_data = &exynos5430_qos_disp;

	ret = platform_device_register(&exynos5_devfreq_disp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_disp_driver);
}
late_initcall(exynos5_devfreq_disp_init);

static void __exit exynos5_devfreq_disp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_disp_driver);
	platform_device_unregister(&exynos5_devfreq_disp_device);
}
module_exit(exynos5_devfreq_disp_exit);
#elif defined(CONFIG_SOC_EXYNOS5430_REV_0)
/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <mach/asv-exynos.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(333000)
#define DEVFREQ_POLLING_PERIOD	(100)

enum devfreq_disp_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV_COUNT,
};

enum devfreq_disp_clk {
	DOUT_ACLK_DISP_333,
	DOUT_ACLK_DISP_222,
	DOUT_SCLK_DSD,
	CLK_COUNT,
};

struct devfreq_data_disp {
	struct device *dev;
	struct devfreq *devfreq;
};

struct devfreq_clk_list devfreq_disp_clk[CLK_COUNT] = {
	{"dout_aclk_disp_333",},
	{"dout_aclk_disp_222",},
	{"dout_sclk_dsd",},
};

struct devfreq_opp_table devfreq_disp_opp_list[] = {
	{LV0,	333000,	0},
	{LV1,	222000,	0},
	{LV2,	167000, 0},
	{LV3,	133000,	0},
};

struct devfreq_clk_info aclk_disp_333[] = {
	{LV0,	333000000,	0,	NULL},
	{LV1,	222000000,	0,	NULL},
	{LV2,	167000000,	0,	NULL},
	{LV3,	134000000,	0,	NULL},
};

struct devfreq_clk_info aclk_disp_222[] = {
	{LV0,	222000000,	0,	NULL},
	{LV1,	167000000,	0,	NULL},
	{LV2,	 83000000,	0,	NULL},
	{LV3,	 83000000,	0,	NULL},
};

struct devfreq_clk_info sclk_dsd[] = {
	{LV0,	333000000,	0,	NULL},
	{LV1,	333000000,	0,	NULL},
	{LV2,	333000000,	0,	NULL},
	{LV3,	 84000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_disp_info_list[] = {
	aclk_disp_333,
	aclk_disp_222,
	sclk_dsd,
};

enum devfreq_disp_clk devfreq_clk_disp_info_idx[] = {
	DOUT_ACLK_DISP_333,
	DOUT_ACLK_DISP_222,
	DOUT_SCLK_DSD,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_disp_pm_domain[] = {
	{"pd-disp",},
	{"pd-disp",},
	{"pd-disp",},
};
#endif

static struct devfreq_simple_ondemand_data exynos5_devfreq_disp_governor_data = {
	.pm_qos_class		= PM_QOS_DISPLAY_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 333000,
};

static struct exynos_devfreq_platdata exynos5430_qos_disp = {
	.default_qos		= 133000,
};

static struct ppmu_info ppmu_disp[] = {
	{
		.base = (void __iomem *)PPMU_D0_RT_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_RT_ADDR,
	},
};

static struct devfreq_exynos devfreq_disp_exynos = {
	.ppmu_list = ppmu_disp,
	.ppmu_count = ARRAY_SIZE(ppmu_disp),
};

static struct pm_qos_request exynos5_disp_qos;
static struct pm_qos_request boot_disp_qos;
static struct pm_qos_request min_disp_thermal_qos;
static struct pm_qos_request exynos5_disp_bts_qos;

extern void exynos5_update_district_int_level(int aclk_disp_333_freq);

void exynos5_update_district_disp_level(unsigned int idx)
{
	if (!pm_qos_request_active(&exynos5_disp_bts_qos))
		return;

	if (idx != LV3)
		pm_qos_update_request(&exynos5_disp_bts_qos, devfreq_disp_opp_list[idx].freq);
	else
		pm_qos_update_request(&exynos5_disp_bts_qos, exynos5430_qos_disp.default_qos);
}

static inline int exynos5_devfreq_disp_get_idx(struct devfreq_opp_table *table,
				unsigned int size,
				unsigned long freq)
{
	int i;

	for (i = 0; i < size; ++i) {
		if (table[i].freq == freq)
			return i;
	}

	return -1;
}

static int exynos5_devfreq_disp_set_freq(struct devfreq_data_disp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;
#ifdef CONFIG_PM_RUNTIME
	struct exynos_pm_domain *pm_domain;
#endif

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;
#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_disp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_disp_info_list); ++i) {
			clk_info = &devfreq_clk_disp_info_list[i][target_idx];
			clk_states = clk_info->states;
#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_disp_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif
			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_disp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_disp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_disp_clk[devfreq_clk_disp_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_disp *disp_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_disp = disp_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long old_freq;

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(DISP) : Invalid OPP to find\n");
		return PTR_ERR(target_opp);
	}

	*target_freq = opp_get_freq(target_opp);
	rcu_read_unlock();

	target_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
						ARRAY_SIZE(devfreq_disp_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_disp_get_idx(devfreq_disp_opp_list,
						ARRAY_SIZE(devfreq_disp_opp_list),
						devfreq_disp->previous_freq);
	old_freq = devfreq_disp->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_update_district_int_level(target_idx);
		exynos5_devfreq_disp_set_freq(disp_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_disp_set_freq(disp_data, target_idx, old_idx);
		exynos5_update_district_int_level(target_idx);
	}
out:
	return ret;
}

static int exynos5_devfreq_disp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_disp *data = dev_get_drvdata(dev);

	if (ppmu_count_total(devfreq_disp_exynos.ppmu_list,
			devfreq_disp_exynos.ppmu_count,
			&devfreq_disp_exynos.val_ccnt,
			&devfreq_disp_exynos.val_pmcnt)) {
		pr_err("DEVFREQ(DISP) : can't calculate bus status\n");
		return -EINVAL;
	}

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_disp_exynos.val_pmcnt;
	stat->total_time = devfreq_disp_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_disp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_disp_target,
	.get_dev_status	= exynos5_devfreq_disp_get_dev_status,
	.max_state	= LV_COUNT,
};

static int exynos5_devfreq_disp_init_ppmu(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_init(&ppmu_disp[i]))
			return -EINVAL;
	}

	return 0;
}

static int exynos5_devfreq_disp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_clk); ++i) {
		devfreq_disp_clk[i].clk = __clk_lookup(devfreq_disp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_disp_clk[i].clk)) {
			pr_err("DEVFREQ(DISP) : %s can't get clock\n", devfreq_disp_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_disp_init_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	int i;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd = platform_get_drvdata(pdev);

		for (i = 0; i < ARRAY_SIZE(devfreq_disp_pm_domain); ++i) {
			if (devfreq_disp_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_disp_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_disp_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

static int exynos5_init_disp_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;

	for (i = 0; i < ARRAY_SIZE(devfreq_disp_opp_list); ++i) {
		freq = devfreq_disp_opp_list[i].freq;

		exynos5_devfreq_disp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, 0);
		if (ret) {
			pr_err("DEVFREQ(DISP) : Failed to add opp entries %uKhz\n", freq);
			return ret;
		} else {
			pr_info("DEVFREQ(DISP) : %uKhz\n", freq);
		}
	}

	return 0;
}

static int exynos5_devfreq_disp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_disp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_disp_reboot_notifier,
};

static int exynos5_devfreq_disp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_disp *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;

	if (exynos5_devfreq_disp_init_ppmu()) {
		ret = -EINVAL;
		goto err_data;
	}

	if (exynos5_devfreq_disp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_disp_init_pm_domain()) {
		ret = -EINVAL;
		goto err_data;
	}
#endif

	data = kzalloc(sizeof(struct devfreq_data_disp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5_devfreq_disp_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_disp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_disp_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);

	data->dev = &pdev->dev;
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_disp_profile,
						"simple_ondemand",
						&exynos5_devfreq_disp_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(DISP) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_disp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_disp_reboot_notifier);

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(exynos5_devfreq_disp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_disp_remove(struct platform_device *pdev)
{
	struct devfreq_data_disp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_disp_thermal_qos);
	pm_qos_remove_request(&exynos5_disp_qos);
	pm_qos_remove_request(&boot_disp_qos);
	pm_qos_remove_request(&exynos5_disp_bts_qos);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_disp_suspend(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_disable(&ppmu_disp[i]))
			return -EINVAL;
	}

	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, exynos5_devfreq_disp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_disp_resume(struct device *dev)
{
	int i;
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_disp_qos))
		pm_qos_update_request(&exynos5_disp_qos, pdata->default_qos);

	for (i = 0; i < ARRAY_SIZE(ppmu_disp); ++i) {
		if (ppmu_reset(&ppmu_disp[i]))
			return -EINVAL;
	}

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_disp_pm = {
	.suspend	= exynos5_devfreq_disp_suspend,
	.resume		= exynos5_devfreq_disp_resume,
};

static struct platform_driver exynos5_devfreq_disp_driver = {
	.probe	= exynos5_devfreq_disp_probe,
	.remove	= exynos5_devfreq_disp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-disp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_disp_pm,
	},
};

static struct platform_device exynos5_devfreq_disp_device = {
	.name	= "exynos5-devfreq-disp",
	.id	= -1,
};

static int __init exynos5_devfreq_disp_qos_init(void)
{
	pm_qos_add_request(&exynos5_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_add_request(&min_disp_thermal_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_add_request(&boot_disp_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_add_request(&exynos5_disp_bts_qos, PM_QOS_DISPLAY_THROUGHPUT, exynos5430_qos_disp.default_qos);
	pm_qos_update_request_timeout(&exynos5_disp_qos,
					exynos5_devfreq_disp_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_disp_qos_init);

static int __init exynos5_devfreq_disp_init(void)
{
	int ret;

	exynos5_devfreq_disp_device.dev.platform_data = &exynos5430_qos_disp;

	ret = platform_device_register(&exynos5_devfreq_disp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_disp_driver);
}
late_initcall(exynos5_devfreq_disp_init);

static void __exit exynos5_devfreq_disp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_disp_driver);
	platform_device_unregister(&exynos5_devfreq_disp_device);
}
module_exit(exynos5_devfreq_disp_exit);
#endif
