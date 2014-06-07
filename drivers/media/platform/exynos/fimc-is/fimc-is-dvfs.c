/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"

extern struct pm_qos_request exynos_isp_qos_int;
extern struct pm_qos_request exynos_isp_qos_mem;
extern struct pm_qos_request exynos_isp_qos_cam;
extern struct pm_qos_request exynos_isp_qos_disp;

DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_HIGH_SPEED_FPS);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE);
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE);

#if defined(ENABLE_DVFS)
/*
 * Static Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'static_scenarios'
 */
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DUAL_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAPTURE),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_CAMCORDING,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_CAMCORDING),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING),
	}, {
		.scenario_id		= FIMC_IS_SN_DUAL_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DUAL_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW),
	}, {
		.scenario_id		= FIMC_IS_SN_HIGH_SPEED_FPS,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_HIGH_SPEED_FPS),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_HIGH_SPEED_FPS),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_CAMCORDING_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAMCORDING_UHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_FHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_FHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_WHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_WHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD),
	}, {
		.scenario_id		= FIMC_IS_SN_REAR_PREVIEW_UHD,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_PREVIEW_UHD),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_VT1,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_VT1),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1),
	}, {
		.scenario_id		= FIMC_IS_SN_FRONT_PREVIEW,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_FRONT_PREVIEW),
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW),
	},
};

/*
 * Dynamic Scenario Set
 * You should describe static scenario by priorities of scenario.
 * And you should name array 'dynamic_scenarios'
 */
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_REAR_CAPTURE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_REAR_CAPTURE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE),
	},
	{
		.scenario_id		= FIMC_IS_SN_DIS_ENABLE,
		.scenario_nm		= DVFS_SN_STR(FIMC_IS_SN_DIS_ENABLE),
		.keep_frame_tick	= KEEP_FRAME_TICK_DEFAULT,
		.check_func		= GET_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE),
	},
};
#else
/*
 * Default Scenario can not be seleted, this declaration is for static variable.
 */
static struct fimc_is_dvfs_scenario static_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.check_func		= NULL,
	},
};
static struct fimc_is_dvfs_scenario dynamic_scenarios[] = {
	{
		.scenario_id		= FIMC_IS_SN_DEFAULT,
		.scenario_nm		= NULL,
		.keep_frame_tick	= 0,
		.check_func		= NULL,
	},
};
#endif

static inline int fimc_is_get_open_sensor_cnt(struct fimc_is_core *core) {
	int i, sensor_cnt = 0;

	for (i = 0; i < FIMC_IS_MAX_NODES; i++)
		if (test_bit(FIMC_IS_SENSOR_OPEN, &(core->sensor[i].state)))
			sensor_cnt++;

	return sensor_cnt;
}

/* dual capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAPTURE)
{
	struct fimc_is_core *core;
	int sensor_cnt = 0;
	core = (struct fimc_is_core *)device->interface->core;
	sensor_cnt = fimc_is_get_open_sensor_cnt(core);

	if ((device->chain0_width > 2560) && (sensor_cnt >= 2))
		return 1;
	else
		return 0;
}

/* dual camcording */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_CAMCORDING)
{
	struct fimc_is_core *core;
	int sensor_cnt = 0;
	core = (struct fimc_is_core *)device->interface->core;
	sensor_cnt = fimc_is_get_open_sensor_cnt(core);

	if ((device->chain0_width <= 2560) && (sensor_cnt >= 2) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* dual preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DUAL_PREVIEW)
{
	struct fimc_is_core *core;
	int sensor_cnt = 0;
	core = (struct fimc_is_core *)device->interface->core;
	sensor_cnt = fimc_is_get_open_sensor_cnt(core);

	if ((device->chain0_width <= 2560) && (sensor_cnt >= 2) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* high speed fps */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_HIGH_SPEED_FPS)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) > 30))
		return 1;
	else
		return 0;
}

/* rear camcording FHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_FHD)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->chain3_width * device->chain3_height <= SIZE_FHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* rear camcording UHD*/
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAMCORDING_UHD)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->chain3_width * device->chain3_height > SIZE_FHD) &&
			(device->chain3_width * device->chain3_height <= SIZE_UHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* rear preview FHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_FHD)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->chain3_width * device->chain3_height <= SIZE_FHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))

		return 1;
	else
		return 0;
}

