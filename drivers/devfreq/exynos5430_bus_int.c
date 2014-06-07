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
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>
#include <mach/regs-clock-exynos5430.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(400000)
#define DEVFREQ_POLLING_PERIOD	(0)

#define INT_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	37500
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

enum devfreq_int_idx {
	LV0_A,
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV5,
	LV6,
	LV_COUNT,
};

enum devfreq_int_clk {
	DOUT_ACLK_BUS1_400,
	DOUT_MIF_PRE,
	DOUT_ACLK_BUS2_400,
	MOUT_BUS_PLL_USER,
	MOUT_MFC_PLL_USER,
	MOUT_ISP_PLL,
	MOUT_MPHY_PLL_USER,
	MOUT_ACLK_G2D_400_A,
	DOUT_ACLK_G2D_400,
	DOUT_ACLK_G2D_266,
	DOUT_ACLK_GSCL_333,
	MOUT_ACLK_MSCL_400_A,
	DOUT_ACLK_MSCL_400,
	MOUT_SCLK_JPEG_A,
	MOUT_SCLK_JPEG_B,
	DOUT_SCLK_JPEG,
	MOUT_ACLK_MFC0_333_A,
	MOUT_ACLK_MFC0_333_B,
	MOUT_ACLK_MFC0_333_C,
	DOUT_ACLK_MFC0_333,
	MOUT_ACLK_MFC1_333_A,
	MOUT_ACLK_MFC1_333_B,
	MOUT_ACLK_MFC1_333_C,
	DOUT_ACLK_MFC1_333,
	DOUT_ACLK_HEVC_400,
	CLK_COUNT,
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	unsigned int use_dvfs;

	struct notifier_block tmu_notifier;
};

struct devfreq_clk_list devfreq_int_clk[CLK_COUNT] = {
	{"dout_aclk_bus1_400",},
	{"dout_mif_pre",},
	{"dout_aclk_bus2_400",},
	{"mout_bus_pll_user",},
	{"mout_mfc_pll_user",},
	{"mout_isp_pll",},
	{"mout_mphy_pll_user",},
	{"mout_aclk_g2d_400_a",},
	{"dout_aclk_g2d_400",},
	{"dout_aclk_g2d_266",},
	{"dout_aclk_gscl_333",},
	{"mout_aclk_mscl_400_a",},
	{"dout_aclk_mscl_400", },
	{"mout_sclk_jpeg_a",},
	{"mout_sclk_jpeg_b",},
	{"dout_sclk_jpeg",},
	{"mout_aclk_mfc0_333_a",},
	{"mout_aclk_mfc0_333_b",},
	{"mout_aclk_mfc0_333_c",},
	{"dout_aclk_mfc0_333",},
	{"mout_aclk_mfc1_333_a",},
	{"mout_aclk_mfc1_333_b",},
	{"mout_aclk_mfc1_333_c",},
	{"dout_aclk_mfc1_333",},
	{"dout_aclk_hevc_400",},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{LV0_A,	543000,	1175000},
	{LV0,	400000,	1075000},
	{LV1,	317000,	1025000},
	{LV2,	267000,	1000000},
	{LV3,	200000,	 975000},
	{LV4,	160000,	 962500},
	{LV5,	133000,	 950000},
	{LV6,	100000,	 937500},
};

