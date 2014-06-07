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

#ifndef FIMC_IS_CORE_H
#define FIMC_IS_CORE_H

#include <linux/version.h>
#include <linux/sched.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
#include <linux/sched/rt.h>
#endif
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <mach/exynos-fimc-is.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif defined(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif

#include "fimc-is-param.h"
#include "fimc-is-interface.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-ischain.h"

#include "fimc-is-video.h"
#include "fimc-is-mem.h"

#define FIMC_IS_DRV_NAME			"exynos-fimc-is"

#define FIMC_IS_COMMAND_TIMEOUT			(3*HZ)
#define FIMC_IS_STARTUP_TIMEOUT			(3*HZ)

#define FIMC_IS_SHUTDOWN_TIMEOUT		(10*HZ)
#define FIMC_IS_FLITE_STOP_TIMEOUT		(3*HZ)

#define FIMC_IS_SENSOR_MAX_ENTITIES		(1)
#define FIMC_IS_SENSOR_PAD_SOURCE_FRONT		(0)
#define FIMC_IS_SENSOR_PADS_NUM			(1)

#define FIMC_IS_FRONT_MAX_ENTITIES		(1)
#define FIMC_IS_FRONT_PAD_SINK			(0)
#define FIMC_IS_FRONT_PAD_SOURCE_BACK		(1)
#define FIMC_IS_FRONT_PAD_SOURCE_BAYER		(2)
#define FIMC_IS_FRONT_PAD_SOURCE_SCALERC	(3)
#define FIMC_IS_FRONT_PADS_NUM			(4)

#define FIMC_IS_BACK_MAX_ENTITIES		(1)
#define FIMC_IS_BACK_PAD_SINK			(0)
#define FIMC_IS_BACK_PAD_SOURCE_3DNR		(1)
#define FIMC_IS_BACK_PAD_SOURCE_SCALERP		(2)
#define FIMC_IS_BACK_PADS_NUM			(3)

#define FIMC_IS_MAX_SENSOR_NAME_LEN		(16)

#define FIMC_IS_A5_MEM_SIZE		(0x01400000)
#define FIMC_IS_REGION_SIZE		(0x00005000)
#define FIMC_IS_SETFILE_SIZE		(0x00140000)
#define FIMC_IS_DEBUG_REGION_ADDR	(0x01340000)
#define FIMC_IS_SHARED_REGION_ADDR	(0x013C0000)

#define FIMC_IS_FW_BASE_MASK		((1 << 26) - 1)

#define FW_SHARED_OFFSET		FIMC_IS_SHARED_REGION_ADDR
#define DEBUG_CNT			(0x0007D000) /* 500KB */
#define DEBUG_OFFSET			FIMC_IS_DEBUG_REGION_ADDR
#define DEBUGCTL_OFFSET			(DEBUG_OFFSET + DEBUG_CNT)

#define MAX_ODC_INTERNAL_BUF_WIDTH	(2560)  /* 4808 in HW */
#define MAX_ODC_INTERNAL_BUF_HEIGHT	(1920)  /* 3356 in HW */
#define SIZE_ODC_INTERNAL_BUF \
	(MAX_ODC_INTERNAL_BUF_WIDTH * MAX_ODC_INTERNAL_BUF_HEIGHT * 3)

#define MAX_DIS_INTERNAL_BUF_WIDTH	(2400)
#define MAX_DIS_INTERNAL_BUF_HEIGHT	(1360)
#define SIZE_DIS_INTERNAL_BUF \
	(MAX_DIS_INTERNAL_BUF_WIDTH * MAX_DIS_INTERNAL_BUF_HEIGHT * 2)

#define MAX_3DNR_INTERNAL_BUF_WIDTH	(1920)
#define MAX_3DNR_INTERNAL_BUF_HEIGHT	(1088)
#define SIZE_DNR_INTERNAL_BUF \
	(MAX_3DNR_INTERNAL_BUF_WIDTH * MAX_3DNR_INTERNAL_BUF_HEIGHT * 2)

#define NUM_ODC_INTERNAL_BUF		(2)
#define NUM_DIS_INTERNAL_BUF		(1)
#define NUM_DNR_INTERNAL_BUF		(2)

#define GATE_IP_ISP			(0)
#define GATE_IP_DRC			(1)
#define GATE_IP_FD			(2)
#define GATE_IP_SCC			(3)
#define GATE_IP_SCP			(4)
#define GATE_IP_ODC			(0)
#define GATE_IP_DIS			(1)
#define GATE_IP_DNR			(2)
#if defined(CONFIG_SOC_EXYNOS5422)
#define DVFS_L0				(600000)
#define DVFS_L1				(500000)
#define DVFS_L1_1			(480000)
#define DVFS_L1_2			(460000)
#define DVFS_L1_3			(440000)

#define DVFS_MIF_L0			(825000)
#define DVFS_MIF_L1			(728000)
#define DVFS_MIF_L2			(633000)
#define DVFS_MIF_L3			(543000)
#define DVFS_MIF_L4			(413000)
#define DVFS_MIF_L5			(275000)

#define I2C_L0				(108000000)
#define I2C_L1				(36000000)
#define I2C_L1_1			(54000000)
#define I2C_L2				(21600000)
#define DVFS_SKIP_FRAME_NUM		(5)
#elif defined(CONFIG_SOC_EXYNOS5430)
#define DVFS_L0				(600000)
#define DVFS_L1				(500000)
#define DVFS_L1_1			(480000)
#define DVFS_L1_2			(460000)
#define DVFS_L1_3			(440000)

#define DVFS_MIF_L0			(800000)
#define DVFS_MIF_L1			(733000)
#define DVFS_MIF_L2			(667000)
#define DVFS_MIF_L3			(533000)
#define DVFS_MIF_L4			(400000)
#define DVFS_MIF_L5			(266000)

#define I2C_L0				(83000000)
#define I2C_L1				(36000000)
#define I2C_L1_1			(54000000)
#define I2C_L2				(21600000)
#define DVFS_SKIP_FRAME_NUM		(5)
#elif defined(CONFIG_SOC_EXYNOS3470) || defined(CONFIG_SOC_EXYNOS5260)
#define DVFS_L0				(266000)
#define DVFS_MIF_L0			(400000)
#define I2C_L0				(108000000)
#define I2C_L1				(36000000)
#define I2C_L1_1			(54000000)
#define I2C_L2				(21600000)
#endif

#define GET_FIMC_IS_NUM_OF_SUBIP(core, subip) \
	(core->pdata->subip_info->_ ## subip.valid)
#define GET_FIMC_IS_NUM_OF_SUBIP2(device, subip) \
	(((struct fimc_is_core *)device->interface->core)->pdata->subip_info->_ ## subip.valid)
#define GET_FIMC_IS_VER_OF_SUBIP(core, subip) \
	((core)->pdata->subip_info->_##subip.version)
#define GET_FIMC_IS_VER_OF_SUBIP2(device, subip) \
	(((struct fimc_is_core *)device->interface->core)->pdata->subip_info->_ ## subip.version)
#define GET_FIMC_IS_ADDR_OF_SUBIP(core, subip) \
	((core)->pdata->subip_info->_##subip.base_addr)
#define GET_FIMC_IS_ADDR_OF_SUBIP2(device, subip) \
	(((struct fimc_is_core *)device->interface->core)->pdata->subip_info->_ ## subip.base_addr)

enum fimc_is_debug_device {
	FIMC_IS_DEBUG_MAIN = 0,
	FIMC_IS_DEBUG_EC,
	FIMC_IS_DEBUG_SENSOR,
	FIMC_IS_DEBUG_ISP,
	FIMC_IS_DEBUG_DRC,
	FIMC_IS_DEBUG_FD,
	FIMC_IS_DEBUG_SDK,
	FIMC_IS_DEBUG_SCALERC,
	FIMC_IS_DEBUG_ODC,
	FIMC_IS_DEBUG_DIS,
	FIMC_IS_DEBUG_TDNR,
	FIMC_IS_DEBUG_SCALERP
};

enum fimc_is_debug_target {
	FIMC_IS_DEBUG_UART = 0,
	FIMC_IS_DEBUG_MEMORY,
	FIMC_IS_DEBUG_DCC3
};

enum fimc_is_front_input_entity {
	FIMC_IS_FRONT_INPUT_NONE = 0,
	FIMC_IS_FRONT_INPUT_SENSOR,
};

enum fimc_is_front_output_entity {
	FIMC_IS_FRONT_OUTPUT_NONE = 0,
	FIMC_IS_FRONT_OUTPUT_BACK,
	FIMC_IS_FRONT_OUTPUT_BAYER,
	FIMC_IS_FRONT_OUTPUT_SCALERC,
};

enum fimc_is_back_input_entity {
	FIMC_IS_BACK_INPUT_NONE = 0,
	FIMC_IS_BACK_INPUT_FRONT,
};

enum fimc_is_back_output_entity {
	FIMC_IS_BACK_OUTPUT_NONE = 0,
	FIMC_IS_BACK_OUTPUT_3DNR,
	FIMC_IS_BACK_OUTPUT_SCALERP,
};

enum fimc_is_front_state {
	FIMC_IS_FRONT_ST_POWERED = 0,
	FIMC_IS_FRONT_ST_STREAMING,
	FIMC_IS_FRONT_ST_SUSPENDED,
};

enum fimc_is_clck_gate_mode {
	CLOCK_GATE_MODE_HOST = 0,
	CLOCK_GATE_MODE_FW,
};

struct fimc_is_sysfs_debug {
	unsigned int en_dvfs;
	unsigned int en_clk_gate;
	unsigned int clk_gate_mode;
};

struct fimc_is_core {
	struct platform_device			*pdev;
	struct resource				*regs_res;
	void __iomem				*regs;
	int					irq;
	u32					id;
	u32					debug_cnt;
	atomic_t				rsccount;
	unsigned long				state;

	/* depended on isp */
	struct exynos_platform_fimc_is		*pdata;

	struct fimc_is_resourcemgr		resourcemgr;
	struct fimc_is_groupmgr			groupmgr;

	struct fimc_is_minfo                    minfo;
	struct fimc_is_mem			mem;
	struct fimc_is_interface		interface;

	struct fimc_is_device_sensor		sensor[FIMC_IS_MAX_NODES];
	struct fimc_is_device_ischain		ischain[FIMC_IS_MAX_NODES];

	struct v4l2_device			v4l2_dev;

	/* 0-bayer, 1-scalerC, 2-3DNR, 3-scalerP */
	struct fimc_is_video			video_3a0;
	struct fimc_is_video			video_3a1;
	struct fimc_is_video			video_isp;
	struct fimc_is_video			video_scc;
	struct fimc_is_video			video_scp;
	struct fimc_is_video			video_vdc;
	struct fimc_is_video			video_vdo;
	struct fimc_is_video			video_3a0c;
	struct fimc_is_video			video_3a1c;

	/* spi */
	struct spi_device			*spi0;
	struct spi_device			*spi1;
	struct spi_device			*t_spi;
};

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
extern const struct fimc_is_vb2 fimc_is_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
extern const struct fimc_is_vb2 fimc_is_vb2_ion;
#endif

extern struct device *fimc_is_dev;

void fimc_is_mem_suspend(void *alloc_ctxes);
void fimc_is_mem_resume(void *alloc_ctxes);
void fimc_is_mem_cache_clean(const void *start_addr, unsigned long size);
void fimc_is_mem_cache_inv(const void *start_addr, unsigned long size);
int fimc_is_init_set(struct fimc_is_core *dev , u32 val);
int fimc_is_load_fw(struct fimc_is_core *dev);
int fimc_is_load_setfile(struct fimc_is_core *dev);
int fimc_is_otf_close(struct fimc_is_device_ischain *ischain);
int fimc_is_spi_reset(void *buf, u32 rx_addr, size_t size);
int fimc_is_spi_read(void *buf, u32 rx_addr, size_t size);
int fimc_is_runtime_suspend(struct device *dev);
int fimc_is_runtime_resume(struct device *dev);

#define CALL_POPS(s, op, args...) (((s)->pdata->op) ? ((s)->pdata->op(args)) : -EPERM)

#endif /* FIMC_IS_CORE_H_ */
