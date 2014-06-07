/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>

#include <plat/devs.h>

#include <mach/map.h>
#include <mach/bts.h>
#include <mach/regs-bts.h>
#include <mach/regs-pmu.h>

enum bts_index {
	BTS_IDX_FIMD1M0 = 0,
	BTS_IDX_FIMD1M1,
	BTS_IDX_MIXER0,
	BTS_IDX_MIXER1,
	BTS_IDX_FIMC_LITE0,
	BTS_IDX_FIMC_LITE1,
	BTS_IDX_3AA_2,
	BTS_IDX_3AA,
	BTS_IDX_ROTATOR,
	BTS_IDX_SSS,
	BTS_IDX_SSSSLIM,
	BTS_IDX_G2D,
	BTS_IDX_EAGLE,
	BTS_IDX_KFC,
	BTS_IDX_MFC0,
	BTS_IDX_MFC1,
	BTS_IDX_G3D0,
	BTS_IDX_G3D1,
	BTS_IDX_MDMA0,
	BTS_IDX_MDMA1,
	BTS_IDX_JPEG0,
	BTS_IDX_JPEG2,
	BTS_IDX_USBDRD300,
	BTS_IDX_USBDRD301,
	BTS_IDX_MMC0,
	BTS_IDX_MMC1,
	BTS_IDX_MMC2,
	BTS_IDX_MSCL0,
	BTS_IDX_MSCL1,
	BTS_IDX_MSCL2,
	BTS_IDX_GSCL0,
	BTS_IDX_GSCL1,
};

enum bts_id {
	BTS_FIMD1M0 = (1 << BTS_IDX_FIMD1M0),
	BTS_FIMD1M1 = (1 << BTS_IDX_FIMD1M1),
	BTS_MIXER0 = (1 << BTS_IDX_MIXER0),
	BTS_MIXER1 = (1 << BTS_IDX_MIXER1),
	BTS_FIMC_LITE0 = (1 << BTS_IDX_FIMC_LITE0),
	BTS_FIMC_LITE1 = (1 << BTS_IDX_FIMC_LITE1),
	BTS_3AA_2 = (1 << BTS_IDX_3AA_2),
	BTS_3AA = (1 << BTS_IDX_3AA),
	BTS_ROTATOR = (1 << BTS_IDX_ROTATOR),
	BTS_SSS = (1 << BTS_IDX_SSS),
	BTS_SSSSLIM = (1 << BTS_IDX_SSSSLIM),
	BTS_G2D = (1 << BTS_IDX_G2D),
	BTS_EAGLE = (1 << BTS_IDX_EAGLE),
	BTS_KFC = (1 << BTS_IDX_KFC),
	BTS_MFC0 = (1 << BTS_IDX_MFC0),
	BTS_MFC1 = (1 << BTS_IDX_MFC1),
	BTS_G3D0 = (1 << BTS_IDX_G3D0),
	BTS_G3D1 = (1 << BTS_IDX_G3D1),
	BTS_MDMA0 = (1 << BTS_IDX_MDMA0),
	BTS_MDMA1 = (1 << BTS_IDX_MDMA1),
	BTS_JPEG0 = (1 << BTS_IDX_JPEG0),
	BTS_JPEG2 = (1 << BTS_IDX_JPEG2),
	BTS_USBDRD300 = (1 << BTS_IDX_USBDRD300),
	BTS_USBDRD301 = (1 << BTS_IDX_USBDRD301),
	BTS_MMC0 = (1 << BTS_IDX_MMC0),
	BTS_MMC1 = (1 << BTS_IDX_MMC1),
	BTS_MMC2 = (1 << BTS_IDX_MMC2),
	BTS_MSCL0 = (1 << BTS_IDX_MSCL0),
	BTS_MSCL1 = (1 << BTS_IDX_MSCL1),
	BTS_MSCL2 = (1 << BTS_IDX_MSCL2),
	BTS_GSCL0 = (1 << BTS_IDX_GSCL0),
	BTS_GSCL1 = (1 << BTS_IDX_GSCL1),
};

enum exynos_bts_scenario {
	BS_DEFAULT,
	BS_LAYER2_SCEN,
	BS_LAYER2_SCEN_DEFAULT,
	BS_MFC_UD_DECODING_ENABLE,
	BS_MFC_UD_DECODING_DISABLE,
	BS_MFC_UD_ENCODING_ENABLE,
	BS_MFC_UD_ENCODING_DISABLE,
	BS_G3D_MO,
	BS_G3D_DEFAULT,
	BS_FIMC_SCEN_ENABLE,
	BS_FIMC_SCEN_DISABLE,
	BS_DISABLE,
	BS_MAX,
};

enum bts_clock_index {
	BTS_CLOCK_G3D = 0,
	BTS_CLOCK_MAX,
};

