/*
 *  Copyright (C) 2010, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __MXT540E_H__
#define __MXT540E_H__

#include <linux/gpio.h>
#include <plat/gpio-cfg.h>

#define MXT540E_DEV_NAME "Atmel MXT540E"

#define MXT540E_SW_RESET_TIME		300	/* msec */
#define MXT540E_HW_RESET_TIME		130	/* msec */

enum {
	RESERVED_T0 = 0,
	RESERVED_T1,
	DEBUG_DELTAS_T2,
	DEBUG_REFERENCES_T3,
	DEBUG_SIGNALS_T4,
	GEN_MESSAGEPROCESSOR_T5,
	GEN_COMMANDPROCESSOR_T6,
	GEN_POWERCONFIG_T7,
	GEN_ACQUISITIONCONFIG_T8,
	TOUCH_MULTITOUCHSCREEN_T9,
	TOUCH_SINGLETOUCHSCREEN_T10,
	TOUCH_XSLIDER_T11,
	TOUCH_YSLIDER_T12,
	TOUCH_XWHEEL_T13,
	TOUCH_YWHEEL_T14,
	TOUCH_KEYARRAY_T15,
	PROCG_SIGNALFILTER_T16,
	PROCI_LINEARIZATIONTABLE_T17,
	SPT_COMCONFIG_T18,
	SPT_GPIOPWM_T19,
	PROCI_GRIPFACESUPPRESSION_T20,
	RESERVED_T21,
	PROCG_NOISESUPPRESSION_T22,
	TOUCH_PROXIMITY_T23,
	PROCI_ONETOUCHGESTUREPROCESSOR_T24,
	SPT_SELFTEST_T25,
	DEBUG_CTERANGE_T26,
	PROCI_TWOTOUCHGESTUREPROCESSOR_T27,
	SPT_CTECONFIG_T28,
	SPT_GPI_T29,
	SPT_GATE_T30,
	TOUCH_KEYSET_T31,
	TOUCH_XSLIDERSET_T32,
	RESERVED_T33,
	GEN_MESSAGEBLOCK_T34,
	SPARE_T35,
	RESERVED_T36,
	DEBUG_DIAGNOSTIC_T37,
	SPT_USERDATA_T38,
	SPARE_T39,
	PROCI_GRIPSUPPRESSION_T40,
	SPARE_T41,
	PROCI_TOUCHSUPPRESSION_T42,
	SPT_DIGITIZER_T43,
	SPARE_T44,
	SPARE_T45,
	SPT_CTECONFIG_T46,
	PROCI_STYLUS_T47,
	PROCG_NOISESUPPRESSION_T48,
	SPARE_T49,
	SPARE_T50,
	SPARE_T51,
	TOUCH_PROXKEY_T52,
	GEN_DATASOURCE_T53,
	SPARE_T54,
	ADAPTIVE_T55,
	SPARE_T56,
	SPT_GENERICDATA_T57,
	SPARE_T58,
	SPARE_T59,
	SPARE_T60,
	SPT_TIMER_T61,
	RESERVED_T255 = 255,
};

struct mxt540e_platform_data {
	int max_finger_touches;
	const u8 **config_e;
	int gpio_read_done;
	int min_x;
	int max_x;
	int min_y;
	int max_y;
	int min_z;
	int max_z;
	int min_w;
	int max_w;
	u8 irqf_trigger_type;
	u8 chrgtime_batt;
	u8 chrgtime_charging;
	u8 tchthr_batt;
	u8 tchthr_charging;
	u8 actvsyncsperx_batt;
	u8 actvsyncsperx_charging;
	u8 calcfg_batt_e;
	u8 calcfg_charging_e;
	u8 atchfrccalthr_e;
	u8 atchfrccalratio_e;
	const u8 *t48_config_batt_e;
	const u8 *t48_config_chrg_e;
	void (*power_on) (struct device *);
	void (*power_off) (struct device *);
	void (*power_on_with_oleddet) (void);
	void (*power_off_with_oleddet) (void);
	void (*register_cb) (void *);
	void (*read_ta_status) (void *);
};

enum {
	MXT_PAGE_UP = 0x01,
	MXT_PAGE_DOWN = 0x02,
	MXT_DELTA_MODE = 0x10,
	MXT_REFERENCE_MODE = 0x11,
	MXT_CTE_MODE = 0x31
};

int get_tsp_status(void);

#define GPIO_LEVEL_LOW	0

static struct charging_status_callbacks {
	void (*tsp_set_charging_cable) (int type);
} charging_cbs;

bool is_cable_attached;

static void tsp_register_callback(void *function)
{
	charging_cbs.tsp_set_charging_cable = function;
}

static void tsp_read_ta_status(void *ta_status)
{
	*(bool *)ta_status = is_cable_attached;
}

static void mxt540e_power_on(struct device *dev)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(dev, "tsp_on");
	if (IS_ERR(pinctrl))
		pr_err("touch pin not configured for power on\n");
}

static void mxt540e_power_off(struct device *dev)
{
	struct mxt540e_platform_data *pdata = dev->platform_data;
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(dev, "tsp_off");
	if (IS_ERR(pinctrl))
		pr_err("touch pin not configured for power off\n");

	gpio_direction_output(pdata->gpio_read_done, GPIO_LEVEL_LOW);
}

