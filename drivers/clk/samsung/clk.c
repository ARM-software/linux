/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file includes utility functions to register clocks to common
 * clock framework for Samsung platforms.
*/

#include <linux/syscore_ops.h>
#include "clk.h"

static DEFINE_SPINLOCK(lock);
static struct clk **clk_table;
static void __iomem *reg_base;
#ifdef CONFIG_OF
static struct clk_onecell_data clk_data;
#endif

#ifdef CONFIG_PM_SLEEP
static struct samsung_clk_reg_dump *reg_dump;
static unsigned long nr_reg_dump;

static int samsung_clk_suspend(void)
{
	struct samsung_clk_reg_dump *rd = reg_dump;
	unsigned long i;

	for (i = 0; i < nr_reg_dump; i++, rd++)
		rd->value = __raw_readl(reg_base + rd->offset);

	return 0;
}

static void samsung_clk_resume(void)
{
	struct samsung_clk_reg_dump *rd = reg_dump;
	unsigned long i;

	for (i = 0; i < nr_reg_dump; i++, rd++)
		__raw_writel(rd->value, reg_base + rd->offset);
}

static struct syscore_ops samsung_clk_syscore_ops = {
	.suspend	= samsung_clk_suspend,
	.resume		= samsung_clk_resume,
};
#endif /* CONFIG_PM_SLEEP */

/* setup the essentials required to support clock lookup using ccf */
void __init samsung_clk_init(struct device_node *np, void __iomem *base,
		unsigned long nr_clks, unsigned long *rdump,
		unsigned long nr_rdump, unsigned long *soc_rdump,
		unsigned long nr_soc_rdump)
{
	reg_base = base;
	if (!np)
		return;

#ifdef CONFIG_OF
	clk_table = kzalloc(sizeof(struct clk *) * nr_clks, GFP_KERNEL);
	if (!clk_table)
		panic("could not allocate clock lookup table\n");

	clk_data.clks = clk_table;
	clk_data.clk_num = nr_clks;
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);
#endif

#ifdef CONFIG_PM_SLEEP
	if (rdump && nr_rdump) {
		unsigned int idx;
		reg_dump = kzalloc(sizeof(struct samsung_clk_reg_dump)
				* (nr_rdump + nr_soc_rdump), GFP_KERNEL);
		if (!reg_dump) {
			pr_err("%s: memory alloc for register dump failed\n",
					__func__);
			return;
		}

		for (idx = 0; idx < nr_rdump; idx++)
			reg_dump[idx].offset = rdump[idx];
		for (idx = 0; idx < nr_soc_rdump; idx++)
			reg_dump[nr_rdump + idx].offset = soc_rdump[idx];
		nr_reg_dump = nr_rdump + nr_soc_rdump;
		register_syscore_ops(&samsung_clk_syscore_ops);
	}
#endif
}

/* add a clock instance to the clock lookup table used for dt based lookup */
void samsung_clk_add_lookup(struct clk *clk, unsigned int id)
{
	if (clk_table && id)
		clk_table[id] = clk;
}

/* register a list of fixed clocks */
void __init samsung_clk_register_fixed_rate(
		struct samsung_fixed_rate_clock *list, unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_fixed_rate(NULL, list->name,
			list->parent_name, list->flags, list->fixed_rate);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(clk, list->id);

		/*
		 * Unconditionally add a clock lookup for the fixed rate clocks.
		 * There are not many of these on any of Samsung platforms.
		 */
		ret = clk_register_clkdev(clk, list->name, NULL);
		if (ret)
			pr_err("%s: failed to register clock lookup for %s",
				__func__, list->name);
	}
}

/* register a list of fixed factor clocks */
void __init samsung_clk_register_fixed_factor(
		struct samsung_fixed_factor_clock *list, unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_fixed_factor(NULL, list->name,
			list->parent_name, list->flags, list->mult, list->div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(clk, list->id);
	}
}

/* register a list of mux clocks */
void __init samsung_clk_register_mux(struct samsung_mux_clock *list,
					unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		#if defined (CONFIG_SOC_EXYNOS5430_REV_1)
		clk = clk_register_mux(NULL, list->name, list->parent_names,
			list->num_parents, list->flags, reg_base + list->offset,
			list->shift, list->width, list->mux_flags, &lock,
			reg_base + list -> stat_offset, list->stat_shift, list->stat_width);
		#else
		clk = clk_register_mux(NULL, list->name, list->parent_names,
			list->num_parents, list->flags, reg_base + list->offset,
			list->shift, list->width, list->mux_flags, &lock);
		#endif
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(clk, list->id);

		/* register a clock lookup only if a clock alias is specified */
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->alias);
		}
	}
}

/* register a list of div clocks */
void __init samsung_clk_register_div(struct samsung_div_clock *list,
					unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_divider(NULL, list->name, list->parent_name,
			list->flags, reg_base + list->offset, list->shift,
			list->width, list->div_flags, &lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(clk, list->id);

		/* register a clock lookup only if a clock alias is specified */
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->alias);
		}
	}
}

struct exynos_clk_gate {
	struct clk_hw hw;
	void __iomem	*reg;
	unsigned long	set_bit;
	u8	bit_idx;
	u8	flags;
	spinlock_t	*lock;
};

#define to_clk_gate(_hw) container_of(_hw, struct exynos_clk_gate, hw)

