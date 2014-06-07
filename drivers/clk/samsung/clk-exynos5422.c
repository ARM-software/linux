/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Jiyun Kim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5422 SoC.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/regs-clock.h>

#include "clk.h"
#include "clk-pll.h"
#include "clk-exynos5422.h"

#undef MUX
#define MUX(_id, cname, pnames, o, s, w, f)                       \
	__MUX(_id, NULL, cname, pnames, o, s, w, f, 0, NULL)

#undef DIV
#define DIV(_id, cname, pname, o, s, w)                                \
	__DIV(_id, NULL, cname, pname, o, s, w, CLK_GET_RATE_NOCACHE, 0, NULL)



enum exynos5422_clks {
	none,

/* CLOCKS from CMU - 1.3.2 */
	fin_pll = 1,
	fout_apll,
	fout_bpll,
	fout_cpll,
	fout_dpll,
	fout_epll,
	fout_rpll,
	fout_ipll,
	fout_spll,
	fout_vpll,
	fout_mpll,

	sclk_hdmiphy = 28,
	sclk_hsic_12m,
	sclk_usbh20,
	mphy_refclk_ixtal24,
	sclk_usbh20_scan_clk,

/* CLK_GATE_BUS_CPU */
	atclk_atb_kfc_arm_master = 32,
	atclk_atb_isp_arm_master,
	aclk_axi_cpud, /* = aclk_cpud */

/* CLK_GATE_SCLK_CPU */
	sclk_hpm = 64,

/* CLK_GATE_IP_CPERI */
	clk_pmu = 160,
	clk_cmu_mempart,
	clk_cmu_top,
	clk_gic_cpu,
	clk_iem_iec,
	clk_iem_apc,
	clk_int_comb_cpu,

/* CLK_GATE_IP_G2D */
	clk_btsslimsss = 224,
	clk_smmuslimsss,
	clk_slimsss,
	clk_ppmuacpx,
	clk_btsg2d,
	clk_btssss,
	clk_btsmdma = 230,
	clk_smmug2d,
	clk_smmusss,
	clk_smmumdma,
	clk_g2d,
	clk_sss,
	clk_mdma,

/* CLK_GATE_IP_ISP0 */
	clk_uart_isp = 384,
	clk_wdt_isp,
	clk_i2c2_isp,
	clk_pwm_isp,
	clk_mtcadc_isp,
	clk_i2c1_isp,
	clk_i2c0_isp = 390,
	clk_mpwm_isp,
	clk_mcuctl_isp,
	clk_ppmuispx,
	clk_ppmuisp,
	clk_bts_mcuisp,
	clk_bts_scalerp,
	clk_bts_scalerc,
	clk_bts_fd,
	clk_bts_drc,
	clk_bts_isp = 400,
	clk_smmu_mcuisp,
	clk_smmu_scalerp,
	clk_smmu_scalerc,
	clk_smmu_fd,
	clk_smmu_drc,
	clk_smmu_isp,
	clk_gicisp,
	clk_mcuisp,
	clk_scalerp,
	clk_scalerc = 410,
	clk_fd,
	clk_drc,
	clk_isp,

/* CLK_GATE_IP_ISP1 */
	aclk_asyncaxim_3dnr_ip = 416,
	aclk_asyncaxim_dis0_ip,
	aclk_asyncaxim_dis1_ip,
	aclk_asyncaxim_fd_ip,
	aclk_asyncaxim_scp_ip = 420,
	clk_spi1_isp,
	clk_spi0_isp,
	clk_bts_3dnr,
	clk_bts_dis1,
	clk_bts_dis0,
	clk_smmu_3dnr,
	clk_smmu_dis1,
	clk_smmu_dis0,
	clk_3dnr,
	clk_dis = 430,
	clk_chain_glue1,

/* CLK_GATE_SCLK_ISP */
	sclk_mpwm_isp = 448,

/* CLK_GATE_BUS_TOP */
	mphy_refclk = 480,
	hsic_12m,
	mphy_ixtal24,
	aclk_200_disp1,
	aclk_400_mscl,
	aclk_400_isp,
	aclk_333,
	aclk_166,
	aclk_266_isp,
	aclk_66_peric,
	aclk_66_psgen = 490,
	pclk_66_gpio,
	aclk_333_432_isp,
	aclk_333_432_gscl,
	aclk_300_gscl,
	aclk_333_432_isp0,
	aclk_300_jpeg,
	aclk_266_g2d,
	aclk_333_g2d,

/* CLK_GATE_BUS_DISP1 */
	aclk_axi_disp1x = 601,

/* CLK_GATE_BUS_FSYS0 */
    aclk_noc_fsys = 748, /* = aclk_200_fsys */
    aclk_noc_fsys2 = 749, /* = aclk_200_fsys2 */
    aclk_pdma1 = 754,
    aclk_pdma0 = 755,

/* CLK_GATE_BUS_PERIS1 */
    pclk_st = 964, /* = mct */

/* CLK_GATE_TOP_SCLK_GSCL */
	sclk_gscl_wrap_b = 1024, /* = sclk_gscl_wb*/
	sclk_gscl_wrap_a, /* = sclk_gscl_wa*/

/* CLK_GATE_TOP_SCLK_DISP1 */
	sclk_sp1_strm = 1056,
	sclk_dp1_ext_mst_vid,
	sclk_pixel,
	sclk_hdmi,
	sclk_mipi1,
	sclk_fimd1,

/* CLK_GATE_TOP_SCLK_MAU */
	sclk_mau_pcm0 = 1088,
	sclk_mau_audio0,

/* CLK_GATE_TOP_SCLK_FSYS */
	sclk_usbdrd301 = 1120,
	sclk_usbdrd300,
	sclk_usbphy300,
	sclk_usbphy301,
	sclk_mmc2,
	sclk_mmc1,
	sclk_mmc0,

/* CLK_GATE_TOP_SCLK_PERIC */
	sclk_i2s2 = 1152,
	sclk_i2s1,
	sclk_pcm2,
	sclk_pcm1,
	sclk_pwm,
	sclk_spdif,
	sclk_spi2,
	sclk_spi1,
	sclk_spi0 = 1160,
	sclk_uart4,
	sclk_uart3,
	sclk_uart2,
	sclk_uart1,
	sclk_uart0,

/* CLK_GATE_TOP_SCLK_CPERI */
	sclk_pwi = 1184,

/* CLK_ TOP_SCLK_ISP */
	sclk_isp_sensor2 = 1216,
	sclk_isp_sensor1,
	sclk_isp_sensor0,
	sclk_pwm_isp,
	sclk_spi1_isp = 1220,
	sclk_spi0_isp,
	sclk_uart_isp,
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
/* CLK_GATE_IP_GSCL0 */
	clk_bts_fimcl1 = 1250,
	clk_bts_fimcl0,
	clk_bts_3aa,
	clk_camif_top_fimcl3 = 1255,
	clk_camif_top_fimcl1,
	clk_camif_top_fimcl0,
	clk_camif_top_3aa,
	clk_bts_fimcl3,
	gscl_fimc_lite1,
	gscl_fimc_lite0,
	clk_3aa,
	clk_gscl1,
	clk_gscl0,

/* CLK_GATE_IP_GSCL1 */
	clk_camif_top_csis0 = 1280,
	gscl_fimc_lite3,
	clk_smmu_fimcl3,
	clk_gscl_wrap_b,
	clk_gscl_wrap_a,
	clk_smmu_gscl1,
	clk_smmu_gscl0,
	clk_smmu_fimcl1,
	clk_smmu_fimcl0,
	clk_smmu_3aa,
#else
/* CLK_GATE_IP_GSCL0 */
	clk_bts_gscl1 = 1248,
	clk_bts_gscl0,
	clk_bts_fimcl1 = 1250,
	clk_bts_fimcl0,
	clk_bts_3aa,
	clk_ppmu_gscl1,
	clk_ppmu_gscl0,
	clk_ppmu_fimcl,
	clk_ppmu_3aa,
	clk_gscaler1,
	clk_gscaler0,
	clk_camif_top_fimcl3,
	clk_camif_top_fimcl1 = 1260,
	clk_camif_top_fimcl0,
	clk_camif_top_3aa,
	clk_bts_fimcl3,
	gscl_fimc_lite1,
	gscl_fimc_lite0,
	clk_3aa,
	clk_gscl1,
	clk_gscl0,

/* CLK_GATE_IP_GSCL1 */
	clk_camif_top_csis0 = 1280,
	gscl_fimc_lite3,
	clk_smmu_fimcl3,
	clk_gscl_wrap_b,
	clk_gscl_wrap_a,
	clk_smmu_gscl1,
	clk_smmu_gscl0,
	clk_smmu_fimcl1,
	clk_smmu_fimcl0,
	clk_smmu_3aa,
#endif
/* CLK_GATE_IP_DISP1 */
	clk_asynctvx = 1312,
	clk_asyncxim_gscl,
	clk_ppmutvx,
	clk_ppmufimd1x,
	clk_btstvm1,
	clk_btstvm0,
	clk_btsfimd1m1,
	clk_btsfimd1m0,
	clk_smmutvx = 1320,
	clk_smmufimd1x_m1, /* = smmu_fimd1 */
	clk_smmufimd1x_m0,
	clk_hdmi,
	clk_mixer,
	clk_dp1,
	clk_dsim1,
	clk_mdnie1,
	clk_mie1,
	clk_fimd1, /* = fimd */

/* CLK_GATE_IP_MFC */
	clk_ppmumfcr = 1344,
	clk_ppmumfcl,
	clk_btsmfcr,
	clk_btsmfcl,
	clk_smmumfcr,
	clk_smmumfcl,
	clk_mfc_ip,

/* CLK_GATE_IP_G3D */
	clk_g3d_ip = 1376, /* = aclk_g3d */
	clk_ahb2apb_g3dp,
	clk_hpm_g3d,
	clk_btsg3d,

/* CLK_GATE_IP_GEN */
	clk_ppmugenx = 1408,
	clk_btsmdma1,
	clk_btsjpeg = 1410,
	clk_btsrotator,
	clk_smmumdma1,
	clk_smmujpeg,
	clk_smmurotator,
	clk_top_trc,
	clk_mdma1,
	clk_jpeg2,
	clk_jpeg,
	clk_rotator,

/* CLK_GATE_IP_FSYS */
	clk_ahb2apb_fsys2 = 1440,
	clk_ppmu_ufs,
	clk_bts_usbdrd301,
	clk_bts_usbdrd300,
	clk_ufs,
	clk_bts_ufs,
	clk_ahb2apb_fsyssp,
	clk_usbdrd301,
	clk_usbdrd300,
	clk_usbhost20,
	clk_sromc = 1450,
	clk_sdmmc2,
	clk_sdmmc1,
	clk_sdmmc0,
	clk_smmurtic,
	clk_ahb2apb_fsys1p,
	clk_rtic,
	clk_pdma,

/* CLK_GATE_IP_PERIC */
	clk_i2c10 = 1472,
	clk_i2c9,
	clk_keyif,
	clk_i2c8,
	clk_spdif,
	clk_pwm,
	clk_pcm2,
	clk_pcm1,
	clk_i2s2 = 1480,
	clk_i2s1,
	clk_spi2,
	clk_spi1,
	clk_spi0,
	clk_tsadc,
	clk_i2chdmi,
	clk_i2c7,
	clk_i2c6,
	clk_i2c5,
	clk_i2c4 = 1490,
	clk_i2c3,
	clk_i2c2,
	clk_i2c1,
	clk_i2c0,
	clk_uart4,
	clk_uart3,
	clk_uart2,
	clk_uart1,
	clk_uart0,

/* CLK_GATE_IP_PERIS */
	clk_abb_apbif = 1504,
	clk_tmu_gpu_apbif,
	clk_tmu_apbif,
	clk_rtc,
	clk_wdt,
	clk_st,
	clk_seckey_apbif = 1510,
	clk_hdmi_sec,
	clk_tzpc9,
	clk_tzpc8,
	clk_tzpc7,
	clk_tzpc6,
	clk_tzpc5,
	clk_tzpc4,
	clk_tzpc3,
	clk_tzpc2,
	clk_tzpc1 = 1520,
	clk_tzpc0,
	clk_sysreg,
	clk_chipid_apbif,

/* CLK_GATE_IP_MSCL */
	clk_alb = 1536,
	clk_asyncaxi2,
	clk_asyncaxi1,
	clk_asyncaxi0,
	clk_smmu2 = 1540,
	clk_smmu1,
	clk_smmu0,
	clk_bts2,
	clk_bts1,
	clk_bts0,
	clk_mscl_ppmu2,
	clk_mscl_ppmu01,
	clk_mscl2,
	clk_mscl1,
	clk_mscl0 = 1550,

/* CLK_GATE_BLOCK */
	clk_disp1 = 1568,
	clk_gscl,
	clk_gen = 1570,
	clk_g3d_block,
	clk_mfc_block,


/* CLK_GATE_BUS_CDREX */
	pclk_core_mem = 1600,
	aclk_cbx_ncci,
	pclk_rs_top,
	pclk_noc_core,
	pclk_phy1,
	pclk_phy0,
	pclk_drex1,
	pclk_drex0,
	pclk_core_misc,
	pclk_bts0eagle,
	rclk_drex1 = 1610,
	rclk_drex0,
	aclk_sfrcdrexp,
	cclk_drex1,
	cclk_drex0,
	clk2x_phy1,
	clk2x_phy0,
	clkm_phy1,
	clkm_phy0,

/* CLK_GATE_IP_CDREX */
	clk_bts2g2d = 1664,
	clk_bts1kfc,
	clk_bts0egle,
	clk_phy1,
	clk_phy0,
	clk_drex1,
	clk_drex0 = 1670,
	clk_sfrtzascp,
	clk_sfrcdrexp,

/* CLK_GATE_ASS */
	gate_ass_niu_p = 1720,
	gate_ass_niu,
	gate_ass_adma,
	gate_ass_timer,
	gate_ass_uart,
	gate_ass_gpio,
	gate_ass_pcm_special,
	gate_ass_pcm_bus,
	gate_ass_i2s_special,
	gate_ass_i2s_bus,
	gate_ass_srp,

/* MUX */
	/* CMU_TOP - bus */
	mout_bpll_ctrl_user = 2000, /*mout_bpll, sclk_bpll */
	mout_cpll_ctrl,/* sclk_cpll */
	mout_dpll_ctrl,/* sclk_dpll */
	mout_epll_ctrl,/* sclk_epll */
	mout_rpll_ctrl,/* sclk_rpll */
	mout_ipll_ctrl,/* sclk_ipll */
	mout_spll_ctrl,/* sclk_spll */
	mout_vpll_ctrl,/* sclk_vpll */
	mout_mpll_ctrl,/* sclk_mpll */

	mout_aclk_200_fsys = 2009,
	mout_pclk_200_fsys,
	mout_aclk_100_noc,
	mout_aclk_400_wcore,
	mout_aclk_400_wcore_bpll,

	mout_aclk_200_fsys_sw,
	mout_pclk_200_fsys_sw,
	mout_aclk_100_noc_sw,
	mout_aclk_400_wcore_sw,

	mout_aclk_200_fsys_user,
	mout_pclk_200_fsys_user,
	mout_aclk_100_noc_user,
	mout_aclk_400_wcore_user,

	mout_aclk_200_fsys2 = 2022,
	mout_aclk_200,
	mout_aclk_400_mscl,
	mout_aclk_400_isp,
	mout_aclk_333,
	mout_aclk_166,
	mout_aclk_266,

	mout_aclk_200_fsys2_sw = 2030,
	mout_aclk_200_sw,
	mout_aclk_400_mscl_sw,
	mout_aclk_400_isp_sw,
	mout_aclk_333_sw,
	mout_aclk_166_sw,
	mout_aclk_266_sw,

