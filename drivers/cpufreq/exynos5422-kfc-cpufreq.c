/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5422 - KFC Core frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/clk-private.h>
#include <linux/pm_qos.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>

#define CPUFREQ_LEVEL_END_CA7	(L14 + 1)
#define L2_LOCAL_PWR_EN		0x3
#define PKG_ID_DVFS_VERSION	(((__raw_readl(S5P_VA_CHIPID + 4)) >> 8) & 0x03)

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA7;
static int min_support_idx_CA7 = (CPUFREQ_LEVEL_END_CA7 - 1);

#ifdef CONFIG_PM_DEBUG
static int is_init_time;
#endif

static struct clk *fout_kpll;
static struct clk *fout_cpll;
static struct clk *dout_cpu_kfc;
static struct clk *dout_hpm_kfc;
static struct clk *mout_hpm_kfc;
static struct clk *mout_cpu_kfc;
static struct clk *mout_kpll_ctrl;
static struct clk *mout_cpll_ctrl;
static struct clk *mout_mx_mspll_kfc;

static unsigned int exynos5422_volt_table_CA7[CPUFREQ_LEVEL_END_CA7];
static unsigned int exynos5422_abb_table_CA7[CPUFREQ_LEVEL_END_CA7];

static struct cpufreq_frequency_table exynos5422_freq_table_CA7[] = {
	{L0,  1600 * 1000},
	{L1,  1500 * 1000},
	{L2,  1400 * 1000},
	{L3,  1300 * 1000},
	{L4,  1200 * 1000},
	{L5,  1100 * 1000},
	{L6,  1000 * 1000},
	{L7,   900 * 1000},
	{L8,   800 * 1000},
	{L9,   700 * 1000},
	{L10,  600 * 1000},
	{L11,  500 * 1000},
	{L12,  400 * 1000},
	{L13,  300 * 1000},
	{L14,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5422_clkdiv_table_CA7[CPUFREQ_LEVEL_END_CA7];

static unsigned int clkdiv_cpu0_5422_CA7[CPUFREQ_LEVEL_END_CA7][5] = {
	/*
	 * Clock divider value for following
	 * { KFC, ACLK, HPM, PCLK, KPLL }
	 */
	/* ARM L0: 1.6GHz */
	{ 0, 3, 7, 5, 3 },

	/* ARM L1: 1.5GMHz */
	{ 0, 3, 7, 5, 3 },

	/* ARM L2: 1.4GMHz */
	{ 0, 3, 7, 5, 3 },

	/* ARM L3: 1.3GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L4: 1.2GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L5: 1.1GHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L6: 1000MHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L7: 900MHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L8: 800MHz */
	{ 0, 2, 7, 5, 3 },

	/* ARM L9: 700MHz */
	{ 0, 2, 7, 4, 3 },

	/* ARM L10: 600MHz */
	{ 0, 2, 7, 4, 3 },

	/* ARM L11: 500MHz */
	{ 0, 2, 7, 4, 3 },

	/* ARM L12: 400MHz */
	{ 0, 2, 7, 3, 3 },

	/* ARM L13: 300MHz */
	{ 0, 2, 7, 3, 3 },

	/* ARM L14: 200MHz */
	{ 0, 2, 7, 3, 3 },
};

static unsigned int exynos5422_kfc_pll_pms_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	/* MDIV | PDIV | SDIV */
	/* KPLL FOUT L0: 2.0GHz */
	/*PLL2450X_PMS(500, 6, 0),*/
	/* KPLL FOUT L0: 1.6GHz */
	((200 << 16) | (3 << 8) | (0x0)),

	/* KPLL FOUT L1: 1.5GHz */
	((250 << 16) | (4 << 8) | (0x0)),

	/* KPLL FOUT L2: 1.4GHz */
	((175 << 16) | (3 << 8) | (0x0)),

	/* KPLL FOUT L3: 1.3GHz */
	((325 << 16) | (6 << 8) | (0x0)),

	/* KPLL FOUT L4: 1.2GHz */
	((200 << 16) | (2 << 8) | (0x1)),

	/* KPLL FOUT L5: 1.1GHz */
	((275 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L6: 1000MHz */
	((250 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L7: 900MHz */
	((150 << 16) | (2 << 8) | (0x1)),

	/* KPLL FOUT L8: 800MHz */
	((200 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L9: 700MHz */
	((175 << 16) | (3 << 8) | (0x1)),

	/* KPLL FOUT L10: 600MHz */
	((200 << 16) | (2 << 8) | (0x2)),

	/* KPLL FOUT L11: 500MHz */
	((250 << 16) | (3 << 8) | (0x2)),

	/* KPLL FOUT L12: 400MHz */
	((200 << 16) | (3 << 8) | (0x2)),

	/* KPLL FOUT L13: 300MHz */
	((200 << 16) | (2 << 8) | (0x3)),

	/* KPLL FOUT L14: 200MHz */
	((200 << 16) | (3 << 8) | (0x3)),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5422_CA7[CPUFREQ_LEVEL_END_CA7] = {
	1200000,    /* LO 1600 */
	1250000,    /* L1 1500 */
	1250000,    /* L2 1400 */
	1250000,    /* L3 1300 */
	1250000,    /* L4 1200 */
	1250000,    /* L5 1100 */
	1100000,    /* L6 1000 */
	1100000,    /* L7  900 */
	1100000,    /* L8  800 */
	1000000,    /* L9  700 */
	1000000,    /* L10 600 */
	1000000,    /* L11 500 */
	1000000,    /* L12 400 */
	 900000,    /* L13 300 */
	 900000,    /* L14 200 */
};

/* Minimum memory throughput in megabytes per second */
static int exynos5422_bus_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	633000, /* 1.6 GHz */
	633000, /* 1.5 GHz */
	633000, /* 1.4 GHz */
	633000, /* 1.3 GHz */
	633000, /* 1.2 GHz */
	633000, /* 1.1 GHz */
	543000, /* 1.0 GHz */
	413000, /* 900 MHz */
	413000, /* 800 MHz */
	275000, /* 700 MHz */
	133000, /* 600 MHz */
	133000, /* 500 MHz */
	0,  /* 400 MHz */
	0,  /* 300 MHz */
	0,  /* 200 MHz */

};

static void exynos5422_set_ema_CA7(unsigned int target_volt)
{
	unsigned int value = 0;
#ifdef CONFIG_PM_DEBUG
	unsigned int temp;
#endif

	if (target_volt < EMA_VOLT_LEV_1)
		value |= EMA_VAL_0;
	else if (target_volt < EMA_VOLT_LEV_2)
		value |= EMA_VAL_1;
	else if (target_volt < EMA_VOLT_LEV_3)
		value |= EMA_VAL_2;
	else
		value |= EMA_VAL_3;

	value = (value << 4) | value;
#ifdef CONFIG_PM_DEBUG
	if (is_init_time) {
		pr_info("[SET_EMA_TEST] volt : %d  value : 0x%x\n",
				target_volt, value);
		goto skip;
	}
#endif
	__raw_writel(value, EXYNOS5422_KFC_EMA_CTRL);

#ifdef CONFIG_PM_DEBUG
	do {
		temp = __raw_readl(EXYNOS5422_KFC_EMA_STATUS);
	} while (temp & EMA_ON_CHANGE);

skip:
	return;
#endif
}

static void exynos5422_set_clkdiv_CA7(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - KFC0 */
	tmp = exynos5422_clkdiv_table_CA7[div_index].clkdiv0;

	__raw_writel(tmp, EXYNOS5_CLK_DIV_KFC0);

	while (__raw_readl(EXYNOS5_CLK_DIV_STAT_KFC0) & 0x11111111)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5_CLK_DIV_KFC0);
	pr_info("DIV_KFC0[0x%x]\n", tmp);
#endif
}

static void exynos5422_set_kfc_pll_CA7(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;
	/* 0. before change to MPLL, set div for MPLL output */
	if ((new_index < L5) && (old_index < L5))
		exynos5422_set_clkdiv_CA7(L5); /* pll_safe_index of CA7 */

	/* 1. CLKMUX_CPU_KFC = mout_mx_mspll_kfc, KFCCLK uses mout_mx_mspll_kfc for lock time */
	if (clk_set_parent(mout_cpu_kfc, mout_mx_mspll_kfc))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_mx_mspll_kfc->name, mout_cpu_kfc->name);
	/* 1.1 CLKMUX_HPM_KFC = mout_mx_mspll_kfc, SCLKHPM uses mout_mx_mspll_kfc for lock time */
	if (clk_set_parent(mout_hpm_kfc, mout_mx_mspll_kfc))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_mx_mspll_kfc->name, mout_hpm_kfc->name);

	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS5_CLK_MUX_STAT_KFC)
			>> EXYNOS5_CLKSRC_KFC_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set KPLL Lock time */
	pdiv = ((exynos5422_kfc_pll_pms_table_CA7[new_index] >> 8) & 0x3f);

	__raw_writel((pdiv * 250), EXYNOS5_KPLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5_KPLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos5422_kfc_pll_pms_table_CA7[new_index];
	__raw_writel(tmp, EXYNOS5_KPLL_CON0);

	/* 4. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_KPLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5_KPLLCON0_LOCKED_SHIFT)));

	/* 5. CLKMUX_CPU_KFC = KPLL */
	if (clk_set_parent(mout_cpu_kfc, mout_kpll_ctrl))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_cpu_kfc->name, mout_kpll_ctrl->name);

	/* 5.1 CLKMUX_HPM_KFC = KPLL */
	if (clk_set_parent(mout_hpm_kfc, mout_kpll_ctrl))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_hpm_kfc->name, mout_kpll_ctrl->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5_CLK_MUX_STAT_KFC);
		tmp &= EXYNOS5_CLKMUX_STATKFC_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS5_CLKSRC_KFC_MUXCORE_SHIFT));

