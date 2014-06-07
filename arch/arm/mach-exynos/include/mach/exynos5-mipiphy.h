/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5 - Helper functions for MIPI-CSIS control
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PLAT_MIPI_PHY_H
#define __PLAT_MIPI_PHY_H __FILE__

extern int exynos5_csis_phy_enable(int id, bool on);
extern int exynos5_dism_phy_enable(int id, bool on);

#endif /* __PLAT_MIPI_PHY_H */
