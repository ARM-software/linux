/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Hyosang Jung
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5430 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock-exynos5430.h>
#include <mach/regs-pmu.h>

#include "clk.h"
#include "clk-pll.h"

struct clk_enabler {
	struct clk		*clk;
	struct list_head	node;
};

static LIST_HEAD(clk_enabler_list);

static inline void add_enabler(const char *name)
{
	struct clk_enabler *ce;
	struct clk *clock;
	clock = __clk_lookup(name);

	if (IS_ERR(clock))
		return;

	ce = kzalloc(sizeof(struct clk_enabler), GFP_KERNEL);
	if (!ce)
		return;

	ce->clk = clock;
	list_add(&ce->node, &clk_enabler_list);
}

static void top_clk_enable(void)
{
	struct clk_enabler *ce;

	add_enabler("aclk_g2d_400");
	add_enabler("aclk_g2d_266");
	add_enabler("aclk_mfc0_333");
	add_enabler("aclk_mfc1_333");
	add_enabler("aclk_hevc_400");
	add_enabler("aclk_isp_400");
	add_enabler("aclk_isp_dis_400");
	add_enabler("aclk_cam0_552");
	add_enabler("aclk_cam0_400");
	add_enabler("aclk_cam0_333");
	add_enabler("aclk_cam1_552");
	add_enabler("aclk_cam1_400");
	add_enabler("aclk_cam1_333");
	add_enabler("aclk_gscl_333");
	add_enabler("aclk_gscl_111");
	add_enabler("aclk_fsys_200");
	add_enabler("aclk_mscl_400");
	add_enabler("aclk_peris_66");
	add_enabler("aclk_peric_66");
	add_enabler("aclk_imem_266");
	add_enabler("aclk_imem_200");

	add_enabler("sclk_jpeg_top");

	add_enabler("sclk_isp_spi0_top");
	add_enabler("sclk_isp_spi1_top");
	add_enabler("sclk_isp_uart_top");
	add_enabler("sclk_isp_sensor0");
	add_enabler("sclk_isp_sensor1");
	add_enabler("sclk_isp_sensor2");

	add_enabler("sclk_hdmi_spdif_top");

	add_enabler("sclk_usbdrd30_top");
	add_enabler("sclk_ufsunipro_top");
	add_enabler("sclk_mmc0_top");
	add_enabler("sclk_mmc1_top");
	add_enabler("sclk_mmc2_top");

	add_enabler("sclk_spi0_top");
	add_enabler("sclk_spi1_top");
	add_enabler("sclk_spi2_top");
	add_enabler("sclk_uart0_top");
	add_enabler("sclk_uart1_top");
	add_enabler("sclk_uart2_top");
	add_enabler("sclk_pcm1_top");
	add_enabler("sclk_i2s1_top");
	add_enabler("sclk_spdif_top");
	add_enabler("sclk_slimbus_top");

	add_enabler("sclk_hpm_mif");
	add_enabler("aclk_cpif_200");
	add_enabler("aclk_disp_333");
	add_enabler("aclk_disp_222");
	add_enabler("aclk_bus1_400");
	add_enabler("aclk_bus2_400");

	add_enabler("sclk_decon_eclk_mif");
	add_enabler("sclk_decon_vclk_mif");
	add_enabler("sclk_dsd_mif");

	list_for_each_entry(ce, &clk_enabler_list, node) {
		clk_prepare(ce->clk);
		clk_enable(ce->clk);
	}

	pr_info("Clock enables : TOP, MIF\n");
}

static void clkout_init_clock(void)
{
	/* ACLK_IMEM_200 / 10 */
	writel(0x10907, EXYNOS5430_VA_CMU_TOP + 0x0C00);
	writel(0x00700, EXYNOS_PMU_DEBUG);
}

