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
#ifdef CONFIG_SOC_EXYNOS5430_REV_0
	/* AUD0 */
	exynos_set_parent("mout_aud_pll_user", "mout_aud_pll");
	exynos_set_parent("mout_aud_pll_sub", "mout_aud_pll_user");

	/* AUD1 */
	exynos_set_parent("mout_sclk_i2s", "mout_aud_pll_user");
	exynos_set_parent("mout_sclk_pcm", "mout_aud_pll_user");
#else
	/* AUD0 */
	exynos_set_parent("mout_aud_pll_user", "fout_aud_pll");

	/* AUD1 */
	exynos_set_parent("mout_sclk_aud_i2s", "mout_aud_pll_user");
	exynos_set_parent("mout_sclk_aud_pcm", "mout_aud_pll_user");
#endif

	exynos_set_rate("fout_aud_pll", 196608010);
	exynos_set_rate("dout_aud_ca5", 196608010);
	exynos_set_rate("dout_aclk_aud", 133000000);
	exynos_set_rate("dout_pclk_dbg_aud", 133000000);

	exynos_set_rate("dout_sclk_aud_i2s", 49152004);
	exynos_set_rate("dout_sclk_aud_pcm", 2048002);
	exynos_set_rate("dout_sclk_aud_slimbus", 24576002);
	exynos_set_rate("dout_sclk_aud_uart", 196608010);

	/* TOP1 */
	exynos_set_parent("mout_aud_pll", "fout_aud_pll");
	exynos_set_parent("mout_aud_pll_user_top", "mout_aud_pll");

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
	exynos_set_parent("mout_aclk_mfc0_333_a", "mout_mfc_pll_user");
	exynos_set_parent("mout_aclk_mfc0_333_b", "mout_aclk_mfc0_333_a");
	exynos_set_parent("mout_aclk_mfc0_333_c", "mout_aclk_mfc0_333_b");

	exynos_set_parent("mout_aclk_mfc1_333_a", "mout_mfc_pll_user");
	exynos_set_parent("mout_aclk_mfc1_333_b", "mout_aclk_mfc1_333_a");
	exynos_set_parent("mout_aclk_mfc1_333_c", "mout_aclk_mfc1_333_b");

	exynos_set_parent("mout_aclk_mfc0_333_user", "aclk_mfc0_333");
	exynos_set_parent("mout_aclk_mfc1_333_user", "aclk_mfc1_333");

	exynos_set_parent("mout_aclk_hevc_400", "mout_bus_pll_user");
	exynos_set_parent("mout_aclk_hevc_400_user", "aclk_hevc_400");

	exynos_set_rate("dout_aclk_mfc0_333", 333*1000000);
	exynos_set_rate("dout_aclk_mfc1_333", 333*1000000);
	exynos_set_rate("dout_aclk_hevc_400", 400*1000000);

	pr_debug("mfc: aclk_mfc0_333 %d aclk_mfc1_333 %d aclk_hevc_400 %d\n",
			exynos_get_rate("aclk_mfc0_333"),
			exynos_get_rate("aclk_mfc1_333"),
			exynos_get_rate("aclk_hevc_400"));
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
	exynos_set_rate("dout_aclk_cam0_400", 413 * 1000000);

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
	exynos_set_rate("dout_aclk_cam1_400", 413 * 1000000);

	/* ACLK_CAM1_333 */
	exynos_set_rate("dout_aclk_cam1_333", 333 * 1000000);
}

void isp_init_clock(void)
{
	/* ACLK_ISP_400 */
	exynos_set_parent("mout_aclk_isp_400", "mout_bus_pll_user");
	exynos_set_rate("dout_aclk_isp_400", 413 * 1000000);

	/* ACLK_ISP_DIS_400 */
	exynos_set_parent("mout_aclk_isp_dis_400", "mout_bus_pll_user");
	exynos_set_rate("dout_aclk_isp_dis_400", 413 * 1000000);
}

