/* linux/arch/arm/mach-exynos/setup-fimc-sensor.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/gpio-cfg.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5422)
#include <mach/regs-clock-exynos5422.h>
#elif defined(CONFIG_SOC_EXYNOS5430)
#include <mach/regs-clock-exynos5430.h>
#endif

#include <mach/exynos-fimc-is.h>
#include <mach/exynos-fimc-is-sensor.h>

static int exynos_fimc_is_sensor_pin_control(struct platform_device *pdev,
	int pin, char *name, u32 act, u32 channel)
{
	int ret = 0;
	char ch_name[30];
	struct pinctrl *pinctrl_ch;

	snprintf(ch_name, sizeof(ch_name), "%s%d", name, channel);
	pr_info("%s(pin(%d), act(%d), ch(%s))\n", __func__, pin, act, ch_name);

	switch (act) {
	case PIN_PULL_NONE:
		break;
	case PIN_OUTPUT_HIGH:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_OUTPUT_HIGH");
			gpio_free(pin);
		}
		break;
	case PIN_OUTPUT_LOW:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			gpio_free(pin);
		}
		break;
	case PIN_INPUT:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_IN, "CAM_GPIO_INPUT");
			gpio_free(pin);
		}
		break;
	case PIN_RESET:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_RESET");
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 0);
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 1);
			usleep_range(1000, 1000);
			gpio_free(pin);
		}
		break;
	case PIN_FUNCTION:
		/* I2C(GPC2), CIS_CLK(GPD7) */
		pinctrl_ch = devm_pinctrl_get_select(&pdev->dev, ch_name);
		if (IS_ERR(pinctrl_ch))
			pr_err("%s: cam %s pins are not configured\n", __func__, ch_name);
		usleep_range(1000, 1000);
		break;
	case PIN_REGULATOR_ON:
		{
			struct regulator *regulator;

			regulator = regulator_get(&pdev->dev, name);
			if (IS_ERR(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			if (regulator_is_enabled(regulator)) {
				pr_warning("%s regulator is already enabled\n", name);
				regulator_put(regulator);
				return 0;
			}

			ret = regulator_enable(regulator);
			if (ret) {
				pr_err("%s : regulator_enable(%s) fail\n", __func__, name);
				regulator_put(regulator);
				return ret;
			}

			regulator_put(regulator);
		}
		break;
	case PIN_REGULATOR_OFF:
		{
			struct regulator *regulator;

			regulator = regulator_get(&pdev->dev, name);
			if (IS_ERR(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				return PTR_ERR(regulator);
			}

			if (!regulator_is_enabled(regulator)) {
				pr_warning("%s regulator is already disabled\n", name);
				regulator_put(regulator);
				return 0;
			}

			ret = regulator_disable(regulator);
			if (ret) {
				pr_err("%s : regulator_disable(%s) fail\n", __func__, name);
				regulator_put(regulator);
				return ret;
			}

			regulator_put(regulator);
		}
		break;
	default:
		pr_err("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_fimc_is_sensor_pins_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 enable)
{
	int ret = 0;
	u32 i = 0;
	struct exynos_sensor_pin (*pin_ctrls)[2][GPIO_CTRL_MAX];
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.platform_data);
	BUG_ON(enable > 1);
	BUG_ON(scenario > SENSOR_SCENARIO_MAX);

	pdata = pdev->dev.platform_data;
	pin_ctrls = pdata->pin_ctrls;

	for (i = 0; i < GPIO_CTRL_MAX; ++i) {
		if (pin_ctrls[scenario][enable][i].act == PIN_END) {
			pr_info("gpio cfg is end(%d)\n", i);
			break;
		}

		ret = exynos_fimc_is_sensor_pin_control(pdev,
			pin_ctrls[scenario][enable][i].pin,
			pin_ctrls[scenario][enable][i].name,
			pin_ctrls[scenario][enable][i].act,
			pdata->csi_ch);
		if (ret) {
			pr_err("exynos5_fimc_is_sensor_gpio(%d, %s, %d, %d) is fail(%d)",
				pin_ctrls[scenario][enable][i].pin,
				pin_ctrls[scenario][enable][i].name,
				pin_ctrls[scenario][enable][i].act,
				pdata->csi_ch,
				ret);
			goto p_err;
		}
	}

p_err:
	return ret;

}

#if defined(CONFIG_SOC_EXYNOS5422)
int exynos5422_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	pr_info("clk_cfg:(ch%d),scenario(%d)\n", channel, scenario);

	switch (channel) {
	case 0:
		/* MIPI-CSIS0 */
		fimc_is_set_parent_dt(pdev, "mout_gscl_wrap_a", "mout_mpll_ctrl");
		fimc_is_set_rate_dt(pdev, "dout_gscl_wrap_a", (532 * 1000000));
		fimc_is_get_rate_dt(pdev, "dout_gscl_wrap_a");
		break;
	case 1:
		/* FL1_550_CAM */
		fimc_is_set_parent_dt(pdev, "mout_aclk_fl1_550_cam", "mout_mpll_ctrl");
		fimc_is_set_rate_dt(pdev, "dout_aclk_fl1_550_cam", (76 * 1000000));
		fimc_is_set_parent_dt(pdev, "mout_aclk_fl1_550_cam_sw", "dout_aclk_fl1_550_cam");
		fimc_is_set_parent_dt(pdev, "mout_aclk_fl1_550_cam_user", "mout_aclk_fl1_550_cam_sw");
		fimc_is_set_rate_dt(pdev, "dout2_cam_blk_550", (38 * 1000000));

		/* MIPI-CSIS1 */
		fimc_is_set_parent_dt(pdev, "mout_gscl_wrap_b", "mout_mpll_ctrl");
		fimc_is_set_rate_dt(pdev, "dout_gscl_wrap_b", (76 * 1000000));
		fimc_is_get_rate_dt(pdev, "dout_gscl_wrap_b");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return ret;
}

int exynos5422_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos5422_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	return 0;
}

int exynos5422_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	u32 frequency;
	char div_name[30];
	char sclk_name[30];

	pr_info("%s:ch(%d)\n", __func__, channel);

	snprintf(div_name, sizeof(div_name), "dout_isp_sensor%d", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, "mout_isp_sensor", "fin_pll");
	fimc_is_set_rate_dt(pdev, div_name, (24 * 1000000));
	fimc_is_enable_dt(pdev, sclk_name);
	frequency = fimc_is_get_rate_dt(pdev, div_name);

	switch (channel) {
	case SENSOR_CONTROL_I2C0:
		fimc_is_enable_dt(pdev, "sclk_gscl_wrap_a");
		fimc_is_enable_dt(pdev, "clk_camif_top_fimcl0");
		fimc_is_enable_dt(pdev, "clk_camif_top_fimcl3");
		fimc_is_enable_dt(pdev, "gscl_fimc_lite0");
		fimc_is_enable_dt(pdev, "gscl_fimc_lite3");
		fimc_is_enable_dt(pdev, "clk_gscl_wrap_a");
		break;
	case SENSOR_CONTROL_I2C1:
	case SENSOR_CONTROL_I2C2:
		fimc_is_enable_dt(pdev, "sclk_gscl_wrap_b");
		fimc_is_enable_dt(pdev, "clk_camif_top_fimcl1");
		fimc_is_enable_dt(pdev, "gscl_fimc_lite1");
		fimc_is_enable_dt(pdev, "clk_gscl_wrap_b");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	if (scenario != SENSOR_SCENARIO_VISION) {
		fimc_is_enable_dt(pdev, "clk_3aa");
		fimc_is_enable_dt(pdev, "clk_camif_top_3aa");
		fimc_is_enable_dt(pdev, "clk_3aa_2");
		fimc_is_enable_dt(pdev, "clk_camif_top_3aa0");
	}
	fimc_is_enable_dt(pdev, "clk_camif_top_csis0");
	fimc_is_enable_dt(pdev, "clk_xiu_si_gscl_cam");
	fimc_is_enable_dt(pdev, "clk_noc_p_rstop_fimcl");

	pr_info("%s(%d, mclk : %d)\n", __func__, channel, frequency);

	return 0;
}

int exynos5422_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_disable_dt(pdev, sclk_name);

	switch (channel) {
	case SENSOR_CONTROL_I2C0:
		fimc_is_disable_dt(pdev, "sclk_gscl_wrap_a");
		fimc_is_disable_dt(pdev, "clk_camif_top_fimcl0");
		fimc_is_disable_dt(pdev, "clk_camif_top_fimcl3");
		fimc_is_disable_dt(pdev, "gscl_fimc_lite0");
		fimc_is_disable_dt(pdev, "gscl_fimc_lite3");
		fimc_is_disable_dt(pdev, "clk_gscl_wrap_a");
		break;
	case SENSOR_CONTROL_I2C2:
		fimc_is_disable_dt(pdev, "sclk_gscl_wrap_b");
		fimc_is_disable_dt(pdev, "clk_camif_top_fimcl1");
		fimc_is_disable_dt(pdev, "gscl_fimc_lite1");
		fimc_is_disable_dt(pdev, "clk_gscl_wrap_b");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return 0;
}
#elif defined(CONFIG_SOC_EXYNOS5430)
int exynos5430_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	if (scenario != SENSOR_SCENARIO_VISION)
		return ret;

	pr_info("clk_cfg(ch%d)\n", channel);

	switch (channel) {
	case 0:
		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "oscclk");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "oscclk");

		/* MIPI-CSIS0 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 1);

		/* FIMC-LITE0 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 1);

		/* FIMC-LITE3 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 1);

		/* ASYNC, FLITE, 3AA, SMMU, QE ... */
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 1);
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 1);

		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "aclk_cam0_333");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s4", "phyclk_rxbyteclkhs0_s4");

		/* MIPI-CSIS0 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis0_b", "mout_aclk_csis0_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis0", 552 * 1000000);

		/* FIMC-LITE0 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_a_b", "mout_aclk_lite_a_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_a", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_a", 276 * 1000000);

		/* FIMC-LITE3 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_d_b", "mout_aclk_lite_d_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_d", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_d", 276 * 1000000);

		/* ASYNC, FLITE, 3AA, SMMU, QE ... */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400", "mout_aclk_cam0_400_user");
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_400", 400 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_aclk_cam0_200", 200 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_cam0_50", 50 * 1000000);
		break;
	case 1:
		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "oscclk");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "oscclk");

		/* MIPI-CSIS1 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 1);

		/* FIMC-LITE1 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 1);

		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam0_333_user", "aclk_cam0_333");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2a", "phyclk_rxbyteclkhs0_s2a");

		/* MIPI-CSIS1 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis1_b", "mout_aclk_csis1_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis1", 552 * 1000000);

		/* FIMC-LITE1 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_lite_b_b", "mout_aclk_lite_b_a");
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_b", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_b", 276 * 1000000);
		break;
	case 2:
		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "oscclk");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "oscclk");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "oscclk");

		/*  MIPI-CSIS2 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 1);

		/* FIMC-LITE2 */
		fimc_is_set_rate_dt(pdev, "dout_aclk_lite_c", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_lite_c", 1);

		/* FIMC-LITE2 PIXELASYNC */
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 1);
		fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 1);
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 1);

		/* USER_MUX_SEL */
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_552_user", "aclk_cam1_552");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_400_user", "aclk_cam1_400");
		fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

		/* MIPI-CSIS PHY */
		fimc_is_set_parent_dt(pdev, "mout_phyclk_rxbyteclkhs0_s2b", "phyclk_rxbyteclkhs0_s2b");

		/*  MIPI-CSIS2 */
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_a", "mout_aclk_cam1_400_user");
		fimc_is_set_parent_dt(pdev, "mout_aclk_csis2_b", "mout_aclk_cam1_333_user");
		fimc_is_set_rate_dt(pdev, "dout_aclk_csis2_a", 333 * 1000000);

		/* FIMC-LITE2 PIXELASYNC */
		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_init_b", "mout_sclk_pixelasync_lite_c_init_a");
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c_init", 552 * 1000000);
		fimc_is_set_rate_dt(pdev, "dout_pclk_pixelasync_lite_c", 276 * 1000000);

		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_a", "mout_aclk_cam0_552_user");
		fimc_is_set_parent_dt(pdev, "mout_sclk_pixelasync_lite_c_b", "mout_aclk_cam0_333_user");
		fimc_is_set_rate_dt(pdev, "dout_sclk_pixelasync_lite_c", 333 * 1000000);
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return ret;
}

