// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Mihail Atanassov <mihail.atanassov@arm.com>
 *
 */
#ifndef _KOMEDA_DEBUGFS_H_
#define _KOMEDA_DEBUGFS_H_

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>

struct drm_minor;

int komeda_kms_debugfs_register(struct drm_minor *minor);

#endif /*CONFIG_DEBUG_FS*/

#endif /*_KOMEDA_DEBUGFS_H_*/
