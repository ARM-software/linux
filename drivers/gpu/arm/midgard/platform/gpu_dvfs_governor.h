/* drivers/gpu/t6xx/kbase/src/platform/gpu_dvfs_governor.h
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
 * @file gpu_dvfs_governor.h
 * DVFS
 */

#ifndef _GPU_DVFS_GOVERNOR_H_
#define _GPU_DVFS_GOVERNOR_H_

typedef enum {
    G3D_DVFS_GOVERNOR_DEFAULT = 0,
    G3D_DVFS_GOVERNOR_STATIC,
    G3D_DVFS_GOVERNOR_BOOSTER,
    G3D_MAX_GOVERNOR_NUM,
} gpu_governor_type;

int gpu_dvfs_governor_init(struct kbase_device *kbdev, int governor_type);
int gpu_dvfs_decide_next_level(struct kbase_device *kbdev, int utilization);
int gpu_dvfs_init_time_in_state(struct exynos_context *platform);
int gpu_dvfs_update_time_in_state(struct exynos_context *platform, int freq);
int gpu_dvfs_get_level(struct exynos_context *platform, int freq);
int gpu_dvfs_get_voltage(struct exynos_context *platform, int freq);
char *gpu_dvfs_get_governor_list(void);

#endif /* _GPU_DVFS_GOVERNOR_H_ */
