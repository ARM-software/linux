/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Jiyun Kim(jiyun83.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>

#include "exynos5422_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(111000)
#define DEVFREQ_POLLING_PERIOD	(100)

#define DISP_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	37500
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

#ifdef DEVFREQ_ISP_DEBUG
void print_clk(struct clk *p, unsigned long set_freq)
{
	printk("%s, set %ld, get %ld\n", p->name, set_freq, clk_get_rate(p)/1000);
}
#endif

enum devfreq_isp_idx {
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV5,
	LV6,
	LV_COUNT,
};

enum devfreq_isp_clk {
	DOUT_ACLK_333_432_GSCL,
	DOUT_ACLK_432_CAM,

	DOUT_ACLK_550_CAM,
	DOUT_ACLK_FL1_550_CAM,
	DOUT_GSCL_WRAP_A,
	DOUT_GSCL_WRAP_B,

	MOUT_IPLL_CTRL,
	MOUT_ACLK_333_432_GSCL,
	MOUT_ACLK_333_432_GSCL_SW,
	MOUT_ACLK_333_432_GSCL_USER,
	MOUT_ACLK_432_CAM,
	MOUT_ACLK_432_CAM_SW,
	MOUT_ACLK_432_CAM_USER,

	MOUT_MPLL_CTRL,
	MOUT_ACLK_550_CAM,
	MOUT_ACLK_550_CAM_SW,
	MOUT_ACLK_550_CAM_USER,
	MOUT_ACLK_FL1_550_CAM,
	MOUT_ACLK_FL1_550_CAM_SW,
	MOUT_ACLK_FL1_550_CAM_USER,
	MOUT_GSCL_WRAP_A,
	MOUT_GSCL_WRAP_B,
	CLK_COUNT,
};

struct devfreq_data_isp {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_isp;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	struct notifier_block tmu_notifier;
};

struct devfreq_clk_list devfreq_isp_clk[CLK_COUNT] = {
	{"dout_aclk_333_432_gscl",},
	{"dout_aclk_432_cam",},
	{"dout_aclk_550_cam",},
	{"dout_aclk_fl1_550_cam",},
	{"dout_gscl_wrap_a",},
	{"dout_gscl_wrap_b",},
	{"mout_ipll_ctrl",},
	{"mout_aclk_333_432_gscl",},
	{"mout_aclk_333_432_gscl_sw",},
	{"mout_aclk_333_432_gscl_user",},
	{"mout_aclk_432_cam",},
	{"mout_aclk_432_cam_sw",},
	{"mout_aclk_432_cam_user",},
	{"mout_mpll_ctrl",},
	{"mout_aclk_550_cam",},
	{"mout_aclk_550_cam_sw",},
	{"mout_aclk_550_cam_user",},
	{"mout_aclk_fl1_550_cam",},
	{"mout_aclk_fl1_550_cam_sw",},
	{"mout_aclk_fl1_550_cam_user",},
	{"mout_gscl_wrap_a",},
	{"mout_gscl_wrap_b",},
};

struct devfreq_opp_table devfreq_isp_opp_list[] = {
	{LV0,	666000,	1015000},
	{LV1,	555000,	1015000},
	{LV2,	444000,	1015000},
	{LV3,	333000, 1015000},
	{LV4,	222000,	1015000},
	{LV5,	111000,	 850000},
	{LV6,	100000,	 850000},
};

struct devfreq_clk_state mux_aclk_333_432_gscl[] = {
	{MOUT_ACLK_333_432_GSCL, MOUT_IPLL_CTRL},
	{MOUT_ACLK_333_432_GSCL_SW, DOUT_ACLK_333_432_GSCL},
	{MOUT_ACLK_333_432_GSCL_USER, MOUT_ACLK_333_432_GSCL_SW},
};

struct devfreq_clk_state mux_aclk_432_cam[] = {
	{MOUT_ACLK_432_CAM, MOUT_IPLL_CTRL},
	{MOUT_ACLK_432_CAM_SW, DOUT_ACLK_432_CAM},
	{MOUT_ACLK_432_CAM_USER, MOUT_ACLK_432_CAM_SW},
};

struct devfreq_clk_state mux_aclk_550_cam[] = {
	{MOUT_ACLK_550_CAM, MOUT_MPLL_CTRL},
	{MOUT_ACLK_550_CAM_SW, DOUT_ACLK_550_CAM},
	{MOUT_ACLK_550_CAM_USER, MOUT_ACLK_550_CAM_SW},
};

struct devfreq_clk_state mux_aclk_fl1_550_cam[] = {
	{MOUT_ACLK_FL1_550_CAM, MOUT_MPLL_CTRL},
	{MOUT_ACLK_FL1_550_CAM_SW, DOUT_ACLK_FL1_550_CAM},
	{MOUT_ACLK_FL1_550_CAM_USER, MOUT_ACLK_FL1_550_CAM_SW},
};

struct devfreq_clk_state mux_gscl_wrap_a[] = {
	{MOUT_GSCL_WRAP_A, MOUT_MPLL_CTRL},
};

struct devfreq_clk_state mux_gscl_wrap_b[] = {
	{MOUT_GSCL_WRAP_B, MOUT_MPLL_CTRL},
};

struct devfreq_clk_states aclk_333_432_gscl_list = {
	.state = mux_aclk_333_432_gscl,
	.state_count = ARRAY_SIZE(mux_aclk_333_432_gscl),
};

struct devfreq_clk_states aclk_432_cam_list = {
	.state = mux_aclk_432_cam,
	.state_count = ARRAY_SIZE(mux_aclk_432_cam),
};

struct devfreq_clk_states aclk_550_cam_list = {
	.state = mux_aclk_550_cam,
	.state_count = ARRAY_SIZE(mux_aclk_550_cam),
};

struct devfreq_clk_states aclk_fl1_550_cam_list = {
	.state = mux_aclk_fl1_550_cam,
	.state_count = ARRAY_SIZE(mux_aclk_fl1_550_cam),
};

struct devfreq_clk_states sclk_gscl_wrap_a_list = {
	.state = mux_gscl_wrap_a,
	.state_count = ARRAY_SIZE(mux_gscl_wrap_a),
};

struct devfreq_clk_states sclk_gscl_wrap_b_list = {
	.state = mux_gscl_wrap_b,
	.state_count = ARRAY_SIZE(mux_gscl_wrap_b),
};

struct devfreq_clk_info aclk_333_432_gscl[] = {
	{LV0,	432000,	0,	&aclk_333_432_gscl_list},
	{LV1,	432000,	0,	&aclk_333_432_gscl_list},
	{LV2,	432000,	0,	&aclk_333_432_gscl_list},
	{LV3,	432000,	0,	&aclk_333_432_gscl_list},
	{LV4,	432000,	0,	&aclk_333_432_gscl_list},
	{LV5,	432000,	0,	&aclk_333_432_gscl_list},
	{LV6,	432000,	0,	&aclk_333_432_gscl_list},
};

struct devfreq_clk_info aclk_432_cam[] = {
	{LV0,	432000,	0,	&aclk_432_cam_list},
	{LV1,	216000,	0,	&aclk_432_cam_list},
	{LV2,	 54000,	0,	&aclk_432_cam_list},
	{LV3,	144000,	0,	&aclk_432_cam_list},
	{LV4,	 54000,	0,	&aclk_432_cam_list},
	{LV5,	 54000,	0,	&aclk_432_cam_list},
	{LV6,	 54000,	0,	&aclk_432_cam_list},
};

struct devfreq_clk_info aclk_550_cam[] = {
	{LV0,	532000,	0,	&aclk_550_cam_list},
	{LV1,	532000,	0,	&aclk_550_cam_list},
	{LV2,	532000,	0,	&aclk_550_cam_list},
	{LV3,	266000,	0,	&aclk_550_cam_list},
	{LV4,	266000,	0,	&aclk_550_cam_list},
	{LV5,	266000,	0,	&aclk_550_cam_list},
	{LV6,	133000,	0,	&aclk_550_cam_list},
};

struct devfreq_clk_info aclk_fl1_550_cam[] = {
	{LV0,	 76000,	0,	&aclk_fl1_550_cam_list},
	{LV1,	 76000,	0,	&aclk_fl1_550_cam_list},
	{LV2,	 67000,	0,	&aclk_fl1_550_cam_list},
	{LV3,	 76000,	0,	&aclk_fl1_550_cam_list},
	{LV4,	 67000,	0,	&aclk_fl1_550_cam_list},
	{LV5,	 67000,	0,	&aclk_fl1_550_cam_list},
	{LV6,	 67000,	0,	&aclk_fl1_550_cam_list},
};

struct devfreq_clk_info sclk_gscl_wrap_a[] = {
	{LV0,	532000,	0,	&sclk_gscl_wrap_a_list},
	{LV1,	532000,	0,	&sclk_gscl_wrap_a_list},
	{LV2,	532000,	0,	&sclk_gscl_wrap_a_list},
	{LV3,	532000,	0,	&sclk_gscl_wrap_a_list},
	{LV4,	532000,	0,	&sclk_gscl_wrap_a_list},
	{LV5,	266000,	0,	&sclk_gscl_wrap_a_list},
	{LV6,	133000,	0,	&sclk_gscl_wrap_a_list},
};

struct devfreq_clk_info sclk_gscl_wrap_b[] = {
	{LV0,	 76000,	0,	&sclk_gscl_wrap_b_list},
	{LV1,	 76000,	0,	&sclk_gscl_wrap_b_list},
	{LV2,	 67000,	0,	&sclk_gscl_wrap_b_list},
	{LV3,	 76000,	0,	&sclk_gscl_wrap_b_list},
	{LV4,	 67000,	0,	&sclk_gscl_wrap_b_list},
	{LV5,	 67000,	0,	&sclk_gscl_wrap_b_list},
	{LV6,	 67000,	0,	&sclk_gscl_wrap_b_list},
};

struct devfreq_clk_info *devfreq_clk_isp_info_list[] = {
	aclk_333_432_gscl,
	aclk_432_cam,
	aclk_550_cam,
	aclk_fl1_550_cam,
	sclk_gscl_wrap_a,
	sclk_gscl_wrap_b,
};

enum devfreq_isp_clk devfreq_clk_isp_info_idx[] = {
	DOUT_ACLK_333_432_GSCL,
	DOUT_ACLK_432_CAM,
	DOUT_ACLK_550_CAM,
	DOUT_ACLK_FL1_550_CAM,
	DOUT_GSCL_WRAP_A,
	DOUT_GSCL_WRAP_B,
};

static struct devfreq_simple_ondemand_data exynos5_devfreq_isp_governor_data = {
	.pm_qos_class		= PM_QOS_CAM_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 666000,
};

static struct exynos_devfreq_platdata exynos5422_qos_isp = {
	.default_qos		= 111000,
};

static struct pm_qos_request exynos5_isp_qos;
static struct pm_qos_request boot_isp_qos;
static struct pm_qos_request min_isp_thermal_qos;

static inline int exynos5_devfreq_isp_get_idx(struct devfreq_opp_table *table,
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

static int exynos5_devfreq_isp_set_freq(struct devfreq_data_isp *data,
					int target_idx,
					int old_idx)
{
	int i, j;
	struct devfreq_clk_info *clk_info;
	struct devfreq_clk_states *clk_states;

	if (target_idx < old_idx) {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); ++i) {
			clk_info = &devfreq_clk_isp_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq * 1000);
#ifdef DEVFREQ_ISP_DEBUG
				print_clk(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
#endif
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_isp_info_list); ++i) {
			clk_info = &devfreq_clk_isp_info_list[i][target_idx];
			clk_states = clk_info->states;

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq * 1000);
#ifdef DEVFREQ_ISP_DEBUG
				print_clk(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
#endif
			}

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_isp_clk[clk_states->state[j].clk_idx].clk,
						devfreq_isp_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0) {
				clk_set_rate(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq * 1000);
#ifdef DEVFREQ_ISP_DEBUG
				print_clk(devfreq_isp_clk[devfreq_clk_isp_info_idx[i]].clk, clk_info->freq);
#endif
			}
		}
	}

	return 0;
}

