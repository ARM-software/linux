/*
 * pin-controller/pin-mux/pin-config/gpio-driver for Samsung's SoC's.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * Copyright (c) 2012 Linaro Ltd
 *		http://www.linaro.org
 *
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This driver implements the Samsung pinctrl driver. It supports setting up of
 * pinmux and pinconf configurations. The gpiolib interface is also included.
 * External interrupt (gpio and wakeup) support are not included in this driver
 * but provides extensions to which platform specific implementation of the gpio
 * and wakeup interrupts can be hooked to.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irqdomain.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>

#include <mach/pinctrl-samsung.h>
#include <mach/exynos-pm.h>

#include "core.h"

#define GROUP_SUFFIX		"-grp"
#define GSUFFIX_LEN		sizeof(GROUP_SUFFIX)
#define FUNCTION_SUFFIX		"-mux"
#define FSUFFIX_LEN		sizeof(FUNCTION_SUFFIX)

/* list of all possible config options supported */
static struct pin_config {
	const char *property;
	enum pincfg_type param;
} cfg_params[] = {
	{ "samsung,pin-pud", PINCFG_TYPE_PUD },
	{ "samsung,pin-drv", PINCFG_TYPE_DRV },
	{ "samsung,pin-con-pdn", PINCFG_TYPE_CON_PDN },
	{ "samsung,pin-pud-pdn", PINCFG_TYPE_PUD_PDN },
	{ "samsung,pin-val", PINCFG_TYPE_DAT },
};

/* Global list of devices (struct samsung_pinctrl_drv_data) */
LIST_HEAD(drvdata_list);

static unsigned int pin_base;

static inline struct samsung_pin_bank *gc_to_pin_bank(struct gpio_chip *gc)
{
	return container_of(gc, struct samsung_pin_bank, gpio_chip);
}

static int samsung_get_group_count(struct pinctrl_dev *pctldev)
{
	struct samsung_pinctrl_drv_data *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pmx->nr_groups;
}

static const char *samsung_get_group_name(struct pinctrl_dev *pctldev,
						unsigned group)
{
	struct samsung_pinctrl_drv_data *pmx = pinctrl_dev_get_drvdata(pctldev);

	return pmx->pin_groups[group].name;
}

static int samsung_get_group_pins(struct pinctrl_dev *pctldev,
					unsigned group,
					const unsigned **pins,
					unsigned *num_pins)
{
	struct samsung_pinctrl_drv_data *pmx = pinctrl_dev_get_drvdata(pctldev);

	*pins = pmx->pin_groups[group].pins;
	*num_pins = pmx->pin_groups[group].num_pins;
	return 0;
}

static int reserve_map(struct device *dev, struct pinctrl_map **map,
		       unsigned *reserved_maps, unsigned *num_maps,
		       unsigned reserve)
{
	unsigned old_num = *reserved_maps;
	unsigned new_num = *num_maps + reserve;
	struct pinctrl_map *new_map;

	if (old_num >= new_num)
		return 0;

	new_map = krealloc(*map, sizeof(*new_map) * new_num, GFP_KERNEL);
	if (!new_map) {
		dev_err(dev, "krealloc(map) failed\n");
		return -ENOMEM;
	}

	memset(new_map + old_num, 0, (new_num - old_num) * sizeof(*new_map));

	*map = new_map;
	*reserved_maps = new_num;

	return 0;
}

static int add_map_mux(struct pinctrl_map **map, unsigned *reserved_maps,
		       unsigned *num_maps, const char *group,
		       const char *function)
{
	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	(*map)[*num_maps].type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)[*num_maps].data.mux.group = group;
	(*map)[*num_maps].data.mux.function = function;
	(*num_maps)++;

	return 0;
}

static int add_map_configs(struct device *dev, struct pinctrl_map **map,
			   unsigned *reserved_maps, unsigned *num_maps,
			   const char *group, unsigned long *configs,
			   unsigned num_configs)
{
	unsigned long *dup_configs;

	if (WARN_ON(*num_maps == *reserved_maps))
		return -ENOSPC;

	dup_configs = kmemdup(configs, num_configs * sizeof(*dup_configs),
			      GFP_KERNEL);
	if (!dup_configs) {
		dev_err(dev, "kmemdup(configs) failed\n");
		return -ENOMEM;
	}

	(*map)[*num_maps].type = PIN_MAP_TYPE_CONFIGS_GROUP;
	(*map)[*num_maps].data.configs.group_or_pin = group;
	(*map)[*num_maps].data.configs.configs = dup_configs;
	(*map)[*num_maps].data.configs.num_configs = num_configs;
	(*num_maps)++;

	return 0;
}

static int add_config(struct device *dev, unsigned long **configs,
		      unsigned *num_configs, unsigned long config)
{
	unsigned old_num = *num_configs;
	unsigned new_num = old_num + 1;
	unsigned long *new_configs;

	new_configs = krealloc(*configs, sizeof(*new_configs) * new_num,
			       GFP_KERNEL);
	if (!new_configs) {
		dev_err(dev, "krealloc(configs) failed\n");
		return -ENOMEM;
	}

	new_configs[old_num] = config;

	*configs = new_configs;
	*num_configs = new_num;

	return 0;
}

static void samsung_dt_free_map(struct pinctrl_dev *pctldev,
				      struct pinctrl_map *map,
				      unsigned num_maps)
{
	int i;

	for (i = 0; i < num_maps; i++)
		if (map[i].type == PIN_MAP_TYPE_CONFIGS_GROUP)
			kfree(map[i].data.configs.configs);

	kfree(map);
}

