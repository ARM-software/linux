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

#define EXYNOS5430_PA_DREX0		0x10400000
#define EXYNOS5430_PA_DREX1		0x10440000

void __iomem *drex0_va_base;
void __iomem *drex1_va_base;

enum bts_index {
	BTS_IDX_DECONM0 = 0,
	BTS_IDX_DECONM1,
	BTS_IDX_DECONM2,
	BTS_IDX_DECONM3,
	BTS_IDX_DECONM4,
	BTS_IDX_DECONTV_M0,
	BTS_IDX_DECONTV_M1,
	BTS_IDX_DECONTV_M2,
	BTS_IDX_DECONTV_M3,
	BTS_IDX_FIMCLITE0,
	BTS_IDX_FIMCLITE1,
	BTS_IDX_FIMCLITE2,
	BTS_IDX_FIMCLITE3,
	BTS_IDX_3AA0,
	BTS_IDX_3AA1,
	BTS_IDX_GSCL0,
	BTS_IDX_GSCL1,
	BTS_IDX_GSCL2,
};

enum bts_id {
	BTS_DECONM0 = (1 << BTS_IDX_DECONM0),
	BTS_DECONM1 = (1 << BTS_IDX_DECONM1),
	BTS_DECONM2 = (1 << BTS_IDX_DECONM2),
	BTS_DECONM3 = (1 << BTS_IDX_DECONM3),
	BTS_DECONM4 = (1 << BTS_IDX_DECONM4),
	BTS_DECONTV_M0 = (1 << BTS_IDX_DECONTV_M0),
	BTS_DECONTV_M1 = (1 << BTS_IDX_DECONTV_M1),
	BTS_DECONTV_M2 = (1 << BTS_IDX_DECONTV_M2),
	BTS_DECONTV_M3 = (1 << BTS_IDX_DECONTV_M3),
	BTS_FIMCLITE0 = (1 << BTS_IDX_FIMCLITE0),
	BTS_FIMCLITE1 = (1 << BTS_IDX_FIMCLITE1),
	BTS_FIMCLITE2 = (1 << BTS_IDX_FIMCLITE2),
	BTS_FIMCLITE3 = (1 << BTS_IDX_FIMCLITE3),
	BTS_3AA0 = (1 << BTS_IDX_3AA0),
	BTS_3AA1 = (1 << BTS_IDX_3AA1),
	BTS_GSCL0 = (1 << BTS_IDX_GSCL0),
	BTS_GSCL1 = (1 << BTS_IDX_GSCL1),
	BTS_GSCL2 = (1 << BTS_IDX_GSCL2),
};

struct bts_table {
	struct bts_set_table *table_list;
	unsigned int table_num;
};

struct bts_info {
	enum bts_id id;
	const char *name;
	unsigned int pa_base;
	void __iomem *va_base;
	struct bts_table table;
	const char *pd_name;
	const char *devname;
	const char *clk_name;
	struct clk *clk;
	bool on;
	struct list_head list;
	struct list_head scen_list;
};

struct bts_set_table {
	unsigned int reg;
	unsigned int val;
};

#define BTS_TABLE(num)					\
static struct bts_set_table axiqos_##num##_table[] = {	\
	{READ_QOS_CONTROL, 0x0},			\
	{WRITE_QOS_CONTROL, 0x0},			\
	{READ_CHANNEL_PRIORITY, num},			\
	{WRITE_CHANNEL_PRIORITY, num},			\
	{READ_QOS_CONTROL, 0x1},			\
	{WRITE_QOS_CONTROL, 0x1}			\
}

BTS_TABLE(0xcccc);
BTS_TABLE(0xdddd);
BTS_TABLE(0xeeee);
BTS_TABLE(0xffff);

static struct bts_set_table disable_table[] = {
	{READ_QOS_CONTROL, 0x0},
	{WRITE_QOS_CONTROL, 0x0},
};

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

