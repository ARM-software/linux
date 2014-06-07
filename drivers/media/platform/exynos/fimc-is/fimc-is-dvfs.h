/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DVFS_H
#define FIMC_IS_DVFS_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "fimc-is-time.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-device-ischain.h"

#define KEEP_FRAME_TICK_DEFAULT (5)
#define DVFS_SN_STR(__SCENARIO) #__SCENARIO
#define GET_DVFS_CHK_FUNC(__SCENARIO) check_ ## __SCENARIO
#define DECLARE_DVFS_CHK_FUNC(__SCENARIO) \
	int check_ ## __SCENARIO \
		(struct fimc_is_device_ischain *device, ...)

#define SIZE_FHD (1920 * 1080)
#define SIZE_WHD (2560 * 1440)
#define SIZE_UHD (3840 * 2160)

enum FIMC_IS_DVFS_SCENARIO_TYPE {
	FIMC_IS_STATIC_SN,
	FIMC_IS_DYNAMIC_SN,
};

struct fimc_is_dvfs_scenario {
	u32 scenario_id;	/* scenario_id */
	char *scenario_nm;	/* string of scenario_id */
	int priority;		/* priority for dynamic scenario */
	int keep_frame_tick;	/* keep qos lock during specific frames when dynamic scenario */

	/* function pointer to check a scenario */
	int (*check_func)(struct fimc_is_device_ischain *device, ...);
};

struct fimc_is_dvfs_scenario_ctrl {
	int cur_scenario_id;	/* selected scenario idx */
	int cur_frame_tick;	/* remained frame tick to keep qos lock in dynamic scenario */
	int scenario_cnt;	/* total scenario count */
	int cur_scenario_idx;	/* selected scenario idx for scenarios */
	struct fimc_is_dvfs_scenario *scenarios;
};

int fimc_is_dvfs_init(struct fimc_is_resourcemgr *resourcemgr);
int fimc_is_dvfs_sel_scenario(u32 type,	struct fimc_is_device_ischain *device);
int fimc_is_get_qos(struct fimc_is_core *core, u32 type, u32 scenario_id);
int fimc_is_set_dvfs(struct fimc_is_device_ischain *device, u32 scenario_id);
#endif
