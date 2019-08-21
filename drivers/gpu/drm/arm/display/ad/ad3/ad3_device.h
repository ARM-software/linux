// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <joncha01@arm.com>
 *
 */
#ifndef _AD3_DEVICE_H_
#define _AD3_DEVICE_H_

struct ad_dev;

void ad3_update_strength(struct ad_dev *ad_dev, u32 s);
void ad3_update_calibration(struct ad_dev *ad_dev, u32 assertive);
void ad3_update_drc(struct ad_dev *ad_dev, u16 drc);
void ad3_save_hw_stat(struct ad_dev *ad_dev);
void ad3_reload_hw_stat(struct ad_dev *ad_dev);

#endif
