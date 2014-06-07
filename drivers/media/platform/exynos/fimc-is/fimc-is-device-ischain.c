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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>
#include <linux/bug.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/smc.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#if defined(CONFIG_SOC_EXYNOS3470)
#include <mach/bts.h>
#endif

#include "fimc-is-time.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-companion.h"
#include "fimc-is-clk-gate.h"
#include "fimc-is-dvfs.h"

#define SDCARD_FW
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_FW_SDCARD			"/data/fimc_is_fw2.bin"

#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_VERSION_SIZE			42
#define FIMC_IS_SETFILE_VER_OFFSET		0x40
#define FIMC_IS_SETFILE_VER_SIZE		52

#define FIMC_IS_CAL_SDCARD			"/data/cal_data.bin"
/*#define FIMC_IS_MAX_CAL_SIZE			(20 * 1024)*/
#define FIMC_IS_MAX_FW_SIZE			(2048 * 1024)
#define FIMC_IS_CAL_START_ADDR			(0x013D0000)
#define FIMC_IS_CAL_RETRY_CNT			(2)
#define FIMC_IS_FW_RETRY_CNT			(2)


/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		(1280) /* sensor margin : 16 */
#define DEFAULT_PREVIEW_STILL_HEIGHT		(720) /* sensor margin : 12 */
#define DEFAULT_CAPTURE_VIDEO_WIDTH		(1920)
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		(1080)
#define DEFAULT_CAPTURE_STILL_WIDTH		(2560)
#define DEFAULT_CAPTURE_STILL_HEIGHT		(1920)
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	(2560)
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	(1440)
#define DEFAULT_PREVIEW_VIDEO_WIDTH		(640)
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		(480)

/* sysfs variable for debug */
extern struct fimc_is_sysfs_debug sysfs_debug;

#ifdef FW_DEBUG
#define DEBUG_FS_ROOT_NAME	"fimc-is"
#define DEBUG_FS_FILE_NAME	"isfw-msg"
static struct dentry		*debugfs_root;
static struct dentry		*debugfs_file;

#define SETFILE_SIZE	0x6000
#define READ_SIZE		0x100

#define HEADER_CRC32_LEN (128 / 2)
#define OEM_CRC32_LEN (192 / 2)
#define AWB_CRC32_LEN (32 / 2)
#define SHADING_CRC32_LEN (2336 / 2)

static char fw_name[100];
static int cam_id;
bool is_dumped_fw_loading_needed = false;

static int isfw_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static int isfw_debug_read(struct file *file, char __user *user_buf,
	size_t buf_len, loff_t *ppos)
{
	char *debug;
	size_t debug_cnt, backup_cnt;
	size_t count1, count2;
	size_t buf_count = 0;
	struct fimc_is_device_ischain *device =
		(struct fimc_is_device_ischain *)file->private_data;
	struct fimc_is_ishcain_mem *imemory;
	struct fimc_is_core *core;

	BUG_ON(!device);

	count1 = 0;
	count2 = 0;
	debug_cnt = 0;
	imemory = &device->imemory;
	core = (struct fimc_is_core *)device->interface->core;

	if (atomic_read(&core->video_isp.refcount) <= 0) {
		err("isp video node is not open");
		goto exit;
	}

	vb2_ion_sync_for_device(imemory->fw_cookie, DEBUG_OFFSET,
		DEBUG_CNT, DMA_FROM_DEVICE);

	debug_cnt = *((int *)(imemory->kvaddr + DEBUGCTL_OFFSET)) - DEBUG_OFFSET;
	backup_cnt = core->debug_cnt;

	if (core->debug_cnt > debug_cnt) {
		count1 = DEBUG_CNT - core->debug_cnt;
		count2 = debug_cnt;
	} else {
		count1 = debug_cnt - core->debug_cnt;
		count2 = 0;
	}

	buf_count = buf_len;

	if (buf_count && count1) {
		debug = (char *)(imemory->kvaddr + DEBUG_OFFSET + core->debug_cnt);

		if (count1 > buf_count)
			count1 = buf_count;

		buf_count -= count1;

		memcpy(user_buf, debug, count1);
		core->debug_cnt += count1;
	}

	if (buf_count && count2) {
		debug = (char *)(imemory->kvaddr + DEBUG_OFFSET);

		if (count2 > buf_count)
			count2 = buf_count;

		buf_count -= count2;

		memcpy(user_buf, debug, count2);
		core->debug_cnt = count2;
	}

	info("FW_READ : Origin(%d), New(%d) - Length(%d)\n",
		backup_cnt,
		core->debug_cnt,
		(buf_len - buf_count));

exit:
	return buf_len - buf_count;
}

static const struct file_operations debug_fops = {
	.open	= isfw_debug_open,
	.read	= isfw_debug_read,
	.llseek	= default_llseek
};

#endif

static const struct sensor_param init_sensor_param = {
	.config = {
#ifdef FIXED_FPS_DEBUG
		.framerate = FIXED_FPS_VALUE,
		.min_target_fps = FIXED_FPS_VALUE,
		.max_target_fps = FIXED_FPS_VALUE,
#else
		.framerate = 30,
		.min_target_fps = 15,
		.max_target_fps = 30,
#endif
	},
};

static const struct taa_param init_taa_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_BAYER,
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.frametime_min = 0,
		.frametime_max = 33333,
		.sensor_binning_ratio_x = 1000,
		.sensor_binning_ratio_y = 1000,
		.err = OTF_INPUT_ERROR_NO,
	},
	.vdma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0,
		.height = 0,
		.format = 0,
		.bitwidth = 0,
		.plane = 0,
		.order = 0,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.buffer_number = 0,
		.buffer_address = 0,
		.sensor_binning_ratio_x = 1000,
		.sensor_binning_ratio_y = 1000,
		.err = 0,
	},
	.ddma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	},
	.vdma4_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.dma_out_mask = 0,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = DMA_INPUT_PLANE_1,
		.order = DMA_INPUT_ORDER_YCbCr,
		.buffer_number = 0,
		.buffer_address = 0,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.vdma2_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_BAYER,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_GB_BG,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xFFFFFFFF,
		.err = DMA_OUTPUT_ERROR_NO,
	},
	.ddma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
};

static const struct isp_param init_isp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
	},
	.vdma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0,
		.height = 0,
		.format = 0,
		.bitwidth = 0,
		.plane = 0,
		.order = 0,
		.buffer_number = 0,
		.buffer_address = 0,
		.sensor_binning_ratio_x = 1000,
		.sensor_binning_ratio_y = 1000,
		.err = 0,
	},
	.vdma3_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.vdma4_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
	.vdma5_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
};

static const struct drc_param init_drc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = DMA_INPUT_PLANE_1,
		.order = DMA_INPUT_ORDER_YCbCr,
		.buffer_number = 0,
		.buffer_address = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct scalerc_param init_scalerc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.arbitrary_cb = 128, /* default value : 128 */
		.arbitrary_cr = 128, /* default value : 128 */
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_CROP_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_CROP_HEIGHT,
		.in_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.in_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.out_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.out_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV422,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_1,
		.order = DMA_OUTPUT_ORDER_CrYCbY,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xffff,
		.reserved[0] = SCALER_DMA_OUT_UNSCALED,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct odc_param init_odc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};

static const struct dis_param init_dis_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
};
static const struct tdnr_param init_tdnr_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.frame = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_2,
		.order = DMA_OUTPUT_ORDER_CbCr,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xffff,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct scalerp_param init_scalerp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.bayer_crop_offset_x = 0,
		.bayer_crop_offset_y = 0,
		.bayer_crop_width = 0,
		.bayer_crop_height = 0,
		.err = OTF_INPUT_ERROR_NO,
	},
	.effect = {
		.cmd = 0,
		.arbitrary_cb = 128, /* default value : 128 */
		.arbitrary_cr = 128, /* default value : 128 */
		.yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL,
		.err = 0,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.crop_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.in_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.in_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.out_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.out_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.err = 0,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.pos_x = 0,
		.pos_y = 0,
		.crop_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.crop_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.err = 0,
	},
	.rotation = {
		.cmd = 0,
		.err = 0,
	},
	.flip = {
		.cmd = 0,
		.err = 0,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
		.crop_offset_x = 0,
		.crop_offset_y = 0,
		.err = OTF_OUTPUT_ERROR_NO,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = DMA_OUTPUT_PLANE_3,
		.order = DMA_OUTPUT_ORDER_NO,
		.buffer_number = 0,
		.buffer_address = 0,
		.dma_out_mask = 0xffff,
		.err = DMA_OUTPUT_ERROR_NO,
	},
};

static const struct fd_param init_fd_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
		.err = CONTROL_ERROR_NO,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
		.err = OTF_INPUT_ERROR_NO,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = 0, .height = 0,
		.format = 0, .bitwidth = 0, .plane = 0,
		.order = 0, .buffer_number = 0, .buffer_address = 0,
		.err = 0,
	},
	.config = {
		.cmd = FD_CONFIG_COMMAND_MAXIMUM_NUMBER |
			FD_CONFIG_COMMAND_ROLL_ANGLE |
			FD_CONFIG_COMMAND_YAW_ANGLE |
			FD_CONFIG_COMMAND_SMILE_MODE |
			FD_CONFIG_COMMAND_BLINK_MODE |
			FD_CONFIG_COMMAND_EYES_DETECT |
			FD_CONFIG_COMMAND_MOUTH_DETECT |
			FD_CONFIG_COMMAND_ORIENTATION |
			FD_CONFIG_COMMAND_ORIENTATION_VALUE,
		.max_number = CAMERA2_MAX_FACES,
		.roll_angle = FD_CONFIG_ROLL_ANGLE_FULL,
		.yaw_angle = FD_CONFIG_YAW_ANGLE_45_90,
		.smile_mode = FD_CONFIG_SMILE_MODE_DISABLE,
		.blink_mode = FD_CONFIG_BLINK_MODE_DISABLE,
		.eye_detect = FD_CONFIG_EYES_DETECT_ENABLE,
		.mouth_detect = FD_CONFIG_MOUTH_DETECT_DISABLE,
		.orientation = FD_CONFIG_ORIENTATION_DISABLE,
		.orientation_value = 0,
		.err = ERROR_FD_NO,
	},
};

#ifndef RESERVED_MEM
static int fimc_is_ishcain_deinitmem(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	vb2_ion_private_free(device->imemory.fw_cookie);

	return ret;
}
#endif

static void fimc_is_ischain_cache_flush(struct fimc_is_device_ischain *this,
	u32 offset, u32 size)
{
	vb2_ion_sync_for_device(this->imemory.fw_cookie,
		offset,
		size,
		DMA_TO_DEVICE);
}

static void fimc_is_ischain_region_invalid(struct fimc_is_device_ischain *device)
{
	vb2_ion_sync_for_device(
		device->imemory.fw_cookie,
		device->imemory.offset_region,
		sizeof(struct is_region),
		DMA_FROM_DEVICE);
}

static void fimc_is_ischain_region_flush(struct fimc_is_device_ischain *device)
{
	vb2_ion_sync_for_device(
		device->imemory.fw_cookie,
		device->imemory.offset_region,
		sizeof(struct is_region),
		DMA_TO_DEVICE);
}

void fimc_is_ischain_meta_flush(struct fimc_is_frame *frame)
{
#ifdef ENABLE_CACHE
	vb2_ion_sync_for_device(
		(void *)frame->cookie_shot,
		0,
		frame->shot_size,
		DMA_TO_DEVICE);
#endif
}

void fimc_is_ischain_meta_invalid(struct fimc_is_frame *frame)
{
#ifdef ENABLE_CACHE
	vb2_ion_sync_for_device(
		(void *)frame->cookie_shot,
		0,
		frame->shot_size,
		DMA_FROM_DEVICE);
#endif
}

static void fimc_is_ischain_version(struct fimc_is_device_ischain *this, char *name, const char *load_bin, u32 size)
{
	struct fimc_is_from_info *pinfo = NULL;
	char version_str[60];

	if (!strcmp(fw_name, name)) {
		memcpy(version_str, &load_bin[size - FIMC_IS_VERSION_SIZE],
			FIMC_IS_VERSION_SIZE);
		version_str[FIMC_IS_VERSION_SIZE] = '\0';

		pinfo = &this->pinfo;
		memcpy(pinfo->header_ver, &version_str[32], 11);
		pinfo->header_ver[11] = '\0';
	} else {
		memcpy(version_str, &load_bin[size - FIMC_IS_SETFILE_VER_OFFSET],
			FIMC_IS_SETFILE_VER_SIZE);
		version_str[FIMC_IS_SETFILE_VER_SIZE] = '\0';

		pinfo = &this->pinfo;
		memcpy(pinfo->setfile_ver, &version_str[17], 4);
		pinfo->setfile_ver[4] = '\0';
	}

	info("%s version : %s\n", name, version_str);
}

void fimc_is_ischain_savefirm(struct fimc_is_device_ischain *this)
{
#ifdef DEBUG_DUMP_FIRMWARE
	loff_t pos;

	write_data_to_file("/data/firmware.bin", (char *)this->imemory.kvaddr,
		(size_t)FIMC_IS_A5_MEM_SIZE, &pos);
#endif
}