static struct bts_info exynos5_bts[] = {
	[BTS_IDX_DECONM0] = {
		.id = BTS_DECONM0,
		.name = "decon0",
		.pa_base = EXYNOS5430_PA_BTS_DECONM0,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm0",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = false,
	},
	[BTS_IDX_DECONM1] = {
		.id = BTS_DECONM1,
		.name = "decon1",
		.pa_base = EXYNOS5430_PA_BTS_DECONM1,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm1",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = false,
	},
	[BTS_IDX_DECONM2] = {
		.id = BTS_DECONM2,
		.name = "decon2",
		.pa_base = EXYNOS5430_PA_BTS_DECONM2,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm2",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = false,
	},
	[BTS_IDX_DECONM3] = {
		.id = BTS_DECONM3,
		.name = "decon3",
		.pa_base = EXYNOS5430_PA_BTS_DECONM3,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm3",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = false,
	},
	[BTS_IDX_DECONM4] = {
		.id = BTS_DECONM4,
		.name = "decon4",
		.pa_base = EXYNOS5430_PA_BTS_DECONM4,
		.pd_name = "spd-decon",
		.clk_name = "gate_bts_deconm4",
		.table.table_list = axiqos_0xdddd_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xdddd_table),
		.on = false,
	},
	[BTS_IDX_DECONTV_M0] = {
		.id = BTS_DECONTV_M0,
		.name = "decontv_m0",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M0,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m0",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_DECONTV_M1] = {
		.id = BTS_DECONTV_M1,
		.name = "decontv_m1",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M1,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m1",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_DECONTV_M2] = {
		.id = BTS_DECONTV_M2,
		.name = "decontv_m2",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M2,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m2",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_DECONTV_M3] = {
		.id = BTS_DECONTV_M3,
		.name = "decontv_m3",
		.pa_base = EXYNOS5430_PA_BTS_DECONTV_M3,
		.pd_name = "spd-decon-tv",
		.clk_name = "gate_bts_decontv_m3",
		.table.table_list = axiqos_0xcccc_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xcccc_table),
		.on = false,
	},
	[BTS_IDX_FIMCLITE0] = {
		.id = BTS_FIMCLITE0,
		.name = "fimclite0",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE0,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_lite_a",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = false,
	},
	[BTS_IDX_FIMCLITE1] = {
		.id = BTS_FIMCLITE1,
		.name = "fimclite1",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE1,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_lite_b",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = false,
	},
	[BTS_IDX_FIMCLITE2] = {
		.id = BTS_FIMCLITE2,
		.name = "fimclite2",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE2,
		.pd_name = "pd-cam1",
		.clk_name = "gate_bts_lite_c",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = false,
	},
	[BTS_IDX_FIMCLITE3] = {
		.id = BTS_FIMCLITE3,
		.name = "fimclite3",
		.pa_base = EXYNOS5430_PA_BTS_FIMC_LITE3,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_lite_d",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = false,
	},
	[BTS_IDX_3AA0] = {
		.id = BTS_3AA0,
		.name = "3aa0",
		.pa_base = EXYNOS5430_PA_BTS_3AA0,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_3aa0",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = false,
	},
	[BTS_IDX_3AA1] = {
		.id = BTS_3AA1,
		.name = "3aa1",
		.pa_base = EXYNOS5430_PA_BTS_3AA1,
		.pd_name = "pd-cam0",
		.clk_name = "gate_bts_3aa1",
		.table.table_list = axiqos_0xeeee_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xeeee_table),
		.on = false,
	},
	[BTS_IDX_GSCL0] = {
		.id = BTS_GSCL0,
		.name = "gscl",
		.pa_base = EXYNOS5430_PA_BTS_GSCL0,
		.pd_name = "gscl-local",
		.clk_name = "gate_bts_gscl0",
		.table.table_list = axiqos_0xffff_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xffff_table),
		.on = false,
	},
	[BTS_IDX_GSCL1] = {
		.id = BTS_GSCL1,
		.name = "gscl1",
		.pa_base = EXYNOS5430_PA_BTS_GSCL1,
		.pd_name = "gscl-local",
		.clk_name = "gate_bts_gscl1",
		.table.table_list = axiqos_0xffff_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xffff_table),
		.on = false,
	},
	[BTS_IDX_GSCL2] = {
		.id = BTS_GSCL2,
		.name = "gscl2",
		.pa_base = EXYNOS5430_PA_BTS_GSCL2,
		.pd_name = "gscl-local",
		.clk_name = "gate_bts_gscl2",
		.table.table_list = axiqos_0xffff_table,
		.table.table_num = ARRAY_SIZE(axiqos_0xffff_table),
		.on = false,
	},
};

static DEFINE_SPINLOCK(bts_lock);
static LIST_HEAD(bts_list);

static void set_bts_ip_table(struct bts_info *bts, bool on)
{
	int i;
	struct bts_set_table *table = bts->table.table_list;
	unsigned int table_num;

	if (on) {
		table = bts->table.table_list;
		table_num = bts->table.table_num;
	} else {
		table = disable_table;
		table_num = ARRAY_SIZE(disable_table);
	}

	BTS_DBG("[BTS] bts set: %s, on/off: %d\n", bts->name, on);

	if (on && bts->clk)
		clk_enable(bts->clk);

	for (i = 0; i < table_num; i++) {
		__raw_writel(table->val, bts->va_base + table->reg);
		BTS_DBG1("[BTS] %x-%x\n", table->reg, table->val);
		table++;
	}

	if (!on && bts->clk)
		clk_disable(bts->clk);
}

