/* drivers/gpu/t6xx/kbase/src/platform/gpu_custom_interface.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_custom_interface.h
 * DVFS
 */

#ifndef _GPU_CUSTOM_INTERFACE_H_
#define _GPU_CUSTOM_INTERFACE_H_

#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
int gpu_create_sysfs_file(struct device *dev);
void gpu_remove_sysfs_file(struct device *dev);
#endif

#endif /* _GPU_CUSTOM_INTERFACE_H_ */
