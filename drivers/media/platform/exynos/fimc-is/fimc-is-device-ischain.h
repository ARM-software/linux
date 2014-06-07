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

#ifndef FIMC_IS_DEVICE_ISCHAIN_H
#define FIMC_IS_DEVICE_ISCHAIN_H

#include <linux/pm_qos.h>

#include "fimc-is-mem.h"
#include "fimc-is-subdev-ctrl.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-resourcemgr.h"

#define SENSOR_MAX_CTL			0x10
#define SENSOR_MAX_CTL_MASK		(SENSOR_MAX_CTL-1)

#define REPROCESSING_FLAG		0x80000000
#define REPROCESSING_MASK		0xF0000000
#define REPROCESSING_SHIFT		28
#define OTF_3AA_MASK			0x0F000000
#define OTF_3AA_SHIFT			24
#define SSX_VINDEX_MASK			0x00FF0000
#define SSX_VINDEX_SHIFT		16
#define TAX_VINDEX_MASK			0x0000FF00
#define TAX_VINDEX_SHIFT		8
#define MODULE_MASK			0x000000FF

#define FIMC_IS_SETFILE_MASK		0x0000FFFF
#define FIMC_IS_ISP_CRANGE_MASK		0x0F000000
#define FIMC_IS_ISP_CRANGE_SHIFT	24
#define FIMC_IS_SCC_CRANGE_MASK		0x00F00000
#define FIMC_IS_SCC_CRANGE_SHIFT	20
#define FIMC_IS_SCP_CRANGE_MASK		0x000F0000
#define FIMC_IS_SCP_CRANGE_SHIFT	16
#define FIMC_IS_CRANGE_FULL		0
#define FIMC_IS_CRANGE_LIMITED		1

/*global state*/
enum fimc_is_ischain_state {
	FIMC_IS_ISCHAIN_OPEN,
	FIMC_IS_ISCHAIN_LOADED,
	FIMC_IS_ISCHAIN_POWER_ON,
	FIMC_IS_ISCHAIN_OPEN_SENSOR,
	FIMC_IS_ISHCAIN_START,
	FIMC_IS_ISCHAIN_REPROCESSING,
};

enum fimc_is_camera_device {
	CAMERA_SINGLE_REAR,
	CAMERA_SINGLE_FRONT,
};

struct fimc_is_from_info {
	u32		bin_start_addr;
	u32		bin_end_addr;
	u32		oem_start_addr;
	u32		oem_end_addr;
	u32		awb_start_addr;
	u32		awb_end_addr;
	u32		shading_start_addr;
	u32		shading_end_addr;
	u32		setfile_start_addr;
	u32		setfile_end_addr;

	char		header_ver[12];
	char		cal_map_ver[4];
	char		setfile_ver[7];
	char		oem_ver[12];
	char		awb_ver[12];
	char		shading_ver[12];
};


struct fimc_is_ishcain_mem {
	/* buffer base */
	dma_addr_t		base;
	/* total length */
	size_t			size;
	/* buffer base */
	dma_addr_t		vaddr_base;
	/* current addr */
	dma_addr_t		vaddr_curr;
	void			*fw_cookie;

	/* fw memory base */
	u32			dvaddr;
	u32			kvaddr;
	/* debug part of fw memory */
	u32			dvaddr_debug;
	u32			kvaddr_debug;
	/* is region part of fw memory */
	u32			offset_region;
	u32			dvaddr_region;
	u32			kvaddr_region;
	/* shared part of is region */
	u32			offset_shared;
	u32			dvaddr_shared;
	u32			kvaddr_shared;
	/* internal memory for ODC */
	u32			dvaddr_odc;
	u32			kvaddr_odc;
	/* internal memory for DIS */
	u32			dvaddr_dis;
	u32			kvaddr_dis;
	/* internal memory for 3DNR */
	u32			dvaddr_3dnr;
	u32			kvaddr_3dnr;

	struct is_region	*is_region;
};

struct fimc_is_device_ischain {
	struct platform_device			*pdev;
	struct exynos_platform_fimc_is		*pdata;
	void __iomem				*regs;

	struct fimc_is_resourcemgr		*resourcemgr;
	struct fimc_is_groupmgr			*groupmgr;
	struct fimc_is_interface		*interface;
	struct fimc_is_mem			*mem;

	u32					instance;
	u32					instance_sensor;
	u32					module;
	struct fimc_is_ishcain_mem		imemory;
	struct fimc_is_from_info		finfo;
	struct fimc_is_from_info		pinfo;
	struct is_region			*is_region;

	bool					force_down;
	unsigned long				state;
	struct mutex				mutex_state;

	u32					dzoom_width;
	u32					bds_width;
	u32					bds_height;
	u32					setfile;
	u32					scp_setfile;

	struct camera2_sm			capability;
	struct camera2_uctl			cur_peri_ctl;
	struct camera2_uctl			peri_ctls[SENSOR_MAX_CTL];

	/*fimc bns*/
	u32					bns_width;
	u32					bns_height;

	/*isp margin*/
	u32					sensor_width;
	u32					sensor_height;
	u32					margin_left;
	u32					margin_right;
	u32					margin_width;
	u32					margin_top;
	u32					margin_bottom;
	u32					margin_height;

	/* chain0 : isp ~ scc */
	struct fimc_is_group			group_3aa;
	struct fimc_is_subdev			taac;
	struct fimc_is_subdev			taap;

	struct fimc_is_group			group_isp;
	u32					chain0_width;
	u32					chain0_height;
	struct fimc_is_subdev			drc;

	/* chain1 : scc ~ dis */
	struct fimc_is_subdev			scc;
	u32					chain1_width;
	u32					chain1_height;
	u32					crop_x;
	u32					crop_y;
	u32					crop_width;
	u32					crop_height;
	struct fimc_is_subdev			dis;
	u32					dis_width;
	u32					dis_height;

