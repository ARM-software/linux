/*
 * devfreq: Generic Dynamic Voltage and Frequency Scaling (DVFS) Framework
 *          for Non-CPU Devices.
 *
 * Copyright (C) 2013 Samsung Electronics
 *      Sangkyu Kim <skwith.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include "exynos5430_ppmu.h"
#include "exynos_ppmu2.h"

static struct workqueue_struct *exynos_ppmu_wq;
static struct delayed_work exynos_ppmu_work;
static unsigned long exynos_ppmu_polling_period;

static DEFINE_MUTEX(exynos_ppmu_lock);
static LIST_HEAD(exynos_ppmu_list);

static struct srcu_notifier_head exynos_ppmu_notifier_list[DEVFREQ_TYPE_COUNT];

static struct ppmu_info ppmu[PPMU_COUNT] = {
	[PPMU_D0_CPU] = {
		.base = (void __iomem *)PPMU_D0_CPU_ADDR,
	},
	[PPMU_D0_GEN] = {
		.base = (void __iomem *)PPMU_D0_GEN_ADDR,
	},
	[PPMU_D0_RT] = {
		.base = (void __iomem *)PPMU_D0_RT_ADDR,
	},
	[PPMU_D1_CPU] = {
		.base = (void __iomem *)PPMU_D1_CPU_ADDR,
	},
	[PPMU_D1_GEN] = {
		.base = (void __iomem *)PPMU_D1_GEN_ADDR,
	},
	[PPMU_D1_RT] = {
		.base = (void __iomem *)PPMU_D1_RT_ADDR,
	},
};

static int exynos5430_ppmu_notifier_list_init(void)
{
	int i;

	for (i = 0; i < DEVFREQ_TYPE_COUNT; ++i)
		srcu_init_notifier_head(&exynos_ppmu_notifier_list[i]);

	return 0;
}

int exynos5430_devfreq_init(struct devfreq_exynos *de)
{
	INIT_LIST_HEAD(&de->node);

	return 0;
}

int exynos5430_devfreq_register(struct devfreq_exynos *de)
{
	int i;

	for (i = 0; i < de->ppmu_count; ++i) {
		if (ppmu_init(&de->ppmu_list[i]))
			return -EINVAL;
	}

	mutex_lock(&exynos_ppmu_lock);
	list_add_tail(&de->node, &exynos_ppmu_list);
	mutex_unlock(&exynos_ppmu_lock);

	return 0;
}

int exynos5430_ppmu_register_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&exynos_ppmu_notifier_list[type], nb);
}

int exynos5430_ppmu_unregister_notifier(enum DEVFREQ_TYPE type, struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&exynos_ppmu_notifier_list[type], nb);
}

static int exynos5430_ppmu_notify(enum DEVFREQ_TYPE type)
{
	BUG_ON(irqs_disabled());

	return srcu_notifier_call_chain(&exynos_ppmu_notifier_list[type], 0, NULL);
}

static void exynos5430_update_polling(unsigned int period)
{
	mutex_lock(&exynos_ppmu_lock);
	exynos_ppmu_polling_period = period;

	cancel_delayed_work_sync(&exynos_ppmu_work);

	if (period == 0) {
		mutex_unlock(&exynos_ppmu_lock);
		return;
	}

	queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
			msecs_to_jiffies(period));

	mutex_unlock(&exynos_ppmu_lock);
}

static void exynos5430_ppmu_update(void)
{
	struct devfreq_exynos *devfreq;

	list_for_each_entry(devfreq, &exynos_ppmu_list, node) {
		if (ppmu_count_total(devfreq->ppmu_list,
					devfreq->ppmu_count,
					&devfreq->val_ccnt,
					&devfreq->val_pmcnt)) {
			pr_err("DEVFREQ(PPMU) : ppmu can't update data\n");
			continue;
		}
	}
}

int exynos5430_ppmu_activate(void)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < PPMU_COUNT; ++i) {
		if (ppmu_init(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}

		if (ppmu_reset(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}
	}
	mutex_unlock(&exynos_ppmu_lock);

	exynos5430_update_polling(100);
	return 0;

err:
	for (; i >= 0; --i)
		ppmu_term(&ppmu[i]);

	return -EINVAL;
}

int exynos5430_ppmu_deactivate(void)
{
	int i;

	exynos5430_update_polling(0);

	mutex_lock(&exynos_ppmu_lock);
	for (i = 0; i < PPMU_COUNT; ++i) {
		if (ppmu_disable(&ppmu[i])) {
			mutex_unlock(&exynos_ppmu_lock);
			goto err;
		}
	}

	mutex_unlock(&exynos_ppmu_lock);

	return 0;

err:
	pr_err("DEVFREQ(PPMU) : can't deactivate counter\n");
	return -EINVAL;
}

static int exynos5430_ppmu_reset(void)
{
	if (ppmu_reset_total(ppmu,
			ARRAY_SIZE(ppmu))) {
		pr_err("DEVFREQ(PPMU) : ppmu can't reset data\n");
		return -EAGAIN;
	}

	return 0;
}

static void exynos5430_monitor(struct work_struct *work)
{
	int i;

	mutex_lock(&exynos_ppmu_lock);

	exynos5430_ppmu_update();

	for (i = 0; i < DEVFREQ_TYPE_COUNT; ++i)
		exynos5430_ppmu_notify(i);

	exynos5430_ppmu_reset();

	queue_delayed_work(exynos_ppmu_wq, &exynos_ppmu_work,
			msecs_to_jiffies(exynos_ppmu_polling_period));

	mutex_unlock(&exynos_ppmu_lock);
}

static int exynos5430_ppmu_probe(struct platform_device *pdev)
{
	exynos_ppmu_wq = create_freezable_workqueue("exynos5430_ppmu_wq");
	INIT_DELAYED_WORK(&exynos_ppmu_work, exynos5430_monitor);
	exynos5430_ppmu_activate();

	return 0;
}

static int exynos5430_ppmu_remove(struct platform_device *pdev)
{
	exynos5430_ppmu_deactivate();
	flush_workqueue(exynos_ppmu_wq);
	destroy_workqueue(exynos_ppmu_wq);

	return 0;
}

static int exynos5430_ppmu_suspend(struct device *dev)
{
	exynos5430_ppmu_deactivate();

	return 0;
}

static int exynos5430_ppmu_resume(struct device *dev)
{
	exynos5430_ppmu_reset();
	exynos5430_update_polling(100);

	return 0;
}

static struct dev_pm_ops exynos5430_ppmu_pm = {
	.suspend	= exynos5430_ppmu_suspend,
	.resume		= exynos5430_ppmu_resume,
};

static struct platform_driver exynos5430_ppmu_driver = {
	.probe	= exynos5430_ppmu_probe,
	.remove	= exynos5430_ppmu_remove,
	.driver	= {
		.name	= "exynos5430-ppmu",
		.owner	= THIS_MODULE,
		.pm	= &exynos5430_ppmu_pm,
	},
};

static struct platform_device exynos5430_ppmu_device = {
	.name	= "exynos5430-ppmu",
	.id	= -1,
};

static int __init exynos5430_ppmu_early_init(void)
{
	return exynos5430_ppmu_notifier_list_init();
}
arch_initcall_sync(exynos5430_ppmu_early_init);

static int __init exynos5430_ppmu_init(void)
{
	int ret;

	ret = platform_device_register(&exynos5430_ppmu_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos5430_ppmu_driver);
}
late_initcall_sync(exynos5430_ppmu_init);

static void __exit exynos5430_ppmu_exit(void)
{
	platform_driver_unregister(&exynos5430_ppmu_driver);
	platform_device_unregister(&exynos5430_ppmu_device);
}
module_exit(exynos5430_ppmu_exit);