struct bts_table {
	struct bts_set_table *table_list;
	unsigned int table_num;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

struct bts_ip_clk {
	const char *clk_name;
	struct clk *clk;
};

struct bts_scen_status {
	bool g3d_flag;
	unsigned int g3d_freq;
	unsigned int media_layers;
	bool layer_scen_flag;
	unsigned int ud_scen;
};

struct bts_info {
	enum bts_id id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table[BS_MAX];
	const char *pd_name;
	const char *clk_name;
	struct clk *clk;
	bool on;
	struct list_head list;
	struct list_head scen_list;
};

struct bts_scen_status pr_state = {
	.g3d_flag = false,
	.g3d_freq = 177,
	.media_layers = 1,
	.layer_scen_flag = false,
	.ud_scen = 0,
};

#define update_g3d_freq(a) (pr_state.g3d_freq = a)
#define update_g3d_flag(a) (pr_state.g3d_flag = a)
#define update_media_layers(a) (pr_state.media_layers = a)
#define update_layer_scen_flag(a) (pr_state.layer_scen_flag = a)
#define update_ud_scen(a) (pr_state.ud_scen = a)

#define G3D_177		(177)
#define LAYERS_2	(2)
#define QOS_0xC		0xc0
#define QOS_0xD		0xc8

#define BTS_CPU (BTS_KFC | BTS_EAGLE)
#define BTS_FIMD (BTS_FIMD1M0 | BTS_FIMD1M1)
#define BTS_MIXER (BTS_MIXER0 | BTS_MIXER1)
#define BTS_FIMC (BTS_FIMC_LITE0 | BTS_FIMC_LITE1 | BTS_3AA | BTS_3AA_2)
#define BTS_MDMA (BTS_MDMA0 | BTS_MDMA1)
#define BTS_MFC (BTS_MFC0 | BTS_MFC1)
#define BTS_G3D (BTS_G3D0 | BTS_G3D1)
#define BTS_JPEG (BTS_JPEG0 | BTS_JPEG2)
#define BTS_USB (BTS_USBDRD300 | BTS_USBDRD301)
#define BTS_MMC (BTS_MMC0 | BTS_MMC1 | BTS_MMC2)
#define BTS_MSCL (BTS_MSCL0 | BTS_MSCL1 | BTS_MSCL2)
#define BTS_GSCL (BTS_GSCL0 | BTS_GSCL1)

#define is_bts_scen_ip(a) (a & (BTS_FIMC | BTS_MFC | BTS_MIXER | BTS_G3D | BTS_FIMD))

#ifdef BTS_DBGGEN
#define BTS_DBG(x...) pr_err(x)
#else
#define BTS_DBG(x...) do {} while (0)
#endif

#ifdef BTS_DBGGEN1
#define BTS_DBG1(x...) pr_err(x)
#else
#define BTS_DBG1(x...) do {} while (0)
#endif

#define BTS_TABLE(num)					\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},			\
	{READ_TOKEN_MAX_VALUE, 0xffdf},			\
	{READ_BW_UPPER_BOUNDARY, 0x18},			\
	{READ_BW_LOWER_BOUNDARY, 0x1},			\
	{READ_INITIAL_TOKEN_VALUE, 0x8},		\
	{WRITE_CHANNEL_PRIORITY, num},			\
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},		\
	{WRITE_BW_UPPER_BOUNDARY, 0x18},		\
	{WRITE_BW_LOWER_BOUNDARY, 0x1},			\
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},		\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}

static struct bts_set_table fbm_l_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0x4444},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_DEMOTION_WINDOW, 0x7fff},
	{WRITE_DEMOTION_TOKEN, 0x1},
	{WRITE_DEFAULT_WINDOW, 0x7fff},
	{WRITE_DEFAULT_TOKEN, 0x1},
	{WRITE_PROMOTION_WINDOW, 0x7fff},
	{WRITE_PROMOTION_TOKEN, 0x1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

static struct bts_set_table g3d_fbm_l_r_high_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{WRITE_CHANNEL_PRIORITY, 0x4444},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_DEMOTION_WINDOW, 0x7fff},
	{WRITE_DEMOTION_TOKEN, 0x1},
	{WRITE_DEFAULT_WINDOW, 0x7fff},
	{WRITE_DEFAULT_TOKEN, 0x1},
	{WRITE_PROMOTION_WINDOW, 0x7fff},
	{WRITE_PROMOTION_TOKEN, 0x1},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{WRITE_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x1},
	{WRITE_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
	{WRITE_QOS_CONTROL, 0x7}
};

static struct bts_set_table g3d_read_ch_static_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x77},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, 0x8},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x2},
	{READ_QOS_CONTROL, 0x3},
};

static struct bts_set_table g3d_read_ch_fbm_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0x4444},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_DEMOTION_WINDOW, 0x7fff},
	{READ_DEMOTION_TOKEN, 0x1},
	{READ_DEFAULT_WINDOW, 0x7fff},
	{READ_DEFAULT_TOKEN, 0x1},
	{READ_PROMOTION_WINDOW, 0x7fff},
	{READ_PROMOTION_TOKEN, 0x1},
	{READ_ISSUE_CAPABILITY_UPPER_BOUNDARY, 0x1f},
	{READ_ISSUE_CAPABILITY_LOWER_BOUNDARY, 0x1f},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x3},
	{READ_FLEXIBLE_BLOCKING_POLARITY, 0x3},
	{READ_QOS_MODE, 0x1},
	{READ_QOS_CONTROL, 0x7},
};

