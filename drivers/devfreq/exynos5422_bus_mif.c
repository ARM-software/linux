/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *		Jiyun Kim(jiyun83.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/opp.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/delay.h>

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <../drivers/clk/samsung/clk.h>

#include <mach/regs-clock-exynos5422.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/devfreq.h>
#include "devfreq_exynos.h"
#include "regs-mem.h"
#include <mach/asv-exynos.h>
#include <mach/smc.h>
#include <mach/tmu.h>
#include "governor.h"

#include <plat/pll.h>
#include "exynos5422_ppmu.h"

#define SET_DREX_TIMING

inline void REGULATOR_SET_VOLTAGE(struct regulator *a, unsigned long b, unsigned long c)
{
	bool dynamic_self_refresh_enabled = (__raw_readl(S5P_VA_DREXI_0 + 0x4) & 0x20) ? 1 : 0; /* check default status */
	if(dynamic_self_refresh_enabled) {
		pr_debug("MIF:dynamic self-refresh enabled\n");
		__raw_writel(__raw_readl(S5P_VA_DREXI_0 + 0x4) & ~0x20, S5P_VA_DREXI_0 + 0x4); /* DREX0: MemControl.deref_en = 0 */
		__raw_writel(__raw_readl(S5P_VA_DREXI_1 + 0x4) & ~0x20, S5P_VA_DREXI_1 + 0x4); /* DREX1: MemControl.deref_en = 0 */
		__raw_writel(0x08000000, S5P_VA_DREXI_0 + 0x10); /* DREX0 chip0: Exit from self refresh */
		__raw_writel(0x08100000, S5P_VA_DREXI_0 + 0x10); /* DREX0 chip1: Exit from self refresh */
		__raw_writel(0x08000000, S5P_VA_DREXI_1 + 0x10); /* DREX1 chip0: Exit from self refresh */
		__raw_writel(0x08100000, S5P_VA_DREXI_1 + 0x10); /* DREX1 chip1: Exit from self refresh */
	}
	regulator_set_voltage(a, b, c);
	if(dynamic_self_refresh_enabled) {
		__raw_writel(__raw_readl(S5P_VA_DREXI_0 + 0x4) | 0x20, S5P_VA_DREXI_0 + 0x4); /* DREX0: MemControl.deref_en = 1 */
		__raw_writel(__raw_readl(S5P_VA_DREXI_1 + 0x4) | 0x20, S5P_VA_DREXI_1 + 0x4); /* DREX1: MemControl.deref_en = 1 */
	}
}

#define MIF_VOLT_STEP		12500
#define COLD_VOLT_OFFSET	37500
#define LIMIT_COLD_VOLTAGE	1250000
#define MIN_COLD_VOLTAGE	950000

static bool en_profile;

#define AREF_CRITICAL		0x17
#define AREF_HOT		0x2E
#define AREF_NORMAL		0x2E

#define BP_CONTORL_ENABLE	0x1
#define BRBRSVCON_ENABLE	0x33
#define QOS_TIMEOUT_VAL0	0x80
#define QOS_TIMEOUT_VAL1	0xFFF

#define SET_0			0
#define SET_1			1

#define NEW_THERMAL
#ifdef CONFIG_EXYNOS_THERMAL
bool mif_is_probed;
#endif

static bool mif_transition_disabled;
static unsigned int enabled_fimc_lite;
unsigned int enabled_ud_encode;
unsigned int enabled_ud_decode;
static unsigned int num_mixer_layers;
static unsigned int num_fimd1_layers;

static struct pm_qos_request exynos5_mif_qos;
static struct pm_qos_request boot_mif_qos;
static struct pm_qos_request media_mif_qos;
static struct pm_qos_request min_mif_thermal_qos;
cputime64_t mif_pre_time;

static struct pm_qos_request exynos5_int_qos;

static DEFINE_MUTEX(media_mutex);

unsigned int timeout_fullhd[][2] = {
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x0FFF0FFF,	0x00000000},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
	{0x00800080,	0x000000FF},
};

struct busfreq_data_mif {
	struct device *dev;
	struct devfreq *devfreq;
	struct opp *curr_opp;
	struct mutex lock;

	struct clk *mclk_cdrex;
	struct clk *mx_mspll_ccore;
	struct clk *mout_bpll;
	struct clk *fout_spll;
	struct clk *mout_spll;
	struct clk *sclk_cdrex;
	struct clk *fout_bpll;
	struct clk *clkm_phy0;
	struct clk *clkm_phy1;

	bool bp_enabled;
	bool changed_timeout;
	unsigned int volt_offset;
	struct regulator *vdd_mif;
	unsigned long mspll_freq;
	unsigned long mspll_volt;
	struct exynos5_ppmu_handle *ppmu;

	struct notifier_block tmu_notifier;
	int busy;
#ifdef CONFIG_EXYNOS_THERMAL
#ifdef NEW_THERMAL
	void __iomem *base_drex0;
	void __iomem *base_drex1;
#endif
#endif
};

#ifdef CONFIG_EXYNOS_THERMAL
#ifdef NEW_THERMAL
#include <linux/workqueue.h>
#define MRSTATUS_THERMAL_BIT_SHIFT (7)
#define MRSTATUS_THERMAL_BIT_MASK  (1)
#define MRSTATUS_THERMAL_LV_MASK   (0x7)

enum devfreq_mif_thermal_autorate {
	RATE_ONE = 0x0000005D,
	RATE_HALF = 0x0000002E,
	RATE_QUARTER = 0x00000017,
};

enum devfreq_mif_thermal_channel {
	THERMAL_CHANNEL0,
	THERMAL_CHANNEL1,
};
struct devfreq_thermal_work {
	struct delayed_work devfreq_mif_thermal_work;
	enum devfreq_mif_thermal_channel channel;
	struct workqueue_struct *work_queue;
	unsigned int polling_period;
	unsigned long max_freq;
	unsigned int thermal_level_cs0;
	unsigned int thermal_level_cs1;
};
static struct workqueue_struct *devfreq_mif_thermal_wq_ch0;
static struct workqueue_struct *devfreq_mif_thermal_wq_ch1;
static struct devfreq_thermal_work devfreq_mif_ch0_work = {
	.channel = THERMAL_CHANNEL0,
	.polling_period = 1000,
};
static struct devfreq_thermal_work devfreq_mif_ch1_work = {
	.channel = THERMAL_CHANNEL1,
	.polling_period = 1000,
};
struct busfreq_data_mif *data_mif;

#endif
#endif
enum mif_bus_idx {
	LV_0 = 0,
	LV_1,
	LV_2,
	LV_3,
	LV_4,
	LV_5,
	LV_6,
	LV_7,
	LV_8,
	LV_END,
};

struct mif_bus_opp_table {
	unsigned int idx;
	unsigned long clk;
	unsigned long volt;
	cputime64_t time_in_state;
};

struct mif_bus_opp_table mif_bus_opp_list[] = {
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{LV_0, 825000, 1050000, 0},
	{LV_1, 728000, 1037500, 0},
	{LV_2, 633000, 1012500, 0},
	{LV_3, 543000,  937500, 0},
	{LV_4, 413000,  887500, 0},
	{LV_5, 275000,  875000, 0},
	{LV_6, 206000,  875000, 0},
	{LV_7, 165000,  875000, 0},
	{LV_8, 138000,  875000, 0},
#else
	{LV_0, 800000, 1050000, 0},
	{LV_1, 733000, 1037500, 0},
	{LV_2, 667000, 1012500, 0},
	{LV_3, 533000,  937500, 0},
	{LV_4, 400000,  887500, 0},
	{LV_5, 266000,  875000, 0},
	{LV_6, 200000,  875000, 0},
	{LV_7, 160000,  875000, 0},
	{LV_8, 133000,  875000, 0},
#endif
};

