/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <mach/pm_domains.h>

/* Sub-domain does not have power on/off features.
 * dummy power on/off function is required.
 */
static inline int exynos_pd_power_dummy(struct exynos_pm_domain *pd,
					int power_flags)
{
	DEBUG_PRINT_INFO("%s: dummy power %s\n", pd->genpd.name, power_flags ? "on":"off");
	pd->status = power_flags;

	return 0;
}

static int exynos_pd_status(struct exynos_pm_domain *pd)
{
	if (unlikely(!pd))
		return -EINVAL;
	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* check STATUS register */
		pd->status = (__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN);
	}
	mutex_unlock(&pd->access_lock);

	return pd->status;
}

static int exynos_pd_power(struct exynos_pm_domain *pd, int power_flags)
{
	unsigned long timeout;

	if (unlikely(!pd))
		return -EINVAL;

	mutex_lock(&pd->access_lock);
	if (likely(pd->base)) {
		/* sc_feedback to OPTION register */
		__raw_writel(pd->pd_option, pd->base+0x8);

		/* on/off value to CONFIGURATION register */
		__raw_writel(power_flags, pd->base);

		/* Wait max 1ms */
		timeout = 100;
		/* check STATUS register */
		while ((__raw_readl(pd->base+0x4) & EXYNOS_INT_LOCAL_PWR_EN) != power_flags) {
			if (timeout == 0) {
				pr_err("%s@%p: %08x, %08x, %08x\n",
					pd->genpd.name,
					pd->base,
					__raw_readl(pd->base),
					__raw_readl(pd->base+4),
					__raw_readl(pd->base+8));
				pr_err(PM_DOMAIN_PREFIX "%s can't control power, timeout\n", pd->name);
				mutex_unlock(&pd->access_lock);
#if defined(CONFIG_SOC_EXYNOS5422)
				pr_info("\e[1;33m  ######### cmu register dump on timeout ######### \e[0m\n");
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_EPLL_CON0", readl(EXYNOS5_EPLL_CON0));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_DPLL_CON0", readl(EXYNOS5_DPLL_CON0));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_MPLL_CON0", readl(EXYNOS5_MPLL_CON0));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_SPLL_CON0", readl(EXYNOS5_SPLL_CON0));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_SRC_TOP6", readl(EXYNOS5_CLK_SRC_TOP6));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_MUX_STAT_TOP6", readl(EXYNOS5_CLK_MUX_STAT_TOP6));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_SRC_TOP7", readl(EXYNOS5_CLK_SRC_TOP7));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_SRC_MASK_TOP7", readl(EXYNOS5_CLK_SRC_MASK_TOP7));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_MUX_STAT_TOP7", readl(EXYNOS5_CLK_MUX_STAT_TOP7));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_SRC_TOP9", readl(EXYNOS5_CLK_SRC_TOP9));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_MUX_STAT_TOP9", readl(EXYNOS5_CLK_MUX_STAT_TOP9));
				pr_info("$$$$$$$$$ %s : %08X\n","EXYNOS5_CLK_GATE_BUS_TOP", readl(EXYNOS5_CLK_GATE_BUS_TOP));
#endif
				return -ETIMEDOUT;
			}
			--timeout;
			cpu_relax();
			usleep_range(8, 10);
		}
		if (unlikely(timeout < 50)) {
			pr_warn(PM_DOMAIN_PREFIX "long delay found during %s is %s\n", pd->name, power_flags ? "on":"off");
			pr_warn("%s@%p: %08x, %08x, %08x\n",
				pd->name,
				pd->base,
				__raw_readl(pd->base),
				__raw_readl(pd->base+4),
				__raw_readl(pd->base+8));
		}
	}
	pd->status = power_flags;
	mutex_unlock(&pd->access_lock);

	DEBUG_PRINT_INFO("%s@%p: %08x, %08x, %08x\n",
	pd->genpd.name, pd->base,
	__raw_readl(pd->base),
	__raw_readl(pd->base+4),
	__raw_readl(pd->base+8));

	return 0;
}

/* exynos_genpd_power_on - power-on callback function for genpd.
 * @genpd: generic power domain.
 *
 * main function of power on sequence.
 * 1. clock on
 * 2. reconfiguration of clock
 * 3. pd power on
 * 4. set bts configuration
 * 5. restore clock sources.
 */