static int samsung_dt_subnode_to_map(struct samsung_pinctrl_drv_data *drvdata,
				     struct device *dev,
				     struct device_node *np,
				     struct pinctrl_map **map,
				     unsigned *reserved_maps,
				     unsigned *num_maps)
{
	int ret, i;
	u32 val;
	unsigned long config;
	unsigned long *configs = NULL;
	unsigned num_configs = 0;
	unsigned reserve;
	struct property *prop;
	const char *group;
	bool has_func = false;

	ret = of_property_read_u32(np, "samsung,pin-function", &val);
	if (!ret)
		has_func = true;

	for (i = 0; i < ARRAY_SIZE(cfg_params); i++) {
		ret = of_property_read_u32(np, cfg_params[i].property, &val);
		if (!ret) {
			config = PINCFG_PACK(cfg_params[i].param, val);
			ret = add_config(dev, &configs, &num_configs, config);
			if (ret < 0)
				goto exit;
		/* EINVAL=missing, which is fine since it's optional */
		} else if (ret != -EINVAL) {
			dev_err(dev, "could not parse property %s\n",
				cfg_params[i].property);
		}
	}

	reserve = 0;
	if (has_func)
		reserve++;
	if (num_configs)
		reserve++;
	ret = of_property_count_strings(np, "samsung,pins");
	if (ret < 0) {
		dev_err(dev, "could not parse property samsung,pins\n");
		goto exit;
	}
	reserve *= ret;

	ret = reserve_map(dev, map, reserved_maps, num_maps, reserve);
	if (ret < 0)
		goto exit;

	of_property_for_each_string(np, "samsung,pins", prop, group) {
		if (has_func) {
			ret = add_map_mux(map, reserved_maps,
						num_maps, group, np->full_name);
			if (ret < 0)
				goto exit;
		}

		if (num_configs) {
			ret = add_map_configs(dev, map, reserved_maps,
					      num_maps, group, configs,
					      num_configs);
			if (ret < 0)
				goto exit;
		}
	}

	ret = 0;
exit:
	kfree(configs);
	return ret;
}

static int samsung_dt_node_to_map(struct pinctrl_dev *pctldev,
					struct device_node *np_config,
					struct pinctrl_map **map,
					unsigned *num_maps)
{
	struct samsung_pinctrl_drv_data *drvdata;
	unsigned reserved_maps;
	struct device_node *np;
	int ret;

	drvdata = pinctrl_dev_get_drvdata(pctldev);

	reserved_maps = 0;
	*map = NULL;
	*num_maps = 0;

	if (!of_get_child_count(np_config))
		return samsung_dt_subnode_to_map(drvdata, pctldev->dev,
							np_config, map,
							&reserved_maps,
							num_maps);

	for_each_child_of_node(np_config, np) {
		ret = samsung_dt_subnode_to_map(drvdata, pctldev->dev, np, map,
						&reserved_maps, num_maps);
		if (ret < 0) {
			samsung_dt_free_map(pctldev, *map, *num_maps);
			return ret;
		}
	}

	return 0;
}

/* GPIO register names */
static char *gpio_regs[] = {"CON", "DAT", "PUD", "DRV", "CON_PDN", "PUD_PDN"};

static void pin_to_reg_bank(struct samsung_pinctrl_drv_data *drvdata,
			unsigned pin, void __iomem **reg, u32 *offset,
			struct samsung_pin_bank **bank);

/* common debug show function */
static void samsung_pin_dbg_show_by_type(struct samsung_pin_bank *bank,
				void __iomem *reg_base, u32 pin_offset,
				struct seq_file *s, unsigned pin,
				enum pincfg_type cfg_type)
{
	struct samsung_pin_bank_type *type;
	u32 data, width, mask, shift, cfg_reg;

	type = bank->type;

	if (!type->fld_width[cfg_type])
		return;

	width = type->fld_width[cfg_type];
	cfg_reg = type->reg_offset[cfg_type];
	mask = (1 << width) - 1;
	shift = pin_offset * width;

	data = readl(reg_base + cfg_reg);

	data >>= shift;
	data &= mask;

	seq_printf(s, " %s(0x%x)", gpio_regs[cfg_type], data);
}

/* show GPIO register status */
static void samsung_pin_dbg_show(struct pinctrl_dev *pctldev,
				struct seq_file *s, unsigned pin)
{
	struct samsung_pinctrl_drv_data *drvdata;
	struct samsung_pin_bank *bank;
	void __iomem *reg_base;
	u32 pin_offset;
	unsigned long flags;
	enum pincfg_type cfg_type;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pin_to_reg_bank(drvdata, pin - drvdata->ctrl->base, &reg_base,
					&pin_offset, &bank);

	spin_lock_irqsave(&bank->slock, flags);

	for (cfg_type = 0; cfg_type < PINCFG_TYPE_NUM; cfg_type++) {
		samsung_pin_dbg_show_by_type(bank, reg_base,
					pin_offset, s, pin, cfg_type);
	}

	spin_unlock_irqrestore(&bank->slock, flags);
}

/* list of pinctrl callbacks for the pinctrl core */
static const struct pinctrl_ops samsung_pctrl_ops = {
	.get_groups_count	= samsung_get_group_count,
	.get_group_name		= samsung_get_group_name,
	.get_group_pins		= samsung_get_group_pins,
	.dt_node_to_map		= samsung_dt_node_to_map,
	.dt_free_map		= samsung_dt_free_map,
	.pin_dbg_show		= samsung_pin_dbg_show,
};

/* check if the selector is a valid pin function selector */
static int samsung_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct samsung_pinctrl_drv_data *drvdata;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	return drvdata->nr_functions;
}

/* return the name of the pin function specified */
static const char *samsung_pinmux_get_fname(struct pinctrl_dev *pctldev,
						unsigned selector)
{
	struct samsung_pinctrl_drv_data *drvdata;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	return drvdata->pmx_functions[selector].name;
}

/* return the groups associated for the specified function selector */
static int samsung_pinmux_get_groups(struct pinctrl_dev *pctldev,
		unsigned selector, const char * const **groups,
		unsigned * const num_groups)
{
	struct samsung_pinctrl_drv_data *drvdata;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	*groups = drvdata->pmx_functions[selector].groups;
	*num_groups = drvdata->pmx_functions[selector].num_groups;
	return 0;
}

/*
 * given a pin number that is local to a pin controller, find out the pin bank
 * and the register base of the pin bank.
 */
static void pin_to_reg_bank(struct samsung_pinctrl_drv_data *drvdata,
			unsigned pin, void __iomem **reg, u32 *offset,
			struct samsung_pin_bank **bank)
{
	struct samsung_pin_bank *b;

	b = drvdata->ctrl->pin_banks;

	while ((pin >= b->pin_base) &&
			((b->pin_base + b->nr_pins - 1) < pin))
		b++;

	*reg = drvdata->virt_base + b->pctl_offset;
	*offset = pin - b->pin_base;
	if (bank)
		*bank = b;
}

