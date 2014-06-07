/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5430 - KFC Core frequency scaling support
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

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-clock-exynos5430.h>
#include <mach/regs-pmu.h>
#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>

#define CPUFREQ_LEVEL_END_CA7	(L18 + 1)
#define L2_LOCAL_PWR_EN		0x7

#undef PRINT_DIV_VAL
#undef ENABLE_CLKOUT

static int max_support_idx_CA7;
static int min_support_idx_CA7 = (CPUFREQ_LEVEL_END_CA7 - 1);

static struct clk *mout_kfc;
static struct clk *mout_kfc_pll;
static struct clk *sclk_bus_pll;
static struct clk *mout_bus_pll_user;
static struct clk *fout_kfc_pll;

static unsigned int exynos5430_volt_table_CA7[CPUFREQ_LEVEL_END_CA7];

static struct cpufreq_frequency_table exynos5430_freq_table_CA7[] = {
	{L0,  2000 * 1000},
	{L1,  1900 * 1000},
	{L2,  1800 * 1000},
	{L3,  1700 * 1000},
	{L4,  1600 * 1000},
	{L5,  1500 * 1000},
	{L6,  1400 * 1000},
	{L7,  1300 * 1000},
	{L8,  1200 * 1000},
	{L9,  1100 * 1000},
	{L10, 1000 * 1000},
	{L11,  900 * 1000},
	{L12,  800 * 1000},
	{L13,  700 * 1000},
	{L14,  600 * 1000},
	{L15,  500 * 1000},
	{L16,  400 * 1000},
	{L17,  300 * 1000},
	{L18,  200 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5430_clkdiv_table_CA7[CPUFREQ_LEVEL_END_CA7];

#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
static unsigned int clkdiv_cpu0_5430_CA7[CPUFREQ_LEVEL_END_CA7][7] = {
	/*
	 * Clock divider value for following
	 * { KFC1, KFC2, ACLK_KFC, PCLK_KFC,
	 *   ATCLK, PCLK_DBG_KFC, SCLK_CNTCLK }
	 */

	/* ARM L0: 2.0GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L1: 1.9GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L2: 1.8GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L3: 1.7GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L4: 1.6GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L5: 1.5GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L6: 1.4GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L7: 1.3GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L8: 1.2GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L9: 1.1GHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L10: 1000MHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L11: 900MHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L12: 800MHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L13: 700MHz */
	{ 0, 0, 1, 6, 6, 6, 3 },

	/* ARM L14: 600MHz */
	{ 0, 0, 1, 6, 6, 6, 3 },

	/* ARM L15: 500MHz */
	{ 0, 0, 1, 5, 5, 5, 3 },

	/* ARM L16: 400MHz */
	{ 0, 0, 1, 5, 5, 5, 3 },

	/* ARM L17: 300MHz */
	{ 0, 0, 1, 4, 4, 4, 3 },

	/* ARM L18: 200MHz */
	{ 0, 0, 1, 2, 2, 2, 3 },
};

static unsigned int clkdiv_cpu1_5430_CA7[CPUFREQ_LEVEL_END_CA7][2] = {
	/*
	 * Clock divider value for following
	 * { SCLK_KFC_PLL, SCLK_HPM_KFC }
	 */

	/* ARM L0: 2.0GHz */
	{ 1, 7 },

	/* ARM L1: 1.9GMHz */
	{ 1, 7 },

	/* ARM L2: 1.8GMHz */
	{ 1, 7 },

	/* ARM L3: 1.7GHz */
	{ 1, 7 },

	/* ARM L4: 1.6GHz */
	{ 1, 7 },

	/* ARM L5: 1.5GMHz */
	{ 1, 7 },

	/* ARM L6: 1.4GMHz */
	{ 1, 7 },

	/* ARM L7: 1.3GHz */
	{ 1, 7 },

	/* ARM L8: 1.2GHz */
	{ 1, 7 },

	/* ARM L9: 1.1GHz */
	{ 1, 7 },

	/* ARM L10: 1000MHz */
	{ 1, 7 },

	/* ARM L11: 900MHz */
	{ 1, 7 },

	/* ARM L12: 800MHz */
	{ 1, 7 },

	/* ARM L13: 700MHz */
	{ 1, 7 },

	/* ARM L14: 600MHz */
	{ 1, 7 },

	/* ARM L15: 500MHz */
	{ 1, 7 },

	/* ARM L16: 400MHz */
	{ 1, 7 },

	/* ARM L17: 300MHz */
	{ 1, 7 },

	/* ARM L18: 200MHz */
	{ 1, 7 },
};

static unsigned int exynos5430_kfc_pll_pms_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	/* MDIV | PDIV | SDIV */
	/* KPLL FOUT L0: 2.0GHz */
	PLL2450X_PMS(500, 6, 0),

	/* KPLL FOUT L1: 1.9GHz */
	PLL2450X_PMS(475, 6, 0),

	/* KPLL FOUT L2: 1.8GHz */
	PLL2450X_PMS(375, 5, 0),

	/* KPLL FOUT L3: 1.7GHz */
	PLL2450X_PMS(425, 6, 0),

	/* KPLL FOUT L4: 1.6GHz */
	PLL2450X_PMS(400, 6, 0),

	/* KPLL FOUT L5: 1.5GHz */
	PLL2450X_PMS(250, 4, 0),

	/* KPLL FOUT L6: 1.4GHz */
	PLL2450X_PMS(350, 6, 0),

	/* KPLL FOUT L7: 1.3GHz */
	PLL2450X_PMS(325, 6, 0),

	/* KPLL FOUT L8: 1.2GHz */
	PLL2450X_PMS(500, 5, 1),

	/* KPLL FOUT L9: 1.1GHz */
	PLL2450X_PMS(550, 6, 1),

	/* KPLL FOUT L10: 1000MHz */
	PLL2450X_PMS(500, 6, 1),

	/* KPLL FOUT L11: 900MHz */
	PLL2450X_PMS(375, 5, 1),

	/* KPLL FOUT L12: 800MHz */
	PLL2450X_PMS(400, 6, 1),

	/* KPLL FOUT L13: 700MHz */
	PLL2450X_PMS(350, 6, 1),

	/* KPLL FOUT L14: 600MHz */
	PLL2450X_PMS(500, 5, 2),

	/* KPLL FOUT L15: 500MHz */
	PLL2450X_PMS(500, 6, 2),

	/* KPLL FOUT L16: 400MHz */
	PLL2450X_PMS(400, 6, 2),

	/* KPLL FOUT L17: 300MHz */
	PLL2450X_PMS(500, 5, 3),

	/* KPLL FOUT L18: 200MHz */
	PLL2450X_PMS(400, 6, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5430_CA7[CPUFREQ_LEVEL_END_CA7] = {
	1225000,	/* L0  2000 */
	1225000,	/* L1  1900 */
	1225000,	/* L2  1800 */
	1225000,	/* L3  1700 */
	1225000,	/* L4  1600 */
	1225000,	/* L5  1500 */
	1187500,	/* L6  1400 */
	1150000,	/* L7  1300 */
	1112500,	/* L8  1200 */
	1075000,	/* L9  1100 */
	1037500,	/* L10 1000 */
	1000000,	/* L11  900 */
	 975000,	/* L12  800 */
	 950000,	/* L13  700 */
	 925000,	/* L14  600 */
	 925000,	/* L15  500 */
	 925000,	/* L16  400 */
	 925000,	/* L17  300 */
	 925000,	/* L18  200 */
};
#else
static unsigned int clkdiv_cpu0_5430_CA7[CPUFREQ_LEVEL_END_CA7][7] = {
	/*
	 * Clock divider value for following
	 * { KFC1, KFC2, ACLK_KFC, PCLK_KFC,
	 *   ATCLK, PCLK_DBG_KFC, SCLK_CNTCLK }
	 */

	/* ARM L0: 2.0GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L1: 1.9GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L2: 1.8GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L3: 1.7GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L4: 1.6GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L5: 1.5GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L6: 1.4GMHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L7: 1.3GHz */
	{ 0, 0, 2, 7, 7, 7, 3 },

	/* ARM L8: 1.2GHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L9: 1.1GHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L10: 1000MHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L11: 900MHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L12: 800MHz */
	{ 0, 0, 1, 7, 7, 7, 3 },

	/* ARM L13: 700MHz */
	{ 0, 0, 1, 6, 6, 6, 3 },

	/* ARM L14: 600MHz */
	{ 0, 0, 1, 6, 6, 6, 3 },

	/* ARM L15: 500MHz */
	{ 0, 0, 1, 5, 5, 5, 3 },

	/* ARM L16: 400MHz */
	{ 0, 0, 1, 5, 5, 5, 3 },

	/* ARM L17: 300MHz */
	{ 0, 0, 1, 4, 4, 4, 3 },

	/* ARM L18: 200MHz */
	{ 0, 0, 0, 2, 2, 2, 3 },
};

static unsigned int clkdiv_cpu1_5430_CA7[CPUFREQ_LEVEL_END_CA7][2] = {
	/*
	 * Clock divider value for following
	 * { SCLK_KFC_PLL, SCLK_HPM_KFC }
	 */

	/* ARM L0: 2.0GHz */
	{ 0, 7 },

	/* ARM L1: 1.9GMHz */
	{ 0, 7 },

	/* ARM L2: 1.8GMHz */
	{ 0, 7 },

	/* ARM L3: 1.7GHz */
	{ 0, 7 },

	/* ARM L4: 1.6GHz */
	{ 0, 7 },

	/* ARM L5: 1.5GMHz */
	{ 0, 7 },

	/* ARM L6: 1.4GMHz */
	{ 0, 7 },

	/* ARM L7: 1.3GHz */
	{ 0, 7 },

	/* ARM L8: 1.2GHz */
	{ 0, 7 },

	/* ARM L9: 1.1GHz */
	{ 0, 7 },

	/* ARM L10: 1000MHz */
	{ 0, 7 },

	/* ARM L11: 900MHz */
	{ 0, 7 },

	/* ARM L12: 800MHz */
	{ 0, 7 },

	/* ARM L13: 700MHz */
	{ 0, 7 },

	/* ARM L14: 600MHz */
	{ 0, 7 },

	/* ARM L15: 500MHz */
	{ 0, 7 },

	/* ARM L16: 400MHz */
	{ 0, 7 },

	/* ARM L17: 300MHz */
	{ 0, 7 },

	/* ARM L18: 200MHz */
	{ 0, 7 },
};

static unsigned int exynos5430_kfc_pll_pms_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	/* MDIV | PDIV | SDIV */
	/* KPLL FOUT L0: 2.0GHz */
	PLL2450X_PMS(500, 6, 0),

	/* KPLL FOUT L1: 1.9GHz */
	PLL2450X_PMS(475, 6, 0),

	/* KPLL FOUT L2: 1.8GHz */
	PLL2450X_PMS(375, 5, 0),

	/* KPLL FOUT L3: 1.7GHz */
	PLL2450X_PMS(425, 6, 0),

	/* KPLL FOUT L4: 1.6GHz */
	PLL2450X_PMS(400, 6, 0),

	/* KPLL FOUT L5: 1.5GHz */
	PLL2450X_PMS(250, 4, 0),

	/* KPLL FOUT L6: 1.4GHz */
	PLL2450X_PMS(350, 6, 0),

	/* KPLL FOUT L7: 1.3GHz */
	PLL2450X_PMS(325, 6, 0),

	/* KPLL FOUT L8: 1.2GHz */
	PLL2450X_PMS(500, 5, 1),

	/* KPLL FOUT L9: 1.1GHz */
	PLL2450X_PMS(550, 6, 1),

	/* KPLL FOUT L10: 1000MHz */
	PLL2450X_PMS(500, 6, 1),

	/* KPLL FOUT L11: 900MHz */
	PLL2450X_PMS(375, 5, 1),

	/* KPLL FOUT L12: 800MHz */
	PLL2450X_PMS(400, 6, 1),

	/* KPLL FOUT L13: 700MHz */
	PLL2450X_PMS(350, 6, 1),

	/* KPLL FOUT L14: 600MHz */
	PLL2450X_PMS(500, 5, 2),

	/* KPLL FOUT L15: 500MHz */
	PLL2450X_PMS(500, 6, 2),

	/* KPLL FOUT L16: 400MHz */
	PLL2450X_PMS(400, 6, 2),

	/* KPLL FOUT L17: 300MHz */
	PLL2450X_PMS(500, 5, 3),

	/* KPLL FOUT L18: 200MHz */
	PLL2450X_PMS(400, 6, 3),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_5430_CA7[CPUFREQ_LEVEL_END_CA7] = {
	1225000,	/* L0  2000 */
	1225000,	/* L1  1900 */
	1225000,	/* L2  1800 */
	1225000,	/* L3  1700 */
	1225000,	/* L4  1600 */
	1225000,	/* L5  1500 */
	1187500,	/* L6  1400 */
	1150000,	/* L7  1300 */
	1112500,	/* L8  1200 */
	1075000,	/* L9  1100 */
	1037500,	/* L10 1000 */
	1000000,	/* L11  900 */
	 962500,	/* L12  800 */
	 925000,	/* L13  700 */
	 900000,	/* L14  600 */
	 900000,	/* L15  500 */
	 900000,	/* L16  400 */
	 900000,	/* L17  300 */
	 900000,	/* L18  200 */
};
#endif

/* Minimum memory throughput in megabytes per second */
static int exynos5430_bus_table_CA7[CPUFREQ_LEVEL_END_CA7] = {
	413000,		/* 2.0 GHz */
	413000,		/* 1.9 GHz */
	413000,		/* 1.8 GHz */
	413000,		/* 1.7 GHz */
	413000,		/* 1.6 GHz */
	275000,		/* 1.5 GHz */
	275000,		/* 1.4 GHz */
	275000,		/* 1.3 GHz */
	206000,		/* 1.2 GHz */
	206000,		/* 1.1 GHz */
	206000,		/* 1.0 GHz */
	165000,		/* 900 MHz */
	165000,		/* 800 MHz */
	138000,		/* 700 MHz */
	138000,		/* 600 MHz */
	0,		/* 500 MHz */
	0,		/* 400 MHz */
	0,		/* 300 MHz */
	0,		/* 200 MHz */
};

static void exynos5430_set_clkdiv_CA7(unsigned int div_index)
{
	unsigned int tmp, tmp1;

	/* Change Divider - KFC0 */
	tmp = exynos5430_clkdiv_table_CA7[div_index].clkdiv0;

	__raw_writel(tmp, EXYNOS5430_DIV_KFC0);

	while (__raw_readl(EXYNOS5430_DIV_STAT_KFC0) & 0x1111111)
		cpu_relax();

	/* Change Divider - KFC1 */
	tmp1 = exynos5430_clkdiv_table_CA7[div_index].clkdiv1;

	__raw_writel(tmp1, EXYNOS5430_DIV_KFC1);

	while (__raw_readl(EXYNOS5430_DIV_STAT_KFC1) & 0x11)
		cpu_relax();

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5430_DIV_KFC0);
	tmp1 = __raw_readl(EXYNOS5430_DIV_KFC1);
	pr_info("%s: DIV_KFC0[0x%08x], DIV_KFC1[0x%08x]\n",
					__func__, tmp, tmp1);
#endif
}

static void exynos5430_set_kfc_pll_CA7(unsigned int new_index, unsigned int old_index)
{
	unsigned int tmp, pdiv;

	/* 1. before change to BUS_PLL, set div for BUS_PLL output */
	if ((new_index < L12) && (old_index < L12))
		exynos5430_set_clkdiv_CA7(L12); /* pll_safe_idx of CA7 */

	/* 2. CLKMUX_SEL_KFC2 = MOUT_BUS_PLL_USER, KFCCLK uses BUS_PLL_USER for lock time */
	if (clk_set_parent(mout_kfc, mout_bus_pll_user))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bus_pll_user->name, mout_kfc->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_KFC2);
		tmp &= EXYNOS5430_SRC_STAT_KFC2_KFC_MASK;
	} while (tmp != 0x2);

