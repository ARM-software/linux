/*
 * drivers/gpu/ion/ion_dummy_driver.c
 *
 * Copyright (C) 2013 Linaro, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/sizes.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include "ion.h"
#include "ion_priv.h"

static struct ion_device *idev;
static struct ion_heap **heaps;

static void *carveout_ptr;
static void *chunk_ptr;

struct platform_device dummy_device_ion = {
	.name           = "ion-dummy",
	.id             = -1,
};

static struct ion_platform_heap dummy_heaps[] = {
		{
			.id	= ION_HEAP_TYPE_SYSTEM,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= "system",
		},
		{
			.id	= ION_HEAP_TYPE_SYSTEM_CONTIG,
			.type	= ION_HEAP_TYPE_SYSTEM_CONTIG,
			.name	= "system contig",
		},
		{
			.id	= ION_HEAP_TYPE_CARVEOUT,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "carveout",
			.size	= SZ_4M,
		},
		{
			.id	= ION_HEAP_TYPE_CHUNK,
			.type	= ION_HEAP_TYPE_CHUNK,
			.name	= "chunk",
			.size	= SZ_4M,
			.align	= SZ_16K,
			.priv	= (void *)(SZ_16K),
		},
		{
			.id     = ION_HEAP_TYPE_DMA,
			.type   = ION_HEAP_TYPE_DMA,
			.name   = "ion_dma_heap",
			.priv   = &dummy_device_ion.dev,
		}
};

static struct ion_platform_data dummy_ion_pdata = {
	.nr = ARRAY_SIZE(dummy_heaps),
	.heaps = dummy_heaps,
};

static void ion_dummy_parse_heap(const char *compatible, int heap)
{
	struct device_node *np;
	struct resource heap_res_dt;
	int index;

	np = of_find_compatible_node(NULL, NULL, compatible);
	if (!np) {
		pr_debug("ion_dummy: Failed to find node - %s", compatible);
		return;
	}

	if (of_address_to_resource(np, 0, &heap_res_dt)) {
		pr_warn("ion_dummy: Failed to get %s resource", compatible);
		return;
	}

	if (heap_res_dt.end <= heap_res_dt.start) {
		pr_warn("ion_dummy: Invalid %s heap size", compatible);
		return;
	}

	for (index = 0; index < dummy_ion_pdata.nr; index++) {
		if (dummy_ion_pdata.heaps[index].id == heap) {
			dummy_ion_pdata.heaps[index].size =
						resource_size(&heap_res_dt);
			dummy_ion_pdata.heaps[index].base = heap_res_dt.start;
			return;
		}
	}

	pr_warn("ion_dummy: No %s entry found in the table", compatible);
}

static int ion_dummy_declare_coherent_memory(struct ion_platform_heap *heap)
{
	struct device *dev = &dummy_device_ion.dev;

	if (!heap->size)
		return 0;

	pr_info("ion_dummy: Declare DMA coherent memory, @%lx, size: %lx\n",
		heap->base, heap->size);
	/*
	 * The coherent memory is declared as DMA_MEMORY_EXCLUSIVE. It means
	 * that if the DMA coherent memory is full, the allocation will fail
	 * instead of falling back to ARM DMA ops.
	 */
	return dma_declare_coherent_memory(dev, heap->base, heap->base,
			heap->size, DMA_MEMORY_MAP | DMA_MEMORY_EXCLUSIVE);
}

static int __init ion_dummy_init(void)
{
	int i, err;

	idev = ion_device_create(NULL);
	if (IS_ERR(idev))
		return PTR_ERR(idev);

	/* Read reserved memory from device tree */
	ion_dummy_parse_heap("ion,dma_heap", ION_HEAP_TYPE_DMA);

	heaps = kcalloc(dummy_ion_pdata.nr, sizeof(struct ion_heap *),
			GFP_KERNEL);
	if (!heaps)
		return -ENOMEM;


	/* Allocate a dummy carveout heap */
	carveout_ptr = alloc_pages_exact(
				dummy_heaps[ION_HEAP_TYPE_CARVEOUT].size,
				GFP_KERNEL);
	if (carveout_ptr)
		dummy_heaps[ION_HEAP_TYPE_CARVEOUT].base =
						virt_to_phys(carveout_ptr);
	else
		pr_err("ion_dummy: Could not allocate carveout\n");

	/* Allocate a dummy chunk heap */
	chunk_ptr = alloc_pages_exact(
				dummy_heaps[ION_HEAP_TYPE_CHUNK].size,
				GFP_KERNEL);
	if (chunk_ptr)
		dummy_heaps[ION_HEAP_TYPE_CHUNK].base = virt_to_phys(chunk_ptr);
	else
		pr_err("ion_dummy: Could not allocate chunk\n");

	for (i = 0; i < dummy_ion_pdata.nr; i++) {
		struct ion_platform_heap *heap_data = &dummy_ion_pdata.heaps[i];

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT &&
		    !heap_data->base)
			continue;

		if (heap_data->type == ION_HEAP_TYPE_CHUNK && !heap_data->base)
			continue;

		if (heap_data->type == ION_HEAP_TYPE_DMA) {
			if (ion_dummy_declare_coherent_memory(heap_data) < 0)
				pr_err("ion_dummy: Failed to declare DMA coherent memory\n");
		}

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			err = PTR_ERR(heaps[i]);
			goto err;
		}
		ion_device_add_heap(idev, heaps[i]);
	}

	return platform_device_register(&dummy_device_ion);

err:
	for (i = 0; i < dummy_ion_pdata.nr; ++i)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);

	if (carveout_ptr) {
		free_pages_exact(carveout_ptr,
				 dummy_heaps[ION_HEAP_TYPE_CARVEOUT].size);
		carveout_ptr = NULL;
	}
	if (chunk_ptr) {
		free_pages_exact(chunk_ptr,
				 dummy_heaps[ION_HEAP_TYPE_CHUNK].size);
		chunk_ptr = NULL;
	}

	dma_release_declared_memory(&dummy_device_ion.dev);

	return err;
}
device_initcall(ion_dummy_init);

static void __exit ion_dummy_exit(void)
{
	int i;

	ion_device_destroy(idev);

	for (i = 0; i < dummy_ion_pdata.nr; i++)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);

	if (carveout_ptr) {
		free_pages_exact(carveout_ptr,
				 dummy_heaps[ION_HEAP_TYPE_CARVEOUT].size);
		carveout_ptr = NULL;
	}
	if (chunk_ptr) {
		free_pages_exact(chunk_ptr,
				 dummy_heaps[ION_HEAP_TYPE_CHUNK].size);
		chunk_ptr = NULL;
	}

	dma_release_declared_memory(&dummy_device_ion.dev);
}
__exitcall(ion_dummy_exit);