static int fimc_is_ischain_loadfirm(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	int location = 0;
	const struct firmware *fw_blob;
	u8 *buf = NULL;
#ifdef SDCARD_FW
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;
	char fw_path[100];

	mdbgd_ischain("%s\n", device, __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(FIMC_IS_FW_SDCARD, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		goto request_fw;
	}

	location = 1;
	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;
	pr_info("start, file path %s, size %ld Bytes\n",
		is_dumped_fw_loading_needed ? fw_path : FIMC_IS_FW_SDCARD, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		dev_err(&device->pdev->dev,
			"failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		dev_err(&device->pdev->dev,
			"failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}

	memcpy((void *)device->imemory.kvaddr, (void *)buf, fsize);
	fimc_is_ischain_cache_flush(device, 0, fsize + 1);
	fimc_is_ischain_version(device, fw_name, buf, fsize);

request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif
		ret = request_firmware(&fw_blob, fw_name, &device->pdev->dev);
		if (ret) {
			err("request_firmware is fail(%d)", ret);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob) {
			merr("fw_blob is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob->data) {
			merr("fw_blob->data is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		memcpy((void *)device->imemory.kvaddr, fw_blob->data,
			fw_blob->size);
		fimc_is_ischain_cache_flush(device, 0, fw_blob->size + 1);
		fimc_is_ischain_version(device, fw_name, fw_blob->data,
			fw_blob->size);

		release_firmware(fw_blob);
#ifdef SDCARD_FW
	}
#endif

out:
#ifdef SDCARD_FW
	if (!fw_requested) {
		if (buf)
			vfree(buf);
		if (fp)
			filp_close(fp, current->files);
		set_fs(old_fs);
	}
#endif

	if (ret)
		err("firmware loading is fail");
	else
		info("Camera: the %s FW were applied successfully.\n",
			((cam_id == CAMERA_SINGLE_REAR) &&
				is_dumped_fw_loading_needed) ? "dumped" : "default");

	return ret;
}

static int fimc_is_ischain_loadsetf(struct fimc_is_device_ischain *device,
	u32 load_addr, char *setfile_name)
{
	int ret = 0;
	int location = 0;
	void *address;
	const struct firmware *fw_blob;
	u8 *buf = NULL;
#ifdef SDCARD_FW
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	int fw_requested = 1;
	char setfile_path[256];
	u32 retry;

	mdbgd_ischain("%s\n", device, __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	memset(setfile_path, 0x00, sizeof(setfile_path));
	snprintf(setfile_path, sizeof(setfile_path), "%s%s",
		FIMC_IS_SETFILE_SDCARD_PATH, setfile_name);
	fp = filp_open(setfile_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		goto request_fw;
	}

	location = 1;
	fw_requested = 0;
	fsize = fp->f_path.dentry->d_inode->i_size;
	info("start, file path %s, size %ld Bytes\n",
		setfile_path, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		dev_err(&device->pdev->dev,
			"failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		dev_err(&device->pdev->dev,
			"failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}

	address = (void *)(device->imemory.kvaddr + load_addr);
	memcpy((void *)address, (void *)buf, fsize);
	fimc_is_ischain_cache_flush(device, load_addr, fsize + 1);
	fimc_is_ischain_version(device, setfile_name, buf, fsize);

request_fw:
	if (fw_requested) {
		set_fs(old_fs);
#endif

		retry = 4;
		ret = request_firmware((const struct firmware **)&fw_blob,
			setfile_name, &device->pdev->dev);
		while (--retry && ret) {
			mwarn("request_firmware is fail(%d)", device, ret);
			ret = request_firmware((const struct firmware **)&fw_blob,
				setfile_name, &device->pdev->dev);
		}

		if (!retry) {
			merr("request_firmware is fail(%d)", device, ret);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob) {
			merr("fw_blob is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		if (!fw_blob->data) {
			merr("fw_blob->data is NULL", device);
			ret = -EINVAL;
			goto out;
		}

		address = (void *)(device->imemory.kvaddr + load_addr);
		memcpy(address, fw_blob->data, fw_blob->size);
		fimc_is_ischain_cache_flush(device, load_addr, fw_blob->size + 1);
		fimc_is_ischain_version(device, setfile_name, fw_blob->data,
			(u32)fw_blob->size);

		release_firmware(fw_blob);
#ifdef SDCARD_FW
	}
#endif

out:
#ifdef SDCARD_FW
	if (!fw_requested) {
		if (buf)
			vfree(buf);
		if (fp)
			filp_close(fp, current->files);
		set_fs(old_fs);
	}
#endif

	if (ret)
		err("setfile loading is fail");
	else
		info("Camera: the %s Setfile were applied successfully.\n",
			((cam_id == CAMERA_SINGLE_REAR) &&
				is_dumped_fw_loading_needed) ? "dumped" : "default");

	return ret;
}

static int fimc_is_ischain_loadcalb(struct fimc_is_device_ischain *device,
	struct fimc_is_module_enum *active_sensor)
{
#if 1
	return 0;
#else
	int ret = 0;
	char *buf = NULL;
	char *cal_ptr;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	char calfile_path[256];

	mdbgd_ischain("%s\n", device, __func__);

	cal_ptr = (char *)(device->imemory.kvaddr + FIMC_IS_CAL_START_ADDR);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	memset(calfile_path, 0x00, sizeof(calfile_path));
	snprintf(calfile_path, sizeof(calfile_path), "%s", FIMC_IS_CAL_SDCARD);
	fp = filp_open(calfile_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		mwarn("failed to filp_open", device);
		memset((void *)cal_ptr, 0xCC, FIMC_IS_MAX_CAL_SIZE);
		fp = NULL;
		ret = -EIO;
		goto out;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	if (fsize != FIMC_IS_MAX_CAL_SIZE) {
		merr("cal_data.bin file size is invalid(%ld size)",
			device, fsize);
		memset((void *)cal_ptr, 0xAC, FIMC_IS_MAX_CAL_SIZE);
		ret = -EINVAL;
		goto out;
	}

	mdbgd_ischain("start, file path %s, size %ld Bytes\n", device, calfile_path, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		dev_err(&device->pdev->dev,
			"failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}
	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		dev_err(&device->pdev->dev,
			"failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto out;
	}

	info("CAL DATA : MAP ver : %c%c%c%c\n", buf[0x60], buf[0x61],
		buf[0x62], buf[0x63]);

	/* CRC check */
	if (CRC32_CHECK == true) {
		memcpy((void *)(cal_ptr) ,(void *)buf, nread);
		info("Camera : the dumped Cal. data was applied successfully.\n");
	} else {
		if (CRC32_HEADER_CHECK == true) {
			pr_err("Camera : CRC32 error but only header section is no problem.\n");
			memset((void *)(cal_ptr + 0x1000), 0xFF, FIMC_IS_MAX_CAL_SIZE - 0x1000);
		} else {
			pr_err("Camera : CRC32 error for all section.\n");
			memset((void *)(cal_ptr), 0xFF, FIMC_IS_MAX_CAL_SIZE);
			ret = -EIO;
		}
	}

out:
	fimc_is_ischain_cache_flush(device, FIMC_IS_CAL_START_ADDR,
		FIMC_IS_MAX_CAL_SIZE);

	if (buf)
		vfree(buf);
	if (fp)
		filp_close(fp, current->files);

	set_fs(old_fs);

	if (ret)
		mwarn("calibration loading is fail", device);

	return ret;
#endif
}

static void fimc_is_ischain_forcedown(struct fimc_is_device_ischain *this,
	bool on)
{
	if (on) {
		printk(KERN_INFO "Set low poweroff mode\n");
		__raw_writel(0x0, PMUREG_ISP_ARM_OPTION);
		__raw_writel(0x1CF82000, PMUREG_ISP_LOW_POWER_OFF);
		this->force_down = true;
	} else {
		printk(KERN_INFO "Clear low poweroff mode\n");
		__raw_writel(0xFFFFFFFF, PMUREG_ISP_ARM_OPTION);
		__raw_writel(0x8, PMUREG_ISP_LOW_POWER_OFF);
		this->force_down = false;
	}
}

void tdnr_s3d_pixel_async_sw_reset(struct fimc_is_device_ischain *this)
{
	u32 cfg = readl(SYSREG_GSCBLK_CFG1);
	/* S3D pixel async sw reset */
	cfg &= ~(1 << 25);
	writel(cfg, SYSREG_GSCBLK_CFG1);

	cfg = readl(SYSREG_ISPBLK_CFG);
	/* 3DNR pixel async sw reset */
	cfg &= ~(1 << 5);
	writel(cfg, SYSREG_ISPBLK_CFG);
}

int fimc_is_ischain_power(struct fimc_is_device_ischain *device, int on)
{
#ifdef CONFIG_ARM_TRUSTZONE
	int i;
#endif
	int ret = 0;
	u32 timeout;
	u32 debug;

	struct device *dev = &device->pdev->dev;
	struct fimc_is_core *core = (struct fimc_is_core *)platform_get_drvdata(device->pdev);

	if (on) {
		/* 1. force poweroff setting */
		if (device->force_down)
			fimc_is_ischain_forcedown(device, false);

		/* 2. FIMC-IS local power enable */
#if defined(CONFIG_PM_RUNTIME)
		mdbgd_ischain("pm_runtime_suspended = %d\n", device, pm_runtime_suspended(dev));
		pm_runtime_get_sync(dev);
#else
		fimc_is_runtime_resume(dev);
		info("%s(%d) - fimc_is runtime resume complete\n", __func__, on);
#endif
#if defined(CONFIG_SOC_EXYNOS3470)
		bts_initialize("pd-cam", true);
#endif
		snprintf(fw_name, sizeof(fw_name), "%s", FIMC_IS_FW);

		/* 3. Load IS firmware */
		ret = fimc_is_ischain_loadfirm(device);
		if (ret) {
			err("failed to fimc_is_request_firmware (%d)", ret);
			clear_bit(FIMC_IS_ISCHAIN_LOADED, &device->state);
			ret = -EINVAL;
			goto exit;
		}
		set_bit(FIMC_IS_ISCHAIN_LOADED, &device->state);

#if defined(CONFIG_SOC_EXYNOS5422)
		tdnr_s3d_pixel_async_sw_reset(device);
#endif /* defined(CONFIG_SOC_EXYNOS5422) */
		/* 4. A5 start address setting */
		mdbgd_ischain("imemory.base(dvaddr) : 0x%08x\n", device, device->imemory.dvaddr);
		mdbgd_ischain("imemory.base(kvaddr) : 0x%08X\n", device, device->imemory.kvaddr);

		if (!device->imemory.dvaddr) {
			merr("firmware device virtual is null", device);
			ret = -ENOMEM;
			goto exit;
		}

		writel(device->imemory.dvaddr, device->regs + BBOAR);

		pr_debug("%s(%d) - check dvaddr validate...\n", __func__, on);

#ifdef CONFIG_ARM_TRUSTZONE
		exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(PA_FIMC_IS_GIC_C + 0x4), 0x000000FF, 0);
		for (i = 0; i < 3; i++)
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(PA_FIMC_IS_GIC_D + 0x80 + (i * 4)), 0xFFFFFFFF, 0);
		for (i = 0; i < 18; i++)
			exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(PA_FIMC_IS_GIC_D + 0x400 + (i * 4)), 0x10101010, 0);

		exynos_smc_readsfr(PA_FIMC_IS_GIC_C + 0x4, &debug);
		pr_info("%s : PA_FIMC_IS_GIC_C : 0x%08x\n", __func__, debug);
		if (debug != 0xFC)
			merr("secure configuration is fail[0x131E0004:%08X]", device, debug);
#endif

		/* 5. A5 power on*/
		writel(0x1, PMUREG_ISP_ARM_CONFIGURATION);

		info("%s(%d) - A5 Power on\n", __func__, on);

		/* 6. enable A5 */
		writel(0x00018000, PMUREG_ISP_ARM_OPTION);
		timeout = 1000;

		pr_debug("%s(%d) - A5 enable start...\n", __func__, on);

		while ((__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) != 0x1) {
			if (timeout == 0)
				err("A5 power on failed\n");
			timeout--;
			udelay(1);
		}

		pr_debug("%s(%d) - A5 enable end...\n", __func__, on);

		set_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);

		/* for mideaserver force down */
		set_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state);

		pr_debug("%s(%d) - change A5 state\n", __func__, on);
	} else {
		/* 1. disable A5 */
		if (test_bit(IS_IF_STATE_START, &device->interface->state))
			writel(0x10000, PMUREG_ISP_ARM_OPTION);
		else
			writel(0x00000, PMUREG_ISP_ARM_OPTION);

		/* Check FW state for WFI of A5 */
		debug = readl(device->interface->regs + ISSR6);
		printk(KERN_INFO "%s: A5 state(0x%x)\n", __func__, debug);
#if defined(CONFIG_SOC_EXYNOS3470)
		bts_initialize("pd-cam", false);
#endif
		/* 2. FIMC-IS local power down */
#if defined(CONFIG_PM_RUNTIME)
		pm_runtime_put_sync(dev);
		mdbgd_ischain("pm_runtime_suspended = %d\n", device, pm_runtime_suspended(dev));

#if defined(CONFIG_SOC_EXYNOS3470)
		writel(0x0, PMUREG_ISP_ARM_SYS_PWR_REG);
#else
		timeout = 1000;
		while ((readl(PMUREG_ISP_STATUS) & 0x1) && timeout) {
			timeout--;
			usleep_range(1000, 1000);
		}
		if (timeout == 0)
			err("ISP power down failed(0x%08x)\n",
				readl(PMUREG_ISP_STATUS));
#endif /* defined(CONFIG_SOC_EXYNOS3470) */
#if defined(CONFIG_SOC_EXYNOS5430)
		timeout = 1000;
		while ((readl(PMUREG_CAM0_STATUS) & 0x1) && timeout) {
			timeout--;
			usleep_range(1000, 1000);
		}
		if (timeout == 0)
			err("CAM0 power down failed(0x%08x)\n",
				readl(PMUREG_CAM0_STATUS));

		timeout = 1000;
		while ((readl(PMUREG_CAM1_STATUS) & 0x1) && timeout) {
			timeout--;
			usleep_range(1000, 1000);
		}
		if (timeout == 0)
			err("CAM1 power down failed(CAM1:0x%08x, A5:0x%08x)\n",
				readl(PMUREG_CAM1_STATUS), readl(PMUREG_ISP_ARM_STATUS));
#endif /* defined(CONFIG_SOC_EXYNOS5430) */
#if defined(CONFIG_SOC_EXYNOS5422)
#endif /* defined(CONFIG_SOC_EXYNOS5422) */
#else
		/* A5 power off*/
		timeout = 1000;
		writel(0x0, PMUREG_ISP_ARM_CONFIGURATION);
		while ((__raw_readl(PMUREG_ISP_ARM_STATUS) & 0x1) && timeout) {
			timeout--;
			udelay(1);
		}
		if (timeout == 0)
			err("A5 power down failed(status:%x)\n",
				__raw_readl(PMUREG_ISP_ARM_STATUS));

		fimc_is_runtime_suspend(dev);
#endif
		clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);

		/* for mideaserver force down */
		clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state);
	}

exit:
	info("%s(%d)\n", __func__, test_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state));
	return ret;
}

static int fimc_is_itf_s_param(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 lindex,
	u32 hindex,
	u32 indexes)
{
	int ret = 0;
	u32 flag, index;
	u32 dst_base, src_base;

	BUG_ON(!device);

	if (frame) {
		if (!test_bit(FIMC_IS_ISHCAIN_START, &device->state)) {
			merr("s_param is fail, device already is stopped", device);
			BUG();
		}

		dst_base = (u32)&device->is_region->parameter;
		src_base = (u32)frame->shot->ctl.entry.parameter;

		for (index = 0; lindex && (index < 32); index++) {
			flag = 1 << index;
			if (lindex & flag) {
				memcpy((u32 *)(dst_base + (index * PARAMETER_MAX_SIZE)),
					(u32 *)(src_base + (index * PARAMETER_MAX_SIZE)),
					PARAMETER_MAX_SIZE);
				lindex &= ~flag;
			}
		}

		for (index = 0; hindex && (index < 32); index++) {
			flag = 1 << index;
			if (hindex & flag) {
				memcpy((u32 *)(dst_base + ((32 + index) * PARAMETER_MAX_SIZE)),
					(u32 *)(src_base + ((32 + index) * PARAMETER_MAX_SIZE)),
					PARAMETER_MAX_SIZE);
				hindex &= ~flag;
			}
		}

		fimc_is_ischain_region_flush(device);
	} else {
		/*
		 * this check code is commented until per-frame control is worked fully
		 *
		 * if ( test_bit(FIMC_IS_ISHCAIN_START, &device->state)) {
		 *	merr("s_param is fail, device already is started", device);
		 *	BUG();
		 * }
		 */

		fimc_is_ischain_region_flush(device);

		if (lindex || hindex) {
			ret = fimc_is_hw_s_param(device->interface,
				device->instance,
				lindex,
				hindex,
				indexes);
		}
	}

	return ret;
}

static void * fimc_is_itf_g_param(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 index)
{
	u32 dst_base, src_base, dst_param, src_param;

	BUG_ON(!device);

	if (frame) {
		if (!test_bit(FIMC_IS_ISHCAIN_START, &device->state)) {
			merr("s_param is fail, device already is stopped", device);
			BUG();
		}

		dst_base = (u32)&frame->shot->ctl.entry.parameter[0];
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
		src_base = (u32)&device->is_region->parameter;
		src_param = (src_base + (index * PARAMETER_MAX_SIZE));
		memcpy((u32 *)dst_param, (u32 *)src_param, PARAMETER_MAX_SIZE);
	} else {
		if ( test_bit(FIMC_IS_ISHCAIN_START, &device->state)) {
			merr("s_param is fail, device already is started", device);
			BUG();
		}

		dst_base = (u32)&device->is_region->parameter;
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
	}

	return (void *)dst_param;
}

static int fimc_is_itf_a_param(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;
	u32 setfile;

	BUG_ON(!device);

	setfile = (device->setfile & FIMC_IS_SETFILE_MASK);

	ret = fimc_is_hw_a_param(device->interface,
		device->instance,
		group,
		setfile);

	return ret;
}

static int fimc_is_itf_f_param(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 setfile;
	u32 group = 0;
#ifdef DEBUG
	u32 navailable = 0;
	struct is_region *region = device->is_region;
#endif

	mdbgd_ischain(" NAME          SIZE    BINNING    FRAMERATE\n", device);
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state) ||
		!IS_ISCHAIN_OTF(device))
		mdbgd_ischain("SENSOR :  %04dx%04d        %1dx%1d          %3d\n",
			device,
			region->parameter.taa.vdma1_input.width + device->margin_width,
			region->parameter.taa.vdma1_input.height + device->margin_height,
			(region->parameter.taa.vdma1_input.sensor_binning_ratio_x / 1000),
			(region->parameter.taa.vdma1_input.sensor_binning_ratio_y / 1000),
			region->parameter.sensor.config.framerate
			);
	else
		mdbgd_ischain("SENSOR :  %04dx%04d        %1dx%1d          %3d\n",
			device,
			region->parameter.sensor.dma_output.width,
			region->parameter.sensor.dma_output.height,
			(region->parameter.taa.otf_input.sensor_binning_ratio_x / 1000),
			(region->parameter.taa.otf_input.sensor_binning_ratio_y / 1000),
			region->parameter.sensor.config.framerate
			);
	mdbgd_ischain(" NAME    ON  BYPASS PATH        SIZE FORMAT\n", device);
	mdbgd_ischain("3AX OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.taa.control.cmd,
		region->parameter.taa.control.bypass,
		region->parameter.taa.otf_input.cmd,
		region->parameter.taa.otf_input.width,
		region->parameter.taa.otf_input.height,
		region->parameter.taa.otf_input.format
		);
	mdbgd_ischain("3AX DI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.taa.control.cmd,
		region->parameter.taa.control.bypass,
		region->parameter.taa.vdma1_input.cmd,
		region->parameter.taa.vdma1_input.width,
		region->parameter.taa.vdma1_input.height,
		region->parameter.taa.vdma1_input.format
		);
	mdbgd_ischain("3AX DO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.taa.control.cmd,
		region->parameter.taa.control.bypass,
		region->parameter.taa.vdma2_output.cmd,
		region->parameter.taa.vdma2_output.width,
		region->parameter.taa.vdma2_output.height,
		region->parameter.taa.vdma2_output.format
		);
	mdbgd_ischain("ISP OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.isp.control.cmd,
		region->parameter.isp.control.bypass,
		region->parameter.isp.otf_input.cmd,
		region->parameter.isp.otf_input.width,
		region->parameter.isp.otf_input.height,
		region->parameter.isp.otf_input.format
		);
	mdbgd_ischain("ISP DI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.isp.control.cmd,
		region->parameter.isp.control.bypass,
		region->parameter.isp.vdma1_input.cmd,
		region->parameter.isp.vdma1_input.width,
		region->parameter.isp.vdma1_input.height,
		region->parameter.isp.vdma1_input.format
		);
	mdbgd_ischain("ISP OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.isp.control.cmd,
		region->parameter.isp.control.bypass,
		region->parameter.isp.otf_output.cmd,
		region->parameter.isp.otf_output.width,
		region->parameter.isp.otf_output.height,
		region->parameter.isp.otf_output.format
		);
	mdbgd_ischain("DRC OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass,
		region->parameter.drc.otf_input.cmd,
		region->parameter.drc.otf_input.width,
		region->parameter.drc.otf_input.height,
		region->parameter.drc.otf_input.format
		);
	mdbgd_ischain("DRC OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.drc.control.cmd,
		region->parameter.drc.control.bypass,
		region->parameter.drc.otf_output.cmd,
		region->parameter.drc.otf_output.width,
		region->parameter.drc.otf_output.height,
		region->parameter.drc.otf_output.format
		);
	mdbgd_ischain("SCC OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.otf_input.cmd,
		region->parameter.scalerc.otf_input.width,
		region->parameter.scalerc.otf_input.height,
		region->parameter.scalerc.otf_input.format
		);
	mdbgd_ischain("SCC DO : %2d    %4d  %3d   %04dx%04d %4d,%d\n", device,
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.dma_output.cmd,
		region->parameter.scalerc.dma_output.width,
		region->parameter.scalerc.dma_output.height,
		region->parameter.scalerc.dma_output.format,
		region->parameter.scalerc.dma_output.plane
		);
	mdbgd_ischain("SCC OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.scalerc.control.cmd,
		region->parameter.scalerc.control.bypass,
		region->parameter.scalerc.otf_output.cmd,
		region->parameter.scalerc.otf_output.width,
		region->parameter.scalerc.otf_output.height,
		region->parameter.scalerc.otf_output.format
		);
	mdbgd_ischain("ODC OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.odc.control.cmd,
		region->parameter.odc.control.bypass,
		region->parameter.odc.otf_input.cmd,
		region->parameter.odc.otf_input.width,
		region->parameter.odc.otf_input.height,
		region->parameter.odc.otf_input.format
		);
	mdbgd_ischain("ODC OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.odc.control.cmd,
		region->parameter.odc.control.bypass,
		region->parameter.odc.otf_output.cmd,
		region->parameter.odc.otf_output.width,
		region->parameter.odc.otf_output.height,
		region->parameter.odc.otf_output.format
		);
	mdbgd_ischain("DIS OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass,
		region->parameter.dis.otf_input.cmd,
		region->parameter.dis.otf_input.width,
		region->parameter.dis.otf_input.height,
		region->parameter.dis.otf_input.format
		);
	mdbgd_ischain("DIS OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.dis.control.cmd,
		region->parameter.dis.control.bypass,
		region->parameter.dis.otf_output.cmd,
		region->parameter.dis.otf_output.width,
		region->parameter.dis.otf_output.height,
		region->parameter.dis.otf_output.format
		);
	mdbgd_ischain("DNR OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.tdnr.control.cmd,
		region->parameter.tdnr.control.bypass,
		region->parameter.tdnr.otf_input.cmd,
		region->parameter.tdnr.otf_input.width,
		region->parameter.tdnr.otf_input.height,
		region->parameter.tdnr.otf_input.format
		);
	mdbgd_ischain("DNR OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.tdnr.control.cmd,
		region->parameter.tdnr.control.bypass,
		region->parameter.tdnr.otf_output.cmd,
		region->parameter.tdnr.otf_output.width,
		region->parameter.tdnr.otf_output.height,
		region->parameter.tdnr.otf_output.format
		);
	mdbgd_ischain("SCP OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.otf_input.cmd,
		region->parameter.scalerp.otf_input.width,
		region->parameter.scalerp.otf_input.height,
		region->parameter.scalerp.otf_input.format
		);
	mdbgd_ischain("SCP DO : %2d    %4d  %3d   %04dx%04d %4d,%d\n", device,
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.dma_output.cmd,
		region->parameter.scalerp.dma_output.width,
		region->parameter.scalerp.dma_output.height,
		region->parameter.scalerp.dma_output.format,
		region->parameter.scalerp.dma_output.plane
		);
	mdbgd_ischain("SCP OO : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.scalerp.control.cmd,
		region->parameter.scalerp.control.bypass,
		region->parameter.scalerp.otf_output.cmd,
		region->parameter.scalerp.otf_output.width,
		region->parameter.scalerp.otf_output.height,
		region->parameter.scalerp.otf_output.format
		);
	mdbgd_ischain("FD  OI : %2d    %4d  %3d   %04dx%04d    %3d\n", device,
		region->parameter.fd.control.cmd,
		region->parameter.fd.control.bypass,
		region->parameter.fd.otf_input.cmd,
		region->parameter.fd.otf_input.width,
		region->parameter.fd.otf_input.height,
		region->parameter.fd.otf_input.format
		);
	mdbgd_ischain(" NAME   CMD    IN_SZIE   OT_SIZE      CROP       POS\n", device);
	mdbgd_ischain("SCC CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerc.input_crop.cmd,
		region->parameter.scalerc.input_crop.in_width,
		region->parameter.scalerc.input_crop.in_height,
		region->parameter.scalerc.input_crop.out_width,
		region->parameter.scalerc.input_crop.out_height,
		region->parameter.scalerc.input_crop.crop_width,
		region->parameter.scalerc.input_crop.crop_height,
		region->parameter.scalerc.input_crop.pos_x,
		region->parameter.scalerc.input_crop.pos_y
		);
	mdbgd_ischain("SCC CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerc.output_crop.cmd,
		navailable,
		navailable,
		navailable,
		navailable,
		region->parameter.scalerc.output_crop.crop_width,
		region->parameter.scalerc.output_crop.crop_height,
		region->parameter.scalerc.output_crop.pos_x,
		region->parameter.scalerc.output_crop.pos_y
		);
	mdbgd_ischain("SCP CI :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerp.input_crop.cmd,
		region->parameter.scalerp.input_crop.in_width,
		region->parameter.scalerp.input_crop.in_height,
		region->parameter.scalerp.input_crop.out_width,
		region->parameter.scalerp.input_crop.out_height,
		region->parameter.scalerp.input_crop.crop_width,
		region->parameter.scalerp.input_crop.crop_height,
		region->parameter.scalerp.input_crop.pos_x,
		region->parameter.scalerp.input_crop.pos_y
		);
	mdbgd_ischain("SCP CO :  %d  %04dx%04d %04dx%04d %04dx%04d %04dx%04d\n", device,
		region->parameter.scalerp.output_crop.cmd,
		navailable,
		navailable,
		navailable,
		navailable,
		region->parameter.scalerp.output_crop.crop_width,
		region->parameter.scalerp.output_crop.crop_height,
		region->parameter.scalerp.output_crop.pos_x,
		region->parameter.scalerp.output_crop.pos_y
		);

	group |= GROUP_ID(device->group_3aa.id);
	group |= GROUP_ID(device->group_isp.id);

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group = GROUP_ID(GROUP_ID_3A0);

	setfile = (device->setfile & FIMC_IS_SETFILE_MASK);

	ret = fimc_is_hw_a_param(device->interface,
		device->instance,
		(group & GROUP_ID_PARM_MASK),
		setfile);
	return ret;
}

static int fimc_is_itf_enum(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_hw_enum(device->interface);
	if (ret) {
		merr("fimc_is_itf_enum is fail(%d)", device, ret);
		CALL_POPS(device, print_pwr, device->pdev);
		CALL_POPS(device, print_clk, device->pdev);
	}

	return ret;
}

static int fimc_is_itf_open(struct fimc_is_device_ischain *device,
	u32 module_id,
	u32 group_id,
	u32 flag,
	struct sensor_open_extended *ext_info)
{
	int ret = 0;
	struct is_region *region;
	struct fimc_is_interface *itf;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!device->sensor);
	BUG_ON(!device->interface);
	BUG_ON(!ext_info);

	region = device->is_region;
	itf = device->interface;

	memcpy(&region->shared[0], ext_info, sizeof(struct sensor_open_extended));

	fimc_is_ischain_region_flush(device);

	ret = fimc_is_hw_open(device->interface,
		device->instance,
		module_id,
		device->imemory.dvaddr_shared,
		group_id,
		flag,
		&device->margin_width,
		&device->margin_height);
	if (ret) {
		merr("fimc_is_hw_open is fail", device);
		CALL_POPS(device, print_cfg, device->pdev,
				fimc_is_sensor_g_instance(device->sensor));
		ret = -EINVAL;
		goto p_err;
	}

	/* HACK */
	device->margin_left = 8;
	device->margin_right = 8;
	device->margin_top = 6;
	device->margin_bottom = 4;
	device->margin_width = device->margin_left + device->margin_right;
	device->margin_height = device->margin_top + device->margin_bottom;
	mdbgd_ischain("margin %dx%d\n", device,
		device->margin_width, device->margin_height);

	fimc_is_ischain_region_invalid(device);

	if (region->shared[MAX_SHARED_COUNT-1] != MAGIC_NUMBER) {
		merr("MAGIC NUMBER error", device);
		ret = -EINVAL;
		goto p_err;
	}

	memset(&region->parameter, 0x0, sizeof(struct is_param_region));

	memcpy(&region->parameter.sensor, &init_sensor_param,
		sizeof(struct sensor_param));
	memcpy(&region->parameter.taa, &init_taa_param,
		sizeof(struct taa_param));
	memcpy(&region->parameter.isp, &init_isp_param,
		sizeof(struct isp_param));
	memcpy(&region->parameter.drc, &init_drc_param,
		sizeof(struct drc_param));
	memcpy(&region->parameter.scalerc, &init_scalerc_param,
		sizeof(struct scalerc_param));
	memcpy(&region->parameter.odc, &init_odc_param,
		sizeof(struct odc_param));
	memcpy(&region->parameter.dis, &init_dis_param,
		sizeof(struct dis_param));
	memcpy(&region->parameter.tdnr, &init_tdnr_param,
		sizeof(struct tdnr_param));
	memcpy(&region->parameter.scalerp, &init_scalerp_param,
		sizeof(struct scalerp_param));
	memcpy(&region->parameter.fd, &init_fd_param,
		sizeof(struct fd_param));

p_err:
	return ret;
}