static unsigned int mif_ud_decode_opp_list[][3] = {
	{LV_3, LV_3, LV_3},
	{LV_3, LV_2, LV_2},
	{LV_2, LV_2, LV_1},
	{LV_2, LV_1, LV_0},
	{LV_1, LV_0, LV_0},
};

static unsigned int mif_fimc_opp_list[][3] = {
	{LV_8, LV_8, LV_8},
	{LV_8, LV_7, LV_5},
	{LV_7, LV_6, LV_4},
	{LV_5, LV_5, LV_4},
	{LV_5, LV_4, LV_1},
};

static unsigned int devfreq_mif_asv_abb[LV_END];

static unsigned int (*exynos5422_dram_param)[3];
static unsigned int *exynos5422_dram_param_spll;

#if defined(SET_DREX_TIMING)
static unsigned int exynos5422_dram_param_3gb[][3] = {
	/* timiningRow, timingData, timingPower */
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{0x575A9713, 0x4740085E, 0x545B0446},	/*825Mhz*/
	{0x4D598651, 0x3730085E, 0x4C510336},	/*728Mhz*/
	{0x4348758F, 0x3730085E, 0x40460335},	/*633Mhz*/
	{0x3A4764CD, 0x3730085E, 0x383C0335},	/*543Mhz*/
	{0x2C35538A, 0x2720085E, 0x2C2E0225},	/*413Mhz*/
	{0x1D244287, 0x2720085E, 0x1C1F0225},	/*275Mhz*/
	{0x162331C6, 0x2720085E, 0x18170225},	/*206Mhz*/
	{0x12223185, 0x2720085E, 0x14130225},	/*165Mhz*/
	{0x11222144, 0x2720085E, 0x10100225},	/*138Mhz*/
#else
	{0x345A96D3, 0x3630065C, 0x50380336},	/* 800Mhz */
	{0x30598651, 0x3630065C, 0x4C340336},	/* 733Mhz */
	{0x2C4885D0, 0x3630065C, 0x442F0335},	/* 667Mhz */
	{0x2347648D, 0x2620065C, 0x38260225},	/* 533Mhz */
	{0x1A35538A, 0x2620065C, 0x281C0225},	/* 400Mhz */
	{0x12244247, 0x2620065C, 0x1C130225},	/* 266Mhz */
	{0x112331C5, 0x2620065C, 0x140E0225},	/* 200Mhz */
	{0x11223185, 0x2620065C, 0x100C0225},	/* 160Mhz */
	{0x11222144, 0x2620065C, 0x100C0225},	/* 133Mhz */
#endif
};

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
static unsigned int exynos5422_dram_param_2gb[][3] = {
	/* timiningRow, timingData, timingPower */
	{0x365A9713, 0x4740085E, 0x543A0446},	/*825Mhz*/
	{0x30598651, 0x3730085E, 0x4C330336},	/*728Mhz*/
	{0x2A48758F, 0x3730085E, 0x402D0335},	/*633Mhz*/
	{0x244764CD, 0x3730085E, 0x38270335},	/*543Mhz*/
	{0x1B35538A, 0x2720085E, 0x2C1D0225},	/*413Mhz*/
	{0x12244287, 0x2720085E, 0x1C140225},	/*275Mhz*/
	{0x112331C6, 0x2720085E, 0x180F0225},	/*206Mhz*/
	{0x12223185, 0x2720085E, 0x140C0225},	/*165Mhz*/
	{0x11222144, 0x2720085E, 0x100C0225},	/*138Mhz*/
};
#endif

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
static unsigned int exynos5422_dram_param_spll_3gb[3] = {
	/* timiningRow, timingData, timingPower */
	0x2A35538A, 0x2720085E, 0x282C0225	/*400Mhz*/
};

static unsigned int exynos5422_dram_param_spll_2gb[3] = {
	/* timiningRow, timingData, timingPower */
	0x1A35538A, 0x2720085E, 0x2720085E	/*400Mhz*/
};

#endif
#endif

/*
 * MIF devfreq notifier
 */
static struct srcu_notifier_head exynos5_mif_transition_notifier_list;

static int __init exynos5_mif_transition_notifier_list_init(void)
{
	srcu_init_notifier_head(&exynos5_mif_transition_notifier_list);

	return 0;
}
pure_initcall(exynos5_mif_transition_notifier_list_init);

int exynos5_mif_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos5_mif_transition_notifier_list, nb);
}
EXPORT_SYMBOL(exynos5_mif_register_notifier);

int exynos5_mif_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos5_mif_transition_notifier_list, nb);
}
EXPORT_SYMBOL(exynos5_mif_unregister_notifier);

int exynos5_mif_notify_transition(struct devfreq_info *info, unsigned int state)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos5_mif_transition_notifier_list, state, info);
}
EXPORT_SYMBOL_GPL(exynos5_mif_notify_transition);

/*
 * MIF devfreq BPLL change notifier
 */
static struct srcu_notifier_head exynos5_mif_bpll_transition_notifier_list;

static int __init exynos5_mif_bpll_transition_notifier_list_init(void)
{
	srcu_init_notifier_head(&exynos5_mif_bpll_transition_notifier_list);

	return 0;
}
pure_initcall(exynos5_mif_bpll_transition_notifier_list_init);

int exynos5_mif_bpll_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos5_mif_bpll_transition_notifier_list, nb);
}
EXPORT_SYMBOL(exynos5_mif_bpll_register_notifier);

int exynos5_mif_bpll_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos5_mif_bpll_transition_notifier_list, nb);
}
EXPORT_SYMBOL(exynos5_mif_bpll_unregister_notifier);

int exynos5_mif_bpll_transition_notify(struct devfreq_info *info, unsigned int state)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos5_mif_bpll_transition_notifier_list, state, info);
}
EXPORT_SYMBOL_GPL(exynos5_mif_bpll_transition_notify);


void exynos5_mif_transition_disable(bool disable)
{
	mif_transition_disabled = disable;
}
EXPORT_SYMBOL_GPL(mif_transition_disabled);

extern void exynos5_update_district_int_level(unsigned int idx);
extern unsigned int int_fimc_opp_list[][3];

