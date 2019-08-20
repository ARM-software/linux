// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */

#ifndef _MALI_AEU_IO_H_
#define _MALI_AEU_IO_H_

#include <linux/io.h>
#include "mali_aeu_log.h"

static inline u32 mali_aeu_read(void __iomem *reg, u32 offset)
{
	u32 val = readl(reg + offset);
	mali_aeu_log(true, offset, val);
	return val;
}

static inline void mali_aeu_write(void __iomem *reg, u32 offset, u32 val)
{
	writel(val, reg + offset);
	mali_aeu_log(false, offset, val);
}

#endif
