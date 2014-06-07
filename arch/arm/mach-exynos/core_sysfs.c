/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * EXYNOS - support to view information of big.LITTLE switcher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <mach/regs-pmu.h>

static struct bus_type core_subsys = {
	.name = "b.L",
	.dev_name = "b.L",
};

#ifdef CONFIG_SCHED_HMP
extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;
#endif

static unsigned int cpu_state(unsigned int cpu)
{
	unsigned int state, offset;
#ifdef CONFIG_SCHED_HMP
	unsigned int cluster, core;
	core = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0);
	cluster = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	offset = (cluster << 2) + core;
#else
	offset = cpu;
#endif

	state = __raw_readl(EXYNOS_ARM_CORE_STATUS(offset));

	return (state & 0xf) != 0;
}

static ssize_t exynos5_core_status_show(struct kobject *kobj,
                        struct kobj_attribute *attr, char *buf)
{
        ssize_t n = 0;
        int cpu;

        for_each_possible_cpu(cpu) {
                unsigned int v = cpu_state(cpu);
                n += scnprintf(buf + n, 11, "cpu %d : %d\n", cpu, v);
        }

        return n;
}

#ifdef CONFIG_SCHED_HMP
static ssize_t exynos5_boot_cluster_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += scnprintf(buf + n, 20, "boot_cluster : %d\n",
		MPIDR_AFFINITY_LEVEL(cpu_logical_map(0), 1));

	return n;
}

static ssize_t exynos5_big_threads_show(struct kobject *kobj,
                        struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
        int cpu;
        unsigned long nr_running = 0;

        for_each_cpu_mask(cpu, hmp_fast_cpu_mask)
                nr_running += nr_running_cpu(cpu);

        n += scnprintf(buf + n, 20, "%ld\n", nr_running);

        return n;
}

static ssize_t exynos5_little_threads_show(struct kobject *kobj,
                        struct kobj_attribute *attr, char *buf)
{
        ssize_t n = 0;
        int cpu;
        unsigned long nr_running = 0;

        for_each_cpu_mask(cpu, hmp_slow_cpu_mask)
                nr_running += nr_running_cpu(cpu);

        n += scnprintf(buf + n, 20, "%ld\n", nr_running);

        return n;

}

static unsigned long long up_migrations, down_migrations;

static ssize_t exynos5_up_migrations_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += scnprintf(buf + n, 20, "%lld\n", up_migrations);

	return n;
}

static ssize_t exynos5_down_migrations_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n += scnprintf(buf + n, 20, "%lld\n", down_migrations);

	return n;
}
#endif

static struct kobj_attribute exynos5_core_status_attr =
        __ATTR(core_status, 0644, exynos5_core_status_show, NULL);
#ifdef CONFIG_SCHED_HMP
static struct kobj_attribute exynos5_boot_cluster_attr =
        __ATTR(boot_cluster, 0644, exynos5_boot_cluster_show, NULL);
static struct kobj_attribute exynos5_big_threads_attr =
        __ATTR(big_threads, 0644, exynos5_big_threads_show, NULL);
static struct kobj_attribute exynos5_little_threads_attr =
        __ATTR(little_threads, 0644, exynos5_little_threads_show, NULL);
static struct kobj_attribute exynos5_up_migrations_attr =
	__ATTR(up_migrations, 0644, exynos5_up_migrations_show, NULL);
static struct kobj_attribute exynos5_down_migrations_attr =
	__ATTR(down_migrations, 0644, exynos5_down_migrations_show, NULL);
#endif

static struct attribute *exynos5_core_sysfs_attrs[] = {
        &exynos5_core_status_attr.attr,
#ifdef CONFIG_SCHED_HMP
	&exynos5_boot_cluster_attr.attr,
	&exynos5_big_threads_attr.attr,
	&exynos5_little_threads_attr.attr,
	&exynos5_up_migrations_attr.attr,
	&exynos5_down_migrations_attr.attr,
#endif
	NULL,
};

static struct attribute_group exynos5_core_sysfs_group = {
	.attrs = exynos5_core_sysfs_attrs,
};

static const struct attribute_group *exynos5_core_sysfs_groups[] = {
	&exynos5_core_sysfs_group,
	NULL,
};

static int __init exynos5_core_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&core_subsys, exynos5_core_sysfs_groups);
	if (ret)
		pr_err("Fail to register exynos5 core subsys\n");

	return ret;
}

late_initcall(exynos5_core_sysfs_init);

#ifdef CONFIG_SCHED_HMP
static int hmp_migration_notifier_handler(struct notifier_block *nb,
                                         unsigned long cmd, void *data)
{
       switch (cmd) {
       case HMP_UP_MIGRATION:
               up_migrations++;
               break;
       case HMP_DOWN_MIGRATION:
               down_migrations++;
               break;
       default:
               break;
       }

       return 0;
}

static struct notifier_block hmp_nb = {
       .notifier_call = hmp_migration_notifier_handler,
};

static int __init exynos5_core_info_early_init(void)
{
	int ret = 0;

	ret = register_hmp_task_migration_notifier(&hmp_nb);
	if (ret)
		pr_err("Fail to register hmp notification\n");

	return ret;
}

early_initcall(exynos5_core_info_early_init);
#endif
