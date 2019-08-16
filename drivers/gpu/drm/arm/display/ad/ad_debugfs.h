/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#ifndef _AD_DEBUGFS_H_
#define _AD_DEBUGFS_H_

#include "ad_device.h"

int ad_debugfs_register(struct ad_dev *ad_dev);
void ad_debugfs_unregister(struct ad_dev *ad_dev);
#endif /*_AD_DEBUGFS_H_*/
