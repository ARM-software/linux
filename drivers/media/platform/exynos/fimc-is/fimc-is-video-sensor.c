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
#include <linux/videodev2_exynos_media.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "fimc-is-device-sensor.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-metadata.h"

const struct v4l2_file_operations fimc_is_sen_video_fops;
const struct v4l2_ioctl_ops fimc_is_sen_video_ioctl_ops;
const struct vb2_ops fimc_is_sen_qops;

int fimc_is_sen_video_probe(void *data)
{
	int ret = 0;
	char name[255];
	u32 number;
	struct fimc_is_device_sensor *device;
	struct fimc_is_video *video;

	BUG_ON(!data);

	device = (struct fimc_is_device_sensor *)data;
	video = &device->video;
	snprintf(name, sizeof(name), "%s%d", FIMC_IS_VIDEO_SENSOR_NAME, device->instance);
	number = FIMC_IS_VIDEO_SS0_NUM + device->instance;

	if (!device->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		name,
		number,
		VFL_DIR_RX,
		&device->mem,
		&device->v4l2_dev,
		&video->lock,
		&fimc_is_sen_video_fops,
		&fimc_is_sen_video_ioctl_ops);
	if (ret)
		dev_err(&device->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	info("[SS%d:V:X] %s(%d)\n", number, __func__, ret);
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int fimc_is_sen_video_open(struct file *file)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_sensor *device;

	vctx = NULL;
	video = video_drvdata(file);
	device = container_of(video, struct fimc_is_device_sensor, video);

	ret = open_vctx(file, video, &vctx, FRAMEMGR_ID_INVALID, FRAMEMGR_ID_SENSOR);
	if (ret) {
		err("open_vctx is fail(%d)", ret);
		goto p_err;
	}

	info("[SS%d:V:%d] %s\n", video->id, vctx->instance, __func__);

	ret = fimc_is_video_open(vctx,
		device,
		VIDEO_SENSOR_READY_BUFFERS,
		video,
		FIMC_IS_VIDEO_TYPE_CAPTURE,
		&fimc_is_sen_qops,
		NULL,
		NULL);
	if (ret) {
		merr("fimc_is_video_open is fail(%d)", vctx, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

	ret = fimc_is_sensor_open(device, vctx);
	if (ret) {
		merr("fimc_is_sen_open is fail(%d)", vctx, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_sen_video_close(struct file *file)
{
	int ret = 0;
	struct fimc_is_video *video = NULL;
	struct fimc_is_video_ctx *vctx = NULL;
	struct fimc_is_device_sensor *device = NULL;

	BUG_ON(!file);

	vctx = file->private_data;
	if (!vctx) {
		err("vctx is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video = vctx->video;
	if (!video) {
		merr("video is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	info("[SS0:V:%d] %s\n", vctx->instance, __func__);

	device = vctx->device;
	if (!device) {
		merr("device is NULL", vctx);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_close(device);
	if (ret)
		err("fimc_is_sensor_close is fail(%d)", ret);

	ret = fimc_is_video_close(vctx);
	if (ret)
		err("fimc_is_video_close is fail(%d)", ret);

	ret = close_vctx(file, video, vctx);
	if (ret)
		err("close_vctx is fail(%d)", ret);

p_err:
	return ret;
}

static unsigned int fimc_is_sen_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_poll(file, vctx, wait);
	if (ret)
		merr("fimc_is_video_poll is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_mmap(file, vctx, vma);
	if (ret)
		merr("fimc_is_video_mmap is fail(%d)", vctx, ret);

	return ret;
}

const struct v4l2_file_operations fimc_is_sen_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_sen_video_open,
	.release	= fimc_is_sen_video_close,
	.poll		= fimc_is_sen_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_sen_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int fimc_is_sen_video_querycap(struct file *file, void *fh,
					struct v4l2_capability *cap)
{
	/* Todo : add to query capability code */
	return 0;
}

static int fimc_is_sen_video_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	/* Todo : add to enumerate format code */
	return 0;
}

static int fimc_is_sen_video_get_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	/* Todo : add to get format code */
	return 0;
}

static int fimc_is_sen_video_set_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_queue *queue;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;

	ret = fimc_is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("fimc_is_video_set_format_mplane is fail(%d)", vctx, ret);
		goto p_err;
	}

	ret = fimc_is_sensor_s_format(device,
		&queue->framecfg.format,
		queue->framecfg.width,
		queue->framecfg.height);
	if (ret) {
		merr("fimc_is_sensor_s_format is fail(%d)", vctx, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_sen_video_cropcap(struct file *file, void *fh,
	struct v4l2_cropcap *cropcap)
{
	/* Todo : add to crop capability code */
	return 0;
}

static int fimc_is_sen_video_get_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	/* Todo : add to get crop control code */
	return 0;
}

static int fimc_is_sen_video_set_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *sensor;

	BUG_ON(!vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	sensor = vctx->device;
	BUG_ON(!sensor);

	fimc_is_sensor_s_format(sensor, &sensor->image.format, crop->c.width, crop->c.height);

	return 0;
}

static int fimc_is_sen_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	BUG_ON(!vctx);

	mdbgv_sensor("%s(buffers : %d)\n", vctx, __func__, buf->count);

	ret = fimc_is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_reqbufs is fail(error %d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_video_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_querybuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

#ifdef DBG_STREAMING
	/*dbg_sensor("%s\n", __func__);*/
#endif

	ret = fimc_is_video_qbuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_qbuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_video_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	bool blocking;
	struct fimc_is_video_ctx *vctx = file->private_data;

#ifdef DBG_STREAMING
	mdbgv_sensor("%s\n", vctx, __func__);
#endif

	ret = fimc_is_video_dqbuf(file, vctx, buf);
	if (ret) {
		blocking = file->f_flags & O_NONBLOCK;
		if (!blocking || (ret != -EAGAIN))
			merr("fimc_is_video_dqbuf is fail(%d)", vctx, ret);
	}

	return ret;
}

static int fimc_is_sen_video_streamon(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_streamon(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamon is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_video_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_streamoff(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamoff is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo: add to enumerate input code */
	info("%s is calld\n", __func__);
	return 0;
}

static int fimc_is_sen_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	/* Todo: add to get input control code */
	return 0;
}

static int fimc_is_sen_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 drive;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;
	struct fimc_is_framemgr *framemgr;

	BUG_ON(!vctx);

	mdbgv_sensor("%s(input : %d)\n", vctx, __func__, input);

	device = vctx->device;
	framemgr = GET_DST_FRAMEMGR(vctx);

	drive = input & SENSOR_DRIVING_MASK;
	input = input & SENSOR_MODULE_MASK;

	ret = fimc_is_sensor_s_input(device, input, drive);
	if (ret) {
		merr("fimc_is_sensor_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_sen_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;
	struct v4l2_subdev *subdev_flite;

	BUG_ON(!ctrl);
	BUG_ON(!vctx);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	subdev_flite = device->subdev_flite;
	if (!subdev_flite) {
		err("subdev_flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_S_STREAM:
		{
			u32 sstream, instant, noblock;

			sstream = (ctrl->value & SENSOR_SSTREAM_MASK) >> SENSOR_SSTREAM_SHIFT;
			instant = (ctrl->value & SENSOR_INSTANT_MASK) >> SENSOR_INSTANT_SHIFT;
			noblock = (ctrl->value & SENSOR_NOBLOCK_MASK) >> SENSOR_NOBLOCK_SHIFT;
			/*
			 * nonblock(0) : blocking command
			 * nonblock(1) : non-blocking command
			 */

			if (sstream == IS_ENABLE_STREAM) {
				ret = fimc_is_sensor_front_start(device, instant, noblock);
				if (ret) {
					merr("fimc_is_sensor_front_start is fail(%d)", device, ret);
					goto p_err;
				}
			} else {
				ret = fimc_is_sensor_front_stop(device);
				if (ret) {
					merr("fimc_is_sensor_front_stop is fail(%d)", device, ret);
					goto p_err;
				}
			}
		}
		break;
	case V4L2_CID_IS_S_BNS:
		if (device->pdata->is_bns == false) {
			mwarn("Could not support BNS\n", device);
			goto p_err;
		}

		ret = fimc_is_sensor_s_bns(device, ctrl->value);
		if (ret) {
			merr("fimc_is_sensor_s_bns is fail(%d)", device, ret);
			goto p_err;
		}

		ret = v4l2_subdev_call(subdev_flite, core, s_ctrl, ctrl);
		if (ret) {
			merr("v4l2_flite_call(s_ctrl) is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	/*
	 * gain boost: min_target_fps,  max_target_fps, scene_mode
	 */
	case V4L2_CID_IS_MIN_TARGET_FPS:
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set min_target_fps: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->min_target_fps = ctrl->value;
		}
		break;
	case V4L2_CID_IS_MAX_TARGET_FPS:
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set max_target_fps: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->max_target_fps = ctrl->value;
		}
		break;
	case V4L2_CID_SCENEMODE:
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set scene_mode: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->scene_mode = ctrl->value;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		if (fimc_is_sensor_s_frame_duration(device, ctrl->value)) {
			err("failed to set frame duration : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		if (fimc_is_sensor_s_exposure_time(device, ctrl->value)) {
			err("failed to set exposure time : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

static int fimc_is_sen_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *sensor;

	BUG_ON(!vctx);
	BUG_ON(!ctrl);

	sensor = vctx->device;
	if (!sensor) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_G_STREAM:
		if (sensor->instant_ret)
			ctrl->value = sensor->instant_ret;
		else
			ctrl->value = (test_bit(FIMC_IS_SENSOR_FRONT_START, &sensor->state) ?
				IS_ENABLE_STREAM : IS_DISABLE_STREAM);
		break;
	case V4L2_CID_IS_G_BNS_SIZE:
		{
			u32 width, height;

			width = fimc_is_sensor_g_bns_width(sensor);
			height = fimc_is_sensor_g_bns_height(sensor);

			ctrl->value = (width << 16) | height;
		}
		break;
	default:
		err("unsupported ioctl(%d)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

static int fimc_is_sen_video_g_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *sensor = vctx->device;
	struct v4l2_captureparm *cp = &parm->parm.capture;
	struct v4l2_fract *tfp = &cp->timeperframe;

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cp->capability |= V4L2_CAP_TIMEPERFRAME;
	tfp->numerator = 1;
	tfp->denominator = sensor->image.framerate;

	return 0;
}

static int fimc_is_sen_video_s_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!parm);

	mdbgv_sensor("%s\n", vctx, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_s_framerate(device, parm);
	if (ret) {
		merr("fimc_is_sen_s_framerate is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops fimc_is_sen_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_sen_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane	= fimc_is_sen_video_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= fimc_is_sen_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= fimc_is_sen_video_set_format_mplane,
	.vidioc_cropcap			= fimc_is_sen_video_cropcap,
	.vidioc_g_crop			= fimc_is_sen_video_get_crop,
	.vidioc_s_crop			= fimc_is_sen_video_set_crop,
	.vidioc_reqbufs			= fimc_is_sen_video_reqbufs,
	.vidioc_querybuf		= fimc_is_sen_video_querybuf,
	.vidioc_qbuf			= fimc_is_sen_video_qbuf,
	.vidioc_dqbuf			= fimc_is_sen_video_dqbuf,
	.vidioc_streamon		= fimc_is_sen_video_streamon,
	.vidioc_streamoff		= fimc_is_sen_video_streamoff,
	.vidioc_enum_input		= fimc_is_sen_video_enum_input,
	.vidioc_g_input			= fimc_is_sen_video_g_input,
	.vidioc_s_input			= fimc_is_sen_video_s_input,
	.vidioc_s_ctrl			= fimc_is_sen_video_s_ctrl,
	.vidioc_g_ctrl			= fimc_is_sen_video_g_ctrl,
	.vidioc_g_parm			= fimc_is_sen_video_g_parm,
	.vidioc_s_parm			= fimc_is_sen_video_s_parm,
};

static int fimc_is_sen_queue_setup(struct vb2_queue *vbq,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[])
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!vctx->video);

	mdbgv_sensor("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	video = vctx->video;

	ret = fimc_is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		allocators);
	if (ret)
		merr("fimc_is_queue_setup is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_sen_buffer_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static inline void fimc_is_sen_wait_prepare(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_prepare(vbq);
}

static inline void fimc_is_sen_wait_finish(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_finish(vbq);
}

static int fimc_is_sen_start_streaming(struct vb2_queue *q,
	unsigned int count)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = q->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state) &&
		test_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state)) {
		set_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
		fimc_is_sensor_back_start(device);
	} else {
		err("already stream on or buffer is not ready(%ld)",
			queue->state);
		clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
		clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
		fimc_is_sensor_back_stop(device);
		ret = -EINVAL;
	}

	return 0;
}

static int fimc_is_sen_stop_streaming(struct vb2_queue *q)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = q->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;

	if (test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
		clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
		clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
		ret = fimc_is_sensor_back_stop(device);
	} else {
		err("already stream off");
		ret = -EINVAL;
	}

	return ret;
}

static void fimc_is_sen_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_video *video;
	struct fimc_is_device_sensor *device;

#ifdef DBG_STREAMING
	mdbgv_sensor("%s(%d)\n", vctx, __func__, vb->v4l2_buf.index);
#endif

	queue = GET_DST_QUEUE(vctx);
	device = vctx->device;
	video = vctx->video;
	if (!video) {
		merr("video is NULL", device);
		return;
	}

	ret = fimc_is_queue_buffer_queue(queue, video->vb2, vb);
	if (ret) {
		merr("fimc_is_queue_buffer_queue is fail(%d)", device, ret);
		return;
	}

	ret = fimc_is_sensor_buffer_queue(device, vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_sensor_buffer_queue is fail(%d)", device, ret);
		return;
	}
}

static int fimc_is_sen_buffer_finish(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_sensor *device;

#ifdef DBG_STREAMING
	mdbgv_sensor("%s(%d)\n", vctx, __func__, vb->v4l2_buf.index);
#endif
	device = vctx->device;

	ret = fimc_is_sensor_buffer_finish(
		device,
		vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_sensor_buffer_finish is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct vb2_ops fimc_is_sen_qops = {
	.queue_setup		= fimc_is_sen_queue_setup,
	.buf_prepare		= fimc_is_sen_buffer_prepare,
	.buf_queue		= fimc_is_sen_buffer_queue,
	.buf_finish		= fimc_is_sen_buffer_finish,
	.wait_prepare		= fimc_is_sen_wait_prepare,
	.wait_finish		= fimc_is_sen_wait_finish,
	.start_streaming	= fimc_is_sen_start_streaming,
	.stop_streaming		= fimc_is_sen_stop_streaming,
};