	mout_aclk_200_fsys2_user,
	mout_aclk_200_disp1_user,
	mout_aclk_400_mscl_user,
	mout_aclk_400_isp_user,
	mout_aclk_333_user = 2041,
	mout_aclk_166_user,
	mout_aclk_266_user,
	mout_aclk_266_isp_user,

	mout_aclk_66,
	mout_aclk_66_sw,
	mout_aclk_66_peric_user,
	mout_aclk_66_psgen_user,
	mout_aclk_66_gpio_user,

	mout_aclk_333_432_isp0 = 2050,
	mout_aclk_333_432_isp,
	mout_aclk_333_432_gscl,
	mout_aclk_300_gscl,
	mout_aclk_300_disp1,
	mout_aclk_300_jpeg,
	mout_aclk_g3d,
	mout_aclk_266_g2d,
	mout_aclk_333_g2d,
	mout_aclk_400_disp1,
	mout_mau_epll_clk = 2060,
	mout_mx_mspll_ccore,
	mout_mx_mspll_cpu,
	mout_mx_mspll_kfc,

	mout_aclk_333_432_isp0_sw,
	mout_aclk_333_432_isp_sw = 2065,
	mout_aclk_333_432_gscl_sw,
	mout_aclk_300_gscl_sw,
	mout_aclk_300_disp1_sw,
	mout_aclk_300_jpeg_sw,
	mout_aclk_g3d_sw = 2070,
	mout_aclk_266_g2d_sw,
	mout_aclk_333_g2d_sw,
	mout_aclk_400_disp1_sw,

	mout_aclk_333_432_isp0_user,
	mout_aclk_333_432_isp_user = 2075,
	mout_aclk_333_432_gscl_user,
	mout_aclk_300_gscl_user,
	mout_aclk_300_disp1_user,
	mout_aclk_300_jpeg_user,
	mout_aclk_g3d_user = 2080,
	mout_aclk_266_g2d_user,
	mout_aclk_333_g2d_user,
	mout_aclk_400_disp1_user,

	/* CMU_TOP - function */
		/* DISP1 */
	mout_pixel = 2100,
	mout_dp1_ext_mst_vid,
	mout_mipi1,
	mout_fimd1_opt,
	mout_fimd1,
	mout_mdnie_pwm1,
	mout_mdnie1,
	mout_fimd1_mdnie1,
	mout_hdmi,

		/* MAU */
	mout_mau_audio0 = 2109,

		/* FSYS */
		/* FSYS2 */
	mout_usbdrd300,	mout_usbdrd301,
	mout_mmc0,	mout_mmc1,	mout_mmc2,
	mout_unipro = 2115,

		/* ISP */
	mout_isp_sensor,
	mout_pwm_isp,
	mout_uart_isp,
	mout_spi0_isp,
	mout_spi1_isp = 2120,

		/* ETC */
	mout_mphy_refclk,

		/* GSCL */

		/* PERIC */
	mout_pwm,
	mout_uart0,	mout_uart1,	mout_uart2,	mout_uart3,	mout_uart4,
	mout_spi0, mout_spi1, mout_spi2,
	mout_audio2, mout_audio1, mout_audio0,
	mout_spdif,

	/* CMU_CPU */
	mout_apll_ctrl, /* = mout_apll */
	mout_cpu,
	mout_hpm,

	/* CMU_KFC */
	mout_kpll_ctrl, /* = mout_kpll */
	mout_cpu_kfc,
	mout_hpm_kfc,

	/* CMU_CDREX */
	/* CMU_CPERI */
	mout_bpll_ctrl, /*TODO: what is differece with mout_bpll_ctrl_user */
	mout_mclk_cdrex,

	/* CMU_ISP */

	/* MSCL */

	/* MFC */

	/* G2D */

	/* DISP1 */

	/* GSCL */

	/* PSGEN */

	/* ASS */
	mout_ass_clk = 2200,
	mout_ass_i2s,

	/* etc */

	mout_pclk_66_gpio_user,
/* DEVIDER */
	/* CMU_TOP - bus */
	dout_aclk_200_fsys = 4000,
	dout_pclk_200_fsys,
	dout_aclk_100_noc,
	dout_aclk_400_wcore,
	dout_aclk_200_fsys2,
	dout_aclk_200,
	dout_aclk_400_mscl,
	dout_aclk_400_isp,
	dout_aclk_333,
	dout_aclk_166,
	dout_aclk_266 = 4010,
	dout_aclk_66,
	dout_aclk_333_432_isp0,
	dout_aclk_333_432_isp,
	dout_aclk_333_432_gscl,
	dout_aclk_300_gscl,
	dout_aclk_300_disp1,
	dout_aclk_300_jpeg,
	dout_aclk_g3d,
	dout_aclk_266_g2d,
	dout_aclk_333_g2d,
	dout_aclk_400_disp1,

	/* CMU_TOP - function */
		/* DISP1 */
	dout_hdmi_pixel = 4100,
	dout_dp1_ext_mst_vid,
	dout_mipi1,
	dout_fimd1,

		/* MAU */
	dout_mau_audio0,
	dout_mau_pcm0,

		/* FSYS */
		/* FSYS2 */
	dout_usbdrd300,	dout_usbdrd301,
	dout_usbphy300,	dout_usbphy301,
	dout_mmc0 = 4110, dout_mmc1, dout_mmc2,
	dout_unipro,

		/* ISP */
	dout_isp_sensor0, dout_isp_sensor1, dout_isp_sensor2,
	dout_pwm_isp,
	dout_uart_isp,
	dout_spi0_isp, dout_spi1_isp = 4120, dout_spi0_isp_pre, dout_spi1_isp_pre,

		/* ETC */
	dout_mphy_refclk,
	dout_aclk_66_sw_div2 = 4124,

		/* GSCL */

		/* PERIC */
	dout_pwm,
	dout_uart0,	dout_uart1,	dout_uart2,	dout_uart3,	dout_uart4,
	dout_spi0, dout_spi1, dout_spi2,
	dout_spi0_pre, dout_spi1_pre, dout_spi2_pre,
	dout_audio2, dout_pcm2, dout_i2s2,
	dout_audio1, dout_pcm1, dout_i2s1,
	dout_audio0,

	/* CMU_CPU */
	dout_arm2 = 4144,
	dout_arm2_lsb,
	dout_arm2_msb,
	dout_cpud,
	dout_atb,
	dout_pclk_dbg,
	dout_apll,
	dout_copy,
	dout_hpm,

	/* CMU_KFC */
	dout_kfc,
	dout_aclk,
	dout_pclk,
	dout_kpll,
	dout_hpm_kfc,

	/* CMU_CDREX */
	/* CMU_CPERI */
	dout_sclk_cdrex,
	dout_clk2x_phy0,
	dout_cclk_drex0 = 4160,
	dout_pclk_drex0,
	dout_cclk_drex1,
	dout_pclk_drex1,
	dout_aclk_cdrex1,
	dout_pclk_cdrex,
	dout_pclk_core_mem,

	/* CMU_ISP */
	dout_ispdiv0_0,
	dout_mcuispdiv0,
	dout_mcuispdiv1,
	dout_ispdiv0 = 4170,
	dout_ispdiv1,
	dout_ispdiv2,

	/* MSCL */
	dout_mscl_blk,

	/* MFC */
	dout_mfc_blk,

	/* G2D */
	dout_acp_pclk,

	/* DISP1 */
	dout_disp1_blk,

	/* GSCL */
	dout_gscl_blk300,
	dout_gscl_blk333,


	/* PSGEN */
	dout_gen_blk,
	dout_jpg_blk,

	/* ASS */
	dout_ass_srp,
	dout_ass_bus,
	dout_ass_i2s,

	/* etc */
	dout2_disp1_blk,

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	/* GATE */
	/* TOP */
	sclk_epll2	= 5000,
	/* sclk_spll2, */ /* = dout_spll_ctrl_div2 */
	aclk_432_scaler = 5002,
	aclk_432_cam,
	aclk_fl1_550_cam,
	aclk_550_cam,
	mau_epll_clk,
	mx_mspll_ccore_phy,

	/* CLK_GATE_IP_CAM */
	clk_3aa_2,
	clk_xiu_mi_gscl_cam,
	clk_xiu_si_gscl_cam,
	clk_noc_p_rstop_fimcl,
	clk_camif_top_3aa0,
	clk_bts_3aa0,
	clk_smmu_3aa0,

	/* MUX */
	/* TOP */
	mout_epll2 = 5300,
	mout_aclk_266_isp,
	mout_aclk_266_isp_sw,
	/* mout_aclk_266_isp_user, */
	mout_aclk_432_scaler,
	mout_aclk_432_scaler_sw = 5304,
	mout_aclk_432_scaler_user,
	mout_aclk_432_cam,
	mout_aclk_432_cam_sw,
	mout_aclk_432_cam_user,
	mout_aclk_fl1_550_cam = 5310,
	mout_aclk_fl1_550_cam_sw,
	mout_aclk_fl1_550_cam_user,
	mout_aclk_550_cam,
	mout_aclk_550_cam_sw,
	mout_aclk_550_cam_user = 5315,
	mout_mau_epll_clk_user,
	mout_mx_mspll_ccore_phy,
	/* GSCL */
	mout_gscl_wrap_a = 5320,
	mout_gscl_wrap_b,

	/* DIV */
	/* TOP */
	dout_epll_ctrl_div2 = 5400,
	dout_spll_ctrl,/* sclk_sw */
	dout_spll_ctrl_div2,
	dout_osc, /* osc_div */
	dout_aclk_266_isp,
	dout_aclk_432_scaler = 5405,
	dout_aclk_432_cam,
	dout_aclk_fl1_550_cam,
	dout_aclk_550_cam,

	/* GSCL */
	dout_gscl_wrap_a = 5410,
	dout_gscl_wrap_b,
	dout2_gscl_blk_300,
	dout2_gscl_blk_333,
	dout2_cam_blk_432,
	dout2_cam_blk_550,
	aclk_550_cam_div2,
	sclk_gscl_wrap_a_div2,

#endif
	clk_dummy1,
	nr_clks,
};

static __initdata void *exynos5422_clk_regs[] = {
	EXYNOS5422_CMU_CPU_SPARE1,
	EXYNOS5_CLK_SRC_CPU,
	EXYNOS5_CLK_DIV_CPU0,
	EXYNOS5_CLK_DIV_CPU1,
	EXYNOS5_CLK_GATE_BUS_CPU,
	EXYNOS5_CLK_GATE_SCLK_CPU,
	EXYNOS5_CLK_SRC_TOP0,
	EXYNOS5_CLK_SRC_TOP1,
	EXYNOS5_CLK_SRC_TOP2,
	EXYNOS5_CLK_SRC_TOP3,
	EXYNOS5_CLK_SRC_TOP4,
	EXYNOS5_CLK_SRC_TOP5,
	EXYNOS5_CLK_SRC_TOP6,
	EXYNOS5_CLK_SRC_TOP7,
	EXYNOS5_CLK_SRC_DISP10,
	EXYNOS5_CLK_SRC_MAUDIO,
	EXYNOS5_CLK_SRC_FSYS,
	EXYNOS5_CLK_SRC_PERIC0,
	EXYNOS5_CLK_SRC_PERIC1,
	EXYNOS5_CLK_SRC_TOP10,
	EXYNOS5_CLK_SRC_TOP11,
	EXYNOS5_CLK_SRC_TOP12,
	EXYNOS5_CLK_SRC_MASK_DISP10,
	EXYNOS5_CLK_SRC_MASK_FSYS,
	EXYNOS5_CLK_SRC_MASK_PERIC0,
	EXYNOS5_CLK_SRC_MASK_PERIC1,
	EXYNOS5_CLK_DIV_TOP0,
	EXYNOS5_CLK_DIV_TOP1,
	EXYNOS5_CLK_DIV_TOP2,
	EXYNOS5_CLK_DIV_DISP10,
	EXYNOS5_CLK_DIV_MAUDIO,
	EXYNOS5_CLK_DIV_FSYS0,
	EXYNOS5_CLK_DIV_FSYS1,
	EXYNOS5_CLK_DIV_FSYS2,
	EXYNOS5_CLK_DIV_PERIC0,
	EXYNOS5_CLK_DIV_PERIC1,
	EXYNOS5_CLK_DIV_PERIC2,
	EXYNOS5_CLK_DIV_PERIC3,
	EXYNOS5_CLK_DIV_PERIC4,
	EXYNOS5_CLK_DIV_G2D,
	EXYNOS5_CLK_GATE_IP_G2D,
	EXYNOS5_CLK_GATE_BUS_TOP,
	EXYNOS5_CLK_GATE_BUS_FSYS0,
	EXYNOS5_CLK_GATE_BUS_PERIC,
	EXYNOS5_CLK_GATE_IP_PERIC,
	EXYNOS5_CLK_GATE_BUS_PERIC1,
	EXYNOS5_CLK_GATE_BUS_PERIS0,
	EXYNOS5_CLK_GATE_BUS_PERIS1,
	EXYNOS5_CLK_GATE_IP_GSCL0,
	EXYNOS5_CLK_GATE_IP_GSCL1,
	EXYNOS5_CLK_GATE_IP_MFC,
	EXYNOS5_CLK_GATE_IP_G3D,
	EXYNOS5_CLK_GATE_IP_GEN,
	EXYNOS5_CLK_GATE_IP_MSCL,
	EXYNOS5_CLK_GATE_IP_DISP1,
	EXYNOS5_CLK_GATE_TOP_SCLK_GSCL,
	EXYNOS5_CLK_GATE_TOP_SCLK_DISP1,
	EXYNOS5_CLK_GATE_TOP_SCLK_MAU,
	EXYNOS5_CLK_GATE_TOP_SCLK_FSYS,
	EXYNOS5_CLK_GATE_TOP_SCLK_PERIC,
	EXYNOS5_CLK_SRC_CDREX,
	EXYNOS5_CLK_GATE_BUS_CDREX,
	EXYNOS5_CLK_GATE_BUS_CDREX1,
	EXYNOS5_CLK_GATE_IP_CDREX,
	EXYNOS5_CLK_GATE_IP_FSYS,
	EXYNOS5_CLK_SRC_KFC,
	EXYNOS5_CLK_DIV_KFC0,
};

/* list of all parent clocks */
/* CMU_TOP */
PNAME(mout_apll_ctrl_p)        = { "fin_pll", "fout_apll", };
PNAME(mout_bpll_ctrl_user_p) 	= { "fin_pll", "fout_bpll", };
PNAME(mout_cpll_ctrl_p)		= { "fin_pll", "fout_cpll", };
PNAME(mout_dpll_ctrl_p)		= { "fin_pll", "fout_dpll", };
PNAME(mout_epll_ctrl_p)		= { "fin_pll", "fout_epll", };
PNAME(mout_rpll_ctrl_p)		= { "fin_pll", "fout_rpll", };
PNAME(mout_ipll_ctrl_p)		= { "fin_pll", "fout_ipll", };
PNAME(mout_spll_ctrl_p)		= { "fin_pll", "fout_spll", };
PNAME(mout_vpll_ctrl_p)		= { "fin_pll", "fout_vpll", };
PNAME(mout_mpll_ctrl_p)		= { "fin_pll", "fout_mpll", };
PNAME(mout_kpll_ctrl_p)        = { "fin_pll", "fout_kpll", };

