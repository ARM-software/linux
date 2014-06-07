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

#ifndef FIMC_IS_RESOURCE_MGR_H
#define FIMC_IS_RESOURCE_MGR_H

#include "fimc-is-groupmgr.h"

struct fimc_is_dvfs_ctrl {
	struct mutex lock;
	int cur_int_qos;
	int cur_mif_qos;
	int cur_cam_qos;
	int cur_i2c_qos;
	int cur_disp_qos;

	struct fimc_is_dvfs_scenario_ctrl *static_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *dynamic_ctrl;
};

struct fimc_is_clk_gate_ctrl {
	spinlock_t lock;
	unsigned long msk_state;
	int msk_cnt[GROUP_ID_MAX];
	u32 msk_lock_by_ischain[FIMC_IS_MAX_NODES];
	struct exynos_fimc_is_clk_gate_info *gate_info;
	u32 msk_clk_on_off_state; /* on/off(1/0) state per ip */
	/*
	 * For check that there's too long clock-on period.
	 * This var will increase when clock on,
	 * And will decrease when clock off.
	 */
	unsigned long chk_on_off_cnt[GROUP_ID_MAX];
};

struct fimc_is_resourcemgr {
	atomic_t				rsccount;
	atomic_t				rsccount_sensor;
	atomic_t				rsccount_ischain;
	atomic_t				rsccount_module; /* sensor module */

	struct fimc_is_dvfs_ctrl		dvfs_ctrl;
	struct fimc_is_clk_gate_ctrl		clk_gate_ctrl;

	void					*private_data;
};

int fimc_is_resource_probe(struct fimc_is_resourcemgr *resourcemgr,
	void *private_data);
int fimc_is_resource_get(struct fimc_is_resourcemgr *resourcemgr);
int fimc_is_resource_put(struct fimc_is_resourcemgr *resourcemgr);
#endif