/* enable or disable a pinmux function */
static void samsung_pinmux_setup(struct pinctrl_dev *pctldev, unsigned selector,
					unsigned group, bool enable)
{
	struct samsung_pinctrl_drv_data *drvdata;
	struct samsung_pin_bank_type *type;
	struct samsung_pin_bank *bank;
	void __iomem *reg;
	u32 mask, shift, data, pin_offset;
	unsigned long flags;
	const struct samsung_pmx_func *func;
	const struct samsung_pin_group *grp;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	func = &drvdata->pmx_functions[selector];
	grp = &drvdata->pin_groups[group];

	pin_to_reg_bank(drvdata, grp->pins[0] - drvdata->ctrl->base,
			&reg, &pin_offset, &bank);
	type = bank->type;
	mask = (1 << type->fld_width[PINCFG_TYPE_FUNC]) - 1;
	shift = pin_offset * type->fld_width[PINCFG_TYPE_FUNC];
	if (shift >= 32) {
		/* Some banks have two config registers */
		shift -= 32;
		reg += 4;
	}

	spin_lock_irqsave(&bank->slock, flags);

	data = readl(reg + type->reg_offset[PINCFG_TYPE_FUNC]);
	data &= ~(mask << shift);
	if (enable)
		data |= func->val << shift;
	writel(data, reg + type->reg_offset[PINCFG_TYPE_FUNC]);

	/*
	 * The pin could be FUNC mode or INPUT mode. Therefore, its
	 * DAT register has to be masked.
	 */
	bank->dat_mask &= ~(1 << pin_offset);
	data = readl(reg + type->reg_offset[PINCFG_TYPE_DAT]);
	data &= bank->dat_mask;
	writel(data, reg + type->reg_offset[PINCFG_TYPE_DAT]);

	spin_unlock_irqrestore(&bank->slock, flags);
}

/* enable a specified pinmux by writing to registers */
static int samsung_pinmux_enable(struct pinctrl_dev *pctldev, unsigned selector,
					unsigned group)
{
	samsung_pinmux_setup(pctldev, selector, group, true);
	return 0;
}

/* disable a specified pinmux by writing to registers */
static void samsung_pinmux_disable(struct pinctrl_dev *pctldev,
					unsigned selector, unsigned group)
{
	samsung_pinmux_setup(pctldev, selector, group, false);
}

/*
 * The calls to gpio_direction_output() and gpio_direction_input()
 * leads to this function call (via the pinctrl_gpio_direction_{input|output}()
 * function called from the gpiolib interface).
 */
static int samsung_pinmux_gpio_set_direction(struct pinctrl_dev *pctldev,
		struct pinctrl_gpio_range *range, unsigned offset, bool input)
{
	struct samsung_pin_bank_type *type;
	struct samsung_pin_bank *bank;
	struct samsung_pinctrl_drv_data *drvdata;
	void __iomem *reg;
	u32 data, pin_offset, mask, shift;
	unsigned long flags;

	bank = gc_to_pin_bank(range->gc);
	type = bank->type;
	drvdata = pinctrl_dev_get_drvdata(pctldev);

	pin_offset = offset - bank->pin_base;
	reg = drvdata->virt_base + bank->pctl_offset +
					type->reg_offset[PINCFG_TYPE_FUNC];

	mask = (1 << type->fld_width[PINCFG_TYPE_FUNC]) - 1;
	shift = pin_offset * type->fld_width[PINCFG_TYPE_FUNC];
	if (shift >= 32) {
		/* Some banks have two config registers */
		shift -= 32;
		reg += 4;
	}

	spin_lock_irqsave(&bank->slock, flags);

	data = readl(reg);
	data &= ~(mask << shift);
	if (!input) {
		data |= FUNC_OUTPUT << shift;
		bank->dat_mask |= 1 << pin_offset;
	} else {
		bank->dat_mask &= ~(1 << pin_offset);
	}
	writel(data, reg);

	spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

/* list of pinmux callbacks for the pinmux vertical in pinctrl core */
static const struct pinmux_ops samsung_pinmux_ops = {
	.get_functions_count	= samsung_get_functions_count,
	.get_function_name	= samsung_pinmux_get_fname,
	.get_function_groups	= samsung_pinmux_get_groups,
	.enable			= samsung_pinmux_enable,
	.disable		= samsung_pinmux_disable,
	.gpio_set_direction	= samsung_pinmux_gpio_set_direction,
};

/* set or get the pin config settings for a specified pin */
static int samsung_pinconf_rw(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long *config, bool set)
{
	struct samsung_pinctrl_drv_data *drvdata;
	struct samsung_pin_bank_type *type;
	struct samsung_pin_bank *bank;
	void __iomem *reg_base;
	enum pincfg_type cfg_type = PINCFG_UNPACK_TYPE(*config);
	u32 data, width, pin_offset, mask, shift;
	u32 cfg_value, cfg_reg;
	unsigned long flags;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pin_to_reg_bank(drvdata, pin - drvdata->ctrl->base, &reg_base,
					&pin_offset, &bank);
	type = bank->type;

	if (cfg_type >= PINCFG_TYPE_NUM || !type->fld_width[cfg_type])
		return -EINVAL;

	width = type->fld_width[cfg_type];
	cfg_reg = type->reg_offset[cfg_type];

	spin_lock_irqsave(&bank->slock, flags);

	mask = (1 << width) - 1;
	shift = pin_offset * width;
	data = readl(reg_base + cfg_reg);

	if (set) {
		cfg_value = PINCFG_UNPACK_VALUE(*config);
		data &= ~(mask << shift);
		data |= (cfg_value << shift);
		writel(data, reg_base + cfg_reg);
	} else {
		data >>= shift;
		data &= mask;
		*config = PINCFG_PACK(cfg_type, data);
	}

	spin_unlock_irqrestore(&bank->slock, flags);

	return 0;
}

/* set the pin config settings for a specified pin */
static int samsung_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
				unsigned long config)
{
	return samsung_pinconf_rw(pctldev, pin, &config, true);
}

/* get the pin config settings for a specified pin */
static int samsung_pinconf_get(struct pinctrl_dev *pctldev, unsigned int pin,
					unsigned long *config)
{
	return samsung_pinconf_rw(pctldev, pin, config, false);
}

/* set the pin config settings for a specified pin group */
static int samsung_pinconf_group_set(struct pinctrl_dev *pctldev,
			unsigned group, unsigned long config)
{
	struct samsung_pinctrl_drv_data *drvdata;
	const unsigned int *pins;
	unsigned int cnt;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pins = drvdata->pin_groups[group].pins;

	for (cnt = 0; cnt < drvdata->pin_groups[group].num_pins; cnt++)
		samsung_pinconf_set(pctldev, pins[cnt], config);

	return 0;
}

