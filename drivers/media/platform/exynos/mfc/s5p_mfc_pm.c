/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk-private.h>

#include <plat/cpu.h>
#include <mach/smc.h>
#include <mach/bts.h>

#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_reg.h"

#define CLK_DEBUG


#if defined(CONFIG_ARCH_EXYNOS4)

#define MFC_PARENT_CLK_NAME	"mout_mfc0"
#define MFC_CLKNAME		"sclk_mfc"
#define MFC_GATE_CLK_NAME	"mfc"

int s5p_mfc_init_pm(struct s5p_mfc_dev *dev)
{
	struct clk *parent, *sclk;
	int ret = 0;

	parent = clk_get(dev->device, "mfc");
	if (IS_ERR(parent)) {
		printk(KERN_ERR "failed to get parent clock\n");
		ret = -ENOENT;
		goto err_p_clk;
	}

	ret = clk_prepare(parent);
	if (ret) {
		printk(KERN_ERR "clk_prepare() failed\n");
		return ret;
	}

	atomic_set(&dev->pm.power, 0);
	atomic_set(&dev->clk_ref, 0);

	dev->pm.device = dev->device;
	pm_runtime_enable(dev->pm.device);

	return 0;

err_g_clk:
	clk_put(sclk);
err_s_clk:
	clk_put(parent);
err_p_clk:
	return ret;
}

#elif defined(CONFIG_ARCH_EXYNOS5)

#define MFC_PARENT_CLK_NAME	"aclk_333"
#define MFC_CLKNAME		"sclk_mfc"
#define MFC_GATE_CLK_NAME	"mfc"

int s5p_mfc_init_pm(struct s5p_mfc_dev *dev)
{
	struct clk *parent_clk = NULL;
	int ret = 0;

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0)
		dev->pm.clock = clk_get(dev->device, "gate_mfc0");
	else if (dev->id == 1)
		dev->pm.clock = clk_get(dev->device, "gate_mfc1");
#elif defined(CONFIG_SOC_EXYNOS5422)
	dev->pm.clock = clk_get(dev->device, "mfc");
#endif

	if (IS_ERR(dev->pm.clock)) {
		printk(KERN_ERR "failed to get parent clock\n");
		ret = -ENOENT;
		goto err_p_clk;
	}

	ret = clk_prepare(dev->pm.clock);
	if (ret) {
		printk(KERN_ERR "clk_prepare() failed\n");
		return ret;
	}

	spin_lock_init(&dev->pm.clklock);
	atomic_set(&dev->pm.power, 0);
	atomic_set(&dev->clk_ref, 0);

	dev->pm.device = dev->device;
	pm_runtime_enable(dev->pm.device);

	clk_put(parent_clk);

	return 0;

err_p_clk:
	clk_put(dev->pm.clock);

	return ret;
}

int s5p_mfc_set_clock_parent(struct s5p_mfc_dev *dev)
{
	struct clk *clk_child = NULL;
	struct clk *clk_parent = NULL;

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0) {
		clk_child = clk_get(dev->device, "mout_aclk_mfc0_333_user");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
		clk_parent = clk_get(dev->device, "aclk_mfc0_333");
		if (IS_ERR(clk_parent)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}
		clk_set_parent(clk_child, clk_parent);
	} else if (dev->id == 1) {
		clk_child = clk_get(dev->device, "mout_aclk_mfc1_333_user");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
		clk_parent = clk_get(dev->device, "aclk_mfc1_333");
		if (IS_ERR(clk_parent)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}
		clk_set_parent(clk_child, clk_parent);
	}
#elif defined(CONFIG_SOC_EXYNOS5422)
	clk_child = clk_get(dev->device, "mout_aclk_333_user");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
	clk_parent = clk_get(dev->device, "mout_aclk_333_sw");
	if (IS_ERR(clk_parent)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
		return PTR_ERR(clk_parent);
	}
	clk_set_parent(clk_child, clk_parent);
#endif
	return 0;
}

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ

/* int_div_lock is only needed for EXYNOS5410 */
#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
extern spinlock_t int_div_lock;
#endif

static int s5p_mfc_clock_set_rate(struct s5p_mfc_dev *dev, unsigned long rate)
{
	struct clk *clk_child = NULL;
#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
	struct clk *clk_parent = NULL;
#endif

#if defined(CONFIG_SOC_EXYNOS5430)
	if (dev->id == 0) {
#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
		clk_child = clk_get(dev->device, "mout_aclk_mfc0_333_a");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}

		if(dev->curr_rate == 552000)
			clk_parent = clk_get(dev->device, "mout_isp_pll");
		else
			clk_parent = clk_get(dev->device, "mout_mfc_pll_user");
		if (IS_ERR(clk_parent)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}

		clk_set_parent(clk_child, clk_parent);
#endif
		clk_child = clk_get(dev->device, "dout_aclk_mfc0_333");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}

	} else if (dev->id == 1) {
#if defined(CONFIG_SOC_EXYNOS5430_REV_1)
		clk_child = clk_get(dev->device, "mout_aclk_mfc1_333_a");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}

		if(dev->curr_rate == 552000)
			clk_parent = clk_get(dev->device, "mout_isp_pll");
		else
			clk_parent = clk_get(dev->device, "mout_mfc_pll_user");
		if (IS_ERR(clk_parent)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
			return PTR_ERR(clk_parent);
		}

		clk_set_parent(clk_child, clk_parent);