static int fimc_is_itf_close(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	struct fimc_is_interface *itf;

	BUG_ON(!device);
	BUG_ON(!device->interface);

	itf = device->interface;

	ret = fimc_is_hw_close(itf, device->instance);
	if (ret) {
		merr("fimc_is_hw_close is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_itf_setfile(struct fimc_is_device_ischain *device,
	char *setfile_name)
{
	int ret = 0;
	u32 setfile_addr = 0;
	struct fimc_is_interface *itf;

	BUG_ON(!device);
	BUG_ON(!device->interface);
	BUG_ON(!setfile_name);

	itf = device->interface;

	mdbgd_ischain("%s(setfile : %s)\n", device, __func__, setfile_name);

	ret = fimc_is_hw_saddr(itf, device->instance, &setfile_addr);
	if (ret) {
		merr("fimc_is_hw_saddr is fail(%d)", device, ret);
		goto p_err;
	}

	if (!setfile_addr) {
		merr("setfile address is NULL", device);
		pr_err("cmd : %08X\n", readl(&itf->com_regs->ihcmd));
		pr_err("id : %08X\n", readl(&itf->com_regs->ihc_sensorid));
		pr_err("param1 : %08X\n", readl(&itf->com_regs->ihc_param1));
		pr_err("param2 : %08X\n", readl(&itf->com_regs->ihc_param2));
		pr_err("param3 : %08X\n", readl(&itf->com_regs->ihc_param3));
		pr_err("param4 : %08X\n", readl(&itf->com_regs->ihc_param4));
		goto p_err;
	}

	ret = fimc_is_ischain_loadsetf(device, setfile_addr, setfile_name);
	if (ret) {
		merr("fimc_is_ischain_loadsetf is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_hw_setfile(itf, device->instance);
	if (ret) {
		merr("fimc_is_hw_setfile is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_map(struct fimc_is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size)
{
	int ret = 0;

	BUG_ON(!device);

	mdbgd_ischain("%s()\n", device, __func__);

	ret = fimc_is_hw_map(device->interface, device->instance, group, shot_addr, shot_size);
	if (ret)
		merr("fimc_is_hw_map is fail(%d)", device, ret);

	return ret;
}

static int fimc_is_itf_unmap(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	mdbgd_ischain("%s()\n", device, __func__);

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group = GROUP_ID(GROUP_ID_3A0);

	ret = fimc_is_hw_unmap(device->interface, device->instance, group);
	if (ret)
		merr("fimc_is_hw_unmap is fail(%d)", device, ret);

	return ret;
}

int fimc_is_itf_stream_on(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 retry = 10000;
#ifdef ENABLE_DVFS
	int scenario_id;
#endif
	struct fimc_is_group *group_3aa, *group_isp;
	struct fimc_is_resourcemgr *resourcemgr;

	BUG_ON(!device);
	BUG_ON(!device->resourcemgr);

	resourcemgr = device->resourcemgr;
	group_3aa = &device->group_3aa;
	group_isp = &device->group_isp;

	/* 3ax, isp group should be started */
	if (!test_bit(FIMC_IS_GROUP_READY, &group_3aa->state)) {
		merr("group isp is not start", device);
		goto p_err;
	}

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) ||
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1)) {
		if (!test_bit(FIMC_IS_GROUP_READY, &group_3aa->state)) {
			merr("group 3ax is not start", device);
			goto p_err;
		}

		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group_3aa->state)) {
			while (--retry && (atomic_read(&group_3aa->scount) <
						group_3aa->async_shots)) {
				udelay(100);
			}
		}
	}

	if (retry)
		info("[ISC:D:%d] stream on ready\n", device->instance);
	else
		pr_err("[ISC:D:%d] stream on NOT ready\n", device->instance);

#ifdef ENABLE_DVFS
	mutex_lock(&resourcemgr->dvfs_ctrl.lock);
	if ((!pm_qos_request_active(&device->user_qos)) &&
			(sysfs_debug.en_dvfs)) {
		/* try to find dynamic scenario to apply */
		scenario_id = fimc_is_dvfs_sel_scenario(FIMC_IS_STATIC_SN, device);
		if (scenario_id >= 0) {
			info("[ISC:D:%d] static scenario(%d)\n",
					device->instance, scenario_id);
			fimc_is_set_dvfs(device, scenario_id);
		}
	}
	mutex_unlock(&resourcemgr->dvfs_ctrl.lock);
#endif
	ret = fimc_is_hw_stream_on(device->interface, device->instance);
	if (ret) {
		merr("fimc_is_hw_stream_on is fail(%d)", device, ret);
		CALL_POPS(device, print_clk, device->pdev);
	}

p_err:
	return ret;
}

int fimc_is_itf_stream_off(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	info("[ISC:D:%d] stream off ready\n", device->instance);

	ret = fimc_is_hw_stream_off(device->interface, device->instance);

	return ret;
}

int fimc_is_itf_process_start(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	ret = fimc_is_hw_process_on(device->interface,
		device->instance, group);

	return ret;
}

int fimc_is_itf_process_stop(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif
	ret = fimc_is_hw_process_off(device->interface,
		device->instance, group, 0);
#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif
	return ret;
}

int fimc_is_itf_force_stop(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
#endif
	/* if there's only one group of isp, send group id by 3a0 */
	if ((group & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group = GROUP_ID(GROUP_ID_3A0);
#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif
	ret = fimc_is_hw_process_off(device->interface,
		device->instance, group, 1);
#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif
	return ret;
}

static int fimc_is_itf_init_process_start(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;

	group |= GROUP_ID(device->group_3aa.id);
	group |= GROUP_ID(device->group_isp.id);

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group = GROUP_ID(GROUP_ID_3A0);

	ret = fimc_is_hw_process_on(device->interface,
		device->instance,
		(group & GROUP_ID_PARM_MASK));

	return ret;
}

static int fimc_is_itf_init_process_stop(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group = 0;

#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
#endif
	group |= GROUP_ID(device->group_3aa.id);
	group |= GROUP_ID(device->group_isp.id);

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group = GROUP_ID(GROUP_ID_3A0);
#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);

	}
#endif
	ret = fimc_is_hw_process_off(device->interface,
		device->instance, (group & GROUP_ID_PARM_MASK), 0);
#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif
	return ret;
}

int fimc_is_itf_i2c_lock(struct fimc_is_device_ischain *this,
			int i2c_clk, bool lock)
{
	int ret = 0;
	struct fimc_is_interface *itf = this->interface;

	if (lock)
		fimc_is_interface_lock(itf);

	ret = fimc_is_hw_i2c_lock(itf, this->instance,
				i2c_clk, lock);

	if (!lock)
		fimc_is_interface_unlock(itf);

	return ret;
}

int fimc_is_itf_g_capability(struct fimc_is_device_ischain *this)
{
	int ret = 0;
#ifdef PRINT_CAPABILITY
	u32 metadata;
	u32 index;
	struct camera2_sm *capability;
#endif

	ret = fimc_is_hw_g_capability(this->interface, this->instance,
		(this->imemory.kvaddr_shared - this->imemory.kvaddr));

	fimc_is_ischain_region_invalid(this);

#ifdef PRINT_CAPABILITY
	memcpy(&this->capability, &this->is_region->shared,
		sizeof(struct camera2_sm));
	capability = &this->capability;

	printk(KERN_INFO "===ColorC================================\n");
	printk(KERN_INFO "===ToneMapping===========================\n");
	metadata = capability->tonemap.maxCurvePoints;
	printk(KERN_INFO "maxCurvePoints : %d\n", metadata);

	printk(KERN_INFO "===Scaler================================\n");
	printk(KERN_INFO "foramt : %d, %d, %d, %d\n",
		capability->scaler.availableFormats[0],
		capability->scaler.availableFormats[1],
		capability->scaler.availableFormats[2],
		capability->scaler.availableFormats[3]);

	printk(KERN_INFO "===StatisTicsG===========================\n");
	index = 0;
	metadata = capability->stats.availableFaceDetectModes[index];
	while (metadata) {
		printk(KERN_INFO "availableFaceDetectModes : %d\n", metadata);
		index++;
		metadata = capability->stats.availableFaceDetectModes[index];
	}
	printk(KERN_INFO "maxFaceCount : %d\n",
		capability->stats.maxFaceCount);
	printk(KERN_INFO "histogrambucketCount : %d\n",
		capability->stats.histogramBucketCount);
	printk(KERN_INFO "maxHistogramCount : %d\n",
		capability->stats.maxHistogramCount);
	printk(KERN_INFO "sharpnessMapSize : %dx%d\n",
		capability->stats.sharpnessMapSize[0],
		capability->stats.sharpnessMapSize[1]);
	printk(KERN_INFO "maxSharpnessMapValue : %d\n",
		capability->stats.maxSharpnessMapValue);

	printk(KERN_INFO "===3A====================================\n");
	printk(KERN_INFO "maxRegions : %d\n", capability->aa.maxRegions);

	index = 0;
	metadata = capability->aa.aeAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.aeAvailableModes[index];
	}
	printk(KERN_INFO "aeCompensationStep : %d,%d\n",
		capability->aa.aeCompensationStep.num,
		capability->aa.aeCompensationStep.den);
	printk(KERN_INFO "aeCompensationRange : %d ~ %d\n",
		capability->aa.aeCompensationRange[0],
		capability->aa.aeCompensationRange[1]);
	index = 0;
	metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	while (metadata) {
		printk(KERN_INFO "TargetFpsRanges : %d ~ %d\n", metadata,
			capability->aa.aeAvailableTargetFpsRanges[index][1]);
		index++;
		metadata = capability->aa.aeAvailableTargetFpsRanges[index][0];
	}
	index = 0;
	metadata = capability->aa.aeAvailableAntibandingModes[index];
	while (metadata) {
		printk(KERN_INFO "aeAvailableAntibandingModes : %d\n",
			metadata);
		index++;
		metadata = capability->aa.aeAvailableAntibandingModes[index];
	}
	index = 0;
	metadata = capability->aa.awbAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "awbAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.awbAvailableModes[index];
	}
	index = 0;
	metadata = capability->aa.afAvailableModes[index];
	while (metadata) {
		printk(KERN_INFO "afAvailableModes : %d\n", metadata);
		index++;
		metadata = capability->aa.afAvailableModes[index];
	}
#endif
	return ret;
}

int fimc_is_itf_power_down(struct fimc_is_interface *interface)
{
	int ret = 0;
#ifdef ENABLE_CLOCK_GATE
	/* HACK */
	struct fimc_is_core *core = (struct fimc_is_core *)interface->core;
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, 0, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif
	ret = fimc_is_hw_power_down(interface, 0);
#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
		sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, 0, false);
#endif
	return ret;
}

int fimc_is_itf_sys_ctl(struct fimc_is_device_ischain *this,
			int cmd, int val)
{
	int ret = 0;
	struct fimc_is_interface *itf = this->interface;

	ret = fimc_is_hw_sys_ctl(itf, this->instance,
				cmd, val);

	return ret;
}

int fimc_is_itf_sensor_mode(struct fimc_is_device_ischain *ischain)
{
	struct fimc_is_device_sensor *sensor = ischain->sensor;

	return fimc_is_hw_sensor_mode(ischain->interface,
			ischain->instance,
			((sensor->mode << 16) | (ischain->module & 0xFFFF)));
}

static int fimc_is_itf_grp_shot(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame)
{
	int ret = 0;
	u32 group_id = 0;
#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core = (struct fimc_is_core *)device->interface->core;
#endif
	BUG_ON(!device);
	BUG_ON(!group);
	BUG_ON(!frame);
	BUG_ON(!frame->shot);

	/* Cache Flush */
	fimc_is_ischain_meta_flush(frame);

	if (frame->shot->magicNumber != SHOT_MAGIC_NUMBER) {
		merr("shot magic number error(0x%08X)\n", device, frame->shot->magicNumber);
		merr("shot_ext size : %d", device, sizeof(struct camera2_shot_ext));
		ret = -EINVAL;
		goto p_err;
	}

#ifdef DBG_STREAMING
	if (group->id == GROUP_ID_3A0)
		info("[3A0:D:%d] GRP%d SHOT(%d)\n", device->instance, group->id, frame->fcount);
	else if (group->id == GROUP_ID_3A1)
		info("[3A1:D:%d] GRP%d SHOT(%d)\n", device->instance, group->id, frame->fcount);
	else if (group->id == GROUP_ID_ISP)
		info("[ISP:D:%d] GRP%d SHOT(%d)\n", device->instance, group->id, frame->fcount);
	else if (group->id == GROUP_ID_DIS)
		info("[DIS:D:%d] GRP%d SHOT(%d)\n", device->instance, group->id, frame->fcount);
	else
		info("[ERR:D:%d] GRP%d SHOT(%d)\n", device->instance, group->id, frame->fcount);
#endif

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
	do_gettimeofday(&frame->time_shot);
#endif
#ifdef EXTERNAL_TIME
	do_gettimeofday(&frame->tzone[TM_SHOT]);
#endif
#endif

#ifdef ENABLE_CLOCK_GATE
	/* HACK */
	/* dynamic clock on */
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_set(core, group->id, true, false);
#endif
	group_id = GROUP_ID(group->id);

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group_id & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group_id = GROUP_ID(GROUP_ID_3A0);

	ret = fimc_is_hw_shot_nblk(device->interface,
		device->instance,
		group_id,
		frame->dvaddr_buffer[0],
		frame->dvaddr_shot,
		frame->fcount,
		frame->rcount);

p_err:
	return ret;
}

int fimc_is_ischain_probe(struct fimc_is_device_ischain *device,
	struct fimc_is_interface *interface,
	struct fimc_is_resourcemgr *resourcemgr,
	struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_mem *mem,
	struct platform_device *pdev,
	u32 instance,
	u32 regs)
{
	int ret = 0;
	struct fimc_is_subdev *scc, *dis, *scp;

	BUG_ON(!interface);
	BUG_ON(!mem);
	BUG_ON(!pdev);
	BUG_ON(!device);

	/*device initialization should be just one time*/
	scc = &device->scc;
	dis = &device->dis;
	scp = &device->scp;

	device->interface	= interface;
	device->mem		= mem;
	device->pdev		= pdev;
	device->pdata		= pdev->dev.platform_data;
	device->regs		= (void *)regs;
	device->instance	= instance;
	device->groupmgr	= groupmgr;
	device->resourcemgr	= resourcemgr;
	device->sensor		= NULL;
	device->margin_left	= 0;
	device->margin_right	= 0;
	device->margin_width	= 0;
	device->margin_top	= 0;
	device->margin_bottom	= 0;
	device->margin_height	= 0;
	device->sensor_width	= 0;
	device->sensor_height	= 0;
	device->dis_width	= 0;
	device->dis_height	= 0;
	device->chain0_width	= 0;
	device->chain0_height	= 0;
	device->chain1_width	= 0;
	device->chain1_height	= 0;
	device->chain2_width	= 0;
	device->chain2_height	= 0;
	device->chain3_width	= 0;
	device->chain3_height	= 0;
	device->crop_x		= 0;
	device->crop_y		= 0;
	device->crop_width	= 0;
	device->crop_height	= 0;
	device->setfile		= 0;
	device->dzoom_width	= 0;
	device->force_down	= false;
	device->is_region	= NULL;

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0))
		fimc_is_group_probe(groupmgr, &device->group_3aa, ENTRY_3AA);

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, isp))
		fimc_is_group_probe(groupmgr, &device->group_isp, ENTRY_ISP);

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, dis))
		fimc_is_group_probe(groupmgr, &device->group_dis, ENTRY_DIS);

	device->drc.entry = ENTRY_DRC;
	device->scc.entry = ENTRY_SCALERC;
	device->dis.entry = ENTRY_DIS;
	device->dnr.entry = ENTRY_TDNR;
	device->scp.entry = ENTRY_SCALERP;
	device->fd.entry = ENTRY_LHFD;
	device->taac.entry = ENTRY_3AAC;
	device->taap.entry = ENTRY_3AAP;

	clear_bit(FIMC_IS_ISCHAIN_OPEN, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_LOADED, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state);

	/* clear group open state */
	clear_bit(FIMC_IS_GROUP_OPEN, &device->group_3aa.state);
	clear_bit(FIMC_IS_GROUP_OPEN, &device->group_isp.state);
	clear_bit(FIMC_IS_GROUP_OPEN, &device->group_dis.state);

	/* clear subdevice state */
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->group_3aa.leader.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->group_isp.leader.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->group_dis.leader.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->drc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->scc.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->dis.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->dnr.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->scp.state);
	clear_bit(FIMC_IS_SUBDEV_OPEN, &device->fd.state);

	clear_bit(FIMC_IS_SUBDEV_START, &device->group_3aa.leader.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->group_isp.leader.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->group_dis.leader.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->drc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->scc.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->dis.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->dnr.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->scp.state);
	clear_bit(FIMC_IS_SUBDEV_START, &device->fd.state);

	mutex_init(&device->mutex_state);

#ifdef FW_DEBUG
	debugfs_root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (debugfs_root)
		mdbgd_ischain("debugfs %s is created\n", device, DEBUG_FS_ROOT_NAME);

	debugfs_file = debugfs_create_file(DEBUG_FS_FILE_NAME, S_IRUSR,
		debugfs_root, device, &debug_fops);
	if (debugfs_file)
		mdbgd_ischain("debugfs %s is created\n", device, DEBUG_FS_FILE_NAME);
#endif

	return ret;
}

int fimc_is_ischain_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx,
	struct fimc_is_minfo *minfo)
{
	int ret = 0;
	struct fimc_is_ishcain_mem *imemory;
#ifdef ENABLE_CLOCK_GATE
	struct fimc_is_core *core;
#endif
	BUG_ON(!device);
	BUG_ON(!device->groupmgr);
	BUG_ON(!vctx);
	BUG_ON(!minfo);

	if (test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
		merr("already open", device);
		ret = -EMFILE;
		goto p_err;
	}

#ifndef RESERVED_MEM
	if (device->instance == 0) {
		/* 1. init memory */
		ret = fimc_is_ishcain_initmem(device);
		if (ret) {
			err("fimc_is_ishcain_initmem is fail(%d)\n", ret);
			goto p_err;
		}
	}
#endif

	clear_bit(FIMC_IS_ISCHAIN_LOADED, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_POWER_ON, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state);
	clear_bit(FIMC_IS_ISHCAIN_START, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state);

	/* 2. Init variables */
	memset(&device->cur_peri_ctl, 0,
		sizeof(struct camera2_uctl));
	memset(&device->peri_ctls, 0,
		sizeof(struct camera2_uctl)*SENSOR_MAX_CTL);
	memset(&device->capability, 0,
		sizeof(struct camera2_sm));

	/* initial state, it's real apply to setting when opening*/
	device->margin_left	= 0;
	device->margin_right	= 0;
	device->margin_width	= 0;
	device->margin_top	= 0;
	device->margin_bottom	= 0;
	device->margin_height	= 0;
	device->sensor_width	= 0;
	device->sensor_height	= 0;
	device->dis_width	= 0;
	device->dis_height	= 0;
	device->chain0_width	= 0;
	device->chain0_height	= 0;
	device->chain1_width	= 0;
	device->chain1_height	= 0;
	device->chain2_width	= 0;
	device->chain2_height	= 0;
	device->chain3_width	= 0;
	device->chain3_height	= 0;
	device->crop_x		= 0;
	device->crop_y		= 0;
	device->crop_width	= 0;
	device->crop_height	= 0;
	device->setfile		= 0;
	device->setfile	|= (FIMC_IS_CRANGE_FULL << FIMC_IS_ISP_CRANGE_SHIFT);
	device->setfile	|= (FIMC_IS_CRANGE_FULL << FIMC_IS_SCC_CRANGE_SHIFT);
	device->setfile	|= (FIMC_IS_CRANGE_FULL << FIMC_IS_SCP_CRANGE_SHIFT);
	device->setfile	|= ISS_SUB_SCENARIO_STILL_PREVIEW;
	device->dzoom_width	= 0;
	device->force_down	= false;
	device->sensor		= NULL;
	device->module		= 0;

	imemory			= &device->imemory;
	imemory->base		= minfo->base;
	imemory->size		= minfo->size;
	imemory->vaddr_base	= minfo->vaddr_base;
	imemory->vaddr_curr	= minfo->vaddr_curr;
	imemory->fw_cookie	= minfo->fw_cookie;
	imemory->dvaddr		= minfo->dvaddr;
	imemory->kvaddr		= minfo->kvaddr;
	imemory->dvaddr_odc	= minfo->dvaddr_odc;
	imemory->kvaddr_odc	= minfo->kvaddr_odc;
	imemory->dvaddr_dis	= minfo->dvaddr_dis;
	imemory->kvaddr_dis	= minfo->kvaddr_dis;
	imemory->dvaddr_3dnr	= minfo->dvaddr_3dnr;
	imemory->kvaddr_3dnr	= minfo->kvaddr_3dnr;
	imemory->offset_region	= (FIMC_IS_A5_MEM_SIZE -
		((device->instance + 1) * FIMC_IS_REGION_SIZE));
	imemory->dvaddr_region	= imemory->dvaddr + imemory->offset_region;
	imemory->kvaddr_region	= imemory->kvaddr + imemory->offset_region;
	imemory->is_region	= (struct is_region *)imemory->kvaddr_region;
	imemory->offset_shared	= (u32)&imemory->is_region->shared[0] -
		imemory->kvaddr;
	imemory->dvaddr_shared	= imemory->dvaddr + imemory->offset_shared;
	imemory->kvaddr_shared	= imemory->kvaddr + imemory->offset_shared;
	device->is_region = imemory->is_region;

	fimc_is_group_open(device->groupmgr, &device->group_isp, GROUP_ID_ISP,
		device->instance, vctx, device, fimc_is_ischain_isp_callback);

	/* subdev open */
	fimc_is_subdev_open(&device->drc, NULL, &init_drc_param.control);
	fimc_is_subdev_open(&device->dis, NULL, &init_dis_param.control);
	fimc_is_subdev_open(&device->dnr, NULL, &init_tdnr_param.control);
	/* FD see only control.command not bypass */
	fimc_is_subdev_open(&device->fd, NULL, NULL);

	/* for mediaserver force close */
	ret = fimc_is_resource_get(device->resourcemgr);
	if (ret) {
		merr("fimc_is_resource_get is fail", device);
		fimc_is_resource_put(device->resourcemgr);
		goto p_err;
	}

	if (device->instance == 0) {
		/* 5. A5 power on */
		ret = fimc_is_ischain_power(device, 1);
		if (ret) {
			err("failed to fimc_is_ischain_power (%d)\n", ret);
			fimc_is_resource_put(device->resourcemgr);
			ret = -EINVAL;
			goto p_err;
		}

		/* W/A for a lower version MCUCTL */
		fimc_is_interface_reset(device->interface);

		mdbgd_ischain("power up and loaded firmware\n", device);
#ifdef ENABLE_CLOCK_GATE
		if (sysfs_debug.en_clk_gate &&
				sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
			core = (struct fimc_is_core *)device->interface->core;
			fimc_is_clk_gate_init(core);
		}
#endif
	}

	set_bit(FIMC_IS_ISCHAIN_OPEN, &device->state);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		core = (struct fimc_is_core *)device->interface->core;
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif
p_err:
	info("[ISC:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_ischain_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	int refcount;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct fimc_is_core *core;
	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	leader = &group->leader;
	queue = GET_SRC_QUEUE(vctx);
	core = (struct fimc_is_core *)device->interface->core;
	refcount = atomic_read(&vctx->video->refcount);
	if (refcount < 0) {
		merr("invalid ischain refcount", device);
		ret = -ENODEV;
		goto exit;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state)) {
		merr("already close", device);
		ret = -EMFILE;
		goto exit;
	}

#ifdef ENABLE_CLOCK_GATE
	core = (struct fimc_is_core *)device->interface->core;
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST) {
		fimc_is_clk_gate_lock_set(core, device->instance, true);
		fimc_is_wrap_clk_gate_set(core, (1 << GROUP_ID_MAX) - 1, true);
	}
#endif
	/* 1. Stop all request */
	ret = fimc_is_ischain_isp_stop(device, leader, queue);
	if (ret)
		merr("fimc_is_ischain_isp_stop is fail", device);

	/* group close */
	ret = fimc_is_group_close(groupmgr, group);
	if (ret)
		merr("fimc_is_group_close is fail", device);

	/* subdev close */
	fimc_is_subdev_close(&device->drc);
	fimc_is_subdev_close(&device->dis);
	fimc_is_subdev_close(&device->dnr);
	fimc_is_subdev_close(&device->fd);

	/* CLOSE_SENSOR */
	if (test_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state)) {
		ret = fimc_is_itf_close(device);
		if (ret)
			merr("fimc_is_itf_close is fail", device);
	}

	/* for mediaserver force close */
	ret = fimc_is_resource_put(device->resourcemgr);
	if (ret) {
		merr("fimc_is_resource_put is fail", device);
		goto exit;
	}

	clear_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state);
	clear_bit(FIMC_IS_ISCHAIN_OPEN, &device->state);

#ifdef ENABLE_CLOCK_GATE
	if (sysfs_debug.en_clk_gate &&
			sysfs_debug.clk_gate_mode == CLOCK_GATE_MODE_HOST)
		fimc_is_clk_gate_lock_set(core, device->instance, false);
