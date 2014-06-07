/* linux/drivers/video/decon_display/decon_pm_exynos5430.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/clk-private.h>

#include <linux/platform_device.h>
#include <mach/map.h>
#include "regs-decon.h"
#include "decon_display_driver.h"
#include "decon_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"

#include <../drivers/clk/samsung/clk.h>

static struct clk *g_mout_sclk_decon_eclk_a;
static struct clk *g_mout_disp_pll, *g_fout_disp_pll;
static struct clk *g_mout_sclk_decon_eclk_user;
static struct clk *g_mout_sclk_dsd_user;
static struct clk *g_aclk_disp_333, *g_mout_aclk_disp_333_user;
static struct clk *g_mout_sclk_decon_eclk_user;
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
static struct clk *g_mout_bus_pll_sub;
static struct clk *g_sclk_decon_eclk_mif;
static struct clk *g_sclk_dsd_mif;
static struct clk *g_aclk_disp_222, *g_mout_aclk_disp_222_user;
static struct clk *g_mout_sclk_decon_eclk_disp;
#else
static struct clk *g_mout_bus_pll_div2;
static struct clk *g_sclk_decon_eclk_disp;
static struct clk *g_sclk_dsd_disp;
static struct clk *g_mout_sclk_decon_eclk;
#endif

static struct clk *g_dout_aclk_disp_333;
static struct clk *g_dout_sclk_decon_eclk;

#ifdef CONFIG_SOC_EXYNOS5430_REV_0
static struct clk *g_mout_sclk_dsd_a, *g_dout_mfc_pll;
#else
static struct clk *g_mout_sclk_dsd_a, *g_mout_mfc_pll_div2;
#endif

static struct clk *g_phyclk_mipidphy_rxclkesc0_phy,
	*g_mout_phyclk_mipidphy_rxclkesc0_user;
static struct clk *g_phyclk_mipidphy_bitclkdiv8_phy,
	*g_mout_phyclk_mipidphy_bitclkdiv8_user;

#define DISPLAY_GET_CLOCK1(node) do {\
	g_##node = clk_get(dev, #node); \
	if (IS_ERR(g_##node)) { \
		pr_err("Failed to clk_get - " #node "\n"); \
		return PTR_ERR(g_##node); \
	} \
	} while (0)

#define DISPLAY_GET_CLOCK2(child, parent) do {\
	g_##child = clk_get(dev, #child); \
	g_##parent = clk_get(dev, #parent); \
	if (IS_ERR(g_##child)) { \
		pr_err("Failed to clk_get - " #child "\n"); \
		return PTR_ERR(g_##child); \
	} \
	if (IS_ERR(g_##parent)) { \
		pr_err("Failed to clk_get - " #parent "\n"); \
		return PTR_ERR(g_##parent); \
	} \
	} while (0)

#define DISPLAY_CLOCK_SET_PARENT(child, parent) do {\
	g_##child = clk_get(dev, #child); \
	g_##parent = clk_get(dev, #parent); \
	if (IS_ERR(g_##child)) { \
		pr_err("Failed to clk_get - " #child "\n"); \
		return PTR_ERR(g_##child); \
	} \
	if (IS_ERR(g_##parent)) { \
		pr_err("Failed to clk_get - " #parent "\n"); \
		return PTR_ERR(g_##parent); \
	} \
	ret = clk_set_parent(g_##child, g_##parent); \
	if (ret < 0) { \
		pr_err("failed to set parent " #parent " of " #child "\n"); \
	} \
	} while (0)

#define DISPLAY_CLOCK_INLINE_SET_PARENT(child, parent) do {\
	ret = clk_set_parent(g_##child, g_##parent); \
	if (ret < 0) { \
		pr_err("failed to set parent " #parent " of " #child "\n"); \
	} \
	} while (0)

#define DISPLAY_CHECK_REGS(addr, mask, val) do {\
	regs = ioremap(addr, 0x4); \
	data = readl(regs); \
	if ((data & mask) != val) { \
		pr_err("[ERROR] Masked value, (0x%08X & " \
			"val(0x%08X)) SHOULD BE 0x%08X, but 0x%08X\n", \
			mask, addr, val, (data&mask)); \
	} \
	iounmap(regs); \
	} while (0)

#define DISPLAY_INLINE_SET_RATE(node, target) \
	clk_set_rate(g_##node, target);

#define DISPLAY_SET_RATE(node, target) do {\
	g_##node = clk_get(dev, #node); \
	if (IS_ERR(g_##node)) { \
		pr_err("Failed to clk_get - " #node "\n"); \
		return PTR_ERR(g_##node); \
	} \
	clk_set_rate(g_##node, target); \
	} while (0)

#define TEMPORARY_RECOVER_CMU(addr, mask, bits, value) do {\
	regs = ioremap(addr, 0x4); \
	data = readl(regs) & ~((mask) << (bits)); \
	data |= ((value & mask) << (bits)); \
	writel(data, regs); \
	iounmap(regs); \
	} while (0)

static void additional_clock_setup(void)
{
	void __iomem *regs;

	/* Set: ignore CLK1 was toggling */
	regs = ioremap(0x13B90508, 0x4);
	writel(0, regs);
	iounmap(regs);
}