void exynos5_update_media_layers(enum devfreq_media_type media_type, unsigned int value)
{
	unsigned int num_total_layers, mif_idx, int_idx;
	unsigned long media_qos_mif_freq = 0;

	mutex_lock(&media_mutex);

	if (media_type == TYPE_FIMC_LITE)
		enabled_fimc_lite = value;
	else if (media_type == TYPE_MIXER)
		num_mixer_layers = value;
	else if (media_type == TYPE_FIMD1)
		num_fimd1_layers = value;
	else if (media_type == TYPE_UD_DECODING)
		enabled_ud_decode = value;
	else if (media_type == TYPE_UD_ENCODING)
		enabled_ud_encode = value;

	num_total_layers = num_mixer_layers + num_fimd1_layers;

	pr_debug("%s: fimc_lite = %s, num_mixer_layers = %u, num_fimd1_layers = %u, "
			"num_total_layers = %u\n", __func__,
			enabled_fimc_lite ? "enabled" : "disabled",
			num_mixer_layers, num_fimd1_layers, num_total_layers);

	if (enabled_fimc_lite) {
		switch (num_total_layers) {
			case NUM_LAYER_5:
				media_qos_mif_freq = mif_bus_opp_list[LV_0].clk;
				break;
			case NUM_LAYER_4:
				if (num_mixer_layers == 0)
					media_qos_mif_freq = mif_bus_opp_list[LV_2].clk;
				else
					media_qos_mif_freq = mif_bus_opp_list[LV_1].clk;
				break;
			case NUM_LAYER_3:
				media_qos_mif_freq = mif_bus_opp_list[LV_2].clk;
				break;
			case NUM_LAYER_2:
				if (num_mixer_layers == 0)
					media_qos_mif_freq = mif_bus_opp_list[LV_3].clk;
				else
					media_qos_mif_freq = mif_bus_opp_list[LV_2].clk;
				break;
			case NUM_LAYER_1:
				media_qos_mif_freq = mif_bus_opp_list[LV_4].clk;
				break;
		}
	} else {
		if (num_fimd1_layers == 0)
			pr_debug("invalid number of fimd1 layers\n");

		mif_idx = mif_fimc_opp_list[num_fimd1_layers][num_mixer_layers];
		media_qos_mif_freq = mif_bus_opp_list[mif_idx].clk;

		int_idx = int_fimc_opp_list[num_fimd1_layers][num_mixer_layers];
		exynos5_update_district_int_level(int_idx);
	}

	if (enabled_ud_decode) {
		if (num_fimd1_layers == 0)
			pr_debug("invalid number of fimd1 layers\n");

		mif_idx = mif_ud_decode_opp_list[num_fimd1_layers][num_mixer_layers];
		media_qos_mif_freq = mif_bus_opp_list[mif_idx].clk;
	}

	switch (num_total_layers) {
		case NUM_LAYER_6:
			if (enabled_fimc_lite)
				media_qos_mif_freq = mif_bus_opp_list[LV_0].clk;
			break;
		case NUM_LAYER_5:
			if (enabled_fimc_lite)
				media_qos_mif_freq = mif_bus_opp_list[LV_2].clk;
			break;
		case NUM_LAYER_4:
			if (enabled_fimc_lite)
				media_qos_mif_freq = mif_bus_opp_list[LV_2].clk;
			else
				media_qos_mif_freq = mif_bus_opp_list[LV_4].clk;
			break;
		case NUM_LAYER_3:
			if (enabled_fimc_lite)
				media_qos_mif_freq = mif_bus_opp_list[LV_3].clk;
			else
				media_qos_mif_freq = mif_bus_opp_list[LV_5].clk;
			break;
		case NUM_LAYER_2:
			if (enabled_fimc_lite)
				media_qos_mif_freq = mif_bus_opp_list[LV_3].clk;
			else
				media_qos_mif_freq = mif_bus_opp_list[LV_7].clk;
			break;
		case NUM_LAYER_1:
		case NUM_LAYER_0:
			if (enabled_fimc_lite) {
				if (num_mixer_layers == 0 && num_fimd1_layers < 2)
					media_qos_mif_freq = mif_bus_opp_list[LV_4].clk;
				else
					media_qos_mif_freq = mif_bus_opp_list[LV_3].clk;
			} else {
				media_qos_mif_freq = mif_bus_opp_list[LV_8].clk;
			}
			break;
	}

	if (pm_qos_request_active(&media_mif_qos))
		pm_qos_update_request(&media_mif_qos, media_qos_mif_freq);

	mutex_unlock(&media_mutex);
}
EXPORT_SYMBOL_GPL(exynos5_update_media_layers);

#ifdef CONFIG_EXYNOS_THERMAL
static unsigned int get_limit_voltage(unsigned int voltage, unsigned int volt_offset)
{
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	if (volt_offset && (voltage + volt_offset < MIN_COLD_VOLTAGE))
		return MIN_COLD_VOLTAGE;

	return voltage + volt_offset;
}
#endif

#ifdef SET_DREX_TIMING
static void exynos5_set_dmc_timing(int target_idx)
{
	unsigned int set_timing_row, set_timing_data, set_timing_power;

	set_timing_row = exynos5422_dram_param[target_idx][0];
	set_timing_data = exynos5422_dram_param[target_idx][1];
	set_timing_power = exynos5422_dram_param[target_idx][2];

	/* set drex timing parameters for target frequency */
	__raw_writel(set_timing_row, EXYNOS5_DREXI_0_TIMINGROW0);
	__raw_writel(set_timing_row, EXYNOS5_DREXI_1_TIMINGROW0);
	__raw_writel(set_timing_data, EXYNOS5_DREXI_0_TIMINGDATA0);
	__raw_writel(set_timing_data, EXYNOS5_DREXI_1_TIMINGDATA0);
	__raw_writel(set_timing_power, EXYNOS5_DREXI_0_TIMINGPOWER0);
	__raw_writel(set_timing_power, EXYNOS5_DREXI_1_TIMINGPOWER0);
}

static void exynos5_set_spll_timing(void)
{
	unsigned int spll_timing_row, spll_timing_data, spll_timing_power;
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	spll_timing_row = exynos5422_dram_param_spll[0];
	spll_timing_data = exynos5422_dram_param_spll[1];
	spll_timing_power = exynos5422_dram_param_spll[2];
#else
	spll_timing_row = exynos5422_dram_param[LV_4][0];
	spll_timing_data = exynos5422_dram_param[LV_4][1];
	spll_timing_power = exynos5422_dram_param[LV_4][2];
#endif
	/* set drex timing parameters for 400MHz switching */
	__raw_writel(spll_timing_row, EXYNOS5_DREXI_0_TIMINGROW1);
	__raw_writel(spll_timing_row, EXYNOS5_DREXI_1_TIMINGROW1);
	__raw_writel(spll_timing_data, EXYNOS5_DREXI_0_TIMINGDATA1);
	__raw_writel(spll_timing_data, EXYNOS5_DREXI_1_TIMINGDATA1);
	__raw_writel(spll_timing_power, EXYNOS5_DREXI_0_TIMINGPOWER1);
	__raw_writel(spll_timing_power, EXYNOS5_DREXI_1_TIMINGPOWER1);
}

static void exynos5_switch_timing(bool set)
{
	unsigned int reg;

	reg = __raw_readl(EXYNOS5_LPDDR3PHY_CON3);

	if (set == SET_0)
		reg &= ~EXYNOS5_TIMING_SET_SWI;
	else
		reg |= EXYNOS5_TIMING_SET_SWI;

	__raw_writel(reg, EXYNOS5_LPDDR3PHY_CON3);
}
#else
static inline void exynos5_set_dmc_timing(int target_idx)
{
	return;
}

static inline void exynos5_set_spll_timing(void)
{
	return;
}

static inline void exynos5_switch_timing(bool set)
{
	return;
}
#endif

static void exynos5_back_pressure_enable(bool enable)
{
	if (enable) {
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_0_BP_CONTROL0);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_0_BP_CONTROL1);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_0_BP_CONTROL2);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_0_BP_CONTROL3);
		__raw_writel(BRBRSVCON_ENABLE, EXYNOS5_DREXI_0_BRBRSVCONTROL);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_1_BP_CONTROL0);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_1_BP_CONTROL1);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_1_BP_CONTROL2);
		__raw_writel(BP_CONTORL_ENABLE, EXYNOS5_DREXI_1_BP_CONTROL3);
		__raw_writel(BRBRSVCON_ENABLE, EXYNOS5_DREXI_1_BRBRSVCONTROL);
	} else {
		__raw_writel(0x0, EXYNOS5_DREXI_0_BP_CONTROL0);
		__raw_writel(0x0, EXYNOS5_DREXI_0_BP_CONTROL1);
		__raw_writel(0x0, EXYNOS5_DREXI_0_BP_CONTROL2);
		__raw_writel(0x0, EXYNOS5_DREXI_0_BP_CONTROL3);
		__raw_writel(0x0, EXYNOS5_DREXI_0_BRBRSVCONTROL);
		__raw_writel(0x0, EXYNOS5_DREXI_1_BP_CONTROL0);
		__raw_writel(0x0, EXYNOS5_DREXI_1_BP_CONTROL1);
		__raw_writel(0x0, EXYNOS5_DREXI_1_BP_CONTROL2);
		__raw_writel(0x0, EXYNOS5_DREXI_1_BP_CONTROL3);
		__raw_writel(0x0, EXYNOS5_DREXI_1_BRBRSVCONTROL);
	}
}