/* CMU_CPU */
PNAME(mout_mx_mspll_cpu_p)     = { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_spll_ctrl" };
PNAME(mout_cpu_p)              = { "mout_apll_ctrl" , "mout_mx_mspll_cpu" };
PNAME(mout_hpm_p)              = { "mout_apll_ctrl" , "mout_mx_mspll_cpu" };
/* CMU_KFC */
PNAME(mout_mx_mspll_kfc_p)     = { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_spll_ctrl" };
PNAME(mout_cpu_kfc_p)          = { "mout_kpll_ctrl" , "mout_mx_mspll_kfc" };
PNAME(mout_hpm_kfc_p)          = { "mout_kpll_ctrl" , "mout_mx_mspll_kfc" };


/*  group1_p child mux list

	mout_aclk_200_fsys,
	mout_pclk_200_fsys,
	mout_aclk_100_noc,
	mout_aclk_400_wcore,
	mout_aclk_200_fsys2,
	mout_aclk_200,
	mout_aclk_400_mscl,
	mout_aclk_400_isp,
	mout_aclk_333,
	mout_aclk_166,
	mout_aclk_266,
	mout_aclk_66,
*/
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(group1_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl"};
PNAME(group1_1_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "dout_spll_ctrl_div2", "sclk_epll2"};
PNAME(group1_2_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "dout_spll_ctrl_div2"};
PNAME(group1_3_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "dout_spll_ctrl_div2", "sclk_epll2", "mout_ipll_ctrl"};
PNAME(group1_4_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl"};
PNAME(group1_5_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_spll_ctrl"};
PNAME(group1_6_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_ipll_ctrl" };
#else
PNAME(group1_p)		= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl"};
#endif
PNAME(group2_p)		= { "fin_pll", "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_spll_ctrl", "mout_ipll_ctrl", "mout_epll_ctrl", "mout_rpll_ctrl" };
#ifndef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(group3_p)		= { "mout_rpll_ctrl", "mout_spll_ctrl" };
PNAME(group4_p)		= { "mout_ipll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl" };
#endif
PNAME(group5_p)		= { "mout_vpll_ctrl", "mout_dpll_ctrl" };
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(group4_1_p)		= { "mout_ipll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "dout_spll_ctrl_div2" };
#endif

/* CMU-TOP */
PNAME(mout_aclk_200_fsys_sw_p) = { "dout_aclk_200_fsys", "mout_spll_ctrl"};
PNAME(mout_aclk_200_fsys_user_p)	= { "fin_pll", "mout_aclk_200_fsys_sw" };
PNAME(mout_pclk_200_fsys_sw_p) = { "dout_pclk_200_fsys", "mout_spll_ctrl"};
PNAME(mout_pclk_200_fsys_user_p)	= { "fin_pll", "mout_pclk_200_fsys_sw" };
PNAME(mout_aclk_100_noc_sw_p)	= { "dout_aclk_100_noc", "mout_spll_ctrl" };
PNAME(mout_aclk_100_noc_user_p)	= { "fin_pll", "mout_aclk_100_noc_sw" };
#ifndef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_400_wcore_bpll_p) = {"mout_aclk_400_wcore", "mout_bpll_ctrl_user"};
#endif
PNAME(mout_aclk_400_wcore_sw_p) = {"dout_aclk_400_wcore", "dout_spll_ctrl"};
PNAME(mout_aclk_400_wcore_user_p) = {"fin_pll", "mout_aclk_400_wcore_sw"};
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_200_fsys2_sw_p) = { "dout_aclk_200_fsys2", "dout_spll_ctrl"};
#else
PNAME(mout_aclk_200_fsys2_sw_p) = { "dout_aclk_200_fsys2", "mout_spll_ctrl"};
#endif
PNAME(mout_aclk_200_fsys2_user_p)	= { "fin_pll", "mout_aclk_200_fsys2_sw" };
PNAME(mout_aclk_200_sw_p) = { "dout_aclk_200", "mout_spll_ctrl"};
PNAME(mout_aclk_200_disp1_user_p)	= { "fin_pll", "mout_aclk_200_sw" };

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_400_isp_sw_p) = { "dout_aclk_400_isp", "dout_spll_ctrl"};
PNAME(mout_aclk_400_isp_user_p)	= { "dout_osc", "mout_aclk_400_isp_sw" };
#else
PNAME(mout_aclk_400_isp_sw_p) = { "dout_aclk_400_isp", "mout_spll_ctrl"};
PNAME(mout_aclk_400_isp_user_p)	= { "fin_pll", "mout_aclk_400_isp_sw" };
#endif

PNAME(mout_aclk_66_sw_p)	= { "dout_aclk_66", "mout_spll_ctrl" };
PNAME(mout_aclk_66_peric_user_p)	= { "fin_pll", "mout_aclk_66_sw" };
PNAME(mout_aclk_66_psgen_user_p)	= { "fin_pll", "mout_aclk_66_sw" };
PNAME(mout_pclk_66_gpio_user_p)	= { "mout_aclk_66_sw", "dout_aclk_66_sw_div2" };

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_400_mscl_sw_p) = { "dout_aclk_400_mscl", "dout_spll_ctrl"};
PNAME(mout_aclk_400_mscl_user_p)	= { "dout_osc", "mout_aclk_400_mscl_sw" };
#else
PNAME(mout_aclk_400_mscl_sw_p) = { "dout_aclk_400_mscl", "mout_spll_ctrl"};
PNAME(mout_aclk_400_mscl_user_p)	= { "fin_pll", "mout_aclk_400_mscl_sw" };
#endif
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_333_sw_p) = { "dout_aclk_333", "dout_spll_ctrl"};
PNAME(mout_aclk_333_user_p)	= { "dout_osc", "mout_aclk_333_sw" };
#else
PNAME(mout_aclk_333_sw_p) = { "dout_aclk_333", "mout_spll_ctrl"};
PNAME(mout_aclk_333_user_p)	= { "fin_pll", "mout_aclk_333_sw" };
#endif
PNAME(mout_aclk_166_sw_p) = { "dout_aclk_166", "mout_spll_ctrl"};
PNAME(mout_aclk_166_user_p)	= { "fin_pll", "mout_aclk_166_sw" };
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_266_sw_p) = { "dout_aclk_266", "dout_spll_ctrl"};
PNAME(mout_aclk_266_user_p)	= { "dout_osc", "mout_aclk_266_sw" };
#else
PNAME(mout_aclk_266_sw_p) = { "dout_aclk_266", "mout_spll_ctrl"};
PNAME(mout_aclk_266_user_p)	= { "fin_pll", "mout_aclk_266_sw" };
#endif
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_333_432_isp0_sw_p) = { "dout_aclk_333_432_isp0", "dout_spll_ctrl"};
PNAME(mout_aclk_333_432_isp0_user_p)	= { "dout_osc", "mout_aclk_333_432_isp0_sw" };
#else
PNAME(mout_aclk_333_432_isp0_sw_p) = { "dout_aclk_333_432_isp0", "mout_spll_ctrl"};
PNAME(mout_aclk_333_432_isp0_user_p)	= { "fin_pll", "mout_aclk_333_432_isp0_sw" };
#endif
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_aclk_333_432_isp_sw_p) = { "dout_aclk_333_432_isp", "dout_spll_ctrl"};
PNAME(mout_aclk_333_432_isp_user_p)	= { "dout_osc", "mout_aclk_333_432_isp_sw" };
#else
PNAME(mout_aclk_333_432_isp_sw_p) = { "dout_aclk_333_432_isp", "mout_spll_ctrl"};
PNAME(mout_aclk_333_432_isp_user_p)	= { "fin_pll", "mout_aclk_333_432_isp_sw" };
#endif
PNAME(mout_aclk_333_432_gscl_sw_p) = { "dout_aclk_333_432_gscl", "mout_spll_ctrl"};
PNAME(mout_aclk_333_432_gscl_user_p)	= { "fin_pll", "mout_aclk_333_432_gscl_sw" };

PNAME(mout_aclk_300_gscl_sw_p) = { "dout_aclk_300_gscl", "mout_spll_ctrl"};
PNAME(mout_aclk_300_gscl_user_p)	= { "fin_pll", "mout_aclk_300_gscl_sw" };


PNAME(mout_aclk_300_disp1_sw_p) = { "dout_aclk_300_disp1", "mout_spll_ctrl"};
PNAME(mout_aclk_300_disp1_user_p)	= { "fin_pll", "mout_aclk_300_disp1_sw" };

PNAME(mout_aclk_300_jpeg_sw_p) = { "dout_aclk_300_jpeg", "mout_spll_ctrl"};
PNAME(mout_aclk_300_jpeg_user_p)	= { "fin_pll", "mout_aclk_300_jpeg_sw" };

PNAME(mout_aclk_g3d_sw_p) = { "dout_aclk_g3d", "mout_spll_ctrl"};
PNAME(mout_aclk_g3d_user_p)	= { "fin_pll", "mout_aclk_g3d_sw" };

PNAME(mout_aclk_266_g2d_sw_p) = { "dout_aclk_266_g2d", "mout_spll_ctrl"};
PNAME(mout_aclk_266_g2d_user_p)	= { "fin_pll", "mout_aclk_266_g2d_sw" };

PNAME(mout_aclk_333_g2d_sw_p) = { "dout_aclk_333_g2d", "mout_spll_ctrl"};
PNAME(mout_aclk_333_g2d_user_p)	= { "fin_pll", "mout_aclk_333_g2d_sw" };

PNAME(mout_hdmi_p)	= { "sclk_hdmiphy", "dout_hdmi_pixel" };
#ifndef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_fimd1_mdnie1_p) = { "mout_fimd1", "mout_mdnie1"};
#endif

PNAME(mout_audio0_p)	= { "fin_pll", "cdclk0", "mout_dpll_ctrl", "mout_mpll_ctrl",
		  "mout_spll_ctrl", "mout_ipll_ctrl", "mout_epll_ctrl", "mout_rpll_ctrl" };
PNAME(mout_audio1_p)	= { "fin_pll", "cdclk1", "mout_dpll", "mout_mpll_ctrl",
		  "mout_spll_ctrl", "mout_ipll_ctrl", "mout_epll_ctrl", "mout_rpll_ctrl" };
PNAME(mout_audio2_p)	= { "fin_pll", "cdclk2", "mout_dpll_ctrl", "mout_mpll",
		  "mout_spll_ctrl", "mout_ipll_ctrl", "mout_epll_ctrl", "mout_rpll" };
PNAME(mout_spdif_p)	= { "fin_pll", "dout_audio0", "dout_audio1", "dout_audio2",
		  "spdif_extclk", "mout_ipll_ctrl", "mout_epll_ctrl", "mout_rpll_ctrl" };
PNAME(mout_mau_audio0_p)	= { "fin_pll", "mau_audiocdclk", "mout_dpll_ctrl", "mout_mpll_ctrl",
			  "mout_spll_ctrl", "mout_ipll_ctrl", "mout_epll_ctrl", "mout_rpll_ctrl" };
PNAME(mout_mclk_cdrex_p)	= { "mout_bpll_ctrl_user", "mout_mx_mspll_ccore" };
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_mx_mspll_ccore_p)	= { "mout_bpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "dout_spll_ctrl_div2",
 "mout_spll_ctrl", "mout_epll_ctrl"};
#else
PNAME(mout_mx_mspll_ccore_p)	= { "mout_cpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_spll_ctrl"};
#endif

PNAME(mout_mau_epll_clk_p) = { "mout_epll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl", "mout_spll_ctrl"};

PNAME(mout_aclk_400_disp1_sw_p) = { "dout_aclk_400_disp1", "mout_spll_ctrl"};
PNAME(mout_aclk_400_disp1_user_p)	= { "fin_pll", "mout_aclk_400_disp1_sw" };

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_ass_clk_p)	= { "fin_pll", "mout_mau_epll_clk_user" };
PNAME(mout_ass_i2s_p)	= { "mout_ass_clk", "cdclk0", "sclk_mau_audio0" };
#else
PNAME(mout_ass_clk_p)	= { "fin_pll", "fout_epll" };
PNAME(mout_ass_i2s_p)	= { "mout_ass_clk", "cdclk0", "sclk_mau_audio0" };
#endif

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
PNAME(mout_epll2_p)	= { "mout_epll_ctrl", "dout_epll_ctrl_div2" };

PNAME(mout_aclk_266_isp_sw_p)	= { "dout_aclk_266_isp", "dout_spll_ctrl" };
PNAME(mout_aclk_266_isp_user_p)	= { "dout_osc", "mout_aclk_266_isp_sw" };

PNAME(mout_aclk_432_scaler_sw_p)	= { "dout_aclk_432_scaler", "dout_spll_ctrl" };
PNAME(mout_aclk_432_scaler_user_p)	= { "dout_osc", "mout_aclk_432_scaler_sw" };

PNAME(mout_aclk_432_cam_sw_p)	= { "dout_aclk_432_cam", "dout_spll_ctrl" };
PNAME(mout_aclk_432_cam_user_p)	= { "dout_osc", "mout_aclk_432_cam_sw" };

PNAME(mout_aclk_fl1_550_cam_sw_p)	= { "dout_aclk_fl1_550_cam", "dout_spll_ctrl" };
PNAME(mout_aclk_fl1_550_cam_user_p)	= { "dout_osc", "mout_aclk_fl1_550_cam_sw" };

PNAME(mout_aclk_550_cam_sw_p)	= { "dout_aclk_550_cam", "dout_spll_ctrl" };
PNAME(mout_aclk_550_cam_user_p)	= { "dout_osc", "mout_aclk_550_cam_sw" };

PNAME(mout_mau_epll_clk_user_p)	= { "dout_osc", "mout_mau_epll_clk" };
PNAME(mout_mx_mspll_ccore_phy_p)	= { "mout_bpll_ctrl", "mout_dpll_ctrl", "mout_mpll_ctrl" };
#endif