static struct bts_set_table axiqes_mfc_un_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
	{READ_CHANNEL_PRIORITY, 0xdddd},
	{READ_TOKEN_MAX_VALUE, 0xffdf},
	{READ_BW_UPPER_BOUNDARY, 0x18},
	{READ_BW_LOWER_BOUNDARY, 0x1},
	{READ_INITIAL_TOKEN_VALUE, 0x8},
	{READ_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{WRITE_CHANNEL_PRIORITY, 0xcccc},
	{WRITE_TOKEN_MAX_VALUE, 0xffdf},
	{WRITE_BW_UPPER_BOUNDARY, 0x18},
	{WRITE_BW_LOWER_BOUNDARY, 0x1},
	{WRITE_INITIAL_TOKEN_VALUE, 0x8},
	{WRITE_FLEXIBLE_BLOCKING_CONTROL, 0x0},
	{READ_QOS_CONTROL, 0x1},
	{WRITE_QOS_CONTROL, 0x1},
};

BTS_TABLE(0x8888);
BTS_TABLE(0xbbbb);
BTS_TABLE(0xcccc);
BTS_TABLE(0xdddd);

static struct bts_ip_clk exynos5_bts_clk[] = {
	[BTS_CLOCK_G3D] = {
		.clk_name = "clk_ahb2apb_g3dp",
	},
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);
static LIST_HEAD(bts_scen_list);