#define MXT540E_MAX_MT_FINGERS          10
#define MXT540E_CHRGTIME_BATT           48
#define MXT540E_CHRGTIME_CHRG           48
#define MXT540E_THRESHOLD_BATT          50
#define MXT540E_THRESHOLD_CHRG          40
#define MXT540E_ACTVSYNCSPERX_BATT              24
#define MXT540E_ACTVSYNCSPERX_CHRG              28
#define MXT540E_CALCFG_BATT             98
#define MXT540E_CALCFG_CHRG             114
#define MXT540E_ATCHFRCCALTHR_WAKEUP            8
#define MXT540E_ATCHFRCCALRATIO_WAKEUP          180
#define MXT540E_ATCHFRCCALTHR_NORMAL            40
#define MXT540E_ATCHFRCCALRATIO_NORMAL          55

static u8 t7_config_e[] = { GEN_POWERCONFIG_T7,
	48, 255, 50
};

static u8 t8_config_e[] = { GEN_ACQUISITIONCONFIG_T8,
	MXT540E_CHRGTIME_BATT, 0, 5, 1, 0, 0, 4, 20,
	MXT540E_ATCHFRCCALTHR_WAKEUP, MXT540E_ATCHFRCCALRATIO_WAKEUP
};

static u8 t9_config_e[] = { TOUCH_MULTITOUCHSCREEN_T9,
	131, 0, 0, 16, 26, 0, 192, MXT540E_THRESHOLD_BATT, 2, 6,
	10, 10, 10, 80, MXT540E_MAX_MT_FINGERS, 20, 40, 20, 31, 3,
	255, 4, 3, 3, 2, 2, 136, 60, 136, 40,
	18, 15, 0, 0, 0
};

static u8 t15_config_e[] = { TOUCH_KEYARRAY_T15,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t18_config_e[] = { SPT_COMCONFIG_T18,
	0, 0
};

static u8 t19_config_e[] = { SPT_GPIOPWM_T19,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t24_config_e[] = { PROCI_ONETOUCHGESTUREPROCESSOR_T24,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t25_config_e[] = { SPT_SELFTEST_T25,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t27_config_e[] = { PROCI_TWOTOUCHGESTUREPROCESSOR_T27,
	0, 0, 0, 0, 0, 0, 0
};

static u8 t40_config_e[] = { PROCI_GRIPSUPPRESSION_T40,
	0, 0, 0, 0, 0
};

static u8 t42_config_e[] = { PROCI_TOUCHSUPPRESSION_T42,
	0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t43_config_e[] = { SPT_DIGITIZER_T43,
	0, 0, 0, 0, 0, 0, 0
};

static u8 t46_config_e[] = { SPT_CTECONFIG_T46,
	0, 0, 16, MXT540E_ACTVSYNCSPERX_BATT, 0, 0, 1, 0
};

static u8 t47_config_e[] = { PROCI_STYLUS_T47,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t48_config_e[] = { PROCG_NOISESUPPRESSION_T48,
	3, 132, MXT540E_CALCFG_BATT, 0, 0, 0, 0, 0, 1, 2,
	0, 0, 0, 6, 6, 0, 0, 28, 4, 64,
	10, 0, 20, 6, 0, 30, 0, 0, 0, 0,
	0, 0, 0, 0, 192, MXT540E_THRESHOLD_BATT, 2, 10, 10, 47,
	MXT540E_MAX_MT_FINGERS, 5, 20, 253, 0, 7, 7, 160, 55, 136,
	0, 18, 5, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};

static u8 t48_config_chrg_e[] = { PROCG_NOISESUPPRESSION_T48,
	3, 132, MXT540E_CALCFG_CHRG, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 6, 6, 0, 0, 36, 4, 64,
	10, 0, 10, 6, 0, 20, 0, 0, 0, 0,
	0, 0, 0, 0, 112, MXT540E_THRESHOLD_CHRG, 2, 10, 5, 47,
	MXT540E_MAX_MT_FINGERS, 5, 20, 253, 0, 7, 7, 160, 55, 136,
	0, 18, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};

static u8 t52_config_e[] = { TOUCH_PROXKEY_T52,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static u8 t55_config_e[] = {ADAPTIVE_T55,
	0, 0, 0, 0, 0, 0
};

static u8 t57_config_e[] = {SPT_GENERICDATA_T57,
	243, 25, 1
};

static u8 t61_config_e[] = {SPT_TIMER_T61,
	0, 0, 0, 0, 0
};

static u8 end_config_e[] = { RESERVED_T255 };

static const u8 *mxt540e_config[] = {
	t7_config_e,
	t8_config_e,
	t9_config_e,
	t15_config_e,
	t18_config_e,
	t19_config_e,
	t24_config_e,
	t25_config_e,
	t27_config_e,
	t40_config_e,
	t42_config_e,
	t43_config_e,
	t46_config_e,
	t47_config_e,
	t48_config_e,
	t52_config_e,
	t55_config_e,
	t57_config_e,
	t61_config_e,
	end_config_e,
};
#endif