static void exynos5_change_timeout(bool change)
{
	if (change) {
		__raw_writel(QOS_TIMEOUT_VAL0, EXYNOS5_DREXI_0_QOSCONTROL8);
		__raw_writel(QOS_TIMEOUT_VAL0, EXYNOS5_DREXI_1_QOSCONTROL8);
	} else {
		__raw_writel(QOS_TIMEOUT_VAL1, EXYNOS5_DREXI_0_QOSCONTROL8);
		__raw_writel(QOS_TIMEOUT_VAL1, EXYNOS5_DREXI_1_QOSCONTROL8);
	}
}

static void exynos5_mif_set_freq(struct busfreq_data_mif *data,
		unsigned long old_freq, unsigned long target_freq)
{
	unsigned int tmp;
	int i, target_idx = LV_0;

	pr_debug("%s: oldfreq %ld, freq %ld \n", __func__, old_freq, target_freq);

	/*
	 * Find setting value with target frequency
	 */
	for (i = LV_0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == target_freq) {
			target_idx = mif_bus_opp_list[i].idx;
			break;
		}
	}

	if (target_idx <= LV_4) {
		if (!pm_qos_request_active(&exynos5_int_qos))
			pm_qos_add_request(&exynos5_int_qos, PM_QOS_DEVICE_THROUGHPUT, 111000);
	} else {
		if (pm_qos_request_active(&exynos5_int_qos))
			pm_qos_remove_request(&exynos5_int_qos);
	}

	if (!data->bp_enabled && (target_freq <= mif_bus_opp_list[LV_4].clk ||
		(enabled_ud_decode || enabled_ud_encode || enabled_fimc_lite))) {
		exynos5_back_pressure_enable(true);
		data->bp_enabled = true;
	}

	if (target_freq > mif_bus_opp_list[LV_5].clk && data->changed_timeout) {
		exynos5_change_timeout(false);
		data->changed_timeout = false;
	}

	clk_prepare_enable(data->fout_spll);
	exynos5_set_spll_timing();
	exynos5_switch_timing(SET_1);

	/* MUX_CORE_SEL = MX_MSPLL_CCORE */
	if (clk_set_parent(data->mclk_cdrex, data->mx_mspll_ccore)) {
		pr_err("Unable to set parent mx_mspll_ccore of clock mclk_cdrex.\n");
	}
	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS5_CLK_MUX_STAT_CDREX)
				>> EXYNOS5_CLKSRC_MCLK_CDREX_SEL_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	exynos5_set_dmc_timing(target_idx);

	/* Change bpll MPS values*/
	clk_set_rate(data->fout_bpll, target_freq * 1000);

	exynos5_switch_timing(SET_0);

	/* MUX_CORE_SEL = BPLL */
	if (clk_set_parent(data->mclk_cdrex, data->mout_bpll)) {
		pr_err("Unable to set parent mout_bpll of clock mclk_cdrex.\n");
	}
	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS5_CLK_MUX_STAT_CDREX)
				>> EXYNOS5_CLKSRC_MCLK_CDREX_SEL_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x1);

	clk_disable_unprepare(data->fout_spll);

	if (target_freq <= mif_bus_opp_list[LV_5].clk && !data->changed_timeout) {
		exynos5_change_timeout(true);
		data->changed_timeout = true;
	}

	if (data ->bp_enabled && (target_freq > mif_bus_opp_list[LV_4].clk &&
		!enabled_ud_decode && !enabled_ud_encode  &&!enabled_fimc_lite)) {
		exynos5_back_pressure_enable(false);
		data->bp_enabled = false;
	}
}

static void exynos5_mif_update_state(unsigned int target_freq)
{
	cputime64_t cur_time = get_jiffies_64();
	cputime64_t tmp_cputime;
	unsigned int target_idx = -EINVAL;
	unsigned int i;

	/*
	 * Find setting value with target frequency
	 */
	for (i = LV_0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == target_freq)
			target_idx = mif_bus_opp_list[i].idx;
	}

	tmp_cputime = cur_time - mif_pre_time;

	mif_bus_opp_list[target_idx].time_in_state =
		mif_bus_opp_list[target_idx].time_in_state + tmp_cputime;

	mif_pre_time = cur_time;
}

unsigned long curr_mif_freq;
static int exynos5_mif_busfreq_target(struct device *dev,
		unsigned long *_freq, u32 flags)
{
	int err = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);
	struct opp *opp;
	struct devfreq_info info;
	unsigned long freq;
	unsigned long old_freq;
	unsigned long target_volt;
	int i, target_idx = LV_0;
	bool set_abb_first_than_volt;

	if (mif_transition_disabled)
		return 0;

	mutex_lock(&data->lock);

	/* Get available opp information */
	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		mutex_unlock(&data->lock);
		return PTR_ERR(opp);
	}
	freq = opp_get_freq(opp);
	target_volt = opp_get_voltage(opp);
	rcu_read_unlock();

#ifdef CONFIG_EXYNOS_THERMAL
#ifdef NEW_THERMAL
	freq = min3(freq,
			devfreq_mif_ch0_work.max_freq,
			devfreq_mif_ch1_work.max_freq);
#endif
#endif
	/* get olg opp information */
	rcu_read_lock();
	old_freq = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	exynos5_mif_update_state(old_freq);

	if (old_freq == freq)
		goto out;

	info.old = old_freq;
	info.new = freq;

#ifdef CONFIG_EXYNOS_THERMAL
	if (data->volt_offset)
		target_volt = get_limit_voltage(target_volt, data->volt_offset);
#endif
	for (i = LV_0; i < LV_END; i++) {
		if (mif_bus_opp_list[i].clk == freq) {
			target_idx = mif_bus_opp_list[i].idx;
			break;
		}
	}

	set_abb_first_than_volt = is_set_abb_first(ID_MIF, old_freq, freq);

	/*
	 * If target freq is higher than old freq
	 * after change voltage, setting freq ratio
	 */
	if (old_freq < freq) {
		if (set_abb_first_than_volt)
			set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		if ((old_freq < data->mspll_freq) && (freq < data->mspll_freq))
			REGULATOR_SET_VOLTAGE(data->vdd_mif, data->mspll_volt,
					data->mspll_volt + MIF_VOLT_STEP);
		else
			REGULATOR_SET_VOLTAGE(data->vdd_mif, target_volt,
					target_volt + MIF_VOLT_STEP);

		if (!set_abb_first_than_volt)
			set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);

		exynos5_mif_set_freq(data, old_freq, freq);
		if ((old_freq < data->mspll_freq) && (freq < data->mspll_freq)) {
			if (set_abb_first_than_volt)
				set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
			REGULATOR_SET_VOLTAGE(data->vdd_mif, target_volt,
					target_volt + MIF_VOLT_STEP);
			if (!set_abb_first_than_volt)
				set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		}

		if (freq == mif_bus_opp_list[LV_0].clk)	{
			clk_prepare_enable(data->clkm_phy0);
			clk_prepare_enable(data->clkm_phy1);
		}
	} else {
		if (old_freq == mif_bus_opp_list[LV_0].clk)	{
			clk_disable_unprepare(data->clkm_phy0);
			clk_disable_unprepare(data->clkm_phy1);
		}
		if ((old_freq < data->mspll_freq) && (freq < data->mspll_freq)) {
			if (set_abb_first_than_volt)
				set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
			REGULATOR_SET_VOLTAGE(data->vdd_mif, data->mspll_volt,
					data->mspll_volt + MIF_VOLT_STEP);
			if (!set_abb_first_than_volt)
				set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		}

		exynos5_mif_set_freq(data, old_freq, freq);
		if (set_abb_first_than_volt)
			set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
		REGULATOR_SET_VOLTAGE(data->vdd_mif, target_volt, target_volt + MIF_VOLT_STEP);
		if (!set_abb_first_than_volt)
			set_match_abb(ID_MIF, devfreq_mif_asv_abb[target_idx]);
	}

	curr_mif_freq = freq;
	data->curr_opp = opp;