int exynos5430_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	switch (channel) {
	case 0:
		fimc_is_enable_dt(pdev, "aclk_csis0");
		fimc_is_enable_dt(pdev, "pclk_csis0");
		fimc_is_enable_dt(pdev, "gate_lite_a");
		fimc_is_enable_dt(pdev, "gate_lite_d");
		break;
	case 1:
		fimc_is_enable_dt(pdev, "aclk_csis1");
		fimc_is_enable_dt(pdev, "pclk_csis1");
		fimc_is_enable_dt(pdev, "gate_lite_b");
		break;
	case 2:
		fimc_is_enable_dt(pdev, "gate_csis2");
		fimc_is_enable_dt(pdev, "gate_lite_c");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return 0;
}

int exynos5430_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	switch (channel) {
	case 0:
		fimc_is_disable_dt(pdev, "aclk_csis0");
		fimc_is_disable_dt(pdev, "pclk_csis0");
		fimc_is_disable_dt(pdev, "gate_lite_a");
		fimc_is_disable_dt(pdev, "gate_lite_d");
		break;
	case 1:
		fimc_is_disable_dt(pdev, "aclk_csis1");
		fimc_is_disable_dt(pdev, "pclk_csis1");
		fimc_is_disable_dt(pdev, "gate_lite_b");
		break;
	case 2:
		fimc_is_disable_dt(pdev, "gate_csis2");
		fimc_is_disable_dt(pdev, "gate_lite_c");
		break;
	default:
		pr_err("channel is invalid(%d)\n", channel);
		break;
	}

	return 0;
}