static void check_display_clocks(void)
{
	void __iomem *regs;
	u32 data = 0x00;

	/* Now check mux values */
	DISPLAY_CHECK_REGS(0x105B0210, 0x00000001, 0x00000001);
	DISPLAY_CHECK_REGS(0x13B90200, 0x00000001, 0x00000001);
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_CHECK_REGS(0x13B90204, 0x000FFFFF, 0x00011111);
#endif
	DISPLAY_CHECK_REGS(0x13B90208, 0x00001100, 0x00001100);
	DISPLAY_CHECK_REGS(0x13B9020C, 0x00000001, 0x00000001);

	/* Now check divider value */
	DISPLAY_CHECK_REGS(0x105B060C, 0x00000070, 0x00000020);
}

int enable_display_decon_clocks(struct device *dev)
{
	int ret = 0;
	void __iomem *regs;
	u32 data;
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	DISPLAY_INLINE_SET_RATE(fout_disp_pll, 142 * MHZ);

#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_a,
		mout_bus_pll_sub);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_user,
		sclk_decon_eclk_mif);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_dsd_user, sclk_dsd_mif);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_disp,
		mout_sclk_decon_eclk_user);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_disp_222_user,
		aclk_disp_222);
#else
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_a,
		mout_bus_pll_div2);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk_user,
		sclk_decon_eclk_disp);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_dsd_user, sclk_dsd_disp);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_decon_eclk,
		mout_sclk_decon_eclk_user);
#endif
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_disp_pll, fout_disp_pll);

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_aclk_disp_333_user,
		aclk_disp_333);

#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_dsd_a, dout_mfc_pll);
#else
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_sclk_dsd_a, mout_mfc_pll_div2);
#endif

	DISPLAY_INLINE_SET_RATE(dout_aclk_disp_333, 222*MHZ);
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_INLINE_SET_RATE(dout_sclk_decon_eclk, 400*MHZ);
#else
	DISPLAY_INLINE_SET_RATE(dout_sclk_decon_eclk, 200*MHZ);
#endif

	additional_clock_setup();

#ifndef CONFIG_SOC_EXYNOS5430_REV_0
	TEMPORARY_RECOVER_CMU(0x13B9020C, 0xFFFFFFFF, 0, 0x0101);
	TEMPORARY_RECOVER_CMU(0x105B060C, 0x7, 4, 0x02);
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	dispdrv->pm_status.ops->clk_on(dispdrv);
#endif
	return ret;
}

int init_display_decon_clocks(struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_GET_CLOCK2(mout_sclk_decon_eclk_a, mout_bus_pll_sub);
	DISPLAY_GET_CLOCK2(mout_aclk_disp_222_user, aclk_disp_222);
	DISPLAY_GET_CLOCK2(mout_sclk_decon_eclk_user,
		sclk_decon_eclk_mif);
	DISPLAY_GET_CLOCK2(mout_sclk_dsd_user, sclk_dsd_mif);
	DISPLAY_GET_CLOCK2(mout_sclk_decon_eclk_disp,
		mout_sclk_decon_eclk_user);
#else
	DISPLAY_GET_CLOCK2(mout_sclk_decon_eclk_a, mout_bus_pll_div2);
	DISPLAY_GET_CLOCK2(mout_sclk_decon_eclk_user,
		sclk_decon_eclk_disp);
	DISPLAY_GET_CLOCK2(mout_sclk_dsd_user, sclk_dsd_disp);
	DISPLAY_GET_CLOCK2(mout_sclk_decon_eclk,
		mout_sclk_decon_eclk_user);
#endif
	DISPLAY_GET_CLOCK1(mout_disp_pll);
	DISPLAY_GET_CLOCK2(mout_aclk_disp_333_user, aclk_disp_333);

#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_GET_CLOCK2(mout_sclk_dsd_a, dout_mfc_pll);
#else
	DISPLAY_GET_CLOCK2(mout_sclk_dsd_a, mout_mfc_pll_div2);
#endif

	DISPLAY_GET_CLOCK1(dout_aclk_disp_333);
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	DISPLAY_GET_CLOCK1(dout_sclk_decon_eclk);
#else
	DISPLAY_GET_CLOCK1(dout_sclk_decon_eclk);
#endif
	enable_display_decon_clocks(dev);

	additional_clock_setup();

	check_display_clocks();
	return ret;
}

