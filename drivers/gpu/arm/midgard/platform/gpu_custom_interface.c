/* drivers/gpu/t6xx/kbase/src/platform/gpu_custom_interface.c
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
 * @file gpu_custom_interface.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/fb.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"
#ifdef CONFIG_CPU_THERMAL_IPA
#include "gpu_ipa.h"
#endif /* CONFIG_CPU_THERMAL_IPA */
#include "gpu_custom_interface.h"

#ifdef CONFIG_MALI_T6XX_DEBUG_SYS
static int gpu_get_asv_table(struct kbase_device *kbdev, char *buf, size_t buf_size)
{
	int i, cnt = 0;
	struct exynos_context *platform;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (buf == NULL)
		return 0;

	cnt += snprintf(buf+cnt, buf_size-cnt, "GPU, vol, min, max, down_stay, mif, int, cpu\n");

	for (i = platform->table_size-1; i >= 0; i--) {
		cnt += snprintf(buf+cnt, buf_size-cnt, "%d, %7d, %2d, %3d, %d, %6d, %6d, %7d\n",
		platform->table[i].clock, platform->table[i].voltage, platform->table[i].min_threshold,
		platform->table[i].max_threshold, platform->table[i].stay_count, platform->table[i].mem_freq,
		platform->table[i].int_freq, platform->table[i].cpu_freq);
	}

	return cnt;
}

static int gpu_get_dvfs_table(struct kbase_device *kbdev, char *buf, size_t buf_size)
{
	int i, cnt = 0;
	struct exynos_context *platform;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (buf == NULL)
		return 0;

	for (i = platform->table_size-1; i >= 0; i--) {
		cnt += snprintf(buf+cnt, buf_size-cnt, " %d", platform->table[i].clock);
	}
	cnt += snprintf(buf+cnt, buf_size-cnt, "\n");

	return cnt;
}

static ssize_t show_time_in_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	int i;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (gpu_control_state_set(kbdev, GPU_CONTROL_IS_POWER_ON, 0))
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_UPDATE_TIME_IN_STATE, platform->cur_clock);
	else
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_UPDATE_TIME_IN_STATE, 0);

	for (i = platform->table_size - 1; i >= 0; i--) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d %llu\n",
				platform->table[i].clock,
				platform->table[i].time);
	}

	if (ret >= PAGE_SIZE - 1) {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_time_in_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	gpu_dvfs_handler_control(kbdev, GPU_HANDLER_INIT_TIME_IN_STATE, 0);
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_clock(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	ssize_t ret = 0;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->cur_clock);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_clock(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	unsigned int freq = 0;
	int ret;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &freq);
	if (ret) {
		GPU_LOG(DVFS_WARNING, "set_clock: invalid value\n");
		return -ENOENT;
	}

	gpu_control_state_set(kbdev, GPU_CONTROL_CHANGE_CLK_VOL, freq);

	return count;
}

