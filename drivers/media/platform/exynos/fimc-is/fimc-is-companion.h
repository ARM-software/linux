/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/vmalloc.h>
#include <linux/firmware.h>

#include "fimc-is-core.h"

int fimc_is_comp_is_valid(struct fimc_is_core *core);
int fimc_is_comp_loadfirm(struct fimc_is_core *core);
int fimc_is_comp_loadsetf(struct fimc_is_core *core);