static int exynos_genpd_power_on(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *pd = container_of(genpd, struct exynos_pm_domain, genpd);
	int ret;

	if (unlikely(!pd->on)) {
		pr_err(PM_DOMAIN_PREFIX "%s cannot support power on\n", pd->name);
		return -EINVAL;
	}

	/* clock enable before pd on */
	if (pd->cb && pd->cb->on_pre)
		pd->cb->on_pre(pd);

	/* power domain on */
	ret = pd->on(pd, EXYNOS_INT_LOCAL_PWR_EN);
	if (unlikely(ret)) {
		pr_err(PM_DOMAIN_PREFIX "%s makes error at power on!\n", genpd->name);
		return ret;
	}

	if (pd->cb && pd->cb->on_post)
		pd->cb->on_post(pd);

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)
	/* enable bts features if exists */
	if (pd->bts)
		bts_initialize(pd->name, true);
#endif

	return 0;
}

/* exynos_genpd_power_off - power-off callback function for genpd.
 * @genpd: generic power domain.
 *
 * main function of power off sequence.
 * 1. clock on
 * 2. reset IPs when necessary
 * 3. pd power off
 * 4. change clock sources to OSC.
 */
static int exynos_genpd_power_off(struct generic_pm_domain *genpd)
{
	struct exynos_pm_domain *pd = container_of(genpd, struct exynos_pm_domain, genpd);
	int ret;

	if (unlikely(!pd->off)) {
		pr_err(PM_DOMAIN_PREFIX "%s can't support power off!\n", genpd->name);
		return -EINVAL;
	}

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)
	/* disable bts features if exists */
	if (pd->bts)
		bts_initialize(pd->name, false);
#endif

	if (pd->cb && pd->cb->off_pre)
		pd->cb->off_pre(pd);

	ret = pd->off(pd, 0);
	if (unlikely(ret)) {
		pr_err(PM_DOMAIN_PREFIX "%s occur error at power off!\n", genpd->name);
		return ret;
	}

	if (pd->cb && pd->cb->off_post)
		pd->cb->off_post(pd);

	return 0;
}

#ifdef CONFIG_OF

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)
/**
 *  of_device_bts_is_available - check if bts feature is enabled or not
 *
 *  @device: Node to check for availability, with locks already held
 *
 *  Returns 1 if the status property is "enabled" or "ok",
 *  0 otherwise
 */
static int of_device_bts_is_available(const struct device_node *device)
{
	const char *status;
	int statlen;

	status = of_get_property(device, "bts-status", &statlen);
	if (status == NULL)
		return 0;

	if (statlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "ok"))
			return 1;
	}

	return 0;
}
#endif

static void exynos_pm_powerdomain_init(struct exynos_pm_domain *pd)
{
	pd->genpd.power_off = exynos_genpd_power_off;
	pd->genpd.power_on = exynos_genpd_power_on;

	/* pd power on/off latency is less than 1ms */
	pd->genpd.power_on_latency_ns = 1000000;
	pd->genpd.power_off_latency_ns = 1000000;

	pd->status = true;
	pd->check_status = exynos_pd_status;

#if defined(CONFIG_EXYNOS5430_BTS) || defined(CONFIG_EXYNOS5422_BTS)
	do {
		int ret;

		/* bts feature is enabled if exists */
		ret = of_device_bts_is_available(pd->genpd.of_node);
		if (ret) {
			pd->bts = 1;
			bts_initialize(pd->name, true);
			DEBUG_PRINT_INFO("%s - bts feature is enabled\n", pd->name);
		}
	} while(0);
#endif

	mutex_init(&pd->access_lock);

	pm_genpd_init(&pd->genpd, NULL, !pd->status);
}

/* show_power_domain - show current power domain status.
 *
 * read the status of power domain and show it.
 */
static void show_power_domain(void)
{
	struct device_node *np;
	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct platform_device *pdev;
		struct exynos_pm_domain *pd;

		if (of_device_is_available(np)) {
			pdev = of_find_device_by_node(np);
			if (!pdev)
				continue;
			pd = platform_get_drvdata(pdev);
			pr_info("   %-9s - %-3s\n", pd->genpd.name,
					pd->check_status(pd) ? "on" : "off");
		} else
			pr_info("   %-9s - %s\n",
						kstrdup(np->name, GFP_KERNEL),
						"on,  always");
	}

	return;
}