#endif
		clk_child = clk_get(dev->device, "dout_aclk_mfc1_333");
		if (IS_ERR(clk_child)) {
			pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
			return PTR_ERR(clk_child);
		}
	}
#elif defined(CONFIG_SOC_EXYNOS5422)
	clk_child = clk_get(dev->device, "dout_aclk_333");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
#endif

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	spin_lock(&int_div_lock);
#endif
	if(clk_child)
		clk_set_rate(clk_child, rate * 1000);

#ifdef CONFIG_ARM_EXYNOS5410_BUS_DEVFREQ
	spin_unlock(&int_div_lock);
#endif

	if(clk_child)
		clk_put(clk_child);

	return 0;
}
#endif
#endif

void s5p_mfc_final_pm(struct s5p_mfc_dev *dev)
{
	clk_put(dev->pm.clock);

	pm_runtime_disable(dev->pm.device);
}

int s5p_mfc_clock_on(struct s5p_mfc_dev *dev)
{
	int ret = 0;
	int state, val;
	unsigned long flags;

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	s5p_mfc_clock_set_rate(dev, dev->curr_rate);
#endif
	ret = clk_enable(dev->pm.clock);
	if (ret < 0)
		return ret;

	if (dev->curr_ctx_drm && dev->is_support_smc) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		mfc_debug(3, "Begin: enable protection\n");
		ret = exynos_smc(SMC_PROTECTION_SET, 0,
					dev->id, SMC_PROTECTION_ENABLE);
		if (!ret) {
			printk("Protection Enable failed! ret(%u)\n", ret);
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
			clk_disable(dev->pm.clock);
			return ret;
		}
		mfc_debug(3, "End: enable protection\n");
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	} else {
		ret = s5p_mfc_mem_resume(dev->alloc_ctx[0]);
		if (ret < 0) {
			clk_disable(dev->pm.clock);
			return ret;
		}
	}

	if (IS_MFCV6(dev)) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		if ((atomic_inc_return(&dev->clk_ref) == 1) &&
				FW_HAS_BUS_RESET(dev)) {
			val = s5p_mfc_read_reg(dev, S5P_FIMV_MFC_BUS_RESET_CTRL);
			val &= ~(0x1);
			s5p_mfc_write_reg(dev, val, S5P_FIMV_MFC_BUS_RESET_CTRL);
		}
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	} else {
		atomic_inc_return(&dev->clk_ref);
	}

	state = atomic_read(&dev->clk_ref);
	mfc_debug(2, "+ %d\n", state);

	return 0;
}

void s5p_mfc_clock_off(struct s5p_mfc_dev *dev)
{
	int state, val;
	unsigned long timeout, flags;
	int ret = 0;

	if (IS_MFCV6(dev)) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		if ((atomic_dec_return(&dev->clk_ref) == 0) &&
				FW_HAS_BUS_RESET(dev)) {
			s5p_mfc_write_reg(dev, 0x1, S5P_FIMV_MFC_BUS_RESET_CTRL);

			timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);
			/* Check bus status */
			do {
				if (time_after(jiffies, timeout)) {
					mfc_err_dev("Timeout while resetting MFC.\n");
					break;
				}
				val = s5p_mfc_read_reg(dev,
						S5P_FIMV_MFC_BUS_RESET_CTRL);
			} while ((val & 0x2) == 0);
		}
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	} else {
		atomic_dec_return(&dev->clk_ref);
	}

	state = atomic_read(&dev->clk_ref);
	if (state < 0) {
		mfc_err_dev("Clock state is wrong(%d)\n", state);
		atomic_set(&dev->clk_ref, 0);
	} else {
		if (dev->curr_ctx_drm && dev->is_support_smc) {
			mfc_debug(3, "Begin: disable protection\n");
			spin_lock_irqsave(&dev->pm.clklock, flags);
			ret = exynos_smc(SMC_PROTECTION_SET, 0,
					dev->id, SMC_PROTECTION_DISABLE);
			if (!ret) {
				printk("Protection Disable failed! ret(%u)\n", ret);
				spin_unlock_irqrestore(&dev->pm.clklock, flags);
				clk_disable(dev->pm.clock);
				return;
			}
			mfc_debug(3, "End: disable protection\n");
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
		} else {
			s5p_mfc_mem_suspend(dev->alloc_ctx[0]);
		}
		clk_disable(dev->pm.clock);
	}
	mfc_debug(2, "- %d\n", state);
}

int s5p_mfc_power_on(struct s5p_mfc_dev *dev)
{
	int ret;
	atomic_set(&dev->pm.power, 1);

	ret = pm_runtime_get_sync(dev->pm.device);

#if defined(CONFIG_SOC_EXYNOS5422)
	bts_initialize("pd-mfc", true);
#endif

	return ret;
}

int s5p_mfc_power_off(struct s5p_mfc_dev *dev)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	bts_initialize("pd-mfc", false);
#endif
	atomic_set(&dev->pm.power, 0);

	return pm_runtime_put_sync(dev->pm.device);
}

int s5p_mfc_get_clk_ref_cnt(struct s5p_mfc_dev *dev)
{
	return atomic_read(&dev->clk_ref);
}