#define CFRATE(_id, pname, f, rate) \
		FRATE(_id, #_id, pname, f, rate)
/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos5422_fixed_rate_ext_clks[] __initdata = {
	FRATE(fin_pll, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

/* fixed rate clocks generated inside the soc */
struct samsung_fixed_rate_clock exynos5422_fixed_rate_clks[] __initdata = {
	FRATE(sclk_hdmiphy, "sclk_hdmiphy", NULL, CLK_IS_ROOT, 24000000),
	FRATE(sclk_pwi, "sclk_pwi", NULL, CLK_IS_ROOT, 24000000),
	FRATE(sclk_usbh20, "sclk_usbh20", NULL, CLK_IS_ROOT, 48000000),
	FRATE(mphy_refclk_ixtal24, "mphy_refclk_ixtal24", NULL, CLK_IS_ROOT, 48000000),
	FRATE(sclk_usbh20_scan_clk, "sclk_usbh20_scan_clk", NULL, CLK_IS_ROOT, 480000000),
};

struct samsung_fixed_factor_clock exynos5422_fixed_factor_clks[] __initdata = {
	FFACTOR(sclk_hsic_12m, "sclk_hsic_12m", "fin_pll", 1, 2, 0),
	FFACTOR(dout_aclk_66_sw_div2, "dout_aclk_66_sw_div2", "mout_aclk_66_sw", 1, 2, 0),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	FFACTOR(dout_epll_ctrl_div2, "dout_epll_ctrl_div2", "mout_epll_ctrl", 1, 2, 0),
	FFACTOR(dout_spll_ctrl_div2, "dout_spll_ctrl_div2", "mout_spll_ctrl", 1, 2, 0),
	FFACTOR(aclk_550_cam_div2, "aclk_550_cam_div2", "aclk_550_cam", 1, 2, 0),
	FFACTOR(sclk_gscl_wrap_a_div2, "sclk_gscl_wrap_a_div2", "sclk_gscl_wrap_a", 1, 2, 0),
#endif
};

#define CMX(_id, cname, pnames, o, s, w) \
		MUX(_id, cname, pnames, (unsigned long)o, s, w, 0)
#define CMUX(_id, o, s, w) \
		MUX(_id, #_id, _id##_p, (unsigned long)o, s, w, 0)
#define CMUX_EX(_id, o, s, w, f) \
		MUX(_id, #_id, _id##_p, (unsigned long)o, s, w, f)
#define CMUX_A(_id, o, s, w, a) \
		MUX_A(_id, #_id, _id##_p, (unsigned long)o, s, w, a)
struct samsung_mux_clock exynos5422_mux_clks[] __initdata = {
	CMUX(mout_apll_ctrl, EXYNOS5_CLK_SRC_CPU, 0, 1),
	CMUX(mout_kpll_ctrl, EXYNOS5_CLK_SRC_KFC, 0, 1),
	CMUX(mout_bpll_ctrl_user, EXYNOS5_CLK_SRC_CDREX, 0, 1),
	CMUX(mout_cpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 28, 1),
	CMUX(mout_dpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 24, 1),
	CMUX(mout_epll_ctrl, EXYNOS5_CLK_SRC_TOP6, 20, 1),
	CMUX(mout_rpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 16, 1),
	CMUX(mout_ipll_ctrl, EXYNOS5_CLK_SRC_TOP6, 12, 1),
	CMUX(mout_spll_ctrl, EXYNOS5_CLK_SRC_TOP6, 8, 1),
	CMUX(mout_vpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 4, 1),
	CMUX(mout_mpll_ctrl, EXYNOS5_CLK_SRC_TOP6, 0, 1),


	CMUX(mout_cpu, EXYNOS5_CLK_SRC_CPU, 16, 1),
	CMUX(mout_hpm, EXYNOS5_CLK_SRC_CPU, 20, 1),
	CMUX(mout_cpu_kfc, EXYNOS5_CLK_SRC_KFC, 16, 1),
	CMUX(mout_hpm_kfc, EXYNOS5_CLK_SRC_KFC, 15, 1),


	/* CMU-TOP */
	CMX(mout_aclk_200_fsys, "mout_aclk_200_fsys", group1_p, EXYNOS5_CLK_SRC_TOP0, 28, 2),
	CMUX(mout_aclk_200_fsys_sw, EXYNOS5_CLK_SRC_TOP10, 28, 1),
	CMUX(mout_aclk_200_fsys_user, EXYNOS5_CLK_SRC_TOP3, 28, 1),

	CMX(mout_pclk_200_fsys, "mout_pclk_200_fsys", group1_p, EXYNOS5_CLK_SRC_TOP0, 24, 2),
	CMUX(mout_pclk_200_fsys_sw, EXYNOS5_CLK_SRC_TOP10, 24, 1),
	CMUX(mout_pclk_200_fsys_user, EXYNOS5_CLK_SRC_TOP3, 24, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_100_noc, "mout_aclk_100_noc", group1_2_p, EXYNOS5_CLK_SRC_TOP0, 20, 2),
#else
	CMX(mout_aclk_100_noc, "mout_aclk_100_noc", group1_p, EXYNOS5_CLK_SRC_TOP0, 20, 2),
#endif
	CMUX(mout_aclk_100_noc_sw, EXYNOS5_CLK_SRC_TOP10, 20, 1),
	CMUX(mout_aclk_100_noc_user, EXYNOS5_CLK_SRC_TOP3, 20, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_400_wcore, "mout_aclk_400_wcore", group1_3_p, EXYNOS5_CLK_SRC_TOP0, 16, 3),
#else
	CMX(mout_aclk_400_wcore, "mout_aclk_400_wcore", group1_p, EXYNOS5_CLK_SRC_TOP0, 16, 2),
	CMUX(mout_aclk_400_wcore_bpll, EXYNOS5_CMU_TOP_SPARE2, 4, 1),
#endif
	CMUX(mout_aclk_400_wcore_sw, EXYNOS5_CLK_SRC_TOP10, 16, 1),
	CMUX(mout_aclk_400_wcore_user, EXYNOS5_CLK_SRC_TOP3, 16, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_200_fsys2, "mout_aclk_200_fsys2", group1_4_p, EXYNOS5_CLK_SRC_TOP0, 12, 2),
#else
	CMX(mout_aclk_200_fsys2, "mout_aclk_200_fsys2", group1_p, EXYNOS5_CLK_SRC_TOP0, 12, 2),
#endif
	CMUX(mout_aclk_200_fsys2_sw, EXYNOS5_CLK_SRC_TOP10, 12, 1),
	CMUX(mout_aclk_200_fsys2_user, EXYNOS5_CLK_SRC_TOP3, 12, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_200, "mout_aclk_200", group1_4_p, EXYNOS5_CLK_SRC_TOP0, 8, 3),
#else
	CMX(mout_aclk_200, "mout_aclk_200", group1_p, EXYNOS5_CLK_SRC_TOP0, 8, 2),
#endif
	CMUX(mout_aclk_200_sw, EXYNOS5_CLK_SRC_TOP10, 8, 1),
	CMUX(mout_aclk_200_disp1_user, EXYNOS5_CLK_SRC_TOP3, 8, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	MUX(mout_aclk_400_mscl, "mout_aclk_400_mscl", group1_1_p,
			(unsigned long)EXYNOS5_CLK_SRC_TOP0, 4, 3, 0),
#else
	MUX(mout_aclk_400_mscl, "mout_aclk_400_mscl", group1_p,
			(unsigned long)EXYNOS5_CLK_SRC_TOP0, 4, 2),
#endif
	CMUX(mout_aclk_400_mscl_sw, EXYNOS5_CLK_SRC_TOP10, 4, 1),
	CMUX(mout_aclk_400_mscl_user, EXYNOS5_CLK_SRC_TOP3, 4, 1),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_400_isp, "mout_aclk_400_isp", group1_1_p, EXYNOS5_CLK_SRC_TOP0, 0, 3),
#else
	CMX(mout_aclk_400_isp, "mout_aclk_400_isp", group1_p, EXYNOS5_CLK_SRC_TOP0, 0, 2),
#endif
	CMUX(mout_aclk_400_isp_sw, EXYNOS5_CLK_SRC_TOP10, 0, 1),
	CMUX_EX(mout_aclk_400_isp_user, EXYNOS5_CLK_SRC_TOP3, 0, 1, CLK_DO_NOT_UPDATE_CHILD),


#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_333, "mout_aclk_333", group1_2_p, EXYNOS5_CLK_SRC_TOP1, 28, 2),
#else
	CMX(mout_aclk_333, "mout_aclk_333", group1_p, EXYNOS5_CLK_SRC_TOP1, 28, 2),
#endif
	CMUX(mout_aclk_333_sw, EXYNOS5_CLK_SRC_TOP11, 28, 1),
	CMUX(mout_aclk_333_user, EXYNOS5_CLK_SRC_TOP4, 28, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_266, "mout_aclk_266", group1_5_p, EXYNOS5_CLK_SRC_TOP1, 20, 2),
#else
	CMX(mout_aclk_266, "mout_aclk_266", group1_p, EXYNOS5_CLK_SRC_TOP1, 20, 2),
#endif
	CMUX(mout_aclk_266_sw, EXYNOS5_CLK_SRC_TOP11, 20, 1),
	CMUX(mout_aclk_266_user, EXYNOS5_CLK_SRC_TOP4, 20, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_166, "mout_aclk_166", group1_4_p, EXYNOS5_CLK_SRC_TOP1, 24, 2),
#else
	CMX(mout_aclk_166, "mout_aclk_166", group1_p, EXYNOS5_CLK_SRC_TOP1, 24, 2),
#endif
	CMUX(mout_aclk_166_sw, EXYNOS5_CLK_SRC_TOP11, 24, 1),
	CMUX(mout_aclk_166_user, EXYNOS5_CLK_SRC_TOP4, 24, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_66, "mout_aclk_66", group1_4_p, EXYNOS5_CLK_SRC_TOP1, 8, 2),
#else
	CMX(mout_aclk_66, "mout_aclk_66", group1_p, EXYNOS5_CLK_SRC_TOP1, 8, 2),
#endif
	CMUX(mout_aclk_66_sw, EXYNOS5_CLK_SRC_TOP11, 8, 1),
	CMUX(mout_aclk_66_peric_user, EXYNOS5_CLK_SRC_TOP4, 8, 1),
	CMUX(mout_aclk_66_psgen_user, EXYNOS5_CLK_SRC_TOP5, 4, 1),
	CMUX(mout_pclk_66_gpio_user, EXYNOS5_CLK_SRC_TOP7, 4, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_333_432_isp0, "mout_aclk_333_432_isp0", group4_1_p, EXYNOS5_CLK_SRC_TOP1, 12, 2),
#else
	CMX(mout_aclk_333_432_isp0, "mout_aclk_333_432_isp0", group4_p, EXYNOS5_CLK_SRC_TOP1, 12, 2),
#endif
	CMUX(mout_aclk_333_432_isp0_sw, EXYNOS5_CLK_SRC_TOP11, 12, 1),
	CMUX_EX(mout_aclk_333_432_isp0_user, EXYNOS5_CLK_SRC_TOP4, 12, 1, CLK_DO_NOT_UPDATE_CHILD),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_333_432_isp, "mout_aclk_333_432_isp", group4_1_p, EXYNOS5_CLK_SRC_TOP1, 4, 2),
#else
	CMX(mout_aclk_333_432_isp, "mout_aclk_333_432_isp", group4_p, EXYNOS5_CLK_SRC_TOP1, 4, 2),
#endif
	CMUX(mout_aclk_333_432_isp_sw, EXYNOS5_CLK_SRC_TOP11, 4, 1),
	CMUX_EX(mout_aclk_333_432_isp_user, EXYNOS5_CLK_SRC_TOP4, 4, 1, CLK_DO_NOT_UPDATE_CHILD),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_333_432_gscl, "mout_aclk_333_432_gscl", group4_1_p, EXYNOS5_CLK_SRC_TOP1, 0, 2),
#else
	CMX(mout_aclk_333_432_gscl, "mout_aclk_333_432_gscl", group4_p, EXYNOS5_CLK_SRC_TOP1, 0, 2),
#endif
	CMUX(mout_aclk_333_432_gscl_sw, EXYNOS5_CLK_SRC_TOP11, 0, 1),
	CMUX(mout_aclk_333_432_gscl_user, EXYNOS5_CLK_SRC_TOP4, 0, 1),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_300_gscl, "mout_aclk_300_gscl", group1_5_p, EXYNOS5_CLK_SRC_TOP2, 28, 2),
#else
	CMX(mout_aclk_300_gscl, "mout_aclk_300_gscl", group1_p, EXYNOS5_CLK_SRC_TOP2, 28, 2),
#endif
	CMUX(mout_aclk_300_gscl_sw, EXYNOS5_CLK_SRC_TOP12, 28, 1),
	CMUX(mout_aclk_300_gscl_user, EXYNOS5_CLK_SRC_TOP5, 28, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_300_disp1, "mout_aclk_300_disp1", group1_5_p, EXYNOS5_CLK_SRC_TOP2, 24, 2),
#else
	CMX(mout_aclk_300_disp1, "mout_aclk_300_disp1", group1_p, EXYNOS5_CLK_SRC_TOP2, 24, 2),
#endif
	CMUX(mout_aclk_300_disp1_sw, EXYNOS5_CLK_SRC_TOP12, 24, 1),
	CMUX(mout_aclk_300_disp1_user, EXYNOS5_CLK_SRC_TOP5, 24, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_300_jpeg, "mout_aclk_300_jpeg", group1_5_p, EXYNOS5_CLK_SRC_TOP2, 20, 2),
#else
	CMX(mout_aclk_300_jpeg, "mout_aclk_300_jpeg", group1_p, EXYNOS5_CLK_SRC_TOP2, 20, 2),
#endif
	CMUX(mout_aclk_300_jpeg_sw, EXYNOS5_CLK_SRC_TOP12, 20, 1),
	CMUX(mout_aclk_300_jpeg_user, EXYNOS5_CLK_SRC_TOP5, 20, 1),

	CMX(mout_aclk_g3d, "mout_aclk_g3d", group5_p, EXYNOS5_CLK_SRC_TOP2, 16, 1),
	CMUX(mout_aclk_g3d_sw, EXYNOS5_CLK_SRC_TOP12, 16, 1),
	CMUX(mout_aclk_g3d_user, EXYNOS5_CLK_SRC_TOP5, 16, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_266_g2d, "mout_aclk_266_g2d", group1_5_p, EXYNOS5_CLK_SRC_TOP2, 12, 2),
#else
	CMX(mout_aclk_266_g2d, "mout_aclk_266_g2d", group1_p, EXYNOS5_CLK_SRC_TOP2, 12, 2),
#endif
	CMUX(mout_aclk_266_g2d_sw, EXYNOS5_CLK_SRC_TOP12, 12, 1),
	CMUX(mout_aclk_266_g2d_user, EXYNOS5_CLK_SRC_TOP5, 12, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_333_g2d, "mout_aclk_333_g2d", group1_5_p, EXYNOS5_CLK_SRC_TOP2, 8, 2),
#else
	CMX(mout_aclk_333_g2d, "mout_aclk_333_g2d", group1_p, EXYNOS5_CLK_SRC_TOP2, 8, 2),
#endif
	CMUX(mout_aclk_333_g2d_sw, EXYNOS5_CLK_SRC_TOP12, 8, 1),
	CMUX(mout_aclk_333_g2d_user, EXYNOS5_CLK_SRC_TOP5, 8, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_aclk_400_disp1, "mout_aclk_400_disp1", group1_1_p, EXYNOS5_CLK_SRC_TOP2, 0, 2),
#else
	CMX(mout_aclk_400_disp1, "mout_aclk_400_disp1", group1_p, EXYNOS5_CLK_SRC_TOP2, 0, 2),
#endif
	CMUX(mout_aclk_400_disp1_sw, EXYNOS5_CLK_SRC_TOP12, 0, 1),
	CMUX(mout_aclk_400_disp1_user, EXYNOS5_CLK_SRC_TOP5, 0, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_mau_epll_clk, "mout_mau_epll_clk", mout_mau_epll_clk_p, EXYNOS5_CLK_SRC_TOP7, 20, 2),
#else
	CMUX(mout_mau_epll_clk, EXYNOS5_CLK_SRC_TOP7, 20, 2),
#endif

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMUX(mout_mx_mspll_ccore, EXYNOS5_CLK_SRC_TOP7, 16, 3),
#else
	CMUX(mout_mx_mspll_ccore, EXYNOS5_CLK_SRC_TOP7, 16, 2),
#endif
	CMUX(mout_mx_mspll_cpu, EXYNOS5_CLK_SRC_TOP7, 12, 2),
	CMUX(mout_mx_mspll_kfc, EXYNOS5_CLK_SRC_TOP7, 8, 2),

	/* DISP1 BLK */
	CMX(mout_pixel, "mout_pixel", group2_p, EXYNOS5_CLK_SRC_DISP10, 24, 3),
	CMUX(mout_hdmi, EXYNOS5_CLK_SRC_DISP10, 28, 1),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_mdnie1, "mout_mdine1", group2_p, EXYNOS5_CLK_SRC_DISP10, 8, 3),
	CMX(mout_fimd1, "mout_fimd1", group2_p, EXYNOS5_CLK_SRC_DISP10, 4, 3),
#else
	CMX(mout_mdnie1, "mout_mdine1", group2_p, EXYNOS5_CLK_SRC_DISP10, 8, 3),
	CMX(mout_fimd1, "mout_fimd1", group3_p, EXYNOS5_CLK_SRC_DISP10, 4, 1),
	CMUX(mout_fimd1_mdnie1, EXYNOS5_CMU_TOP_SPARE2, 8, 1),
#endif
	/* AUDIO */
	CMUX(mout_mau_audio0, EXYNOS5_CLK_SRC_MAUDIO, 28, 3),
	CMUX(mout_mclk_cdrex, EXYNOS5_CLK_SRC_CDREX, 4, 1),

	/* FSYS_BLK */
	CMX(mout_usbdrd301, "mout_usbdrd301", group2_p, EXYNOS5_CLK_SRC_FSYS, 4, 3),
	CMX(mout_mmc0, "mout_mmc0", group2_p, EXYNOS5_CLK_SRC_FSYS, 8, 3),
	CMX(mout_mmc1, "mout_mmc1", group2_p, EXYNOS5_CLK_SRC_FSYS, 12, 3),
	CMX(mout_mmc2, "mout_mmc2", group2_p, EXYNOS5_CLK_SRC_FSYS, 16, 3),
	CMX(mout_usbdrd300, "mout_usbdrd300", group2_p, EXYNOS5_CLK_SRC_FSYS, 20, 3),
	CMX(mout_unipro, "mout_unipro", group2_p, EXYNOS5_CLK_SRC_FSYS, 24, 3),

	/* ISP_BLK */
	CMX(mout_isp_sensor, "mout_isp_sensor", group2_p, EXYNOS5_CLK_SRC_ISP , 28, 3),
	CMX(mout_pwm_isp , "mout_pwm_isp", group2_p, EXYNOS5_CLK_SRC_ISP, 24, 3),
	CMX(mout_uart_isp , "mout_uart_isp", group2_p, EXYNOS5_CLK_SRC_ISP, 20, 3),
	CMX(mout_spi0_isp , "mout_spi0_isp", group2_p, EXYNOS5_CLK_SRC_ISP, 16, 3),
	CMX(mout_spi1_isp , "mout_spi1_isp", group2_p, EXYNOS5_CLK_SRC_ISP, 12, 3),

	/* PERIC_BLK */
	CMX(mout_pwm, "mout_pwm", group2_p, EXYNOS5_CLK_SRC_PERIC0, 24, 3),
	CMX(mout_uart0, "mout_uart0", group2_p, EXYNOS5_CLK_SRC_PERIC0, 4, 3),
	CMX(mout_uart1, "mout_uart1", group2_p, EXYNOS5_CLK_SRC_PERIC0, 8, 3),
	CMX(mout_uart2, "mout_uart2", group2_p, EXYNOS5_CLK_SRC_PERIC0, 12, 3),
	CMX(mout_uart3, "mout_uart3", group2_p, EXYNOS5_CLK_SRC_PERIC0, 16, 3),
	CMX(mout_spi0, "mout_spi0", group2_p, EXYNOS5_CLK_SRC_PERIC1, 20, 3),
	CMX(mout_spi1, "mout_spi1", group2_p, EXYNOS5_CLK_SRC_PERIC1, 24, 3),
	CMX(mout_spi2, "mout_spi2", group2_p, EXYNOS5_CLK_SRC_PERIC1, 28, 3),
	CMUX(mout_audio0, EXYNOS5_CLK_SRC_PERIC1, 8, 3),
	CMUX(mout_audio1, EXYNOS5_CLK_SRC_PERIC1, 12, 3),
	CMUX(mout_audio2, EXYNOS5_CLK_SRC_PERIC1, 16, 3),
	CMUX(mout_spdif, EXYNOS5_CLK_SRC_PERIC0, 28, 3),

	/* TOP */
	CMX(mout_mphy_refclk, "mout_mphy_refclk", group2_p, EXYNOS5_CLK_SRC_FSYS , 28, 3),

	/* ASS */
	CMX(mout_ass_clk, "mout_ass_clk", mout_ass_clk_p, EXYNOS_CLKSRC_AUDSS, 0, 1),
	CMX(mout_ass_i2s, "mout_ass_i2s", mout_ass_i2s_p, EXYNOS_CLKSRC_AUDSS, 2, 2),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	/* EPLL */
	CMUX_A(mout_epll2, EXYNOS5_CLK_SRC_TOP7, 28, 1, "sclk_epll2"),

	CMX(mout_aclk_266_isp, "mout_aclk_266_isp", group1_6_p, EXYNOS5_CLK_SRC_TOP8, 12, 2),
	CMUX(mout_aclk_266_isp_sw, EXYNOS5_CLK_SRC_TOP13, 12, 1),
	CMUX_EX(mout_aclk_266_isp_user, EXYNOS5_CLK_SRC_TOP9, 12, 1, CLK_DO_NOT_UPDATE_CHILD),

	CMX(mout_aclk_432_scaler, "mout_aclk_432_scaler", group4_1_p, EXYNOS5_CLK_SRC_TOP8, 28, 2),
	CMUX(mout_aclk_432_scaler_sw, EXYNOS5_CLK_SRC_TOP13, 28, 1),
	CMUX_EX(mout_aclk_432_scaler_user, EXYNOS5_CLK_SRC_TOP9, 28, 1, CLK_DO_NOT_UPDATE_CHILD),

	CMX(mout_aclk_432_cam, "mout_aclk_432_cam", group4_1_p, EXYNOS5_CLK_SRC_TOP8, 24, 2),
	CMUX(mout_aclk_432_cam_sw, EXYNOS5_CLK_SRC_TOP13, 24, 1),
	CMUX(mout_aclk_432_cam_user, EXYNOS5_CLK_SRC_TOP9, 24, 1),

	CMX(mout_aclk_fl1_550_cam, "mout_aclk_fl1_550_cam", group1_1_p, EXYNOS5_CLK_SRC_TOP8, 20, 3),
	CMUX(mout_aclk_fl1_550_cam_sw, EXYNOS5_CLK_SRC_TOP13, 20, 1),
	CMUX(mout_aclk_fl1_550_cam_user, EXYNOS5_CLK_SRC_TOP9, 20, 1),

	CMX(mout_aclk_550_cam, "mout_aclk_550_cam", group1_1_p, EXYNOS5_CLK_SRC_TOP8, 16, 3),
	CMUX(mout_aclk_550_cam_sw, EXYNOS5_CLK_SRC_TOP13, 16, 1),
	CMUX(mout_aclk_550_cam_user, EXYNOS5_CLK_SRC_TOP9, 16, 1),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_mau_epll_clk_user, "mout_mau_epll_clk_user", mout_mau_epll_clk_user_p, EXYNOS5_CLK_SRC_TOP9, 8, 1),
#else
	CMUX(mout_mau_epll_clk_user, EXYNOS5_CLK_SRC_TOP9, 8, 1),
#endif

	CMUX(mout_mx_mspll_ccore_phy, EXYNOS5_CLK_SRC_TOP7, 0, 3),

	/* GSCL */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CMX(mout_gscl_wrap_a, "mout_gscl_wrap_a", group2_p, EXYNOS5_CLK_SRC_CAM, 28, 3),
	CMX(mout_gscl_wrap_b, "mout_gscl_wrap_b", group2_p, EXYNOS5_CLK_SRC_CAM, 24, 3),
#else
	CMX(mout_gscl_wrap_a, "mout_gscl_wrap_a", group3_p, EXYNOS5_CLK_SRC_CAM, 28, 3),
	CMX(mout_gscl_wrap_b, "mout_gscl_wrap_b", group3_p, EXYNOS5_CLK_SRC_CAM, 24, 3),
#endif
#endif
};

#define CDV(_id, cname, pname, o, s, w) \
		DIV(_id, cname, pname, (unsigned long)o, s, w)
#define CDIV(_id, pname, o, s, w) \
		DIV(_id, #_id, pname, (unsigned long)o, s, w)
#define CDIV_A(_id, pname, o, s, w, a) \
		DIV_A(_id, #_id, pname, (unsigned long)o, s, w, a)
struct samsung_div_clock exynos5422_div_clks[] __initdata = {
	CDIV(dout_arm2_msb, "mout_cpu", EXYNOS5_CLK_DIV_CPU0, 0, 3),
	CDIV(dout_arm2_lsb, "mout_cpu", EXYNOS5_CLK_DIV_CPU0, 28, 3),
	CDIV(dout_apll, "mout_apll_ctrl", EXYNOS5_CLK_DIV_CPU0, 24, 3),
	CDIV(dout_cpud, "dout_arm2_lsb", EXYNOS5_CLK_DIV_CPU0, 4, 3),
	CDIV(dout_atb, "dout_arm2_lsb", EXYNOS5_CLK_DIV_CPU0, 16, 3),
	CDIV(dout_pclk_dbg, "dout_arm2_lsb", EXYNOS5_CLK_DIV_CPU0, 20, 3),
	CDIV(dout_copy, "mout_hpm", EXYNOS5_CLK_MUX_STAT_CPU, 20, 3),
	CDIV(dout_hpm, "dout_copy", EXYNOS5_CLK_DIV_CPU1, 4, 3),

	CDIV(dout_kfc, "mout_cpu_kfc", EXYNOS5_CLK_DIV_KFC0, 0, 3),
	CDIV(dout_kpll, "mout_kpll_ctrl", EXYNOS5_CLK_DIV_KFC0, 24, 3),
	CDIV(dout_aclk, "dout_kfc", EXYNOS5_CLK_DIV_KFC0, 4, 3),
	CDIV(dout_pclk, "dout_kfc", EXYNOS5_CLK_DIV_KFC0, 20, 3),
	CDIV(dout_hpm_kfc, "mout_hpm_kfc", EXYNOS5_CLK_DIV_KFC0, 8, 3),

    /* CMU_TOP */
	CDIV(dout_aclk_200_fsys, "mout_aclk_200_fsys", EXYNOS5_CLK_DIV_TOP0, 28, 3),

    CDIV(dout_pclk_200_fsys, "mout_pclk_200_fsys", EXYNOS5_CLK_DIV_TOP0, 24, 3),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
    CDIV(dout_aclk_100_noc, "mout_aclk_100_noc", EXYNOS5_CLK_DIV_TOP0, 20, 4),
#else
    CDIV(dout_aclk_100_noc, "mout_aclk_100_noc", EXYNOS5_CLK_DIV_TOP0, 20, 3),
#endif

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
    CDIV(dout_aclk_400_wcore, "mout_aclk_400_wcore", EXYNOS5_CLK_DIV_TOP0, 16, 3),
#else
    CDIV(dout_aclk_400_wcore, "mout_aclk_400_wcore_bpll", EXYNOS5_CLK_DIV_TOP0, 16, 3),
#endif
    CDIV(dout_aclk_200_fsys2, "mout_aclk_200_fsys2", EXYNOS5_CLK_DIV_TOP0, 12, 3),
    CDIV(dout_aclk_200, "mout_aclk_200", EXYNOS5_CLK_DIV_TOP0, 8, 3),
    CDIV(dout_aclk_400_mscl, "mout_aclk_400_mscl", EXYNOS5_CLK_DIV_TOP0, 4, 3),
    CDIV(dout_aclk_400_isp, "mout_aclk_400_isp", EXYNOS5_CLK_DIV_TOP0, 0, 3),
	CDIV(dout_aclk_333, "mout_aclk_333", EXYNOS5_CLK_DIV_TOP1, 28, 3),
    CDIV(dout_aclk_166, "mout_aclk_166", EXYNOS5_CLK_DIV_TOP1, 24, 3),
    CDIV(dout_aclk_266, "mout_aclk_266", EXYNOS5_CLK_DIV_TOP1, 20, 3),
    CDIV(dout_aclk_66, "mout_aclk_66", EXYNOS5_CLK_DIV_TOP1, 8, 6),

    CDIV(dout_aclk_333_432_isp0, "mout_aclk_333_432_isp0",    EXYNOS5_CLK_DIV_TOP1, 16, 3),
    CDIV(dout_aclk_333_432_isp, "mout_aclk_333_432_isp",    EXYNOS5_CLK_DIV_TOP1, 4, 3),
    CDIV(dout_aclk_333_432_gscl, "mout_aclk_333_432_gscl",  EXYNOS5_CLK_DIV_TOP1, 0, 3),
    CDIV(dout_aclk_300_disp1, "mout_aclk_300_disp1", EXYNOS5_CLK_DIV_TOP2, 24, 3),
	CDIV(dout_aclk_300_jpeg, "mout_aclk_300_jpeg", EXYNOS5_CLK_DIV_TOP2, 20, 3),

    CDIV(dout_aclk_g3d, "mout_aclk_g3d", EXYNOS5_CLK_DIV_TOP2, 16, 3),
    CDIV(dout_aclk_266_g2d, "mout_aclk_266_g2d", EXYNOS5_CLK_DIV_TOP2, 12, 3),
    CDIV(dout_aclk_333_g2d, "mout_aclk_333_g2d", EXYNOS5_CLK_DIV_TOP2, 8, 3),
    CDIV(dout_aclk_400_disp1, "mout_aclk_400_disp1", EXYNOS5_CLK_DIV_TOP2, 4, 3),
	CDIV(dout_aclk_300_gscl, "mout_aclk_300_gscl", EXYNOS5_CLK_DIV_TOP2, 28, 3),

	/* DISP1 Block */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CDIV(dout_fimd1, "mout_fimd1", EXYNOS5_CLK_DIV_DISP10, 0, 4),
#else
	CDIV(dout_fimd1, "mout_fimd1_mdnie1", EXYNOS5_CLK_DIV_DISP10, 0, 4),
#endif
	CDIV(dout_mipi1, "mout_mipi1", EXYNOS5_CLK_DIV_DISP10, 16, 8),
	CDIV(dout_dp1_ext_mst_vid, "mout_dp1_ext_mst_vid", EXYNOS5_CLK_DIV_DISP10, 24, 4),
	CDIV(dout_hdmi_pixel, "mout_pixel", EXYNOS5_CLK_DIV_DISP10, 28, 4),
	CDIV(dout2_disp1_blk, "mout_aclk_200_disp1", EXYNOS5_CLK_DIV2_RATIO0, 16, 2),/* TODO */
	/* Audio Block */
	CDIV(dout_mau_audio0, "mout_mau_audio0", EXYNOS5_CLK_DIV_MAUDIO, 20, 4),
	CDIV(dout_mau_pcm0, "dout_mau_audio0", EXYNOS5_CLK_DIV_MAUDIO, 24, 8),

	/* USB3.0 */
	CDIV(dout_usbphy301, "mout_usbdrd301", EXYNOS5_CLK_DIV_FSYS0, 12, 4),
	CDIV(dout_usbphy300, "mout_usbdrd300", EXYNOS5_CLK_DIV_FSYS0, 16, 4),
	CDIV(dout_usbdrd301, "mout_usbdrd301", EXYNOS5_CLK_DIV_FSYS0, 20, 4),
	CDIV(dout_usbdrd300, "mout_usbdrd300", EXYNOS5_CLK_DIV_FSYS0, 24, 4),

	/* MMC */
	CDIV(dout_mmc0, "mout_mmc0", EXYNOS5_CLK_DIV_FSYS1, 0, 10),
	CDIV(dout_mmc1, "mout_mmc1", EXYNOS5_CLK_DIV_FSYS1, 10, 10),
	CDIV(dout_mmc2, "mout_mmc2", EXYNOS5_CLK_DIV_FSYS1, 20, 10),

	CDIV(dout_unipro, "mout_unipro", EXYNOS5_CLK_DIV_FSYS2, 24, 8),

	/* UART and PWM */
	CDIV(dout_uart0, "mout_uart0", EXYNOS5_CLK_DIV_PERIC0, 8, 4),
	CDIV(dout_uart1, "mout_uart1", EXYNOS5_CLK_DIV_PERIC0, 12, 4),
	CDIV(dout_uart2, "mout_uart2", EXYNOS5_CLK_DIV_PERIC0, 16, 4),
	CDIV(dout_uart3, "mout_uart3", EXYNOS5_CLK_DIV_PERIC0, 20, 4),
	CDIV(dout_pwm, "mout_pwm", EXYNOS5_CLK_DIV_PERIC0, 28, 4),

	/* SPI */
	CDIV(dout_spi0, "mout_spi0", EXYNOS5_CLK_DIV_PERIC1, 20, 4),
	CDIV(dout_spi1, "mout_spi1", EXYNOS5_CLK_DIV_PERIC1, 24, 4),
	CDIV(dout_spi2, "mout_spi2", EXYNOS5_CLK_DIV_PERIC1, 28, 4),

	/* PCM */
	CDIV(dout_pcm1, "dout_audio1", EXYNOS5_CLK_DIV_PERIC2, 16, 8),
	CDIV(dout_pcm2, "dout_audio2", EXYNOS5_CLK_DIV_PERIC2, 24, 8),

	/* Audio - I2S */
	CDIV(dout_i2s1, "dout_audio1", EXYNOS5_CLK_DIV_PERIC3, 6, 6),
	CDIV(dout_i2s2, "dout_audio2", EXYNOS5_CLK_DIV_PERIC3, 12, 6),
	CDIV(dout_audio0, "mout_audio0", EXYNOS5_CLK_DIV_PERIC3, 20, 4),
	CDIV(dout_audio1, "mout_audio1", EXYNOS5_CLK_DIV_PERIC3, 24, 4),
	CDIV(dout_audio2, "mout_audio2", EXYNOS5_CLK_DIV_PERIC3, 28, 4),

	/* SPI Pre-Ratio */
	CDIV(dout_spi0_pre, "dout_spi0", EXYNOS5_CLK_DIV_PERIC4, 8, 8),
	CDIV(dout_spi1_pre, "dout_spi1", EXYNOS5_CLK_DIV_PERIC4, 16, 8),
	CDIV(dout_spi2_pre, "dout_spi2", EXYNOS5_CLK_DIV_PERIC4, 24, 8),

	/* DREX */
	CDIV(dout_pclk_cdrex, "dout_aclk_cdrex1", EXYNOS5_CLK_DIV_CDREX0, 28, 3),
	CDIV(dout_pclk_drex0, "dout_cclk_drex0", EXYNOS5_CLK_DIV_CDREX0, 28, 3),
	CDIV(dout_pclk_drex1, "dout_cclk_drex1", EXYNOS5_CLK_DIV_CDREX0, 28, 3),
	CDIV(dout_sclk_cdrex, "mout_mclk_cdrex", EXYNOS5_CLK_DIV_CDREX0, 24, 3),

	CDIV(dout_cclk_drex0, "dout_clk2x_phy0", EXYNOS5_CLK_DIV_CDREX0, 8, 3),
	CDIV(dout_cclk_drex1, "dout_clk2x_phy0", EXYNOS5_CLK_DIV_CDREX0, 8, 3),
	CDIV(dout_aclk_cdrex1, "dout_clk2x_phy0", EXYNOS5_CLK_DIV_CDREX0, 16, 3),

	CDIV(dout_clk2x_phy0, "dout_sclk_cdrex", EXYNOS5_CLK_DIV_CDREX0, 3, 5),
	CDIV(dout_pclk_core_mem, "mout_mclk_cdrex", EXYNOS5_CLK_DIV_CDREX1, 8, 3),

	/* G2D */
	CDIV(dout_acp_pclk, "aclk_266_g2d", EXYNOS5_CLK_DIV_G2D, 4, 3),

	/* ASS */
	CDIV(dout_ass_srp, "mout_ass_clk", EXYNOS_CLKDIV_AUDSS, 0, 15),
	CDIV(dout_ass_bus, "dout_ass_srp", EXYNOS_CLKDIV_AUDSS, 4, 15),
	CDIV(dout_ass_i2s, "mout_ass_i2s", EXYNOS_CLKDIV_AUDSS, 8, 15),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	/* TOP */
	CDIV(dout_spll_ctrl, "mout_spll_ctrl", EXYNOS5_CLK_DIV_TOP9, 24, 6),
	CDIV(dout_osc, "fin_pll", EXYNOS5_CLK_DIV_TOP9, 20, 3),
	CDIV(dout_aclk_266_isp, "mout_aclk_266_isp", EXYNOS5_CLK_DIV_TOP8, 12, 3),
	CDIV(dout_aclk_432_scaler, "mout_aclk_432_scaler", EXYNOS5_CLK_DIV_TOP8, 28, 3),
	CDIV(dout_aclk_432_cam, "mout_aclk_432_cam", EXYNOS5_CLK_DIV_TOP8, 24, 3),
	CDIV(dout_aclk_fl1_550_cam, "mout_aclk_fl1_550_cam", EXYNOS5_CLK_DIV_TOP8, 20, 3),
	CDIV(dout_aclk_550_cam, "mout_aclk_550_cam", EXYNOS5_CLK_DIV_TOP8, 16, 3),

	/* GSCL */
	CDIV(dout_gscl_wrap_a, "mout_gscl_wrap_a", EXYNOS5_CLK_DIV_CAM, 28, 4),
	CDIV(dout_gscl_wrap_b, "mout_gscl_wrap_b", EXYNOS5_CLK_DIV_CAM, 24, 4),
	CDIV(dout2_gscl_blk_300, "aclk_300_gscl", EXYNOS5_CLK_DIV2_RATIO0, 4, 2),
	CDIV(dout2_gscl_blk_333, "aclk_333_432_gscl", EXYNOS5_CLK_DIV2_RATIO0, 6, 2),
	CDIV(dout2_cam_blk_432, "aclk_432_cam", EXYNOS5_CLK_DIV4_RATIO, 12, 2),
	CDIV(dout2_cam_blk_550, "aclk_fl1_550_cam", EXYNOS5_CLK_DIV4_RATIO, 8, 2),

	/* ISP */
	CDIV(dout_isp_sensor0, "mout_isp_sensor", EXYNOS5_SCLK_DIV_ISP0, 8, 8),
	CDIV(dout_isp_sensor1, "mout_isp_sensor", EXYNOS5_SCLK_DIV_ISP0, 16, 8),
	CDIV(dout_isp_sensor2, "mout_isp_sensor", EXYNOS5_SCLK_DIV_ISP0, 24, 8),

	CDIV(dout_pwm_isp, "mout_pwm_isp", EXYNOS5_SCLK_DIV_ISP1, 28, 4),
	CDIV(dout_uart_isp, "mout_uart_isp", EXYNOS5_SCLK_DIV_ISP1, 24, 4),

	CDIV(dout_spi0_isp, "mout_spi0_isp", EXYNOS5_SCLK_DIV_ISP1, 16, 4),
	CDIV(dout_spi1_isp, "mout_spi1_isp", EXYNOS5_SCLK_DIV_ISP1, 20, 4),

	CDIV(dout_spi0_isp_pre, "dout_spi0_isp", EXYNOS5_SCLK_DIV_ISP1, 0, 8),
	CDIV(dout_spi1_isp_pre, "dout_spi1_isp", EXYNOS5_SCLK_DIV_ISP1, 8, 8),

	CDIV(dout_ispdiv0_0, "aclk_333_432_isp0", EXYNOS5_CLK_DIV_ISP0, 0, 3),
	CDIV(dout_mcuispdiv0, "aclk_400_isp", EXYNOS5_CLK_DIV_ISP1, 0, 3),
	CDIV(dout_mcuispdiv1, "aclk_400_isp", EXYNOS5_CLK_DIV_ISP1, 4, 3),
	CDIV(dout_ispdiv0, "aclk_333_432_isp", EXYNOS5_CLK_DIV_ISP0, 0, 3),
	CDIV(dout_ispdiv1, "aclk_333_432_isp", EXYNOS5_CLK_DIV_ISP0, 4, 3),
	CDIV(dout_ispdiv2, "dout_ispdiv1", EXYNOS5_CLK_DIV_ISP2, 0, 3),
#endif
};

#define CGATE(_id, cname, pname, o, b, f, gf) \
		GATE(_id, cname, pname, (unsigned long)o, b, f, gf)

#define CMGATE(_id, cname, pname, o, b, f, gf, s) \
		MGATE(_id, cname, pname, (unsigned long)o, b, f, gf, (unsigned long)s)

struct samsung_gate_clock exynos5422_gate_clks[] __initdata = {
	GATE_A(pclk_st, "pclk_st", "aclk_66_psgen", (unsigned long) EXYNOS5_CLK_GATE_BUS_PERIS1, 2, 0, 0, "mct"),
	/* CMU_CPU */
	CGATE(aclk_axi_cpud, "aclk_axi_cpud", "dout_cpud", EXYNOS5_CLK_GATE_BUS_CPU, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(sclk_hpm, "sclk_hpm", "dout_hpm", EXYNOS5_CLK_GATE_SCLK_CPU, 1, CLK_IGNORE_UNUSED, 0),

	/* CMU_TOP */
	CGATE(aclk_noc_fsys, "aclk_noc_fsys", "mout_aclk_200_fsys_user",
			EXYNOS5_CLK_GATE_BUS_FSYS0, 9, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_noc_fsys2, "aclk_noc_fsys2", "mout_aclk_200_fsys2_user",
			EXYNOS5_CLK_GATE_BUS_FSYS0, 10, CLK_IGNORE_UNUSED, 0),

	CGATE(aclk_200_disp1, "aclk_200_disp1", "mout_aclk_200_disp1_user", EXYNOS5_CLK_GATE_BUS_TOP, 18, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_400_mscl, "aclk_400_mscl", "mout_aclk_400_mscl_user", EXYNOS5_CLK_GATE_BUS_TOP, 17, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_400_isp, "aclk_400_isp", "mout_aclk_400_isp_user", EXYNOS5_CLK_GATE_BUS_TOP, 16, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_66_peric, "aclk_66_peric", "mout_aclk_66_peric_user", EXYNOS5_CLK_GATE_BUS_TOP, 11, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_66_psgen, "aclk_66_psgen", "mout_aclk_66_psgen_user", EXYNOS5_CLK_GATE_BUS_TOP, 10, CLK_IGNORE_UNUSED, 0),
	CGATE(pclk_66_gpio, "pclk_66_gpio", "mout_pclk_66_gpio_user", EXYNOS5_CLK_GATE_BUS_TOP, 9, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_432_isp, "aclk_333_432_isp", "mout_aclk_333_432_isp_user", EXYNOS5_CLK_GATE_BUS_TOP, 8, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_432_gscl, "aclk_333_432_gscl", "mout_aclk_333_432_gscl_user", EXYNOS5_CLK_GATE_BUS_TOP, 7, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_300_gscl, "aclk_300_gscl", "mout_aclk_300_gscl_user", EXYNOS5_CLK_GATE_BUS_TOP, 6, CLK_IGNORE_UNUSED, 0),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CGATE(aclk_333_432_isp0, "aclk_333_432_isp0", "mout_aclk_333_432_isp0_user", EXYNOS5_CLK_GATE_BUS_TOP, 5, CLK_IGNORE_UNUSED, 0),
#else
	CGATE(aclk_333_432_isp0, "aclk_333_432_isp0", "mout_aclk_333_432_isp0_user", EXYNOS5_CLK_GATE_BUS_TOP, 5, CLK_IGNORE_UNUSED, 0),
#endif
	CGATE(aclk_300_jpeg, "aclk_300_jpeg", "mout_aclk_300_jpeg_user", EXYNOS5_CLK_GATE_BUS_TOP, 4, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_333_g2d, "aclk_333_g2d", "mout_aclk_333_g2d_user", EXYNOS5_CLK_GATE_BUS_TOP, 0, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_266_g2d, "aclk_266_g2d", "mout_aclk_266_g2d_user", EXYNOS5_CLK_GATE_BUS_TOP, 1, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_166, "aclk_166", "mout_aclk_166_user", EXYNOS5_CLK_GATE_BUS_TOP, 14, CLK_IGNORE_UNUSED, 0),
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CGATE(aclk_333, "aclk_333", "mout_aclk_333_user", EXYNOS5_CLK_GATE_BUS_TOP, 15, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_266_isp, "aclk_266_isp", "mout_aclk_266_isp_user", EXYNOS5_CLK_GATE_BUS_TOP, 13, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_432_scaler, "aclk_432_scaler", "mout_aclk_432_scaler_user", EXYNOS5_CLK_GATE_BUS_TOP, 27, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_432_cam, "aclk_432_cam", "mout_aclk_432_cam_user", EXYNOS5_CLK_GATE_BUS_TOP, 26, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_fl1_550_cam, "aclk_fl1_550_cam", "mout_aclk_fl1_550_cam_user", EXYNOS5_CLK_GATE_BUS_TOP, 25, CLK_IGNORE_UNUSED, 0),
	CGATE(aclk_550_cam, "aclk_550_cam", "mout_aclk_550_cam_user", EXYNOS5_CLK_GATE_BUS_TOP, 24, CLK_IGNORE_UNUSED, 0),
	CGATE(mau_epll_clk, "mau_epll_clk", "mout_mau_epll_clk_user", EXYNOS5_CLK_GATE_BUS_TOP, 23, CLK_IGNORE_UNUSED, 0),
/* no gate exist for mx_mspll_ccore_phy */
/* temporary use src mask */
	CGATE(mx_mspll_ccore_phy, "mx_mspll_ccore_phy", "mout_mx_mspll_ccore_phy", EXYNOS5_CLK_SRC_MASK_TOP7, 0, CLK_IGNORE_UNUSED, 0),
#endif

	/* sclk */
	CGATE(sclk_uart0, "sclk_uart0", "dout_uart0",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_uart1, "sclk_uart1", "dout_uart1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 1, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_uart2, "sclk_uart2", "dout_uart2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 2, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_uart3, "sclk_uart3", "dout_uart3",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spi0, "sclk_spi0", "dout_spi0_pre",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 6, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spi1, "sclk_spi1", "dout_spi1_pre",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 7, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spi2, "sclk_spi2", "dout_spi2_pre",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 8, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_spdif, "sclk_spdif", "mout_spdif",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pwm, "sclk_pwm", "dout_pwm",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 11, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pcm1, "sclk_pcm1", "dout_pcm1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 15, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pcm2, "sclk_pcm2", "dout_pcm2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 16, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_i2s1, "sclk_i2s1", "dout_i2s1",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 17, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_i2s2, "sclk_i2s2", "dout_i2s2",
		EXYNOS5_CLK_GATE_TOP_SCLK_PERIC, 18, CLK_SET_RATE_PARENT, 0),

	CGATE(sclk_mmc0, "sclk_mmc0", "dout_mmc0",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mmc1, "sclk_mmc1", "dout_mmc1",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 1, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mmc2, "sclk_mmc2", "dout_mmc2",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 2, CLK_SET_RATE_PARENT, 0),

	CGATE(sclk_usbphy301, "sclk_usbphy301", "dout_usbphy301",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 7, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbphy300, "sclk_usbphy300", "dout_usbphy300",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 8, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbdrd300, "sclk_usbdrd300", "dout_usbdrd300",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_usbdrd301, "sclk_usbdrd301", "dout_usbdrd301",
		EXYNOS5_CLK_GATE_TOP_SCLK_FSYS, 10, CLK_SET_RATE_PARENT, 0),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CGATE(sclk_gscl_wrap_a, "sclk_gscl_wrap_a", "dout_gscl_wrap_a",
		EXYNOS5_CLK_GATE_TOP_SCLK_GSCL, 6, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_gscl_wrap_b, "sclk_gscl_wrap_b", "dout_gscl_wrap_b",
		EXYNOS5_CLK_GATE_TOP_SCLK_GSCL, 7, CLK_SET_RATE_PARENT, 0),
#else
	CGATE(sclk_gscl_wrap_a, "sclk_gscl_wrap_a", "aclK333_432_gscl",
		EXYNOS5_CLK_GATE_TOP_SCLK_GSCL, 6, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_gscl_wrap_b, "sclk_gscl_wrap_b", "aclk333_432_gscl",
		EXYNOS5_CLK_GATE_TOP_SCLK_GSCL, 7, CLK_SET_RATE_PARENT, 0),
#endif
	/* Display */
	CGATE(sclk_fimd1, "sclk_fimd1", "dout_fimd1",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 0, CLK_SET_RATE_PARENT, 0),

    /* mixer, disp */
	CGATE(aclk_axi_disp1x, "aclk_axi_disp1x", "mout_aclk_300_disp1_user",
		EXYNOS5_CLK_GATE_BUS_DISP1, 4, CLK_SET_RATE_PARENT, 0),

	CGATE(sclk_mipi1, "sclk_mipi1", "dout_mipi1",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 3, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_hdmi, "sclk_hdmi", "mout_hdmi",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 9, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_pixel, "sclk_pixel", "dout_hdmi_pixel",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 10, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_dp1_ext_mst_vid, "sclk_dp1_ext_mst_vid", "dout_dp1_ext_mst_vid",
		EXYNOS5_CLK_GATE_TOP_SCLK_DISP1, 20, CLK_SET_RATE_PARENT, 0),

	/* Maudio Block */
	CGATE(sclk_mau_audio0, "sclk_mau_audio0", "dout_mau_audio0",
		EXYNOS5_CLK_GATE_TOP_SCLK_MAU, 0, CLK_SET_RATE_PARENT, 0),
	CGATE(sclk_mau_pcm0, "sclk_mau_pcm0", "dout_mau_pcm0",
		EXYNOS5_CLK_GATE_TOP_SCLK_MAU, 1, CLK_SET_RATE_PARENT, 0),

	/* FSYS */
	CGATE(clk_usbhost20, "clk_usbhost20", "aclk_noc_fsys", EXYNOS5_CLK_GATE_IP_FSYS, 18, 0, 0),
	CMGATE(clk_usbdrd301, "clk_usbdrd301", "aclk_noc_fsys", EXYNOS5_CLK_GATE_IP_FSYS, 20, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 26 | 0x1 << 20),
	CMGATE(clk_usbdrd300, "clk_usbdrd300", "aclk_noc_fsys", EXYNOS5_CLK_GATE_IP_FSYS, 19, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 25 | 0x1 << 19),
	CMGATE(clk_ufs, "clk_ufs", "dout_unipro", EXYNOS5_CLK_GATE_IP_FSYS, 23, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 23 | 0x1 << 22),
	CGATE(clk_sdmmc2, "clk_sdmmc2", "aclk_noc_fsys2", EXYNOS5_CLK_GATE_IP_FSYS, 14, 0, 0),
	CGATE(clk_sdmmc1, "clk_sdmmc1", "aclk_noc_fsys2", EXYNOS5_CLK_GATE_IP_FSYS, 13, 0, 0),
	CGATE(clk_sdmmc0, "clk_sdmmc0", "aclk_noc_fsys2", EXYNOS5_CLK_GATE_IP_FSYS, 12, 0, 0),

	CGATE(aclk_pdma0, "aclk_pdma0", "aclk_noc_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 1, 0, 0),
	CGATE(aclk_pdma1, "aclk_pdma1", "aclk_noc_fsys", EXYNOS5_CLK_GATE_BUS_FSYS0, 2, 0, 0),

	/* PERIS */
	CGATE(clk_wdt, "clk_wdt", "aclk_66_psgen", EXYNOS5_CLK_GATE_IP_PERIS, 19, 0, 0),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
/* CLK_GATE_IP_GSCL0 */
	CGATE(clk_camif_top_fimcl3, "clk_camif_top_fimcl3", "sclk_gscl_wrap_a_div2", EXYNOS5_CLK_GATE_IP_GSCL0, 13, 0, 0),
	CGATE(clk_camif_top_fimcl1, "clk_camif_top_fimcl1", "dout2_cam_blk_550", EXYNOS5_CLK_GATE_IP_GSCL0, 11, 0, 0),
	CGATE(clk_camif_top_fimcl0, "clk_camif_top_fimcl0", "sclk_gscl_wrap_a_div2", EXYNOS5_CLK_GATE_IP_GSCL0, 10, 0, 0),
	CGATE(clk_camif_top_3aa, "clk_camif_top_3aa", "dout2_cam_blk_432", EXYNOS5_CLK_GATE_IP_GSCL0, 9, 0, 0),
	CGATE(gscl_fimc_lite1, "gscl_fimc_lite1", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 6, 0, 0),
	CGATE(gscl_fimc_lite0, "gscl_fimc_lite0", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 5, 0, 0),
	CGATE(clk_3aa, "clk_3aa", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 4, 0, 0),
	CMGATE(clk_gscl0, "clk_gscl0", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 0, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 28 | 0x1 << 14 | 0x1 << 0),
	CMGATE(clk_gscl1, "clk_gscl1", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 1, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 29 | 0x1 << 15 | 0x1 << 1),

/* CLK_GATE_IP_GSCL1 */
	CGATE(clk_camif_top_csis0, "clk_camif_top_csis0", "dout2_gscl_blk_333", EXYNOS5_CLK_GATE_IP_GSCL1, 18, 0, 0),
	CGATE(gscl_fimc_lite3, "gscl_fimc_lite3", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 17, 0, 0),
	CGATE(clk_smmu_fimcl3, "clk_smmu_fimcl3", "dout2_gscl_blk_333", EXYNOS5_CLK_GATE_IP_GSCL1, 16, 0, 0),
	CGATE(clk_gscl_wrap_b, "clk_gscl_wrap_b", "dout_gscl_wrap_b", EXYNOS5_CLK_GATE_IP_GSCL1, 13, 0, 0),
	CGATE(clk_gscl_wrap_a, "clk_gscl_wrap_a", "dout_gscl_wrap_a", EXYNOS5_CLK_GATE_IP_GSCL1, 12, 0, 0),
	CGATE(clk_smmu_gscl1, "clk_smmu_gscl1", "dout2_gscl_blk_300", EXYNOS5_CLK_GATE_IP_GSCL1, 7, 0, 0),
	CGATE(clk_smmu_gscl0, "clk_smmu_gscl0", "dout2_gscl_blk_300", EXYNOS5_CLK_GATE_IP_GSCL1, 6, 0, 0),
	CGATE(clk_smmu_fimcl1, "clk_smmu_fimcl1", "dout2_gscl_blk_333", EXYNOS5_CLK_GATE_IP_GSCL1, 4, 0, 0),
	CGATE(clk_smmu_fimcl0, "clk_smmu_fimcl0", "dout2_gscl_blk_333", EXYNOS5_CLK_GATE_IP_GSCL1, 3, 0, 0),
	CGATE(clk_smmu_3aa, "clk_smmu_3aa", "dout2_gscl_blk_333", EXYNOS5_CLK_GATE_IP_GSCL1, 2, 0, 0),
#else
	/* GSCL */
	CGATE(clk_gscl0, "clk_gscl0", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 0, 0, 0),
	CGATE(clk_gscl1, "clk_gscl1", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 1, 0, 0),
	CGATE(clk_3aa, "clk_3aa", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL0, 4, 0, 0),

	CGATE(clk_smmu_3aa, "clk_smmu_3aa", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 2, 0, 0),
	CGATE(clk_smmu_fimcl0, "clk_smmu_fimcl0", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 3, 0, 0),
	CGATE(clk_smmu_fimcl1, "clk_smmu_fimcl1", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 4, 0, 0),
	CGATE(clk_smmu_gscl0, "clk_smmu_gscl0", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 6, 0, 0),
	CGATE(clk_smmu_gscl1, "clk_smmu_gscl1", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 7, 0, 0),

	CGATE(clk_gscl_wrap_a, "clk_gscl_wrap_a", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 12, 0, 0),
	CGATE(clk_gscl_wrap_b, "clk_gscl_wrap_b", "aclk_300_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 13, 0, 0),
	CGATE(clk_smmu_fimcl3, "clk_smmu_fimcl3,", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 16, 0, 0),
	CGATE(gscl_fimc_lite3, "gscl_fimc_lite3", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_GSCL1, 17, 0, 0),
#endif
	/* CLK_GATE_IP_DISP1 */
	CMGATE(clk_fimd1, "clk_fimd1", "dout_fimd1", EXYNOS5_CLK_GATE_IP_DISP1, 0, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 10| 0x1 << 11 | 0x1 << 0),
	CGATE(clk_mie1, "clk_mie1", "mout_aclk_300_disp1_user", EXYNOS5_CLK_GATE_IP_DISP1, 1, 0, 0),
	CGATE(clk_mdnie1, "clk_mdnie1", "mout_aclk_300_disp1_user", EXYNOS5_CLK_GATE_IP_DISP1, 2, 0, 0),
	CGATE(clk_dsim1, "clk_dsim1", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 3, 0, 0),
	CGATE(clk_dp1, "clk_dp1", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 4, 0, 0),
	CMGATE(clk_mixer, "clk_mixer", "sclk_hdmi", EXYNOS5_CLK_GATE_IP_DISP1, 5, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 14 | 0x1 << 13 | 0x1 << 5),
	CGATE(clk_hdmi, "clk_hdmi", "sclk_pixel", EXYNOS5_CLK_GATE_IP_DISP1, 6, 0, 0),
	CGATE(clk_smmufimd1x_m0, "clk_smmufimd1x_m0", "mout_aclk_300_disp1_user", EXYNOS5_CLK_GATE_IP_DISP1, 7, 0, 0),
	CGATE(clk_smmufimd1x_m1, "clk_smmufimd1x_m1", "mout_aclk_300_disp1_user", EXYNOS5_CLK_GATE_IP_DISP1, 8, 0, 0),
	CGATE(clk_smmutvx, "clk_smmutvx", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 9, 0, 0),
	CGATE(clk_ppmufimd1x, "clk_ppmufimd1x", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 17, 0, 0),
	CGATE(clk_ppmutvx, "clk_ppmutvx", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 18, 0, 0),
	CGATE(clk_asyncxim_gscl, "clk_asyncxim_gscl", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 19, 0, 0),
	CGATE(clk_asynctvx, "clk_asynctvx", "aclk_200_disp1", EXYNOS5_CLK_GATE_IP_DISP1, 20, 0, 0),

	CMGATE(clk_mfc_ip, "clk_mfc_ip", "aclk_333", EXYNOS5_CLK_GATE_IP_MFC, 0, CLK_GATE_MULTI_BIT_SET, 0, 0x3 << 3 | 0x1 << 0),
	CGATE(clk_smmumfcl, "clk_smmu_mfcl", "aclk_333", EXYNOS5_CLK_GATE_IP_MFC, 1, 0, 0),
	CGATE(clk_smmumfcr, "clk_smmu_mfcr", "aclk_333", EXYNOS5_CLK_GATE_IP_MFC, 2, 0, 0),
	CMGATE(clk_dummy1, "clk_dummy1", NULL, EXYNOS5_CLK_GATE_IP_MFC, 5, CLK_GATE_MULTI_BIT_SET, 0, 0x3 << 5),

	CMGATE(clk_g3d_ip, "clk_g3d_ip", "mout_aclk_g3d_user", EXYNOS5_CLK_GATE_IP_G3D, 9, CLK_GATE_MULTI_BIT_SET, 0, 0x3 << 8 | 0x1 << 1),
	CGATE(clk_ahb2apb_g3dp, "clk_ahb2apb_g3dp", "mout_aclk_g3d_user", EXYNOS5_CLK_GATE_IP_G3D, 8, 0, 0),

	CMGATE(clk_rotator, "clk_rotator", "aclk_gen", EXYNOS5_CLK_GATE_IP_GEN, 1, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 11 | 0x1 << 1),
	CMGATE(clk_jpeg, "clk_jpeg", "aclk_300_jpeg", EXYNOS5_CLK_GATE_IP_GEN, 2, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 12 | 0x1 << 2),
	CGATE(clk_jpeg2, "clk_jpeg2", "aclk_300_jpeg", EXYNOS5_CLK_GATE_IP_GEN, 3, 0, 0),
	CMGATE(clk_mdma1, "clk_mdma1", "aclk_gen", EXYNOS5_CLK_GATE_IP_GEN, 4, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 14 | 0x1 << 4),
	CGATE(clk_smmurotator, "clk_smmurotator", "aclk_gen", EXYNOS5_CLK_GATE_IP_GEN, 6, 0, 0),
	CGATE(clk_smmujpeg, "clk_smmujpeg", "aclk_300_jpeg", EXYNOS5_CLK_GATE_IP_GEN, 7, 0, 0),
	CGATE(clk_smmumdma1, "clk_smmumdma1", "aclk_gen", EXYNOS5_CLK_GATE_IP_GEN, 9, 0, 0),

	CMGATE(clk_mscl0, "clk_mscl0", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 0, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 5 | 0x1 <<0),
	CMGATE(clk_mscl1, "clk_mscl1", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 1, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 6 | 0x1 <<1),
	CMGATE(clk_mscl2, "clk_mscl2", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 2, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 7 | 0x1 <<2),
	CGATE(clk_mscl_ppmu01, "clk_mscl_ppmu01", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 3, 0, 0),
	CGATE(clk_mscl_ppmu2, "clk_mscl_ppmu2", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 4, 0, 0),
	CGATE(clk_smmu0, "clk_smmu0", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 8, 0, 0),
	CGATE(clk_smmu1, "clk_smmu1", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 9, 0, 0),
	CGATE(clk_smmu2, "clk_smmu2", "aclk_400_mscl", EXYNOS5_CLK_GATE_IP_MSCL, 10, 0, 0),

	/* BUS_CDREX */
	CGATE(clkm_phy0, "clkm_phy0", "dout_sclk_cdrex", EXYNOS5_CLK_GATE_BUS_CDREX, 0, 0, 0),
	CGATE(clkm_phy1, "clkm_phy1", "dout_sclk_cdrex", EXYNOS5_CLK_GATE_BUS_CDREX, 1, 0, 0),

	/* ASS */
	CGATE(gate_ass_niu_p, "ass_niu_p", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 11, 0, 0),
	CGATE(gate_ass_niu, "ass_niu", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 10, 0, 0),
	CGATE(gate_ass_adma, "ass_adma", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 9, 0, 0),
	CGATE(gate_ass_timer, "ass_timer", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 8, 0, 0),
	CGATE(gate_ass_uart, "ass_uart", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 7, 0, 0),
	CGATE(gate_ass_gpio, "ass_gpio", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 6, 0, 0),
	CGATE(gate_ass_pcm_special, "ass_pcm_special", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 5, 0, 0),
	CGATE(gate_ass_pcm_bus, "ass_pcm_bus", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 4, 0, 0),
	CGATE(gate_ass_i2s_special, "ass_i2s_special", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 3, 0, 0),
	CGATE(gate_ass_i2s_bus, "ass_i2s_bus", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 2, 0, 0),
	CGATE(gate_ass_srp, "ass_srp", "mout_ass_clk", EXYNOS_CLKGATE_AUDSS, 0, 0, 0),

	/* G2D IP */
	CMGATE(clk_g2d, "clk_g2d", "aclk_333_g2d", EXYNOS5_CLK_GATE_IP_G2D, 3, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 10 | 0x1 << 3),
	CMGATE(clk_slimsss, "clk_slimsss", "aclk_266_g2d", EXYNOS5_CLK_GATE_IP_G2D, 12, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 14 | 0x1 << 12),
	CMGATE(clk_sss, "clk_sss", "aclk_266_g2d", EXYNOS5_CLK_GATE_IP_G2D, 2, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 9 | 0x1 << 2),
	CGATE(clk_smmug2d, "clk_smmug2d", "aclk_333_g2d", EXYNOS5_CLK_GATE_IP_G2D, 7, 0, 0),
	CMGATE(clk_mdma, "clk_mdma", "aclk_333_g2d", EXYNOS5_CLK_GATE_IP_G2D, 1, CLK_GATE_MULTI_BIT_SET, 0, 0x1 << 8 | 0x1 << 1),
	CGATE(clk_smmumdma, "clk_smmumdma", "aclk_333_g2d", EXYNOS5_CLK_GATE_IP_G2D, 5, 0, 0),
	CGATE(clk_smmusss, "clk_smmusss", "aclk_266_g2d", EXYNOS5_CLK_GATE_IP_G2D, 6, 0, 0),
	CGATE(clk_smmuslimsss, "clk_smmuslimsss", "aclk_266_g2d", EXYNOS5_CLK_GATE_IP_G2D, 13, 0, 0),

	/* CLK_GATE_IP_PERIC */
	CGATE(clk_i2c10, "clk_i2c10", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 31, 0, 0),
	CGATE(clk_i2c9, "clk_i2c9", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 30, 0, 0),
	CGATE(clk_keyif, "clk_keyif", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 29, 0, 0),
	CGATE(clk_i2c8, "clk_i2c8", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 28, 0, 0),
	CGATE(clk_spdif, "clk_spdif", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 26, 0, 0),
	CGATE(clk_pwm, "clk_pwm", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 24, 0, 0),
	CGATE(clk_pcm2, "clk_pcm2", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 23, 0, 0),
	CGATE(clk_pcm1, "clk_pcm1", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 22, 0, 0),
	CGATE(clk_i2s2, "clk_i2s2", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 21, 0, 0),
	CGATE(clk_i2s1, "clk_i2s1", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 20, 0, 0),

	CGATE(clk_spi2, "clk_spi2", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 18, 0, 0),
	CGATE(clk_spi1, "clk_spi1", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 17, 0, 0),
	CGATE(clk_spi0, "clk_spi0", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 16, 0, 0),

	CGATE(clk_tsadc,"clk_tsadc","aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 15, 0, 0),
	CGATE(clk_i2chdmi, "clk_i2chdmi", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 14, 0, 0),
	CGATE(clk_i2c7, "clk_i2c7", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 13, 0, 0),
	CGATE(clk_i2c6, "clk_i2c6", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 12, 0, 0),
	CGATE(clk_i2c5, "clk_i2c5", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 11, 0, 0),
	CGATE(clk_i2c4, "clk_i2c4", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 10, 0, 0),

	CGATE(clk_i2c3, "clk_i2c3", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 9, 0, 0),
	CGATE(clk_i2c2, "clk_i2c2", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 8, 0, 0),
	CGATE(clk_i2c1, "clk_i2c1", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 7, 0, 0),
	CGATE(clk_i2c0, "clk_i2c0", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 6, 0, 0),
	CGATE(clk_uart4, "clk_uart4", "aclk_66_peric",  EXYNOS5_CLK_GATE_IP_PERIC, 4, 0, 0),
	CGATE(clk_uart3, "clk_uart3", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 3, 0, 0),
	CGATE(clk_uart2, "clk_uart2", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 2, 0, 0),
	CGATE(clk_uart1, "clk_uart1", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 1, 0, 0),
	CGATE(clk_uart0, "clk_uart0", "aclk_66_peric", EXYNOS5_CLK_GATE_IP_PERIC, 0, 0, 0),

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	CGATE(sclk_isp_sensor0, "sclk_isp_sensor0", "dout_isp_sensor0", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 4, 0, 0),
	CGATE(sclk_isp_sensor1, "sclk_isp_sensor1", "dout_isp_sensor1", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 8, 0, 0),
	CGATE(sclk_isp_sensor2, "sclk_isp_sensor2", "dout_isp_sensor2", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 12, 0, 0),

	CGATE(sclk_uart_isp, "sclk_uart_isp", "dout_uart_isp", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 0, 0, 0),
	CGATE(sclk_spi0_isp, "sclk_spi0_isp", "dout_spi0_isp_pre", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 1, 0, 0),
	CGATE(sclk_spi1_isp, "sclk_spi1_isp", "dout_spi1_isp_pre", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 2, 0, 0),
	CGATE(sclk_pwm_isp, "sclk_pwm_isp", "dout_pwm_isp", EXYNOS5_CLK_GATE_TOP_SCLK_ISP, 3, 0, 0),

	/* CLK_GATE_IP_CAM */
	CGATE(clk_3aa_2, "clk_3aa_2", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_CAM, 31, 0, 0),
	CGATE(clk_xiu_mi_gscl_cam, "clk_xiu_mi_gscl_cam", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_CAM, 30, 0, 0),
	CGATE(clk_xiu_si_gscl_cam, "clk_xiu_si_gscl_cam", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_CAM, 29, 0, 0),
	CGATE(clk_noc_p_rstop_fimcl, "clk_noc_p_rstop_fimcl", "aclk_333_432_gscl", EXYNOS5_CLK_GATE_IP_CAM, 28, 0, 0),
	CGATE(clk_camif_top_3aa0, "clk_camif_top_3aa0", "aclk_550_cam", EXYNOS5_CLK_GATE_IP_CAM, 27, 0, 0),
	CGATE(clk_smmu_3aa0, "clk_smmu_3aa0", "dout2_gscl_blk_333", EXYNOS5_CLK_GATE_IP_CAM, 25, 0, 0),
#endif
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos5422-oscclk", .data = (void *)0, },
	{ },
};

struct samsung_pll_rate_table cpll_rate_table[] = {
	/* rate		p	m	s	k */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ 666000000U,   4,  222,    1,  0},
#else
	{ 666000000U,   4,  222,    1,  0},
	{ 640000000U,   3,  160,    1,  0},
	{ 320000000U,   3,  160,    2,  0},
#endif
};

struct samsung_pll_rate_table epll_rate_table[] = {
	/* rate		p	m	s	k */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ 466000000U,   3,  233,    2,  0},
#else
	{ 600000000U,   2,  100,    1,  0},
	{ 400000000U,   3,  200,    2,  0},
	{ 200000000U,   3,  200,    3,  0},
	{ 180633600U,   5,  301,    3,  0},
	{ 100000000U,   3,  200,    4,  0},
	{  67737600U,   5,  452,    5,  0},
	{  49152000U,   3,  197,    5,  0},
	{  45158400U,   3,  181,    5,  0},
#endif
};

struct samsung_pll_rate_table ipll_rate_table[] = {
	/* rate		p	m	s	k */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ 432000000U,   4,  288,    2,  0},
#else
	{ 864000000U,	4,  288,    1,  0},
	{ 666000000U,	4,  222,    1,  0},
	{ 432000000U,   4,  288,    2,  0},
	{ 370000000U,   3,  185,    2,  0},
#endif
};

struct samsung_pll_rate_table vpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 600000000U,   2,  200,    2,  0},
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ 543000000U,   2,  181,    2,  0},
#else
	{ 533000000U,   6,  533,    2,  0},
#endif
	{ 480000000U,   2,  160,    2,  0},
	{ 420000000U,   2,  140,    2,  0},
	{ 350000000U,   3,  175,    2,  0},
	{ 266000000U,   3,  266,    3,  0},
	{ 177000000U,   2,  118,    3,  0},
	{ 100000000U,   3,  200,    4,  0},
};

struct samsung_pll_rate_table spll_rate_table[] = {
	/* rate		p	m	s	k */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ 800000000U,   3,  200,    1,  0},
#else
	{ 400000000U,   3,  200,    2,  0},
	{  66000000U,   4,  352,    5,  0},
#endif
};