int init_display_driver_clocks(struct device *dev)
{
	int ret = 0;

	DISPLAY_GET_CLOCK1(fout_disp_pll);
	DISPLAY_INLINE_SET_RATE(fout_disp_pll, 142 * MHZ);
	msleep(20);

	DISPLAY_CLOCK_SET_PARENT(mout_phyclk_mipidphy_rxclkesc0_user,
		phyclk_mipidphy_rxclkesc0_phy);
	DISPLAY_CLOCK_SET_PARENT(mout_phyclk_mipidphy_bitclkdiv8_user,
		phyclk_mipidphy_bitclkdiv8_phy);

	return ret;
}

int enable_display_driver_power(struct device *dev)
{
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	struct pinctrl *pinctrl;
#endif
	int id;
	int ret = 0;
	struct display_driver *dispdrv;
	struct display_gpio *gpio;

	dispdrv = get_display_driver();

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	id = gpio_request(gpio->id[0], "lcd_reset");
	if (id < 0) {
		pr_err("Failed to get gpio number for the lcd power\n");
		return -EINVAL;
	}
	gpio_direction_output(gpio->id[0], 0x01);

	gpio_set_value(gpio->id[0], 1);
	usleep_range(5000, 6000);

	gpio_set_value(gpio->id[0], 0);
	usleep_range(5000, 6000);
	msleep(20);

	gpio_set_value(gpio->id[0], 1);
	usleep_range(5000, 6000);

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	pinctrl = devm_pinctrl_get_select(dev, "turnon_tes");
	if (IS_ERR(pinctrl))
		pr_err("failed to get tes pinctrl - ON");
#endif
	gpio_free(gpio->id[0]);

	return ret;
}

int disable_display_driver_power(struct device *dev)
{
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	struct pinctrl *pinctrl;
#endif
	int id;
	int ret = 0;
	struct display_driver *dispdrv;
	struct display_gpio *gpio;

	dispdrv = get_display_driver();

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio();
	id = gpio_request(gpio->id[0], "lcd_reset");
	if (id < 0) {
		pr_err("Failed to get gpio number for the lcd power\n");
		return -EINVAL;
	}
	gpio_set_value(gpio->id[0], 0);
	usleep_range(5000, 6000);

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	pinctrl = devm_pinctrl_get_select(dev, "turnoff_tes");
	if (IS_ERR(pinctrl))
		pr_err("failed to get tes pinctrl - OFF");
#endif
	gpio_free(gpio->id[0]);

	return ret;
}

int enable_display_driver_clocks(struct device *dev)
{
	int ret = 0;

	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_phyclk_mipidphy_rxclkesc0_user,
		phyclk_mipidphy_rxclkesc0_phy);
	DISPLAY_CLOCK_INLINE_SET_PARENT(mout_phyclk_mipidphy_bitclkdiv8_user,
		phyclk_mipidphy_bitclkdiv8_phy);

	check_display_clocks();

	return ret;
}

int disable_display_decon_clocks(struct device *dev)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	dispdrv->pm_status.ops->clk_off(dispdrv);
#endif

	return 0;
}

int enable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int disable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int enable_display_dsd_clocks(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	if (!dispdrv->decon_driver.dsd_clk) {
		dispdrv->decon_driver.dsd_clk = __clk_lookup("gate_dsd");
		if (IS_ERR(dispdrv->decon_driver.dsd_clk)) {
			pr_err("Failed to clk_get - gate_dsd\n");
			return -EBUSY;
		}
	}

	if (dispdrv->decon_driver.dsd_clk->enable_count == 0)
		clk_prepare_enable(dispdrv->decon_driver.dsd_clk);
	return 0;
}