/* when latency is over pre-defined value warning message will be shown.
 * default values are:
 * start/stop latency - 50us
 * state save/restore latency - 500us
 */
static struct gpd_timing_data gpd_td = {
	.stop_latency_ns = 50000,
	.start_latency_ns = 50000,
	.save_state_latency_ns = 500000,
	.restore_state_latency_ns = 500000,
};

static void exynos_add_device_to_pd(struct exynos_pm_domain *pd,
					 struct device *dev)
{
	int ret;

	if (unlikely(!pd)) {
		pr_err(PM_DOMAIN_PREFIX "can't add device, domain is empty\n");
		return;
	}

	if (unlikely(!dev)) {
		pr_err(PM_DOMAIN_PREFIX "can't add device, device is empty\n");
		return;
	}

	DEBUG_PRINT_INFO("adding %s to power domain %s\n", dev_name(dev), pd->genpd.name);

	while (1) {
		ret = __pm_genpd_add_device(&pd->genpd, dev, &gpd_td);
		if (ret != -EAGAIN)
			break;
		cond_resched();
	}

	if (!ret) {
		pm_genpd_dev_need_restore(dev, true);
		pr_info(PM_DOMAIN_PREFIX "%s, Device : %s Registered\n", pd->genpd.name, dev_name(dev));
	} else
		pr_err(PM_DOMAIN_PREFIX "%s can't add device %s\n", pd->genpd.name, dev_name(dev));
}

static void exynos_remove_device_from_pd(struct device *dev)
{
	struct generic_pm_domain *genpd = dev_to_genpd(dev);
	int ret;

	DEBUG_PRINT_INFO("removing %s from power domain %s\n", dev_name(dev), genpd->name);

	while (1) {
		ret = pm_genpd_remove_device(genpd, dev);
		if (ret != -EAGAIN)
			break;
		cond_resched();
	}
}

static void exynos_read_domain_from_dt(struct device *dev)
{
	struct platform_device *pd_pdev;
	struct exynos_pm_domain *pd;
	struct device_node *node;

	/* add platform device to power domain
	 * 1. get power domain node for given platform device
	 * 2. get power domain device from power domain node
	 * 3. get power domain structure from power domain device
	 * 4. add given platform device to power domain structure.
	 */
	node = of_parse_phandle(dev->of_node, "samsung,power-domain", 0);
	if (!node)
		return;

	pd_pdev = of_find_device_by_node(node);
	if (!pd_pdev)
		return;

	pd = platform_get_drvdata(pd_pdev);
	exynos_add_device_to_pd(pd, dev);
}