int exynos5430_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	u32 frequency;
	char mux_name[30];
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(mux_name, sizeof(mux_name), "mout_sclk_isp_sensor%d", channel);
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, mux_name, "oscclk");
	fimc_is_set_rate_dt(pdev, div_a_name, 24 * 1000000);
	fimc_is_set_rate_dt(pdev, div_b_name, 24 * 1000000);
	frequency = fimc_is_get_rate_dt(pdev, sclk_name);

	pr_info("%s(%d, mclk : %d)\n", __func__, channel, frequency);

	return 0;
}

int exynos5430_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	char mux_name[30];
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(mux_name, sizeof(mux_name), "mout_sclk_isp_sensor%d", channel);
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, mux_name, "oscclk");
	fimc_is_set_rate_dt(pdev, div_a_name, 1);
	fimc_is_set_rate_dt(pdev, div_b_name, 1);
	fimc_is_get_rate_dt(pdev, sclk_name);

	return 0;
}
#endif

/* Wrapper functions */
int exynos_fimc_is_sensor_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430)
	exynos5430_fimc_is_sensor_iclk_cfg(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430)
	exynos5430_fimc_is_sensor_iclk_on(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430)
	exynos5430_fimc_is_sensor_iclk_off(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430)
	exynos5430_fimc_is_sensor_mclk_on(pdev, scenario, channel);
#endif
	return 0;
}

int exynos_fimc_is_sensor_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS5422)
	exynos5422_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#elif defined(CONFIG_SOC_EXYNOS5430)
	exynos5430_fimc_is_sensor_mclk_off(pdev, scenario, channel);
#endif
	return 0;
}