	/* 3. Set KFC_PLL Lock time */
	pdiv = ((exynos5430_kfc_pll_pms_table_CA7[new_index] &
		EXYNOS5430_PLL_PDIV_MASK) >> EXYNOS5430_PLL_PDIV_SHIFT);

	__raw_writel((pdiv * 150), EXYNOS5430_KFC_PLL_LOCK);

	/* 4. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
	tmp &= ~(EXYNOS5430_PLL_MDIV_MASK |
		EXYNOS5430_PLL_PDIV_MASK |
		EXYNOS5430_PLL_SDIV_MASK);
	tmp |= exynos5430_kfc_pll_pms_table_CA7[new_index];
	__raw_writel(tmp, EXYNOS5430_KFC_PLL_CON0);

	/* 5. wait_lock_time */
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS5430_KFC_PLL_CON0_LOCKED_SHIFT)));

	/* 6. CLKMUX_SEL_KFC2 = MOUT_KFC_PLL */
	if (clk_set_parent(mout_kfc, mout_kfc_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_kfc_pll->name, mout_kfc->name);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS5430_SRC_STAT_KFC2);
		tmp &= EXYNOS5430_SRC_STAT_KFC2_KFC_MASK;
	} while (tmp != 0x1);

	/* 7. restore original div value */
	if ((new_index < L12) && (old_index < L12))
		exynos5430_set_clkdiv_CA7(new_index);
}

