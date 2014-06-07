/* linux/drivers/video/decon_display/decon_display_driver.c
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
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>
#include <linux/exynos_iovmm.h>

#include "decon_display_driver.h"
#include "decon_dt.h"
#include "decon_pm.h"

#ifdef CONFIG_SOC_EXYNOS5430
#include "decon_fb.h"
#else
#include "fimd_fb.h"
#endif

#include "decon_debug.h"

#ifdef CONFIG_OF
static const struct of_device_id decon_disp_device_table[] = {
	{ .compatible = "samsung,exynos5-disp_driver" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_disp_device_table);
#endif

static struct display_driver g_display_driver;

int init_display_decon_clocks(struct device *dev);
int enable_display_decon_clocks(struct device *dev);
int disable_display_decon_clocks(struct device *dev);
int enable_display_decon_runtimepm(struct device *dev);
int disable_display_decon_runtimepm(struct device *dev);
int enable_display_dsd_clocks(struct device *dev);
int disable_display_dsd_clocks(struct device *dev);
int init_display_driver_clocks(struct device *dev);
int enable_display_driver_clocks(struct device *dev);
int enable_display_driver_power(struct device *dev);
int disable_display_driver_power(struct device *dev);

int parse_display_driver_dt(struct platform_device *np, struct display_driver *ddp);
struct s3c_fb_driverdata *get_display_drvdata(void);
struct s3c_fb_platdata *get_display_platdata(void);
struct mipi_dsim_config *get_display_dsi_drvdata(void);
struct mipi_dsim_lcd_config *get_display_lcd_drvdata(void);
struct display_gpio *get_display_dsi_reset_gpio(void);
struct mic_config *get_display_mic_config(void);

extern int s5p_mipi_dsi_disable(struct mipi_dsim_device *dsim);
/* init display operations for pm and parsing functions */
static int init_display_operations(void)
{
#define DT_OPS g_display_driver.dt_ops
#define DSI_OPS g_display_driver.dsi_driver.dsi_ops
#define DECON_OPS g_display_driver.decon_driver.decon_ops
	DT_OPS.parse_display_driver_dt = parse_display_driver_dt;
	DT_OPS.get_display_drvdata = get_display_drvdata;
	DT_OPS.get_display_platdata = get_display_platdata;
	DT_OPS.get_display_dsi_drvdata = get_display_dsi_drvdata;
	DT_OPS.get_display_lcd_drvdata = get_display_lcd_drvdata;
	DT_OPS.get_display_dsi_reset_gpio = get_display_dsi_reset_gpio;
#ifdef CONFIG_DECON_MIC
	DT_OPS.get_display_mic_config = get_display_mic_config;
#endif

	DSI_OPS.init_display_driver_clocks = init_display_driver_clocks;
	DSI_OPS.enable_display_driver_clocks = enable_display_driver_clocks;
	DSI_OPS.enable_display_driver_power = enable_display_driver_power;
	DSI_OPS.disable_display_driver_power = disable_display_driver_power;

	DECON_OPS.init_display_decon_clocks = init_display_decon_clocks;
	DECON_OPS.enable_display_decon_clocks = enable_display_decon_clocks;
	DECON_OPS.disable_display_decon_clocks = disable_display_decon_clocks;
	DECON_OPS.enable_display_decon_runtimepm = enable_display_decon_runtimepm;
	DECON_OPS.disable_display_decon_runtimepm = disable_display_decon_runtimepm;
#ifdef CONFIG_SOC_EXYNOS5430
	DECON_OPS.enable_display_dsd_clocks = enable_display_dsd_clocks;
	DECON_OPS.disable_display_dsd_clocks = disable_display_dsd_clocks;
#endif
#undef DT_OPS
#undef DSI_OPS
#undef DECON_OPS

	return 0;
}

/* create_disp_components - create all components in display sub-system.
 * */
