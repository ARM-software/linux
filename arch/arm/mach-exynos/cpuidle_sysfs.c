/* linux/arch/arm/mach-exynos/cpuidle.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <linux/tick.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/debugfs.h>

#include <asm/proc-fns.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <asm/unified.h>
#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/tlbflush.h>
#include <asm/cpuidle.h>

#include <mach/regs-pmu.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/exynos-pm.h>
#ifdef CONFIG_SND_SAMSUNG_AUDSS
#include <sound/exynos.h>
#endif

#include <plat/pm.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/regs-serial.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-core.h>
#include <plat/usb-phy.h>
#include <plat/clock.h>

#include <linux/module.h>
#include <linux/sched.h>

extern unsigned int enter_counter[8];
extern unsigned int early_wakeup_counter[8];
extern unsigned int enter_residency[8];
extern int test_start;

struct platform_device exynos_device_cpuidle = {
	.name	= "cpuidle_test",
	.id	= -1,
};

static ssize_t show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret += snprintf(buf+ret, PAGE_SIZE-ret, "current mode is %d\n", test_start);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%u %u %u %u\n",
						enter_counter[4],
						enter_counter[5],
						enter_counter[6],
						enter_counter[7]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%u %u %u %u\n",
						early_wakeup_counter[4],
						early_wakeup_counter[5],
						early_wakeup_counter[6],
						early_wakeup_counter[7]);
	ret += snprintf(buf+ret, PAGE_SIZE-ret, "%u %u %u %u\n",
						enter_residency[4],
						enter_residency[5],
						enter_residency[6],
						enter_residency[7]);

	return ret;
}

static ssize_t store_command(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	char test[32];

	sscanf(buf, "%31s", test);

	switch (test[0]) {
	/* test start */
	case '1':
		test_start = 1;
		break;

	/* test stop */
	case '0':
		test_start = 0;
		break;

	/* value reset */
	case 'r':
		test_start = 'r';
		break;

	/* periodically print */
	case 's':
		test_start = 's';
		break;

	default:
		printk("echo <1/0/r/s> > control\n");
	}

	return count;
}

static DEVICE_ATTR(control, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, show_status, store_command);

static struct attribute *control_device_attrs[] = {
	&dev_attr_control.attr,
	NULL,
};

static const struct attribute_group control_device_attr_group = {
	.attrs = control_device_attrs,
};

static int cpuidle_test_probe(struct platform_device *pdev)
{
	struct class *runtime_pm_class;
	struct device *runtime_pm_dev;
	int ret;

	runtime_pm_class = class_create(THIS_MODULE, "cpuidle");
	runtime_pm_dev = device_create(runtime_pm_class, NULL, 0, NULL, "test");
	ret = sysfs_create_group(&runtime_pm_dev->kobj, &control_device_attr_group);
	if (ret) {
		pr_err("Runtime PM Test : error to create sysfs\n");
		return -EINVAL;
	}

	return 0;
}

static struct platform_driver cpuidle_test_driver = {
	.probe		= cpuidle_test_probe,
	.driver		= {
		.name	= "cpuidle_test",
		.owner	= THIS_MODULE,
	},
};

static int __init cpuidle_test_driver_init(void)
{
	platform_device_register(&exynos_device_cpuidle);

	return platform_driver_register(&cpuidle_test_driver);
}
device_initcall(cpuidle_test_driver_init);