/* rear preview WHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_WHD)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->chain3_width * device->chain3_height > SIZE_FHD) &&
			(device->chain3_width * device->chain3_height <= SIZE_WHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* rear preview UHD */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_PREVIEW_UHD)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(fimc_is_sensor_g_framerate(device->sensor) <= 30) &&
			(device->chain3_width * device->chain3_height > SIZE_WHD) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 != ISS_SUB_SCENARIO_VIDEO))
		return 1;
	else
		return 0;
}

/* front vt1 */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_VT1)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_FRONT) &&
			((device->setfile & FIMC_IS_SETFILE_MASK) \
			 == ISS_SUB_SCENARIO_FRONT_VT1))
		return 1;
	else
		return 0;
}

/* front preview */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_FRONT_PREVIEW)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_FRONT) &&
			!(((device->setfile & FIMC_IS_SETFILE_MASK) \
					== ISS_SUB_SCENARIO_FRONT_VT1) ||
				((device->setfile & FIMC_IS_SETFILE_MASK) \
				 == ISS_SUB_SCENARIO_FRONT_VT2)))
		return 1;
	else
		return 0;
}

/* rear capture */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_REAR_CAPTURE)
{
	if ((device->sensor->pdev->id == SENSOR_POSITION_REAR) &&
			(test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)))
		return 1;
	else
		return 0;
}

/* dis */
DECLARE_DVFS_CHK_FUNC(FIMC_IS_SN_DIS_ENABLE)
{
	if (test_bit(FIMC_IS_SUBDEV_START, &device->dis.state))
		return 1;
	else
		return 0;
}

static int fimc_is_set_pwm(struct fimc_is_device_ischain *device, u32 pwm_qos)
{
	int ret = 0;
	u32 base_addr;
	void __iomem *addr;

	base_addr = GET_FIMC_IS_ADDR_OF_SUBIP2(device, pwm);

	if (base_addr) {
		addr = ioremap(base_addr + FIMC_IS_PWM_TCNTB0, SZ_4);
		writel(pwm_qos, addr);
		dbg("PWM SFR Read(%08X), pwm_qos(%08X)\n", readl(addr), pwm_qos);
		iounmap(addr);
	}

	return ret;
}

int fimc_is_dvfs_init(struct fimc_is_resourcemgr *resourcemgr)
{
	int i;
	pr_info("%s\n",	__func__);

	BUG_ON(!resourcemgr);

	resourcemgr->dvfs_ctrl.cur_int_qos = 0;
	resourcemgr->dvfs_ctrl.cur_mif_qos = 0;
	resourcemgr->dvfs_ctrl.cur_cam_qos = 0;
	resourcemgr->dvfs_ctrl.cur_i2c_qos = 0;
	resourcemgr->dvfs_ctrl.cur_disp_qos = 0;

	/* init spin_lock for clock gating */
	mutex_init(&resourcemgr->dvfs_ctrl.lock);

	if (!(resourcemgr->dvfs_ctrl.static_ctrl))
		resourcemgr->dvfs_ctrl.static_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);
	if (!(resourcemgr->dvfs_ctrl.dynamic_ctrl))
		resourcemgr->dvfs_ctrl.dynamic_ctrl =
			kzalloc(sizeof(struct fimc_is_dvfs_scenario_ctrl), GFP_KERNEL);

	if (!resourcemgr->dvfs_ctrl.static_ctrl || !resourcemgr->dvfs_ctrl.dynamic_ctrl) {
		err("dvfs_ctrl alloc is failed!!\n");
		return -ENOMEM;
	}

	/* set priority by order */
	for (i = 0; i < ARRAY_SIZE(static_scenarios); i++)
		static_scenarios[i].priority = i;
	for (i = 0; i < ARRAY_SIZE(dynamic_scenarios); i++)
		dynamic_scenarios[i].priority = i;

	resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_id	= -1;
	resourcemgr->dvfs_ctrl.static_ctrl->cur_scenario_idx	= -1;
	resourcemgr->dvfs_ctrl.static_ctrl->scenarios		= static_scenarios;
	if (static_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		resourcemgr->dvfs_ctrl.static_ctrl->scenario_cnt	= 0;
	else
		resourcemgr->dvfs_ctrl.static_ctrl->scenario_cnt	= ARRAY_SIZE(static_scenarios);

	resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_scenario_id	= -1;
	resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_scenario_idx	= -1;
	resourcemgr->dvfs_ctrl.dynamic_ctrl->cur_frame_tick	= -1;
	resourcemgr->dvfs_ctrl.dynamic_ctrl->scenarios		= dynamic_scenarios;
	if (static_scenarios[0].scenario_id == FIMC_IS_SN_DEFAULT)
		resourcemgr->dvfs_ctrl.dynamic_ctrl->scenario_cnt	= 0;
	else
		resourcemgr->dvfs_ctrl.dynamic_ctrl->scenario_cnt	= ARRAY_SIZE(dynamic_scenarios);

	return 0;
}