#endif
exit:
	pr_info("[ISC:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_ischain_init(struct fimc_is_device_ischain *device,
	u32 module_id,
	u32 group_id,
	u32 video_id,
	u32 flag)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_core *core
		= (struct fimc_is_core *)platform_get_drvdata(device->pdev);

	BUG_ON(!device);
	BUG_ON(!device->sensor);

	mdbgd_ischain("%s(module : %d)\n", device, __func__, module_id);

	sensor = device->sensor;

	if (test_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state)) {
		mwarn("sensor is already open", device);
		goto p_err;
	}

	if (!test_bit(FIMC_IS_SENSOR_OPEN, &sensor->state)) {
		merr("I2C gpio is not yet set", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_g_module(sensor, &module);
	if (ret) {
		merr("fimc_is_sensor_g_module is fail(%d)", device, ret);
		goto p_err;
	}

	if (module->id != module_id) {
		merr("module id is invalid(%d != %d)", device, module->id, module_id);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_ischain_isp_s_input(device, video_id);
	if (ret) {
		merr("fimc_is_ischain_isp_s_input is fail(%d)", device, ret);
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		/* sensor instance means flite channel */
		if(sensor->instance == 0){
			/* Load calibration data from sensor */
			ret = fimc_is_ischain_loadcalb(device, NULL);
			if (ret) {
				err("loadcalb fail");
				goto p_err;
			}
		}
	}

	/* FW loading of peripheral device */
	if ((module->position == SENSOR_POSITION_REAR)
		&& !test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		/* set target spi channel */
		if (TARGET_SPI_CH_FOR_PERI == 0)
			core->t_spi = core->spi0;
		else
			core->t_spi = core->spi1;

		if (fimc_is_comp_is_valid(core) == 0) {
			ret = fimc_is_comp_loadfirm(core);
			if (ret) {
				err("fimc_is_comp_loadfirm() fail");
				goto p_err;
			}

			ret = fimc_is_comp_loadsetf(core);
			if (ret) {
				err("fimc_is_comp_loadsetf() fail");
				goto p_err;
			}
		} else {
			module->ext.companion_con.product_name
				= COMPANION_NAME_NOTHING;
		}
	}

	if ((device->instance) == 0) {
		ret = fimc_is_itf_enum(device);
		if (ret) {
			err("enum fail");
			goto p_err;
		}
	}

#if (FW_HAS_SENSOR_MODE_CMD)
	ret = fimc_is_itf_open(device, ((sensor->mode << 16) | (module_id & 0xFFFF)),
			group_id, flag, &module->ext);
#else
	ret = fimc_is_itf_open(device, module_id, group_id, flag, &module->ext);
#endif
	if (ret) {
		merr("open fail", device);
		goto p_err;
	}

	ret = fimc_is_itf_setfile(device, module->setfile_name);
	if (ret) {
		merr("setfile fail", device);
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		ret = fimc_is_itf_stream_off(device);
		if (ret) {
			merr("streamoff fail", device);
			goto p_err;
		}
	}

	ret = fimc_is_itf_init_process_stop(device);
	if (ret) {
		merr("fimc_is_itf_init_process_stop is fail", device);
		goto p_err;
	}

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		measure_period(&device->group_3aa.time, 1);
		measure_period(&device->group_isp.time, 1);
		measure_period(&device->group_dis.time, 1);
	} else {
		measure_period(&device->group_3aa.time, 66);
		measure_period(&device->group_isp.time, 66);
		measure_period(&device->group_dis.time, 66);
	}
#endif
#endif

	device->module = module_id;
	set_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state);

p_err:
	return ret;
}

static int fimc_is_ischain_s_setfile(struct fimc_is_device_ischain *device,
	u32 setfile, u32 *lindex, u32 *hindex, u32 *indexes)
{
	int ret = 0;
	u32 crange;
	struct param_otf_output *otf_output;
	struct param_scaler_imageeffect *scaler_effect;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	info("[ISC:D:%d] setfile: 0x%08X\n", device->instance, setfile);

	if ((setfile & FIMC_IS_SETFILE_MASK) >= ISS_SUB_END) {
		merr("setfile id(%d) is invalid", device,
				(setfile & FIMC_IS_SETFILE_MASK));
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * Color Range
	 * 0 : Wide range
	 * 1 : Narrow range
	 */
	otf_output = fimc_is_itf_g_param(device, NULL, PARAM_ISP_OTF_OUTPUT);
	crange = (setfile & FIMC_IS_ISP_CRANGE_MASK) >> FIMC_IS_ISP_CRANGE_SHIFT;
	if (crange)
		otf_output->format = OTF_OUTPUT_FORMAT_YUV444_TRUNCATED;
	else
		otf_output->format = OTF_OUTPUT_FORMAT_YUV444;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_OUTPUT);

	info("[ISC:D:%d] ISP color range: %s\n", device->instance,
			(crange ? "limited" : "full"));

	scaler_effect = fimc_is_itf_g_param(device, NULL, PARAM_SCALERC_IMAGE_EFFECT);
	crange = (setfile & FIMC_IS_SCC_CRANGE_MASK) >> FIMC_IS_SCC_CRANGE_SHIFT;
	if (crange)
		scaler_effect->yuv_range = SCALER_OUTPUT_YUV_RANGE_NARROW;
	else
		scaler_effect->yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_IMAGE_EFFECT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_IMAGE_EFFECT);
	(*indexes)++;

	info("[ISC:D:%d] SCC color range: %s\n", device->instance,
			(crange ? "limited" : "full"));

	scaler_effect = fimc_is_itf_g_param(device, NULL, PARAM_SCALERP_IMAGE_EFFECT);
	crange = (setfile & FIMC_IS_SCP_CRANGE_MASK) >> FIMC_IS_SCP_CRANGE_SHIFT;
	if (crange)
		scaler_effect->yuv_range = SCALER_OUTPUT_YUV_RANGE_NARROW;
	else
		scaler_effect->yuv_range = SCALER_OUTPUT_YUV_RANGE_FULL;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_IMAGE_EFFECT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_IMAGE_EFFECT);
	(*indexes)++;

	info("[ISC:D:%d] SCP color range: %s\n", device->instance,
			(crange ? "limited" : "full"));

p_err:
	return ret;
}

static int fimc_is_ischain_s_3aa_size(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 *input_crop,
	u32 *output_crop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *group_3aa;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct param_control *taa_control;
	struct param_otf_input *taa_otf_input;
	struct param_dma_input *taa_dma_input;
	u32 binning;
	u32 bns_binning;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);
	BUG_ON(!device->sensor);

	binning = fimc_is_sensor_g_bratio(device->sensor);
	bns_binning = fimc_is_sensor_g_bns_ratio(device->sensor);

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0))
		group_3aa = &device->group_3aa;
	else
		group_3aa = &device->group_isp;
	leader = &group_3aa->leader;
	queue = GET_LEADER_QUEUE(leader);
	if (!queue) {
		merr("get queue fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	taa_control = fimc_is_itf_g_param(device, frame, PARAM_3AA_CONTROL);
	taa_control->cmd = CONTROL_COMMAND_START;
	taa_control->bypass = CONTROL_BYPASS_DISABLE;
	taa_control->run_mode = 1;
	*lindex |= LOWBIT_OF(PARAM_3AA_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_3AA_CONTROL);
	(*indexes)++;

	taa_otf_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_OTF_INPUT);

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_3aa.state))
		taa_otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		taa_otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;

	taa_otf_input->width = device->sensor_width;
	taa_otf_input->height = device->sensor_height;
	taa_otf_input->bayer_crop_enable = 1;
	taa_otf_input->bayer_crop_offset_x = input_crop[0];
	taa_otf_input->bayer_crop_offset_y = input_crop[1];
	taa_otf_input->bayer_crop_width = input_crop[2];
	taa_otf_input->bayer_crop_height = input_crop[3];
	taa_otf_input->sensor_binning_ratio_x = binning;
	taa_otf_input->sensor_binning_ratio_y = binning;

	taa_otf_input->bns_binning_enable = 1;
	taa_otf_input->bns_binning_ratio_x = bns_binning;
	taa_otf_input->bns_binning_ratio_y = bns_binning;
	taa_otf_input->bns_margin_left = 0;
	taa_otf_input->bns_margin_top = 0;
	taa_otf_input->bns_output_width = device->bns_width;
	taa_otf_input->bns_output_height = device->bns_height;

	taa_otf_input->format = OTF_INPUT_FORMAT_BAYER;
	taa_otf_input->bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
	taa_otf_input->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	taa_otf_input->frametime_min = 0;
	taa_otf_input->frametime_max = 1000000;
	*lindex |= LOWBIT_OF(PARAM_3AA_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_OTF_INPUT);
	(*indexes)++;

	taa_dma_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA1_INPUT);

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_3aa.state))
		taa_dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	else
		taa_dma_input->cmd = DMA_INPUT_COMMAND_BUF_MNGR;

	taa_dma_input->width = device->sensor_width;
	taa_dma_input->height = device->sensor_height;
	taa_dma_input->dma_crop_offset_x = 0;
	taa_dma_input->dma_crop_offset_y = 0;
	taa_dma_input->dma_crop_width = device->sensor_width - device->margin_width;
	taa_dma_input->dma_crop_height = device->sensor_height - device->margin_height;
	taa_dma_input->bayer_crop_enable = 1;
	taa_dma_input->bayer_crop_offset_x = input_crop[0];
	taa_dma_input->bayer_crop_offset_y = input_crop[1];
	taa_dma_input->bayer_crop_width = input_crop[2];
	taa_dma_input->bayer_crop_height = input_crop[3];

	if (!GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0)) {
		taa_dma_input->bds_out_enable = ISP_BDS_COMMAND_ENABLE;
		taa_dma_input->bds_out_width = output_crop[2];
		taa_dma_input->bds_out_height = output_crop[3];
	}

	taa_dma_input->user_min_frame_time = 0;
	taa_dma_input->user_max_frame_time = 1000000;

	if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR12) {
		taa_dma_input->format = DMA_INPUT_FORMAT_BAYER_PACKED12;
	} else if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR16) {
		taa_dma_input->format = DMA_INPUT_FORMAT_BAYER;
	} else {
		merr("Invalid bayer format(%d)", device, queue->framecfg.format.pixelformat);
		ret = -EINVAL;
		goto p_err;
	}

	taa_dma_input->bitwidth = DMA_INPUT_BIT_WIDTH_10BIT;
	taa_dma_input->order = DMA_INPUT_ORDER_GR_BG;
	taa_dma_input->plane = 1;
	taa_dma_input->buffer_number = 0;
	taa_dma_input->buffer_address = 0;
	taa_dma_input->sensor_binning_ratio_x = binning;
	taa_dma_input->sensor_binning_ratio_y = binning;
	/*
	 * hidden spec
	 *     [0] : sensor size is dma input size
	 *     [X] : sensor size is reserved field
	 */
	taa_dma_input->reserved[1] = 0;
	taa_dma_input->reserved[2] = 0;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA1_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA1_INPUT);
	(*indexes)++;

p_err:
	return ret;
}

static int fimc_is_ischain_s_chain0_size(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 width,
	u32 height,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct fimc_is_group *group_isp;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;
	struct param_otf_input *otf_input;
	struct param_dma_input *dma_input;
	struct param_otf_output *otf_output;
	struct param_scaler_input_crop *input_crop;
	u32 chain0_width, chain0_height;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	chain0_width = width;
	chain0_height = height;

	group_isp = &device->group_isp;
	if (!group_isp) {
		merr("get gourp_isp fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	leader = &group_isp->leader;
	if (!leader) {
		merr("get leader fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	queue = GET_LEADER_QUEUE(leader);
	if (!queue) {
		merr("get queue fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	mdbgd_ischain("request chain0 size : %dx%d\n", device, chain0_width, chain0_height);

	/* ISP */
	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0)) {
		dma_input = fimc_is_itf_g_param(device, frame, PARAM_ISP_VDMA1_INPUT);
		dma_input->cmd = DMA_INPUT_COMMAND_BUF_MNGR;
		dma_input->width = chain0_width;
		dma_input->height = chain0_height;
		dma_input->bitwidth = DMA_INPUT_BIT_WIDTH_10BIT;

		if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR12) {
			dma_input->format = DMA_INPUT_FORMAT_BAYER_PACKED12;
		} else if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR16) {
			dma_input->format = DMA_INPUT_FORMAT_BAYER;
		} else {
			merr("Invalid bayer format(%d)", device, queue->framecfg.format.pixelformat);
			ret = -EINVAL;
			goto p_err;
		}

		*lindex |= LOWBIT_OF(PARAM_ISP_VDMA1_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_ISP_VDMA1_INPUT);
		(*indexes)++;
	}

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_ISP_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = chain0_width;
	otf_output->height = chain0_height;
	otf_output->format = OTF_OUTPUT_FORMAT_YUV444;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_ISP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ISP_OTF_OUTPUT);
	(*indexes)++;

	/* DRC */
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_DRC_OTF_INPUT);
	otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	otf_input->width = chain0_width;
	otf_input->height = chain0_height;
	*lindex |= LOWBIT_OF(PARAM_DRC_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_DRC_OTF_INPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_DRC_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = chain0_width;
	otf_output->height = chain0_height;
	*lindex |= LOWBIT_OF(PARAM_DRC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_DRC_OTF_OUTPUT);
	(*indexes)++;

	/* SCC */
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_INPUT);
	otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	otf_input->width = chain0_width;
	otf_input->height = chain0_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_INPUT);
	(*indexes)++;

	/* SCC CROP */
	input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_INPUT_CROP);
	input_crop->pos_x = 0;
	input_crop->pos_y = 0;
	input_crop->crop_width = chain0_width;
	input_crop->crop_height = chain0_height;
	input_crop->in_width = chain0_width;
	input_crop->in_height = chain0_height;

	*lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	(*indexes)++;

	device->bds_width = width;
	device->bds_height = height;

p_err:
	return ret;
}

static int fimc_is_ischain_s_chain1_size(struct fimc_is_device_ischain *device,
	u32 width, u32 height, u32 *lindex, u32 *hindex, u32 *indexes)
{
	int ret = 0;
	struct scalerc_param *scc_param;
	struct odc_param *odc_param;
	struct dis_param *dis_param;
	u32 chain1_width, chain1_height;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		return 0;

	scc_param = &device->is_region->parameter.scalerc;
	odc_param = &device->is_region->parameter.odc;
	dis_param = &device->is_region->parameter.dis;
	chain1_width = width;
	chain1_height = height;

	mdbgd_ischain("current chain1 size : %dx%d\n", device,
		device->chain1_width, device->chain1_height);
	mdbgd_ischain("request chain1 size : %dx%d\n", device,
		chain1_width, chain1_height);

	if (!chain1_width) {
		err("chain1 width is zero");
		ret = -EINVAL;
		goto exit;
	}

	if (!chain1_height) {
		err("chain1 height is zero");
		ret = -EINVAL;
		goto exit;
	}

	/* SCC OUTPUT */
	scc_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_param->input_crop.out_width = chain1_width;
	scc_param->input_crop.out_height = chain1_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	(*indexes)++;

	scc_param->output_crop.cmd = SCALER_CROP_COMMAND_DISABLE;
	scc_param->output_crop.pos_x = 0;
	scc_param->output_crop.pos_y = 0;
	scc_param->output_crop.crop_width = chain1_width;
	scc_param->output_crop.crop_height = chain1_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	(*indexes)++;

	scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	scc_param->otf_output.width = chain1_width;
	scc_param->otf_output.height = chain1_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	(*indexes)++;

	/* ODC */
	odc_param->otf_input.width = chain1_width;
	odc_param->otf_input.height = chain1_height;
	*lindex |= LOWBIT_OF(PARAM_ODC_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_ODC_OTF_INPUT);
	(*indexes)++;

	odc_param->otf_output.width = chain1_width;
	odc_param->otf_output.height = chain1_height;
	*lindex |= LOWBIT_OF(PARAM_ODC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_ODC_OTF_OUTPUT);
	(*indexes)++;

	/* DIS INPUT */
	dis_param->otf_input.width = chain1_width;
	dis_param->otf_input.height = chain1_height;
	*lindex |= LOWBIT_OF(PARAM_DIS_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_DIS_OTF_INPUT);
	(*indexes)++;

exit:
	return ret;
}

static int fimc_is_ischain_s_chain2_size(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 width,
	u32 height,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_output *dma_output;
	u32 chain2_width, chain2_height;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		return 0;

	mdbgd_ischain("request chain2 size : %dx%d\n", device, width, height);
	mdbgd_ischain("current chain2 size : %dx%d\n",
		device, device->chain2_width, device->chain2_height);

	/* CALCULATION */
	chain2_width = width;
	chain2_height = height;

	/* DIS OUTPUT */
	otf_output = fimc_is_itf_g_param(device, frame, PARAM_DIS_OTF_OUTPUT);
	otf_output->width = chain2_width;
	otf_output->height = chain2_height;
	*lindex |= LOWBIT_OF(PARAM_DIS_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_DIS_OTF_OUTPUT);
	(*indexes)++;

	otf_input = fimc_is_itf_g_param(device, frame, PARAM_TDNR_OTF_INPUT);
	otf_input->width = chain2_width;
	otf_input->height = chain2_height;
	*lindex |= LOWBIT_OF(PARAM_TDNR_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_OTF_INPUT);
	(*indexes)++;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_TDNR_DMA_OUTPUT);
	dma_output->width = chain2_width;
	dma_output->height = chain2_height;
	*lindex |= LOWBIT_OF(PARAM_TDNR_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_DMA_OUTPUT);
	(*indexes)++;

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_TDNR_OTF_OUTPUT);
	otf_output->width = chain2_width;
	otf_output->height = chain2_height;
	*lindex |= LOWBIT_OF(PARAM_TDNR_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_OTF_OUTPUT);
	(*indexes)++;

	otf_input = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_INPUT);
	otf_input->width = chain2_width;
	otf_input->height = chain2_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_INPUT);
	(*indexes)++;

	return ret;
}

/**
 Utility function to adjust output crop size based on the
 H/W limitation of SCP scaling.
 output_crop_w and output_crop_h are call-by reference parameter,
 which contain intended cropping size. Adjusted size will be stored on
 those parameters when this function returns.
 */
static int fimc_is_ischain_scp_adjust_crop(struct fimc_is_device_ischain *device,
	struct scalerp_param *scp_param,
	u32 *output_crop_w, u32 *output_crop_h)
{
	int changed = 0;

	if (*output_crop_w > scp_param->otf_input.width * 4) {
		mwarn("Cannot be scaled up beyond 4 times(%d -> %d)",
			device, scp_param->otf_input.width, *output_crop_w);
		*output_crop_w = scp_param->otf_input.width * 4;
		changed |= 0x01;
	}

	if (*output_crop_h > scp_param->otf_input.height * 4) {
		mwarn("Cannot be scaled up beyond 4 times(%d -> %d)",
			device, scp_param->otf_input.height, *output_crop_h);
		*output_crop_h = scp_param->otf_input.height * 4;
		changed |= 0x02;
	}

	if (*output_crop_w < (scp_param->otf_input.width + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scp_param->otf_input.width, *output_crop_w);
		*output_crop_w = (scp_param->otf_input.width + 15) / 16;
		changed |= 0x10;
	}

	if (*output_crop_h < (scp_param->otf_input.height + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scp_param->otf_input.height, *output_crop_h);
		*output_crop_h = (scp_param->otf_input.height + 15) / 16;
		changed |= 0x20;
	}

	return changed;
}

static int fimc_is_ischain_s_chain3_size(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	u32 width,
	u32 height,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_scaler_input_crop *input_crop;
	struct param_scaler_output_crop *output_crop;
	struct param_otf_input *otf_input;
	struct param_otf_output *otf_output;
	struct param_dma_output *dma_output;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_queue *queue;
	struct scalerp_param *scp_param;
	u32 chain2_width, chain2_height;
	u32 chain3_width, chain3_height;
	u32 scp_crop_width, scp_crop_height;
	u32 scp_crop_x, scp_crop_y;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state))
		return 0;

	/* Adjust output crop to prevent exceeding SCP limitation */
	scp_param = &device->is_region->parameter.scalerp;
	fimc_is_ischain_scp_adjust_crop(device, scp_param, &width, &height);

	vctx = device->scp.vctx;
	queue = &vctx->q_dst;

	chain2_width = device->chain2_width;
	chain2_height = device->chain2_height;
	chain3_width = width;
	chain3_height = height;

	scp_crop_x = 0;
	scp_crop_y = 0;
	scp_crop_width = chain2_width;
	scp_crop_height = chain2_height;

	mdbgd_ischain("request chain3 size : %dx%d\n", device, width, height);
	mdbgd_ischain("current chain3 size : %dx%d\n",
		device, device->chain3_width, device->chain3_height);

	/* SCP */
	input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_INPUT_CROP);
	input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	input_crop->pos_x = scp_crop_x;
	input_crop->pos_y = scp_crop_y;
	input_crop->crop_width = scp_crop_width;
	input_crop->crop_height = scp_crop_height;
	input_crop->in_width = chain2_width;
	input_crop->in_height = chain2_height;
	input_crop->out_width = chain3_width;
	input_crop->out_height = chain3_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_INPUT_CROP);
	(*indexes)++;

	/*
	 * scaler can't apply stride to each plane, only y plane.
	 * basically cb, cr plane should be half of y plane,
	 * and it's automatically set
	 *
	 * 3 plane : all plane should be 8 or 16 stride
	 * 2 plane : y plane should be 32, 16 stride, others should be half stride of y
	 * 1 plane : all plane should be 8 stride
	 */
	/*
	 * limitation of output_crop.pos_x and pos_y
	 * YUV422 3P, YUV420 3P : pos_x and pos_y should be x2
	 * YUV422 1P : pos_x should be x2
	 */
	output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OUTPUT_CROP);
	if (queue->framecfg.width_stride[0]) {
		output_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
		output_crop->pos_x = 0;
		output_crop->pos_y = 0;
		output_crop->crop_width = chain3_width + queue->framecfg.width_stride[0];
		output_crop->crop_height = chain3_height;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
		(*indexes)++;
	} else {
		output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
		(*indexes)++;
	}

	otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_OUTPUT);
	otf_output->width = chain3_width;
	otf_output->height = chain3_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_OTF_OUTPUT);
	(*indexes)++;

	dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_DMA_OUTPUT);
	dma_output->width = chain3_width;
	dma_output->height = chain3_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	(*indexes)++;

	/* FD */
	otf_input = fimc_is_itf_g_param(device, frame, PARAM_FD_OTF_INPUT);
	otf_input->width = chain3_width;
	otf_input->height = chain3_height;
	*lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
	(*indexes)++;

	return ret;
}

