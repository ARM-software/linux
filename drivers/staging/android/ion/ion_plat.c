/*
 * drivers/staging/android/ion-plat.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include "ion.h"

#ifdef CONFIG_ION_CARVEOUT_HEAP
static const struct ion_platform_heap_ops ion_carveout_heap_ops = {
	.heap_create = &ion_carveout_heap_create,
	.heap_destroy = &ion_carveout_heap_destroy,
};
#endif

#ifdef CONFIG_ION_CHUNK_HEAP
static const struct ion_platform_heap_ops ion_chunk_heap_ops = {
	.heap_create = &ion_chunk_heap_create,
	.heap_destroy = &ion_chunk_heap_destroy,
};
#endif

static const struct of_device_id ion_plat_match[] = {
#ifdef CONFIG_ION_CARVEOUT_HEAP
       { .compatible = "ion,carveout", .data = &ion_carveout_heap_ops, },
#endif
#ifdef CONFIG_ION_CHUNK_HEAP
       { .compatible = "ion,chunk", .data = &ion_chunk_heap_ops, },
#endif
       {},
};
MODULE_DEVICE_TABLE(of, ion_plat_match);

static int ion_plat_of(struct device *dev, struct ion_platform_heap *data)
{
	struct device_node *node = dev->of_node;
	const struct of_device_id *match = of_match_node(ion_plat_match, node);
	unsigned chunk;

	if (!match || !match->data)
		return -ENODEV;

	data->name = node->name;
	data->ops = match->data;

	if (!of_property_read_u32(node, "chunk", &chunk))
		data->priv = (void*)(uintptr_t)chunk;

	return 0;
}

static int ion_plat_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ion_platform_heap *data;
	struct ion_heap *heap;
	char *name;
	int ret;

	data = devm_kmalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = ion_plat_of(dev, data);
	if (ret)
		return ret;

	dev->platform_data = data;

	if (!data->ops->heap_create) {
		dev_err(dev, "Platform heap_create callback missing\n");
		return -ENODEV;
	}

	name = devm_kmalloc(dev, strlen(data->name) + 10, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		dev_err(dev, "reserved memory init failed: %d\n", ret);
		return ret;
	}

	heap = data->ops->heap_create(data);
        if (IS_ERR(heap)) {
		dev_err(dev, "failed to create heap\n");
                ret = PTR_ERR(heap);
		goto err;
	}
	/* Prepend and append the name to meet ION naming convention */
	sprintf(name, "ion_%s_heap", data->name);

	heap->owner = THIS_MODULE;
	data->heap = heap;
	heap->name = name;

	ion_device_add_heap(heap);

	return 0;

err:
	of_reserved_mem_device_release(dev);

	return ret;
}

static int ion_plat_remove(struct platform_device *pdev)
{
	struct ion_platform_heap *data = pdev->dev.platform_data;

	ion_device_remove_heap(data->heap);
	if (data->ops->heap_destroy)
		data->ops->heap_destroy(data);
	of_reserved_mem_device_release(&pdev->dev);

	return 0;
}

static struct platform_driver ion_plat_driver = {
       .probe = ion_plat_probe,
       .remove = ion_plat_remove,
       .driver = {
               .name = "ion_plat",
               .of_match_table = of_match_ptr(ion_plat_match),
       },
};

module_platform_driver(ion_plat_driver);

MODULE_LICENSE("GPL");
