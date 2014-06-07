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

#ifndef FIMC_IS_CONFIG_H
#define FIMC_IS_CONFIG_H

/* configuration - default post processing */
#define ENABLE_SETFILE
#define ENABLE_DRC
/* #define ENABLE_ODC */
/* #define ENABLE_VDIS */
/* #define ENABLE_TDNR */
#define ENABLE_FD
#define ENABLE_CLOCK_GATE
#define HAS_FW_CLOCK_GATE
/* #define ENABLE_CACHE */
#define ENABLE_FULL_BYPASS
#define ENABLE_FAST_SHOT
#define USE_OWN_FAULT_HANDLER
/* #define ENABLE_MIF_400 */
/* #define ENABLE_DTP */

#if defined(CONFIG_PM_DEVFREQ)
#define ENABLE_DVFS
#endif

#if defined(CONFIG_SOC_EXYNOS5430)
#undef ENABLE_SETFILE
#define SUPPORTED_IS_CMD_VER	132
#define TARGET_SPI_CH_FOR_PERI	0
#endif

#if defined(CONFIG_SOC_EXYNOS5422)
#undef ENABLE_SETFILE
#define SUPPORTED_IS_CMD_VER	132
#define TARGET_SPI_CH_FOR_PERI	0
#undef HAS_FW_CLOCK_GATE
#endif

#if defined(CONFIG_SOC_EXYNOS5260)
#undef ENABLE_SETFILE
#undef ENABLE_DRC
#undef ENABLE_FULL_BYPASS
#define SUPPORTED_EARLY_BUF_DONE
#define SUPPORTED_IS_CMD_VER	131
#define TARGET_SPI_CH_FOR_PERI	1
#endif

#if defined(CONFIG_SOC_EXYNOS3470)
#undef ENABLE_SETFILE
#undef ENABLE_DRC
#undef ENABLE_DVFS
#undef ENABLE_FULL_BYPASS
#define SUPPORTED_IS_CMD_VER	131
#define TARGET_SPI_CH_FOR_PERI	1
#endif
/*
 * -----------------------------------------------------------------------------
 * Debug Message Configuration
 * -----------------------------------------------------------------------------
 */

/* #define DEBUG */
#define DBG_VIDEO
#define DBG_DEVICE
#define DBG_PER_FRAME
/* #define DBG_STREAMING */
#define DEBUG_INSTANCE 0xF
#define BUG_ON_ENABLE
/* #define FIXED_FPS_DEBUG */
#define FIXED_FPS_VALUE 10
/* #define DBG_FLITEISR */
#define FW_DEBUG
#define RESERVED_MEM
/* #define SCALER_CROP_DZOOM */
/* #define USE_ADVANCED_DZOOM */
/* #define TASKLET_MSG */
/* #define PRINT_CAPABILITY */
/* #define PRINT_BUFADDR */
/* #define PRINT_DZOOM */
#define ISDRV_VERSION 244

#if (defined(BAYER_CROP_DZOOM) && defined(SCALER_CROP_DZOOM))
#error BAYER_CROP_DZOOM and SCALER_CROP_DZOOM can''t be enable together
#endif

/*
 * driver version extension
 */
#ifdef ENABLE_CLOCK_GATE
#define get_drv_clock_gate() 0x1
#else
#define get_drv_clock_gate() 0x0
#endif
#ifdef ENABLE_DVFS
#define get_drv_dvfs() 0x2
#else
#define get_drv_dvfs() 0x0
#endif

#define GET_3AA_ID(video) ((video->id < 14) ? 0 : 1)
#define GET_3AAC_ID(video) ((video->id < FIMC_IS_VIDEO_3A1_NUM) ? 0 : 1)