static int exynos5_devfreq_isp_set_volt(struct devfreq_data_isp *data,
		unsigned long volt,
		unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_isp, volt, volt_range);
	data->old_volt = volt;
out:
	return 0;
}

#ifdef CONFIG_EXYNOS_THERMAL
static unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	if (volt_offset && (voltage + volt_offset < MIN_COLD_VOLTAGE))
		return MIN_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

static int exynos5_devfreq_isp_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_isp *isp_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_isp = isp_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&isp_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(ISP) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, isp_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_isp_get_idx(devfreq_isp_opp_list,
						ARRAY_SIZE(devfreq_isp_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_isp_get_idx(devfreq_isp_opp_list,
						ARRAY_SIZE(devfreq_isp_opp_list),
						devfreq_isp->previous_freq);
	old_freq = devfreq_isp->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_devfreq_isp_set_volt(isp_data, target_volt, target_volt + VOLT_STEP);
		exynos5_devfreq_isp_set_freq(isp_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_isp_set_freq(isp_data, target_idx, old_idx);
		exynos5_devfreq_isp_set_volt(isp_data, target_volt, target_volt + VOLT_STEP);
	}
out:
	mutex_unlock(&isp_data->lock);

	return ret;
}

static int exynos5_devfreq_isp_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_isp *data = dev_get_drvdata(dev);

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = 0;
	stat->total_time = 1;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_isp_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_isp_target,
	.get_dev_status	= exynos5_devfreq_isp_get_dev_status,
	.max_state	= LV_COUNT,
};