static bool exynos5430_pms_change_CA7(unsigned int old_index,
				      unsigned int new_index)
{
	unsigned int old_pm = (exynos5430_kfc_pll_pms_table_CA7[old_index] >>
				EXYNOS5430_PLL_PDIV_SHIFT);
	unsigned int new_pm = (exynos5430_kfc_pll_pms_table_CA7[new_index] >>
				EXYNOS5430_PLL_PDIV_SHIFT);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos5430_set_frequency_CA7(unsigned int old_index,
					 unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos5430_pms_change_CA7(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			exynos5430_set_clkdiv_CA7(new_index);
			/* 2. Change just s value in kfc_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5430_kfc_pll_pms_table_CA7[new_index] & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_KFC_PLL_CON0);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			exynos5430_set_clkdiv_CA7(new_index);
			/* 2. Change the kfc_pll m,p,s value */
			exynos5430_set_kfc_pll_CA7(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5430_pms_change_CA7(old_index, new_index)) {
			/* 1. Change just s value in kfc_pll m,p,s value */
			tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON0);
			tmp &= ~EXYNOS5430_PLL_SDIV_MASK;
			tmp |= (exynos5430_kfc_pll_pms_table_CA7[new_index] & EXYNOS5430_PLL_SDIV_MASK);
			__raw_writel(tmp, EXYNOS5430_KFC_PLL_CON0);
			/* 2. Change the system clock divider values */
			exynos5430_set_clkdiv_CA7(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the kfc_pll m,p,s value */
			exynos5430_set_kfc_pll_CA7(new_index, old_index);
			/* 2. Change the system clock divider values */
			exynos5430_set_clkdiv_CA7(new_index);
		}
	}

	clk_set_rate(fout_kfc_pll, exynos5430_freq_table_CA7[new_index].frequency * 1000);
}

static void __init set_volt_table_CA7(void)
{
	unsigned int i;
	unsigned int asv_volt = 0;

	for (i = 0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		asv_volt = get_match_volt(ID_KFC, exynos5430_freq_table_CA7[i].frequency);
		if (!asv_volt)
			exynos5430_volt_table_CA7[i] = asv_voltage_5430_CA7[i];
		else
			exynos5430_volt_table_CA7[i] = asv_volt;

		pr_info("CPUFREQ of CA7  L%d : %d uV\n", i,
				exynos5430_volt_table_CA7[i]);
	}

#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
	max_support_idx_CA7 = L5;	/* 1.5GHz */
	min_support_idx_CA7 = L15;	/* 500MHz */
#else
	max_support_idx_CA7 = L5;
	min_support_idx_CA7 = L18;
#endif
}

static bool exynos5430_is_alive_CA7(void)
{
	unsigned int tmp;

	tmp = __raw_readl(EXYNOS5430_KFC_PLL_CON1);
	tmp &= EXYNOS5430_PLL_BYPASS_MASK;
	tmp >>= EXYNOS5430_PLL_BYPASS_SHIFT;

	return !tmp ? true : false;
}

int __init exynos5_cpufreq_CA7_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table_CA7();

	mout_kfc = __clk_lookup("mout_kfc");
	if (!mout_kfc) {
		pr_err("failed get mout_kfc clk\n");
		return -EINVAL;
	}

	mout_kfc_pll = __clk_lookup("mout_kfc_pll");
	if (!mout_kfc_pll) {
		pr_err("failed get mout_kfc_pll clk\n");
		goto err_mout_kfc_pll;
	}

#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
	sclk_bus_pll = __clk_lookup("sclk_bus_pll");
	if (!sclk_bus_pll) {
		pr_err("failed get sclk_bus_pll clk\n");
		goto err_sclk_bus_pll;
	}

	mout_bus_pll_user = __clk_lookup("mout_bus_pll_kfc_user");
	if (!mout_bus_pll_user) {
		pr_err("failed get mout_bus_pll_kfc_user clk\n");
		goto err_mout_bus_pll_user;
	}
#else
	sclk_bus_pll = __clk_lookup("mout_bus_pll_sub");
	if (!sclk_bus_pll) {
		pr_err("failed get mout_bus_pll_sub clk\n");
		goto err_sclk_bus_pll;
	}

	mout_bus_pll_user = __clk_lookup("mout_bus_pll_user_kfc");
	if (!mout_bus_pll_user) {
		pr_err("failed get mout_bus_pll_user_kfc clk\n");
		goto err_mout_bus_pll_user;
	}
#endif

	if (clk_set_parent(mout_bus_pll_user, sclk_bus_pll)) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_bus_pll->name, mout_bus_pll_user->name);
		goto err_clk_set_parent;
	}

	rate = clk_get_rate(mout_bus_pll_user) / 1000;

	fout_kfc_pll = __clk_lookup("fout_kfc_pll");
	if (!fout_kfc_pll) {
		pr_err("failed get fout_kfc_pll clk\n");
		goto err_fout_kfc_pll;
	}

	clk_put(sclk_bus_pll);

	for (i = L0; i < CPUFREQ_LEVEL_END_CA7; i++) {
		exynos5430_clkdiv_table_CA7[i].index = i;

		/* CLK_DIV_KFC0 */
		tmp = __raw_readl(EXYNOS5430_DIV_KFC0);

		tmp &= ~(EXYNOS5430_DIV_KFC0_KFC1_MASK |
			EXYNOS5430_DIV_KFC0_KFC2_MASK |
			EXYNOS5430_DIV_KFC0_ACLK_KFC_MASK |
			EXYNOS5430_DIV_KFC0_PCLK_KFC_MASK |
			EXYNOS5430_DIV_KFC0_ATCLK_KFC_MASK |
			EXYNOS5430_DIV_KFC0_PCLK_DBG_KFC_MASK |
			EXYNOS5430_DIV_KFC0_CNTCLK_KFC_MASK);

		tmp |= ((clkdiv_cpu0_5430_CA7[i][0] << EXYNOS5430_DIV_KFC0_KFC1_SHIFT) |
			(clkdiv_cpu0_5430_CA7[i][1] << EXYNOS5430_DIV_KFC0_KFC2_SHIFT) |
			(clkdiv_cpu0_5430_CA7[i][2] << EXYNOS5430_DIV_KFC0_ACLK_KFC_SHIFT) |
			(clkdiv_cpu0_5430_CA7[i][3] << EXYNOS5430_DIV_KFC0_PCLK_KFC_SHIFT) |
			(clkdiv_cpu0_5430_CA7[i][4] << EXYNOS5430_DIV_KFC0_ATCLK_KFC_SHIFT) |
			(clkdiv_cpu0_5430_CA7[i][5] << EXYNOS5430_DIV_KFC0_PCLK_DBG_KFC_SHIFT) |
			(clkdiv_cpu0_5430_CA7[i][6] << EXYNOS5430_DIV_KFC0_CNTCLK_KFC_SHIFT));

		exynos5430_clkdiv_table_CA7[i].clkdiv0 = tmp;

		/* CLK_DIV_KFC1 */
		tmp = __raw_readl(EXYNOS5430_DIV_KFC1);

		tmp &= ~(EXYNOS5430_DIV_KFC1_KFC_PLL_MASK |
			EXYNOS5430_DIV_KFC1_SCLK_HPM_KFC_MASK);

		tmp |= ((clkdiv_cpu1_5430_CA7[i][0] << EXYNOS5430_DIV_KFC1_KFC_PLL_SHIFT) |
			(clkdiv_cpu1_5430_CA7[i][1] << EXYNOS5430_DIV_KFC1_SCLK_HPM_KFC_SHIFT));

		exynos5430_clkdiv_table_CA7[i].clkdiv1 = tmp;
	}

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L12;
	info->max_support_idx = max_support_idx_CA7;
	info->min_support_idx = min_support_idx_CA7;