#ifdef err
#undef err
#endif
#define err(fmt, args...) \
	pr_err("[@][ERR]%s:%d: " fmt "\n", __func__, __LINE__, ##args)

/* multi-stream */
#define merr(fmt, object, args...) \
	pr_err("[@][%d][ERR]%s:%d: " fmt "\n", object->instance, __func__, __LINE__, ##args)

/* multi-stream & runtime error */
#define mrerr(fmt, object, frame, args...) \
	pr_err("[@][%d:F%d][ERR]%s:%d: " fmt "\n", object->instance, frame->fcount, __func__, __LINE__, ##args)

#ifdef warn
#undef warn
#endif
#define warn(fmt, args...) \
	pr_warning("[@][WRN] " fmt "\n", ##args)

#define mwarn(fmt, this, args...) \
	pr_warning("[@][%d][WRN] " fmt "\n", this->instance, ##args)

#define info(fmt, args...) \
	pr_info("[@]" fmt, ##args)

#define minfo(fmt, object, args...) \
	pr_info("[@][%d]" fmt, object->instance, ##args)

#define mrinfo(fmt, object, frame, args...) \
	pr_info("[@][%d:F%d]" fmt, object->instance, frame->fcount,  ##args)

#define mdbg_common(prefix, fmt, instance, args...)				\
	do {									\
		if ((1<<(instance)) & DEBUG_INSTANCE)				\
			pr_info("[@]" prefix fmt, instance, ##args);		\
	} while (0)

#if (defined(DEBUG) && defined(DBG_VIDEO))
#define dbg(fmt, args...)

#define dbg_warning(fmt, args...) \
	printk(KERN_INFO "%s[WAR] Warning! " fmt, __func__, ##args)

/* debug message for video node */
#define mdbgv_vid(fmt, args...) \
	pr_info("[@][COM:V] " fmt, ##args)

#define mdbgv_sensor(fmt, this, args...) \
	mdbg_common("[%d][SS%d:V] ", fmt, this->video->id, this->instance, ##args)

#define mdbgv_3aa(fmt, this, args...) \
	mdbg_common("[%d][3A%d:V] ", fmt, GET_3AA_ID(this->video), this->instance, ##args)

#define mdbgv_3aac(fmt, this, args...) \
	mdbg_common("[%d][3A%dC:V] ", fmt, GET_3AAC_ID(this->video), this->instance, ##args)

#define dbg_isp(fmt, args...) \
	printk(KERN_INFO "[ISP] " fmt, ##args)

#define mdbgv_isp(fmt, this, args...) \
	mdbg_common("[%d][ISP:V] ", fmt, this->instance, ##args)

#define dbg_scp(fmt, args...) \
	printk(KERN_INFO "[SCP] " fmt, ##args)

#define mdbgv_scp(fmt, this, args...) \
	mdbg_common("[%d][SCP:V] ", fmt, this->instance, ##args)

#define dbg_scc(fmt, args...) \
	printk(KERN_INFO "[SCC] " fmt, ##args)

#define mdbgv_scc(fmt, this, args...) \
	mdbg_common("[%d][SCC:V] ", fmt, this->instance, ##args)

#define dbg_vdisc(fmt, args...) \
	printk(KERN_INFO "[VDC] " fmt, ##args)

#define mdbgv_vdc(fmt, this, args...) \
	mdbg_common("[%d][VDC:V] ", fmt, this->instance, ##args)

#define dbg_vdiso(fmt, args...) \
	printk(KERN_INFO "[VDO] " fmt, ##args)

#define mdbgv_vdo(fmt, this, args...) \
	mdbg_common("[%d][VDO:V] ", fmt, this->instance, ##args)
#else
#define dbg(fmt, args...)

/* debug message for video node */
#define mdbgv_vid(fmt, this, args...)
#define dbg_sensor(fmt, args...)
#define mdbgv_sensor(fmt, this, args...)
#define mdbgv_3aa(fmt, this, args...)
#define mdbgv_3aac(fmt, this, args...)
#define dbg_isp(fmt, args...)
#define mdbgv_isp(fmt, this, args...)
#define dbg_scp(fmt, args...)
#define mdbgv_scp(fmt, this, args...)
#define dbg_scc(fmt, args...)
#define mdbgv_scc(fmt, this, args...)
#define dbg_vdisc(fmt, args...)
#define mdbgv_vdc(fmt, this, args...)
#define dbg_vdiso(fmt, args...)
#define mdbgv_vdo(fmt, this, args...)
#endif

#if (defined(DEBUG) && defined(DBG_DEVICE))
/* debug message for device */
#define mdbgd_sensor(fmt, this, args...) \
	mdbg_common("[%d][SEN:D] ", fmt, this->instance, ##args)

#define mdbgd_front(fmt, this, args...) \
	mdbg_common("[%d][FRT:D] ", fmt, this->instance, ##args)

#define mdbgd_back(fmt, this, args...) \
	mdbg_common("[%d][BAK:D] ", fmt, this->instance, ##args)

#define mdbgd_3a0(fmt, this, args...) \
	mdbg_common("[%d][3A0:D] ", fmt, this->instance, ##args)

#define mdbgd_3a1(fmt, this, args...) \
	mdbg_common("[%d][3A1:D] ", fmt, this->instance, ##args)

#define mdbgd_isp(fmt, this, args...) \
	mdbg_common("[%d][ISP:D] ", fmt, this->instance, ##args)

#define mdbgd_ischain(fmt, this, args...) \
	mdbg_common("[%d][ISC:D] ", fmt, this->instance, ##args)

#define mdbgd_group(fmt, group, args...) \
	mdbg_common("[%d][GP%d:D] ", fmt, group->device->instance, group->id, ##args)

#define dbg_core(fmt, args...) \
	pr_info("[@][COR] " fmt, ##args)
#else
/* debug message for device */
#define mdbgd_sensor(fmt, this, args...)
#define mdbgd_front(fmt, this, args...)
#define mdbgd_back(fmt, this, args...)
#define mdbgd_isp(fmt, this, args...)
#define dbg_ischain(fmt, args...)
#define mdbgd_ischain(fmt, this, args...)
#define dbg_core(fmt, args...)
#define dbg_interface(fmt, args...)
#define dbg_frame(fmt, args...)
#define mdbgd_group(fmt, group, args...)
#endif

#if (defined(DEBUG) && defined(DBG_STREAMING))
#define dbg_interface(fmt, args...) \
	pr_info("[@][ITF] " fmt, ##args)
#define dbg_frame(fmt, args...) \
	pr_info("[@][FRM] " fmt, ##args)
#else
#define dbg_interface(fmt, args...)
#define dbg_frame(fmt, args...)
#endif

#if (defined(DEBUG) && defined(DBG_PER_FRAME))
#define mdbg_pframe(fmt, object, frame, args...) \
	pr_info("[@][%d:F%d]" fmt, object->instance, frame->fcount,  ##args)
#else
#define mdbg_pframe(fmt, object, frame, args...)
#endif

#endif
