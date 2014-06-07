/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Hyunki Koo <hyunki00.koo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for pwm timer Clock Controller.
*/

#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>

#include "clk.h"

/* Each of the timers 0 through 5 go through the following
 * clock tree, with the inputs depending on the timers.
 *
 * pclk ---- [ prescaler 0 ] -+---> timer 0
 *			      +---> timer 1
 *
 * pclk ---- [ prescaler 1 ] -+---> timer 2
 *			      +---> timer 3
 *			      \---> timer 4
 *
 * Which are fed into the timers as so:
 *
 * prescaled 0 ---- [ div 2,4,8,16 ] ---\
 *				       [mux] -> timer 0 (tin)
 * tclk 0 ------------------------------/
 *
 * prescaled 0 ---- [ div 2,4,8,16 ] ---\
 *				       [mux] -> timer 1 (tin)
 * tclk 0 ------------------------------/
 *
 *
 * prescaled 1 ---- [ div 2,4,8,16 ] ---\
 *				       [mux] -> timer 2 (tin)
 * tclk 1 ------------------------------/
 *
 * prescaled 1 ---- [ div 2,4,8,16 ] ---\
 *				       [mux] -> timer 3 (tin)
 * tclk 1 ------------------------------/
 *
 * prescaled 1 ---- [ div 2,4,8, 16 ] --\
 *				       [mux] -> timer 4 (tin)
 * tclk 1 ------------------------------/
 *
 * Since the mux and the divider are tied together in the
 * same register space, it is impossible to set the parent
 * and the rate at the same time. To avoid this, we add an
 * intermediate 'prescaled-and-divided' clock to select
 * as the parent for the timer input clock called tdiv.
 *
 * prescaled clk --> pwm-tdiv ---\
 *                             [ mux ] --> timer X
 * tclk -------------------------/
 *
 * tclk is deprecated in exynos
 *
*/

static DEFINE_SPINLOCK(lock);
static struct clk **clk_table;
static struct clk_onecell_data clk_data;

#define REG_TCFG0			0x00
#define REG_TCFG1			0x04
#define REG_TCON			0x08
#define REG_TINT_CSTAT			0x44
#define MASK_TCFG0_PRESCALE0		0x00FF
#define MASK_TCFG0_PRESCALE1		0xFF00

enum exynos5430_pwm_clks {
	pwm_clock = 0,
	pwm_scaler0,
	pwm_scaler1,
	pwm_tclk0,
	pwm_tclk1,
	pwm_tdiv0 = 5,
	pwm_tdiv1,
	pwm_tdiv2,
	pwm_tdiv3,
	pwm_tdiv4,
	pwm_tin0 = 10,
	pwm_tin1,
	pwm_tin2,
	pwm_tin3,
	pwm_tin4,
	exynos_pwm_max_clks,
};

static const char *pwm_tin0_p[] = { "pwm-tdiv0", "pwm-tclk" };
static const char *pwm_tin1_p[] = { "pwm-tdiv1", "pwm-tclk" };
static const char *pwm_tin2_p[] = { "pwm-tdiv2", "pwm-tclk" };
static const char *pwm_tin3_p[] = { "pwm-tdiv3", "pwm-tclk" };
static const char *pwm_tin4_p[] = { "pwm-tdiv4", "pwm-tclk" };

static const struct clk_div_table pwm_div_table[5] = {
	/* { val, div } */
	{ 0, 1 },
	{ 1, 2 },
	{ 2, 4 },
	{ 3, 8 },
	{ 4, 16 },
};

