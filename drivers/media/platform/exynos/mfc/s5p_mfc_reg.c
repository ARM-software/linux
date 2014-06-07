/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_reg.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include "s5p_mfc_reg.h"

void s5p_mfc_write_reg(struct s5p_mfc_dev *dev, unsigned int data, unsigned int offset)
{
	writel(data, dev->regs_base + offset);
}

unsigned int s5p_mfc_read_reg(struct s5p_mfc_dev *dev, unsigned int offset)
{
	return readl(dev->regs_base + offset);
}