static int fimc_is_ischain_s_path(struct fimc_is_device_ischain *device,
	u32 *lindex, u32 *hindex, u32 *indexes)
{
	int ret = 0;
	struct isp_param *isp_param;
	struct drc_param *drc_param;
	struct scalerc_param *scc_param;
	struct odc_param *odc_param;
	struct dis_param *dis_param;
	struct tdnr_param *dnr_param;
	struct scalerp_param *scp_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	isp_param = &device->is_region->parameter.isp;
	drc_param = &device->is_region->parameter.drc;
	scc_param = &device->is_region->parameter.scalerc;
	odc_param = &device->is_region->parameter.odc;
	dis_param = &device->is_region->parameter.dis;
	dnr_param = &device->is_region->parameter.tdnr;
	scp_param = &device->is_region->parameter.scalerp;

	isp_param->control.cmd = CONTROL_COMMAND_START;
	isp_param->control.bypass = CONTROL_BYPASS_DISABLE;
	isp_param->control.run_mode = 1;
	*lindex |= LOWBIT_OF(PARAM_ISP_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_ISP_CONTROL);
	(*indexes)++;

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		(*indexes)++;

		odc_param->control.cmd = CONTROL_COMMAND_STOP;
		*lindex |= LOWBIT_OF(PARAM_ODC_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_ODC_CONTROL);
		(*indexes)++;

		fimc_is_subdev_drc_bypass(device, &drc_param->control, lindex, hindex, indexes);
		fimc_is_subdev_dis_stop(device, dis_param, lindex, hindex, indexes);
		fimc_is_subdev_dnr_stop(device, &dnr_param->control, lindex, hindex, indexes);

		scp_param->control.cmd = CONTROL_COMMAND_STOP;
		*lindex |= LOWBIT_OF(PARAM_SCALERP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_CONTROL);
		(*indexes)++;
	} else {
		scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
		(*indexes)++;

		odc_param->control.cmd = CONTROL_COMMAND_START;
		*lindex |= LOWBIT_OF(PARAM_ODC_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_ODC_CONTROL);
		(*indexes)++;

		fimc_is_subdev_drc_bypass(device, &drc_param->control, lindex, hindex, indexes);
		fimc_is_subdev_dis_bypass(device, dis_param, lindex, hindex, indexes);
		fimc_is_subdev_dnr_bypass(device, &dnr_param->control, lindex, hindex, indexes);

		scp_param->control.cmd = CONTROL_COMMAND_START;
#ifdef SCALER_PARALLEL_MODE
		scp_param->otf_input.scaler_path_sel = OTF_INPUT_PARAL_PATH;
#else
		scp_param->otf_input.scaler_path_sel = OTF_INPUT_SERIAL_PATH;
#endif
		*lindex |= LOWBIT_OF(PARAM_SCALERP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_SCALERP_CONTROL);
		(*indexes)++;
	}

	return ret;
}

#ifdef ENABLE_SETFILE
static int fimc_is_ischain_chg_setfile(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group_id = 0;
	struct fimc_is_group *group_isp;
	u32 indexes, lindex, hindex;

	BUG_ON(!device);

	group_isp = &device->group_isp;
	indexes = lindex = hindex = 0;

	if (group_isp->smp_shot.count < 1) {
		merr("group%d is working(%d), setfile change is fail",
			device, group_isp->id, group_isp->smp_shot.count);
		goto p_err;
	}

	group_id |= GROUP_ID(device->group_3aa.id);
	group_id |= GROUP_ID(device->group_isp.id);

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group_id & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group_id = GROUP_ID(GROUP_ID_3A0);

	if (test_bit(FIMC_IS_GROUP_ACTIVE, &device->group_dis.state))
		group_id |= GROUP_ID(device->group_dis.id);

	ret = fimc_is_itf_process_stop(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_stop fail", device);
		goto p_err;
	}

	ret = fimc_is_ischain_s_setfile(device, device->setfile, &lindex, &hindex, &indexes);
	if (ret)
		merr("fimc_is_ischain_s_setfile is fail", device);

	ret = fimc_is_itf_s_param(device, NULL, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_a_param(device, group_id);
	if (ret) {
		merr("fimc_is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_start(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_start fail", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}
#endif

#ifdef SCALER_CROP_DZOOM
static int fimc_is_ischain_s_dzoom(struct fimc_is_device_ischain *this,
	u32 crop_x, u32 crop_y, u32 crop_width)
{
	int ret = 0;
	u32 indexes, lindex, hindex;
	u32 chain0_width, chain0_height;
	u32 temp_width, temp_height, input_width;
	u32 zoom_input, zoom_target;
	u32 crop_cx, crop_cy, crop_cwidth, crop_cheight;
	struct scalerc_param *scc_param;
	u32 chain0_ratio, preview_ratio;
	u32 chain0_ratio_width, chain0_ratio_height;
#ifdef USE_ADVANCED_DZOOM
	u32 zoom_pre, zoom_post, zoom_pre_max;
	u32 crop_px, crop_py, crop_pwidth, crop_pheight;
	u32 chain1_width, chain1_height;
	u32 chain2_width, chain2_height;
	u32 chain3_width, chain3_height;
	u32 scp_input_width, scp_input_height;
	struct scalerp_param *scp_param;

	scc_param = &this->is_region->parameter.scalerc;
	scp_param = &this->is_region->parameter.scalerp;
	indexes = lindex = hindex = 0;
	chain0_width = this->chain0_width;
	chain0_height = this->chain0_height;
	chain1_width = this->chain1_width;
	chain1_height = this->chain1_height;
	chain2_width = this->chain2_width;
	chain2_height = this->chain2_height;
	chain3_width = this->chain3_width;
	chain3_height = this->chain3_height;
#ifdef PRINT_DZOOM
	printk(KERN_INFO "chain0(%d, %d), chain1(%d, %d), chain2(%d, %d)\n",
		chain0_width, chain0_height,
		chain1_width, chain1_height,
		chain2_width, chain2_height);
#endif
#else
	scc_param = &this->is_region->parameter.scalerc;
	indexes = lindex = hindex = 0;
	chain0_width = this->chain0_width;
	chain0_height = this->chain0_height;
#ifdef PRINT_DZOOM
	printk(KERN_INFO "chain0(%d, %d)\n", chain0_width, chain0_height);
#endif
#endif

	/* CHECK */
	input_width = crop_width;
	temp_width = crop_width + (crop_x<<1);
	if (temp_width != chain0_width) {
		err("input width is not valid(%d != %d)",
			temp_width, chain0_width);
		/* if invalid input come, dzoom is not apply and
		shot command is sent to firmware */
		ret = 0;
		goto exit;
	}

	chain0_ratio_width = chain0_width;
	chain0_ratio_height = chain0_height;

#ifdef USE_ADVANCED_DZOOM
	zoom_input = (chain0_ratio_width * 1000) / crop_width;
	zoom_pre_max = (chain0_ratio_width * 1000) / chain1_width;

	if (zoom_pre_max < 1000)
		zoom_pre_max = 1000;

#ifdef PRINT_DZOOM
	printk(KERN_INFO "zoom input : %d, premax-zoom : %d\n",
		zoom_input, zoom_pre_max);
#endif

	if (test_bit(FIMC_IS_SUBDEV_START, &this->dis.state))
		zoom_target = (zoom_input * 91 + 34000) / 125;
	else
		zoom_target = zoom_input;

	if (zoom_target > zoom_pre_max) {
		zoom_pre = zoom_pre_max;
		zoom_post = (zoom_target * 1000) / zoom_pre;
	} else {
		zoom_pre = zoom_target;
		zoom_post = 1000;
	}

	/* CALCULATION */
	temp_width = (chain0_ratio_width * 1000) / zoom_pre;
	temp_height = (chain0_ratio_height * 1000) / zoom_pre;
	crop_cx = (chain0_width - temp_width)>>1;
	crop_cy = (chain0_height - temp_height)>>1;
	crop_cwidth = chain0_width - (crop_cx<<1);
	crop_cheight = chain0_height - (crop_cy<<1);

	scc_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_param->input_crop.pos_x = crop_cx;
	scc_param->input_crop.pos_y = crop_cy;
	scc_param->input_crop.crop_width = crop_cwidth;
	scc_param->input_crop.crop_height = crop_cheight;
	scc_param->input_crop.in_width = chain0_width;
	scc_param->input_crop.in_height = chain0_height;
	scc_param->input_crop.out_width = chain1_width;
	scc_param->input_crop.out_height = chain1_height;
	lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	indexes++;

#ifdef PRINT_DZOOM
	printk(KERN_INFO "pre-zoom target : %d(%d, %d, %d %d)\n",
		zoom_pre, crop_cx, crop_cy, crop_cwidth, crop_cheight);
#endif

#ifdef SCALER_PARALLEL_MODE
	scp_input_width = chain0_width;
	scp_input_height = chain0_height;
#else
	scp_input_width = chain2_width;
	scp_input_height = chain2_height;
#endif
	temp_width = (scp_input_width * 1000) / zoom_post;
	temp_height = (scp_input_height * 1000) / zoom_post;
	crop_px = (scp_input_width - temp_width)>>1;
	crop_py = (scp_input_height - temp_height)>>1;
	crop_pwidth = scp_input_width - (crop_px<<1);
	crop_pheight = scp_input_height - (crop_py<<1);

	scp_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scp_param->input_crop.pos_x = crop_px;
	scp_param->input_crop.pos_y = crop_py;
	scp_param->input_crop.crop_width = crop_pwidth;
	scp_param->input_crop.crop_height = crop_pheight;
	scp_param->input_crop.in_width = scp_input_width;
	scp_param->input_crop.in_height = scp_input_height;
	scp_param->input_crop.out_width = chain3_width;
	scp_param->input_crop.out_height = chain3_height;
	lindex |= LOWBIT_OF(PARAM_SCALERP_INPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERP_INPUT_CROP);
	indexes++;

#ifdef PRINT_DZOOM
	printk(KERN_INFO "post-zoom target : %d(%d, %d, %d %d)\n",
		zoom_post, crop_px, crop_py, crop_pwidth, crop_pheight);
#endif
#else
	zoom_input = (chain0_ratio_width * 1000) / crop_width;

	if (test_bit(FIMC_IS_SUBDEV_START, &this->dis.state))
		zoom_target = (zoom_input * 91 + 34000) / 125;
	else
		zoom_target = zoom_input;

	temp_width = (chain0_ratio_width * 1000) / zoom_target;
	temp_height = (chain0_ratio_height * 1000) / zoom_target;
	crop_cx = (chain0_width - temp_width)>>1;
	crop_cy = (chain0_height - temp_height)>>1;
	crop_cwidth = chain0_width - (crop_cx<<1);
	crop_cheight = chain0_height - (crop_cy<<1);

	scc_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_param->input_crop.pos_x = crop_cx;
	scc_param->input_crop.pos_y = crop_cy;
	scc_param->input_crop.crop_width = crop_cwidth;
	scc_param->input_crop.crop_height = crop_cheight;
	lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	indexes++;

#ifdef PRINT_DZOOM
	printk(KERN_INFO "zoom input : %d, zoom target : %d(%d, %d, %d %d)\n",
		zoom_input, zoom_target,
		crop_cx, crop_cy, crop_cwidth, crop_cheight);
#endif
#endif

	ret = fimc_is_itf_s_param(this, indexes, lindex, hindex);
	if (ret) {
		err("fimc_is_itf_s_param is fail\n");
		ret = -EINVAL;
		goto exit;
	}

	this->crop_x = crop_cx;
	this->crop_y = crop_cy;
	this->crop_width = crop_cwidth;
	this->crop_height = crop_cheight;
	this->dzoom_width = input_width;

exit:
	return ret;
}
#endif

#ifdef ENABLE_DRC
static int fimc_is_ischain_drc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct param_control *ctl_param;
	u32 group_id = 0;
	struct fimc_is_group *group;

	mdbgd_ischain("%s\n", device, __func__);

	group = &device->group_isp;
	if (!group) {
		merr("group is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
		GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group_id = GROUP_ID(GROUP_ID_3A0);
	else
		group_id = GROUP_ID(group->id);

	lindex = hindex = indexes = 0;
	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);

	ret = fimc_is_itf_process_stop(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (bypass)
		fimc_is_subdev_drc_bypass(device, ctl_param, &lindex, &hindex, &indexes);
	else
		fimc_is_subdev_drc_start(device, ctl_param, &lindex, &hindex, &indexes);

	frame->shot->ctl.entry.lowIndexParam |= lindex;
	frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	ret = fimc_is_itf_a_param(device, group_id);
	if (ret) {
		merr("fimc_is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_start(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (bypass)
		clear_bit(FIMC_IS_SUBDEV_START, &device->drc.state);
	else
		set_bit(FIMC_IS_SUBDEV_START, &device->drc.state);

p_err:
	mrinfo("[DRC] bypass : %d\n", device, frame, bypass);
	return ret;
}
#endif

#ifdef ENABLE_TDNR
static int fimc_is_ischain_dnr_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame,
	bool bypass)
{
	int ret = 0;
	u32 lindex, hindex, indexes;
	struct param_control *ctl_param;

	mdbgd_ischain("%s\n", device, __func__);

	lindex = hindex = indexes = 0;
	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_TDNR_CONTROL);

	if (bypass)
		fimc_is_subdev_dnr_bypass(device, ctl_param, &lindex, &hindex, &indexes);
	else
		fimc_is_subdev_dnr_start(device, ctl_param, &lindex, &hindex, &indexes);

	frame->shot->ctl.entry.lowIndexParam |= lindex;
	frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	if (bypass)
		clear_bit(FIMC_IS_SUBDEV_START, &device->dnr.state);
	else
		set_bit(FIMC_IS_SUBDEV_START, &device->dnr.state);

p_err:
	mrinfo("[DNR] bypass : %d\n", device, frame, bypass);
	return ret;
}
#endif

static int fimc_is_ischain_fd_bypass(struct fimc_is_device_ischain *device,
	bool bypass)
{
	int ret = 0;
	struct fd_param *fd_param;
	struct fimc_is_subdev *fd;
	struct fimc_is_group *group;
	u32 indexes, lindex, hindex;
	u32 group_id = 0;

	BUG_ON(!device);

	mdbgd_ischain("%s(%d)\n", device, __func__, bypass);

	fd = &device->fd;
	group = fd->group;
	if (!group) {
		merr("group is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	group_id |= GROUP_ID(group->id);
	fd_param = &device->is_region->parameter.fd;
	indexes = lindex = hindex = 0;

	/* if there's only one group of isp, send group id by 3a0 */
	if ((group_id & GROUP_ID(GROUP_ID_ISP)) &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
		group_id = GROUP_ID(GROUP_ID_3A0);

	ret = fimc_is_itf_process_stop(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (bypass) {
		fd_param->control.cmd = CONTROL_COMMAND_STOP;
		fd_param->control.bypass = CONTROL_BYPASS_DISABLE;
	} else {
		fd_param->control.cmd = CONTROL_COMMAND_START;
		fd_param->control.bypass = CONTROL_BYPASS_DISABLE;
	}

	lindex |= LOWBIT_OF(PARAM_FD_CONTROL);
	hindex |= HIGHBIT_OF(PARAM_FD_CONTROL);
	indexes++;

	fd_param->otf_input.width = device->chain3_width;
	fd_param->otf_input.height = device->chain3_height;
	lindex |= LOWBIT_OF(PARAM_FD_OTF_INPUT);
	hindex |= HIGHBIT_OF(PARAM_FD_OTF_INPUT);
	indexes++;

	ret = fimc_is_itf_s_param(device, NULL, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_start(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (bypass) {
		clear_bit(FIMC_IS_SUBDEV_START, &fd->state);
		mdbgd_ischain("FD off\n", device);
	} else {
		set_bit(FIMC_IS_SUBDEV_START, &fd->state);
		mdbgd_ischain("FD on\n", device);
	}

p_err:
	return ret;
}

int fimc_is_ischain_3aa_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	u32 group_id;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(!vctx->video);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	group_id = GET_3AA_ID(vctx->video);

	ret = fimc_is_group_open(groupmgr,
		group,
		group_id,
		device->instance,
		vctx,
		device,
		fimc_is_ischain_3aa_callback);
	if (ret)
		merr("fimc_is_group_open is fail", device);

	return ret;
}

int fimc_is_ischain_3aa_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	leader = &group->leader;
	queue = GET_SRC_QUEUE(vctx);

	ret = fimc_is_ischain_3aa_stop(device, leader, queue);
	if (ret)
		merr("fimc_is_ischain_3aa_stop is fail", device);

	ret = fimc_is_group_close(groupmgr, group);
	if (ret)
		merr("fimc_is_group_close is fail", device);

	return ret;
}

int fimc_is_ischain_3aa_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!leader);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_group_process_start(groupmgr, group, queue);
	if (ret) {
		merr("fimc_is_group_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &leader->state);

p_err:
	return ret;
}

int fimc_is_ischain_3aa_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!leader);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	if (!test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		mwarn("already stop", device);
		goto p_err;
	}

	ret = fimc_is_group_process_stop(groupmgr, group, queue);
	if (ret) {
		merr("fimc_is_group_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(FIMC_IS_SUBDEV_START, &leader->state);

p_err:
	info("[3A%d:D:%d] %s(%d, %d)\n", group->id, device->instance, __func__,
		ret, atomic_read(&group->scount));
	return ret;
}

int fimc_is_ischain_3aa_reqbufs(struct fimc_is_device_ischain *device,
	u32 count)
{
	int ret = 0;
	struct fimc_is_group *group;

	BUG_ON(!device);

	group = &device->group_3aa;

	if (!count) {
		ret = fimc_is_itf_unmap(device, GROUP_ID(group->id));
		if (ret)
			merr("fimc_is_itf_unmap is fail(%d)", device, ret);
	}

	return ret;
}

int fimc_is_ischain_3aa_s_format(struct fimc_is_device_ischain *device,
	u32 width, u32 height)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;

	BUG_ON(!device);

	group = &device->group_3aa;
	leader = &group->leader;

	leader->input.width = width;
	leader->input.height = height;

	return ret;
}

int fimc_is_ischain_3aa_s_input(struct fimc_is_device_ischain *device,
	u32 input)
{
	int ret = 0;
	u32 otf_input;
	struct fimc_is_group *group;
	struct fimc_is_groupmgr *groupmgr;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_3aa;
	otf_input = (input & OTF_3AA_MASK) >> OTF_3AA_SHIFT;

	mdbgd_ischain("%s() calling fimc_is_group_init\n", device, __func__);

	ret = fimc_is_group_init(groupmgr, group, otf_input, 0);
	if (ret) {
		merr("fimc_is_group_init is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_3aa_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state));

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = fimc_is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret)
		merr("fimc_is_group_buffer_queue is fail(%d)", device, ret);

	return ret;
}

int fimc_is_ischain_3aa_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_3aa;

	ret = fimc_is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("fimc_is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

int fimc_is_ischain_3aa_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct taa_param *taa_param;
	u32 lindex, hindex, indexes;
	u32 crop_x, crop_y, crop_width, crop_height;
	u32 *input_crop;
	u32 *output_crop;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);
	BUG_ON(!node);

#ifdef DBG_STREAMING
	mdbgd_ischain("3AA TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	taa_param = &device->is_region->parameter.taa;
	input_crop = node->input.cropRegion;
	output_crop = node->output.cropRegion;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_3aa.state)) {
		crop_x = taa_param->otf_input.bayer_crop_offset_x;
		crop_y = taa_param->otf_input.bayer_crop_offset_y;
		crop_width = taa_param->otf_input.bayer_crop_width;
		crop_height = taa_param->otf_input.bayer_crop_height;
	} else {
		crop_x = taa_param->vdma1_input.bayer_crop_offset_x;
		crop_y = taa_param->vdma1_input.bayer_crop_offset_y;
		crop_width = taa_param->vdma1_input.bayer_crop_width;
		crop_height = taa_param->vdma1_input.bayer_crop_height;
	}

	if (IS_NULL_COORD(input_crop)) {
		input_crop[0] = crop_x;
		input_crop[1] = crop_y;
		input_crop[2] = crop_width;
		input_crop[3] = crop_height;
	}

	if (!GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0)) {
		if ((taa_param->vdma1_input.bds_out_width != output_crop[2]) ||
			(taa_param->vdma1_input.bds_out_height != output_crop[3]) ||
			(taa_param->vdma1_input.bayer_crop_width != input_crop[2]) ||
			(taa_param->vdma1_input.bayer_crop_height != input_crop[3])) {
			ret = fimc_is_ischain_s_3aa_size(device,
				ldr_frame,
				input_crop,
				output_crop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3aa_size is fail(%d)", device, ret);
				goto p_err;
			}
			mrinfo("[3AA] in_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				input_crop[0], input_crop[1], input_crop[2], input_crop[3]);
			mdbg_pframe("[3AA] out_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);
		}
	} else {
		if ((input_crop[0] != crop_x) ||
			(input_crop[1] != crop_y) ||
			(input_crop[2] != crop_width) ||
			(input_crop[3] != crop_height)) {
			ret = fimc_is_ischain_s_3aa_size(device,
				ldr_frame,
				input_crop,
				output_crop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3aa_size is fail(%d)", device, ret);
				goto p_err;
			}

			mrinfo("[3AA] in_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				input_crop[0], input_crop[1], input_crop[2], input_crop[3]);
		}
	}

	ldr_frame->shot->ctl.entry.lowIndexParam |= lindex;
	ldr_frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, 0);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_3aa_ops = {
	.start_streaming	= fimc_is_ischain_3aa_start,
	.stop_streaming		= fimc_is_ischain_3aa_stop
};

static int fimc_is_ischain_3aap_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct taa_param *taa_param,
	u32 *output_crop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *taa_vdma2_output;

	if ((output_crop[2] > taa_param->otf_input.bayer_crop_width) ||
		(output_crop[3] > taa_param->otf_input.bayer_crop_height)) {
		mrerr("bds output size is invalid((%d, %d) > (%d, %d))", device, frame,
			output_crop[2],
			output_crop[3],
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	/* HACK */
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->group_3aa.state)) {
		struct param_otf_input *taa_otf_input;

		taa_otf_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_OTF_INPUT);
		taa_otf_input->bds_out_enable = ISP_BDS_COMMAND_ENABLE;
		taa_otf_input->bds_out_width = output_crop[2];
		taa_otf_input->bds_out_height = output_crop[3];
		*lindex |= LOWBIT_OF(PARAM_3AA_OTF_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_3AA_OTF_INPUT);
		(*indexes)++;
	} else {
		struct param_dma_input *taa_dma_input;

		taa_dma_input = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA1_INPUT);
		taa_dma_input->bds_out_enable = ISP_BDS_COMMAND_ENABLE;
		taa_dma_input->bds_out_width = output_crop[2];
		taa_dma_input->bds_out_height = output_crop[3];
		*lindex |= LOWBIT_OF(PARAM_3AA_VDMA1_INPUT);
		*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA1_INPUT);
		(*indexes)++;
	}

	taa_vdma2_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA2_OUTPUT);
	taa_vdma2_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	taa_vdma2_output->width = output_crop[2];
	taa_vdma2_output->height = output_crop[3];
	taa_vdma2_output->buffer_number = 0;
	taa_vdma2_output->buffer_address = 0;
	taa_vdma2_output->dma_out_mask = 0;
	taa_vdma2_output->bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT;
	taa_vdma2_output->notify_dma_done = DMA_OUTPUT_NOTIFY_DMA_DONE_ENBABLE;

	if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR12) {
		taa_vdma2_output->format = DMA_INPUT_FORMAT_BAYER_PACKED12;
	} else if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR16) {
		taa_vdma2_output->format = DMA_INPUT_FORMAT_BAYER;
	} else {
		mwarn("Invalid bayer format", device);
		ret = -EINVAL;
		goto p_err;
	}

	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}


static int fimc_is_ischain_3aap_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *taa_vdma2_output;

	mdbgd_ischain("%s\n", device, __func__);

	taa_vdma2_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA2_OUTPUT);
	taa_vdma2_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA2_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return ret;
}