static ssize_t show_fbdev(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;
	int i;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	for (i = 0; i < num_registered_fb; i++)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "fb[%d] xres=%d, yres=%d, addr=0x%lx\n", i, registered_fb[i]->var.xres, registered_fb[i]->var.yres, registered_fb[i]->fix.smem_start);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_vol(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	ssize_t ret = 0;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->cur_voltage);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_utilization(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->utilization);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t show_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->dvfs_status);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	if (sysfs_streq("0", buf))
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_OFF, 0);
	else if (sysfs_streq("1", buf))
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_ON, 0);
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_asv_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	ret += gpu_get_asv_table(kbdev, buf+ret, (size_t)PAGE_SIZE-ret);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_dvfs_table(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev;
	ssize_t ret = 0;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	ret += gpu_get_dvfs_table(kbdev, buf+ret, (size_t)PAGE_SIZE-ret);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_max_lock_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	unsigned long flags;
	int locked_clock = -1;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	locked_clock = platform->max_lock;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	if (locked_clock > 0)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", locked_clock);
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "-1");

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_max_lock_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	int ret, clock = 0;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf)) {
		platform->target_lock_type = SYSFS_LOCK;
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MAX_UNLOCK, 0);
	} else {
		ret = kstrtoint(buf, 0, &clock);
		if (ret) {
			GPU_LOG(DVFS_WARNING, "set_clock: invalid value\n");
			return -ENOENT;
		}

		ret = gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GET_LEVEL, clock);
		if ((ret < 0) || (ret > platform->table_size - 1)) {
			GPU_LOG(DVFS_WARNING, "set_clock: invalid value\n");
			return -ENOENT;
		}

		if (clock == platform->table[platform->table_size-1].clock) {
			platform->target_lock_type = SYSFS_LOCK;
			gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MAX_UNLOCK, 0);
		} else {
			platform->target_lock_type = SYSFS_LOCK;
			gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MAX_LOCK, clock);
		}
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_min_lock_dvfs(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	unsigned long flags;
	int locked_clock = -1;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	locked_clock = platform->min_lock;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	if (locked_clock > 0)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", locked_clock);
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "-1");

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_min_lock_dvfs(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	int ret, clock = 0;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf)) {
		platform->target_lock_type = SYSFS_LOCK;
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MIN_UNLOCK, 0);
	} else {
		ret = kstrtoint(buf, 0, &clock);
		if (ret) {
			GPU_LOG(DVFS_WARNING, "set_clock: invalid value\n");
			return -ENOENT;
		}

		ret = gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GET_LEVEL, clock);
		if ((ret < 0) || (ret > platform->table_size - 1)) {
			GPU_LOG(DVFS_WARNING, "set_clock: invalid value\n");
			return -ENOENT;
		}

		if (clock == platform->table[0].clock) {
			platform->target_lock_type = SYSFS_LOCK;
			gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MIN_UNLOCK, 0);
		} else {
			platform->target_lock_type = SYSFS_LOCK;
			gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MIN_LOCK, clock);
		}
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_tmu(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (platform->tmu_status)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "1");
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "0");

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_tmu_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf)) {
		if (platform->voltage_margin != 0) {
			platform->voltage_margin = 0;
			gpu_control_state_set(kbdev, GPU_CONTROL_SET_MARGIN, 0);
		}
		platform->target_lock_type = TMU_LOCK;
		gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_MAX_UNLOCK, 0);
		platform->tmu_status = false;
	} else if (sysfs_streq("1", buf))
		platform->tmu_status = true;
	else
		GPU_LOG(DVFS_WARNING, "invalid val -only [0 or 1] is accepted\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_power_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct kbase_device *kbdev = dev_get_drvdata(dev);
	ssize_t ret = 0;

	if (gpu_control_state_set(kbdev, GPU_CONTROL_IS_POWER_ON, 0) > 0)
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "1");
	else
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "0");

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t show_governor(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "[List]\n%s[Current Governor] %d",
				platform->governor_list, platform->governor_type);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_governor(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	int ret;
	int next_governor_type;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &next_governor_type);

	if ((next_governor_type < 0) && (next_governor_type >= platform->governor_num)) {
		GPU_LOG(DVFS_WARNING, "set_governor: invalid value\n");
		return -ENOENT;
	}

	ret = gpu_dvfs_handler_control(kbdev, GPU_HANDLER_DVFS_GOVERNOR_CHANGE, next_governor_type);

	if (ret < 0) {
		GPU_LOG(DVFS_WARNING, "set_governor: fail to set the new governor\n");
		return -ENOENT;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_wakeup_lock(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->wakeup_lock);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_wakeup_lock(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (sysfs_streq("0", buf))
		platform->wakeup_lock = false;
	else if (sysfs_streq("1", buf))
		platform->wakeup_lock = true;
	else
		GPU_LOG(DVFS_WARNING, "invalid val -only [0 or 1] is accepted\n");
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

static ssize_t show_debug_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_get_debug_level());

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}

	return ret;
}