/* get the pin config settings for a specified pin group */
static int samsung_pinconf_group_get(struct pinctrl_dev *pctldev,
				unsigned int group, unsigned long *config)
{
	struct samsung_pinctrl_drv_data *drvdata;
	const unsigned int *pins;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pins = drvdata->pin_groups[group].pins;
	samsung_pinconf_get(pctldev, pins[0], config);
	return 0;
}

/* show whole PUD, DRV, CON_PDN and PUD_PDN register status */
static void samsung_pinconf_dbg_show(struct pinctrl_dev *pctldev,
				struct seq_file *s, unsigned pin)
{
	struct samsung_pinctrl_drv_data *drvdata;
	struct samsung_pin_bank *bank;
	void __iomem *reg_base;
	u32 pin_offset;
	unsigned long flags;
	enum pincfg_type cfg_type;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pin_to_reg_bank(drvdata, pin - drvdata->ctrl->base, &reg_base,
					&pin_offset, &bank);

	spin_lock_irqsave(&bank->slock, flags);

	for (cfg_type = PINCFG_TYPE_PUD; cfg_type <= PINCFG_TYPE_PUD_PDN
					; cfg_type++) {
		samsung_pin_dbg_show_by_type(bank, reg_base,
					pin_offset, s, pin, cfg_type);
	}

	spin_unlock_irqrestore(&bank->slock, flags);
}

/* show group's PUD, DRV, CON_PDN and PUD_PDN register status */
static void samsung_pinconf_group_dbg_show(struct pinctrl_dev *pctldev,
				struct seq_file *s, unsigned group)
{
	struct samsung_pinctrl_drv_data *drvdata;
	const unsigned int *pins;
	int i;

	drvdata = pinctrl_dev_get_drvdata(pctldev);
	pins = drvdata->pin_groups[group].pins;

	for (i = 0; i < drvdata->pin_groups[group].num_pins; i++) {
		seq_printf(s, "\n\t%s:", pin_get_name(pctldev, pins[i]));
		samsung_pinconf_dbg_show(pctldev, s, pins[i]);
	}
}

/* list of pinconfig callbacks for pinconfig vertical in the pinctrl code */
static const struct pinconf_ops samsung_pinconf_ops = {
	.pin_config_get		= samsung_pinconf_get,
	.pin_config_set		= samsung_pinconf_set,
	.pin_config_group_get	= samsung_pinconf_group_get,
	.pin_config_group_set	= samsung_pinconf_group_set,
	.pin_config_dbg_show	= samsung_pinconf_dbg_show,
	.pin_config_group_dbg_show = samsung_pinconf_group_dbg_show,
};

/* gpiolib gpio_set callback function */
static void samsung_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
	struct samsung_pin_bank *bank = gc_to_pin_bank(gc);
	struct samsung_pin_bank_type *type = bank->type;
	unsigned long flags;
	void __iomem *reg;
	u32 data;

	reg = bank->drvdata->virt_base + bank->pctl_offset;

	spin_lock_irqsave(&bank->slock, flags);

	data = readl(reg + type->reg_offset[PINCFG_TYPE_DAT]);
	data &= ~(1 << offset);
	if (value)
		data |= 1 << offset;
	data &= bank->dat_mask;
	writel(data, reg + type->reg_offset[PINCFG_TYPE_DAT]);

	spin_unlock_irqrestore(&bank->slock, flags);
}

/* gpiolib gpio_get callback function */
static int samsung_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	void __iomem *reg;
	u32 data;
	struct samsung_pin_bank *bank = gc_to_pin_bank(gc);
	struct samsung_pin_bank_type *type = bank->type;

	reg = bank->drvdata->virt_base + bank->pctl_offset;

	data = readl(reg + type->reg_offset[PINCFG_TYPE_DAT]);
	data >>= offset;
	data &= 1;
	return data;
}

/*
 * gpiolib gpio_direction_input callback function. The setting of the pin
 * mux function as 'gpio input' will be handled by the pinctrl susbsystem
 * interface.
 */
static int samsung_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	return pinctrl_gpio_direction_input(gc->base + offset);
}

/*
 * gpiolib gpio_direction_output callback function. The setting of the pin
 * mux function as 'gpio output' will be handled by the pinctrl susbsystem
 * interface.
 */
static int samsung_gpio_direction_output(struct gpio_chip *gc, unsigned offset,
							int value)
{
	struct samsung_pin_bank *bank = gc_to_pin_bank(gc);

	bank->dat_mask |= 1 << offset;

	samsung_gpio_set(gc, offset, value);
	return pinctrl_gpio_direction_output(gc->base + offset);
}

/*
 * gpiolib gpio_to_irq callback function. Creates a mapping between a GPIO pin
 * and a virtual IRQ, if not already present.
 */
static int samsung_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct samsung_pin_bank *bank = gc_to_pin_bank(gc);
	unsigned int virq;

	if (!bank->irq_domain)
		return -ENXIO;

	virq = irq_create_mapping(bank->irq_domain, offset);

	return (virq) ? : -ENXIO;
}

static struct samsung_pin_group *samsung_pinctrl_create_groups(
				struct device *dev,
				struct samsung_pinctrl_drv_data *drvdata,
				unsigned int *cnt)
{
	struct pinctrl_desc *ctrldesc = &drvdata->pctl;
	struct samsung_pin_group *groups, *grp;
	const struct pinctrl_pin_desc *pdesc;
	int i;

	groups = devm_kzalloc(dev, ctrldesc->npins * sizeof(*groups),
				GFP_KERNEL);
	if (!groups) {
		dev_err(dev, "failed allocate memory for ping group list\n");
		return ERR_PTR(-EINVAL);
	}
	grp = groups;

	pdesc = ctrldesc->pins;
	for (i = 0; i < ctrldesc->npins; ++i, ++pdesc, ++grp) {
		grp->name = pdesc->name;
		grp->pins = &pdesc->number;
		grp->num_pins = 1;
	}

	*cnt = ctrldesc->npins;
	return groups;
}