int disable_display_dsd_clocks(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	if (!dispdrv->decon_driver.dsd_clk)
		return -EBUSY;

	if (dispdrv->decon_driver.dsd_clk->enable_count > 0)
		clk_disable_unprepare(dispdrv->decon_driver.dsd_clk);
	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
bool check_camera_is_running(void)
{
	/* CAM1 STATUS */
	if (readl(S5P_VA_PMU + 0x40A4) & 0x1)
		return true;
	else
		return false;
}

bool get_display_power_status(void)
{
	/* DISP_STATUS */
	if (readl(S5P_VA_PMU + 0x4084) & 0x1)
		return true;
	else
		return false;
}

void set_hw_trigger_mask(struct s3c_fb *sfb, bool mask)
{
	unsigned int val;

	val = readl(sfb->regs + TRIGCON);
	if (mask)
		val &= ~(TRIGCON_HWTRIGMASK_I80_RGB);
	else
		val |= (TRIGCON_HWTRIGMASK_I80_RGB);

	writel(val, sfb->regs + TRIGCON);
}

int get_display_line_count(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	return (readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT);
}

void set_default_hibernation_mode(struct display_driver *dispdrv)
{
	bool clock_gating = false;
	bool power_gating = false;
	bool hotplug_gating = false;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	clock_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
	power_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING_DEEPSTOP
	hotplug_gating = true;
#endif
	dispdrv->pm_status.clock_gating_on = clock_gating;
	dispdrv->pm_status.power_gating_on = power_gating;
	dispdrv->pm_status.hotplug_gating_on = hotplug_gating;
}

#define DECON_VCLK_ECLK_MUX_MASKING
int decon_vclk_eclk_mux_control(bool enable)
{
	int ret = 0;
	void __iomem *regs;
	u32 data = 0x00;

	if (enable)
		TEMPORARY_RECOVER_CMU(0x13B9030C, 0x11, 0, 0x11);
	else
		TEMPORARY_RECOVER_CMU(0x13B9030C, 0x11, 0, 0x0);
	return ret;
}
void decon_clock_on(struct display_driver *dispdrv)
{
#ifndef DECON_VCLK_ECLK_MUX_MASKING
	if (!dispdrv->decon_driver.clk) {
		dispdrv->decon_driver.clk = __clk_lookup("gate_decon");
		if (IS_ERR(dispdrv->decon_driver.clk)) {
			pr_err("Failed to clk_get - gate_decon\n");
			return;
		}
	}
	clk_prepare(dispdrv->decon_driver.clk);
	clk_enable(dispdrv->decon_driver.clk);
#else
	decon_vclk_eclk_mux_control(true);
#endif
}

void mic_clock_on(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	if (!dispdrv->mic_driver.clk) {
		dispdrv->mic_driver.clk = __clk_lookup("gate_mic");
		if (IS_ERR(dispdrv->mic_driver.clk)) {
			pr_err("Failed to clk_get - gate_mic\n");
			return;
		}
	}
	clk_prepare(dispdrv->mic_driver.clk);
	clk_enable(dispdrv->mic_driver.clk);
#endif
}

void dsi_clock_on(struct display_driver *dispdrv)
{
	if (!dispdrv->dsi_driver.clk) {
		dispdrv->dsi_driver.clk = __clk_lookup("gate_dsim0");
		if (IS_ERR(dispdrv->dsi_driver.clk)) {
			pr_err("Failed to clk_get - gate_dsi\n");
			return;
		}
	}
	clk_prepare(dispdrv->dsi_driver.clk);
	clk_enable(dispdrv->dsi_driver.clk);
}

void decon_clock_off(struct display_driver *dispdrv)
{
#ifndef DECON_VCLK_ECLK_MUX_MASKING
	clk_disable(dispdrv->decon_driver.clk);
	clk_unprepare(dispdrv->decon_driver.clk);
#else
	decon_vclk_eclk_mux_control(false);
#endif
}

void dsi_clock_off(struct display_driver *dispdrv)
{
	clk_disable(dispdrv->dsi_driver.clk);
	clk_unprepare(dispdrv->dsi_driver.clk);
}

void mic_clock_off(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	clk_disable(dispdrv->mic_driver.clk);
	clk_unprepare(dispdrv->mic_driver.clk);
#endif
}

struct pm_ops decon_pm_ops = {
	.clk_on		= decon_clock_on,
	.clk_off 	= decon_clock_off,
};
#ifdef CONFIG_DECON_MIC
struct pm_ops mic_pm_ops = {
	.clk_on		= mic_clock_on,
	.clk_off 	= mic_clock_off,
};
#endif
struct pm_ops dsi_pm_ops = {
	.clk_on		= dsi_clock_on,
	.clk_off 	= dsi_clock_off,
};

#else
int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	pm_runtime_get_sync(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}
#endif