void g3d_init_clock(void)
{
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

	exynos_set_rate("dout_aclk_mscl_400", 413 * 1000000);
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
				, "mout_aclk_g2d_400_a", "mout_bus_pll_user");

	if (exynos_set_parent("mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a");

	if (exynos_set_parent("mout_aclk_g2d_400_user", "aclk_g2d_400"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_400_user", "aclk_g2d_400");

	if (exynos_set_parent("mout_aclk_g2d_266_user", "aclk_g2d_266"))
		pr_err("Unable to set clock %s's parent %s\n"
				, "mout_aclk_g2d_266_user", "aclk_g2d_266");

	if (exynos_set_rate("dout_aclk_g2d_400", 413 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_400");

	if (exynos_set_rate("dout_aclk_g2d_266", 276 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_aclk_g2d_266");

	clk_rate1 = exynos_get_rate("aclk_g2d_400");
	clk_rate2 = exynos_get_rate("aclk_g2d_266");

	pr_info("[%s:%d] aclk_g2d_400:%d, aclk_g2d_266:%d\n"
			, __func__, __LINE__, clk_rate1, clk_rate2);
}

void jpeg_init_clock(void)
{
	if (exynos_set_parent("mout_sclk_jpeg_a", "mout_bus_pll_user"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_bus_pll_user", "mout_sclk_jpeg_a");

	if (exynos_set_parent("mout_sclk_jpeg_b", "mout_sclk_jpeg_a"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpog_a", "mout_sclk_jpeg_b");

	if (exynos_set_parent("mout_sclk_jpeg_c", "mout_sclk_jpeg_b"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_b", "mout_sclk_jpeg_c");

	if (exynos_set_parent("dout_sclk_jpeg", "mout_sclk_jpeg_c"))
		pr_err("Unable to set parent %s of clock %s\n",
				"mout_sclk_jpeg_c", "dout_sclk_jpeg");

	if (exynos_set_parent("sclk_jpeg_mscl", "dout_sclk_jpeg"))
		pr_err("Unable to set parent %s of clock %s\n",
				"dout_sclk_jpeg", "sclk_jpeg_mscl");

	if (exynos_set_parent("mout_sclk_jpeg_user", "sclk_jpeg_mscl"))
		pr_err("Unable to set parent %s of clock %s\n",
				"sclk_jpeg_mscl", "mout_sclk_jpeg_user");

	if (exynos_set_rate("dout_sclk_jpeg", 413 * 1000000))
		pr_err("Can't set %s clock rate\n", "dout_sclk_jpeg");

	pr_debug("jpeg: sclk_jpeg %d\n", exynos_get_rate("dout_sclk_jpeg"));
}

void cpif_init_clock(void)
{
	exynos_set_parent("mout_phyclk_lli_tx0_symbol_user",
			  "phyclk_lli_tx0_symbol");
	exynos_set_parent("mout_phyclk_lli_rx0_symbol_user",
			  "phyclk_lli_rx0_symbol");
	exynos_set_parent("mout_mphy_pll",
			  "fout_mphy_pll");
}

void crypto_init_clock(void)
{
	exynos_set_rate("dout_aclk_imem_266", 160*1000000);
	exynos_set_rate("dout_aclk_imem_200", 160*1000000);
}

void pwm_init_clock(void)
{
	clk_register_fixed_factor(NULL, "pwm-clock",
			"pclk_pwm",CLK_SET_RATE_PARENT, 1, 1);
}

void decon_tv_init_clock(void)
{
	exynos_set_parent("mout_sclk_decon_tv_eclk_a", "mout_bus_pll_div2");
	exynos_set_parent("mout_sclk_decon_tv_eclk_b", "mout_sclk_decon_tv_eclk_a");
	exynos_set_parent("mout_sclk_decon_tv_eclk_c", "mout_sclk_decon_tv_eclk_b");
	exynos_set_parent("dout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_c");

	exynos_set_rate("dout_sclk_decon_tv_eclk", 413 * 1000000);

	exynos_set_parent("sclk_decon_tv_eclk_disp", "dout_sclk_decon_tv_eclk");
	exynos_set_parent("mout_sclk_decon_tv_eclk_user", "sclk_decon_tv_eclk_disp");
	exynos_set_parent("mout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_user");
	exynos_set_parent("dout_sclk_decon_tv_eclk_disp", "mout_sclk_decon_tv_eclk");

	exynos_set_rate("dout_sclk_decon_tv_eclk_disp", 413 * 1000000);
}

void clocks_to_oscclk(void)
{
	exynos_set_parent("mout_aud_pll", "fin_pll");
	exynos_set_parent("mout_aud_pll_user_top", "fin_pll");
	exynos_set_parent("mout_mphy_pll_user", "fin_pll");
	exynos_set_parent("mout_disp_pll", "fin_pll");

	exynos_set_parent("mout_aclk_mscl_400_b", "mout_mphy_pll_user");
	exynos_set_parent("mout_aclk_g2d_400_b", "mout_mphy_pll_user");
	exynos_set_parent("mout_aclk_cam0_333_user", "oscclk");
	exynos_set_parent("mout_aclk_cam0_400_user", "oscclk");
	exynos_set_parent("mout_aclk_cam0_552_user", "oscclk");
	exynos_set_parent("mout_phyclk_rxbyteclkhs0_s4", "oscclk");
	exynos_set_parent("mout_phyclk_rxbyteclkhs0_s2a", "oscclk");
	exynos_set_parent("mout_sclk_isp_uart_user", "oscclk");
	exynos_set_parent("mout_sclk_isp_spi1_user", "oscclk");
	exynos_set_parent("mout_sclk_isp_spi0_user", "oscclk");
	exynos_set_parent("mout_aclk_cam1_333_user", "oscclk");
	exynos_set_parent("mout_aclk_cam1_400_user", "oscclk");
	exynos_set_parent("mout_aclk_cam1_552_user", "oscclk");
	exynos_set_parent("mout_phyclk_lli_rx0_symbol_user", "oscclk");
	exynos_set_parent("mout_phyclk_lli_tx0_symbol_user", "oscclk");
	exynos_set_parent("mout_phyclk_ufs_mphy_to_lli_user", "oscclk");
	exynos_set_parent("mout_sclk_dsim0_user", "oscclk");
	exynos_set_parent("mout_sclk_dsd_user", "oscclk");
	exynos_set_parent("mout_sclk_decon_tv_eclk_user", "oscclk");
	exynos_set_parent("mout_sclk_decon_vclk_user", "oscclk");
	exynos_set_parent("mout_sclk_decon_eclk_user", "oscclk");
	exynos_set_parent("mout_aclk_disp_333_user", "oscclk");
	exynos_set_parent("mout_phyclk_mipidphy_bitclkdiv8_user", "oscclk");
	exynos_set_parent("mout_phyclk_mipidphy_rxclkesc0_user", "oscclk");
	exynos_set_parent("mout_phyclk_hdmiphy_tmds_clko_user", "oscclk");
	exynos_set_parent("mout_phyclk_hdmiphy_pixel_clko_user", "oscclk");
	exynos_set_parent("mout_aclk_g2d_266_user", "oscclk");
	exynos_set_parent("mout_aclk_g2d_400_user", "oscclk");
	exynos_set_parent("mout_aclk_hevc_400_user", "oscclk");
	exynos_set_parent("mout_aclk_isp_dis_400_user", "oscclk");
	exynos_set_parent("mout_aclk_isp_400_user", "oscclk");
	exynos_set_parent("mout_aclk_mfc0_333_user", "oscclk");
	exynos_set_parent("mout_aclk_mfc1_333_user", "oscclk");
	exynos_set_parent("mout_sclk_jpeg_user", "oscclk");
	exynos_set_parent("mout_aclk_mscl_400_user", "oscclk");
}

void clocks_restore_from_oscclk(void)
{
	exynos_set_parent("mout_mphy_pll_user", "sclk_mphy_pll");
	exynos_set_parent("mout_disp_pll", "fout_disp_pll");

	exynos_set_parent("mout_aclk_mscl_400_b", "mout_aclk_mscl_400_a");
	exynos_set_parent("mout_aclk_g2d_400_b", "mout_aclk_g2d_400_a");
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
	pwm_init_clock();
	decon_tv_init_clock();

	clocks_to_oscclk();
}

static __init int exynos5430_clock_late_init(void)
{
	clocks_restore_from_oscclk();

	return 0;
}
late_initcall(exynos5430_clock_late_init);
