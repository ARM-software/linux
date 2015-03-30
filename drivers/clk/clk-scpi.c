/*
 * System Control and Power Interface (SCPI) Protocol based clock driver
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/scpi_protocol.h>

struct scpi_clk {
	u32 id;
	const char *name;
	struct clk_hw hw;
	struct scpi_dvfs_info *info;
	unsigned long rate_min;
	unsigned long rate_max;
};

#define to_scpi_clk(clk) container_of(clk, struct scpi_clk, hw)

static struct scpi_ops *scpi_ops;

static unsigned long scpi_clk_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	return scpi_ops->clk_get_val(clk->id);
}

static long scpi_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	if (clk->rate_min && rate < clk->rate_min)
		rate = clk->rate_min;
	if (clk->rate_max && rate > clk->rate_max)
		rate = clk->rate_max;

	return rate;
}

static int scpi_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			     unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	return scpi_ops->clk_set_val(clk->id, rate);
}

static struct clk_ops scpi_clk_ops = {
	.recalc_rate = scpi_clk_recalc_rate,
	.round_rate = scpi_clk_round_rate,
	.set_rate = scpi_clk_set_rate,
};

/* find closest match to given frequency in OPP table */
static int __scpi_dvfs_round_rate(struct scpi_clk *clk, unsigned long rate)
{
	int idx;
	u32 fmin = 0, fmax = ~0, ftmp;
	struct scpi_opp *opp = clk->info->opps;

	for (idx = 0; idx < clk->info->count; idx++, opp++) {
		ftmp = opp->freq;
		if (ftmp >= (u32)rate) {
			if (ftmp <= fmax)
				fmax = ftmp;
		} else {
			if (ftmp >= fmin)
				fmin = ftmp;
		}
	}
	if (fmax != ~0)
		return fmax;
	return fmin;
}

static unsigned long scpi_dvfs_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);
	int idx = scpi_ops->dvfs_get_idx(clk->id);
	struct scpi_opp *opp;

	if (idx < 0)
		return 0;

	opp = clk->info->opps + idx;
	return opp->freq;
}

static long scpi_dvfs_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);

	return __scpi_dvfs_round_rate(clk, rate);
}

static int __scpi_find_dvfs_index(struct scpi_clk *clk, unsigned long rate)
{
	int idx, max_opp = clk->info->count;
	struct scpi_opp *opp = clk->info->opps;

	for (idx = 0; idx < max_opp; idx++, opp++)
		if (opp->freq == rate)
			break;
	return (idx == max_opp) ? -EINVAL : idx;
}

static int scpi_dvfs_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct scpi_clk *clk = to_scpi_clk(hw);
	int ret = __scpi_find_dvfs_index(clk, rate);

	if (ret < 0)
		return ret;
	return scpi_ops->dvfs_set_idx(clk->id, (u8)ret);
}

static struct clk_ops scpi_dvfs_ops = {
	.recalc_rate = scpi_dvfs_recalc_rate,
	.round_rate = scpi_dvfs_round_rate,
	.set_rate = scpi_dvfs_set_rate,
};

static struct clk *
scpi_dvfs_ops_init(struct device *dev, struct device_node *np,
		   struct scpi_clk *sclk)
{
	struct clk_init_data init;
	struct scpi_dvfs_info *info;

	init.name = sclk->name;
	init.flags = CLK_IS_ROOT;
	init.num_parents = 0;
	init.ops = &scpi_dvfs_ops;
	sclk->hw.init = &init;

	info = scpi_ops->dvfs_get_info(sclk->id);
	if (IS_ERR(info))
		return (struct clk *)info;

	sclk->info = info;

	return devm_clk_register(dev, &sclk->hw);
}

static struct clk *
scpi_clk_ops_init(struct device *dev, struct device_node *np,
		  struct scpi_clk *sclk)
{
	struct clk_init_data init;
	int ret;

	init.name = sclk->name;
	init.flags = CLK_IS_ROOT;
	init.num_parents = 0;
	init.ops = &scpi_clk_ops;
	sclk->hw.init = &init;

	ret = scpi_ops->clk_get_range(sclk->id, &sclk->rate_min,
				      &sclk->rate_max);
	if (!sclk->rate_max)
		ret = -EINVAL;
	if (ret)
		return ERR_PTR(ret);