	/* 6. restore original div value */
	if ((new_index < L5) && (old_index < L5))
		exynos5422_set_clkdiv_CA7(new_index);
		
	clk_set_rate(fout_kpll, exynos5422_freq_table_CA7[new_index].frequency * 1000);
}

static bool exynos5422_pms_change_CA7(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos5422_kfc_pll_pms_table_CA7[old_index] >> 8);
	unsigned int new_pm = (exynos5422_kfc_pll_pms_table_CA7[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5422_set_frequency_CA7(unsigned int old_index,
					 unsigned int new_index)
{
unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5422_pms_change_CA7(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5422_set_clkdiv_CA7(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_KPLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5422_kfc_pll_pms_table_CA7[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_KPLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5422_set_clkdiv_CA7(new_index);
			/* 2. Change the apll m,p,s value */
			exynos5422_set_kfc_pll_CA7(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5422_pms_change_CA7(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_KPLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5422_kfc_pll_pms_table_CA7[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_KPLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5422_set_clkdiv_CA7(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the apll m,p,s value */
			exynos5422_set_kfc_pll_CA7(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5422_set_clkdiv_CA7(new_index);
		}
	}

	clk_set_rate(fout_kpll, exynos5422_freq_table_CA7[new_index].frequency * 1000);
	pr_debug("post clk [%ld]\n", clk_get_rate(dout_cpu_kfc));
}

static void __init set_volt_table_CA7(void)
{
	unsigned int i;
	unsigned int asv_volt __maybe_unused;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		/* FIXME: need to update voltage table for REV1 */
		asv_volt = get_match_volt(ID_KFC, exynos5422_freq_table_CA7[i].frequency);

		if (!asv_volt)
			exynos5422_volt_table_CA7[i] = asv_voltage_5422_CA7[i];
		else
			exynos5422_volt_table_CA7[i] = asv_volt;

		pr_info("CPUFREQ of CA7  L%d : %d uV\n", i,
			exynos5422_volt_table_CA7[i]);

		exynos5422_abb_table_CA7[i] =
			get_match_abb(ID_KFC, exynos5422_freq_table_CA7[i].frequency);

		pr_info("CPUFREQ of CA7  L%d : ABB %d\n", i,
				exynos5422_abb_table_CA7[i]);
	}
	
	/* A7's Max/Min Frequencies */
	if(exynos5422_tbl_ver_is_bin2())
		max_support_idx_CA7 = L3;
	else
		max_support_idx_CA7 = L2;
		
	if (PKG_ID_DVFS_VERSION == 0x02)
		min_support_idx_CA7 = L14;
	else
		min_support_idx_CA7 = L14;
}

static bool exynos5422_is_alive_CA7(void)
{
	unsigned int tmp = true;
	tmp = __raw_readl(EXYNOS54XX_KFC_COMMON_STATUS) & L2_LOCAL_PWR_EN;

	return tmp ? true : false;
}

int __init exynos5_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;
#ifdef CONFIG_PM_DEBUG
	is_init_time = 1;
#endif
	set_volt_table_CA7();

	fout_cpll = __clk_lookup("fout_cpll");
	if (IS_ERR(fout_cpll))
		goto err_get_clock;

	dout_cpu_kfc = __clk_lookup("dout_kfc");
	if (IS_ERR(dout_cpu_kfc))
		goto err_get_clock;

	dout_hpm_kfc = __clk_lookup("dout_hpm_kfc");
	if (IS_ERR(dout_hpm_kfc))
		goto err_get_clock;

	mout_cpu_kfc = __clk_lookup("mout_cpu_kfc");
	if (IS_ERR(mout_cpu_kfc))
		goto err_get_clock;

	mout_hpm_kfc = __clk_lookup("mout_hpm_kfc");
	if (IS_ERR(mout_hpm_kfc))
		goto err_get_clock;

	mout_mx_mspll_kfc = __clk_lookup("mout_mx_mspll_kfc");
	if (IS_ERR(mout_mx_mspll_kfc))
		goto err_get_clock;

	mout_cpll_ctrl = __clk_lookup("mout_cpll_ctrl");
	if (IS_ERR(mout_cpll_ctrl))
		goto err_get_clock;

	mout_kpll_ctrl = __clk_lookup("mout_kpll_ctrl");
	if (IS_ERR(mout_kpll_ctrl))
		goto err_get_clock;

	fout_kpll = __clk_lookup("fout_kpll");
	if (IS_ERR(mout_kpll_ctrl))
		goto err_get_clock;

	clk_set_parent(mout_kpll_ctrl, fout_kpll);
	clk_set_parent(mout_mx_mspll_kfc, mout_cpll_ctrl);
	clk_set_parent(mout_cpu_kfc, mout_kpll_ctrl);
	clk_set_parent(mout_hpm_kfc, mout_kpll_ctrl);

	rate = clk_get_rate(mout_mx_mspll_kfc) / 1000;
	pr_info("kfc clock[%ld], mx_mspll clock[%ld]\n", clk_get_rate(dout_cpu_kfc) / 1000, rate);

	for (i = L0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		exynos5422_clkdiv_table_CA7[i].index = i;

		tmp = __raw_readl(EXYNOS5_CLK_DIV_KFC0);

		tmp &= ~(EXYNOS5_CLKDIV_KFC0_CORE_MASK |
		EXYNOS5_CLKDIV_KFC0_ACLK_MASK |
		EXYNOS5_CLKDIV_KFC0_HPM_MASK |
		EXYNOS5_CLKDIV_KFC0_PCLK_MASK |
		EXYNOS5_CLKDIV_KFC0_KPLL_MASK);

		tmp |= ((clkdiv_cpu0_5422_CA7[i][0] << EXYNOS5_CLKDIV_KFC0_CORE_SHIFT) |
		(clkdiv_cpu0_5422_CA7[i][1] << EXYNOS5_CLKDIV_KFC0_ACLK_SHIFT) |
		(clkdiv_cpu0_5422_CA7[i][2] << EXYNOS5_CLKDIV_KFC0_HPM_SHIFT) |
		(clkdiv_cpu0_5422_CA7[i][3] << EXYNOS5_CLKDIV_KFC0_PCLK_SHIFT)|
		(clkdiv_cpu0_5422_CA7[i][4] << EXYNOS5_CLKDIV_KFC0_KPLL_SHIFT));

		exynos5422_clkdiv_table_CA7[i].clkdiv0 = tmp;
	}

	info->mpll_freq_khz = rate;
	/*info->pm_lock_idx = L0;*/
	info->pll_safe_idx = L5;
	info->max_support_idx = max_support_idx_CA7;
	info->min_support_idx = min_support_idx_CA7;

	info->boot_cpu_min_qos = exynos5422_freq_table_CA7[L2].frequency;
	info->boot_cpu_max_qos = exynos5422_freq_table_CA7[L2].frequency;
	info->cpu_clk = fout_kpll;
	info->bus_table = exynos5422_bus_table_CA7;

	/*info->max_op_freqs = exynos5422_max_op_freq_b_evt0;*/

	info->volt_table = exynos5422_volt_table_CA7;
	info->abb_table = exynos5422_abb_table_CA7;
	info->freq_table = exynos5422_freq_table_CA7;
	info->set_freq = exynos5422_set_frequency_CA7;
	info->need_apll_change = exynos5422_pms_change_CA7;
	info->is_alive = exynos5422_is_alive_CA7;
	info->set_ema = exynos5422_set_ema_CA7;


#ifdef ENABLE_CLKOUT
	tmp = __raw_readl(EXYNOS5_CLKOUT_CMU_KFC);
	tmp &= ~0xffff;
	tmp |= 0x1904;
	__raw_writel(tmp, EXYNOS5_CLKOUT_CMU_KFC);
#endif

#ifdef CONFIG_PM_DEBUG
	info->set_ema(EMA_VOLT_LEV_0 - 12500);
	info->set_ema(EMA_VOLT_LEV_1 - 12500);
	info->set_ema(EMA_VOLT_LEV_2 - 12500);
	info->set_ema(EMA_VOLT_LEV_3 - 12500);
	info->set_ema(EMA_VOLT_LEV_3 + 12500);
	is_init_time = 0;
#endif
	return 0;

err_get_clock:

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