static int create_disp_components(struct platform_device *pdev)
{
	int ret = 0;

#ifdef CONFIG_DECON_MIC
	ret = create_decon_mic(pdev);
	if (ret < 0) {
		pr_err("display error: MIC create failed.");
		return ret;
	}
#endif

	/* IMPORTANT: MIPI-DSI component should be 1'st created. */
	ret = create_mipi_dsi_controller(pdev);
	if (ret < 0) {
		pr_err("display error: mipi-dsi controller create failed.");
		return ret;
	}

	ret = create_decon_display_controller(pdev);
	if (ret < 0) {
		pr_err("display error: display controller create failed.");
		return ret;
	}

	return ret;
}

/* disp_driver_fault_handler - fault handler for display device driver */
int disp_driver_fault_handler(struct iommu_domain *iodmn, struct device *dev,
	unsigned long addr, int id, void *param)
{
	struct display_driver *dispdrv;

	dispdrv = (struct display_driver*)param;
	decon_dump_registers(dispdrv);
	return 0;
}

/* register_debug_features - for registering debug features.
 * currently registered features are like as follows...
 * - iovmm falult handler
 * - ... */
static void register_debug_features(void)
{
	/* 1. fault handler registration */
	iovmm_set_fault_handler(g_display_driver.display_driver,
		disp_driver_fault_handler, &g_display_driver);
}

/* s5p_decon_disp_probe - probe function of the display driver */
static int s5p_decon_disp_probe(struct platform_device *pdev)
{
	int ret = -1;

	init_display_operations();

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	init_display_pm(&g_display_driver);
#endif

	/* parse display driver device tree & convers it to objects
	 * for each platform device */
	ret = g_display_driver.dt_ops.parse_display_driver_dt(pdev, &g_display_driver);

	GET_DISPDRV_OPS(&g_display_driver).init_display_driver_clocks(&pdev->dev);
	GET_DISPCTL_OPS(&g_display_driver).init_display_decon_clocks(&pdev->dev);

	create_disp_components(pdev);

	register_debug_features();

	return ret;
}

static int s5p_decon_disp_remove(struct platform_device *pdev)
{
	return 0;
}

static int display_driver_runtime_suspend(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	return s3c_fb_runtime_suspend(dev);
#else
	return 0;
#endif
}

static int display_driver_runtime_resume(struct device *dev)
{
#ifdef CONFIG_PM_RUNTIME
	return s3c_fb_runtime_resume(dev);
#else
	return 0;
#endif
}

#ifdef CONFIG_PM_SLEEP
static int display_driver_resume(struct device *dev)
{
	return s3c_fb_resume(dev);
}

static int display_driver_suspend(struct device *dev)
{
	return s3c_fb_suspend(dev);
}
#endif

static void display_driver_shutdown(struct platform_device *pdev)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	disp_pm_add_refcount(get_display_driver());
#endif
	s5p_mipi_dsi_disable(g_display_driver.dsi_driver.dsim);
}

static const struct dev_pm_ops s5p_decon_disp_ops = {
#ifdef CONFIG_PM_SLEEP
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = display_driver_suspend,
	.resume = display_driver_resume,
#endif
#endif
	.runtime_suspend	= display_driver_runtime_suspend,
	.runtime_resume		= display_driver_runtime_resume,
};

/* get_display_driver - for returning reference of display
 * driver context */
struct display_driver *get_display_driver(void)
{
	return &g_display_driver;
}

static struct platform_driver s5p_decon_disp_driver = {
	.probe = s5p_decon_disp_probe,
	.remove = s5p_decon_disp_remove,
	.shutdown = display_driver_shutdown,
	.driver = {
		.name = "s5p-decon-display",
		.owner = THIS_MODULE,
		.pm = &s5p_decon_disp_ops,
		.of_match_table = of_match_ptr(decon_disp_device_table),
	},
};

static int s5p_decon_disp_register(void)
{
	platform_driver_register(&s5p_decon_disp_driver);

	return 0;
}

static void s5p_decon_disp_unregister(void)
{
	platform_driver_unregister(&s5p_decon_disp_driver);
}
late_initcall(s5p_decon_disp_register);
module_exit(s5p_decon_disp_unregister);

MODULE_AUTHOR("Donggyun, ko <donggyun.ko@samsung.com>");
MODULE_DESCRIPTION("Samusung DECON-DISP driver");
MODULE_LICENSE("GPL");
