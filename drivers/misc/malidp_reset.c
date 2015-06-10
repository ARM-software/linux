// SPDX-License-Identifier: GPL-2.0
/*
 *
 * (C) COPYRIGHT 2015-2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

#include <asm/atomic.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>

#define RESET_GLOBAL (1 << 2)
#define RESET_DC1    (1 << 1)
#define RESET_DC2    (1 << 0)
#define MALIDP_RESET_MASK   (RESET_GLOBAL | RESET_DC1 | RESET_DC2)
#define RESET_DEFAULT_MASK MALIDP_RESET_MASK

struct malidp_reset_device {
	struct device *device;
	void __iomem *regs;
	atomic_t mask;
};

static void do_reset(struct malidp_reset_device *reset_dev)
{
	int mask = atomic_read(&reset_dev->mask);

	if (mask) {
		dev_info(reset_dev->device, "Conducting reset with mask %u\n",
			mask);
		writel(mask, reset_dev->regs);
		udelay(1);
		writel(0, reset_dev->regs);
	} else {
		dev_info(reset_dev->device, "Zero mask, no reset\n");
	}
}

static ssize_t show_mask(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct malidp_reset_device *reset_dev = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%u\n", atomic_read(&reset_dev->mask));

	return ret;
}

static ssize_t store_mask(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct malidp_reset_device *reset_dev = dev_get_drvdata(dev);
	u32 val;

	if (!count)
		return 0;
	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	atomic_set(&reset_dev->mask, val & MALIDP_RESET_MASK);

	return count;
}

static ssize_t store_reset(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct malidp_reset_device *reset_dev = dev_get_drvdata(dev);

	if (count)
		do_reset(reset_dev);

	return count;
}

/*
 * "mask" sets the reset lines which will be activated when a reset is
 * requested (either by writing to "reset" or via the system-pm callbacks)
 * The bottom 3 bits are meaningful (and the rest will be ignored):
 * bit 2: Global reset
 * bit 1: Primary core reset
 * bit 0: Secodary core reset
 * N.B. Setting "mask" to zero effectively disables the reset widget
 */
static DEVICE_ATTR(mask, S_IWUSR | S_IRUGO, show_mask, store_mask);
/*
 * Writing any value to "reset" will immediately conduct a reset according to
 * the bits set in "mask"
 */
static DEVICE_ATTR(reset, S_IWUSR, NULL, store_reset);

static struct attribute *reset_attrs[] = {
	&dev_attr_mask.attr,
	&dev_attr_reset.attr,
	NULL,
};

static struct attribute_group reset_attr_group = {
	.attrs = reset_attrs,
};

static int malidp_reset_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct malidp_reset_device *dev;
	struct resource *iomem;
	void __iomem *regs;
	int mask = 0;
	int ret = 0;

	if (!np)
		return -EINVAL;

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iomem)
		return -ENODEV;

	regs = devm_ioremap_resource(&pdev->dev, iomem);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	dev = devm_kzalloc(&pdev->dev,
			   sizeof(struct malidp_reset_device),
			   GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->device = &pdev->dev;
	dev->regs = regs;

	if (of_property_read_u32(np, "mask", &mask))
		mask = RESET_DEFAULT_MASK;
	atomic_set(&dev->mask, mask & MALIDP_RESET_MASK);

	ret = sysfs_create_group(&pdev->dev.kobj, &reset_attr_group);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, dev);
	dev_info(dev->device, "Mali-DP Reset Widget probed\n");

	return ret;
}

static int malidp_reset_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &reset_attr_group);
	return 0;
}

static const struct of_device_id malidp_reset_dt_ids[] = {
	{ .compatible = "arm,malidp-reset" },
};

#ifdef CONFIG_PM_SLEEP
int malidp_reset_suspend(struct device *dev)
{
	struct malidp_reset_device *reset_dev = dev_get_drvdata(dev);
	do_reset(reset_dev);
	return 0;
}

static const struct dev_pm_ops malidp_reset_pm_ops = {
	.suspend_noirq = malidp_reset_suspend,
};
#endif

static struct platform_driver malidp_reset_driver = {
	.probe = malidp_reset_probe,
	.remove = malidp_reset_remove,
	.driver = {
		.name = "malidp-reset",
		.of_match_table = of_match_ptr(malidp_reset_dt_ids),
#ifdef CONFIG_PM_SLEEP
		.pm = &malidp_reset_pm_ops,
#endif
	},
};

module_platform_driver(malidp_reset_driver);
MODULE_LICENSE("GPL");