void bts_initialize(const char *pd_name, bool on)
{
	struct bts_info *bts;

	spin_lock(&bts_lock);

	BTS_DBG("[%s] pd_name: %s, on/off:%x\n", __func__, pd_name, on);
	list_for_each_entry(bts, &bts_list, list)
		if (pd_name && bts->pd_name && !strcmp(bts->pd_name, pd_name)) {
			BTS_DBG("[BTS] %s on/off:%d\n", bts->name, bts->on);
			bts->on = on;

			set_bts_ip_table(bts, on);
		}
	spin_unlock(&bts_lock);
}

static int exynos5_bts_status_open_show(struct seq_file *buf, void *d)
{
	unsigned int i;
	unsigned int val_r, val_w;

	for(i = 0; i < ARRAY_SIZE(exynos5_bts); i++) {
		if (exynos5_bts[i].on) {
			if (exynos5_bts[i].clk) {
				clk_prepare(exynos5_bts[i].clk);
				clk_enable(exynos5_bts[i].clk);
			}
			val_r = __raw_readl(exynos5_bts[i].va_base + READ_QOS_CONTROL);
			val_w = __raw_readl(exynos5_bts[i].va_base + WRITE_QOS_CONTROL);
			if (val_r && val_w) {
				val_r = __raw_readl(exynos5_bts[i].va_base + READ_CHANNEL_PRIORITY);
				val_w = __raw_readl(exynos5_bts[i].va_base + WRITE_CHANNEL_PRIORITY);
				seq_printf(buf, "%s on, priority ch_r:0x%x,ch_w:0x%x\n",
						exynos5_bts[i].name, val_r, val_w);
			} else {
				seq_printf(buf, "%s control disable\n",
						exynos5_bts[i].name);
			}
			if (exynos5_bts[i].clk) {
				clk_disable(exynos5_bts[i].clk);
				clk_unprepare(exynos5_bts[i].clk);
			}
		} else {
			seq_printf(buf, "%s off\n", exynos5_bts[i].name);
		}
	}

	return 0;
}

static int exynos5_bts_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos5_bts_status_open_show, inode->i_private);
}

static const struct file_operations debug_status_fops = {
	.open		= exynos5_bts_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int debug_deconm_set(void *data, u64 val)
{
	bool on = exynos5_bts[BTS_IDX_DECONM0].on;

	spin_lock(&bts_lock);

	exynos5_bts[BTS_IDX_DECONM0].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONM0].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONM1].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONM1].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONM2].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONM2].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONM3].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONM3].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONM4].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONM4].table.table_list[3].val = val;

	if (on) {
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONM0], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONM1], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONM2], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONM3], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONM4], on);
	} else {
		pr_info("[BTS]DECONEM0 is power off\n");
	}

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_deconm_get(void *data, u64 *val)
{
	*val = exynos5_bts[BTS_IDX_DECONM0].on;
	return 0;
}

static int debug_decontv_set(void *data, u64 val)
{
	bool on = exynos5_bts[BTS_IDX_DECONTV_M0].on;

	spin_lock(&bts_lock);

	exynos5_bts[BTS_IDX_DECONTV_M0].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M0].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M1].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M1].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M2].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M2].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M3].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_DECONTV_M3].table.table_list[3].val = val;

	if (on) {
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONTV_M0], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONTV_M1], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONTV_M2], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_DECONTV_M3], on);
	} else {
		pr_info("[BTS]DECONETV is power off\n");
	}

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_decontv_get(void *data, u64 *val)
{
	*val = exynos5_bts[BTS_IDX_DECONTV_M0].on;
	return 0;
}

static int debug_cam0_set(void *data, u64 val)
{
	bool on = exynos5_bts[BTS_IDX_FIMCLITE0].on;

	spin_lock(&bts_lock);

	exynos5_bts[BTS_IDX_FIMCLITE0].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_FIMCLITE0].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_FIMCLITE1].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_FIMCLITE1].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_FIMCLITE3].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_FIMCLITE3].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_3AA0].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_3AA1].table.table_list[3].val = val;

	if (on) {
		set_bts_ip_table(&exynos5_bts[BTS_IDX_FIMCLITE0], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_FIMCLITE1], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_FIMCLITE3], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_3AA0], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_3AA1], on);
	} else {
		pr_info("[BTS]CAM0 is power off\n");
	}

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_cam0_get(void *data, u64 *val)
{
	*val = exynos5_bts[BTS_IDX_FIMCLITE0].on;
	return 0;
}