struct devfreq_clk_state aclk_g2d_mfc_pll[] = {
	{MOUT_ACLK_G2D_400_A,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state aclk_g2d_bus_pll[] = {
	{MOUT_ACLK_G2D_400_A,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_state aclk_mscl_mfc_pll[] = {
	{MOUT_ACLK_MSCL_400_A,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state aclk_mscl_bus_pll[] = {
	{MOUT_ACLK_MSCL_400_A,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_state sclk_jpeg_mfc_pll[] = {
	{MOUT_SCLK_JPEG_B,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state sclk_jpeg_bus_pll[] = {
	{MOUT_SCLK_JPEG_B,	MOUT_SCLK_JPEG_A},
};

struct devfreq_clk_state aclk_mfc0_333_isp_pll[] = {
	{MOUT_ACLK_MFC0_333_A,	MOUT_ISP_PLL},
	{MOUT_ACLK_MFC0_333_B,	MOUT_ACLK_MFC0_333_A},
	{MOUT_ACLK_MFC0_333_C,	MOUT_ACLK_MFC0_333_B},
};

struct devfreq_clk_state aclk_mfc0_333_mphy_pll[] = {
	{MOUT_ACLK_MFC0_333_C,	MOUT_MPHY_PLL_USER},
};

struct devfreq_clk_state aclk_mfc0_333_mfc_pll[] = {
	{MOUT_ACLK_MFC0_333_A,	MOUT_MFC_PLL_USER},
	{MOUT_ACLK_MFC0_333_B,	MOUT_ACLK_MFC0_333_A},
	{MOUT_ACLK_MFC0_333_C,	MOUT_ACLK_MFC0_333_B},
};

struct devfreq_clk_state aclk_mfc1_333_mfc_pll[] = {
	{MOUT_ACLK_MFC1_333_A,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_states aclk_g2d_mfc_pll_list = {
	.state = aclk_g2d_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_g2d_mfc_pll),
};

struct devfreq_clk_states aclk_g2d_bus_pll_list = {
	.state = aclk_g2d_bus_pll,
	.state_count = ARRAY_SIZE(aclk_g2d_bus_pll),
};

struct devfreq_clk_states aclk_mscl_mfc_pll_list = {
	.state = aclk_mscl_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_mfc_pll),
};

struct devfreq_clk_states aclk_mscl_bus_pll_list = {
	.state = aclk_mscl_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_bus_pll),
};

struct devfreq_clk_states sclk_jpeg_mfc_pll_list = {
	.state = sclk_jpeg_mfc_pll,
	.state_count = ARRAY_SIZE(sclk_jpeg_mfc_pll),
};

struct devfreq_clk_states sclk_jpeg_bus_pll_list = {
	.state = sclk_jpeg_bus_pll,
	.state_count = ARRAY_SIZE(sclk_jpeg_bus_pll),
};

struct devfreq_clk_states aclk_mfc0_333_isp_pll_list = {
	.state = aclk_mfc0_333_isp_pll,
	.state_count = ARRAY_SIZE(aclk_mfc0_333_isp_pll),
};

struct devfreq_clk_states aclk_mfc0_333_mphy_pll_list = {
	.state = aclk_mfc0_333_mphy_pll,
	.state_count = ARRAY_SIZE(aclk_mfc0_333_mphy_pll),
};

struct devfreq_clk_states aclk_mfc0_333_mfc_pll_list = {
	.state = aclk_mfc0_333_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mfc0_333_mfc_pll),
};

struct devfreq_clk_states aclk_mfc1_333_mfc_pll_list = {
	.state = aclk_mfc1_333_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mfc1_333_mfc_pll),
};

struct devfreq_clk_info aclk_bus1_400[] = {
	{LV0_A,	400000000,	0,	NULL},
	{LV0,	400000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	200000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info dout_mif_pre[] = {
	{LV0_A,	800000000,	0,	NULL},
	{LV0,	800000000,	0,	NULL},
	{LV1,	800000000,	0,	NULL},
	{LV2,	800000000,	0,	NULL},
	{LV3,	800000000,	0,	NULL},
	{LV4,	800000000,	0,	NULL},
	{LV5,	800000000,	0,	NULL},
	{LV6,	800000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus2_400[] = {
	{LV0_A,	400000000,	0,	NULL},
	{LV0,	400000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	200000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_g2d_400[] = {
	{LV0_A,	400000000,	0,	&aclk_g2d_bus_pll_list},
	{LV0,	400000000,	0,	&aclk_g2d_bus_pll_list},
	{LV1,	317000000,	0,	&aclk_g2d_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_g2d_bus_pll_list},
	{LV3,	200000000,	0,	&aclk_g2d_bus_pll_list},
	{LV4,	160000000,	0,	&aclk_g2d_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_g2d_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_g2d_bus_pll_list},
};

struct devfreq_clk_info aclk_g2d_266[] = {
	{LV0_A,	267000000,	0,	NULL},
	{LV0,	267000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	200000000,	0,	NULL},
	{LV3,	160000000,	0,	NULL},
	{LV4,	134000000,	0,	NULL},
	{LV5,	100000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_gscl_333[] = {
	{LV0_A,	317000000,	0,	NULL},
	{LV0,	317000000,	0,	NULL},
	{LV1,	317000000,	0,	NULL},
	{LV2,	317000000,	0,	NULL},
	{LV3,	317000000,	0,	NULL},
	{LV4,	317000000,	0,	NULL},
	{LV5,	317000000,	0,	NULL},
	{LV6,	159000000,	0,	NULL},
};

struct devfreq_clk_info aclk_mscl[] = {
	{LV0_A,	400000000,	0,	&aclk_mscl_bus_pll_list},
	{LV0,	400000000,	0,	&aclk_mscl_bus_pll_list},
	{LV1,	317000000,	0,	&aclk_mscl_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_mscl_bus_pll_list},
	{LV3,	200000000,	0,	&aclk_mscl_bus_pll_list},
	{LV4,	160000000,	0,	&aclk_mscl_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_mscl_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_mscl_bus_pll_list},
};

struct devfreq_clk_info sclk_jpeg[] = {
	{LV0_A,	400000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV0,	400000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV1,	317000000,	0,	&sclk_jpeg_mfc_pll_list},
	{LV2,	267000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV3,	200000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV4,	160000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV5,	134000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV6,	100000000,	0,	&sclk_jpeg_bus_pll_list},
};

struct devfreq_clk_info aclk_mfc0_333[] = {
	{LV0_A,	633000000,	0,	&aclk_mfc0_333_mfc_pll_list},
	{LV0,	500000000,	0,	&aclk_mfc0_333_mphy_pll_list},
	{LV1,	317000000,	0,	&aclk_mfc0_333_mfc_pll_list},
	{LV2,	317000000,	0,	&aclk_mfc0_333_mfc_pll_list},
	{LV3,	317000000,	0,	&aclk_mfc0_333_mfc_pll_list},
	{LV4,	211000000,	0,	&aclk_mfc0_333_mfc_pll_list},
	{LV5,	159000000,	0,	&aclk_mfc0_333_mfc_pll_list},
	{LV6,	 80000000,	0,	&aclk_mfc0_333_mfc_pll_list},
};

struct devfreq_clk_info aclk_mfc1_333[] = {
	{LV0_A,	317000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV0,	317000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV1,	211000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV2,	211000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV3,	211000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV4,	159000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV5,	106000000,	0,	&aclk_mfc1_333_mfc_pll_list},
	{LV6,	 80000000,	0,	&aclk_mfc1_333_mfc_pll_list},
};

struct devfreq_clk_info aclk_hevc_400[] = {
	{LV0_A,	400000000,	0,	NULL},
	{LV0,	400000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	200000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus1_400,
	dout_mif_pre,
	aclk_bus2_400,
	aclk_g2d_400,
	aclk_g2d_266,
	aclk_gscl_333,
	aclk_mscl,
	sclk_jpeg,
	aclk_mfc0_333,
	aclk_mfc1_333,
	aclk_hevc_400,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS1_400,
	DOUT_MIF_PRE,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_G2D_400,
	DOUT_ACLK_G2D_266,
	DOUT_ACLK_GSCL_333,
	DOUT_ACLK_MSCL_400,
	DOUT_SCLK_JPEG,
	DOUT_ACLK_MFC0_333,
	DOUT_ACLK_MFC1_333,
	DOUT_ACLK_HEVC_400,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{"pd-bus1",},
	{"pd-bus2",},
	{"pd-bus2",},
	{"pd-g2d",},
	{"pd-g2d",},
	{"pd-gscl",},
	{"pd-mscl",},
	{"pd-mscl",},
	{"pd-mfc0",},
	{"pd-mfc1",},
	{"pd-hevc",},
};
#endif

static struct devfreq_simple_ondemand_data exynos5_devfreq_int_governor_data = {
	.pm_qos_class		= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 400000,
};

static struct exynos_devfreq_platdata exynos5430_qos_int = {
	.default_qos		= 100000,
};

static struct ppmu_info ppmu_int[] = {
	{
		.base = (void __iomem *)PPMU_D0_GEN_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_GEN_ADDR,
	},
};

static struct devfreq_exynos devfreq_int_exynos = {
	.ppmu_list = ppmu_int,
	.ppmu_count = ARRAY_SIZE(ppmu_int),
};

static struct devfreq_dynamic_clkgate exynos5_int_dynamic_clkgates[] = {
	{.paddr = 0x14830200,	.vaddr = 0x00,	.bit = 0x8,	.freq = 133000},
	{.paddr = 0x13430200,	.vaddr = 0x00,	.bit = 0x8,	.freq = 133000},
	{.paddr = 0x13430200,	.vaddr = 0x00,	.bit = 0x4,	.freq = 133000},
};

static int exynos5_init_int_dynamic_clkgate(struct platform_device *pdev)
{
	unsigned int i;
	unsigned int list_cnt;
	list_cnt = ARRAY_SIZE(exynos5_int_dynamic_clkgates);
	for (i = 0; i < list_cnt; i++) {
		exynos5_int_dynamic_clkgates[i].vaddr =
			(unsigned long)devm_ioremap(&pdev->dev, exynos5_int_dynamic_clkgates[i].paddr, SZ_4);
	}
	return 0;
}

static int exynos5_deinit_int_dynamic_clkgate(struct platform_device *pdev)
{
	unsigned int i;
	unsigned int list_cnt;
	list_cnt = ARRAY_SIZE(exynos5_int_dynamic_clkgates);
	for (i = 0; i < list_cnt; i++)
		if (exynos5_int_dynamic_clkgates[i].vaddr)
			devm_ioremap_release(&pdev->dev, (void *)exynos5_int_dynamic_clkgates[i].vaddr);
	return 0;
}

static void exynos5_enable_dynamic_clkgate(struct devfreq_dynamic_clkgate *clkgate_list,
						unsigned int list_cnt, bool up_case,
						unsigned long freq)
{
	unsigned int i;
	unsigned int tmp;
	void __iomem *reg;

	for (i = 0; i < list_cnt; i++) {
		if (up_case && clkgate_list[i].freq < freq) {
			/* disable dynamic clock gating */
			if (likely(clkgate_list[i].vaddr)) {
				reg = (void __iomem *)clkgate_list[i].vaddr;
				tmp = readl(reg);
				tmp &= ~(clkgate_list[i].bit);
				writel(tmp, reg);
			}
		} else if (!up_case && clkgate_list[i].freq > freq) {
			/* enable dynamic clock gating */
			if (likely(clkgate_list[i].vaddr)) {
				reg = (void __iomem *)clkgate_list[i].vaddr;
				tmp = readl(reg);
				tmp |= clkgate_list[i].bit;
				writel(tmp, reg);
			}
		}
	}
}

static struct pm_qos_request exynos5_int_qos;
static struct pm_qos_request boot_int_qos;
static struct pm_qos_request min_int_thermal_qos;
static struct pm_qos_request exynos5_int_bts_qos;
static struct devfreq_data_int *data_int;

int district_level_by_disp_333[] = {
	LV4,
	LV5,
	LV6,
	LV6,
};

static inline int exynos5_devfreq_int_get_idx(struct devfreq_opp_table *table,
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

static int exynos5_devfreq_int_set_clk(struct devfreq_data_int *data,
					int target_idx,
					struct clk *clk,
					struct devfreq_clk_info *clk_info)
{
	int i;
	struct devfreq_clk_states *clk_states = clk_info->states;

	if (clk_get_rate(clk) < clk_info->freq) {
		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_int_clk[clk_states->state[i].clk_idx].clk,
					devfreq_int_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info->freq != 0)
			clk_set_rate(clk, clk_info->freq);
	} else {
		if (clk_info->freq != 0)
			clk_set_rate(clk, clk_info->freq);

		if (clk_states) {
			for (i = 0; i < clk_states->state_count; ++i) {
				clk_set_parent(devfreq_int_clk[clk_states->state[i].clk_idx].clk,
					devfreq_int_clk[clk_states->state[i].parent_clk_idx].clk);
			}
		}

		if (clk_info->freq != 0)
			clk_set_rate(clk, clk_info->freq);
	}

	return 0;
}

void exynos5_int_notify_power_status(const char *pd_name, unsigned int turn_on)
{
	int i;
	int cur_freq_idx;

	if (!turn_on ||
		!data_int->use_dvfs)
		return;

	mutex_lock(&data_int->lock);
	cur_freq_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
                                                ARRAY_SIZE(devfreq_int_opp_list),
                                                data_int->devfreq->previous_freq);
	if (cur_freq_idx == -1) {
		mutex_unlock(&data_int->lock);
		pr_err("DEVFREQ(INT) : can't find target_idx to apply notify of power\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(devfreq_int_pm_domain); ++i) {
		if (devfreq_int_pm_domain[i].pm_domain_name == NULL)
			continue;
		if (strcmp(devfreq_int_pm_domain[i].pm_domain_name, pd_name))
			continue;

		exynos5_devfreq_int_set_clk(data_int,
						cur_freq_idx,
						devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk,
						devfreq_clk_int_info_list[i]);
	}
	mutex_unlock(&data_int->lock);
}

void exynos5_update_district_int_level(int aclk_disp_333_idx)
{
	int int_qos = LV6;

	if (aclk_disp_333_idx < 0 ||
		ARRAY_SIZE(district_level_by_disp_333) <= aclk_disp_333_idx) {
		pr_err("DEVFREQ(INT) : can't update distriction of int level by aclk_disp_333\n");
		return;
	}

	int_qos = district_level_by_disp_333[aclk_disp_333_idx];

	if (pm_qos_request_active(&exynos5_int_bts_qos))
		pm_qos_update_request(&exynos5_int_bts_qos, devfreq_int_opp_list[int_qos].freq);
}

static int exynos5_devfreq_int_set_freq(struct devfreq_data_int *data,
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
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_int_pm_domain[i].pm_domain;

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
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_int_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);
#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}

	return 0;
}

static int exynos5_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_int, volt, volt_range);
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

static int exynos5_devfreq_int_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *int_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_int = int_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&int_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		mutex_unlock(&int_data->lock);
		dev_err(dev, "DEVFREQ(INT) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, int_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						devfreq_int->previous_freq);
	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0 ||
		old_idx < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_enable_dynamic_clkgate(exynos5_int_dynamic_clkgates,
						ARRAY_SIZE(exynos5_int_dynamic_clkgates),
						true, *target_freq);
		exynos5_devfreq_int_set_volt(int_data, target_volt, target_volt + VOLT_STEP);
		exynos5_devfreq_int_set_freq(int_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_int_set_freq(int_data, target_idx, old_idx);
		exynos5_devfreq_int_set_volt(int_data, target_volt, target_volt + VOLT_STEP);
		exynos5_enable_dynamic_clkgate(exynos5_int_dynamic_clkgates,
						ARRAY_SIZE(exynos5_int_dynamic_clkgates),
						false, *target_freq);
	}
out:
	mutex_unlock(&int_data->lock);

	return ret;
}

static int exynos5_devfreq_int_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);

	if (!data->use_dvfs)
		return -EAGAIN;

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_int_exynos.val_pmcnt;
	stat->total_time = devfreq_int_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_int_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_int_target,
	.get_dev_status	= exynos5_devfreq_int_get_dev_status,
	.max_state	= LV_COUNT,
};

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_int_init_pm_domain(void)
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

		for (i = 0; i < ARRAY_SIZE(devfreq_int_pm_domain); ++i) {
			if (devfreq_int_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_int_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_int_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

static int exynos5_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); ++i) {
		devfreq_int_clk[i].clk = __clk_lookup(devfreq_int_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}
static int exynos5_init_int_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_opp_list); ++i) {
		freq = devfreq_int_opp_list[i].freq;
		volt = get_match_volt(ID_INT, freq);
		if (!volt)
			volt = devfreq_int_opp_list[i].volt;

		exynos5_devfreq_int_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(INT) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(INT) : %uKhz, %uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos5_devfreq_int_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_int_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_int_reboot_notifier = {
	.notifier_call = exynos5_devfreq_int_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos,
					exynos5_devfreq_int_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos,
					exynos5430_qos_int.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_int_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_int *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;

	if (exynos5_devfreq_int_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_int_init_pm_domain()) {
		ret = -EINVAL;
		goto err_data;
	}
#endif

	data = kzalloc(sizeof(struct devfreq_data_int), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5_devfreq_int_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_int_profile.freq_table == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_int_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	ret = exynos5_init_int_dynamic_clkgate(pdev);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	data_int = data;
	mutex_init(&data->lock);

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	data->vdd_int = regulator_get(NULL, "vdd_int");
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_int_profile,
						"simple_ondemand",
						&exynos5_devfreq_int_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_int_notifier;

	exynos5430_devfreq_register(&devfreq_int_exynos);
	exynos5430_ppmu_register_notifier(INT, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = devfreq_int_opp_list[LV0].freq;

	register_reboot_notifier(&exynos5_int_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_int_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	data->use_dvfs = true;

	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(exynos5_devfreq_int_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_int_thermal_qos);
	pm_qos_remove_request(&exynos5_int_qos);
	pm_qos_remove_request(&boot_int_qos);
	pm_qos_remove_request(&exynos5_int_bts_qos);

	exynos5_deinit_int_dynamic_clkgate(pdev);

	regulator_put(data->vdd_int);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_int_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_int_pm = {
	.suspend	= exynos5_devfreq_int_suspend,
	.resume		= exynos5_devfreq_int_resume,
};

static struct platform_driver exynos5_devfreq_int_driver = {
	.probe	= exynos5_devfreq_int_probe,
	.remove	= exynos5_devfreq_int_remove,
	.driver	= {
		.name	= "exynos5-devfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_int_pm,
	},
};

static struct platform_device exynos5_devfreq_int_device = {
	.name	= "exynos5-devfreq-int",
	.id	= -1,
};

static int __init exynos5_devfreq_int_qos_init(void)
{
	pm_qos_add_request(&exynos5_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_add_request(&min_int_thermal_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_add_request(&boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_add_request(&exynos5_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_update_request_timeout(&exynos5_int_qos,
					exynos5_devfreq_int_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_int_qos_init);

static int __init exynos5_devfreq_int_init(void)
{
	int ret;

	exynos5_devfreq_int_device.dev.platform_data = &exynos5430_qos_int;

	ret = platform_device_register(&exynos5_devfreq_int_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_int_driver);
}
late_initcall(exynos5_devfreq_int_init);

static void __exit exynos5_devfreq_int_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_int_driver);
	platform_device_unregister(&exynos5_devfreq_int_device);
}
module_exit(exynos5_devfreq_int_exit);
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
#include <linux/of_address.h>
#include <linux/of_platform.h>

#include <mach/tmu.h>
#include <mach/asv-exynos.h>
#include <mach/pm_domains.h>
#include <mach/regs-clock-exynos5430.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"
#include "devfreq_exynos.h"
#include "governor.h"

#define DEVFREQ_INITIAL_FREQ	(400000)
#define DEVFREQ_POLLING_PERIOD	(0)

#define INT_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	37500
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

enum devfreq_int_idx {
	LV0_A,
	LV0_B,
	LV0,
	LV1,
	LV2,
	LV3,
	LV4,
	LV5,
	LV6,
	LV_COUNT,
};

enum devfreq_int_clk {
	DOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS2_400,
	MOUT_BUS_PLL_USER,
	MOUT_MFC_PLL_USER,
	MOUT_ACLK_G2D_400_A,
	DOUT_ACLK_G2D_400,
	DOUT_ACLK_G2D_266,
	DOUT_ACLK_GSCL_333,
	MOUT_ACLK_MSCL_400_A,
	DOUT_ACLK_MSCL_400,
	MOUT_SCLK_JPEG_A,
	MOUT_SCLK_JPEG_B,
	DOUT_SCLK_JPEG,
	DOUT_ACLK_MFC0_333,
	DOUT_ACLK_MFC1_333,
	DOUT_ACLK_HEVC_400,
	CLK_COUNT,
};

struct devfreq_data_int {
	struct device *dev;
	struct devfreq *devfreq;

	struct regulator *vdd_int;
	unsigned long old_volt;
	unsigned long volt_offset;

	struct mutex lock;

	struct notifier_block tmu_notifier;
};

struct devfreq_clk_list devfreq_int_clk[CLK_COUNT] = {
	{"dout_aclk_bus1_400",},
	{"dout_aclk_bus2_400",},
	{"mout_bus_pll_user",},
	{"mout_mfc_pll_user",},
	{"mout_aclk_g2d_400_a",},
	{"dout_aclk_g2d_400",},
	{"dout_aclk_g2d_266",},
	{"dout_aclk_gscl_333",},
	{"mout_aclk_mscl_400_a",},
	{"dout_aclk_mscl_400", },
	{"mout_sclk_jpeg_a",},
	{"mout_sclk_jpeg_b",},
	{"dout_sclk_jpeg",},
	{"dout_aclk_mfc0_333",},
	{"dout_aclk_mfc1_333",},
	{"dout_aclk_hevc_400",},
};

struct devfreq_opp_table devfreq_int_opp_list[] = {
	{LV0_A,	667000,	1175000},
	{LV0_B,	533000,	1075000},
	{LV0,	400000,	1000000},
	{LV1,	333000,	1000000},
	{LV2,	266000,	1000000},
	{LV3,	200000,	1000000},
	{LV4,	160000,	1000000},
	{LV5,	133000,	1000000},
	{LV6,	100000,	1000000},
};

struct devfreq_clk_state aclk_g2d_mfc_pll[] = {
	{MOUT_ACLK_G2D_400_A,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state aclk_g2d_bus_pll[] = {
	{MOUT_ACLK_G2D_400_A,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_state aclk_mscl_mfc_pll[] = {
	{MOUT_ACLK_MSCL_400_A,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state aclk_mscl_bus_pll[] = {
	{MOUT_ACLK_MSCL_400_A,	MOUT_BUS_PLL_USER},
};

struct devfreq_clk_state sclk_jpeg_mfc_pll[] = {
	{MOUT_SCLK_JPEG_B,	MOUT_MFC_PLL_USER},
};

struct devfreq_clk_state sclk_jpeg_bus_pll[] = {
	{MOUT_SCLK_JPEG_B,	MOUT_SCLK_JPEG_A},
};

struct devfreq_clk_states aclk_g2d_mfc_pll_list = {
	.state = aclk_g2d_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_g2d_mfc_pll),
};

struct devfreq_clk_states aclk_g2d_bus_pll_list = {
	.state = aclk_g2d_bus_pll,
	.state_count = ARRAY_SIZE(aclk_g2d_bus_pll),
};

struct devfreq_clk_states aclk_mscl_mfc_pll_list = {
	.state = aclk_mscl_mfc_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_mfc_pll),
};

struct devfreq_clk_states aclk_mscl_bus_pll_list = {
	.state = aclk_mscl_bus_pll,
	.state_count = ARRAY_SIZE(aclk_mscl_bus_pll),
};

struct devfreq_clk_states sclk_jpeg_mfc_pll_list = {
	.state = sclk_jpeg_mfc_pll,
	.state_count = ARRAY_SIZE(sclk_jpeg_mfc_pll),
};

struct devfreq_clk_states sclk_jpeg_bus_pll_list = {
	.state = sclk_jpeg_bus_pll,
	.state_count = ARRAY_SIZE(sclk_jpeg_bus_pll),
};

struct devfreq_clk_info aclk_bus1_400[] = {
	{LV0_A,	400000000,	0,	NULL},
	{LV0_B,	400000000,	0,	NULL},
	{LV0,	400000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	200000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_bus2_400[] = {
	{LV0_A,	400000000,	0,	NULL},
	{LV0_B,	400000000,	0,	NULL},
	{LV0,	400000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	200000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_g2d_400[] = {
	{LV0_A,	400000000,	0,	&aclk_g2d_bus_pll_list},
	{LV0_B,	400000000,	0,	&aclk_g2d_bus_pll_list},
	{LV0,	400000000,	0,	&aclk_g2d_bus_pll_list},
	{LV1,	334000000,	0,	&aclk_g2d_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_g2d_bus_pll_list},
	{LV3,	200000000,	0,	&aclk_g2d_bus_pll_list},
	{LV4,	160000000,	0,	&aclk_g2d_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_g2d_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_g2d_bus_pll_list},
};

struct devfreq_clk_info aclk_g2d_266[] = {
	{LV0_A,	267000000,	0,	NULL},
	{LV0_B,	267000000,	0,	NULL},
	{LV0,	267000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	200000000,	0,	NULL},
	{LV3,	160000000,	0,	NULL},
	{LV4,	134000000,	0,	NULL},
	{LV5,	100000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info aclk_gscl_333[] = {
	{LV0_A,	334000000,	0,	NULL},
	{LV0_B,	334000000,	0,	NULL},
	{LV0,	334000000,	0,	NULL},
	{LV1,	334000000,	0,	NULL},
	{LV2,	334000000,	0,	NULL},
	{LV3,	334000000,	0,	NULL},
	{LV4,	334000000,	0,	NULL},
	{LV5,	167000000,	0,	NULL},
	{LV6,	167000000,	0,	NULL},
};

struct devfreq_clk_info aclk_mscl[] = {
	{LV0_A,	400000000,	0,	&aclk_mscl_bus_pll_list},
	{LV0_B,	400000000,	0,	&aclk_mscl_bus_pll_list},
	{LV0,	400000000,	0,	&aclk_mscl_bus_pll_list},
	{LV1,	334000000,	0,	&aclk_mscl_mfc_pll_list},
	{LV2,	267000000,	0,	&aclk_mscl_bus_pll_list},
	{LV3,	200000000,	0,	&aclk_mscl_bus_pll_list},
	{LV4,	160000000,	0,	&aclk_mscl_bus_pll_list},
	{LV5,	134000000,	0,	&aclk_mscl_bus_pll_list},
	{LV6,	100000000,	0,	&aclk_mscl_bus_pll_list},
};

struct devfreq_clk_info sclk_jpeg[] = {
	{LV0_A,	400000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV0_B,	400000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV0,	400000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV1,	334000000,	0,	&sclk_jpeg_mfc_pll_list},
	{LV2,	267000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV3,	200000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV4,	160000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV5,	134000000,	0,	&sclk_jpeg_bus_pll_list},
	{LV6,	100000000,	0,	&sclk_jpeg_bus_pll_list},
};

struct devfreq_clk_info aclk_mfc0_333[] = {
	{LV0_A,	334000000,	0,	NULL},
	{LV0_B,	334000000,	0,	NULL},
	{LV0,	334000000,	0,	NULL},
	{LV1,	222000000,	0,	NULL},
	{LV2,	222000000,	0,	NULL},
	{LV3,	222000000,	0,	NULL},
	{LV4,	167000000,	0,	NULL},
	{LV5,	111000000,	0,	NULL},
	{LV6,	 83000000,	0,	NULL},
};

struct devfreq_clk_info aclk_mfc1_333[] = {
	{LV0_A,	334000000,	0,	NULL},
	{LV0_B,	334000000,	0,	NULL},
	{LV0,	334000000,	0,	NULL},
	{LV1,	222000000,	0,	NULL},
	{LV2,	222000000,	0,	NULL},
	{LV3,	222000000,	0,	NULL},
	{LV4,	167000000,	0,	NULL},
	{LV5,	111000000,	0,	NULL},
	{LV6,	 83000000,	0,	NULL},
};

struct devfreq_clk_info aclk_hevc_400[] = {
	{LV0_A,	400000000,	0,	NULL},
	{LV0_B,	400000000,	0,	NULL},
	{LV0,	400000000,	0,	NULL},
	{LV1,	267000000,	0,	NULL},
	{LV2,	267000000,	0,	NULL},
	{LV3,	200000000,	0,	NULL},
	{LV4,	160000000,	0,	NULL},
	{LV5,	134000000,	0,	NULL},
	{LV6,	100000000,	0,	NULL},
};

struct devfreq_clk_info *devfreq_clk_int_info_list[] = {
	aclk_bus1_400,
	aclk_bus2_400,
	aclk_g2d_400,
	aclk_g2d_266,
	aclk_gscl_333,
	aclk_mscl,
	sclk_jpeg,
	aclk_mfc0_333,
	aclk_mfc1_333,
	aclk_hevc_400,
};

enum devfreq_int_clk devfreq_clk_int_info_idx[] = {
	DOUT_ACLK_BUS1_400,
	DOUT_ACLK_BUS2_400,
	DOUT_ACLK_G2D_400,
	DOUT_ACLK_G2D_266,
	DOUT_ACLK_GSCL_333,
	DOUT_ACLK_MSCL_400,
	DOUT_SCLK_JPEG,
	DOUT_ACLK_MFC0_333,
	DOUT_ACLK_MFC1_333,
	DOUT_ACLK_HEVC_400,
};

#ifdef CONFIG_PM_RUNTIME
struct devfreq_pm_domain_link devfreq_int_pm_domain[] = {
	{"pd-bus1",},
	{"pd-bus2",},
	{"pd-g2d",},
	{"pd-g2d",},
	{"pd-gscl",},
	{"pd-mscl",},
	{"pd-mscl",},
	{"pd-mfc0",},
	{"pd-mfc1",},
	{"pd-hevc",},
};
#endif

static struct devfreq_simple_ondemand_data exynos5_devfreq_int_governor_data = {
	.pm_qos_class		= PM_QOS_DEVICE_THROUGHPUT,
	.upthreshold		= 95,
	.cal_qos_max		= 667000,
};

static struct exynos_devfreq_platdata exynos5430_qos_int = {
	.default_qos		= 100000,
};

static struct ppmu_info ppmu_int[] = {
	{
		.base = (void __iomem *)PPMU_D0_GEN_ADDR,
	}, {
		.base = (void __iomem *)PPMU_D1_GEN_ADDR,
	},
};

static struct devfreq_exynos devfreq_int_exynos = {
	.ppmu_list = ppmu_int,
	.ppmu_count = ARRAY_SIZE(ppmu_int),
};

static struct pm_qos_request exynos5_int_qos;
static struct pm_qos_request boot_int_qos;
static struct pm_qos_request min_int_thermal_qos;
static struct pm_qos_request exynos5_int_bts_qos;

int district_level_by_disp_333[] = {
	LV0,
	LV2,
	LV4,
	LV5,
};

void exynos5_update_district_int_level(int aclk_disp_333_idx)
{
	int int_qos = LV6;

	if (aclk_disp_333_idx < 0 ||
		ARRAY_SIZE(district_level_by_disp_333) <= aclk_disp_333_idx) {
		pr_err("DEVFREQ(INT) : can't update distriction of int level by aclk_disp_333\n");
		return;
	}

	int_qos = district_level_by_disp_333[aclk_disp_333_idx];

	if (pm_qos_request_active(&exynos5_int_bts_qos))
		pm_qos_update_request(&exynos5_int_bts_qos, devfreq_int_opp_list[int_qos].freq);
}

static inline int exynos5_devfreq_int_get_idx(struct devfreq_opp_table *table,
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

static int exynos5_devfreq_int_set_freq(struct devfreq_data_int *data,
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
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_int_pm_domain[i].pm_domain;

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
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(devfreq_clk_int_info_list); ++i) {
			clk_info = &devfreq_clk_int_info_list[i][target_idx];
			clk_states = clk_info->states;

#ifdef CONFIG_PM_RUNTIME
			pm_domain = devfreq_int_pm_domain[i].pm_domain;

			if (pm_domain != NULL) {
				mutex_lock(&pm_domain->access_lock);
				if ((__raw_readl(pm_domain->base + 0x4) & EXYNOS_INT_LOCAL_PWR_EN) == 0) {
					mutex_unlock(&pm_domain->access_lock);
					continue;
				}
			}
#endif

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

			if (clk_states) {
				for (j = 0; j < clk_states->state_count; ++j) {
					clk_set_parent(devfreq_int_clk[clk_states->state[j].clk_idx].clk,
						devfreq_int_clk[clk_states->state[j].parent_clk_idx].clk);
				}
			}

			if (clk_info->freq != 0)
				clk_set_rate(devfreq_int_clk[devfreq_clk_int_info_idx[i]].clk, clk_info->freq);

#ifdef CONFIG_PM_RUNTIME
			if (pm_domain != NULL)
				mutex_unlock(&pm_domain->access_lock);
#endif
		}
	}

	return 0;
}

static int exynos5_devfreq_int_set_volt(struct devfreq_data_int *data,
					unsigned long volt,
					unsigned long volt_range)
{
	if (data->old_volt == volt)
		goto out;

	regulator_set_voltage(data->vdd_int, volt, volt_range);
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

static int exynos5_devfreq_int_target(struct device *dev,
					unsigned long *target_freq,
					u32 flags)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct devfreq_data_int *int_data = platform_get_drvdata(pdev);
	struct devfreq *devfreq_int = int_data->devfreq;
	struct opp *target_opp;
	int target_idx, old_idx;
	unsigned long target_volt;
	unsigned long old_freq;

	mutex_lock(&int_data->lock);

	rcu_read_lock();
	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		rcu_read_unlock();
		dev_err(dev, "DEVFREQ(INT) : Invalid OPP to find\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = opp_get_freq(target_opp);
	target_volt = opp_get_voltage(target_opp);
#ifdef CONFIG_EXYNOS_THERMAL
	target_volt = get_limit_voltage(target_volt, int_data->volt_offset);
#endif
	rcu_read_unlock();

	target_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						*target_freq);
	old_idx = exynos5_devfreq_int_get_idx(devfreq_int_opp_list,
						ARRAY_SIZE(devfreq_int_opp_list),
						devfreq_int->previous_freq);
	old_freq = devfreq_int->previous_freq;

	if (target_idx < 0)
		goto out;

	if (old_freq == *target_freq)
		goto out;

	if (old_freq < *target_freq) {
		exynos5_devfreq_int_set_volt(int_data, target_volt, target_volt + VOLT_STEP);
		exynos5_devfreq_int_set_freq(int_data, target_idx, old_idx);
	} else {
		exynos5_devfreq_int_set_freq(int_data, target_idx, old_idx);
		exynos5_devfreq_int_set_volt(int_data, target_volt, target_volt + VOLT_STEP);
	}
out:
	mutex_unlock(&int_data->lock);

	return ret;
}

static int exynos5_devfreq_int_get_dev_status(struct device *dev,
						struct devfreq_dev_status *stat)
{
	struct devfreq_data_int *data = dev_get_drvdata(dev);

	stat->current_frequency = data->devfreq->previous_freq;
	stat->busy_time = devfreq_int_exynos.val_pmcnt;
	stat->total_time = devfreq_int_exynos.val_ccnt;

	return 0;
}

static struct devfreq_dev_profile exynos5_devfreq_int_profile = {
	.initial_freq	= DEVFREQ_INITIAL_FREQ,
	.polling_ms	= DEVFREQ_POLLING_PERIOD,
	.target		= exynos5_devfreq_int_target,
	.get_dev_status	= exynos5_devfreq_int_get_dev_status,
	.max_state	= LV_COUNT,
};

#ifdef CONFIG_PM_RUNTIME
static int exynos5_devfreq_int_init_pm_domain(void)
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

		for (i = 0; i < ARRAY_SIZE(devfreq_int_pm_domain); ++i) {
			if (devfreq_int_pm_domain[i].pm_domain_name == NULL)
				continue;

			if (!strcmp(devfreq_int_pm_domain[i].pm_domain_name, pd->genpd.name))
				devfreq_int_pm_domain[i].pm_domain = pd;
		}
	}

	return 0;
}
#endif

static int exynos5_devfreq_int_init_clock(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_clk); ++i) {
		devfreq_int_clk[i].clk = __clk_lookup(devfreq_int_clk[i].clk_name);
		if (IS_ERR_OR_NULL(devfreq_int_clk[i].clk)) {
			pr_err("DEVFREQ(INT) : %s can't get clock\n", devfreq_int_clk[i].clk_name);
			return -EINVAL;
		}
	}

	return 0;
}
static int exynos5_init_int_table(struct device *dev)
{
	unsigned int i;
	unsigned int ret;
	unsigned int freq;
	unsigned int volt;

	for (i = 0; i < ARRAY_SIZE(devfreq_int_opp_list); ++i) {
		freq = devfreq_int_opp_list[i].freq;
		volt = get_match_volt(ID_INT, freq);
		if (!volt)
			volt = devfreq_int_opp_list[i].volt;

		exynos5_devfreq_int_profile.freq_table[i] = freq;

		ret = opp_add(dev, freq, volt);
		if (ret) {
			pr_err("DEVFREQ(INT) : Failed to add opp entries %uKhz, %uV\n", freq, volt);
			return ret;
		} else {
			pr_info("DEVFREQ(INT) : %uKhz, %uV\n", freq, volt);
		}
	}

	return 0;
}

static int exynos5_devfreq_int_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_notifier_block *devfreq_nb;

	devfreq_nb = container_of(nb, struct devfreq_notifier_block, nb);

	mutex_lock(&devfreq_nb->df->lock);
	update_devfreq(devfreq_nb->df);
	mutex_unlock(&devfreq_nb->df->lock);

	return NOTIFY_OK;
}

static int exynos5_devfreq_int_reboot_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	pm_qos_update_request(&boot_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_int_reboot_notifier = {
	.notifier_call = exynos5_devfreq_int_reboot_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_devfreq_int_tmu_notifier(struct notifier_block *nb, unsigned long event,
						void *v)
{
	struct devfreq_data_int *data = container_of(nb, struct devfreq_data_int, tmu_notifier);
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos,
					exynos5_devfreq_int_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_int);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			regulator_set_voltage(data->vdd_int, set_volt, set_volt + VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&min_int_thermal_qos))
			pm_qos_update_request(&min_int_thermal_qos,
					exynos5430_qos_int.default_qos);
	}

	return NOTIFY_OK;
}
#endif

static int exynos5_devfreq_int_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct devfreq_data_int *data;
	struct devfreq_notifier_block *devfreq_nb;
	struct exynos_devfreq_platdata *plat_data;

	if (exynos5_devfreq_int_init_clock()) {
		ret = -EINVAL;
		goto err_data;
	}

#ifdef CONFIG_PM_RUNTIME
	if (exynos5_devfreq_int_init_pm_domain()) {
		ret = -EINVAL;
		goto err_data;
	}
#endif

	data = kzalloc(sizeof(struct devfreq_data_int), GFP_KERNEL);
	if (data == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate private data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	exynos5_devfreq_int_profile.freq_table = kzalloc(sizeof(int) * LV_COUNT, GFP_KERNEL);
	if (exynos5_devfreq_int_profile.freq_table == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate freq table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos5_init_int_table(&pdev->dev);
	if (ret)
		goto err_inittable;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	data->volt_offset = 0;
	data->dev = &pdev->dev;
	data->vdd_int = regulator_get(NULL, "vdd_int");
	data->devfreq = devfreq_add_device(data->dev,
						&exynos5_devfreq_int_profile,
						"simple_ondemand",
						&exynos5_devfreq_int_governor_data);

	devfreq_nb = kzalloc(sizeof(struct devfreq_notifier_block), GFP_KERNEL);
	if (devfreq_nb == NULL) {
		pr_err("DEVFREQ(INT) : Failed to allocate notifier block\n");
		ret = -ENOMEM;
		goto err_nb;
	}

	devfreq_nb->df = data->devfreq;
	devfreq_nb->nb.notifier_call = exynos5_devfreq_int_notifier;

	exynos5430_devfreq_register(&devfreq_int_exynos);
	exynos5430_ppmu_register_notifier(INT, &devfreq_nb->nb);

	plat_data = data->dev->platform_data;

	data->devfreq->min_freq = plat_data->default_qos;
	data->devfreq->max_freq = exynos5_devfreq_int_governor_data.cal_qos_max;

	register_reboot_notifier(&exynos5_int_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_devfreq_int_tmu_notifier;
	exynos_tmu_add_notifier(&data->tmu_notifier);
#endif
	return ret;
err_nb:
	devfreq_remove_device(data->devfreq);
err_inittable:
	kfree(exynos5_devfreq_int_profile.freq_table);
err_freqtable:
	kfree(data);
err_data:
	return ret;
}

static int exynos5_devfreq_int_remove(struct platform_device *pdev)
{
	struct devfreq_data_int *data = platform_get_drvdata(pdev);

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_int_thermal_qos);
	pm_qos_remove_request(&exynos5_int_qos);
	pm_qos_remove_request(&boot_int_qos);
	pm_qos_remove_request(&exynos5_int_bts_qos);

	regulator_put(data->vdd_int);

	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_int_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, exynos5_devfreq_int_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_int_resume(struct device *dev)
{
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	if (pm_qos_request_active(&exynos5_int_qos))
		pm_qos_update_request(&exynos5_int_qos, pdata->default_qos);

	return 0;
}

static struct dev_pm_ops exynos5_devfreq_int_pm = {
	.suspend	= exynos5_devfreq_int_suspend,
	.resume		= exynos5_devfreq_int_resume,
};

static struct platform_driver exynos5_devfreq_int_driver = {
	.probe	= exynos5_devfreq_int_probe,
	.remove	= exynos5_devfreq_int_remove,
	.driver	= {
		.name	= "exynos5-devfreq-int",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_int_pm,
	},
};

static struct platform_device exynos5_devfreq_int_device = {
	.name	= "exynos5-devfreq-int",
	.id	= -1,
};

static int __init exynos5_devfreq_int_qos_init(void)
{
	pm_qos_add_request(&exynos5_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_add_request(&min_int_thermal_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_add_request(&boot_int_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_add_request(&exynos5_int_bts_qos, PM_QOS_DEVICE_THROUGHPUT, exynos5430_qos_int.default_qos);
	pm_qos_update_request_timeout(&exynos5_int_qos,
					exynos5_devfreq_int_profile.initial_freq, 40000 * 1000);

	return 0;
}
device_initcall(exynos5_devfreq_int_qos_init);

static int __init exynos5_devfreq_int_init(void)
{
	int ret;

	exynos5_devfreq_int_device.dev.platform_data = &exynos5430_qos_int;

	ret = platform_device_register(&exynos5_devfreq_int_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_int_driver);
}
late_initcall(exynos5_devfreq_int_init);

static void __exit exynos5_devfreq_int_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_int_driver);
	platform_device_unregister(&exynos5_devfreq_int_device);
}
module_exit(exynos5_devfreq_int_exit);
#endif