static void aud_init_clock(void)
{
	/* Enable AUD_PLL (Default 393.216MHz)*/
	writel((1 << 31) | readl(EXYNOS5430_AUD_PLL_CON0),
					EXYNOS5430_AUD_PLL_CON0);

	/* AUD0 */
	exynos_set_parent("mout_aud_pll", "fout_aud_pll");
	exynos_set_parent("mout_aud_pll_user", "mout_aud_pll");
	exynos_set_parent("mout_aud_dpll_user", "fin_pll");
	exynos_set_parent("mout_aud_pll_sub", "mout_aud_pll_user");
	exynos_set_rate("dout_aud_ca5", 393216020);
	exynos_set_rate("dout_aclk_aud", 133000000);
	exynos_set_rate("dout_aud_pclk_dbg", 133000000);

	/* AUD1 */
	exynos_set_parent("mout_sclk_i2s_a", "mout_aud_pll_sub");
	exynos_set_parent("mout_sclk_pcm_a", "mout_aud_pll_sub");
	exynos_set_rate("dout_sclk_i2s", 49152004);
	exynos_set_rate("dout_sclk_pcm", 2048002);
	exynos_set_rate("dout_sclk_slimbus_aud", 24576002);
	exynos_set_rate("dout_sclk_uart", 133000000);

	/* TOP1 */
	exynos_set_parent("mout_aud_pll_user_top", "mout_aud_pll");
	exynos_set_parent("mout_aud_dpll_user_top", "fin_pll");

	/* TOP_PERIC1 */
	exynos_set_parent("mout_sclk_audio0", "mout_aud_pll_user_top");
	exynos_set_parent("mout_sclk_audio1", "mout_aud_pll_user_top");
	exynos_set_parent("mout_sclk_spdif", "dout_sclk_audio0");
	exynos_set_rate("dout_sclk_audio0", 24576002);
	exynos_set_rate("dout_sclk_audio1", 49152004);
	exynos_set_rate("dout_sclk_pcm1", 2048002);
	exynos_set_rate("dout_sclk_i2s1", 49152004);
}

void mfc_init_clock(void)
{
	exynos_set_parent("mout_aclk_mfc0_333_user", "aclk_mfc0_333");
	exynos_set_parent("mout_aclk_mfc1_333_user", "aclk_mfc1_333");
	exynos_set_parent("mout_aclk_hevc_400_user", "aclk_hevc_400");

	exynos_set_parent("mout_mfc_pll_user", "dout_mfc_pll");
	exynos_set_parent("mout_bus_pll_user", "dout_bus_pll");
}

void adma_init_clock(void)
{
	void __iomem *aud_base;
	unsigned int reg;

	aud_base = ioremap(0x11400000, SZ_4K);

	reg = __raw_readl(aud_base + 0x8);
	reg &= ~0x1;
	__raw_writel(reg, aud_base + 0x8);
	reg |= 0x1;
	__raw_writel(reg, aud_base + 0x8);

	iounmap(aud_base);
}

void cam0_init_clock(void)
{
	/* ACLK_CAM0_552 */
	exynos_set_rate("dout_aclk_cam0_552", 552 * 1000000);

	/* ACLK_CAM0_400 */
	exynos_set_rate("dout_aclk_cam0_400_top", 400 * 1000000);

	/* ACLK_CAM0_333 */
	exynos_set_rate("dout_aclk_cam0_333", 333 * 1000000);
}

void cam1_init_clock(void)
{
	/* ACLK_CAM1_552 */
	exynos_set_parent("mout_aclk_cam1_552_a", "mout_isp_pll");
	exynos_set_parent("mout_aclk_cam1_552_b", "mout_aclk_cam1_552_a");
	exynos_set_rate("dout_aclk_cam1_552", 552 * 1000000);

	/* ACLK_CAM1_400 */
	exynos_set_rate("dout_aclk_cam1_400", 400 * 1000000);

	/* ACLK_CAM1_333 */
	exynos_set_rate("dout_aclk_cam1_333", 333 * 1000000);
}

void isp_init_clock(void)
{
	/* ACLK_ISP_400 */
	exynos_set_parent("mout_aclk_isp_400", "mout_bus_pll_user");
	exynos_set_rate("dout_aclk_isp_400", 400 * 1000000);

	/* ACLK_ISP_DIS_400 */
	exynos_set_parent("mout_aclk_isp_dis_400", "mout_bus_pll_user");
	exynos_set_rate("dout_aclk_isp_dis_400", 400 * 1000000);
}