	/* chain2 : dis ~ scp */
	struct fimc_is_group			group_dis;
	u32					chain2_width;
	u32					chain2_height;
	struct fimc_is_subdev			dnr;

	/* chain3 : scp ~ fd */
	struct fimc_is_subdev			scp;
	u32					chain3_width;
	u32					chain3_height;
	struct fimc_is_subdev			fd;

	u32					private_data;
	struct fimc_is_device_sensor		*sensor;
	struct pm_qos_request			user_qos;
};

/*global function*/
int fimc_is_ischain_probe(struct fimc_is_device_ischain *device,
	struct fimc_is_interface *interface,
	struct fimc_is_resourcemgr *resourcemgr,
	struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_mem *mem,
	struct platform_device *pdev,
	u32 instance,
	u32 regs);
int fimc_is_ischain_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx,
	struct fimc_is_minfo *minfo);
int fimc_is_ischain_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_init(struct fimc_is_device_ischain *device,
	u32 module_id,
	u32 group_id,
	u32 video_id,
	u32 flag);
int fimc_is_ischain_g_capability(struct fimc_is_device_ischain *this,
	u32 user_ptr);
int fimc_is_ischain_print_status(struct fimc_is_device_ischain *this);
void fimc_is_ischain_meta_invalid(struct fimc_is_frame *frame);

/* 3AA subdev */
int fimc_is_ischain_3aa_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_3aa_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_3aa_s_input(struct fimc_is_device_ischain *device,
	u32 input);
int fimc_is_ischain_3aa_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue);
int fimc_is_ischain_3aa_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue);
int fimc_is_ischain_3aa_reqbufs(struct fimc_is_device_ischain *device,
	u32 count);
int fimc_is_ischain_3aa_s_format(struct fimc_is_device_ischain *device,
	u32 width, u32 height);
int fimc_is_ischain_3aa_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index);
int fimc_is_ischain_3aa_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index);

/* isp subdev */
int fimc_is_ischain_isp_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue);
int fimc_is_ischain_isp_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue);
int fimc_is_ischain_isp_reqbufs(struct fimc_is_device_ischain *device,
	u32 count);
int fimc_is_ischain_isp_s_format(struct fimc_is_device_ischain *this,
	u32 width, u32 height);
int fimc_is_ischain_isp_s_input(struct fimc_is_device_ischain *this,
	u32 input);
int fimc_is_ischain_isp_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index);
int fimc_is_ischain_isp_buffer_finish(struct fimc_is_device_ischain *this,
	u32 index);

/*scc subdev*/
/*scp subdev*/
int fimc_is_ischain_scp_s_format(struct fimc_is_device_ischain *device,
	u32 pixelformat, u32 width, u32 height);

/* vdisc subdev */
int fimc_is_ischain_dis_start(struct fimc_is_device_ischain *this,
	bool bypass);
int fimc_is_ischain_dis_stop(struct fimc_is_device_ischain *this);
int fimc_is_ischain_vdc_s_format(struct fimc_is_device_ischain *this,
	u32 width, u32 height);

/* vdiso subdev */
int fimc_is_ischain_vdo_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_vdo_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_vdo_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue);
int fimc_is_ischain_vdo_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue);
int fimc_is_ischain_vdo_s_format(struct fimc_is_device_ischain *this,
	u32 width, u32 height);
int fimc_is_ischain_vdo_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index);
int fimc_is_ischain_vdo_buffer_finish(struct fimc_is_device_ischain *this,
	u32 index);

/*special api for sensor*/
int fimc_is_ischain_3aa_callback(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);
int fimc_is_ischain_isp_callback(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);
int fimc_is_ischain_dis_callback(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);
int fimc_is_ischain_camctl(struct fimc_is_device_ischain *this,
	struct fimc_is_frame *frame,
	u32 fcount);
int fimc_is_ischain_tag(struct fimc_is_device_ischain *ischain,
	struct fimc_is_frame *frame);

int fimc_is_itf_stream_on(struct fimc_is_device_ischain *this);
int fimc_is_itf_stream_off(struct fimc_is_device_ischain *this);
int fimc_is_itf_process_start(struct fimc_is_device_ischain *device,
	u32 group);
int fimc_is_itf_process_stop(struct fimc_is_device_ischain *device,
	u32 group);
int fimc_is_itf_force_stop(struct fimc_is_device_ischain *device,
	u32 group);
int fimc_is_itf_map(struct fimc_is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size);
int fimc_is_itf_i2c_lock(struct fimc_is_device_ischain *this,
			int i2c_clk, bool lock);

extern const struct fimc_is_queue_ops fimc_is_ischain_3aa_ops;
extern const struct fimc_is_queue_ops fimc_is_ischain_isp_ops;
extern const struct fimc_is_queue_ops fimc_is_ischain_vdo_ops;
extern const struct fimc_is_queue_ops fimc_is_ischain_sub_ops;

int fimc_is_itf_power_down(struct fimc_is_interface *interface);
int fimc_is_ischain_power(struct fimc_is_device_ischain *this, int on);
void fimc_is_ischain_savefirm(struct fimc_is_device_ischain *this);

#define IS_ISCHAIN_OTF(device)				\
	(test_bit(FIMC_IS_GROUP_OTF_INPUT, &(device)->group_3aa.state))
#define IS_EQUAL_COORD(i, o)				\
	(((i)[0] != (o)[0]) || ((i)[1] != (o)[1]) ||	\
	 ((i)[2] != (o)[2]) || ((i)[3] != (o)[3]))
#define IS_NULL_COORD(c)				\
	(!(c)[0] && !(c)[1] && !(c)[2] && !(c)[3])
#endif