static int debug_cam1_set(void *data, u64 val)
{
	bool on = exynos5_bts[BTS_IDX_FIMCLITE0].on;

	spin_lock(&bts_lock);

	exynos5_bts[BTS_IDX_FIMCLITE2].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_FIMCLITE2].table.table_list[3].val = val;

	if (on)
		set_bts_ip_table(&exynos5_bts[BTS_IDX_FIMCLITE2], on);
	else
		pr_info("[BTS]CAM1 is power off\n");

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_cam1_get(void *data, u64 *val)
{
	*val = exynos5_bts[BTS_IDX_FIMCLITE2].on;
	return 0;
}

static int debug_gscl_local_set(void *data, u64 val)
{
	bool on = exynos5_bts[BTS_IDX_GSCL0].on;

	spin_lock(&bts_lock);

	exynos5_bts[BTS_IDX_GSCL0].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_GSCL0].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_GSCL1].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_GSCL1].table.table_list[3].val = val;
	exynos5_bts[BTS_IDX_GSCL2].table.table_list[2].val = val;
	exynos5_bts[BTS_IDX_GSCL2].table.table_list[3].val = val;

	if (on) {
		set_bts_ip_table(&exynos5_bts[BTS_IDX_GSCL0], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_GSCL1], on);
		set_bts_ip_table(&exynos5_bts[BTS_IDX_GSCL2], on);
	} else {
		pr_info("[BTS]GSCL LOCAL PATH disable \n");
	}

	spin_unlock(&bts_lock);
	return 0;
}

static int debug_gscl_local_get(void *data, u64 *val)
{
	*val = exynos5_bts[BTS_IDX_GSCL0].on;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_deconm_fops, debug_deconm_get, debug_deconm_set, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_tv_fops, debug_decontv_get, debug_decontv_set, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_cam0_fops, debug_cam0_get, debug_cam0_set, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_cam1_fops, debug_cam1_get, debug_cam1_set, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_gscl_local_fops, debug_gscl_local_get, debug_gscl_local_set, "%llx\n");

void bts_debugfs(void)
{
	struct dentry *den;

	den = debugfs_create_dir("bts", NULL);
	debugfs_create_file("bts_status", 0444,
			den, NULL, &debug_status_fops);
	debugfs_create_file("deconm", 0644,
			den, NULL, &debug_deconm_fops);
	debugfs_create_file("tv", 0644,
			den, NULL, &debug_tv_fops);
	debugfs_create_file("cam0", 0644,
			den, NULL, &debug_cam0_fops);
	debugfs_create_file("cam1", 0644,
			den, NULL, &debug_cam1_fops);
	debugfs_create_file("gscl-local", 0644,
			den, NULL, &debug_gscl_local_fops);
}

static void bts_drex_init(void)
{

	BTS_DBG("[BTS][%s] bts drex init\n", __func__);

	__raw_writel(0x00000000, drex0_va_base + 0xD8);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xD0);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xC8);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xC0);
	__raw_writel(0x0FFF0FFF, drex0_va_base + 0xA0);
	__raw_writel(0x00000000, drex0_va_base + 0x100);
	__raw_writel(0x58555855, drex0_va_base + 0x104);
	__raw_writel(0x00000000, drex1_va_base + 0xD8);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xD0);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xC8);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xC0);
	__raw_writel(0x0FFF0FFF, drex1_va_base + 0xA0);
	__raw_writel(0x00000000, drex1_va_base + 0x100);
	__raw_writel(0x58555855, drex1_va_base + 0x104);
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
			if (IS_ERR(clk))
				pr_err("failed to get bts clk %s\n",
						exynos5_bts[i].clk_name);
			else {
				exynos5_bts[i].clk = clk;
				clk_prepare(exynos5_bts[i].clk);
			}
		}

		list_add(&exynos5_bts[i].list, &bts_list);
	}

	drex0_va_base = ioremap(EXYNOS5430_PA_DREX0, SZ_4K);
	drex1_va_base = ioremap(EXYNOS5430_PA_DREX1, SZ_4K);

	bts_drex_init();

	bts_debugfs();
	return 0;
}
arch_initcall(exynos5_bts_init);