static int fimc_is_ischain_3aap_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct camera2_scaler_uctl *scalerUd;
	struct taa_param *taa_param;
	u32 lindex, hindex, indexes;
	u32 *output_crop;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);
	BUG_ON(!node);

#ifdef DBG_STREAMING
	mdbgd_ischain("3AAP TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	taa_param = &device->is_region->parameter.taa;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	/* HACK */
	framemgr = GET_SUBDEV_FRAMEMGR(subdev->leader);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	/* HACK */
	queue = GET_SUBDEV_QUEUE(subdev->leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (node->request) {
		output_crop = node->output.cropRegion;

		if (output_crop[0] || output_crop[1]) {
			mwarn("crop pos(%d, %d) is ignored", device,
				output_crop[0],
				output_crop[1]);
			output_crop[0] = 0;
			output_crop[1] = 0;
		}

		if (!output_crop[0] && !output_crop[1] &&
			!output_crop[2] && !output_crop[3]) {
			output_crop[0] = 0;
			output_crop[1] = 0;
			output_crop[2] = taa_param->vdma2_output.width;
			output_crop[3] = taa_param->vdma2_output.height;
		}

		if ((output_crop[2] != taa_param->vdma2_output.width) ||
			(output_crop[3] != taa_param->vdma2_output.height) ||
			!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_3aap_start(device,
				subdev,
				ldr_frame,
				queue,
				taa_param,
				output_crop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3aap_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("[3AP] ot_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_8, flags);

		fimc_is_frame_request_head(framemgr, &frame);
		if (frame) {
			if (!frame->stream) {
				framemgr_x_barrier_irqr(framemgr, 0, flags);
				merr("frame->stream is NULL", device);
				ret = -EINVAL;
				goto p_err;
			}

			scalerUd->taapTargetAddress[0] = frame->dvaddr_buffer[0];
			scalerUd->taapTargetAddress[1] = 0;
			scalerUd->taapTargetAddress[2] = 0;
			frame->stream->findex = ldr_frame->index;
			set_bit(OUT_3AAP_FRAME, &ldr_frame->out_flag);
			set_bit(REQ_FRAME, &frame->req_flag);
			fimc_is_frame_trans_req_to_pro(framemgr, frame);
		} else {
			mwarn("3aap %d frame is drop", device, ldr_frame->fcount);
			scalerUd->taapTargetAddress[0] = 0;
			scalerUd->taapTargetAddress[1] = 0;
			scalerUd->taapTargetAddress[2] = 0;
			node->request = 0;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_8, flags);
	} else {
		if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_3aap_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3aap_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[3AP:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		mwarn("3aap request is 0", device);
		scalerUd->taapTargetAddress[0] = 0;
		scalerUd->taapTargetAddress[1] = 0;
		scalerUd->taapTargetAddress[2] = 0;
		node->request = 0;
	}

	ldr_frame->shot->ctl.entry.lowIndexParam |= lindex;
	ldr_frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, 0);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_3aac_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct taa_param *taa_param,
	u32 *output_crop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *taa_vdma4_output;

	if ((output_crop[2] != taa_param->otf_input.bayer_crop_width) ||
		(output_crop[3] != taa_param->otf_input.bayer_crop_height)) {
		merr("bds output size is invalid((%d, %d) != (%d, %d))", device,
			output_crop[2],
			output_crop[3],
			taa_param->otf_input.bayer_crop_width,
			taa_param->otf_input.bayer_crop_height);
		ret = -EINVAL;
		goto p_err;
	}

	taa_vdma4_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA4_OUTPUT);
	taa_vdma4_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	taa_vdma4_output->width = output_crop[2];
	taa_vdma4_output->height = output_crop[3];
	taa_vdma4_output->buffer_number = 0;
	taa_vdma4_output->buffer_address = 0;
	taa_vdma4_output->dma_out_mask = 0;
	taa_vdma4_output->bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT;
	taa_vdma4_output->notify_dma_done = DMA_OUTPUT_NOTIFY_DMA_DONE_ENBABLE;

	if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR12) {
		taa_vdma4_output->format = DMA_INPUT_FORMAT_BAYER_PACKED12;
	} else if (queue->framecfg.format.pixelformat == V4L2_PIX_FMT_SBGGR16) {
		taa_vdma4_output->format = DMA_INPUT_FORMAT_BAYER;
	} else {
		mwarn("Invalid bayer format", device);
		ret = -EINVAL;
		goto p_err;
	}

	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_3aac_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *taa_vdma4_output;

	mdbgd_ischain("%s\n", device, __func__);

	taa_vdma4_output = fimc_is_itf_g_param(device, frame, PARAM_3AA_VDMA4_OUTPUT);
	taa_vdma4_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_3AA_VDMA4_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return ret;
}

static int fimc_is_ischain_3aac_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct camera2_scaler_uctl *scalerUd;
	struct taa_param *taa_param;
	u32 lindex, hindex, indexes;
	u32 *output_crop;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("3AAC TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	taa_param = &device->is_region->parameter.taa;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (node->request) {
		output_crop = node->output.cropRegion;

		output_crop[0] = 0;
		output_crop[1] = 0;
		output_crop[2] = taa_param->otf_input.bayer_crop_width;
		output_crop[3] = taa_param->otf_input.bayer_crop_height;

		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_3aac_start(device,
				subdev,
				ldr_frame,
				queue,
				taa_param,
				output_crop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3aac_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("[3AC] ot_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_10, flags);

		fimc_is_frame_request_head(framemgr, &frame);
		if (frame) {
			scalerUd->taacTargetAddress[0] = frame->dvaddr_buffer[0];
			scalerUd->taacTargetAddress[1] = 0;
			scalerUd->taacTargetAddress[2] = 0;
			frame->stream->findex = ldr_frame->index;
			set_bit(OUT_3AAC_FRAME, &ldr_frame->out_flag);
			set_bit(REQ_FRAME, &frame->req_flag);
			fimc_is_frame_trans_req_to_pro(framemgr, frame);
		} else {
			mwarn("3aac %d frame is drop", device, ldr_frame->fcount);
			scalerUd->taacTargetAddress[0] = 0;
			scalerUd->taacTargetAddress[1] = 0;
			scalerUd->taacTargetAddress[2] = 0;
			node->request = 0;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_10, flags);
	} else {
		if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_3aac_stop(device,
				subdev,
				ldr_frame,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_3aac_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[3AC:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->taacTargetAddress[0] = 0;
		scalerUd->taacTargetAddress[1] = 0;
		scalerUd->taacTargetAddress[2] = 0;
		node->request = 0;
	}

	ldr_frame->shot->ctl.entry.lowIndexParam |= lindex;
	ldr_frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, 0);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_isp_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct sensor_param *sensor_param;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_subdev *leader_3aa;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
#ifdef ENABLE_BAYER_CROP
	u32 crop_x, crop_y, crop_width, crop_height;
	u32 sensor_width, sensor_height, sensor_ratio;
	u32 chain0_width, chain0_height, chain0_ratio;
	u32 chain3_width, chain3_height, chain3_ratio;
	u32 chain1_wmin, chain1_hmin;
#endif
	u32 input_crop[4] = {0, };
	u32 output_crop[4] = {0, };
	u32 sensor_width, sensor_height;
	u32 bns_width, bns_height;
	u32 framerate;
	u32 lindex = 0;
	u32 hindex = 0;
	u32 indexes = 0;

	BUG_ON(!device);
	BUG_ON(!device->sensor);
	BUG_ON(!queue);

	mdbgd_isp("%s()\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	framemgr = &queue->framemgr;
	if (device->group_3aa.id == GROUP_ID_INVALID)
		leader_3aa = NULL;
	else
		leader_3aa = &device->group_3aa.leader;
	sensor_param = &device->is_region->parameter.sensor;
	sensor_width = fimc_is_sensor_g_width(device->sensor);
	sensor_height = fimc_is_sensor_g_height(device->sensor);
	bns_width = fimc_is_sensor_g_bns_width(device->sensor);
	bns_height = fimc_is_sensor_g_bns_height(device->sensor);
	framerate = fimc_is_sensor_g_framerate(device->sensor);

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	/* 1. check chain size */
	device->sensor_width = sensor_width;
	device->sensor_height = sensor_height;
	device->bns_width = bns_width;
	device->bns_height = bns_height;

	if (leader_3aa && (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) ||
				GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1))) {
		if (test_bit(FIMC_IS_SUBDEV_OPEN, &leader_3aa->state) &&
				(leader_3aa->output.width != leader->input.width)) {
			merr("width size is invalid(%d != %d)", device,
					leader_3aa->output.width, leader->input.width);
			ret = -EINVAL;
			goto p_err;
		}

		if (test_bit(FIMC_IS_SUBDEV_OPEN, &leader_3aa->state) &&
				(leader_3aa->output.height != leader->input.height)) {
			merr("height size is invalid(%d != %d)", device,
					leader_3aa->output.height, leader->input.height);
			ret = -EINVAL;
			goto p_err;
		}
	}

	if (leader_3aa && (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) ||
				GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1))) {
		device->chain0_width = leader->input.width;
		device->chain0_height = leader->input.height;
	} else {
		device->chain0_width = leader->input.width - device->margin_width;
		device->chain0_height = leader->input.height - device->margin_height;
	}

	device->dzoom_width = 0;
	device->bds_width = 0;
	device->bds_height = 0;
#ifdef ENABLE_BAYER_CROP
	/* 2. crop calculation */
	sensor_width = device->sensor_width;
	sensor_height = device->sensor_height;
	chain3_width = device->chain3_width;
	chain3_height = device->chain3_height;
	crop_width = sensor_width;
	crop_height = sensor_height;
	crop_x = crop_y = 0;

	sensor_ratio = sensor_width * 1000 / sensor_height;
	chain3_ratio = chain3_width * 1000 / chain3_height;

	if (sensor_ratio == chain3_ratio) {
		crop_width = sensor_width;
		crop_height = sensor_height;
	} else if (sensor_ratio < chain3_ratio) {
		/*
		 * isp dma input limitation
		 * height : 2 times
		 */
		crop_height =
			(sensor_width * chain3_height) / chain3_width;
		crop_height = ALIGN(crop_height, 2);
		crop_y = ((sensor_height - crop_height) >> 1) & 0xFFFFFFFE;
	} else {
		/*
		 * isp dma input limitation
		 * width : 4 times
		 */
		crop_width =
			(sensor_height * chain3_width) / chain3_height;
		crop_width = ALIGN(crop_width, 4);
		crop_x = ((sensor_width - crop_width) >> 1) & 0xFFFFFFFE;
	}
	device->chain0_width = crop_width;
	device->chain0_height = crop_height;

	device->dzoom_width = crop_width;
	device->crop_width = crop_width;
	device->crop_height = crop_height;
	device->crop_x = crop_x;
	device->crop_y = crop_y;

	dbg_isp("crop_x : %d, crop y : %d\n", crop_x, crop_y);
	dbg_isp("crop width : %d, crop height : %d\n",
		crop_width, crop_height);

	/* 2. scaling calculation */
	chain1_wmin = (crop_width >> 4) & 0xFFFFFFFE;
	chain1_hmin = (crop_height >> 4) & 0xFFFFFFFE;

	if (chain1_wmin > device->chain1_width) {
		printk(KERN_INFO "scc down scale limited : (%d,%d)->(%d,%d)\n",
			device->chain1_width, device->chain1_height,
			chain1_wmin, chain1_hmin);
		device->chain1_width = chain1_wmin;
		device->chain1_height = chain1_hmin;
		device->chain2_width = chain1_wmin;
		device->chain2_height = chain1_hmin;
	}
#endif

	input_crop[0] = 0;
	input_crop[1] = 0;
	input_crop[2] = device->bns_width - device->margin_width;
	input_crop[3] = device->bns_height - device->margin_height;
	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0)) {
		/*
		 * In case of dirty bayer capture, reprocessing instance does not use 3aa.
		 * In this case, ischain device has no 3aa group.
		 */
		if (leader_3aa && test_bit(FIMC_IS_SUBDEV_OPEN, &leader_3aa->state))
			fimc_is_ischain_s_3aa_size(device, NULL, input_crop,
					output_crop, &lindex, &hindex, &indexes);
	} else {
		output_crop[0] = 0;
		output_crop[1] = 0;
		output_crop[2] = device->chain0_width;
		output_crop[3] = device->chain0_height;

		fimc_is_ischain_s_3aa_size(device, NULL, input_crop,
				output_crop, &lindex, &hindex, &indexes);
	}

	fimc_is_ischain_s_chain0_size(device,
		NULL, device->chain0_width, device->chain0_height,
				&lindex, &hindex, &indexes);

	fimc_is_ischain_s_chain1_size(device,
		device->chain1_width, device->chain1_height,
				&lindex, &hindex, &indexes);

	fimc_is_ischain_s_chain2_size(device,
		NULL, device->chain2_width, device->chain2_height,
				&lindex, &hindex, &indexes);

	fimc_is_ischain_s_chain3_size(device,
		NULL, device->chain3_width, device->chain3_height,
				&lindex, &hindex, &indexes);

	info("[3AA:D:%d] 3aa size(%d x %d)\n", device->instance, input_crop[2], input_crop[3]);
	info("[ISC:D:%d] chain0 size(%d x %d)\n", device->instance,
		device->chain0_width, device->chain0_height);
	info("[ISC:D:%d] chain1 size(%d x %d)\n", device->instance,
		device->chain1_width, device->chain1_height);
	info("[ISC:D:%d] chain2 size(%d x %d)\n", device->instance,
		device->chain2_width, device->chain2_height);
	info("[ISC:D:%d] chain3 size(%d x %d)\n", device->instance,
		device->chain3_width, device->chain3_height);


	fimc_is_ischain_s_path(device, &lindex, &hindex, &indexes);

	fimc_is_ischain_s_setfile(device, device->setfile, &lindex, &hindex, &indexes);

	if (test_bit(FIMC_IS_ISCHAIN_OPEN_SENSOR, &device->state))
		fimc_is_itf_sensor_mode(device);

#ifdef FIXED_FPS_DEBUG
	sensor_param->config.framerate = FIXED_FPS_VALUE;
#else
	sensor_param->config.framerate = framerate;
#endif
	if (device->sensor->min_target_fps > 0)
		sensor_param->config.min_target_fps = device->sensor->min_target_fps;
	if (device->sensor->max_target_fps > 0)
		sensor_param->config.max_target_fps = device->sensor->max_target_fps;
	if (device->sensor->scene_mode >= AA_SCENE_MODE_UNSUPPORTED)
		sensor_param->config.scene_mode = device->sensor->scene_mode;

	sensor_param->dma_output.width = sensor_width;
	sensor_param->dma_output.height = sensor_height;

	lindex = 0xFFFFFFFF;
	hindex = 0xFFFFFFFF;
	indexes = 64;

	ret = fimc_is_itf_s_param(device , NULL, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_f_param(device);
	if (ret) {
		merr("fimc_is_itf_f_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_sys_ctl(device, IS_SYS_CLOCK_GATE, sysfs_debug.clk_gate_mode);
	if (ret) {
		merr("fimc_is_itf_sys_ctl is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	/*
	 * this code is enabled when camera 2.0 feature is enabled
	 * ret = fimc_is_itf_g_capability(device);
	 * if (ret) {
	 *	err("fimc_is_itf_g_capability is fail\n");
	 *	ret = -EINVAL;
	 *	goto p_err;
	 *}
	 */

	ret = fimc_is_itf_init_process_start(device);
	if (ret) {
		merr("fimc_is_itf_init_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_group_process_start(groupmgr, group, queue);
	if (ret) {
		merr("fimc_is_group_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &leader->state);
	set_bit(FIMC_IS_ISHCAIN_START, &device->state);

p_err:
	info("[ISP:D:%d] %s(%d)\n", device->instance, __func__, ret);
	return ret;
}

int fimc_is_ischain_isp_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!leader);
	BUG_ON(!queue);

	mdbgd_isp("%s\n", device, __func__);

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	if (!test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		mwarn("already stop", device);
		goto p_err;
	}

	ret = fimc_is_group_process_stop(groupmgr, group, queue);
	if (ret) {
		merr("fimc_is_group_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(FIMC_IS_SUBDEV_START, &leader->state);
	clear_bit(FIMC_IS_ISHCAIN_START, &device->state);

p_err:
	info("[ISP:D:%d] %s(%d, %d)\n", device->instance, __func__,
		ret, atomic_read(&group->scount));
	return ret;
}

int fimc_is_ischain_isp_reqbufs(struct fimc_is_device_ischain *device,
	u32 count)
{
	int ret = 0;
	struct fimc_is_group *group;

	BUG_ON(!device);

	group = &device->group_isp;

	if (!count) {
		ret = fimc_is_itf_unmap(device, GROUP_ID(group->id));
		if (ret)
			merr("fimc_is_itf_unmap is fail(%d)", device, ret);
	}

	return ret;
}

int fimc_is_ischain_isp_s_format(struct fimc_is_device_ischain *device,
	u32 width, u32 height)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);

	group = &device->group_isp;
	subdev = &group->leader;

	subdev->input.width = width;
	subdev->input.height = height;

	return ret;
}

int fimc_is_ischain_isp_s_input(struct fimc_is_device_ischain *device,
	u32 input)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_groupmgr *groupmgr;

	BUG_ON(!device);

	group = &device->group_isp;
	groupmgr = device->groupmgr;

	mdbgd_ischain("%s() calling fimc_is_group_init\n", device, __func__);

	ret = fimc_is_group_init(groupmgr, group, false, input);
	if (ret) {
		merr("fimc_is_group_init is fail", device);
		ret = -EINVAL;
	}

	return ret;
}

int fimc_is_ischain_isp_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state));

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = fimc_is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("fimc_is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_isp_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_isp;

	ret = fimc_is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("fimc_is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

int fimc_is_ischain_isp_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct isp_param *isp_param;
	u32 lindex, hindex, indexes;
	u32 *input_crop;
	u32 *output_crop;

	BUG_ON(!device);
	BUG_ON(!device->is_region);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);
	BUG_ON(!node);

#ifdef DBG_STREAMING
	mdbgd_ischain("ISP TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	isp_param = &device->is_region->parameter.isp;
	input_crop = node->input.cropRegion;
	output_crop = node->output.cropRegion;

	if (!GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0)) {
		if (IS_NULL_COORD(output_crop)) {
			output_crop[0] = 0;
			output_crop[1] = 0;
			output_crop[2] = isp_param->otf_output.width;
			output_crop[3] = isp_param->otf_output.height;
		}

		if ((output_crop[2] != isp_param->otf_output.width) ||
			(output_crop[3] != isp_param->otf_output.height)) {
			ret = fimc_is_ischain_s_chain0_size(device,
				ldr_frame,
				output_crop[2],
				output_crop[3],
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_s_chain0_size is fail(%d)", device, ret);
				goto p_err;
			}

			mrinfo("[ISP] out_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);
		}
	} else {
		if (IS_NULL_COORD(input_crop)) {
			input_crop[0] = 0;
			input_crop[1] = 0;
			input_crop[2] = isp_param->vdma1_input.width;
			input_crop[3] = isp_param->vdma1_input.height;
		}

		if ((input_crop[2] != isp_param->vdma1_input.width) ||
			(input_crop[3] != isp_param->vdma1_input.height)) {
			ret = fimc_is_ischain_s_chain0_size(device,
				ldr_frame,
				input_crop[2],
				input_crop[3],
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_s_chain0_size is fail(%d)", device, ret);
				goto p_err;
			}

		mrinfo("[ISP] in_crop[%d, %d, %d, %d]\n", device, ldr_frame,
			input_crop[0], input_crop[1], input_crop[2], input_crop[3]);
		}
	}

	ldr_frame->shot->ctl.entry.lowIndexParam |= lindex;
	ldr_frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, 0);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_isp_ops = {
	.start_streaming	= fimc_is_ischain_isp_start,
	.stop_streaming		= fimc_is_ischain_isp_stop
};

static int fimc_is_ischain_scc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct scalerc_param *scc_param,
	u32 *input_crop,
	u32 *output_crop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	u32 planes, i, j, buf_index;
	struct param_dma_output *scc_dma_output;
	struct param_otf_output *scc_otf_output;
	struct param_scaler_input_crop *scc_input_crop;
	struct param_scaler_output_crop *scc_output_crop;
#ifndef SCALER_PARALLEL_MODE
	struct param_otf_input *scp_otf_input;
#endif

	if (output_crop[2] > scc_param->otf_input.width * 4) {
		mwarn("Cannot be scaled up beyond 4 times(%d -> %d)",
			device, scc_param->otf_input.width, output_crop[2]);
		output_crop[2] = scc_param->otf_input.width * 4;
	}

	if (output_crop[3] > scc_param->otf_input.height * 4) {
		mwarn("Cannot be scaled up beyond 4 times(%d -> %d)",
			device, scc_param->otf_input.height, output_crop[3]);
		output_crop[3] = scc_param->otf_input.height * 4;
	}

	if (output_crop[2] < (scc_param->otf_input.width + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scc_param->otf_input.width, output_crop[2]);
		output_crop[2] = (scc_param->otf_input.width + 15) / 16;
	}

	if (output_crop[3] < (scc_param->otf_input.height + 15) / 16) {
		mwarn("Cannot be scaled down beyond 1/16 times(%d -> %d)",
			device, scc_param->otf_input.height, output_crop[3]);
		output_crop[3] = (scc_param->otf_input.height + 15) / 16;
	}

	planes = queue->framecfg.format.num_planes;
	for (i = 0; i < queue->buf_maxcount; i++) {
		for (j = 0; j < planes; j++) {
			buf_index = i*planes + j;
			device->is_region->shared[447+buf_index] = queue->buf_dva[i][j];
		}
	}

	mdbgd_ischain("buf_num:%d buf_plane:%d shared[447] : 0x%X\n",
		device,
		queue->buf_maxcount,
		queue->framecfg.format.num_planes,
		device->imemory.kvaddr_shared + 447 * sizeof(u32));

	/* setting always although otf output is not used. */
	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		scc_otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
		scc_otf_output->width = output_crop[2];
		scc_otf_output->height = output_crop[3];
	} else {
		scc_otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
#ifdef SCALER_PARALLEL_MODE
		scc_otf_output->width = output_crop[2];
		scc_otf_output->height = output_crop[3];
#else
		scp_otf_input = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OTF_INPUT);
		scc_otf_output->width = scp_otf_input->width;
		scc_otf_output->height = scp_otf_input->height;
#endif
	}
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	(*indexes)++;

	scc_input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_INPUT_CROP);
	scc_input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_input_crop->pos_x = input_crop[0];
	scc_input_crop->pos_y = input_crop[1];
	scc_input_crop->crop_width = input_crop[2];
	scc_input_crop->crop_height = input_crop[3];
	scc_input_crop->in_width = scc_param->otf_input.width;
	scc_input_crop->in_height = scc_param->otf_input.height;
	scc_input_crop->out_width = output_crop[2];
	scc_input_crop->out_height = output_crop[3];
	*lindex |= LOWBIT_OF(PARAM_SCALERC_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_INPUT_CROP);
	(*indexes)++;

	if (test_bit(FIMC_IS_ISCHAIN_REPROCESSING, &device->state)) {
		scc_output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
		scc_output_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
		scc_output_crop->pos_x = output_crop[0];
		scc_output_crop->pos_y = output_crop[1];
		scc_output_crop->crop_width = output_crop[2];
		scc_output_crop->crop_height = output_crop[3];
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		(*indexes)++;

		scc_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_DMA_OUTPUT);
		scc_dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		scc_dma_output->buffer_number = queue->buf_maxcount;
		scc_dma_output->plane = queue->framecfg.format.num_planes - 1;
		scc_dma_output->buffer_address = device->imemory.dvaddr_shared + 447*sizeof(u32);
		scc_dma_output->width = output_crop[2];
		scc_dma_output->height = output_crop[3];
		scc_dma_output->reserved[0] = SCALER_DMA_OUT_SCALED;
	} else {
		scc_output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
		scc_output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
		scc_output_crop->pos_x = output_crop[0];
		scc_output_crop->pos_y = output_crop[1];
		scc_output_crop->crop_width = output_crop[2];
		scc_output_crop->crop_height = output_crop[3];
		*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
		(*indexes)++;

		scc_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_DMA_OUTPUT);
		scc_dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		scc_dma_output->buffer_number = queue->buf_maxcount;
		scc_dma_output->plane = queue->framecfg.format.num_planes - 1;
		scc_dma_output->buffer_address = device->imemory.dvaddr_shared + 447*sizeof(u32);
		scc_dma_output->width = input_crop[2];
		scc_dma_output->height = input_crop[3];
		scc_dma_output->reserved[0] = SCALER_DMA_OUT_UNSCALED;
	}

	switch (queue->framecfg.format.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
		scc_dma_output->format = DMA_OUTPUT_FORMAT_YUV422,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_1;
		scc_dma_output->order = DMA_OUTPUT_ORDER_CrYCbY;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		scc_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_2;
		scc_dma_output->order = DMA_OUTPUT_ORDER_CbCr;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12:
		scc_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_2;
		scc_dma_output->order = DMA_OUTPUT_ORDER_CrCb;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		scc_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scc_dma_output->plane = DMA_OUTPUT_PLANE_3;
		scc_dma_output->order = DMA_OUTPUT_ORDER_NO;
		break;
	default:
		mwarn("unknown preview pixelformat", device);
		break;
	}

	*lindex |= LOWBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return ret;
}