out:
	mutex_unlock(&data->lock);

	return err;
}

static int exynos5_mif_bus_get_dev_status(struct device *dev,
		struct devfreq_dev_status *stat)
{
	struct busfreq_data_mif *data = dev_get_drvdata(dev);
	unsigned long busy_data;
	unsigned int int_ccnt = 0;
	unsigned long int_pmcnt = 0;

	rcu_read_lock();
	stat->current_frequency = opp_get_freq(data->curr_opp);
	rcu_read_unlock();

	/*
	 * Bandwidth of memory interface is 128bits
	 * So bus can transfer 16bytes per cycle
	 */
	busy_data = exynos5_ppmu_get_busy(data->ppmu, PPMU_SET_DDR,
			&int_ccnt, &int_pmcnt);

	/* TODO: ppmu will return 0, when after suspend/resume */
	if(!(int_ccnt | int_pmcnt))
		return 0;

	stat->total_time = int_ccnt;
	stat->busy_time = int_pmcnt;

	if (en_profile)
		pr_info("%lu,%lu\n", stat->busy_time, stat->total_time);

	return 0;
}

static struct devfreq_dev_profile exynos5_mif_devfreq_profile = {
	.initial_freq	= 825000,
	.polling_ms	= 100,
	.target		= exynos5_mif_busfreq_target,
	.get_dev_status	= exynos5_mif_bus_get_dev_status,
	.max_state = LV_END,
};

static int exynos5422_dram_parameter(void)
{
	unsigned int pkg_id;

	pkg_id = __raw_readl(S5P_VA_CHIPID + 4);

	pkg_id = (pkg_id >> 4) & 0xf;

	if (pkg_id == 0x2) {
		exynos5422_dram_param = exynos5422_dram_param_2gb;
		exynos5422_dram_param_spll = exynos5422_dram_param_spll_2gb;
		return 0;
	}

	if (pkg_id == 0x0 || pkg_id == 0x1 || pkg_id == 0x7) {
		exynos5422_dram_param = exynos5422_dram_param_3gb;
		exynos5422_dram_param_spll = exynos5422_dram_param_spll_3gb;
		return 0;
	}

	return -EINVAL;
}

static int exynos5422_mif_table(struct busfreq_data_mif *data)
{
	unsigned int i;
	unsigned int ret;
	unsigned int asv_volt;
	unsigned int asv_abb = 0;

	/* will add code for ASV information setting function in here */

	for (i = 0; i < ARRAY_SIZE(mif_bus_opp_list); i++) {
		asv_volt = get_match_volt(ID_MIF, mif_bus_opp_list[i].clk);

		if (!asv_volt)
			asv_volt = mif_bus_opp_list[i].volt;

		pr_info("MIF %luKhz ASV is %duV\n", mif_bus_opp_list[i].clk, asv_volt);

		exynos5_mif_devfreq_profile.freq_table[i] = mif_bus_opp_list[i].clk;

		ret = opp_add(data->dev, mif_bus_opp_list[i].clk, asv_volt);

		if (ret) {
			dev_err(data->dev, "Fail to add opp entries.\n");
			return ret;
		}
		asv_abb = get_match_abb(ID_MIF, mif_bus_opp_list[i].clk);
		if (!asv_abb)
			devfreq_mif_asv_abb[i] = ABB_BYPASS;
		else
			devfreq_mif_asv_abb[i] = asv_abb;
		pr_info("DEVFREQ(MIF) : %luKhz, ABB %u\n", mif_bus_opp_list[i].clk, devfreq_mif_asv_abb[i]);
	}

	return 0;
}

#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_USAGE)
static struct devfreq_simple_usage_data exynos5_mif_governor_data = {
	.upthreshold		= 85,
	.target_percentage	= 80,
	.proportional		= 100,
	.cal_qos_max		= 825000,
	.pm_qos_class		= PM_QOS_BUS_THROUGHPUT,
};
#endif

#ifdef CONFIG_EXYNOS_THERMAL
#ifdef NEW_THERMAL
static void exynos5_devfreq_thermal_event(struct devfreq_thermal_work *work)
{
	if (work->polling_period == 0)
		return;

	queue_delayed_work(work->work_queue,
			&work->devfreq_mif_thermal_work,
			msecs_to_jiffies(work->polling_period));
}

static ssize_t mif_show_templvl_ch0_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch0_work.thermal_level_cs0);
}
static ssize_t mif_show_templvl_ch0_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch0_work.thermal_level_cs1);
}
static ssize_t mif_show_templvl_ch1_0(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch1_work.thermal_level_cs0);
}
static ssize_t mif_show_templvl_ch1_1(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", devfreq_mif_ch1_work.thermal_level_cs1);
}

static DEVICE_ATTR(mif_templvl_ch0_0, 0644, mif_show_templvl_ch0_0, NULL);
static DEVICE_ATTR(mif_templvl_ch0_1, 0644, mif_show_templvl_ch0_1, NULL);
static DEVICE_ATTR(mif_templvl_ch1_0, 0644, mif_show_templvl_ch1_0, NULL);
static DEVICE_ATTR(mif_templvl_ch1_1, 0644, mif_show_templvl_ch1_1, NULL);

static struct attribute *devfreq_mif_sysfs_entries[] = {
	&dev_attr_mif_templvl_ch0_0.attr,
	&dev_attr_mif_templvl_ch0_1.attr,
	&dev_attr_mif_templvl_ch1_0.attr,
	&dev_attr_mif_templvl_ch1_1.attr,
	NULL,
};

static struct attribute_group devfreq_mif_attr_group = {
	.name   = "temp_level",
	.attrs  = devfreq_mif_sysfs_entries,
};