/* register exynos_pwm clocks */
void __init exynos_pwm_clk_init(struct device_node *np)
{
	static void __iomem *reg_base;
	unsigned int reg_tcfg0;

	reg_base = of_iomap(np, 0);

	if (!reg_base) {
		pr_err("%s: failed to map pwm registers\n", __func__);
		return;
	}

	clk_table = kzalloc(sizeof(struct clk *) * exynos_pwm_max_clks,
				GFP_KERNEL);
	if (!clk_table) {
		pr_err("%s: could not allocate clk lookup table\n", __func__);
		return;
	}

	clk_data.clks = clk_table;
	clk_data.clk_num = exynos_pwm_max_clks;
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);

	reg_tcfg0 = __raw_readl(reg_base + REG_TCFG0);
	reg_tcfg0 &= ~(MASK_TCFG0_PRESCALE0 | MASK_TCFG0_PRESCALE1);
	__raw_writel(reg_tcfg0, reg_base + REG_TCFG0);

	clk_table[pwm_scaler0] = clk_register_divider(NULL, "pwm-scaler0",
				"pwm-clock", 0, reg_base + REG_TCFG0, 0, 8,
				0, &lock);
	clk_table[pwm_scaler1] = clk_register_divider(NULL, "pwm-scaler1",
				"pwm-clock", 0, reg_base + REG_TCFG0, 8, 8,
				0, &lock);

	clk_table[pwm_tdiv0] = clk_register_divider_table(NULL, "pwm-tdiv0",
				"pwm-scaler0", 0, reg_base + REG_TCFG1, 0, 4,
				0, pwm_div_table , &lock);

	clk_table[pwm_tdiv1] = clk_register_divider_table(NULL, "pwm-tdiv1",
				"pwm-scaler0", 0, reg_base + REG_TCFG1, 4, 4,
				0, pwm_div_table , &lock);

	clk_table[pwm_tdiv2] = clk_register_divider_table(NULL, "pwm-tdiv2",
				"pwm-scaler1", 0, reg_base + REG_TCFG1, 8, 4,
				0, pwm_div_table , &lock);

	clk_table[pwm_tdiv3] = clk_register_divider_table(NULL, "pwm-tdiv3",
				"pwm-scaler1", 0, reg_base + REG_TCFG1, 12, 4,
				0, pwm_div_table , &lock);

	clk_table[pwm_tdiv4] = clk_register_divider_table(NULL, "pwm-tdiv4",
				"pwm-scaler1", 0, reg_base + REG_TCFG1, 16, 4,
				0, pwm_div_table , &lock);

	#if defined (CONFIG_SOC_EXYNOS5430_REV_1)
	clk_table[pwm_tin0] = clk_register_mux(NULL, "pwm-tin0",
				pwm_tin0_p, ARRAY_SIZE(pwm_tin0_p), 0,
				reg_base + REG_TCFG1, 3, 0, 0, &lock, 0, 0, 0);

	clk_table[pwm_tin1] = clk_register_mux(NULL, "pwm-tin1",
				pwm_tin1_p, ARRAY_SIZE(pwm_tin1_p), 0,
				reg_base + REG_TCFG1, 7, 0, 0, &lock, 0, 0, 0);

	clk_table[pwm_tin2] = clk_register_mux(NULL, "pwm-tin2",
				pwm_tin2_p, ARRAY_SIZE(pwm_tin2_p), 0,
				reg_base + REG_TCFG1, 11, 0, 0, &lock, 0, 0, 0);

	clk_table[pwm_tin3] = clk_register_mux(NULL, "pwm-tin3",
				pwm_tin3_p, ARRAY_SIZE(pwm_tin3_p), 0,
				reg_base + REG_TCFG1, 15, 0, 0, &lock, 0, 0, 0);

	clk_table[pwm_tin4] = clk_register_mux(NULL, "pwm-tin4",
				pwm_tin4_p, ARRAY_SIZE(pwm_tin4_p), 0,
				reg_base + REG_TCFG1, 19, 0, 0, &lock, 0, 0, 0);
	#else
	clk_table[pwm_tin0] = clk_register_mux(NULL, "pwm-tin0",
				pwm_tin0_p, ARRAY_SIZE(pwm_tin0_p), 0,
				reg_base + REG_TCFG1, 3, 0, 0, &lock);

	clk_table[pwm_tin1] = clk_register_mux(NULL, "pwm-tin1",
				pwm_tin1_p, ARRAY_SIZE(pwm_tin1_p), 0,
				reg_base + REG_TCFG1, 7, 0, 0, &lock);

	clk_table[pwm_tin2] = clk_register_mux(NULL, "pwm-tin2",
				pwm_tin2_p, ARRAY_SIZE(pwm_tin2_p), 0,
				reg_base + REG_TCFG1, 11, 0, 0, &lock);

	clk_table[pwm_tin3] = clk_register_mux(NULL, "pwm-tin3",
				pwm_tin3_p, ARRAY_SIZE(pwm_tin3_p), 0,
				reg_base + REG_TCFG1, 15, 0, 0, &lock);

	clk_table[pwm_tin4] = clk_register_mux(NULL, "pwm-tin4",
				pwm_tin4_p, ARRAY_SIZE(pwm_tin4_p), 0,
				reg_base + REG_TCFG1, 19, 0, 0, &lock);
	#endif

	pr_info("Exynos: pwm: clock setup completed\n");
}
CLK_OF_DECLARE(exynos_pwm_clk, "samsung,exynos-pwm-clock",
		exynos_pwm_clk_init);
