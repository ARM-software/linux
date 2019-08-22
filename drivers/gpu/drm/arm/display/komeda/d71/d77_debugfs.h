// SPDX-License-Identifier: GPL-2.0
/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 *
 */
#ifndef _D77_DEBUGFS_H_
#define _D77_DEBUGFS_H_

#include <linux/debugfs.h>
#include "d71_dev.h"

#if defined(CONFIG_DEBUG_FS)
int d77_setup_perf_counters(struct d71_pipeline *pipe);
#else

static inline
int d77_setup_perf_counters(struct d71_pipeline *pipe)
{
	return 0;
}

#endif

#endif