static void exynos5_devfreq_thermal_monitor(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct devfreq_thermal_work *thermal_work =
		container_of(d_work, struct devfreq_thermal_work, devfreq_mif_thermal_work);
	unsigned int mrstatus, tmp_thermal_level, max_thermal_level = 0;
	unsigned int timingaref_value = RATE_ONE;
	unsigned long max_freq = exynos5_mif_governor_data.cal_qos_max;
	bool throttling = false;
	void __iomem *base_drex = NULL;

	if (thermal_work->channel == THERMAL_CHANNEL0) {
		base_drex = data_mif->base_drex0;
	} else if (thermal_work->channel == THERMAL_CHANNEL1) {
		base_drex = data_mif->base_drex1;
	}
	__raw_writel(0x09001000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs0 = tmp_thermal_level;

	if (thermal_work->channel == THERMAL_CHANNEL0)
		devfreq_mif_ch0_work.thermal_level_cs0 = thermal_work->thermal_level_cs0;
	else if (thermal_work->channel == THERMAL_CHANNEL1)
		devfreq_mif_ch1_work.thermal_level_cs0 = thermal_work->thermal_level_cs0;

	__raw_writel(0x09101000, base_drex + 0x10);
	mrstatus = __raw_readl(base_drex + 0x54);
	tmp_thermal_level = (mrstatus & MRSTATUS_THERMAL_LV_MASK);
	if (tmp_thermal_level > max_thermal_level)
		max_thermal_level = tmp_thermal_level;

	thermal_work->thermal_level_cs1 = tmp_thermal_level;

	if (thermal_work->channel == THERMAL_CHANNEL0)
		devfreq_mif_ch0_work.thermal_level_cs1 = thermal_work->thermal_level_cs1;
	else if (thermal_work->channel == THERMAL_CHANNEL1)
		devfreq_mif_ch1_work.thermal_level_cs1 = thermal_work->thermal_level_cs1;

	switch (max_thermal_level) {
		case 0:
		case 1:
		case 2:
		case 3:
			timingaref_value = RATE_HALF;
			thermal_work->polling_period = 1000;
			break;
		case 4:
			timingaref_value = RATE_HALF;
			thermal_work->polling_period = 300;
			break;
		case 6:
			throttling = true;
		case 5:
			timingaref_value = RATE_QUARTER;
			thermal_work->polling_period = 100;
			break;
		default:
			pr_err("DEVFREQ(MIF) : can't support memory thermal level\n");
			return;
	}

	if (throttling){
		max_freq = mif_bus_opp_list[LV_6].clk;
	}
	else
		max_freq = exynos5_mif_governor_data.cal_qos_max;

	if (thermal_work->max_freq != max_freq) {
		thermal_work->max_freq = max_freq;
		mutex_lock(&data_mif->devfreq->lock);
		data_mif->devfreq->max_freq = max_freq;
		update_devfreq(data_mif->devfreq);
		mutex_unlock(&data_mif->devfreq->lock);
	}

	__raw_writel(timingaref_value, base_drex + 0x30);
	exynos5_devfreq_thermal_event(thermal_work);
}

static void exynos5_devfreq_init_thermal(void)
{
	devfreq_mif_thermal_wq_ch0 = create_freezable_workqueue("devfreq_thermal_wq_ch0");
	devfreq_mif_thermal_wq_ch1 = create_freezable_workqueue("devfreq_thermal_wq_ch1");

	INIT_DELAYED_WORK(&devfreq_mif_ch0_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);
	INIT_DELAYED_WORK(&devfreq_mif_ch1_work.devfreq_mif_thermal_work,
			exynos5_devfreq_thermal_monitor);

	devfreq_mif_ch0_work.work_queue = devfreq_mif_thermal_wq_ch0;
	devfreq_mif_ch1_work.work_queue = devfreq_mif_thermal_wq_ch1;

	exynos5_devfreq_thermal_event(&devfreq_mif_ch0_work);
	exynos5_devfreq_thermal_event(&devfreq_mif_ch1_work);
}
#endif
#endif
static ssize_t mif_show_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int i;
	ssize_t len = 0;
	ssize_t write_cnt = (ssize_t)((PAGE_SIZE / LV_END) - 2);

	for (i = LV_0; i < LV_END; i++)
		len += snprintf(buf + len, write_cnt, "%ld %llu\n", mif_bus_opp_list[i].clk,
				(unsigned long long)mif_bus_opp_list[i].time_in_state);

	return len;
}

static DEVICE_ATTR(mif_time_in_state, 0644, mif_show_state, NULL);

static ssize_t show_upthreshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", exynos5_mif_governor_data.upthreshold);
}

static ssize_t store_upthreshold(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	if (count > sizeof(value))
		goto out;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1 || value > 100)
		goto out;

	exynos5_mif_governor_data.upthreshold = value;
out:
	return count;
}

static DEVICE_ATTR(upthreshold, S_IRUGO | S_IWUSR, show_upthreshold, store_upthreshold);

static ssize_t show_target_percentage(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", exynos5_mif_governor_data.target_percentage);
}

static ssize_t store_target_percentage(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	if (count > sizeof(value))
		goto out;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1 || value > 100)
		goto out;

	exynos5_mif_governor_data.target_percentage = value;
out:
	return count;
}

static DEVICE_ATTR(target_percentage, S_IRUGO | S_IWUSR, show_target_percentage, store_target_percentage);

static ssize_t show_proportional(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", exynos5_mif_governor_data.proportional);
}

static ssize_t store_proportional(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	if (count > sizeof(value))
		goto out;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1 || value > 100)
		goto out;

	exynos5_mif_governor_data.proportional = value;
out:
	return count;
}

static DEVICE_ATTR(proportional, S_IRUGO | S_IWUSR, show_proportional, store_proportional);

static ssize_t show_en_profile(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", en_profile ? "true" : "false");
}

static ssize_t store_en_profile(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	if (count > sizeof(value))
		goto out;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1 || value > 2)
		goto out;

	if (value)
		en_profile = true;
	else
		en_profile = false;

out:
	return count;
}

static DEVICE_ATTR(en_profile, S_IRUGO | S_IWUSR, show_en_profile, store_en_profile);

static struct attribute *busfreq_mif_entries[] = {
	&dev_attr_mif_time_in_state.attr,
	&dev_attr_upthreshold.attr,
	&dev_attr_target_percentage.attr,
	&dev_attr_proportional.attr,
	&dev_attr_en_profile.attr,
	NULL,
};

static struct attribute_group devfreq_attr_group = {
	.name	= "time_in_state",
	.attrs	= busfreq_mif_entries,
};

static ssize_t show_freq_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, count = 0;
	struct opp *opp;
	ssize_t write_cnt = (ssize_t)((PAGE_SIZE / ARRAY_SIZE(mif_bus_opp_list)) - 2);
	struct device *mif_dev = dev->parent;

	if (!unlikely(mif_dev)) {
		pr_err("%s: device is not probed\n", __func__);
		return -ENODEV;
	}

	rcu_read_lock();
	for (i = 0; i < ARRAY_SIZE(mif_bus_opp_list); i++) {
		opp = opp_find_freq_exact(mif_dev, mif_bus_opp_list[i].clk, true);
		if (!IS_ERR_OR_NULL(opp))
			count += snprintf(&buf[count], write_cnt, "%lu ", opp_get_freq(opp));
	}
	rcu_read_unlock();

	count += snprintf(&buf[count], 2, "\n");
	return count;
}

static DEVICE_ATTR(freq_table, S_IRUGO, show_freq_table, NULL);

static struct exynos_devfreq_platdata exynos5422_qos_mif = {
	.default_qos = 138000,
};

static int exynos5_mif_reboot_notifier_call(struct notifier_block *this,
		unsigned long code, void *_cmd)
{
	pm_qos_update_request(&exynos5_mif_qos, exynos5_mif_devfreq_profile.initial_freq);

	return NOTIFY_DONE;
}

