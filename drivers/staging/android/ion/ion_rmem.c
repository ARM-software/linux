/*
 * Based on work from:
 *   Andrew Andrianov <andrew@ncrmnt.org>
 *   Google
 *   The Linux Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include "ion.h"

static int ion_rmem_device_init(struct reserved_mem *rmem, struct device *dev)
{
       struct ion_platform_heap *heap = dev_get_platdata(dev);

       heap->base = rmem->base;
       heap->size = rmem->size;

       return 0;
}

static void ion_rmem_device_release(struct reserved_mem *rmem,
                                   struct device *dev)
{
}

static const struct reserved_mem_ops rmem_dma_ops = {
       .device_init    = ion_rmem_device_init,
       .device_release = ion_rmem_device_release,
};

static int __init ion_rmem_setup(struct reserved_mem *rmem)
{
       if (of_get_flat_dt_prop(rmem->fdt_node, "no-map", NULL)) {
	       pr_err("ion-reserved: no-map option is not supported\n");
	       return -EINVAL;
       }

       pr_info("Ion memory setup at %pa size %llu MiB\n",
               &rmem->base, rmem->size / SZ_1M);
       rmem->ops = &rmem_dma_ops;

       return 0;
}

RESERVEDMEM_OF_DECLARE(ion, "ion-reserved", ion_rmem_setup);