struct samsung_pll_rate_table mpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 532000000U,   3,  266,    2,  0},
};


struct samsung_pll_rate_table rpll_rate_table[] = {
	/* rate		p	m	s	k */
	{ 300000000U,   2,  100,    2, 0},
	{ 266000000U,   3,  266,    3, 0},
};

struct samsung_pll_rate_table bpll_rate_table[] = {
	/* rate		p	m	s	k */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{ 925000000U,   4,  307,    1,  0},
	{ 825000000U,   4,  275,    1,  0},
	{ 728000000U,   3,  182,    1,  0},
	{ 633000000U,   4,  211,    1,  0},
	{ 543000000U,   2,  181,    2,  0},
	{ 413000000U,   6,  413,    2,  0},
	{ 275000000U,   3,  275,    3,  0},
	{ 206000000U,   3,  206,    3,  0},
	{ 165000000U,   2,  110,    3,  0},
	{ 138000000U,   2,  184,    4,  0},
#else
	{ 933000000U,   4,  311,    1,  0},
	{ 800000000U,   3,  200,    1,  0},
	{ 733000000U,   2,  122,    1,  0},
	{ 667000000U,   2,  111,    1,  0},
	{ 533000000U,   3,  266,    2,  0},
	{ 400000000U,   3,  200,    2,  0},
	{ 266000000U,   3,  266,    3,  0},
	{ 200000000U,   3,  200,    3,  0},
	{ 160000000U,   3,  160,    3,  0},
#endif
};

