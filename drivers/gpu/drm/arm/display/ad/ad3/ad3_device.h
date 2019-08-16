// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <joncha01@arm.com>
 *
 */
#ifndef _AD3_DEVICE_H_
#define _AD3_DEVICE_H_

struct ad_dev;

void ad3_save_hw_stat(struct ad_dev *ad_dev);
void ad3_reload_hw_stat(struct ad_dev *ad_dev);

#endif