static int exynos_pm_notifier_call(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct device *dev = data;

	switch (event) {
	case BUS_NOTIFY_BIND_DRIVER:
		if (dev->of_node)
			exynos_read_domain_from_dt(dev);
		break;

	case BUS_NOTIFY_UNBOUND_DRIVER:
		exynos_remove_device_from_pd(dev);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block platform_nb = {
	.notifier_call = exynos_pm_notifier_call,
};

static __init int exynos_pm_dt_parse_domains(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *pd;
		struct device_node *children;
		int ret, val;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			pr_err(PM_DOMAIN_PREFIX "%s: failed to allocate memory for domain\n",
					__func__);
			return -ENOMEM;
		}

		pd->genpd.name = kstrdup(np->name, GFP_KERNEL);
		pd->name = pd->genpd.name;
		pd->genpd.of_node = np;
		pd->base = of_iomap(np, 0);
		pd->on = exynos_pd_power;
		pd->off = exynos_pd_power;
		pd->cb = exynos_pd_find_callback(pd);

		ret = of_property_read_u32_index(np, "pd-option", 0, &val);
		if (ret)
			pd->pd_option = 0x0102;
		else
			pd->pd_option = val;

		if (pd->cb) {
			if (pd->cb->on)
				pd->on = pd->cb->on;
			if (pd->cb->off)
				pd->off = pd->cb->off;
		}

		platform_set_drvdata(pdev, pd);

		exynos_pm_powerdomain_init(pd);

		/* add LOGICAL sub-domain
		 * It is not assumed that there is REAL sub-domain.
		 * Power on/off functions are not defined here.
		 */
		for_each_child_of_node(np, children) {
			struct exynos_pm_domain *sub_pd;
			struct platform_device *sub_pdev;

			if (!children)
				break;

			sub_pd = kzalloc(sizeof(*sub_pd), GFP_KERNEL);
			if (!sub_pd) {
				pr_err("%s: failed to allocate memory for power domain\n",
						__func__);
				return -ENOMEM;
			}

			sub_pd->genpd.name = kstrdup(children->name, GFP_KERNEL);
			sub_pd->name = sub_pd->genpd.name;
			sub_pd->genpd.of_node = children;
			sub_pd->on = exynos_pd_power_dummy;
			sub_pd->off = exynos_pd_power_dummy;
			sub_pd->cb = NULL;

			/* kernel does not create sub-domain pdev. */
			sub_pdev = of_find_device_by_node(children);
			if (!sub_pdev)
				sub_pdev = of_platform_device_create(children, NULL, &pdev->dev);
			if (!sub_pdev) {
				pr_err(PM_DOMAIN_PREFIX "sub domain allocation failed: %s\n",
							kstrdup(children->name, GFP_KERNEL));
				continue;
			}
			platform_set_drvdata(sub_pdev, sub_pd);

			exynos_pm_powerdomain_init(sub_pd);

			if (pm_genpd_add_subdomain(&pd->genpd, &sub_pd->genpd))
				pr_err("PM DOMAIN: %s can't add subdomain %s\n", pd->genpd.name, sub_pd->genpd.name);
		}
	}

	/* EXCEPTION: add physical sub-pd to master pd using device tree */
	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		struct exynos_pm_domain *parent_pd, *child_pd;
		struct device_node *parent;
		struct platform_device *parent_pd_pdev, *child_pd_pdev;
		int i;

		/* skip unmanaged power domain */
		if (!of_device_is_available(np))
			continue;

		/* child_pd_pdev should have value. */
		child_pd_pdev = of_find_device_by_node(np);
		child_pd = platform_get_drvdata(child_pd_pdev);

		/* search parents in device tree */
		for (i = 0; i < MAX_PARENT_POWER_DOMAIN; i++) {
			/* find parent node if available */
			parent = of_parse_phandle(np, "parent", i);
			if (!parent)
				break;

			/* display error when parent is unmanaged. */
			if (!of_device_is_available(parent)) {
				pr_err(PM_DOMAIN_PREFIX "%s is not managed by runtime pm.\n",
						kstrdup(parent->name, GFP_KERNEL));
				continue;
			}

			/* parent_pd_pdev should have value. */
			parent_pd_pdev = of_find_device_by_node(parent);
			parent_pd = platform_get_drvdata(parent_pd_pdev);

			if (pm_genpd_add_subdomain(&parent_pd->genpd, &child_pd->genpd))
				pr_err(PM_DOMAIN_PREFIX "%s cannot add subdomain %s\n",
						parent_pd->name, child_pd->name);
			else
				pr_info(PM_DOMAIN_PREFIX "%s has a new child %s.\n",
						parent_pd->name, child_pd->name);
		}
	}

	bus_register_notifier(&platform_bus_type, &platform_nb);

	pr_info("EXYNOS5: PM Domain Initialize\n");
	/* show information of power domain registration */
	show_power_domain();

	return 0;
}
#endif

static int __init exynos5_pm_domain_init(void)
{
#ifdef CONFIG_OF
	if (of_have_populated_dt())
		return exynos_pm_dt_parse_domains();
#endif

	pr_err(PM_DOMAIN_PREFIX "PM Domain works along with Device Tree\n");
	return -EPERM;
}
arch_initcall(exynos5_pm_domain_init);

static __init int exynos_pm_domain_idle(void)
{
	unsigned long j1 = jiffies+HZ;

	/* HACK: wait 1sec not to interfere late-probed devices */
	while(time_before(jiffies, j1))
		schedule();

	pr_info(PM_DOMAIN_PREFIX "Power off unused power domains.\n");
	pm_genpd_poweroff_unused();

	return 0;
}
late_initcall_sync(exynos_pm_domain_idle);
