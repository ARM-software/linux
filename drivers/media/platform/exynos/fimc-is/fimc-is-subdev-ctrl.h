/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SUBDEV_H
#define FIMC_IS_SUBDEV_H

#include "fimc-is-param.h"
#include "fimc-is-video.h"

struct fimc_is_group;
struct fimc_is_device_ischain;

enum fimc_is_subdev_state {
	FIMC_IS_SUBDEV_OPEN,
	FIMC_IS_SUBDEV_START
};

struct fimc_is_subdev_path {
	u32					width;
	u32					height;
};

struct fimc_is_subdev {
	u32					id;
	u32					entry;
	unsigned long				state;
	struct mutex				mutex_state;

	struct fimc_is_subdev_path		input;
	struct fimc_is_subdev_path		output;

	struct fimc_is_group			*group;
	struct fimc_is_video_ctx		*vctx;
	struct fimc_is_subdev			*leader;
};

#define GET_LEADER_FRAMEMGR(leader) \
	(((leader) && (leader)->vctx) ? (&(leader)->vctx->q_src.framemgr) : NULL)
#define GET_SUBDEV_FRAMEMGR(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->q_dst.framemgr) : NULL)

#define GET_LEADER_QUEUE(leader) \
	(((leader) && (leader)->vctx) ? (&(leader)->vctx->q_src) : NULL)
#define GET_SUBDEV_QUEUE(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->q_dst) : NULL)

/*common subdev*/
int fimc_is_subdev_open(struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx,
	const struct param_control *init_ctl);
int fimc_is_subdev_close(struct fimc_is_subdev *subdev);
int fimc_is_subdev_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue);
int fimc_is_subdev_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue);
int fimc_is_subdev_s_format(struct fimc_is_subdev *subdev,
	u32 width, u32 height);
int fimc_is_subdev_buffer_queue(struct fimc_is_subdev *subdev,
	u32 index);
int fimc_is_subdev_buffer_finish(struct fimc_is_subdev *subdev,
	u32 index);

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_drc_start(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_drc_bypass(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes);
#endif