static struct bts_info exynos5_bts[] = {
	[BTS_IDX_FIMD1M0] = {
		.id = BTS_FIMD1M0,
		.name = "fimd1m0",
		.pa_base = EXYNOS5_PA_BTS_DISP10,
		.pd_name = "spd-fimd",
		.clk_name = "clk_fimd1",
		.table[BS_DEFAULT].table_list = axiqos_0x8888_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_FIMC_SCEN_ENABLE].table_list = axiqos_0xdddd_table,
		.table[BS_FIMC_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_FIMC_SCEN_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_FIMC_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_MFC_UD_DECODING_ENABLE].table_list = axiqos_0x8888_table,
		.table[BS_MFC_UD_DECODING_ENABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_MFC_UD_DECODING_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_MFC_UD_DECODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqos_0xbbbb_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqos_0xbbbb_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = false,
	},
	[BTS_IDX_FIMD1M1] = {
		.id = BTS_FIMD1M1,
		.name = "fimd1m1",
		.pa_base = EXYNOS5_PA_BTS_DISP11,
		.pd_name = "spd-fimd",
		.clk_name = "clk_fimd1",
		.table[BS_DEFAULT].table_list = axiqos_0x8888_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_FIMC_SCEN_ENABLE].table_list = axiqos_0xdddd_table,
		.table[BS_FIMC_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_FIMC_SCEN_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_FIMC_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_MFC_UD_DECODING_ENABLE].table_list = axiqos_0x8888_table,
		.table[BS_MFC_UD_DECODING_ENABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_MFC_UD_DECODING_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_MFC_UD_DECODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqos_0xbbbb_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqos_0xbbbb_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = false,
	},
	[BTS_IDX_MIXER0] = {
		.id = BTS_MIXER0,
		.name = "mixer0",
		.pa_base = EXYNOS5_PA_BTS_MIXER0,
		.pd_name = "spd-mixer",
		.clk_name = "clk_mixer",
		.table[BS_DEFAULT].table_list = axiqos_0x8888_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_FIMC_SCEN_ENABLE].table_list = axiqos_0xdddd_table,
		.table[BS_FIMC_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_FIMC_SCEN_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_FIMC_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = false,
	},
	[BTS_IDX_MIXER1] = {
		.id = BTS_MIXER1,
		.name = "mixer1",
		.pa_base = EXYNOS5_PA_BTS_MIXER1,
		.pd_name = "spd-mixer",
		.clk_name = "clk_mixer",
		.table[BS_DEFAULT].table_list = axiqos_0x8888_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.table[BS_FIMC_SCEN_ENABLE].table_list = axiqos_0xdddd_table,
		.table[BS_FIMC_SCEN_ENABLE].table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.table[BS_FIMC_SCEN_DISABLE].table_list = axiqos_0x8888_table,
		.table[BS_FIMC_SCEN_DISABLE].table_num = ARRAY_SIZE(axiqos_0x8888_table),
		.on = false,
	},
	[BTS_IDX_FIMC_LITE0] = {
		.id = BTS_FIMC_LITE0,
		.name = "fimc_lite0",
		.pa_base = EXYNOS5_PA_BTS_FIMCLITE0,
		.pd_name = "pd-fimclite",
		.clk_name = "gscl_fimc_lite0",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_FIMC_LITE1] = {
		.id = BTS_FIMC_LITE1,
		.name = "fimc_lite1",
		.pa_base = EXYNOS5_PA_BTS_FIMCLITE1,
		.pd_name = "pd-fimclite",
		.clk_name = "gscl_fimc_lite1",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_3AA_2] = {
		.id = BTS_3AA_2,
		.name = "3aa_2",
		.pa_base = EXYNOS5_PA_BTS_3AA_2,
		.pd_name = "pd-fimclite",
		.clk_name = "clk_3aa_2",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_3AA] = {
		.id = BTS_3AA,
		.name = "3aa",
		.pa_base = EXYNOS5_PA_BTS_3AA,
		.pd_name = "pd-fimclite",
		.clk_name = "clk_3aa",
		.table[BS_DEFAULT].table_list = axiqos_0xcccc_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_ROTATOR] = {
		.id = BTS_ROTATOR,
		.name = "rotator",
		.pa_base = EXYNOS5_PA_BTS_ROTATOR,
		.pd_name = "DEFAULT",
		.clk_name = "clk_rotator",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_SSS] = {
		.id = BTS_SSS,
		.name = "sss",
		.pa_base = EXYNOS5_PA_BTS_SSS,
		.pd_name = "DEFAULT",
		.clk_name = "clk_sss",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_SSSSLIM] = {
		.id = BTS_SSSSLIM,
		.name = "sssslim",
		.pa_base = EXYNOS5_PA_BTS_SSSSLIM,
		.pd_name = "DEFAULT",
		.clk_name = "clk_slimsss",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_G2D] = {
		.id = BTS_G2D,
		.name = "g2d",
		.pa_base = EXYNOS5_PA_BTS_G2D,
		.pd_name = "DEFAULT",
		.clk_name = "clk_g2d",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_EAGLE] = {
		.id = BTS_EAGLE,
		.name = "eagle",
		.pa_base = EXYNOS5_PA_BTS_EAGLE,
		.pd_name = "pd-eagle",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_KFC] = {
		.id = BTS_KFC,
		.name = "kfc",
		.pa_base = EXYNOS5_PA_BTS_KFC,
		.pd_name = "pd-kfc",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MFC0] = {
		.id = BTS_MFC0,
		.name = "mfc0",
		.pa_base = EXYNOS5_PA_BTS_MFC0,
		.pd_name = "pd-mfc",
		.clk_name = "clk_mfc_ip",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.table[BS_MFC_UD_DECODING_ENABLE].table_list = axiqes_mfc_un_table,
		.table[BS_MFC_UD_DECODING_ENABLE].table_num = ARRAY_SIZE(axiqes_mfc_un_table),
		.table[BS_MFC_UD_DECODING_DISABLE].table_list = fbm_l_r_high_table,
		.table[BS_MFC_UD_DECODING_DISABLE].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqes_mfc_un_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqes_mfc_un_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = fbm_l_r_high_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MFC1] = {
		.id = BTS_MFC1,
		.name = "mfc1",
		.pa_base = EXYNOS5_PA_BTS_MFC1,
		.pd_name = "pd-mfc",
		.clk_name = "clk_mfc_ip",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.table[BS_MFC_UD_DECODING_ENABLE].table_list = axiqes_mfc_un_table,
		.table[BS_MFC_UD_DECODING_ENABLE].table_num = ARRAY_SIZE(axiqes_mfc_un_table),
		.table[BS_MFC_UD_DECODING_DISABLE].table_list = fbm_l_r_high_table,
		.table[BS_MFC_UD_DECODING_DISABLE].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.table[BS_MFC_UD_ENCODING_ENABLE].table_list = axiqes_mfc_un_table,
		.table[BS_MFC_UD_ENCODING_ENABLE].table_num = ARRAY_SIZE(axiqes_mfc_un_table),
		.table[BS_MFC_UD_ENCODING_DISABLE].table_list = fbm_l_r_high_table,
		.table[BS_MFC_UD_ENCODING_DISABLE].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_G3D0] = {
		.id = BTS_G3D0,
		.name = "g3d0",
		.pa_base = EXYNOS5_PA_BTS_G3D0,
		.pd_name = "pd-g3d",
		.clk_name = "clk_g3d_ip",
		.table[BS_DEFAULT].table_list = g3d_fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(g3d_fbm_l_r_high_table),
		.table[BS_G3D_MO].table_list = g3d_read_ch_static_table,
		.table[BS_G3D_MO].table_num = ARRAY_SIZE(g3d_read_ch_static_table),
		.table[BS_G3D_DEFAULT].table_list = g3d_read_ch_fbm_table,
		.table[BS_G3D_DEFAULT].table_num = ARRAY_SIZE(g3d_read_ch_fbm_table),
		.on = false,
	},
	[BTS_IDX_G3D1] = {
		.id = BTS_G3D1,
		.name = "g3d1",
		.pa_base = EXYNOS5_PA_BTS_G3D1,
		.pd_name = "pd-g3d",
		.clk_name = "clk_g3d_ip",
		.table[BS_DEFAULT].table_list = g3d_fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(g3d_fbm_l_r_high_table),
		.table[BS_G3D_MO].table_list = g3d_read_ch_static_table,
		.table[BS_G3D_MO].table_num = ARRAY_SIZE(g3d_read_ch_static_table),
		.table[BS_G3D_DEFAULT].table_list = g3d_read_ch_fbm_table,
		.table[BS_G3D_DEFAULT].table_num = ARRAY_SIZE(g3d_read_ch_fbm_table),
		.on = false,
	},
	[BTS_IDX_MDMA0] = {
		.id = BTS_MDMA0,
		.name = "mdma0",
		.pa_base = EXYNOS5_PA_BTS_MDMA,
		.pd_name = "DEFAULT",
		.clk_name = "clk_mdma",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MDMA1] = {
		.id = BTS_MDMA1,
		.name = "mdma1",
		.pa_base = EXYNOS5_PA_BTS_MDMA1,
		.pd_name = "DEFAULT",
		.clk_name = "clk_mdma1",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_JPEG0] = {
		.id = BTS_JPEG0,
		.name = "jpeg0",
		.pa_base = EXYNOS5_PA_BTS_JPEG,
		.pd_name = "spd-jpeg1",
		.clk_name = "clk_jpeg",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_JPEG2] = {
		.id = BTS_JPEG2,
		.name = "jpeg2",
		.pa_base = EXYNOS5_PA_BTS_JPEG2,
		.pd_name = "spd-jpeg2",
		.clk_name = "clk_jpeg2",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_USBDRD300] = {
		.id = BTS_USBDRD300,
		.name = "usbdrd300",
		.pa_base = EXYNOS5_PA_BTS_USBDRD300,
		.pd_name = "DEFAULT",
		.clk_name = "clk_usbdrd300",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_USBDRD301] = {
		.id = BTS_USBDRD301,
		.name = "usbdrd301",
		.pa_base = EXYNOS5_PA_BTS_USBDRD301,
		.pd_name = "DEFAULT",
		.clk_name = "clk_usbdrd301",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MMC0] = {
		.id = BTS_MMC0,
		.name = "mmc0",
		.pa_base = EXYNOS5_PA_BTS_MMC0,
		.pd_name = "DEFAULT",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MMC1] = {
		.id = BTS_MMC1,
		.name = "mmc1",
		.pa_base = EXYNOS5_PA_BTS_MMC1,
		.pd_name = "DEFAULT",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MMC2] = {
		.id = BTS_MMC2,
		.name = "mmc2",
		.pa_base = EXYNOS5_PA_BTS_MMC2,
		.pd_name = "DEFAULT",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MSCL0] = {
		.id = BTS_MSCL0,
		.name = "mscl0",
		.pa_base = EXYNOS5_PA_BTS_MSCL0,
		.pd_name = "pd-mscl",
		.clk_name = "clk_mscl0",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MSCL1] = {
		.id = BTS_MSCL1,
		.name = "mscl1",
		.pa_base = EXYNOS5_PA_BTS_MSCL1,
		.pd_name = "pd-mscl",
		.clk_name = "clk_mscl1",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_MSCL2] = {
		.id = BTS_MSCL2,
		.name = "mscl2",
		.pa_base = EXYNOS5_PA_BTS_MSCL2,
		.pd_name = "pd-mscl",
		.clk_name = "clk_mscl2",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_GSCL0] = {
		.id = BTS_GSCL0,
		.name = "gscl0",
		.pa_base = EXYNOS5_PA_BTS_GSCL0,
		.pd_name = "spd-gscl0",
		.clk_name = "clk_gscl0",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
	[BTS_IDX_GSCL1] = {
		.id = BTS_GSCL1,
		.name = "gscl1",
		.pa_base = EXYNOS5_PA_BTS_GSCL1,
		.pd_name = "spd-gscl1",
		.clk_name = "clk_gscl1",
		.table[BS_DEFAULT].table_list = fbm_l_r_high_table,
		.table[BS_DEFAULT].table_num = ARRAY_SIZE(fbm_l_r_high_table),
		.on = false,
	},
};

struct bts_scenario {
	const char *name;
	unsigned int ip;
	enum exynos_bts_scenario id;
};

static struct bts_scenario bts_scen[] = {
	[BS_DEFAULT] = {
		.name = "bts_default",
		.id = BS_DEFAULT,
	},
	[BS_LAYER2_SCEN] = {
		.name = "bts_layer2_scen",
		.id = BS_LAYER2_SCEN,
	},
	[BS_LAYER2_SCEN_DEFAULT] = {
		.name = "bts_layer2_scen_default",
		.id = BS_LAYER2_SCEN_DEFAULT,
	},
	[BS_MFC_UD_DECODING_ENABLE] = {
		.name = "bts_mfc_ud_decoding_enable",
		.ip = BTS_MFC | BTS_FIMD,
		.id = BS_MFC_UD_DECODING_ENABLE,
	},
	[BS_MFC_UD_DECODING_DISABLE] = {
		.name = "bts_mfc_ud_decoding_disable",
		.ip = BTS_MFC | BTS_FIMD,
		.id = BS_MFC_UD_DECODING_DISABLE,
	},
	[BS_MFC_UD_ENCODING_ENABLE] = {
		.name = "bts_mfc_ud_encoding_enabled",
		.ip = BTS_MFC | BTS_FIMD,
		.id = BS_MFC_UD_ENCODING_ENABLE,
	},
	[BS_MFC_UD_ENCODING_DISABLE] = {
		.name = "bts_mfc_ud_encoding_disable",
		.ip = BTS_MFC | BTS_FIMD,
		.id = BS_MFC_UD_ENCODING_DISABLE,
	},
	[BS_G3D_MO] = {
		.name = "bts_g3d_mo",
		.ip = BTS_G3D,
		.id = BS_G3D_MO,
	},
	[BS_G3D_DEFAULT] = {
		.name = "bts_g3d_default",
		.ip = BTS_G3D,
		.id = BS_G3D_DEFAULT,
	},
	[BS_FIMC_SCEN_ENABLE] = {
		.name = "bts_fimc_scen_enable",
		.ip = BTS_FIMD | BTS_MIXER,
		.id = BS_FIMC_SCEN_ENABLE,
	},
	[BS_FIMC_SCEN_DISABLE] = {
		.name = "bts_fimc_scen_disable",
		.ip = BTS_FIMD | BTS_MIXER,
		.id = BS_FIMC_SCEN_DISABLE,
	},
	[BS_MAX] = {
		.name = "undefined"
	}
};

static unsigned int read_clusterid(void)
{
	unsigned int mpidr;
	asm ("mrc\tp15, 0, %0, c0, c0, 5\n":"=r"(mpidr));
	return (mpidr >> 8) & 0xff;
}

static int exynos_bts_notifier_event(struct notifier_block *this,
					  unsigned long event,
					  void *ptr)
{
	unsigned int reg;
	switch (event) {
	case PM_POST_SUSPEND:
		if (!read_clusterid()) {
			bts_initialize("pd-eagle", true);

			reg = __raw_readl(S5P_VA_PMU);
			__raw_writel(reg | EXYNOS5422_ACE_KFC, EXYNOS5422_SFR_AXI_CGDIS1_REG);

			bts_initialize("pd-kfc", true);

			__raw_writel(reg, EXYNOS5422_SFR_AXI_CGDIS1_REG);

		} else {
			bts_initialize("pd-kfc", true);

			reg = __raw_readl(EXYNOS5422_SFR_AXI_CGDIS1_REG);
			__raw_writel(reg | EXYNOS5422_ACE_EAGLE, EXYNOS5422_SFR_AXI_CGDIS1_REG);

			bts_initialize("pd-eagle", true);

			__raw_writel(reg, EXYNOS5422_SFR_AXI_CGDIS1_REG);
		}
			bts_initialize("DEFAULT", true);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bts_notifier = {
	.notifier_call = exynos_bts_notifier_event,
};

static void set_bts_ip_table(enum exynos_bts_scenario scen,
		struct bts_info *bts)
{
	int i;
	struct bts_set_table *table = bts->table[scen].table_list;

	if (!bts->on) {
		BTS_DBG("[BTS] scen:%s, bts off: %s\n", bts_scen[scen].name, bts->name);
		return;
	}

	BTS_DBG("[BTS] scen:%s, bts set: %s\n", bts_scen[scen].name, bts->name);

	if ((bts->id & BTS_G3D) && exynos5_bts_clk[BTS_CLOCK_G3D].clk)
		clk_enable(exynos5_bts_clk[BTS_CLOCK_G3D].clk);

	if (bts->clk)
		clk_enable(bts->clk);

	for (i = 0; i < bts->table[scen].table_num; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	if ((bts->id & BTS_G3D) && exynos5_bts_clk[BTS_CLOCK_G3D].clk)
		clk_disable(exynos5_bts_clk[BTS_CLOCK_G3D].clk);

	if (bts->clk)
		clk_disable(bts->clk);
}

static void set_timeout_val(unsigned int offset, unsigned int val)
{
	BTS_DBG("[BTS] %s offset:0x%x, val:0x%x\n", __func__, offset, val);

	__raw_writel(val, S5P_VA_DREXI_0 + offset);
	__raw_writel(val, S5P_VA_DREXI_1 + offset);
}

static void set_bts_scenario(enum exynos_bts_scenario scen)
{
	struct bts_info *bts;

	if (scen == BS_DEFAULT)
		return;

	list_for_each_entry(bts, &bts_scen_list, scen_list)
		if (bts->id & bts_scen[scen].ip)
			set_bts_ip_table(scen, bts);

	switch (scen) {
	case BS_LAYER2_SCEN:
		set_timeout_val(QOS_0xC, 0x00000020);
		break;
	case BS_LAYER2_SCEN_DEFAULT:
		set_timeout_val(QOS_0xC, 0x00000080);
		break;
	case BS_MFC_UD_DECODING_ENABLE:
		set_timeout_val(QOS_0xC, 0x00000080);
		set_timeout_val(QOS_0xD, 0x00000040);
		break;
	case BS_MFC_UD_DECODING_DISABLE:
		set_timeout_val(QOS_0xC, 0x00000080);
		set_timeout_val(QOS_0xD, 0x00000fff);
		break;
	case BS_MFC_UD_ENCODING_ENABLE:
		set_timeout_val(QOS_0xC, 0x00000fff);
		set_timeout_val(QOS_0xD, 0x00000032);
		break;
	case BS_MFC_UD_ENCODING_DISABLE:
		set_timeout_val(QOS_0xC, 0x00000080);
		set_timeout_val(QOS_0xD, 0x00000fff);
		break;
	default:
		break;
	}
}

void bts_scen_update(enum bts_scen_type type, unsigned int val)
{
	enum exynos_bts_scenario scen = BS_DEFAULT;

	spin_lock(&bts_lock);

	switch (type) {
	case TYPE_LAYERS:
		if (!exynos5_bts[BTS_IDX_MIXER0].on)
			break;

		if (exynos5_bts[BTS_IDX_FIMC_LITE0].on &&
			exynos5_bts[BTS_IDX_FIMD1M0].on) {
			BTS_DBG("[BTS] LAYERS: %u\n", val);
			if (!pr_state.layer_scen_flag && (val <= LAYERS_2)) {
				update_layer_scen_flag(true);
				scen = BS_LAYER2_SCEN;
			} else if (pr_state.layer_scen_flag) {
				update_layer_scen_flag(false);
				scen = BS_LAYER2_SCEN_DEFAULT;
			}
		} else if (pr_state.layer_scen_flag) {
			update_layer_scen_flag(false);
			scen = BS_LAYER2_SCEN_DEFAULT;
		}
		update_media_layers(val);
		break;
	case TYPE_MFC_UD_DECODING:
		BTS_DBG("[BTS] MFC_UD_DECODING: %u\n", val);
		if (val) {
			update_ud_scen(pr_state.ud_scen | BS_MFC_UD_DECODING_ENABLE);
			scen = BS_MFC_UD_DECODING_ENABLE;
		} else {
			update_ud_scen(pr_state.ud_scen & ~BS_MFC_UD_DECODING_ENABLE);
			if (exynos5_bts[BTS_IDX_FIMC_LITE0].on && !pr_state.ud_scen)
				scen = BS_FIMC_SCEN_ENABLE;
			else
				scen = BS_MFC_UD_DECODING_DISABLE;
		}
		break;

	case TYPE_MFC_UD_ENCODING:
		BTS_DBG("[BTS] MFC_UD_ENCODING: %u\n", val);
		if (val) {
			update_ud_scen(pr_state.ud_scen | BS_MFC_UD_ENCODING_ENABLE);
			scen = BS_MFC_UD_ENCODING_ENABLE;
		} else {
			update_ud_scen(pr_state.ud_scen & ~BS_MFC_UD_ENCODING_ENABLE);
			if (exynos5_bts[BTS_IDX_FIMC_LITE0].on && !pr_state.ud_scen)
				scen = BS_FIMC_SCEN_ENABLE;
			else
				scen = BS_MFC_UD_ENCODING_DISABLE;
		}

		break;

	case TYPE_G3D_FREQ:
		if (!exynos5_bts[BTS_IDX_G3D0].on)
			break;

		if (exynos5_bts[BTS_IDX_FIMC_LITE0].on) {
			BTS_DBG("[BTS] G3D freq: %u\n", val);
			if (!pr_state.g3d_flag && (val <= G3D_177)) {
				update_g3d_flag(true);
				scen = BS_G3D_MO;
			} else if (pr_state.g3d_flag && (val > G3D_177)) {
				update_g3d_flag(false);
				scen = BS_G3D_DEFAULT;
			}
		} else if (pr_state.g3d_flag) {
			update_g3d_flag(false);
			scen = BS_G3D_DEFAULT;
		}
		update_g3d_freq(val);
		break;

	default:
		break;
	}
	set_bts_scenario(scen);

	spin_unlock(&bts_lock);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;
	bool fimc_state = false;
	bool fimc_flag = false;
	bool g3d_state = false;
	bool mixer_state = false;
	enum exynos_bts_scenario scen = BS_DEFAULT;

	spin_lock(&bts_lock);

	BTS_DBG("[BTS][%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			bts->on = on;
			BTS_DBG("[BTS] %s on/off:%d\n", bts->name, bts->on);

			if (bts->id & BTS_FIMC) {
				fimc_state = on;
				fimc_flag = true;
			} else if (bts->id & BTS_G3D) {
				g3d_state = on;
			} else if (bts->id & BTS_MIXER) {
				mixer_state = on;
			}

			if (on)
				set_bts_ip_table(scen, bts);
		}

	if (fimc_flag && exynos5_bts[BTS_IDX_G3D0].on) {
		if (pr_state.g3d_freq <= G3D_177) {
			update_g3d_flag(fimc_state);
			scen = fimc_state ? BS_G3D_MO : BS_G3D_DEFAULT;
		}
	} else if (g3d_state && exynos5_bts[BTS_IDX_FIMC_LITE0].on) {
		update_g3d_freq(177);
		if (pr_state.g3d_freq <= G3D_177) {
			update_g3d_flag(true);
			scen = BS_G3D_MO;
		}
	}
	set_bts_scenario(scen);

	scen = BS_DEFAULT;
	if (fimc_flag && !pr_state.ud_scen) {
		pr_err("[BTS] ud_scen: %u \n", pr_state.ud_scen);
		if (fimc_state)
			scen = BS_FIMC_SCEN_ENABLE;
		else
			scen = BS_FIMC_SCEN_DISABLE;
	}
	set_bts_scenario(scen);

	scen = BS_DEFAULT;
	if (fimc_flag && exynos5_bts[BTS_IDX_FIMD1M0].on &&
			exynos5_bts[BTS_IDX_MIXER0].on) {
		if (pr_state.media_layers <= LAYERS_2) {
			update_layer_scen_flag(fimc_state);
			scen = fimc_state ? BS_LAYER2_SCEN : BS_LAYER2_SCEN_DEFAULT;
		}
	} else if (mixer_state && exynos5_bts[BTS_IDX_FIMD1M0].on &&
			exynos5_bts[BTS_IDX_MIXER0].on) {
		update_layer_scen_flag(false);
		if (pr_state.media_layers <= LAYERS_2) {
			update_layer_scen_flag(true);
			scen = BS_LAYER2_SCEN;
		}
	}
	set_bts_scenario(scen);

	spin_unlock(&bts_lock);
}

static void bts_drex_init(void)
{
	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, S5P_VA_DREXI_0 + 0x00D8);
	__raw_writel(0x00000080, S5P_VA_DREXI_0 + 0x00C0);
	__raw_writel(0x00000FFF, S5P_VA_DREXI_0 + 0x00C8);
	__raw_writel(0x00000FFF, S5P_VA_DREXI_0 + 0x00A0);
	__raw_writel(0x00000000, S5P_VA_DREXI_0 + 0x0100);
	__raw_writel(0x88588858, S5P_VA_DREXI_0 + 0x0104);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0214);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0224);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0234);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0244);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0218);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0228);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0238);
	__raw_writel(0x01000604, S5P_VA_DREXI_0 + 0x0248);
	__raw_writel(0x00000000, S5P_VA_DREXI_0 + 0x0210);
	__raw_writel(0x00000000, S5P_VA_DREXI_0 + 0x0220);
	__raw_writel(0x00000000, S5P_VA_DREXI_0 + 0x0230);
	__raw_writel(0x00000000, S5P_VA_DREXI_0 + 0x0240);

	__raw_writel(0x00000000, S5P_VA_DREXI_1 + 0x00D8);
	__raw_writel(0x00000080, S5P_VA_DREXI_1 + 0x00C0);
	__raw_writel(0x00000FFF, S5P_VA_DREXI_1 + 0x00C8);
	__raw_writel(0x00000FFF, S5P_VA_DREXI_1 + 0x00A0);
	__raw_writel(0x00000000, S5P_VA_DREXI_1 + 0x0100);
	__raw_writel(0x88588858, S5P_VA_DREXI_1 + 0x0104);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0214);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0224);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0234);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0244);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0218);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0228);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0238);
	__raw_writel(0x01000604, S5P_VA_DREXI_1 + 0x0248);
	__raw_writel(0x00000000, S5P_VA_DREXI_1 + 0x0210);
	__raw_writel(0x00000000, S5P_VA_DREXI_1 + 0x0220);
	__raw_writel(0x00000000, S5P_VA_DREXI_1 + 0x0230);
	__raw_writel(0x00000000, S5P_VA_DREXI_1 + 0x0240);
}