static ssize_t set_debug_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int debug_level, ret;

	ret = kstrtoint(buf, 0, &debug_level);
	if (ret) {
		GPU_LOG(DVFS_WARNING, "set_debug_level: invalid value\n");
		return -ENOENT;
	}

	if ((debug_level < 0) || (debug_level > DVFS_DEBUG_END)) {
		GPU_LOG(DVFS_WARNING, "set_debug_level: invalid value\n");
		return -ENOENT;
	}

	gpu_set_debug_level(debug_level);

	return count;
}

static ssize_t show_polling_speed(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = dev_get_drvdata(dev);
	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", platform->polling_speed);

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not see\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return ret;
}

static ssize_t set_polling_speed(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef CONFIG_MALI_T6XX_DVFS
	struct kbase_device *kbdev;
	struct exynos_context *platform;
	int ret, polling_speed;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

	platform = (struct exynos_context *)kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	ret = kstrtoint(buf, 0, &polling_speed);

	if ((polling_speed < 100) || (polling_speed > 1000)) {
		GPU_LOG(DVFS_WARNING, "set_polling_speed: invalid value\n");
		return -ENOENT;
	}

	platform->polling_speed = polling_speed;

	if (ret < 0) {
		GPU_LOG(DVFS_WARNING, "set_polling_speed: fail to set the new governor\n");
		return -ENOENT;
	}
#else
	GPU_LOG(DVFS_WARNING, "G3D DVFS build config is disabled. You can not set\n");
#endif /* CONFIG_MALI_T6XX_DVFS */
	return count;
}

#ifdef CONFIG_CPU_THERMAL_IPA
static ssize_t show_norm_utilization(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_EXYNOS_THERMAL
	struct kbase_device *kbdev;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;
#ifdef CONFIG_MALI_T6XX_DVFS
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%d", gpu_ipa_dvfs_get_norm_utilisation(kbdev));
#else
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "-1");
#endif

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "EXYNOS THERMAL build config is disabled. You can not set\n");
#endif

	return ret;
}

static ssize_t show_utilization_stats(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
#ifdef CONFIG_EXYNOS_THERMAL
	struct kbase_device *kbdev;
	struct mali_debug_utilisation_stats stats;

	kbdev = dev_get_drvdata(dev);

	if (!kbdev)
		return -ENODEV;

#ifdef CONFIG_MALI_T6XX_DVFS
	gpu_ipa_dvfs_get_utilisation_stats(&stats);

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "util=%d norm_util=%d norm_freq=%d time_busy=%u time_idle=%u time_tick=%d",
					stats.s.utilisation, stats.s.norm_utilisation,
					stats.s.freq_for_norm, stats.time_busy, stats.time_idle,
					stats.time_tick);
#else
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "-1");
#endif

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf+ret, PAGE_SIZE-ret, "\n");
	} else {
		buf[PAGE_SIZE-2] = '\n';
		buf[PAGE_SIZE-1] = '\0';
		ret = PAGE_SIZE-1;
	}
#else
	GPU_LOG(DVFS_WARNING, "EXYNOS THERMAL build config is disabled. You can not set\n");
#endif

	return ret;
}
#endif /* CONFIG_CPU_THERMAL_IPA */

/** The sysfs file @c clock, fbdev.
 *
 * This is used for obtaining information about the mali t6xx operating clock & framebuffer address,
 */