void g3d_init_clock(void)
{
	exynos_set_parent("dout_aclk_g3d", "mout_g3d_pll");

	__raw_writel(__raw_readl(EXYNOS5430_G3D_PLL_CON0) | (0xe << 28),
			EXYNOS5430_G3D_PLL_CON0);
	while (__raw_readl(EXYNOS5430_G3D_PLL_CON0) | (0x1 << 29))
		break;

	exynos_set_parent("mout_g3d_pll", "fin_pll");
	exynos_set_parent("mout_g3d_pll", "fout_g3d_pll");

	__raw_writel(0x1F, EXYNOS5430_ENABLE_IP_G3D0);
}

void gsc_init_clock(void)
{
	exynos_set_parent("mout_aclk_gscl_333", "mout_mfc_pll_user");

	exynos_set_parent("dout_aclk_gscl_333", "mout_aclk_gscl_333");
	exynos_set_parent("dout_aclk_gscl_111", "mout_aclk_gscl_333");

	exynos_set_parent("aclk_gscl_333", "dout_aclk_gscl_333");
	exynos_set_parent("aclk_gscl_111", "dout_aclk_gscl_111");

	exynos_set_parent("mout_aclk_gscl_333_user", "aclk_gscl_333");
	exynos_set_parent("mout_aclk_gscl_111_user", "aclk_gscl_111");

	exynos_set_rate("dout_aclk_gscl_333", 333 * 1000000);
	exynos_set_rate("dout_aclk_gscl_111", 111 * 1000000);
}

