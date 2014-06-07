/* linux/drivers/decon_display/decon_mic.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Haowei Li <haowei.li@samsung.com>
 *
 * Samsung MIC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>

#include "decon_mic.h"
#include "decon_display_driver.h"
#include "decon_mipi_dsi_lowlevel.h"
#include "decon_mipi_dsi.h"
#include "regs-mipidsim.h"
#include "decon_fb.h"
#include "decon_dt.h"
#include "decon_pm.h"

#ifdef CONFIG_OF
static const struct of_device_id exynos5_mic[] = {
	{ .compatible = "samsung,exynos5-mic" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_mic);
#endif

enum mic_on_off {
	DECON_MIC_OFF = 0,
	DECON_MIC_ON = 1
};

struct decon_mic {
	struct device *dev;
	void __iomem *reg_base;
	struct decon_lcd *lcd;
	struct mic_config *mic_config;
	bool decon_mic_on;
};

struct decon_mic *mic_for_decon;
EXPORT_SYMBOL(mic_for_decon);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int decon_mic_hibernation_power_on(struct display_driver *dispdrv);
int decon_mic_hibernation_power_off(struct display_driver *dispdrv);
#endif

#ifdef CONFIG_SOC_EXYNOS5430
static int decon_mic_set_sys_reg(struct decon_mic *mic, bool enable)
{
	u32 data;
	void __iomem *sysreg_va;

	sysreg_va = ioremap(mic->mic_config->sysreg1, 0x4);

	if (enable) {
		data = readl(sysreg_va) & ~(0xf);
		data |= (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0);
		writel(data, sysreg_va);
		iounmap(sysreg_va);

		sysreg_va = ioremap(mic->mic_config->sysreg2, 0x4);
		writel(0x80000000, sysreg_va);
		iounmap(sysreg_va);
	} else {
		data = readl(sysreg_va) & ~(0xf);
		writel(data, sysreg_va);
		iounmap(sysreg_va);
	}

	return 0;
}

static void decon_mic_set_image_size(struct decon_mic *mic)
{
	u32 data = 0;
	struct decon_lcd *lcd = mic->lcd;

	data = (lcd->yres << DECON_MIC_IMG_V_SIZE_SHIFT)
		| (lcd->xres << DECON_MIC_IMG_H_SIZE_SHIFT);

	writel(data, mic->reg_base + DECON_MIC_IMG_SIZE);
}

static unsigned int decon_mic_calc_bs_size(struct decon_mic *mic)
{
	struct decon_lcd *lcd = mic->lcd;
	u32 temp1, temp2, bs_size;

	temp1 = lcd->xres / 4 * 2;
	temp2 = lcd->xres % 4;
	bs_size = temp1 + temp2;

	return bs_size;
}

static void decon_mic_set_2d_bit_stream_size(struct decon_mic *mic)
{
	u32 data;

	data = decon_mic_calc_bs_size(mic);

	writel(data, mic->reg_base + DECON_MIC_2D_OUTPUT_TIMING_2);
}

static void decon_mic_set_mic_base_operation(struct decon_mic *mic, bool enable)
{
	u32 data = readl(mic->reg_base);
	struct decon_lcd *lcd = mic->lcd;

	if (enable) {
		data |= DECON_MIC_OLD_CORE | DECON_MIC_CORE_ENABLE
			| DECON_MIC_UPDATE_REG | DECON_MIC_ON_REG;

		if (lcd->mode == COMMAND_MODE)
			data |= DECON_MIC_COMMAND_MODE;
		else
			data |= DECON_MIC_VIDEO_MODE;
	} else {
		data &= ~DECON_MIC_CORE_ENABLE;
		data |= DECON_MIC_UPDATE_REG;
	}

	writel(data, mic->reg_base);
}

int create_decon_mic(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct display_driver *dispdrv;
	struct decon_mic *mic;
	struct resource *res;

	dispdrv = get_display_driver();

	mic = devm_kzalloc(dev, sizeof(struct decon_mic), GFP_KERNEL);
	if (!mic) {
		dev_err(dev, "no memory for mic driver");
		return -ENOMEM;
	}

	mic->dev = dev;

	mic->lcd = decon_get_lcd_info();

	mic->mic_config = dispdrv->dt_ops.get_display_mic_config();

	mic->decon_mic_on = false;

	res = dispdrv->mic_driver.regs;
	if (!res) {
		dev_err(dev, "failed to find resource\n");
		return -ENOENT;
	}

	mic->reg_base = ioremap(res->start, resource_size(res));
	if (!mic->reg_base) {
		dev_err(dev, "failed to map registers\n");
		return -ENXIO;
	}

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);

	decon_mic_set_image_size(mic);

	decon_mic_set_2d_bit_stream_size(mic);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic_for_decon = mic;

	mic->decon_mic_on = true;

	dispdrv->mic_driver.mic = mic;
#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	dispdrv->mic_driver.ops->pwr_on = decon_mic_hibernation_power_on;
	dispdrv->mic_driver.ops->pwr_off = decon_mic_hibernation_power_off;
#endif

	dev_info(dev, "MIC driver has been probed\n");
	return 0;
}

int decon_mic_enable(struct decon_mic *mic)
{
	if (mic->decon_mic_on == true)
		return 0;

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);

	decon_mic_set_image_size(mic);

	decon_mic_set_2d_bit_stream_size(mic);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic->decon_mic_on = true;

	dev_info(mic->dev, "MIC driver is ON;\n");

	return 0;
}

int decon_mic_disable(struct decon_mic *mic)
{
	if (mic->decon_mic_on == false)
		return 0;
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	dev_info(mic->dev, "MIC driver is OFF;\n");

	return 0;
}

int decon_mic_sw_reset(struct decon_mic *mic)
{
	void __iomem *regs = mic->reg_base + DECON_MIC_OP;

	u32 data = readl(regs);

	data |= DECON_MIC_SW_RST;
	writel(data, regs);

	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int decon_mic_hibernation_power_on(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);
	decon_mic_set_image_size(mic);
	decon_mic_set_2d_bit_stream_size(mic);
	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic->decon_mic_on = true;

	return 0;
}

int decon_mic_hibernation_power_off(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF);
	decon_mic_sw_reset(mic);
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	return 0;
}
#endif
#else
int decon_mic_sw_reset(struct decon_mic *mic)
{
	void __iomem *regs = mic->reg_base + DECON_MIC_OP;

	u32 data = readl(regs);

	data |= DECON_MIC_SW_RST;
	writel(data, regs);

	return 0;
}

static int decon_mic_set_sys_reg(struct decon_mic *mic, bool enable)
{
	return 0;
}

static void decon_mic_set_image_size(struct decon_mic *mic)
{
	u32 data = 0;
	struct decon_lcd *lcd = mic->lcd;

	data = (lcd->yres << DECON_MIC_IMG_V_SIZE_SHIFT)
		| (lcd->xres << DECON_MIC_IMG_H_SIZE_SHIFT);

	writel(data, mic->reg_base + DECON_MIC_IMG_SIZE);
}

static unsigned int decon_mic_calc_bs_size(struct decon_mic *mic)
{
	struct decon_lcd *lcd = mic->lcd;
	u32 temp1, temp2, bs_size;

	temp1 = lcd->xres / 4 * 2;
	temp2 = lcd->xres % 4;
	bs_size = temp1 + temp2;

	return bs_size;
}

static void decon_mic_set_mic_base_operation(struct decon_mic *mic, bool enable)
{
	struct decon_lcd *lcd = mic->lcd;
	u32 data = readl(mic->reg_base);

	if (enable) {
		data &= ~(DECON_MIC_CORE_MASK | DECON_MIC_MODE_MASK |
			DECON_MIC_CORE_NEW_MASK | DECON_MIC_PSR_MASK |
			DECON_MIC_BS_SWAP_MASK | DECON_MIC_ON_MASK |
			DECON_MIC_UPDATE_REG_MASK);
		data |= DECON_MIC_CORE_ENABLE |
			DECON_MIC_NEW_CORE | DECON_MIC_ON_REG | DECON_MIC_UPDATE_REG;

		if (lcd->mode == COMMAND_MODE)
			data |= DECON_MIC_COMMAND_MODE;
		else
			data |= DECON_MIC_VIDEO_MODE;
	} else {
		data &= ~DECON_MIC_CORE_ENABLE;
		data |= DECON_MIC_UPDATE_REG;
	}

	writel(data, mic->reg_base);
}

static void decon_mic_set_porch_timing(struct decon_mic *mic)
{
	struct decon_lcd *lcd = mic->lcd;
	u32 data, v_period, h_period;

	v_period = lcd->vsa + lcd->yres + lcd->vbp + lcd->vfp;
	data = lcd->vsa << DECON_MIC_V_PULSE_WIDTH_SHIFT
			| v_period << DECON_MIC_V_PERIOD_LINE_SHIFT;
	writel(data, mic->reg_base + DECON_MIC_V_TIMING_0);

	data = lcd->vbp << DECON_MIC_V_VBP_SIZE_SHIFT
			| lcd->vfp << DECON_MIC_V_VFP_SIZE_SHIFT;
	writel(data, mic->reg_base + DECON_MIC_V_TIMING_1);

	h_period = lcd->hsa + lcd->xres + lcd->hbp + lcd->hfp;
	data = lcd->hsa << DECON_MIC_INPUT_H_PULSE_WIDTH_SHIFT
			| h_period << DECON_MIC_INPUT_H_PERIOD_PIXEL_SHIFT;

	writel(data, mic->reg_base + DECON_MIC_INPUT_TIMING_0);

	data = lcd->hbp << DECON_MIC_INPUT_HBP_SIZE_SHIFT
			| lcd->hfp << DECON_MIC_INPUT_HFP_SIZE_SHIFT;

	writel(data, mic->reg_base + DECON_MIC_INPUT_TIMING_1);
}

static void decon_mic_set_output_timing(struct decon_mic *mic)
{
	struct decon_lcd *lcd = mic->lcd;
	u32 data, h_period_2d;
	u32 hsa_2d = lcd->hsa;
	u32 hbp_2d = lcd->hbp;
	u32 bs_2d = decon_mic_calc_bs_size(mic);
	u32 hfp_2d = lcd->hfp + bs_2d;

	h_period_2d = hsa_2d + hbp_2d + bs_2d + hfp_2d;

	data = hsa_2d << DECON_MIC_OUT_H_PULSE_WIDTH_SHIFT
			| h_period_2d << DECON_MIC_OUT_H_PERIOD_PIXEL_SHIFT;

	writel(data, mic->reg_base + DECON_MIC_2D_OUTPUT_TIMING_0);

	data = hbp_2d << DECON_MIC_OUT_HBP_SIZE_SHIFT
			| hfp_2d << DECON_MIC_OUT_HFP_SIZE_SHIFT;

	writel(data, mic->reg_base + DECON_MIC_2D_OUTPUT_TIMING_1);

	writel(bs_2d, mic->reg_base + DECON_MIC_2D_OUTPUT_TIMING_2);
}

static void decon_mic_set_alg_param(struct decon_mic *mic)
{

}

int decon_mic_enable(struct decon_mic *mic)
{
	if (mic->decon_mic_on == true)
		return 0;

	decon_mic_sw_reset(mic);
	decon_mic_set_sys_reg(mic, DECON_MIC_ON);
	decon_mic_set_porch_timing(mic);
	decon_mic_set_image_size(mic);
	decon_mic_set_output_timing(mic);
	decon_mic_set_alg_param(mic);
	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic->decon_mic_on = true;

	dev_info(mic->dev, "MIC driver is ON;\n");

	return 0;
}

int decon_mic_disable(struct decon_mic *mic)
{
	if (mic->decon_mic_on == false)
		return 0;
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	dev_info(mic->dev, "MIC driver is OFF;\n");

	return 0;
}

int create_decon_mic(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct display_driver *dispdrv;
	struct decon_mic *mic;
	struct resource *res;
	int ret;

	dispdrv = get_display_driver();

	mic = devm_kzalloc(dev, sizeof(struct decon_mic), GFP_KERNEL);
	if (!mic) {
		dev_err(dev, "no memory for mic driver");
		return -ENOMEM;
	}

	mic->dev = dev;

	mic->lcd = decon_get_lcd_info();

	mic->mic_config = dispdrv->dt_ops.get_display_mic_config();

	mic->decon_mic_on = false;

	res = dispdrv->mic_driver.regs;
	if (!res) {
		dev_err(dev, "failed to find resource\n");
		ret = -ENOENT;
	}

	mic->reg_base = ioremap(res->start, resource_size(res));
	if (!mic->reg_base) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
	}

	decon_mic_enable(mic);
	mic_for_decon = mic;

	mic->decon_mic_on = true;

	dispdrv->mic_driver.mic = mic;
	dev_info(dev, "MIC driver has been probed\n");
	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
int decon_mic_hibernation_power_on(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_sw_reset(mic);
	decon_mic_set_sys_reg(mic, DECON_MIC_ON);
	decon_mic_set_porch_timing(mic);
	decon_mic_set_image_size(mic);
	decon_mic_set_output_timing(mic);
	decon_mic_set_alg_param(mic);
	decon_mic_set_mic_base_operation(mic, DECON_MIC_ON);

	mic->decon_mic_on = true;

	return 0;
}

int decon_mic_hibernation_power_off(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF);
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	return 0;
}
#endif
#endif

MODULE_AUTHOR("Haowei Li <Haowei.li@samsung.com>");
MODULE_DESCRIPTION("Samsung MIC driver");
MODULE_LICENSE("GPL");
