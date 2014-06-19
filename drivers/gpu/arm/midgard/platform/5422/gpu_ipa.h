
#ifndef _GPU_IPA_H_
#define _GPU_IPA_H_

#include <mali_kbase.h>
#include "mali_kbase_platform.h"

#if SOC_NAME == 5422
#define POWER_COEFF_GPU 59 /* all core on param */
#elif SOC_NAME == 5430
#define POWER_COEFF_GPU 46 /* Scale_to_Helsinki (0.777777778) value was multiplied */
#endif

int gpu_ipa_dvfs_get_norm_utilisation(struct kbase_device *kbdev);
void gpu_ipa_dvfs_get_utilisation_stats(struct mali_debug_utilisation_stats *stats);
void gpu_ipa_dvfs_calc_norm_utilisation(struct kbase_device *kbdev);
int gpu_ipa_dvfs_max_lock(int clock);
int gpu_ipa_dvfs_max_unlock(void);
int get_ipa_dvfs_max_freq(void);
#endif
