/*
 * linux/drivers/media/video/exynos/hevc/hevc_pm.c
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
#include <linux/clk-private.h>

#include <plat/cpu.h>

#include "hevc_common.h"
#include "hevc_debug.h"
#include "hevc_pm.h"
#include "hevc_reg.h"

#define CLK_DEBUG

static struct hevc_pm *pm;
atomic_t clk_ref_hevc;
static int power_on_flag;

#if defined(CONFIG_ARCH_EXYNOS4)

#define HEVC_PARENT_CLK_NAME	"mout_hevc0"
#define HEVC_CLKNAME		"sclk_hevc"
#define HEVC_GATE_CLK_NAME	"hevc"

int hevc_init_pm(struct hevc_dev *dev)
{
	struct clk *parent, *sclk;
	int ret = 0;

	pm = &dev->pm;

	parent = clk_get(dev->device, "gate_hevc");
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

	atomic_set(&pm->power, 0);
	atomic_set(&clk_ref_hevc, 0);

	pm->device = dev->device;
	pm_runtime_enable(pm->device);

	return 0;

err_g_clk:
	clk_put(sclk);
err_s_clk:
	clk_put(parent);
err_p_clk:
	return ret;
}

#elif defined(CONFIG_ARCH_EXYNOS5)

#define HEVC_PARENT_CLK_NAME	"aclk_333"
#define HEVC_CLKNAME		"sclk_hevc"
#define HEVC_GATE_CLK_NAME	"hevc"

int hevc_init_pm(struct hevc_dev *dev)
{
	int ret = 0;

	pm = &dev->pm;

	/* clock for gating */
	pm->clock = clk_get(dev->device, "gate_hevc");
	if (IS_ERR(pm->clock)) {
		printk(KERN_ERR "failed to get clock-gating control\n");
		ret = PTR_ERR(pm->clock);
		goto err_g_clk;
	}

	ret = clk_prepare(pm->clock);
	if (ret) {
		printk(KERN_ERR "clk_prepare() failed\n");
		return ret;
	}

	spin_lock_init(&pm->clklock);
	atomic_set(&pm->power, 0);
	atomic_set(&clk_ref_hevc, 0);

	pm->device = dev->device;
	pm_runtime_enable(pm->device);

	return 0;

err_g_clk:
	clk_put(pm->clock);
	return ret;
}

int hevc_set_clock_parent(struct hevc_dev *dev)
{
	struct clk *clk_child;
	struct clk *clk_parent;

	clk_child = clk_get(dev->device, "mout_aclk_hevc_400_user");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n",__clk_get_name(clk_child));
		return PTR_ERR(clk_child);
	}
	clk_parent = clk_get(dev->device, "aclk_hevc_400");
	if (IS_ERR(clk_parent)) {
		pr_err("failed to get %s clock\n", __clk_get_name(clk_parent));
		return PTR_ERR(clk_parent);
	}
	clk_set_parent(clk_child, clk_parent);

	return 0;
}

#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
static int hevc_clock_set_rate(struct hevc_dev *dev, unsigned long rate)
{
	struct clk *parent_clk = NULL;
	int ret = 0;

	parent_clk = clk_get(dev->device, "dout_aclk_hevc_400");
	if (IS_ERR(parent_clk)) {
		hevc_err("failed to get parent clock dout_aclk_hevc_400\n");
		ret = PTR_ERR(parent_clk);
		goto err_g_clk;
	}

	clk_set_rate(parent_clk, rate * 1000);

	clk_put(parent_clk);

	return 0;

err_g_clk:
	return ret;
}
#endif
#endif

void hevc_final_pm(struct hevc_dev *dev)
{
	clk_put(pm->clock);

	pm_runtime_disable(pm->device);
}

int hevc_clock_on(void)
{
	int ret = 0;
	int state, val;
	struct hevc_dev *dev = platform_get_drvdata(to_platform_device(pm->device));
	unsigned long flags;

#ifdef CONFIG_ARM_EXYNOS5430_BUS_DEVFREQ
	hevc_clock_set_rate(dev, dev->curr_rate);
#endif
	ret = clk_enable(pm->clock);
	if (ret < 0)
		return ret;

	if (!dev->curr_ctx_drm) {
		ret = hevc_mem_resume(dev->alloc_ctx[0]);
		if (ret < 0) {
			clk_disable(pm->clock);
			return ret;
		}
	}

	spin_lock_irqsave(&pm->clklock, flags);
	if (atomic_inc_return(&clk_ref_hevc) == 1) {
		if(power_on_flag == 0){
			val = hevc_read_reg(HEVC_BUS_RESET_CTRL);
			val &= ~(0x1);
			hevc_write_reg(val, HEVC_BUS_RESET_CTRL);
		}
		power_on_flag = 0;
	}
	spin_unlock_irqrestore(&pm->clklock, flags);

	state = atomic_read(&clk_ref_hevc);
	hevc_debug(3, "+ %d", state);

	return 0;
}

void hevc_clock_off(void)
{
	int state, val;
	unsigned long timeout, flags;
	struct hevc_dev *dev = platform_get_drvdata(to_platform_device(pm->device));

	spin_lock_irqsave(&pm->clklock, flags);
	if (atomic_dec_return(&clk_ref_hevc) == 0) {
		hevc_write_reg(0x1, HEVC_BUS_RESET_CTRL);

		timeout = jiffies + msecs_to_jiffies(HEVC_BW_TIMEOUT);
		/* Check bus status */
		do {
			if (time_after(jiffies, timeout)) {
				hevc_err("Timeout while resetting HEVC.\n");
				break;
			}
			val = hevc_read_reg(
					HEVC_BUS_RESET_CTRL);
		} while ((val & 0x2) == 0);
	}
	spin_unlock_irqrestore(&pm->clklock, flags);

	state = atomic_read(&clk_ref_hevc);
	if (state < 0) {
		hevc_err("Clock state is wrong(%d)\n", state);
		atomic_set(&clk_ref_hevc, 0);
	} else {
		if (!dev->curr_ctx_drm)
			hevc_mem_suspend(dev->alloc_ctx[0]);
		clk_disable(pm->clock);
	}

}

int hevc_power_on(void)
{
	atomic_set(&pm->power, 1);

	power_on_flag = 1;
	return pm_runtime_get_sync(pm->device);
	return 0;
}

int hevc_power_off(void)
{
	atomic_set(&pm->power, 0);

	return pm_runtime_put_sync(pm->device);
	return 0;
}

int hevc_get_clk_ref_cnt(void)
{
	return atomic_read(&clk_ref_hevc);
}