static struct notifier_block exynos5_mif_reboot_notifier = {
	.notifier_call = exynos5_mif_reboot_notifier_call,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos5_bus_mif_tmu_notifier(struct notifier_block *notifier,
		unsigned long event, void *v)
{
	struct busfreq_data_mif *data = container_of(notifier, struct busfreq_data_mif,
			tmu_notifier);
	struct exynos_devfreq_platdata *pdata = data->dev->platform_data;
	unsigned int prev_volt, set_volt;
	unsigned int *on = v;

	if (event == TMU_COLD) {
		if (pm_qos_request_active(&exynos5_mif_qos))
			pm_qos_update_request(&exynos5_mif_qos,
					exynos5_mif_devfreq_profile.initial_freq);

		if (*on) {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != COLD_VOLT_OFFSET) {
				data->volt_offset = COLD_VOLT_OFFSET;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			/* setting voltage for MIF about cold temperature */
			set_volt = get_limit_voltage(prev_volt, data->volt_offset);
			REGULATOR_SET_VOLTAGE(data->vdd_mif, set_volt, set_volt + MIF_VOLT_STEP);

			mutex_unlock(&data->lock);
		} else {
			mutex_lock(&data->lock);

			prev_volt = regulator_get_voltage(data->vdd_mif);

			if (data->volt_offset != 0) {
				data->volt_offset = 0;
			} else {
				mutex_unlock(&data->lock);
				return NOTIFY_OK;
			}

			/* restore voltage for MIF */
			set_volt = get_limit_voltage(prev_volt - COLD_VOLT_OFFSET, data->volt_offset);
			REGULATOR_SET_VOLTAGE(data->vdd_mif, set_volt, set_volt + MIF_VOLT_STEP);

			mutex_unlock(&data->lock);
		}

		if (pm_qos_request_active(&exynos5_mif_qos))
			pm_qos_update_request(&exynos5_mif_qos, pdata->default_qos);
	}

#ifndef NEW_THERMAL
	switch (event) {
		case MIF_TH_LV1:
			__raw_writel(AREF_NORMAL, EXYNOS5_DREXI_0_TIMINGAREF);
			__raw_writel(AREF_NORMAL, EXYNOS5_DREXI_1_TIMINGAREF);

			if (pm_qos_request_active(&min_mif_thermal_qos))
				pm_qos_update_request(&min_mif_thermal_qos, pdata->default_qos);

			break;
		case MIF_TH_LV2:
			/*
			 * In case of temperature increment, set MIF level 266Mhz as minimum
			 * before changing dram refresh counter.
			 */
			if (*on < MIF_TH_LV2) {
				if (pm_qos_request_active(&min_mif_thermal_qos))
					pm_qos_update_request(&min_mif_thermal_qos,
							mif_bus_opp_list[LV_5].clk);
			}

			__raw_writel(AREF_HOT, EXYNOS5_DREXI_0_TIMINGAREF);
			__raw_writel(AREF_HOT, EXYNOS5_DREXI_1_TIMINGAREF);

			/*
			 * In case of temperature decrement, set MIF level 266Mhz as minimum
			 * after changing dram refresh counter.
			 */
			if (*on > MIF_TH_LV2) {
				if (pm_qos_request_active(&min_mif_thermal_qos))
					pm_qos_update_request(&min_mif_thermal_qos,
							mif_bus_opp_list[LV_5].clk);
			}

			break;
		case MIF_TH_LV3:
			if (pm_qos_request_active(&min_mif_thermal_qos))
				pm_qos_update_request(&min_mif_thermal_qos, mif_bus_opp_list[LV_4].clk);

			__raw_writel(AREF_CRITICAL, EXYNOS5_DREXI_0_TIMINGAREF);
			__raw_writel(AREF_CRITICAL, EXYNOS5_DREXI_1_TIMINGAREF);

			break;
	}
#endif
	return NOTIFY_OK;
}
#endif

#define clk_get(a, b) __clk_lookup(b)
static int exynos5_devfreq_probe(struct platform_device *pdev)
{
	struct busfreq_data_mif *data;
	struct opp *opp, *mspll_opp;
	struct device *dev = &pdev->dev;
	unsigned int tmp;
	unsigned long tmpfreq;
	struct exynos_devfreq_platdata *pdata;
	int err = 0;
	unsigned long initial_freq;
	unsigned long initial_volt, current_volt;

	data = kzalloc(sizeof(struct busfreq_data_mif), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Failed to allocate memory for MIF\n");
		return -ENOMEM;
	}

	exynos5_mif_devfreq_profile.freq_table = kzalloc(sizeof(int) * LV_END, GFP_KERNEL);
	if (exynos5_mif_devfreq_profile.freq_table == NULL) {
		pr_err("DEVFREQ(MIF) : Failed to allocate freq table\n");
		kfree(data);
		return -ENOMEM;
	}

	if (exynos5422_dram_parameter()) {
		dev_err(dev, "Can't support memory type\n");
		goto err_regulator;
	}

	/* Enable pause function for DREX2 DVFS */
	tmp = __raw_readl(EXYNOS5_DMC_PAUSE_CTRL);
	tmp |= EXYNOS5_DMC_PAUSE_ENABLE;
	__raw_writel(tmp, EXYNOS5_DMC_PAUSE_CTRL);

	exynos5_set_spll_timing();

	data->dev = dev;
	mutex_init(&data->lock);

	/* Setting table for MIF*/
	exynos5422_mif_table(data);

	data->vdd_mif = regulator_get(dev, "vdd_mif");
	if (IS_ERR(data->vdd_mif)) {
		dev_err(dev, "Cannot get the regulator \"vdd_mif\"\n");
		err = PTR_ERR(data->vdd_mif);
		goto err_regulator;
	}

	/* Get clock */
	data->mclk_cdrex = clk_get(dev, "mout_mclk_cdrex");
	if (IS_ERR(data->mclk_cdrex)) {
		dev_err(dev, "Cannot get clock \"mclk_cdrex\"\n");
		err = PTR_ERR(data->mclk_cdrex);
		goto err_mout_mclk_cdrex;
	}

	data->mx_mspll_ccore = clk_get(dev, "mout_mx_mspll_ccore");
	if (IS_ERR(data->mx_mspll_ccore)) {
		dev_err(dev, "Cannot get clock \"mx_mspll_ccore\"\n");
		err = PTR_ERR(data->mx_mspll_ccore);
		goto err_mx_mspll_ccore;
	}

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	data->mout_spll = clk_get(dev, "dout_spll_ctrl_div2");
#else
	data->mout_spll = clk_get(dev, "mout_spll_ctrl");
#endif
	if (IS_ERR(data->mout_spll)) {
		dev_err(dev, "Cannot get clock \"mout_spll\"\n");
		err = PTR_ERR(data->mout_spll);
		goto err_mout_spll;
	}

	clk_set_parent(data->mx_mspll_ccore, data->mout_spll);
	clk_put(data->mout_spll);

	data->fout_spll = clk_get(dev, "fout_spll");
	if (IS_ERR(data->fout_spll)) {
		dev_err(dev, "Cannot get clock \"fout_spll\"\n");
		err = PTR_ERR(data->fout_spll);
		goto err_fout_spll;
	}

	data->mout_bpll = clk_get(dev, "mout_bpll_ctrl_user");
	if (IS_ERR(data->mout_bpll)) {
		dev_err(dev, "Cannot get clock \"mout_bpll\"\n");
		err = PTR_ERR(data->mout_bpll);
		goto err_mout_bpll;
	}

	data->fout_bpll = clk_get(dev, "fout_bpll");
	if (IS_ERR(data->fout_bpll)) {
		dev_err(dev, "Cannot get clock \"fout_bpll\"\n");
		err = PTR_ERR(data->fout_bpll);
		goto err_fout_bpll;
	}

	data->clkm_phy0 = clk_get(dev, "clkm_phy0");
	if (IS_ERR(data->clkm_phy0)) {
		dev_err(dev, "Cannot get clock \"clkm_phy0\"\n");
		err = PTR_ERR(data->clkm_phy0);
		goto err_clkm_phy;
	}
	data->clkm_phy1 = clk_get(dev, "clkm_phy1");
	if (IS_ERR(data->clkm_phy1)) {
		dev_err(dev, "Cannot get clock \"clkm_phy1\"\n");
		err = PTR_ERR(data->clkm_phy1);
		goto err_clkm_phy;
	}

	clk_prepare_enable(data->clkm_phy0);
	clk_prepare_enable(data->clkm_phy1);

	/* support ASV setting */
	initial_freq = clk_get_rate(data->mclk_cdrex);
	initial_volt = get_match_volt(ID_MIF, initial_freq/1000);
	REGULATOR_SET_VOLTAGE(data->vdd_mif, initial_volt, initial_volt);
	current_volt = regulator_get_voltage(data->vdd_mif);
	if (current_volt != initial_volt)
		dev_err(dev, "Cannot set default asv voltage\n");
	pr_info("MIF: set ASV freq %ld, voltage %ld\n", initial_freq/1000, current_volt);

	data->ppmu = exynos5_ppmu_get();
	if (!data->ppmu)
		goto err_opp_add;

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &exynos5_mif_devfreq_profile.initial_freq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
				exynos5_mif_devfreq_profile.initial_freq);
		err = PTR_ERR(opp);
		goto err_opp_add;
	}
	rcu_read_unlock();

	mif_pre_time = get_jiffies_64();

	data->curr_opp = opp;
	data->volt_offset = 0;
	data->bp_enabled = false;
	data->changed_timeout = false;