	return devm_clk_register(dev, &sclk->hw);
}

static const struct of_device_id scpi_clk_match[] = {
	{ .compatible = "arm,scpi-dvfs", .data = scpi_dvfs_ops_init, },
	{ .compatible = "arm,scpi-clk", .data = scpi_clk_ops_init, },
	{}
};

static int scpi_clk_probe(struct platform_device *pdev)
{
	struct clk **clks;
	int idx, count;
	struct clk_onecell_data *clk_data;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct clk *(*clk_ops_init)(struct device *, struct device_node *,
				    struct scpi_clk *);

	if (!of_device_is_available(np))
		return -ENODEV;

	clk_ops_init = of_match_node(scpi_clk_match, np)->data;
	if (!clk_ops_init)
		return -ENODEV;

	count = of_property_count_strings(np, "clock-output-names");
	if (count < 0) {
		dev_err(dev, "%s: invalid clock output count\n", np->name);
		return -EINVAL;
	}

	clk_data = devm_kmalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data) {
		dev_err(dev, "failed to allocate clock provider data\n");
		return -ENOMEM;
	}

	clks = devm_kmalloc(dev, count * sizeof(*clks), GFP_KERNEL);
	if (!clks) {
		dev_err(dev, "failed to allocate clock providers\n");
		return -ENOMEM;
	}

	for (idx = 0; idx < count; idx++) {
		struct scpi_clk *sclk;
		u32 val;

		sclk = devm_kzalloc(dev, sizeof(*sclk), GFP_KERNEL);
		if (!sclk) {
			dev_err(dev, "failed to allocate scpi clocks\n");
			return -ENOMEM;
		}

		if (of_property_read_string_index(np, "clock-output-names",
						  idx, &sclk->name)) {
			dev_err(dev, "invalid clock name @ %s\n", np->name);
			return -EINVAL;
		}

		if (of_property_read_u32_index(np, "clock-indices",
					       idx, &val)) {
			dev_err(dev, "invalid clock index @ %s\n", np->name);
			return -EINVAL;
		}

		sclk->id = val;

		clks[idx] = clk_ops_init(dev, np, sclk);
		if (IS_ERR(clks[idx])) {
			dev_err(dev, "failed to register clock '%s'\n",
				sclk->name);
		}

		dev_dbg(dev, "Registered clock '%s'\n", sclk->name);
	}

	clk_data->clks = clks;
	clk_data->clk_num = idx;
	of_clk_add_provider(np, of_clk_src_onecell_get, clk_data);

	platform_set_drvdata(pdev, clk_data);

	return 0;
}

static int scpi_clk_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver scpi_clk_driver = {
	.driver	= {
		.name = "scpi_clk",
		.of_match_table = scpi_clk_match,
	},
	.probe = scpi_clk_probe,
	.remove = scpi_clk_remove,
};

static int scpi_clocks_probe(struct platform_device *pdev)
{
	scpi_ops = get_scpi_ops();
	if (!scpi_ops)
		return -ENXIO;

	return of_platform_populate(pdev->dev.of_node, scpi_clk_match,
				    NULL, &pdev->dev);
}

static int scpi_clocks_remove(struct platform_device *pdev)
{
	of_platform_depopulate(&pdev->dev);
	scpi_ops = NULL;
	return 0;
}

static const struct of_device_id scpi_clocks_ids[] = {
	{ .compatible = "arm,scpi-clocks", },
	{}
};

static struct platform_driver scpi_clocks_driver = {
	.driver	= {
		.name = "scpi_clocks",
		.of_match_table = scpi_clocks_ids,
	},
	.probe = scpi_clocks_probe,
	.remove = scpi_clocks_remove,
};

static int __init scpi_clocks_init(void)
{
	int ret;

	ret = platform_driver_register(&scpi_clk_driver);
	if (ret)
		return ret;

	return platform_driver_register(&scpi_clocks_driver);
}
module_init(scpi_clocks_init);

static void __exit scpi_clocks_exit(void)
{
	platform_driver_unregister(&scpi_clk_driver);
	platform_driver_unregister(&scpi_clocks_driver);
}
module_exit(scpi_clocks_exit);

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM SCPI clock driver");
MODULE_LICENSE("GPL");
