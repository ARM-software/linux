// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#ifndef _MALI_AEU_LOG_H_
#define _MALI_AEU_LOG_H_

#include <linux/device.h>
#include <linux/debugfs.h>

void mali_aeu_log_init(struct dentry *root);
void mali_aeu_log_exit(void);
int mali_aeu_log(bool read, u32 reg, u32 val);
#endif
