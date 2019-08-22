// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */

#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/debugfs.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include "mali_aeu_dev.h"

static const struct of_device_id aeu_of_match[] = {
	{
		.compatible = "arm,mali-aeu",
	},
	{},
};

#ifdef CONFIG_IOMMU_DMA
#include <linux/dma-iommu.h>
static int
mali_aeu_iommu_fault_handler(struct iommu_domain *domain, struct device *dev,
				unsigned long iova, int flags, void *data)
{
	pr_err_ratelimited("iommu fault in %s access (iova = %#lx)\n",
			(flags & IOMMU_FAULT_WRITE) ? "write" : "read", iova);
	return -EFAULT;
}

static struct iommu_domain *mali_aeu_get_iommu(struct device *dev)
{
	struct iommu_domain *domain;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain)
		dev_err(dev, "get iommu domain failed!\n");
	else
		iommu_set_fault_handler(domain,
					mali_aeu_iommu_fault_handler,
					dev);
	return domain;
}

#else
#define mali_aeu_get_iommu(...)	(NULL)
#endif

static struct mali_aeu_device *to_aeu_device(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	return platform_get_drvdata(pdev);
}

#if defined(CONFIG_PM_SLEEP)
static int mali_aeu_sys_pm_suspend(struct device *dev)
{
	struct mali_aeu_device *adev = to_aeu_device(dev);

	dev_info(dev, "%s\n", __func__);
	if (pm_runtime_status_suspended(dev))
		return 0;

	mali_aeu_paused(adev);

	return 0;
}

static int mali_aeu_sys_pm_suspend_late(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	if (!pm_runtime_status_suspended(dev)) {
		struct mali_aeu_device *adev = to_aeu_device(dev);

		mali_aeu_hw_deactive(adev->hw_dev);
		pm_runtime_set_suspended(dev);
	}
	return 0;
}

static int mali_aeu_sys_pm_resume_early(struct device *dev)
{
	struct mali_aeu_device *adev = to_aeu_device(dev);

	mali_aeu_hw_active(adev->hw_dev);
	dev_info(dev, "%s\n", __func__);

	pm_runtime_set_active(dev);
	return 0;
}

static void mali_aeu_sys_pm_complete(struct device *dev)
{
	struct mali_aeu_device *adev = to_aeu_device(dev);
	dev_info(dev, "%s\n", __func__);

	mali_aeu_resume(adev);
}

static int mali_aeu_sys_pm_notifier(struct notifier_block *notifier,
		unsigned long event, void *data)
{
	struct mali_aeu_device *adev = container_of(notifier,
				struct mali_aeu_device, aeu_pm_nb);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		adev->status = AEU_PAUSED;
		dev_info(adev->dev, "%s: PM_SUSPEND_PREPARE\n", __func__);
		break;
	case PM_POST_SUSPEND:
		adev->status = AEU_ACTIVE;
		dev_info(adev->dev, "%s: PM_POST_SUSPEND\n", __func__);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
#endif

static int mali_aeu_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *reg_base;
	struct mali_aeu_device *adev;
	struct device_node *np = pdev->dev.of_node;
	struct dentry *parent = NULL;
	int ret;

	if (!of_match_device(pdev->dev.driver->of_match_table, &pdev->dev))
		return -1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "memory resource error!\n");
		return -1;
	}

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret)
		dev_dbg(&pdev->dev, "%s: no memory region used\n", __func__);

	if (debugfs_initialized()) {
		parent = debugfs_create_dir(AEU_NAME, NULL);
		if (IS_ERR(parent))
			parent = NULL;
	}

	adev = devm_kzalloc(&pdev->dev, sizeof(*adev), GFP_KERNEL);
	if (!adev) {
		dev_err(&pdev->dev, "alloc aeu deivce error!\n");
		ret = -ENOMEM;
		goto dbg_free;
	}

	reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(reg_base)) {
		dev_err(&pdev->dev, "register mapping is wrong!\n");
		ret = PTR_ERR(reg_base);
		goto dbg_free;
	}

	adev->iommu = mali_aeu_get_iommu(&pdev->dev);
	if (!adev->iommu)
		dev_warn(&pdev->dev, "no smmu connected\n");

	adev->hw_dev = mali_aeu_hw_init(reg_base, &pdev->dev, np, parent,
					&adev->hw_info);
	if (!adev->hw_dev) {
		dev_err(&pdev->dev, "initialize aeu hardware error!\n");
		ret = -ENOMEM;
		goto dbg_free;
	}

	ret = mali_aeu_device_init(adev, pdev, parent);
	if (ret) {
		dev_err(&pdev->dev, "init aeu device error!\n");
		goto dbg_free;
	}

	platform_set_drvdata(pdev, adev);

	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		mali_aeu_hw_active(adev->hw_dev);
		pm_runtime_set_active(&pdev->dev);
	}

#if defined(CONFIG_PM_SLEEP)
	adev->aeu_pm_nb.notifier_call = mali_aeu_sys_pm_notifier;
	ret = register_pm_notifier(&adev->aeu_pm_nb);
	if (ret) {
		dev_err(&pdev->dev, "could not register pm notifier\n");
		pm_runtime_disable(&pdev->dev);
		if (!pm_runtime_suspended(&pdev->dev)) {
			mali_aeu_hw_deactive(adev->hw_dev);
			pm_runtime_set_suspended(&pdev->dev);
		}
		goto dbg_free;
	}
#endif
	return 0;

dbg_free:
	if (parent)
		debugfs_remove_recursive(parent);
	return ret;
}

static int mali_aeu_remove(struct platform_device *pdev)
{
	struct mali_aeu_device *adev = platform_get_drvdata(pdev);

#if defined(CONFIG_PM_SLEEP)
	unregister_pm_notifier(&adev->aeu_pm_nb);
#endif
	pm_runtime_disable(&pdev->dev);
	mali_aeu_hw_exit(adev->hw_dev);
	mali_aeu_device_destroy(adev);
	of_reserved_mem_device_release(&pdev->dev);
	return 0;
}

#if defined(CONFIG_PM)
static int mali_aeu_rt_pm_suspend(struct device *dev)
{
	struct mali_aeu_device *adev = to_aeu_device(dev);

	mali_aeu_hw_deactive(adev->hw_dev);
	return 0;
}

static int mali_aeu_rt_pm_resume(struct device *dev)
{
	struct mali_aeu_device *adev = to_aeu_device(dev);

	mali_aeu_hw_active(adev->hw_dev);
	return 0;
}
#endif

static const struct dev_pm_ops mali_aeu_pm_ops = {
	SET_RUNTIME_PM_OPS(mali_aeu_rt_pm_suspend,
			mali_aeu_rt_pm_resume, NULL)
#if defined(CONFIG_PM_SLEEP)
	.suspend = mali_aeu_sys_pm_suspend,
	.suspend_late = mali_aeu_sys_pm_suspend_late,
	.resume_early = mali_aeu_sys_pm_resume_early,
	.complete = mali_aeu_sys_pm_complete,
#endif
};

static struct platform_driver mali_aeu_driver = {
	.probe = mali_aeu_probe,
	.remove = mali_aeu_remove,
	.driver = {
		.name = AEU_NAME,
		.of_match_table = aeu_of_match,
		.pm = &mali_aeu_pm_ops,
	},
};

module_platform_driver(mali_aeu_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mali AEU driver");
MODULE_VERSION("1:0.0");
MODULE_AUTHOR("Jonathan Chai <jonathan.chai@arm.com>");