#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
	/* booting frequency is 1.3GHz */
	info->boot_cpu_min_qos = exynos5430_freq_table_CA7[L7].frequency;
	info->boot_cpu_max_qos = exynos5430_freq_table_CA7[L7].frequency;
#else
	info->boot_cpu_min_qos = exynos5430_freq_table_CA7[L5].frequency;
	info->boot_cpu_max_qos = exynos5430_freq_table_CA7[L5].frequency;
#endif
	info->bus_table = exynos5430_bus_table_CA7;
	info->cpu_clk = fout_kfc_pll;

	info->volt_table = exynos5430_volt_table_CA7;
	info->freq_table = exynos5430_freq_table_CA7;
	info->set_freq = exynos5430_set_frequency_CA7;
	info->need_apll_change = exynos5430_pms_change_CA7;
	info->is_alive = exynos5430_is_alive_CA7;

#ifdef ENABLE_CLKOUT
	/* dividing KFC_CLK to 1/16 */
	tmp = __raw_readl(EXYNOS5430_CLKOUT_CMU_KFC);
	tmp &= ~0xfff;
	tmp |= 0xf01;
	__raw_writel(tmp, EXYNOS5430_CLKOUT_CMU_KFC);
#endif

	return 0;

err_fout_kfc_pll:
err_clk_set_parent:
	clk_put(mout_bus_pll_user);
err_mout_bus_pll_user:
	clk_put(sclk_bus_pll);
err_sclk_bus_pll:
	clk_put(mout_kfc_pll);
err_mout_kfc_pll:
	clk_put(mout_kfc);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