struct samsung_pll_rate_table apll_rate_table[] = {
	/* rate		p	m	s	k */
	{2400000000U,   2,  200,    0,  0},
	{2300000000U,   6,  575,    0,  0},
	{2200000000U,   3,  275,    0,  0},
	{2100000000U,   2,  175,    0,  0},
	{2000000000U,   3,  250,    0,  0},
	{1900000000U,   6,  475,    0,  0},
	{1800000000U,   3,  225,    0,  0},
	{1700000000U,   6,  425,    0,  0},
	{1600000000U,   3,  200,    0,  0},
	{1500000000U,   4,  250,    0,  0},
	{1400000000U,   3,  175,    0,  0},
	{1300000000U,   6,  325,    0,  0},
	{1200000000U,   2,  200,    1,  0},
	{1100000000U,   3,  275,    1,  0},
	{1000000000U,   3,  250,    1,  0},
	{ 900000000U,   2,  150,    1,  0},
	{ 800000000U,   3,  200,    1,  0},
	{ 700000000U,   3,  175,    1,  0},
	{ 600000000U,   2,  100,    1,  0},
	{ 500000000U,   3,  250,    2,  0},
	{ 400000000U,   3,  200,    2,  0},
	{ 300000000U,   2,  100,    2,  0},
	{ 200000000U,   3,  200,    3,  0},
};