static int samsung_pinctrl_create_function(struct device *dev,
				struct samsung_pinctrl_drv_data *drvdata,
				struct device_node *func_np,
				struct samsung_pmx_func *func)
{
	int npins;
	int ret;
	char *fname;
	int i;

	if (of_property_read_u32(func_np, "samsung,pin-function", &func->val))
		return 0;

	npins = of_property_count_strings(func_np, "samsung,pins");
	if (npins < 1) {
		dev_err(dev, "invalid pin list in %s node", func_np->name);
		return -EINVAL;
	}

	/* derive function name from the node name */
	fname = devm_kzalloc(dev, strlen(func_np->name) + 1, GFP_KERNEL);
	if (!fname) {
		dev_err(dev, "failed to alloc memory for func name\n");
		return -ENOMEM;
	}

	strlcpy(fname, func_np->full_name, strlen(func_np->full_name) + 1);
	func->name = fname;

	func->groups = devm_kzalloc(dev, npins * sizeof(char *), GFP_KERNEL);
	if (!func->groups) {
		dev_err(dev, "failed to alloc memory for group list "
				"in pin function");
		return -ENOMEM;
	}

	for (i = 0; i < npins; ++i) {
		const char *gname;
		char *gname_copy;

		ret = of_property_read_string_index(func_np, "samsung,pins",
							i, &gname);
		if (ret) {
			dev_err(dev,
				"failed to read pin name %d from %s node\n",
				i, func_np->name);
			return ret;
		}

		gname_copy = devm_kzalloc(dev, strlen(gname) + 1, GFP_KERNEL);
		if (!gname_copy)
			return -ENOMEM;
		strlcpy(gname_copy, gname, strlen(gname) + 1);

		func->groups[i] = gname_copy;

	}

	func->num_groups = npins;
	return 1;
}

static struct samsung_pmx_func *samsung_pinctrl_create_functions(
				struct device *dev,
				struct samsung_pinctrl_drv_data *drvdata,
				unsigned int *cnt)
{
	struct samsung_pmx_func *functions, *func;
	struct device_node *dev_np = dev->of_node;
	struct device_node *cfg_np;
	unsigned int func_cnt = 0;
	int ret;

	/*
	 * Iterate over all the child nodes of the pin controller node
	 * and create pin groups and pin function lists.
	 */
	for_each_child_of_node(dev_np, cfg_np) {
		struct device_node *func_np;
		if (!of_get_child_count(cfg_np)) {
			if (!of_find_property(cfg_np,
			    "samsung,pin-function", NULL))
				continue;
			++func_cnt;
			continue;
		}

		for_each_child_of_node(cfg_np, func_np) {
			if (!of_find_property(func_np,
			    "samsung,pin-function", NULL))
				continue;
			++func_cnt;
		}
	}

	functions = devm_kzalloc(dev, func_cnt * sizeof(*functions),
					GFP_KERNEL);
	if (!functions) {
		dev_err(dev, "failed to allocate memory for function list\n");
		return ERR_PTR(-EINVAL);
	}
	func = functions;

	/*
	 * Iterate over all the child nodes of the pin controller node
	 * and create pin groups and pin function lists.
	 */
	func_cnt = 0;
	for_each_child_of_node(dev_np, cfg_np) {
		struct device_node *func_np;

		if (!of_get_child_count(cfg_np)) {
			ret = samsung_pinctrl_create_function(dev, drvdata,
							cfg_np, func);
			if (ret < 0)
				return ERR_PTR(ret);
			if (ret > 0) {
				++func;
				++func_cnt;
			}
			continue;
		}

		for_each_child_of_node(cfg_np, func_np) {
			ret = samsung_pinctrl_create_function(dev, drvdata,
						func_np, func);
			if (ret < 0)
				return ERR_PTR(ret);
			if (ret > 0) {
				++func;
				++func_cnt;
			}
		}
	}

	*cnt = func_cnt;
	return functions;
}

/*
 * Parse the information about all the available pin groups and pin functions
 * from device node of the pin-controller. A pin group is formed with all
 * the pins listed in the "samsung,pins" property.
 */

static int samsung_pinctrl_parse_dt(struct platform_device *pdev,
				    struct samsung_pinctrl_drv_data *drvdata)
{
	struct device *dev = &pdev->dev;
	struct samsung_pin_group *groups;
	struct samsung_pmx_func *functions;
	unsigned int grp_cnt = 0, func_cnt = 0;

	groups = samsung_pinctrl_create_groups(dev, drvdata, &grp_cnt);
	if (IS_ERR(groups)) {
		dev_err(dev, "failed to parse pin groups\n");
		return PTR_ERR(groups);
	}

	functions = samsung_pinctrl_create_functions(dev, drvdata, &func_cnt);
	if (IS_ERR(functions)) {
		dev_err(dev, "failed to parse pin functions\n");
		return PTR_ERR(groups);
	}

	drvdata->pin_groups = groups;
	drvdata->nr_groups = grp_cnt;
	drvdata->pmx_functions = functions;
	drvdata->nr_functions = func_cnt;

	return 0;
}

/* set dat register's mask to avoid func mode pin */
static void samsung_set_dat_mask(struct samsung_pin_bank *bank, const u32 data)
{
	u32 cnt, func, mask, width;

	width = bank->type->fld_width[PINCFG_TYPE_FUNC];
	mask = (1 << width) - 1;

	bank->dat_mask = 0;
	for (cnt = 0; cnt < 8; cnt++) {
		func = (data >> (width * cnt)) & mask;
		if (func == FUNC_OUTPUT)
			bank->dat_mask |= 1 << cnt;
	}
}

/* register the pinctrl interface with the pinctrl subsystem */
static int samsung_pinctrl_register(struct platform_device *pdev,
				    struct samsung_pinctrl_drv_data *drvdata)
{
	struct pinctrl_desc *ctrldesc = &drvdata->pctl;
	struct pinctrl_pin_desc *pindesc, *pdesc;
	struct samsung_pin_bank *pin_bank;
	char *pin_names;
	int pin, bank, ret;

	ctrldesc->name = "samsung-pinctrl";
	ctrldesc->owner = THIS_MODULE;
	ctrldesc->pctlops = &samsung_pctrl_ops;
	ctrldesc->pmxops = &samsung_pinmux_ops;
	ctrldesc->confops = &samsung_pinconf_ops;