static int fimc_is_ischain_scc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct scalerc_param *scc_param,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *scc_dma_output;
	struct param_otf_output *scc_otf_output;
	struct param_scaler_output_crop *scc_output_crop;

	mdbgd_ischain("%s\n", device, __func__);

	scc_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_DMA_OUTPUT);
	scc_dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_DMA_OUTPUT);
	(*indexes)++;

	scc_otf_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OTF_OUTPUT);
	scc_otf_output->width = device->chain1_width;
	scc_otf_output->height = device->chain1_height;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OTF_OUTPUT);
	(*indexes)++;

	scc_output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERC_OUTPUT_CROP);
	scc_output_crop->cmd = SCALER_CROP_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERC_OUTPUT_CROP);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return ret;
}

static int fimc_is_ischain_scc_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct camera2_scaler_uctl *scalerUd;
	struct scalerc_param *scc_param;
	u32 lindex, hindex, indexes;
	u32 *input_crop, *output_crop;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("SCC TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	scc_param = &device->is_region->parameter.scalerc;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (node->request) {
		input_crop = node->input.cropRegion;
		output_crop = node->output.cropRegion;

		if (!input_crop[0] && !input_crop[1] &&
			!input_crop[2] && !input_crop[3]) {
			input_crop[0] = scc_param->input_crop.pos_x;
			input_crop[1] = scc_param->input_crop.pos_y;
			input_crop[2] = scc_param->input_crop.crop_width;
			input_crop[3] = scc_param->input_crop.crop_height;
		}

		if (!output_crop[0] && !output_crop[1] &&
			!output_crop[2] && !output_crop[3]) {
			output_crop[0] = scc_param->output_crop.pos_x;
			output_crop[1] = scc_param->output_crop.pos_y;
			output_crop[2] = scc_param->output_crop.crop_width;
			output_crop[3] = scc_param->output_crop.crop_height;
		}

		if ((input_crop[0] != scc_param->input_crop.pos_x) ||
			(input_crop[1] != scc_param->input_crop.pos_y) ||
			(input_crop[2] != scc_param->input_crop.crop_width) ||
			(input_crop[3] != scc_param->input_crop.crop_height) ||
			(output_crop[0] != scc_param->output_crop.pos_x) ||
			(output_crop[1] != scc_param->output_crop.pos_y) ||
			(output_crop[2] != scc_param->output_crop.crop_width) ||
			(output_crop[3] != scc_param->output_crop.crop_height) ||
			(input_crop[2] != scc_param->dma_output.width) ||
			(input_crop[3] != scc_param->dma_output.height) ||
			!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {

			ret = fimc_is_ischain_scc_start(device,
				subdev,
				ldr_frame,
				queue,
				scc_param,
				input_crop,
				output_crop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scc_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("[SCC] in_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				input_crop[0], input_crop[1], input_crop[2], input_crop[3]);
			mdbg_pframe("[SCC] ot_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_8, flags);

		fimc_is_frame_request_head(framemgr, &frame);
		if (frame) {
			if (!frame->stream) {
				framemgr_x_barrier_irqr(framemgr, 0, flags);
				merr("frame->stream is NULL", device);
				ret = -EINVAL;
				goto p_err;
			}

			scalerUd->sccTargetAddress[0] = frame->dvaddr_buffer[0];
			scalerUd->sccTargetAddress[1] = frame->dvaddr_buffer[1];
			scalerUd->sccTargetAddress[2] = frame->dvaddr_buffer[2];
			frame->stream->findex = ldr_frame->index;
			set_bit(OUT_SCC_FRAME, &ldr_frame->out_flag);
			set_bit(REQ_FRAME, &frame->req_flag);
			fimc_is_frame_trans_req_to_pro(framemgr, frame);
		} else {
			mwarn("scc %d frame is drop", device, ldr_frame->fcount);
			scalerUd->sccTargetAddress[0] = 0;
			scalerUd->sccTargetAddress[1] = 0;
			scalerUd->sccTargetAddress[2] = 0;
			node->request = 0;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_8, flags);
	} else {
		if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_scc_stop(device,
				subdev,
				ldr_frame,
				scc_param,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scc_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[SCC:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->sccTargetAddress[0] = 0;
		scalerUd->sccTargetAddress[1] = 0;
		scalerUd->sccTargetAddress[2] = 0;
		node->request = 0;
	}

	ldr_frame->shot->ctl.entry.lowIndexParam |= lindex;
	ldr_frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, 0);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}


static int fimc_is_ischain_scp_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct fimc_is_queue *queue,
	struct scalerp_param *scp_param,
	u32 *input_crop,
	u32 *output_crop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	u32 planes, i, j, buf_index;
	struct param_dma_output *scp_dma_output;
	struct param_scaler_input_crop *scp_input_crop;
	struct param_scaler_output_crop	 *scp_output_crop;

	fimc_is_ischain_scp_adjust_crop(device, scp_param, &output_crop[2], &output_crop[3]);

	planes = queue->framecfg.format.num_planes;
	for (i = 0; i < queue->buf_maxcount; i++) {
		for (j = 0; j < planes; j++) {
			buf_index = i*planes + j;
			device->is_region->shared[400 + buf_index] = queue->buf_dva[i][j];
		}
	}

	mdbgd_ischain("buf_num:%d buf_plane:%d shared[400] : 0x%X\n",
		device,
		queue->buf_maxcount,
		queue->framecfg.format.num_planes,
		device->imemory.kvaddr_shared + 400 * sizeof(u32));

	scp_input_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_INPUT_CROP);
	scp_input_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	scp_input_crop->pos_x = input_crop[0];
	scp_input_crop->pos_y = input_crop[1];
	scp_input_crop->crop_width = input_crop[2];
	scp_input_crop->crop_height = input_crop[3];
#ifdef SCALER_PARALLEL_MODE
	scp_input_crop->in_width = device->bds_width;
	scp_input_crop->in_height = device->bds_height;
	scp_input_crop->out_width = output_crop[2];
	scp_input_crop->out_height = output_crop[3];
#else
	scp_input_crop->in_width = scp_param->otf_input.width;
	scp_input_crop->in_height = scp_param->otf_input.height;
	scp_input_crop->out_width = scp_param->otf_output.width;
	scp_input_crop->out_height = scp_param->otf_output.height;
#endif
	*lindex |= LOWBIT_OF(PARAM_SCALERP_INPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_INPUT_CROP);
	(*indexes)++;

	/*
	 * scaler can't apply stride to each plane, only y plane.
	 * basically cb, cr plane should be half of y plane,
	 * and it's automatically set
	 *
	 * 3 plane : all plane should be 8 or 16 stride
	 * 2 plane : y plane should be 32, 16 stride, others should be half stride of y
	 * 1 plane : all plane should be 8 stride
	 */
	/*
	 * limitation of output_crop.pos_x and pos_y
	 * YUV422 3P, YUV420 3P : pos_x and pos_y should be x2
	 * YUV422 1P : pos_x should be x2
	 */
	scp_output_crop = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_OUTPUT_CROP);
	scp_output_crop->cmd = SCALER_CROP_COMMAND_ENABLE;
	scp_output_crop->pos_x = output_crop[0];
	scp_output_crop->pos_y = output_crop[1];
	scp_output_crop->crop_width = output_crop[2];
	scp_output_crop->crop_height = output_crop[3];
	*lindex |= LOWBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_OUTPUT_CROP);
	(*indexes)++;

	scp_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_DMA_OUTPUT);
	scp_dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	scp_dma_output->buffer_number = queue->buf_maxcount;
	scp_dma_output->plane = queue->framecfg.format.num_planes - 1;
	scp_dma_output->buffer_address = device->imemory.dvaddr_shared + 400 * sizeof(u32);
	scp_dma_output->width = output_crop[2];
	scp_dma_output->height = output_crop[3];

	switch (queue->framecfg.format.pixelformat) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		scp_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scp_dma_output->plane = DMA_OUTPUT_PLANE_3;
		scp_dma_output->order = DMA_OUTPUT_ORDER_NO;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		scp_dma_output->format = OTF_OUTPUT_FORMAT_YUV420,
		scp_dma_output->plane = DMA_OUTPUT_PLANE_2;
		scp_dma_output->order = DMA_OUTPUT_ORDER_CbCr;
		break;
	default:
		mwarn("unknown preview pixelformat", device);
		break;
	}

	*lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	(*indexes)++;

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return ret;
}

static int fimc_is_ischain_scp_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame,
	struct scalerp_param *scp_param,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct param_dma_output *scp_dma_output;

	mdbgd_ischain("%s\n", device, __func__);

	scp_dma_output = fimc_is_itf_g_param(device, frame, PARAM_SCALERP_DMA_OUTPUT);
	scp_dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_SCALERP_DMA_OUTPUT);
	(*indexes)++;

	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return ret;
}

