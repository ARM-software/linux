// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */

#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include "mali_aeu_dev.h"

static struct clk *aclk;

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

static int mali_aeu_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *reg_base;
	struct mali_aeu_device *adev;
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

	adev = devm_kzalloc(&pdev->dev, sizeof(*adev), GFP_KERNEL);
	if (!adev) {
		dev_err(&pdev->dev, "alloc adu deivce error!\n");
		return -ENOMEM;
	}

	aclk = devm_clk_get(&pdev->dev, "aclk");
	if (IS_ERR(aclk)) {
		dev_err(&pdev->dev, "aclk failed!\n");
		return -1;
	}

	clk_prepare_enable(aclk);

	reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(reg_base)) {
		dev_err(&pdev->dev, "register mapping is wrong!\n");
		return PTR_ERR(reg_base);
	}

	adev->iommu = mali_aeu_get_iommu(&pdev->dev);
	if (!adev->iommu)
		dev_warn(&pdev->dev, "no smmu connected\n");

	adev->hw_dev = mali_aeu_hw_init(reg_base, &pdev->dev, &adev->hw_info);
	if (!adev->hw_dev) {
		dev_err(&pdev->dev, "initialize aeu hardware error!\n");
		return -ENOMEM;
	}

	ret = mali_aeu_device_init(adev, pdev);
	if (ret) {
		dev_err(&pdev->dev, "init aeu device error!\n");
		return ret;
	}

	platform_set_drvdata(pdev, adev);
	return 0;
}

static int mali_aeu_remove(struct platform_device *pdev)
{
	struct mali_aeu_device *adev = platform_get_drvdata(pdev);

	mali_aeu_hw_exit(adev->hw_dev);
	mali_aeu_device_destroy(adev);
	of_reserved_mem_device_release(&pdev->dev);
	clk_disable_unprepare(aclk);
	devm_clk_put(&pdev->dev, aclk);
	return 0;
}

static struct platform_driver mali_aeu_driver = {
	.probe = mali_aeu_probe,
	.remove = mali_aeu_remove,
	.driver = {
		.name = "mali-aeu",
		.of_match_table = aeu_of_match,
	},
};

module_platform_driver(mali_aeu_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mali AEU driver");
MODULE_VERSION("1:0.0");
MODULE_AUTHOR("Jonathan Chai <jonathan.chai@arm.com>");