	pindesc = devm_kzalloc(&pdev->dev, sizeof(*pindesc) *
			drvdata->ctrl->nr_pins, GFP_KERNEL);
	if (!pindesc) {
		dev_err(&pdev->dev, "mem alloc for pin descriptors failed\n");
		return -ENOMEM;
	}
	ctrldesc->pins = pindesc;
	ctrldesc->npins = drvdata->ctrl->nr_pins;

	/* dynamically populate the pin number and pin name for pindesc */
	for (pin = 0, pdesc = pindesc; pin < ctrldesc->npins; pin++, pdesc++)
		pdesc->number = pin + drvdata->ctrl->base;

	/*
	 * allocate space for storing the dynamically generated names for all
	 * the pins which belong to this pin-controller.
	 */
	pin_names = devm_kzalloc(&pdev->dev, sizeof(char) * PIN_NAME_LENGTH *
					drvdata->ctrl->nr_pins, GFP_KERNEL);
	if (!pin_names) {
		dev_err(&pdev->dev, "mem alloc for pin names failed\n");
		return -ENOMEM;
	}

	/* for each pin, the name of the pin is pin-bank name + pin number */
	for (bank = 0; bank < drvdata->ctrl->nr_banks; bank++) {
		pin_bank = &drvdata->ctrl->pin_banks[bank];
		for (pin = 0; pin < pin_bank->nr_pins; pin++) {
			sprintf(pin_names, "%s-%d", pin_bank->name, pin);
			pdesc = pindesc + pin_bank->pin_base + pin;
			pdesc->name = pin_names;
			pin_names += PIN_NAME_LENGTH;
		}
	}

	ret = samsung_pinctrl_parse_dt(pdev, drvdata);
	if (ret) {
		pinctrl_unregister(drvdata->pctl_dev);
		return ret;
	}

	drvdata->pctl_dev = pinctrl_register(ctrldesc, &pdev->dev, drvdata);
	if (!drvdata->pctl_dev) {
		dev_err(&pdev->dev, "could not register pinctrl driver\n");
		return -EINVAL;
	}

	for (bank = 0; bank < drvdata->ctrl->nr_banks; ++bank) {
		void __iomem 	*reg;
		unsigned long 	flags;
		u32 		data;

		pin_bank = &drvdata->ctrl->pin_banks[bank];
		pin_bank->grange.name = pin_bank->name;
		pin_bank->grange.id = bank;
		pin_bank->grange.pin_base = pin_bank->pin_base;
		pin_bank->grange.base = pin_bank->gpio_chip.base;
		pin_bank->grange.npins = pin_bank->gpio_chip.ngpio;
		pin_bank->grange.gc = &pin_bank->gpio_chip;
		pinctrl_add_gpio_range(drvdata->pctl_dev, &pin_bank->grange);

		reg = drvdata->virt_base + pin_bank->pctl_offset +
				pin_bank->type->reg_offset[PINCFG_TYPE_FUNC];

		spin_lock_irqsave(&pin_bank->slock, flags);

		/* set initial dat_mask */
		data = readl(reg);
		samsung_set_dat_mask(pin_bank, data);

		spin_unlock_irqrestore(&pin_bank->slock, flags);
	}

	return 0;
}

static const struct gpio_chip samsung_gpiolib_chip = {
	.set = samsung_gpio_set,
	.get = samsung_gpio_get,
	.direction_input = samsung_gpio_direction_input,
	.direction_output = samsung_gpio_direction_output,
	.to_irq = samsung_gpio_to_irq,
	.owner = THIS_MODULE,
};

/* register the gpiolib interface with the gpiolib subsystem */
static int samsung_gpiolib_register(struct platform_device *pdev,
				    struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	struct gpio_chip *gc;
	int ret;
	int i;

	for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
		bank->gpio_chip = samsung_gpiolib_chip;

		gc = &bank->gpio_chip;
		gc->base = ctrl->base + bank->pin_base;
		gc->ngpio = bank->nr_pins;
		gc->dev = &pdev->dev;
		gc->of_node = bank->of_node;
		gc->label = bank->name;

		ret = gpiochip_add(gc);
		if (ret) {
			dev_err(&pdev->dev, "failed to register gpio_chip %s, error code: %d\n",
							gc->label, ret);
			goto fail;
		}
	}

	return 0;

fail:
	for (--i, --bank; i >= 0; --i, --bank)
		if (gpiochip_remove(&bank->gpio_chip))
			dev_err(&pdev->dev, "gpio chip %s remove failed\n",
							bank->gpio_chip.label);
	return ret;
}

/* unregister the gpiolib interface with the gpiolib subsystem */
static int samsung_gpiolib_unregister(struct platform_device *pdev,
				      struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	struct samsung_pin_bank *bank = ctrl->pin_banks;
	int ret = 0;
	int i;

	for (i = 0; !ret && i < ctrl->nr_banks; ++i, ++bank)
		ret = gpiochip_remove(&bank->gpio_chip);

	if (ret)
		dev_err(&pdev->dev, "gpio chip remove failed\n");

	return ret;
}

static const struct of_device_id samsung_pinctrl_dt_match[];

/* retrieve the soc specific data */
static struct samsung_pin_ctrl *samsung_pinctrl_get_soc_data(
				struct samsung_pinctrl_drv_data *d,
				struct platform_device *pdev)
{
	int id;
	const struct of_device_id *match;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *np;
	struct samsung_pin_ctrl *ctrl;
	struct samsung_pin_bank *bank;
	int i;

	id = of_alias_get_id(node, "pinctrl");
	if (id < 0) {
		dev_err(&pdev->dev, "failed to get alias id\n");
		return NULL;
	}
	match = of_match_node(samsung_pinctrl_dt_match, node);
	ctrl = (struct samsung_pin_ctrl *)match->data + id;

	bank = ctrl->pin_banks;
	for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
		spin_lock_init(&bank->slock);
		bank->drvdata = d;
		bank->pin_base = ctrl->nr_pins;
		ctrl->nr_pins += bank->nr_pins;
	}

	for_each_child_of_node(node, np) {
		if (!of_find_property(np, "gpio-controller", NULL))
			continue;
		bank = ctrl->pin_banks;
		for (i = 0; i < ctrl->nr_banks; ++i, ++bank) {
			if (!strcmp(bank->name, np->name)) {
				bank->of_node = np;
				break;
			}
		}
	}

	ctrl->base = pin_base;
	pin_base += ctrl->nr_pins;

	return ctrl;
}