int fimc_is_ischain_scp_s_format(struct fimc_is_device_ischain *device,
	u32 pixelformat, u32 width, u32 height)
{
	int ret = 0;

	/* check scaler size limitation */
	switch (pixelformat) {
	/*
	 * YUV422 1P, YUV422 2P : x8
	 * YUV422 3P : x16
	 */
	case V4L2_PIX_FMT_YUV422P:
		if (width % 8) {
			merr("width(%d) of format(%d) is not supported size",
				device, width, pixelformat);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	/*
	 * YUV420 2P : x8
	 * YUV420 3P : x16
	 */
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		if (width % 8) {
			merr("width(%d) of format(%d) is not supported size",
				device, width, pixelformat);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		if (width % 16) {
			merr("width(%d) of format(%d) is not supported size",
				device, width, pixelformat);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		merr("format(%d) is not supported", device, pixelformat);
		ret = -EINVAL;
		goto p_err;
		break;
	}

	device->chain1_width = width;
	device->chain1_height = height;
	device->chain2_width = width;
	device->chain2_height = height;
	device->chain3_width = width;
	device->chain3_height = height;

p_err:
	return ret;
}

static int fimc_is_ischain_scp_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct camera2_scaler_uctl *scalerUd;
	struct scalerp_param *scp_param;
	u32 lindex, hindex, indexes;
	u32 *input_crop, *output_crop;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

#ifdef DBG_STREAMING
	mdbgd_ischain("SCP TAG(request %d)\n", device, node->request);
#endif

	lindex = hindex = indexes = 0;
	scp_param = &device->is_region->parameter.scalerp;
	scalerUd = &ldr_frame->shot->uctl.scalerUd;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	queue = GET_SUBDEV_QUEUE(subdev);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (node->request) {
		input_crop = node->input.cropRegion;
		output_crop = node->output.cropRegion;

		if (!input_crop[0] && !input_crop[1] &&
			!input_crop[2] && !input_crop[3]) {
			input_crop[0] = scp_param->input_crop.pos_x;
			input_crop[1] = scp_param->input_crop.pos_y;
			input_crop[2] = scp_param->input_crop.crop_width;
			input_crop[3] = scp_param->input_crop.crop_height;
		}

		if (!output_crop[0] && !output_crop[1] &&
			!output_crop[2] && !output_crop[3]) {
			output_crop[0] = scp_param->output_crop.pos_x;
			output_crop[1] = scp_param->output_crop.pos_y;
			output_crop[2] = scp_param->output_crop.crop_width;
			output_crop[3] = scp_param->output_crop.crop_height;
		}

		if ((input_crop[0] != scp_param->input_crop.pos_x) ||
			(input_crop[1] != scp_param->input_crop.pos_y) ||
			(input_crop[2] != scp_param->input_crop.crop_width) ||
			(input_crop[3] != scp_param->input_crop.crop_height) ||
			(output_crop[0] != scp_param->output_crop.pos_x) ||
			(output_crop[1] != scp_param->output_crop.pos_y) ||
			(output_crop[2] != scp_param->output_crop.crop_width) ||
			(output_crop[3] != scp_param->output_crop.crop_height) ||
			!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
#ifdef SCALER_PARALLEL_MODE
			ret = fimc_is_ischain_s_chain2_size(device,
				ldr_frame,
				input_crop[2],
				input_crop[3],
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_s_chain2_size is fail(%d)", device, ret);
				goto p_err;
			}

			ret = fimc_is_ischain_s_chain3_size(device,
				ldr_frame,
				output_crop[2],
				output_crop[3],
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_s_chain3_size is fail(%d)", device, ret);
				goto p_err;
			}
			mrinfo("[SCPX] xx_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);

#endif
			ret = fimc_is_ischain_scp_start(device,
				subdev,
				ldr_frame,
				queue,
				scp_param,
				input_crop,
				output_crop,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scp_start is fail(%d)", device, ret);
				goto p_err;
			}

			mdbg_pframe("[SCP] in_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				input_crop[0], input_crop[1], input_crop[2], input_crop[3]);
			mdbg_pframe("[SCP] ot_crop[%d, %d, %d, %d]\n", device, ldr_frame,
				output_crop[0], output_crop[1], output_crop[2], output_crop[3]);
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_9, flags);

		fimc_is_frame_request_head(framemgr, &frame);
		if (frame) {
			if (!frame->stream) {
				framemgr_x_barrier_irqr(framemgr, 0, flags);
				merr("frame->stream is NULL", device);
				ret = -EINVAL;
				goto p_err;
			}

			scalerUd->scpTargetAddress[0] = frame->dvaddr_buffer[0];
			scalerUd->scpTargetAddress[1] = frame->dvaddr_buffer[1];
			scalerUd->scpTargetAddress[2] = frame->dvaddr_buffer[2];
			frame->stream->findex = ldr_frame->index;
			set_bit(OUT_SCP_FRAME, &ldr_frame->out_flag);
			set_bit(REQ_FRAME, &frame->req_flag);
			fimc_is_frame_trans_req_to_pro(framemgr, frame);
		} else {
			mwarn("scp %d frame is drop", device, ldr_frame->fcount);
			scalerUd->scpTargetAddress[0] = 0;
			scalerUd->scpTargetAddress[1] = 0;
			scalerUd->scpTargetAddress[2] = 0;
			node->request = 0;
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_9, flags);
	} else {
		if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_scp_stop(device,
				subdev,
				ldr_frame,
				scp_param,
				&lindex,
				&hindex,
				&indexes);
			if (ret) {
				merr("fimc_is_ischain_scp_stop is fail(%d)", device, ret);
				goto p_err;
			}

			info("[SCP:D:%d] off, %d\n", device->instance, ldr_frame->fcount);
		}

		scalerUd->scpTargetAddress[0] = 0;
		scalerUd->scpTargetAddress[1] = 0;
		scalerUd->scpTargetAddress[2] = 0;
		node->request = 0;
	}

	ldr_frame->shot->ctl.entry.lowIndexParam |= lindex;
	ldr_frame->shot->ctl.entry.highIndexParam |= hindex;
	ret = fimc_is_itf_s_param(device, ldr_frame, lindex, hindex, 0);
	if (ret) {
		mrerr("fimc_is_itf_s_param is fail(%d)", device, ldr_frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_dis_start(struct fimc_is_device_ischain *device,
	bool bypass)
{
	int ret = 0;
	u32 group_id = 0;
	struct dis_param *dis_param;
	u32 chain1_width, chain1_height;
	u32 indexes, lindex, hindex;

	struct fimc_is_group *group;
	struct fimc_is_groupmgr *groupmgr;

	mdbgd_ischain("%s()\n", device, __func__);

	BUG_ON(!device);

	chain1_width = device->dis_width;
	chain1_height = device->dis_height;
	indexes = lindex = hindex = 0;
	dis_param = &device->is_region->parameter.dis;
	group_id |= GROUP_ID(device->group_isp.id);
	group_id |= GROUP_ID(device->group_dis.id);

	group = &device->group_dis;
	groupmgr = device->groupmgr;

	mdbgd_ischain("%s() calling fimc_is_group_init\n", device, __func__);

	ret = fimc_is_group_init(groupmgr, group, false, 0);
	if (ret) {
		merr("fimc_is_group_init is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_stop(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_ischain_s_chain1_size(device,
		chain1_width, chain1_height, &lindex, &hindex, &indexes);

	if (bypass)
		fimc_is_subdev_dis_bypass(device,
			dis_param, &lindex, &hindex, &indexes);
	else
		fimc_is_subdev_dis_start(device,
			dis_param, &lindex, &hindex, &indexes);

	ret = fimc_is_itf_s_param(device, NULL, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_a_param(device, group_id);
	if (ret) {
		merr("fimc_is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_start(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &device->dis.state);
	mdbgd_ischain("DIS on\n", device);

	device->chain1_width = chain1_width;
	device->chain1_height = chain1_height;

p_err:
	return ret;
}

int fimc_is_ischain_dis_stop(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	u32 group_id = 0;
	struct dis_param *dis_param;
	u32 chain1_width, chain1_height;
	u32 indexes, lindex, hindex;

	mdbgd_ischain("%s()\n", device, __func__);

	chain1_width = device->chain2_width;
	chain1_height = device->chain2_height;
	indexes = lindex = hindex = 0;
	dis_param = &device->is_region->parameter.dis;
	group_id |= GROUP_ID(device->group_isp.id);
	group_id |= GROUP_ID(device->group_dis.id);

	ret = fimc_is_itf_process_stop(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_ischain_s_chain1_size(device,
		chain1_width, chain1_height, &lindex, &hindex, &indexes);

	fimc_is_subdev_dis_bypass(device,
		dis_param, &lindex, &hindex, &indexes);

	ret = fimc_is_itf_s_param(device, NULL, lindex, hindex, indexes);
	if (ret) {
		merr("fimc_is_itf_s_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_a_param(device, group_id);
	if (ret) {
		merr("fimc_is_itf_a_param is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	group_id = GROUP_ID(device->group_isp.id);
	ret = fimc_is_itf_process_start(device, group_id);
	if (ret) {
		merr("fimc_is_itf_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(FIMC_IS_SUBDEV_START, &device->dis.state);
	mdbgd_ischain("DIS off\n", device);

	device->chain1_width = chain1_width;
	device->chain1_height = chain1_height;

p_err:
	return ret;
}

int fimc_is_ischain_dis_tag(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *ldr_frame,
	struct camera2_node *node)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct camera2_scaler_uctl *scalerUd;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!ldr_frame);
	BUG_ON(!ldr_frame->shot);

	scalerUd = &ldr_frame->shot->uctl.scalerUd;

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (node->request) {
		if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_dis_start(device,
				ldr_frame->shot_ext->dis_bypass);
			if (ret) {
				merr("vdisc_start is fail", device);
				goto p_err;
			}
		}

		framemgr_e_barrier_irqs(framemgr, 0, flags);

		fimc_is_frame_request_head(framemgr, &frame);
		if (frame) {
			scalerUd->disTargetAddress[0] = frame->dvaddr_buffer[0];
			scalerUd->disTargetAddress[1] = frame->dvaddr_buffer[1];
			scalerUd->disTargetAddress[2] = frame->dvaddr_buffer[2];
			frame->stream->findex = ldr_frame->index;
			set_bit(OUT_DIS_FRAME, &ldr_frame->out_flag);
			set_bit(REQ_FRAME, &frame->req_flag);
			fimc_is_frame_trans_req_to_pro(framemgr, frame);
		} else {
			mwarn("dis %d frame is drop", device, ldr_frame->fcount);
			ldr_frame->shot->uctl.scalerUd.disTargetAddress[0] = 0;
			ldr_frame->shot->uctl.scalerUd.disTargetAddress[1] = 0;
			ldr_frame->shot->uctl.scalerUd.disTargetAddress[2] = 0;
			node->request = 0;
			fimc_is_gframe_cancel(device->groupmgr,
				&device->group_dis, ldr_frame->fcount);
		}

		framemgr_x_barrier_irqr(framemgr, 0, flags);
	} else {
		if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
			ret = fimc_is_ischain_dis_stop(device);
			if (ret) {
				merr("vdisc_stop is fail", device);
				goto p_err;
			}
		}

		scalerUd->disTargetAddress[0] = 0;
		scalerUd->disTargetAddress[1] = 0;
		scalerUd->disTargetAddress[2] = 0;
		node->request = 0;
	}

p_err:
	return ret;
}

int fimc_is_ischain_vdc_s_format(struct fimc_is_device_ischain *device,
	u32 width, u32 height)
{
	int ret = 0;

	device->dis_width = width;
	device->dis_height = height;

	return ret;
}

int fimc_is_ischain_vdo_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_open(groupmgr, group, GROUP_ID_DIS,
		device->instance, vctx, device, fimc_is_ischain_dis_callback);
	if (ret)
		merr("fimc_is_group_open is fail", device);

	return ret;
}

int fimc_is_ischain_vdo_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_subdev *leader;
	struct fimc_is_queue *queue;

	BUG_ON(!device);

	groupmgr = device->groupmgr;
	group = &device->group_dis;
	leader = &group->leader;
	queue = GET_SRC_QUEUE(vctx);

	ret = fimc_is_ischain_vdo_stop(device, leader, queue);
	if (ret)
		merr("fimc_is_ischain_vdo_stop is fail", device);

	ret = fimc_is_group_close(groupmgr, group);
	if (ret)
		merr("fimc_is_group_close is fail", device);

	return ret;
}

int fimc_is_ischain_vdo_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!leader);
	BUG_ON(!queue);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("already start", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_group_process_start(groupmgr, group, queue);
	if (ret) {
		merr("fimc_is_group_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &leader->state);

p_err:
	return ret;
}

int fimc_is_ischain_vdo_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *leader,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!leader);
	BUG_ON(!queue);

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	if (!test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		mwarn("already stop", device);
		goto p_err;
	}

	ret = fimc_is_group_process_stop(groupmgr, group, queue);
	if (ret) {
		merr("fimc_is_group_process_start is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_itf_process_stop(device, GROUP_ID_DIS);
	if (ret) {
		merr("fimc_is_itf_process_stop is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(FIMC_IS_SUBDEV_START, &leader->state);

p_err:
	info("[DIS:D:%d] %s(%d, %d)\n", device->instance, __func__,
		ret, atomic_read(&group->scount));
	return ret;
}

int fimc_is_ischain_vdo_s_format(struct fimc_is_device_ischain *this,
	u32 width, u32 height)
{
	int ret = 0;

	return ret;
}

int fimc_is_ischain_vdo_buffer_queue(struct fimc_is_device_ischain *device,
	struct fimc_is_queue *queue,
	u32 index)
{
	int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);
	BUG_ON(!test_bit(FIMC_IS_ISCHAIN_OPEN, &device->state));

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_buffer_queue(groupmgr, group, queue, index);
	if (ret) {
		merr("fimc_is_group_buffer_queue is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_vdo_buffer_finish(struct fimc_is_device_ischain *device,
	u32 index)
{
		int ret = 0;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;

	BUG_ON(!device);

#ifdef DBG_STREAMING
	mdbgd_ischain("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_dis;

	ret = fimc_is_group_buffer_finish(groupmgr, group, index);
	if (ret)
		merr("fimc_is_group_buffer_finish is fail(%d)", device, ret);

	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_vdo_ops = {
	.start_streaming	= fimc_is_ischain_vdo_start,
	.stop_streaming		= fimc_is_ischain_vdo_stop
};

int fimc_is_ischain_g_capability(struct fimc_is_device_ischain *this,
	u32 user_ptr)
{
	int ret = 0;

	ret = copy_to_user((void *)user_ptr, &this->capability,
		sizeof(struct camera2_sm));

	return ret;
}

int fimc_is_ischain_print_status(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *isp;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_interface *itf;

	isp = &device->group_isp.leader;
	vctx = isp->vctx;
	framemgr = GET_SRC_FRAMEMGR(vctx);
	itf = device->interface;

	fimc_is_frame_print_free_list(framemgr);
	fimc_is_frame_print_request_list(framemgr);
	fimc_is_frame_print_process_list(framemgr);
	fimc_is_frame_print_complete_list(framemgr);

	return ret;
}

int fimc_is_ischain_3aa_callback(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *check_frame)
{
	int ret = 0;
	u32 capture_id;
	unsigned long flags;
	struct fimc_is_group *group;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_subdev *leader, *taac, *taap, *scc, *dis, *scp;
	struct camera2_node *node, *leader_node;
#ifdef ENABLE_SETFILE
	u32 setfile_save;
#endif
#ifdef ENABLE_FAST_SHOT
	uint32_t af_trigger_bk;
#endif

#ifdef DBG_STREAMING
	mdbgd_ischain("%s()\n", device, __func__);
#endif

	BUG_ON(!device);
	BUG_ON(!check_frame);

	group = &device->group_3aa;
	leader = &group->leader;
	framemgr = GET_LEADER_FRAMEMGR(leader);

	fimc_is_frame_request_head(framemgr, &frame);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		return -EINVAL;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%X != %X)", device,
			(u32)frame, (u32)check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(frame->memory == FRAME_INI_MEM)) {
		fimc_is_itf_map(device, GROUP_ID(group->id),
			frame->dvaddr_shot, frame->shot_size);
		frame->memory = FRAME_MAP_MEM;
	}

	frame->shot->ctl.entry.lowIndexParam = 0;
	frame->shot->ctl.entry.highIndexParam = 0;
	frame->shot->dm.entry.lowIndexParam = 0;
	frame->shot->dm.entry.highIndexParam = 0;
	leader_node = &frame->shot_ext->node_group.leader;

#ifdef ENABLE_SETFILE
	if (frame->shot_ext->setfile != device->setfile) {
		setfile_save = device->setfile;
		device->setfile = frame->shot_ext->setfile;

		ret = fimc_is_ischain_chg_setfile(device);
		if (ret) {
			err("fimc_is_ischain_chg_setfile is fail");
			device->setfile = setfile_save;
			goto p_err;
		}
	}
#endif

#ifdef ENABLE_FAST_SHOT
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &group->state)) {
		af_trigger_bk = frame->shot->ctl.aa.afTrigger;
		memcpy(&frame->shot->ctl.aa, &group->fast_ctl.aa,
			sizeof(struct camera2_aa_ctl));
		memcpy(&frame->shot->ctl.scaler, &group->fast_ctl.scaler,
			sizeof(struct camera2_scaler_ctl));
		frame->shot->ctl.aa.afTrigger = af_trigger_bk;
	}
#endif

	ret = fimc_is_ischain_3aa_tag(device, leader, frame, leader_node);
	if (ret) {
		merr("fimc_is_ischain_3aa_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		node = &frame->shot_ext->node_group.capture[capture_id];
		taac = taap = scc = dis = scp = NULL;

		switch (node->vid) {
		case 0:
			break;
		case FIMC_IS_VIDEO_3A0C_NUM:
		case FIMC_IS_VIDEO_3A1C_NUM:
			taac = group->subdev[ENTRY_3AAC];
			break;
		case FIMC_IS_VIDEO_3A0P_NUM:
		case FIMC_IS_VIDEO_3A1P_NUM:
			taap = group->subdev[ENTRY_3AAP];
			break;
		case FIMC_IS_VIDEO_SCC_NUM:
			scc = group->subdev[ENTRY_SCALERC];
			break;
		case FIMC_IS_VIDEO_VDC_NUM:
			dis = group->subdev[ENTRY_DIS];
			break;
		case FIMC_IS_VIDEO_SCP_NUM:
			scp = group->subdev[ENTRY_SCALERP];
			break;
		default:
			merr("capture0 vid(%d) is invalid", device, node->vid);
			ret = -EINVAL;
			goto p_err;
		}

		if (taac) {
			ret = fimc_is_ischain_3aac_tag(device, taac, frame, node);
			if (ret) {
				merr("fimc_is_ischain_3aac_tag is fail(%d)", device, ret);
				goto p_err;
			}
		}

		if (taap) {
			ret = fimc_is_ischain_3aap_tag(device, taap, frame, node);
			if (ret) {
				merr("fimc_is_ischain_3aap_tag is fail(%d)", device, ret);
				goto p_err;
			}
		}

		if (scc) {
			ret = fimc_is_ischain_scc_tag(device, scc, frame, node);
			if (ret) {
				merr("fimc_is_ischain_scc_tag fail(%d)", device, ret);
				goto p_err;
			}
		}

		if (dis) {
			ret = fimc_is_ischain_dis_tag(device, dis, frame, node);
			if (ret) {
				merr("fimc_is_ischain_dis_tag fail(%d)", device, ret);
				goto p_err;
			}
		}

		if (scp) {
			ret = fimc_is_ischain_scp_tag(device, scp, frame, node);
			if (ret) {
				merr("fimc_is_ischain_scp_tag fail(%d)", device, ret);
				goto p_err;
			}
		}
	}

p_err:
	if (ret) {
		merr("3aa shot(index : %d) is skipped(error : %d)", device,
				frame->index, ret);
	} else {
		framemgr_e_barrier_irqs(framemgr, 0, flags);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		framemgr_x_barrier_irqr(framemgr, 0, flags);
		set_bit(REQ_3AA_SHOT, &frame->req_flag);
		fimc_is_itf_grp_shot(device, group, frame);
	}

	return ret;
}

int fimc_is_ischain_isp_callback(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *check_frame)
{
	int ret = 0;
	u32 capture_id;
	unsigned long flags;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_subdev *leader, *scc, *dis, *dnr, *scp, *fd;
	struct camera2_node *node, *leader_node;
#ifdef ENABLE_SETFILE
	u32 setfile_save;
#endif

	BUG_ON(!device);
	BUG_ON(!check_frame);
	BUG_ON(device->instance_sensor >= FIMC_IS_MAX_NODES);

#ifdef DBG_STREAMING
	mdbgd_isp("%s\n", device, __func__);
#endif

	groupmgr = device->groupmgr;
	group = &device->group_isp;
	leader = &group->leader;
	dnr = group->subdev[ENTRY_TDNR];
	fd = group->subdev[ENTRY_LHFD];
	framemgr = GET_LEADER_FRAMEMGR(leader);

	/*
	   BE CAREFUL WITH THIS
	1. buffer queue, all compoenent stop, so it's good
	2. interface callback, all component will be stop until new one is came
	   therefore, i expect lock object is not necessary in here
	*/

	BUG_ON(!framemgr);

	fimc_is_frame_request_head(framemgr, &frame);

	if (unlikely(!frame)) {
		merr("frame is NULL", device);
		return -EINVAL;
	}

	if (unlikely(frame != check_frame)) {
		merr("frame checking is fail(%X != %X)", device,
			(u32)frame, (u32)check_frame);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!frame->shot)) {
		merr("frame->shot is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(frame->memory == FRAME_INI_MEM)) {
		/* if there's only one group of isp, send group id by 3a0 */
		if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
				GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0)
			fimc_is_itf_map(device, GROUP_ID(GROUP_ID_3A0),
					frame->dvaddr_shot, frame->shot_size);
		else
			fimc_is_itf_map(device, GROUP_ID(group->id),
					frame->dvaddr_shot, frame->shot_size);
		frame->memory = FRAME_MAP_MEM;
	}

	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0) {
		frame->shot->uctl.scalerUd.taapTargetAddress[0] =
			frame->dvaddr_buffer[0];
		frame->shot->uctl.scalerUd.taapTargetAddress[1] = 0;
		frame->shot->uctl.scalerUd.taapTargetAddress[2] = 0;
	}

	frame->shot->ctl.entry.lowIndexParam = 0;
	frame->shot->ctl.entry.highIndexParam = 0;
	frame->shot->dm.entry.lowIndexParam = 0;
	frame->shot->dm.entry.highIndexParam = 0;
	leader_node = &frame->shot_ext->node_group.leader;

#ifdef ENABLE_SETFILE
	if (GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0) == 0 &&
			GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a1) == 0) {
		if (frame->shot_ext->setfile != device->setfile) {
			setfile_save = device->setfile;
			device->setfile = frame->shot_ext->setfile;

			ret = fimc_is_ischain_chg_setfile(device);
			if (ret) {
				err("fimc_is_ischain_chg_setfile is fail");
				device->setfile = setfile_save;
				goto p_err;
			}
		}
	}
#endif

#ifdef ENABLE_DRC
	if (frame->shot_ext->drc_bypass) {
		if (test_bit(FIMC_IS_SUBDEV_START, &device->drc.state)) {
			ret = fimc_is_ischain_drc_bypass(device, frame, true);
			if (ret) {
				err("fimc_is_ischain_drc_bypass(1) is fail");
				goto p_err;
			}
		}
	} else {
		if (!test_bit(FIMC_IS_SUBDEV_START, &device->drc.state)) {
			ret = fimc_is_ischain_drc_bypass(device, frame, false);
			if (ret) {
				err("fimc_is_ischain_drc_bypass(0) is fail");
				goto p_err;
			}
		}
	}
#endif

#ifdef ENABLE_TDNR
	if (dnr) {
		if (frame->shot_ext->dnr_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_START, &dnr->state)) {
				ret = fimc_is_ischain_dnr_bypass(device, frame, true);
				if (ret) {
					merr("dnr_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_START, &dnr->state)) {
				ret = fimc_is_ischain_dnr_bypass(device, frame, false);
				if (ret) {
					merr("dnr_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

#ifdef ENABLE_FD
	if (fd) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_START, &fd->state)) {
				ret = fimc_is_ischain_fd_bypass(device, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_START, &fd->state)) {
				ret = fimc_is_ischain_fd_bypass(device, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

#ifdef SCALER_CROP_DZOOM
	crop_width = frame->shot->ctl.scaler.cropRegion[2];
	/* Digital zoom is not supported in multiple sensor mode */
	if (crop_width && (crop_width != device->dzoom_width)) {
		ret = fimc_is_ischain_s_dzoom(device,
			frame->shot->ctl.scaler.cropRegion[0],
			frame->shot->ctl.scaler.cropRegion[1],
			frame->shot->ctl.scaler.cropRegion[2]);
		if (ret) {
			err("fimc_is_ischain_s_dzoom(%d, %d, %d) is fail",
				frame->shot->ctl.scaler.cropRegion[0],
				frame->shot->ctl.scaler.cropRegion[1],
				frame->shot->ctl.scaler.cropRegion[2]);
			goto exit;
		}
	}
#endif

	if (!GET_FIMC_IS_NUM_OF_SUBIP2(device, 3a0)) {
		ret = fimc_is_ischain_3aa_tag(device, leader, frame, leader_node);
		if (ret) {
			merr("fimc_is_ischain_3aa_tag is fail(%d)", device, ret);
			goto p_err;
		}
	}

	ret = fimc_is_ischain_isp_tag(device, leader, frame, leader_node);
	if (ret) {
		merr("fimc_is_ischain_isp_tag is fail(%d)", device, ret);
		goto p_err;
	}

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		node = &frame->shot_ext->node_group.capture[capture_id];
		scc = dis = scp = NULL;

		switch (node->vid) {
		case 0:
			break;
		case FIMC_IS_VIDEO_SCC_NUM:
			scc = group->subdev[ENTRY_SCALERC];
			break;
		case FIMC_IS_VIDEO_VDC_NUM:
			dis = group->subdev[ENTRY_DIS];
			break;
		case FIMC_IS_VIDEO_SCP_NUM:
			scp = group->subdev[ENTRY_SCALERP];
			break;
		case FIMC_IS_VIDEO_3A0C_NUM:
		case FIMC_IS_VIDEO_3A1C_NUM:
		case FIMC_IS_VIDEO_3A0P_NUM:
		case FIMC_IS_VIDEO_3A1P_NUM:
		default:
			merr("capture0 vid(%d) is invalid", device, node->vid);
			ret = -EINVAL;
			goto p_err;
		}

		if (scc) {
			ret = fimc_is_ischain_scc_tag(device, scc, frame, node);
			if (ret) {
				merr("fimc_is_ischain_scc_tag fail(%d)", device, ret);
				goto p_err;
			}
		}

		if (dis) {
			ret = fimc_is_ischain_dis_tag(device, dis, frame, node);
			if (ret) {
				merr("fimc_is_ischain_dis_tag fail(%d)", device, ret);
				goto p_err;
			}
		}

		if (scp) {
			ret = fimc_is_ischain_scp_tag(device, scp, frame, node);
			if (ret) {
				merr("fimc_is_ischain_scp_tag fail(%d)", device, ret);
				goto p_err;
			}
		}
	}

p_err:
	if (ret) {
		merr("isp shot(index : %d) is skipped(error : %d)",
				device, frame->index, ret);
	} else {
		framemgr_e_barrier_irqs(framemgr, 0, flags);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		framemgr_x_barrier_irqr(framemgr, 0, flags);
		set_bit(REQ_ISP_SHOT, &frame->req_flag);
		fimc_is_itf_grp_shot(device, group, frame);
	}

	return ret;
}

int fimc_is_ischain_dis_callback(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *check_frame)
{
	int ret = 0;
	unsigned long flags;
	u32 capture_id;
	bool dis_req, scp_req;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_group *group;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_subdev *leader, *dnr, *scp, *fd;
	struct camera2_node *node;

#ifdef DBG_STREAMING
	mdbgd_ischain("%s()\n", device, __func__);
#endif

	BUG_ON(!device);
	BUG_ON(!check_frame);

	group = &device->group_dis;
	vctx = group->leader.vctx;
	framemgr = GET_SRC_FRAMEMGR(vctx);
	dis_req = scp_req = false;
	leader = &group->leader;
	dnr = group->subdev[ENTRY_TDNR];
	fd = group->subdev[ENTRY_LHFD];

	fimc_is_frame_request_head(framemgr, &frame);

	if (frame != check_frame) {
		merr("grp_frame is invalid(%X != %X)", device,
			(u32)frame, (u32)check_frame);
		return -EINVAL;
	}

	frame->shot->ctl.entry.lowIndexParam = 0;
	frame->shot->ctl.entry.highIndexParam = 0;
	frame->shot->dm.entry.lowIndexParam = 0;
	frame->shot->dm.entry.highIndexParam = 0;

#ifdef ENABLE_TDNR
	if (dnr) {
		if (frame->shot_ext->dnr_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_START, &dnr->state)) {
				ret = fimc_is_ischain_dnr_bypass(device, frame, true);
				if (ret) {
					merr("dnr_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_START, &dnr->state)) {
				ret = fimc_is_ischain_dnr_bypass(device, frame, false);
				if (ret) {
					merr("dnr_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

#ifdef ENABLE_FD
	if (fd) {
		if (frame->shot_ext->fd_bypass) {
			if (test_bit(FIMC_IS_SUBDEV_START, &fd->state)) {
				ret = fimc_is_ischain_fd_bypass(device, true);
				if (ret) {
					merr("fd_bypass(1) is fail", device);
					goto p_err;
				}
			}
		} else {
			if (!test_bit(FIMC_IS_SUBDEV_START, &fd->state)) {
				ret = fimc_is_ischain_fd_bypass(device, false);
				if (ret) {
					merr("fd_bypass(0) is fail", device);
					goto p_err;
				}
			}
		}
	}
#endif

	for (capture_id = 0; capture_id < CAPTURE_NODE_MAX; ++capture_id) {
		node = &frame->shot_ext->node_group.capture[capture_id];
		scp = NULL;

		switch (node->vid) {
		case 0:
			break;
		case FIMC_IS_VIDEO_SCP_NUM:
			scp = group->subdev[ENTRY_SCALERP];
			break;
		case FIMC_IS_VIDEO_SCC_NUM:
		case FIMC_IS_VIDEO_VDC_NUM:
		case FIMC_IS_VIDEO_3A0C_NUM:
		case FIMC_IS_VIDEO_3A1C_NUM:
		case FIMC_IS_VIDEO_3A0P_NUM:
		case FIMC_IS_VIDEO_3A1P_NUM:
		default:
			merr("capture0 vid(%d) is invalid", device, node->vid);
			ret = -EINVAL;
			goto p_err;
		}

		if (scp) {
			ret = fimc_is_ischain_scp_tag(device, scp, frame, node);
			if (ret) {
				merr("fimc_is_ischain_scp_tag fail(%d)", device, ret);
				goto p_err;
			}
		}
	}

p_err:
	if (ret) {
		err("dis shot(index : %d) is skipped(error : %d)",
				frame->index, ret);
	} else {
		framemgr_e_barrier_irqs(framemgr, 0, flags);
		fimc_is_frame_trans_req_to_pro(framemgr, frame);
		framemgr_x_barrier_irqr(framemgr, 0, flags);
		set_bit(REQ_DIS_SHOT, &frame->req_flag);
		fimc_is_itf_grp_shot(device, group, frame);
	}

	return ret;
}

int fimc_is_ischain_camctl(struct fimc_is_device_ischain *this,
	struct fimc_is_frame *frame,
	u32 fcount)
{
	int ret = 0;
#ifdef ENABLE_SENSOR_DRIVER
	struct fimc_is_interface *itf;
	struct camera2_uctl *applied_ctl;

	struct camera2_sensor_ctl *isp_sensor_ctl;
	struct camera2_lens_ctl *isp_lens_ctl;
	struct camera2_flash_ctl *isp_flash_ctl;

	u32 index;

#ifdef DBG_STREAMING
	mdbgd_ischain("%s()\n", device, __func__);
#endif

	itf = this->interface;
	isp_sensor_ctl = &itf->isp_peri_ctl.sensorUd.ctl;
	isp_lens_ctl = &itf->isp_peri_ctl.lensUd.ctl;
	isp_flash_ctl = &itf->isp_peri_ctl.flashUd.ctl;

	/*lens*/
	index = (fcount + 0) & SENSOR_MAX_CTL_MASK;
	applied_ctl = &this->peri_ctls[index];
	applied_ctl->lensUd.ctl.focusDistance = isp_lens_ctl->focusDistance;

	/*sensor*/
	index = (fcount + 1) & SENSOR_MAX_CTL_MASK;
	applied_ctl = &this->peri_ctls[index];
	applied_ctl->sensorUd.ctl.exposureTime = isp_sensor_ctl->exposureTime;
	applied_ctl->sensorUd.ctl.frameDuration = isp_sensor_ctl->frameDuration;
	applied_ctl->sensorUd.ctl.sensitivity = isp_sensor_ctl->sensitivity;

	/*flash*/
	index = (fcount + 0) & SENSOR_MAX_CTL_MASK;
	applied_ctl = &this->peri_ctls[index];
	applied_ctl->flashUd.ctl.flashMode = isp_flash_ctl->flashMode;
	applied_ctl->flashUd.ctl.firingPower = isp_flash_ctl->firingPower;
	applied_ctl->flashUd.ctl.firingTime = isp_flash_ctl->firingTime;
#endif
	return ret;
}

int fimc_is_ischain_tag(struct fimc_is_device_ischain *ischain,
	struct fimc_is_frame *frame)
{
	int ret = 0;
#ifdef ENABLE_SENSOR_DRIVER
	struct camera2_uctl *applied_ctl;
	struct timeval curtime;
	u32 fcount;

	fcount = frame->fcount;
	applied_ctl = &ischain->peri_ctls[fcount & SENSOR_MAX_CTL_MASK];

	do_gettimeofday(&curtime);

	/* Request */
	frame->shot->dm.request.frameCount = fcount;

	/* Lens */
	frame->shot->dm.lens.focusDistance =
		applied_ctl->lensUd.ctl.focusDistance;

	/* Sensor */
	frame->shot->dm.sensor.exposureTime =
		applied_ctl->sensorUd.ctl.exposureTime;
	frame->shot->dm.sensor.sensitivity =
		applied_ctl->sensorUd.ctl.sensitivity;
	frame->shot->dm.sensor.frameDuration =
		applied_ctl->sensorUd.ctl.frameDuration;
	frame->shot->dm.sensor.timeStamp =
		(uint64_t)curtime.tv_sec*1000000 + curtime.tv_usec;

	/* Flash */
	frame->shot->dm.flash.flashMode =
		applied_ctl->flashUd.ctl.flashMode;
	frame->shot->dm.flash.firingPower =
		applied_ctl->flashUd.ctl.firingPower;
	frame->shot->dm.flash.firingTime =
		applied_ctl->flashUd.ctl.firingTime;
#else
	struct timespec curtime;

	do_posix_clock_monotonic_gettime(&curtime);

	frame->shot->dm.request.frameCount = frame->fcount;
	frame->shot->dm.sensor.timeStamp = fimc_is_get_timestamp();
#endif
	return ret;
}