int fimc_is_dvfs_sel_scenario(u32 type, struct fimc_is_device_ischain *device)
{
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;
	struct fimc_is_dvfs_scenario_ctrl *static_ctrl, *dynamic_ctrl;
	struct fimc_is_dvfs_scenario *scenarios;
	struct fimc_is_dvfs_scenario *cur_scenario;
	struct fimc_is_resourcemgr *resourcemgr;
	int i, scenario_id, scenario_cnt;

	if (device == NULL) {
		err("device is NULL\n");
		return -EINVAL;
	}

	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
	static_ctrl = dvfs_ctrl->static_ctrl;
	dynamic_ctrl = dvfs_ctrl->dynamic_ctrl;

	if (type == FIMC_IS_DYNAMIC_SN) {
		/* dynamic scenario */
		if (!dynamic_ctrl) {
			err("dynamic_dvfs_ctrl is NULL\n");
			return -EINVAL;
		}

		if (dynamic_ctrl->scenario_cnt == 0) {
			pr_debug("dynamic_scenario's count is jero\n");
			return -EINVAL;
		}

		scenarios = dynamic_ctrl->scenarios;
		scenario_cnt = dynamic_ctrl->scenario_cnt;

		if (dynamic_ctrl->cur_frame_tick >= 0) {
			(dynamic_ctrl->cur_frame_tick)--;
			/*
			 * when cur_frame_tick is lower than 0, clear current scenario.
			 * This means that current frame tick to keep dynamic scenario
			 * was expired.
			 */
			if (dynamic_ctrl->cur_frame_tick < 0) {
				dynamic_ctrl->cur_scenario_id = -1;
				dynamic_ctrl->cur_scenario_idx = -1;
			}
		}
	} else {
		/* static scenario */
		if (!static_ctrl) {
			err("static_dvfs_ctrl is NULL\n");
			return -EINVAL;
		}

		if (static_ctrl->scenario_cnt == 0) {
			pr_debug("static_scenario's count is jero\n");
			return -EINVAL;
		}

		scenarios = static_ctrl->scenarios;
		scenario_cnt = static_ctrl->scenario_cnt;
	}

	for (i = 0; i < scenario_cnt; i++) {
		if (!scenarios[i].check_func) {
			warn("check_func[%d] is NULL\n", i);
			continue;
		}

		if ((scenarios[i].check_func(device)) > 0) {
			scenario_id = scenarios[i].scenario_id;

			if (type == FIMC_IS_DYNAMIC_SN) {
				cur_scenario = &scenarios[dynamic_ctrl->cur_scenario_idx];

				/*
				 * if condition 1 or 2 is true
				 * condition 1 : There's no dynamic scenario applied.
				 * condition 2 : Finded scenario's prority was higher than current
				 */
				if ((dynamic_ctrl->cur_scenario_id <= 0) ||
						(scenarios[i].priority < (cur_scenario->priority))) {
					dynamic_ctrl->cur_scenario_id = scenarios[i].scenario_id;
					dynamic_ctrl->cur_scenario_idx = i;
					dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
				} else {
					/* if finded scenario is same */
					if (scenarios[i].priority == (cur_scenario->priority))
						dynamic_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
					return -EAGAIN;
				}
			} else {
				static_ctrl->cur_scenario_id = scenario_id;
				static_ctrl->cur_scenario_idx = i;
				static_ctrl->cur_frame_tick = scenarios[i].keep_frame_tick;
			}

			return scenario_id;
		}
	}

	if (type == FIMC_IS_DYNAMIC_SN)
		return -EAGAIN;

	static_ctrl->cur_scenario_id = FIMC_IS_SN_DEFAULT;
	static_ctrl->cur_scenario_idx = -1;
	static_ctrl->cur_frame_tick = -1;

	return FIMC_IS_SN_DEFAULT;
}