static int samsung_pinctrl_probe(struct platform_device *pdev)
{
	struct samsung_pinctrl_drv_data *drvdata;
	struct device *dev = &pdev->dev;
	struct samsung_pin_ctrl *ctrl;
	struct resource *res;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "device tree node not found\n");
		return -ENODEV;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(dev, "failed to allocate memory for driver's "
				"private data\n");
		return -ENOMEM;
	}

	ctrl = samsung_pinctrl_get_soc_data(drvdata, pdev);
	if (!ctrl) {
		dev_err(&pdev->dev, "driver data not available\n");
		return -EINVAL;
	}
	drvdata->ctrl = ctrl;
	drvdata->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	drvdata->virt_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(drvdata->virt_base))
		return PTR_ERR(drvdata->virt_base);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res)
		drvdata->irq = res->start;

	ret = samsung_gpiolib_register(pdev, drvdata);
	if (ret)
		return ret;

	ret = samsung_pinctrl_register(pdev, drvdata);
	if (ret) {
		samsung_gpiolib_unregister(pdev, drvdata);
		return ret;
	}

	ctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ctrl->pinctrl)) {
		dev_err(dev, "could not get pinctrl\n");
		return PTR_ERR(ctrl->pinctrl);
	}

	ctrl->pins_default = pinctrl_lookup_state(ctrl->pinctrl,
						 PINCTRL_STATE_DEFAULT);
	if (IS_ERR(ctrl->pins_default))
		dev_dbg(dev, "could not get default pinstate\n");

	ctrl->pins_sleep = pinctrl_lookup_state(ctrl->pinctrl,
					      PINCTRL_STATE_SLEEP);
	if (IS_ERR(ctrl->pins_sleep))
		dev_dbg(dev, "could not get sleep pinstate\n");

	if (ctrl->eint_gpio_init)
		ctrl->eint_gpio_init(drvdata);
	if (ctrl->eint_wkup_init)
		ctrl->eint_wkup_init(drvdata);

	platform_set_drvdata(pdev, drvdata);

	/* Add to the global list */
	list_add_tail(&drvdata->node, &drvdata_list);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_CPU_IDLE)
/* save gpio registers */
static void samsung_pinctrl_save_regs(
				struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	void __iomem *virt_base = drvdata->virt_base;
	int i;

	for (i = 0; i < ctrl->nr_banks; i++) {
		struct samsung_pin_bank *bank = &ctrl->pin_banks[i];
		void __iomem *reg = virt_base + bank->pctl_offset;

		u8 *offs = bank->type->reg_offset;
		u8 *widths = bank->type->fld_width;
		enum pincfg_type type;

		/* Registers without a powerdown config aren't lost */
		if (!widths[PINCFG_TYPE_CON_PDN])
			continue;

		for (type = 0; type < PINCFG_TYPE_NUM; type++)
			if (widths[type])
				bank->pm_save[type] = readl(reg + offs[type]);

		if (widths[PINCFG_TYPE_FUNC] * bank->nr_pins > 32) {
			/* Some banks have two config registers */
			bank->pm_save[PINCFG_TYPE_NUM] =
				readl(reg + offs[PINCFG_TYPE_FUNC] + 4);
			pr_debug("Save %s @ %p (con %#010x %08x)\n",
				 bank->name, reg,
				 bank->pm_save[PINCFG_TYPE_FUNC],
				 bank->pm_save[PINCFG_TYPE_NUM]);
		} else {
			pr_debug("Save %s @ %p (con %#010x)\n", bank->name,
				 reg, bank->pm_save[PINCFG_TYPE_FUNC]);
		}
	}
}

/* restore gpio registers */
static void samsung_pinctrl_restore_regs(
				struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	void __iomem *virt_base = drvdata->virt_base;
	int i;

	for (i = 0; i < ctrl->nr_banks; i++) {
		struct samsung_pin_bank *bank = &ctrl->pin_banks[i];
		void __iomem *reg = virt_base + bank->pctl_offset;

		u8 *offs = bank->type->reg_offset;
		u8 *widths = bank->type->fld_width;
		enum pincfg_type type;

		/* Registers without a powerdown config aren't lost */
		if (!widths[PINCFG_TYPE_CON_PDN])
			continue;

		if (widths[PINCFG_TYPE_FUNC] * bank->nr_pins > 32) {
			/* Some banks have two config registers */
			pr_debug("%s @ %p (con %#010x %08x => %#010x %08x)\n",
				 bank->name, reg,
				 readl(reg + offs[PINCFG_TYPE_FUNC]),
				 readl(reg + offs[PINCFG_TYPE_FUNC] + 4),
				 bank->pm_save[PINCFG_TYPE_FUNC],
				 bank->pm_save[PINCFG_TYPE_NUM]);
			writel(bank->pm_save[PINCFG_TYPE_NUM],
			       reg + offs[PINCFG_TYPE_FUNC] + 4);
		} else {
			pr_debug("%s @ %p (con %#010x => %#010x)\n", bank->name,
				 reg, readl(reg + offs[PINCFG_TYPE_FUNC]),
				 bank->pm_save[PINCFG_TYPE_FUNC]);
		}
		writel(bank->pm_save[PINCFG_TYPE_FUNC], reg + offs[PINCFG_TYPE_FUNC]);
		writel(bank->pm_save[PINCFG_TYPE_DAT] & bank->dat_mask,
				reg + offs[PINCFG_TYPE_DAT]);
		for (type = PINCFG_TYPE_PUD; type < PINCFG_TYPE_NUM; type++)
			if (widths[type])
				writel(bank->pm_save[type], reg + offs[type]);
	}
}

#endif /* defined(CONFIG_PM) || defined(CONFIG_CPU_IDLE) */

#ifdef CONFIG_CPU_IDLE

/* set PDN registers */
static void samsung_pinctrl_set_pdn_previos_state(
			struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	void __iomem *virt_base = drvdata->virt_base;
	int i;

	for (i = 0; i < ctrl->nr_banks; i++) {
		struct samsung_pin_bank *bank = &ctrl->pin_banks[i];
		void __iomem *reg = virt_base + bank->pctl_offset;

		u8 *offs = bank->type->reg_offset;
		u8 *widths = bank->type->fld_width;

		if (!widths[PINCFG_TYPE_CON_PDN])
			continue;

		/* set previous state */
		writel(0xffffffff, reg + offs[PINCFG_TYPE_CON_PDN]);
		writel(bank->pm_save[PINCFG_TYPE_PUD],
				reg + offs[PINCFG_TYPE_PUD_PDN]);
	}
}