static int exynos5_devfreq_isp_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_clk); ++i) {
		devfreq_isp_clk[i].clk = __clk_lookup(devfreq_isp_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_isp_clk[i].clk)) {
			pr_err("DEVFREQ(ISP) : %s can't get clock\n", devfreq_isp_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}

static int exynos5_init_isp_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < ARRAY_SIZE(devfreq_isp_opp_list); ++i) {
		freq = devfreq_isp_opp_list[i].freq;
		volt = get_match_volt(ID_ISP, freq);

		if (!volt)
			volt = devfreq_isp_opp_list[i].volt;

		exynos5_devfreq_isp_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(ISP) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(ISP) : %uKhz, %uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos5_devfreq_isp_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_isp_qos, exynos5_devfreq_isp_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_isp_reboot_notifier = {
	.notifier_call = exynos5_devfreq_isp_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_isp_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_isp *data = container_of(nb, struct devfreq_data_isp, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_isp_thermal_qos))
			pm_qos_update_request(&min_isp_thermal_qos,
					exynos5_devfreq_isp_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_isp);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_isp, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_isp);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_isp, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_isp_thermal_qos))
			pm_qos_update_request(&min_isp_thermal_qos,
					exynos5422_qos_isp.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_isp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_isp *data;
	struct exynos_devfreq_platdata *plat_data;

	if (exynos5_devfreq_isp_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

	data = kzalloc(sizeof(struct devfreq_data_isp), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5_devfreq_isp_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_isp_profile.freq_table == NULL) {
		pr_err("DEVFREQ(ISP) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_isp_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	data->vdd_isp = regulator_get(NULL, "vdd_cam_isp_1.0v");
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_isp_profile,
						"simple_ondemand",
						&exynos5_devfreq_isp_governor_data);
	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_isp_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_isp_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_isp_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	return ret;
err_inittable:
	devfreq_remove_device(data->devfreq);
	kfree(exynos5_devfreq_isp_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_isp_remove(struct platform_device *pdev)
{
	struct devfreq_data_isp *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_isp_thermal_qos);
	pm_qos_remove_request(&exynos5_isp_qos);
	pm_qos_remove_request(&boot_isp_qos);

	regulator_put(data->vdd_isp);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_isp_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_isp_qos))
		pm_qos_update_request(&exynos5_isp_qos, exynos5_devfreq_isp_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_isp_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_isp_qos))
		pm_qos_update_request(&exynos5_isp_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_isp_pm = {
	.suspend	= exynos5_devfreq_isp_suspend,
	.resume		= exynos5_devfreq_isp_resume,
};

static struct platform_driver exynos5_devfreq_isp_driver = {
	.probe	= exynos5_devfreq_isp_probe,
	.remove	= exynos5_devfreq_isp_remove,
	.driver	= {
		.name	= "exynos5-devfreq-isp",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_isp_pm,
	},
};

static struct platform_device exynos5_devfreq_isp_device = {
	.name	= "exynos5-devfreq-isp",
	.id	= -1,
};

static int exynos5_devfreq_isp_qos_init(void)
{
	pm_qos_add_request(&exynos5_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos5422_qos_isp.default_qos);
	pm_qos_add_request(&min_isp_thermal_qos, PM_QOS_CAM_THROUGHPUT, exynos5422_qos_isp.default_qos);
	pm_qos_add_request(&boot_isp_qos, PM_QOS_CAM_THROUGHPUT, exynos5422_qos_isp.default_qos);
	return 0;
}
device_initcall(exynos5_devfreq_isp_qos_init);

static int __init exynos5_devfreq_isp_init(void)
{
	int ret;

	exynos5_devfreq_isp_device.dev.platform_data = &exynos5422_qos_isp;

	ret = platform_device_register(&exynos5_devfreq_isp_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_isp_driver);
}
late_initcall(exynos5_devfreq_isp_init);

static void __exit exynos5_devfreq_isp_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_isp_driver);
	platform_device_unregister(&exynos5_devfreq_isp_device);
}
module_exit(exynos5_devfreq_isp_exit);
#endif
