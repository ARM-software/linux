// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#ifndef _MALI_AEU_DEV_H_
#define _MALI_AEU_DEV_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include "mali_aeu_hw.h"

#define AEU_NAME	"mali-aeu"

struct mali_aeu_device {
	struct device			*dev;

	struct v4l2_device		v4l2_dev;
	struct video_device		vdev;
	struct v4l2_m2m_dev		*m2mdev;
	struct iommu_domain		*iommu;
	struct device_dma_parameters	dma_parms;
	/* protect access in different instance */
	struct mutex			aeu_mutex;

	struct mali_aeu_hw_device	*hw_dev;
	struct mali_aeu_hw_info		hw_info;

	struct dentry			*dbg_folder;
};

int mali_aeu_device_init(struct mali_aeu_device *adev,
			 struct platform_device *pdev, struct dentry *parent);
int mali_aeu_device_destroy(struct mali_aeu_device *adev);
#endif