static int __init exynos5_bts_init(void)
{
	int i;
	struct clk *clk;

	BTS_DBG("[BTS][%s] bts init\n", __func__);

	for (i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		exynos5_bts[i].va_base
			= ioremap(exynos5_bts[i].pa_base, SZ_4K);

		if (exynos5_bts[i].clk_name) {
			clk = __clk_lookup(exynos5_bts[i].clk_name);

			if (IS_ERR(clk)) {
				pr_err("failed to get bts clk %s\n",
						exynos5_bts[i].clk_name);
			} else {
				clk_prepare(clk);
				exynos5_bts[i].clk = clk;
			}
		}

		list_add(&exynos5_bts[i].list, &bts_list);
		if (is_bts_scen_ip(exynos5_bts[i].id))
		        list_add(&exynos5_bts[i].scen_list, &bts_scen_list);
	}

	for (i = 0; i < ARRAY_SIZE(exynos5_bts_clk); i++) {
		clk = __clk_lookup(exynos5_bts_clk[i].clk_name);
		if (IS_ERR(clk)) {
			pr_err("failed to get bts clk %s\n",
					exynos5_bts_clk[i].clk_name);
		} else {
			clk_prepare(clk);
			exynos5_bts_clk[i].clk = clk;
		}
	}

	bts_drex_init();

	bts_initialize("pd-eagle", true);
	bts_initialize("pd-kfc", true);
	bts_initialize("DEFAULT", true);

	register_pm_notifier(&exynos_bts_notifier);

	return 0;
}
arch_initcall(exynos5_bts_init);