/* notifier function for LPA mode */
static int samsung_pinctrl_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	struct samsung_pinctrl_drv_data *drvdata;

	switch (cmd) {
	case LPA_ENTER:
		list_for_each_entry(drvdata, &drvdata_list, node) {
			struct samsung_pin_ctrl *ctrl = drvdata->ctrl;

			if (!ctrl->suspend)
				continue;

			samsung_pinctrl_save_regs(drvdata);
			samsung_pinctrl_set_pdn_previos_state(drvdata);
		}
		break;
	case LPA_EXIT:
		list_for_each_entry(drvdata, &drvdata_list, node) {
			struct samsung_pin_ctrl *ctrl = drvdata->ctrl;

			if (!ctrl->resume)
				continue;

			samsung_pinctrl_restore_regs(drvdata);
		}
		break;
	}

	return NOTIFY_OK;
}

#endif /* CONFIG_CPU_IDLE */

#ifdef CONFIG_PM

/**
 * samsung_pinctrl_suspend_dev - save pinctrl state for suspend for a device
 *
 * Save data for all banks handled by this device.
 */
static void samsung_pinctrl_suspend_dev(
	struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;
	int ret;

	if (!ctrl->suspend)
		return;

	samsung_pinctrl_save_regs(drvdata);
	ctrl->suspend(drvdata);

	if (!IS_ERR(ctrl->pins_sleep)) {
		/* This is ignore to disable mux configuration. */
		ctrl->pinctrl->state = NULL;

		ret = pinctrl_select_state(ctrl->pinctrl, ctrl->pins_sleep);
		if (ret)
			dev_err(drvdata->dev, "could not set default pinstate\n");
	}

}

/**
 * samsung_pinctrl_resume_dev - restore pinctrl state from suspend for a device
 *
 * Restore one of the banks that was saved during suspend.
 *
 * We don't bother doing anything complicated to avoid glitching lines since
 * we're called before pad retention is turned off.
 */
static void samsung_pinctrl_resume_dev(struct samsung_pinctrl_drv_data *drvdata)
{
	struct samsung_pin_ctrl *ctrl = drvdata->ctrl;

	if (!ctrl->resume)
		return;

	ctrl->resume(drvdata);
	samsung_pinctrl_restore_regs(drvdata);

	/* For changing state without writing register. */
	ctrl->pinctrl->state = ctrl->pins_default;
}

/**
 * samsung_pinctrl_suspend - save pinctrl state for suspend
 *
 * Save data for all banks across all devices.
 */
static int samsung_pinctrl_suspend(void)
{
	struct samsung_pinctrl_drv_data *drvdata;

	list_for_each_entry(drvdata, &drvdata_list, node) {
		samsung_pinctrl_suspend_dev(drvdata);
	}

	return 0;
}

/**
 * samsung_pinctrl_resume - restore pinctrl state for suspend
 *
 * Restore data for all banks across all devices.
 */
static void samsung_pinctrl_resume(void)
{
	struct samsung_pinctrl_drv_data *drvdata;

	list_for_each_entry_reverse(drvdata, &drvdata_list, node) {
		samsung_pinctrl_resume_dev(drvdata);
	}
}

#else
#define samsung_pinctrl_suspend		NULL
#define samsung_pinctrl_resume		NULL
#endif

static struct syscore_ops samsung_pinctrl_syscore_ops = {
	.suspend	= samsung_pinctrl_suspend,
	.resume		= samsung_pinctrl_resume,
};

static const struct of_device_id samsung_pinctrl_dt_match[] = {
#ifdef CONFIG_PINCTRL_EXYNOS
	{ .compatible = "samsung,exynos4210-pinctrl",
		.data = (void *)exynos4210_pin_ctrl },
	{ .compatible = "samsung,exynos4x12-pinctrl",
		.data = (void *)exynos4x12_pin_ctrl },
	{ .compatible = "samsung,exynos5250-pinctrl",
		.data = (void *)exynos5250_pin_ctrl },
	{ .compatible = "samsung,exynos5422-pinctrl",
		.data = (void *)exynos5422_pin_ctrl },
	{ .compatible = "samsung,exynos5430-evt0-pinctrl",
		.data = (void *)exynos5430_evt0_pin_ctrl },
	{ .compatible = "samsung,exynos5430-pinctrl",
		.data = (void *)exynos5430_pin_ctrl },
#endif
#ifdef CONFIG_PINCTRL_S3C64XX
	{ .compatible = "samsung,s3c64xx-pinctrl",
		.data = s3c64xx_pin_ctrl },
#endif
	{},
};
MODULE_DEVICE_TABLE(of, samsung_pinctrl_dt_match);

static struct platform_driver samsung_pinctrl_driver = {
	.probe		= samsung_pinctrl_probe,
	.driver = {
		.name	= "samsung-pinctrl",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_pinctrl_dt_match),
	},
};

#ifdef CONFIG_CPU_IDLE
static struct notifier_block samsung_pinctrl_notifier_block = {
	.notifier_call = samsung_pinctrl_notifier,
	.priority = 1,
};
#endif /*CONFIG_CPU_IDLE */

static int __init samsung_pinctrl_drv_register(void)
{
	/*
	 * Register syscore ops for save/restore of registers across suspend.
	 * It's important to ensure that this driver is running at an earlier
	 * initcall level than any arch-specific init calls that install syscore
	 * ops that turn off pad retention (like exynos_pm_resume).
	 */
	register_syscore_ops(&samsung_pinctrl_syscore_ops);
#ifdef CONFIG_CPU_IDLE
	exynos_pm_register_notifier(&samsung_pinctrl_notifier_block);
#endif

	return platform_driver_register(&samsung_pinctrl_driver);
}
postcore_initcall(samsung_pinctrl_drv_register);

static void __exit samsung_pinctrl_drv_unregister(void)
{
	platform_driver_unregister(&samsung_pinctrl_driver);
}
module_exit(samsung_pinctrl_drv_unregister);

MODULE_AUTHOR("Thomas Abraham <thomas.ab@samsung.com>");
MODULE_DESCRIPTION("Samsung pinctrl driver");
MODULE_LICENSE("GPL v2");
