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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>

#define MAX_SIF	5

static void __iomem *cci_base;

struct cci_slave_interface {
	unsigned int offset[MAX_SIF];
	char ip_name[MAX_SIF][15];
};

static struct cci_slave_interface csif;

static struct bus_type cci_subsys = {
	.name = "cci",
	.dev_name = "cci",
};

void cci_snoop_enable(unsigned int sif)
{
	if (!cci_base || sif >= MAX_SIF)
		return;

	if ((__raw_readl(cci_base + csif.offset[sif]) & 3))
		return;

	__raw_writel(0x3, cci_base + csif.offset[sif]);
	dsb();

	while (__raw_readl(cci_base + 0xc) & 0x1);
}

void cci_snoop_disable(unsigned int sif)
{
	if (!cci_base || sif >= MAX_SIF)
		return;

	if (!(__raw_readl(cci_base + csif.offset[sif]) & 3))
		return;

	__raw_writel(0x0, cci_base + csif.offset[sif]);
	dsb();

	while (__raw_readl(cci_base + 0xc) & 0x1);
}

static ssize_t cci_snoop_status_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int i;

	for (i = 0; i < MAX_SIF; i++) {
		unsigned int v = __raw_readl(cci_base + csif.offset[i]);
		v = (v & 3) ? 1 : 0;
		n += scnprintf(buf + n, 50, "%s\t(CCI_SIF%d) : %d\n",
					csif.ip_name[i], i, v);
	}

	return n;
}

static struct kobj_attribute cci_snoop_status_attr =
	__ATTR(snoop_status, 0644, cci_snoop_status_show, NULL);

static struct attribute *cci_sysfs_attrs[] = {
	&cci_snoop_status_attr.attr,
	NULL,
};

static struct attribute_group cci_sysfs_group = {
	.attrs = cci_sysfs_attrs,
};

static const struct attribute_group *cci_sysfs_groups[] = {
	&cci_sysfs_group,
	NULL,
};

static int cci_init_dt(struct device_node *dn)
{
	const unsigned int *cci_reg;
	unsigned int cci_addr;
	int len, i = 0;
	struct device_node *node = NULL;

	if (!dn)
		return -ENODEV;

	cci_reg = of_get_property(dn, "reg", &len);
	if (!cci_reg)
		return -ESPIPE; /* Illegal seek */

	cci_addr = be32_to_cpup(cci_reg);

	cci_base = ioremap(cci_addr, SZ_64K);
	if (!cci_base)
		return -ENOMEM;

	pr_debug("cci_addr = %#x\n", cci_addr);

	while ((node = of_find_node_by_type(node, "cci"))) {
		const char *ip_name;
		const unsigned int *offset;

		offset = of_get_property(node, "reg", &len);
		if (!offset)
			return -ESPIPE;
		csif.offset[i] = be32_to_cpup(offset);

		if (!of_property_read_string(node, "SIF", &ip_name))
			strncpy(csif.ip_name[i], ip_name, strlen(ip_name));
		else
			return -ESPIPE;

		pr_debug("offset : %#x, ip : %s\n", csif.offset[i],
							csif.ip_name[i]);

		i++;
	}

	return 0;
}

static int cci_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (!pdev->dev.of_node) {
		pr_crit("[%s]No such device\n", __func__);
		ret = -ENODEV;
		goto cci_probe_err;
	}

	ret = cci_init_dt(pdev->dev.of_node);
	if (ret) {
		pr_info("Error to init dt of CCI : %d\n", ret);
		goto cci_probe_err;
	}

	ret = subsys_system_register(&cci_subsys, cci_sysfs_groups);
	if (ret)
		pr_info("Fail to register cci subsys\n");

cci_probe_err:
	return ret;
}

static int cci_remove(struct platform_device *pdev)
{
	struct resource *res = pdev->resource;

	release_region(res->start, resource_size(res));
	return 0;
}

static const struct of_device_id of_cci_matches[] = {
	{.compatible = "arm,cci"},
	{},
};
MODULE_DEVICE_TABLE(of, of_cci_id);

static struct platform_driver cci_platform_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "cci",
		.of_match_table = of_match_ptr(of_cci_matches),
	},
	.probe = cci_probe,
	.remove = cci_remove,
};

module_platform_driver(cci_platform_driver);