static void exynos_clk_gate_endisable(struct clk_hw *hw, int enable)
{
	struct exynos_clk_gate *gate = to_clk_gate(hw);
	int set = gate->flags & CLK_GATE_SET_TO_DISABLE ? 1 : 0;
	unsigned long flags = 0;
	u32 reg;

	set ^= enable;

	if (gate->lock)
		spin_lock_irqsave(gate->lock, flags);

	reg = readl(gate->reg);

	if (set)
		reg |= gate->set_bit;
	else
		reg &= ~(gate->set_bit);

	writel(reg, gate->reg);

	if (gate->lock)
		spin_unlock_irqrestore(gate->lock, flags);
}

static int exynos5_clk_gate_enable(struct clk_hw *hw)
{
	exynos_clk_gate_endisable(hw, 1);

	return 0;
}

static void exynos5_clk_gate_disable(struct clk_hw *hw)
{
	exynos_clk_gate_endisable(hw, 0);
}

static int exynos5_clk_gate_is_enabled(struct clk_hw *hw)
{
	u32 reg;
	struct exynos_clk_gate *gate = to_clk_gate(hw);

	reg = readl(gate->reg);

	/* if a set bit disables this clk, flip it before masking */
	if (gate->flags & CLK_GATE_SET_TO_DISABLE)
		reg ^= BIT(gate->bit_idx);

	reg &= BIT(gate->bit_idx);

	return reg ? 1 : 0;
}

const struct clk_ops exynos5_clk_gate_ops = {
	.enable = exynos5_clk_gate_enable,
	.disable = exynos5_clk_gate_disable,
	.is_enabled = exynos5_clk_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(exynos5_clk_gate_ops);

struct clk *exynos_clk_register_gate(struct device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		void __iomem *reg, u8 bit_idx,
		u8 clk_gate_flags, spinlock_t *lock, unsigned long set_bit)
{
	struct exynos_clk_gate *gate;
	struct clk *clk;
	struct clk_init_data init;

	/* allocate the gate */
	gate = kzalloc(sizeof(struct exynos_clk_gate), GFP_KERNEL);
	if (!gate) {
		pr_err("%s: could not allocate gated clk\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	init.name = name;
	init.ops = &exynos5_clk_gate_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name: NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct clk_gate assignments */
	gate->reg = reg;
	gate->bit_idx = bit_idx;
	gate->flags = clk_gate_flags;
	gate->lock = lock;
	gate->hw.init = &init;
	gate->set_bit = set_bit;

	clk = clk_register(dev, &gate->hw);

	if (IS_ERR(clk))
		kfree(gate);

	return clk;
}

/* register a list of gate clocks */
void __init samsung_clk_register_gate(struct samsung_gate_clock *list,
						unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		if (list->flags & CLK_GATE_MULTI_BIT_SET)
			clk = exynos_clk_register_gate(NULL, list->name, list->parent_name,
					list->flags, reg_base + list->offset,
					list->bit_idx, list->gate_flags, &lock, list->set_bit);
		else
			clk = clk_register_gate(NULL, list->name, list->parent_name,
					list->flags, reg_base + list->offset,
					list->bit_idx, list->gate_flags, &lock);

		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		/* register a clock lookup only if a clock alias is specified */
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
							list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, list->alias);
		}

		samsung_clk_add_lookup(clk, list->id);
	}
}

/*
 * obtain the clock speed of all external fixed clock sources from device
 * tree and register it
 */
void __init samsung_clk_of_register_fixed_ext(
			struct samsung_fixed_rate_clock *fixed_rate_clk,
			unsigned int nr_fixed_rate_clk,
			struct of_device_id *clk_matches)
{
	const struct of_device_id *match;
	struct device_node *np;
	u32 freq;

	for_each_matching_node_and_match(np, clk_matches, &match) {
		if (of_property_read_u32(np, "clock-frequency", &freq))
			continue;
		fixed_rate_clk[(u32)match->data].fixed_rate = freq;
	}
	samsung_clk_register_fixed_rate(fixed_rate_clk, nr_fixed_rate_clk);
}

/* utility function to get the rate of a specified clock */
unsigned long _get_rate(const char *clk_name)
{
	struct clk *clk;
	unsigned long rate;

	clk = clk_get(NULL, clk_name);
	if (IS_ERR(clk)) {
		pr_err("%s: could not find clock %s\n", __func__, clk_name);
		return 0;
	}
	rate = clk_get_rate(clk);
	clk_put(clk);
	return rate;
}

/* utility function to set parent with names */
int exynos_set_parent(const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;

	p= __clk_lookup(parent);
	if (IS_ERR(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c= __clk_lookup(child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	return clk_set_parent(c, p);
}

/* utility function to get parent name with name */
struct clk *exynos_get_parent(const char *child)
{
	struct clk *c;

	c = __clk_lookup(child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return NULL;
	}

	return clk_get_parent(c);
}

/* utility function to set rate with name */
int exynos_set_rate(const char *conid, unsigned int rate)
{
	struct clk *target;

	target = __clk_lookup(conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	return clk_set_rate(target, rate);
}

/* utility function to get rate with name */
unsigned int  exynos_get_rate(const char *conid)
{
	struct clk *target;

	target = __clk_lookup(conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	return clk_get_rate(target);
}