int fimc_is_get_qos(struct fimc_is_core *core, u32 type, u32 scenario_id)
{
	struct exynos_platform_fimc_is	*pdata = NULL;
	int qos = 0;

	pdata = core->pdata;
	if (pdata == NULL) {
		err("pdata is NULL\n");
		return -EINVAL;
	}

	if (!pdata->get_int_qos || !pdata->get_mif_qos)
		goto struct_qos;

	switch (type) {
		case FIMC_IS_DVFS_INT:
			qos = pdata->get_int_qos(scenario_id);
			break;
		case FIMC_IS_DVFS_MIF:
			qos = pdata->get_mif_qos(scenario_id);
			break;
		case FIMC_IS_DVFS_I2C:
			if (pdata->get_i2c_qos)
				qos = pdata->get_i2c_qos(scenario_id);
			break;
	}
	goto exit;

struct_qos:
	if (max(0, (int)type) >= FIMC_IS_DVFS_END) {
		err("Cannot find DVFS value");
		return -EINVAL;
	}

	qos = pdata->dvfs_data[scenario_id][type];

exit:
	return qos;
}

int fimc_is_set_dvfs(struct fimc_is_device_ischain *device, u32 scenario_id)
{
	int ret = 0;
	int int_qos, mif_qos, i2c_qos, cam_qos, disp_qos, pwm_qos = 0;
	int refcount;
	struct fimc_is_core *core;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_dvfs_ctrl *dvfs_ctrl;

	if (device == NULL) {
		err("device is NULL\n");
		return -EINVAL;
	}

	core = (struct fimc_is_core *)device->interface->core;
	resourcemgr = device->resourcemgr;
	dvfs_ctrl = &(resourcemgr->dvfs_ctrl);

	refcount = atomic_read(&core->video_isp.refcount);
	if (refcount < 0) {
		err("invalid ischain refcount");
		goto exit;
	}

	int_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_INT, scenario_id);
	mif_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_MIF, scenario_id);
	i2c_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_I2C, scenario_id);
	cam_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_CAM, scenario_id);
	disp_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_DISP, scenario_id);
	pwm_qos = fimc_is_get_qos(core, FIMC_IS_DVFS_PWM, scenario_id);

	if ((int_qos < 0) || (mif_qos < 0) || (i2c_qos < 0)
	|| (cam_qos < 0) || (disp_qos < 0) || (pwm_qos < 0)) {
		err("getting qos value is failed!!\n");
		return -EINVAL;
	}

	/* check current qos */
	if (int_qos && dvfs_ctrl->cur_int_qos != int_qos) {
		if (i2c_qos) {
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, true);
			if (ret) {
				err("fimc_is_itf_i2_clock fail\n");
				goto exit;
			}
		}

		if (pwm_qos) {
			fimc_is_set_pwm(device, pwm_qos);
			if (ret) {
				err("fimc_is_set_pwm fail\n");
				goto exit;
			}
		}

		pm_qos_update_request(&exynos_isp_qos_int, int_qos);
		dvfs_ctrl->cur_int_qos = int_qos;

		if (i2c_qos) {
			/* i2c unlock */
			ret = fimc_is_itf_i2c_lock(device, i2c_qos, false);
			if (ret) {
				err("fimc_is_itf_i2c_unlock fail\n");
				goto exit;
			}
		}
	}

	if (mif_qos && dvfs_ctrl->cur_mif_qos != mif_qos) {
		pm_qos_update_request(&exynos_isp_qos_mem, mif_qos);
		dvfs_ctrl->cur_mif_qos = mif_qos;
	}

	if (cam_qos && dvfs_ctrl->cur_cam_qos != cam_qos) {
		pm_qos_update_request(&exynos_isp_qos_cam, cam_qos);
		dvfs_ctrl->cur_cam_qos = cam_qos;
	}

	if (disp_qos && dvfs_ctrl->cur_disp_qos != disp_qos) {
		pm_qos_update_request(&exynos_isp_qos_disp, disp_qos);
		dvfs_ctrl->cur_disp_qos = disp_qos;
	}

	dbg("[RSC:%d]: New QoS [INT(%d), MIF(%d), CAM(%d), DISP(%d), I2C(%d), PWM(%d)]\n",
			device->instance, int_qos, mif_qos,
			cam_qos, disp_qos, i2c_qos, pwm_qos);
exit:
	return ret;
}