#ifdef CONFIG_EXYNOS_THERMAL
	data->tmu_notifier.notifier_call = exynos5_bus_mif_tmu_notifier;
#ifdef NEW_THERMAL
	data->base_drex0 = S5P_VA_DREXI_0;
	data->base_drex1 = S5P_VA_DREXI_1;
	data_mif = data;
#endif
#endif
	platform_set_drvdata(pdev, data);
#if defined(CONFIG_DEVFREQ_GOV_SIMPLE_USAGE)
	data->devfreq = devfreq_add_device(dev, &exynos5_mif_devfreq_profile,
			"simple_usage", &exynos5_mif_governor_data);
#endif
#if defined(CONFIG_DEVFREQ_GOV_USERSPACE)
	data->devfreq = devfreq_add_device(dev, &exynos5_mif_devfreq_profile, "user_space", NULL);
#endif
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto err_opp_add;
	}

	/* set mspll frequency and voltage information for devfreq */
	tmpfreq = clk_get_rate(data->mx_mspll_ccore) / 1000;

	rcu_read_lock();
	mspll_opp = devfreq_recommended_opp(dev, &tmpfreq, 0);
	if (IS_ERR(mspll_opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		err = PTR_ERR(mspll_opp);

		goto err_opp_add;
	}
	data->mspll_freq = opp_get_freq(mspll_opp);
	data->mspll_volt = opp_get_voltage(mspll_opp);
	rcu_read_unlock();

	/* Set Max information for devfreq */
	tmpfreq = ULONG_MAX;

	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &tmpfreq);
	if (IS_ERR(opp)) {
		rcu_read_unlock();
		dev_err(dev, "%s: Invalid OPP.\n", __func__);
		err = PTR_ERR(opp);

		goto err_opp_add;
	}
	data->devfreq->max_freq = opp_get_freq(opp);
	rcu_read_unlock();

	devfreq_register_opp_notifier(dev, data->devfreq);

	/* Create file for time_in_state */
	err = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_attr_group);

	/* Add sysfs for freq_table */
	err = device_create_file(&data->devfreq->dev, &dev_attr_freq_table);
	if (err)
		pr_err("%s: Fail to create sysfs file\n", __func__);

	err = sysfs_create_group(&data->devfreq->dev.kobj, &devfreq_mif_attr_group);

	pdata = pdev->dev.platform_data;
	if (!pdata)
		pdata = &exynos5422_qos_mif;

#ifdef CONFIG_EXYNOS_THERMAL
#ifdef NEW_THERMAL
	devfreq_mif_ch0_work.max_freq = exynos5_mif_governor_data.cal_qos_max;
	devfreq_mif_ch1_work.max_freq = exynos5_mif_governor_data.cal_qos_max;
#endif
#endif
	pm_qos_add_request(&exynos5_mif_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&boot_mif_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);
	pm_qos_add_request(&media_mif_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);
	pm_qos_update_request_timeout(&boot_mif_qos,
			exynos5_mif_devfreq_profile.initial_freq, 40000 * 1000); /* 40 second */
	pm_qos_add_request(&min_mif_thermal_qos, PM_QOS_BUS_THROUGHPUT, pdata->default_qos);

	register_reboot_notifier(&exynos5_mif_reboot_notifier);

#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&data->tmu_notifier);
	mif_is_probed = true;
#ifdef NEW_THERMAL
	devfreq_mif_ch0_work.max_freq = exynos5_mif_governor_data.cal_qos_max;
	devfreq_mif_ch1_work.max_freq = exynos5_mif_governor_data.cal_qos_max;
	exynos5_devfreq_init_thermal();
#endif
#endif
	return 0;

err_opp_add:
	clk_put(data->clkm_phy0);
	clk_put(data->clkm_phy1);
err_clkm_phy:
	clk_put(data->fout_bpll);
err_fout_bpll:
	clk_put(data->mout_bpll);
err_mout_bpll:
	clk_put(data->mx_mspll_ccore);
err_mx_mspll_ccore:
	clk_put(data->mclk_cdrex);
err_mout_mclk_cdrex:
	clk_put(data->fout_spll);
err_fout_spll:
	clk_put(data->mout_spll);
err_mout_spll:
	regulator_put(data->vdd_mif);
err_regulator:
	kfree(exynos5_mif_devfreq_profile.freq_table);
	kfree(data);

	return err;
}

static int exynos5_devfreq_remove(struct platform_device *pdev)
{
	struct busfreq_data_mif *data = platform_get_drvdata(pdev);

#ifdef CONFIG_EXYNOS_THERMAL
#ifdef NEW_THERMAL
	flush_workqueue(devfreq_mif_thermal_wq_ch0);
	destroy_workqueue(devfreq_mif_thermal_wq_ch0);
	flush_workqueue(devfreq_mif_thermal_wq_ch1);
	destroy_workqueue(devfreq_mif_thermal_wq_ch1);
#endif
#endif

	devfreq_remove_device(data->devfreq);

	pm_qos_remove_request(&min_mif_thermal_qos);
	pm_qos_remove_request(&exynos5_mif_qos);

	clk_put(data->mclk_cdrex);
	clk_put(data->mx_mspll_ccore);
	clk_put(data->mout_bpll);
	clk_put(data->fout_spll);
	clk_put(data->mout_spll);
	clk_put(data->clkm_phy0);
	clk_put(data->clkm_phy1);

	regulator_put(data->vdd_mif);

	kfree(exynos5_mif_devfreq_profile.freq_table);
	kfree(data);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int exynos5_devfreq_suspend(struct device *dev)
{
	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, exynos5_mif_devfreq_profile.initial_freq);

	return 0;
}

static int exynos5_devfreq_resume(struct device *dev)
{
	unsigned int tmp;
	struct exynos_devfreq_platdata *pdata = dev->platform_data;

	/* Enable pause function for DREX2 DVFS */
	tmp = __raw_readl(EXYNOS5_DMC_PAUSE_CTRL);
	tmp |= EXYNOS5_DMC_PAUSE_ENABLE;
	__raw_writel(tmp, EXYNOS5_DMC_PAUSE_CTRL);

	exynos5_set_spll_timing();
	exynos5_back_pressure_enable(false);
	exynos5_change_timeout(false);

	if (pm_qos_request_active(&exynos5_mif_qos))
		pm_qos_update_request(&exynos5_mif_qos, pdata->default_qos);

	return 0;
}

static const struct dev_pm_ops exynos5_devfreq_pm = {
	.suspend = exynos5_devfreq_suspend,
	.resume	= exynos5_devfreq_resume,
};

static struct platform_driver exynos5_devfreq_mif_driver = {
	.probe	= exynos5_devfreq_probe,
	.remove	= exynos5_devfreq_remove,
	.driver = {
		.name	= "exynos5-devfreq-mif",
		.owner	= THIS_MODULE,
		.pm	= &exynos5_devfreq_pm,
	},
};

static struct platform_device exynos5_devfreq_mif_device = {
	.name	= "exynos5-devfreq-mif",
	.id	= -1,
};

static int __init exynos5_devfreq_init(void)
{
	int ret;

	exynos5_devfreq_mif_device.dev.platform_data = &exynos5422_qos_mif;

	ret = platform_device_register(&exynos5_devfreq_mif_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5_devfreq_mif_driver);
}
late_initcall(exynos5_devfreq_init);

static void __exit exynos5_devfreq_exit(void)
{
	platform_driver_unregister(&exynos5_devfreq_mif_driver);
	platform_device_unregister(&exynos5_devfreq_mif_device);
}
module_exit(exynos5_devfreq_exit);