struct samsung_pll_rate_table kpll_rate_table[] = {
	/* rate     p   m   s   k */
	{1600000000U,   3,  200,    0,  0},
	{1500000000U,   4,  250,    0,  0},
	{1400000000U,   3,  175,    0,  0},
	{1300000000U,   6,  325,    0,  0},
	{1200000000U,   2,  200,    1,  0},
	{1100000000U,   3,  275,    1,  0},
	{1000000000U,   3,  250,    1,  0},
	{ 900000000U,   2,  150,    1,  0},
	{ 800000000U,   3,  200,    1,  0},
	{ 700000000U,   3,  175,    1,  0},
	{ 600000000U,   2,  100,    1,  0},
	{ 500000000U,   3,  250,    2,  0},
	{ 400000000U,   3,  200,    2,  0},
	{ 300000000U,   2,  100,    2,  0},
	{ 200000000U,   3,  200,    3,  0},
};

struct samsung_pll_rate_table dpll_rate_table[] = {
	/* rate		p	m	s	k */
	{  66000000U,   4,  352,    5,  0},
};

#define EXYNOS5422_PRINT_CMU(name) \
	pr_info("  - %s  : 0x%x\n", \
			#name, \
			__raw_readl(EXYNOS5_##name))

void show_exynos_cmu(void)
{
	pr_info("\n");
	pr_info(" -----------------------------------------------------------------------------------\n");
	pr_info(" **** CMU register dump ****\n");
	EXYNOS5422_PRINT_CMU(APLL_CON0);
	EXYNOS5422_PRINT_CMU(KPLL_CON0);
	EXYNOS5422_PRINT_CMU(BPLL_CON0);
	EXYNOS5422_PRINT_CMU(CPLL_CON0);
	EXYNOS5422_PRINT_CMU(DPLL_CON0);
	EXYNOS5422_PRINT_CMU(IPLL_CON0);
	EXYNOS5422_PRINT_CMU(MPLL_CON0);
	EXYNOS5422_PRINT_CMU(SPLL_CON0);
	EXYNOS5422_PRINT_CMU(VPLL_CON0);
	EXYNOS5422_PRINT_CMU(EPLL_CON0);
	EXYNOS5422_PRINT_CMU(RPLL_CON0);
	pr_info(" -----------------------------------------------------------------------------------\n");
}

/* register exynos5422 clocks */
void __init exynos5422_clk_init(struct device_node *np)
{
	samsung_clk_init(np, 0, nr_clks, (unsigned long *) exynos5422_clk_regs,
			ARRAY_SIZE(exynos5422_clk_regs), NULL, 0);

	samsung_clk_of_register_fixed_ext(exynos5422_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5422_fixed_rate_ext_clks),
			ext_clk_match);
	samsung_clk_register_pll35xx("fout_apll", "fin_pll",
			EXYNOS5_APLL_LOCK, EXYNOS5_APLL_CON0, apll_rate_table, ARRAY_SIZE(apll_rate_table));

	samsung_clk_register_pll35xx("fout_bpll", "fin_pll",
			EXYNOS5_BPLL_LOCK, EXYNOS5_BPLL_CON0, bpll_rate_table, ARRAY_SIZE(bpll_rate_table));

	samsung_clk_register_pll35xx("fout_cpll", "fin_pll",
			EXYNOS5_CPLL_LOCK, EXYNOS5_CPLL_CON0, cpll_rate_table, ARRAY_SIZE(cpll_rate_table));

	samsung_clk_register_pll35xx("fout_dpll", "fin_pll",
			EXYNOS5_DPLL_LOCK, EXYNOS5_DPLL_CON0, dpll_rate_table, ARRAY_SIZE(dpll_rate_table));

	samsung_clk_register_pll35xx("fout_ipll", "fin_pll",
			EXYNOS5_IPLL_LOCK, EXYNOS5_IPLL_CON0, ipll_rate_table, ARRAY_SIZE(ipll_rate_table));

	samsung_clk_register_pll35xx("fout_kpll", "fin_pll",
			EXYNOS5_KPLL_LOCK, EXYNOS5_KPLL_CON0, kpll_rate_table, ARRAY_SIZE(kpll_rate_table));

	samsung_clk_register_pll35xx("fout_mpll", "fin_pll",
			EXYNOS5_MPLL_LOCK, EXYNOS5_MPLL_CON0, mpll_rate_table, ARRAY_SIZE(mpll_rate_table));

	samsung_clk_register_pll35xx("fout_spll", "fin_pll",
			EXYNOS5_SPLL_LOCK, EXYNOS5_SPLL_CON0, spll_rate_table, ARRAY_SIZE(spll_rate_table));

	samsung_clk_register_pll35xx("fout_vpll", "fin_pll",
			EXYNOS5_VPLL_LOCK, EXYNOS5_VPLL_CON0, vpll_rate_table, ARRAY_SIZE(vpll_rate_table));

	samsung_clk_register_pll36xx("fout_epll", "fin_pll",
			EXYNOS5_EPLL_LOCK, EXYNOS5_EPLL_CON0, epll_rate_table, ARRAY_SIZE(epll_rate_table));

	samsung_clk_register_pll36xx("fout_rpll", "fin_pll",
			EXYNOS5_RPLL_LOCK, EXYNOS5_RPLL_CON0, rpll_rate_table, ARRAY_SIZE(rpll_rate_table));

	samsung_clk_register_fixed_rate(exynos5422_fixed_rate_clks,
			ARRAY_SIZE(exynos5422_fixed_rate_clks));

	samsung_clk_register_fixed_factor(exynos5422_fixed_factor_clks,
			ARRAY_SIZE(exynos5422_fixed_factor_clks));

	samsung_clk_register_mux(exynos5422_mux_clks,
			ARRAY_SIZE(exynos5422_mux_clks));

	samsung_clk_register_div(exynos5422_div_clks,
			ARRAY_SIZE(exynos5422_div_clks));

	samsung_clk_register_gate(exynos5422_gate_clks,
			ARRAY_SIZE(exynos5422_gate_clks));

	exynos5422_clock_init();
	pr_info("Exynos5422: clock setup completed\n");
}
CLK_OF_DECLARE(exynos5422_clk, "samsung,exynos5422-clock", exynos5422_clk_init);