static void spi_clock_init(void)
{
	exynos_set_parent("mout_sclk_spi0", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi1", "mout_bus_pll_user");
	exynos_set_parent("mout_sclk_spi2", "mout_bus_pll_user");

	/* dout_sclk_spi_a should be 100Mhz */
	exynos_set_rate("dout_sclk_spi0_a", 100000000);
	exynos_set_rate("dout_sclk_spi1_a", 100000000);
	exynos_set_rate("dout_sclk_spi2_a", 100000000);
}

void usb_init_clock(void)
{
	exynos_set_parent("mout_sclk_usbdrd30_user", "oscclk");

	exynos_set_parent("mout_phyclk_usbdrd30_udrd30_phyclock",
			"phyclk_usbdrd30_udrd30_phyclock_phy");
	exynos_set_parent("mout_phyclk_usbdrd30_udrd30_pipe_pclk",
			"phyclk_usbdrd30_udrd30_pipe_pclk_phy");

	exynos_set_parent("mout_phyclk_usbhost20_phy_freeclk",
			"phyclk_usbhost20_phy_freeclk_phy");
	exynos_set_parent("mout_phyclk_usbhost20_phy_phyclock",
			"phyclk_usbhost20_phy_phyclock_phy");
	exynos_set_parent("mout_phyclk_usbhost20_phy_clk48mohci",
			"phyclk_usbhost20_phy_clk48mohci_phy");
	exynos_set_parent("mout_phyclk_usbhost20_phy_hsic1",
			"phyclk_usbhost20_phy_hsic1_phy");
}

void mscl_init_clock(void)
{
	exynos_set_parent("mout_aclk_mscl_400_a", "mout_bus_pll_user");
	exynos_set_parent("mout_aclk_mscl_400_b", "mout_aclk_mscl_400_a");
	exynos_set_parent("dout_aclk_mscl_400", "mout_aclk_mscl_400_b");
	exynos_set_parent("aclk_mscl_400", "dout_aclk_mscl_400");
	exynos_set_parent("mout_aclk_mscl_400_user", "aclk_mscl_400");
	exynos_set_parent("dout_pclk_mscl", "mout_aclk_mscl_400_user");

	exynos_set_parent("aclk_m2mscaler0", "mout_aclk_mscl_400_user");
	exynos_set_parent("aclk_m2mscaler1", "mout_aclk_mscl_400_user");
	exynos_set_parent("pclk_m2mscaler0", "dout_pclk_mscl");
	exynos_set_parent("pclk_m2mscaler1", "dout_pclk_mscl");

	exynos_set_parent("aclk_jpeg", "mout_aclk_mscl_400_user");
	exynos_set_parent("pclk_jpeg", "dout_pclk_mscl");

	exynos_set_rate("dout_aclk_mscl_400", 400 * 1000000);
	exynos_set_rate("dout_pclk_mscl", 100 * 1000000);

	pr_debug("scaler_0: aclk_m2mscaler0 %d pclk_m2mscaler0 %d\n",
			exynos_get_rate("aclk_m2mscaler0"),
			exynos_get_rate("pclk_m2mscaler0"));

	pr_debug("scaler_1: aclk_m2mscaler1 %d pclk_m2mscaler1 %d\n",
			exynos_get_rate("aclk_m2mscaler1"),
			exynos_get_rate("pclk_m2mscaler1"));

	pr_debug("jpeg: aclk_jpeg %d pclk_jpeg %d\n",
			exynos_get_rate("aclk_jpeg"),
			exynos_get_rate("pclk_jpeg"));
}

void g2d_init_clock(void)
{
	int clk_rate1;
	int clk_rate2;

	if (exynos_set_parent("mout_aclk_g2d_400_a", "mout_bus_pll_user"))
		pr_err("Unable to set clock %s's parent %s\n"
				,"mout_aclk_g2d_400_a", "mout_bus_pll_user");

	if (exynos_set_parent("mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a"))
		pr_err("Unable to set clock %s's parent %s\n"
				,"mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a");

	if (exynos_set_parent("mout_aclk_g2d_400_user", "aclk_g2d_400"))
		pr_err("Unable to set clock %s's parent %s\n"
				,"mout_aclk_g2d_400_user", "aclk_g2d_400");

	if (exynos_set_parent("mout_aclk_g2d_266_user", "aclk_g2d_266"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_266_user", "aclk_g2d_266");

	if (exynos_set_rate("dout_aclk_g2d_400", 400 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_400");

	if (exynos_set_rate("dout_aclk_g2d_266", 267 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_266");

	clk_rate1 = exynos_get_rate("aclk_g2d_400");
	clk_rate2 = exynos_get_rate("aclk_g2d_266");

	pr_debug("[%s:%d] aclk_g2d_400:%d, aclk_g2d_266:%d\n"
			, __func__, __LINE__, clk_rate1, clk_rate2);
}

void jpeg_init_clock(void)
{
	if (exynos_set_parent("mout_sclk_jpeg_a", "mout_bus_pll_user"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_a", "mout_bus_pll_user");

	if (exynos_set_parent("mout_sclk_jpeg_b", "mout_sclk_jpeg_a"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_b", "mout_sclk_jpog_a");

	if (exynos_set_parent("mout_sclk_jpeg_c", "mout_sclk_jpeg_b"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_c", "mout_sclk_jpeg_b");

	if (exynos_set_parent("dout_sclk_jpeg", "mout_sclk_jpeg_c"))
		pr_err("Unable to set parent %s of clock %s\n",
				"dout_sclk_jpeg", "mout_sclk_jpeg_c");

	if (exynos_set_parent("mout_sclk_jpeg_user", "dout_sclk_jpeg"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_user", "dout_sclk_jpeg");

	if (exynos_set_rate("dout_sclk_jpeg", 400 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_sclk_jpeg");

	pr_debug("jpeg: sclk_jpeg %d\n", exynos_get_rate("dout_sclk_jpeg"));
}

void cpif_init_clock(void)
{
	exynos_set_parent("mout_phyclk_lli_tx0_symbol_user",
			  "phyclk_lli_tx0_symbol");
	exynos_set_parent("mout_phyclk_lli_rx0_symbol_user",
			  "phyclk_lli_rx0_symbol");
}

void crypto_init_clock(void)
{
	exynos_set_rate("dout_aclk_imem_266", 267*1000000);
	exynos_set_rate("dout_aclk_imem_200", 200*1000000);
}

void __init exynos5430_clock_init(void)
{
	top_clk_enable();
	mfc_init_clock();
	clkout_init_clock();
	aud_init_clock();
	adma_init_clock();
	cam0_init_clock();
	cam1_init_clock();
	isp_init_clock();
	g3d_init_clock();
	gsc_init_clock();
	/* spi clock init */
	spi_clock_init();
	usb_init_clock();
	mscl_init_clock();
	g2d_init_clock();
	jpeg_init_clock();
	cpif_init_clock();
	crypto_init_clock();
}