DEVICE_ATTR(clock, S_IRUGO|S_IWUSR, show_clock, set_clock);
DEVICE_ATTR(fbdev, S_IRUGO, show_fbdev, NULL);
DEVICE_ATTR(vol, S_IRUGO, show_vol, NULL);
DEVICE_ATTR(dvfs, S_IRUGO|S_IWUSR, show_dvfs, set_dvfs);
DEVICE_ATTR(dvfs_max_lock, S_IRUGO|S_IWUSR, show_max_lock_dvfs, set_max_lock_dvfs);
DEVICE_ATTR(dvfs_min_lock, S_IRUGO|S_IWUSR, show_min_lock_dvfs, set_min_lock_dvfs);
DEVICE_ATTR(time_in_state, S_IRUGO|S_IWUSR, show_time_in_state, set_time_in_state);
DEVICE_ATTR(tmu, S_IRUGO|S_IWUSR, show_tmu, set_tmu_control);
DEVICE_ATTR(utilization, S_IRUGO, show_utilization, NULL);
#ifdef CONFIG_CPU_THERMAL_IPA
DEVICE_ATTR(norm_utilization, S_IRUGO, show_norm_utilization, NULL);
DEVICE_ATTR(utilization_stats, S_IRUGO, show_utilization_stats, NULL);
#endif /* CONFIG_CPU_THERMAL_IPA */
DEVICE_ATTR(asv_table, S_IRUGO, show_asv_table, NULL);
DEVICE_ATTR(dvfs_table, S_IRUGO, show_dvfs_table, NULL);
DEVICE_ATTR(power_state, S_IRUGO, show_power_state, NULL);
DEVICE_ATTR(dvfs_governor, S_IRUGO|S_IWUSR, show_governor, set_governor);
DEVICE_ATTR(wakeup_lock, S_IRUGO|S_IWUSR, show_wakeup_lock, set_wakeup_lock);
DEVICE_ATTR(debug_level, S_IRUGO|S_IWUSR, show_debug_level, set_debug_level);
DEVICE_ATTR(polling_speed, S_IRUGO|S_IWUSR, show_polling_speed, set_polling_speed);

int gpu_create_sysfs_file(struct device *dev)
{
	if (device_create_file(dev, &dev_attr_clock)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [clock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_fbdev)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [fbdev]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_vol)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [vol]\n");
	goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [dvfs]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_max_lock)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [dvfs_max_lock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_min_lock)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [dvfs_min_lock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_time_in_state)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [time_in_state]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_tmu)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [tmu]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_utilization)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [utilization]\n");
		goto out;
	}
#ifdef CONFIG_CPU_THERMAL_IPA
	if (device_create_file(dev, &dev_attr_norm_utilization)) {
		dev_err(dev, "Couldn't create sysfs file [norm_utilization]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_utilization_stats)) {
		dev_err(dev, "Couldn't create sysfs file [utilization_stats]\n");
		goto out;
	}
#endif /* CONFIG_CPU_THERMAL_IPA */
	if (device_create_file(dev, &dev_attr_asv_table)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [asv_table]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_table)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [dvfs_table]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_power_state)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [power_state]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_dvfs_governor)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [dvfs_governor]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_wakeup_lock)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [wakeup_lock]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_debug_level)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [debug_level]\n");
		goto out;
	}

	if (device_create_file(dev, &dev_attr_polling_speed)) {
		GPU_LOG(DVFS_ERROR, "Couldn't create sysfs file [polling_speed]\n");
		goto out;
	}

	return 0;
out:
	return -ENOENT;
}

void gpu_remove_sysfs_file(struct device *dev)
{
	device_remove_file(dev, &dev_attr_clock);
	device_remove_file(dev, &dev_attr_fbdev);
	device_remove_file(dev, &dev_attr_vol);
	device_remove_file(dev, &dev_attr_dvfs);
	device_remove_file(dev, &dev_attr_dvfs_max_lock);
	device_remove_file(dev, &dev_attr_dvfs_min_lock);
	device_remove_file(dev, &dev_attr_time_in_state);
	device_remove_file(dev, &dev_attr_tmu);
	device_remove_file(dev, &dev_attr_utilization);
#ifdef CONFIG_CPU_THERMAL_IPA
	device_remove_file(dev, &dev_attr_norm_utilization);
	device_remove_file(dev, &dev_attr_utilization_stats);
#endif /* CONFIG_CPU_THERMAL_IPA */
	device_remove_file(dev, &dev_attr_asv_table);
	device_remove_file(dev, &dev_attr_dvfs_table);
	device_remove_file(dev, &dev_attr_power_state);
	device_remove_file(dev, &dev_attr_dvfs_governor);
	device_remove_file(dev, &dev_attr_wakeup_lock);
	device_remove_file(dev, &dev_attr_debug_level);
	device_remove_file(dev, &dev_attr_polling_speed);
}
#endif /* CONFIG_MALI_T6XX_DEBUG_SYS */
