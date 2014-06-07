/*
 * MIPI-LLI driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/gpio.h>
#include <linux/vmalloc.h>
#include <linux/mipi-lli.h>

/* IPC_MEMSIZE should be less than 4MB */
#define IPC_MEMSIZE	(4 * SZ_1M)

static struct mipi_lli *g_lli;

/**
 * mipi_lli_request_sh_region
 * @sh_addr: shared physical address
 * @size: allocation region size
 *
 * Returns a pointer to the allocated shared memory buffer address.
 */
void __iomem *mipi_lli_request_sh_region(u32 sh_addr, u32 size)
{
	if (!g_lli)
		return NULL;
	return (void __iomem *)g_lli->shdmem_addr;
}
EXPORT_SYMBOL(mipi_lli_request_sh_region);

/**
 * mipi_lli_release_sh_region
 * @rgn: request memory region
 */
void mipi_lli_release_sh_region(void *rgn)
{
	return;
}
EXPORT_SYMBOL(mipi_lli_release_sh_region);

/**
 * mipi_lli_get_phys_base
 *
 * Returns physical base address.
 */
unsigned long mipi_lli_get_phys_base(void)
{
	return g_lli->phy_addr;
}
EXPORT_SYMBOL(mipi_lli_get_phys_base);

/**
 * mipi_lli_release_sh_region
 *
 * Returns phys address size.
 */
unsigned long mipi_lli_get_phys_size(void)
{
	return g_lli->shdmem_size;
}
EXPORT_SYMBOL(mipi_lli_get_phys_size);

/**
 * mipi_lli_get_link_status
 *
 * Returns mipi_lli_link_status.
 */
int mipi_lli_get_link_status(void)
{
	return g_lli->state;
}
EXPORT_SYMBOL(mipi_lli_get_link_status);

/**
 * mipi_lli_register_handler
 * @handler: callback function when signal interrupt is occured.
 * @data: parameter when handler is callbacked.
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
int mipi_lli_register_handler(void (*handler)(void *, u32), void *data)
{
	if (!handler)
		return -EINVAL;

	/* Register interrupt handler */
	g_lli->hd.data = data;
	g_lli->hd.handler = handler;

	return 0;
}
EXPORT_SYMBOL(mipi_lli_register_handler);

/**
 * mipi_lli_unregister_handler
 * @handler: callback function when signal interrupt is occured.
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
int mipi_lli_unregister_handler(void (*handler)(void *, u32))
{
	if (!handler || (g_lli->hd.handler != handler))
		return -EINVAL;

	/* Unregister interrupt handler */
	g_lli->hd.data = NULL;
	g_lli->hd.handler = NULL;

	return 0;
}
EXPORT_SYMBOL(mipi_lli_unregister_handler);

/**
 * mipi_lli_read_interrupt - Reload all resource for re-init
 * @cmd: Send interrupt command
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
void mipi_lli_send_interrupt(u32 cmd)
{
	if (!g_lli || !g_lli->driver || !g_lli->driver->read_signal)
		return;

	g_lli->driver->send_signal(g_lli, cmd);
}
EXPORT_SYMBOL(mipi_lli_send_interrupt);

/**
 * mipi_lli_reset_interrupt - Reload all resource for re-init
 * @cmd: Clear all interrupts
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
void mipi_lli_reset_interrupt(void)
{
	if (!g_lli || !g_lli->driver || !g_lli->driver->read_signal)
		return;

	g_lli->driver->reset_signal(g_lli);
}
EXPORT_SYMBOL(mipi_lli_reset_interrupt);

/**
 * mipi_lli_read_interrupt - Reload all resource for re-init
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
u32 mipi_lli_read_interrupt(void)
{
	if (!g_lli || !g_lli->driver || !g_lli->driver->read_signal)
		return 0;

	return g_lli->driver->read_signal(g_lli);
}
EXPORT_SYMBOL(mipi_lli_read_interrupt);

/**
 * mipi_lli_reload - Reload all resource for re-init
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
void mipi_lli_reload(void)
{
	if (!g_lli || !g_lli->driver || !g_lli->driver->init)
		return;

	g_lli->driver->init(g_lli);
}
EXPORT_SYMBOL(mipi_lli_reload);

static void mipi_lli_send_signal_test(struct mipi_lli *lli)
{
	u32 i;

	for (i = 0; i < 32; i++)
		lli->driver->send_signal(g_lli, (1 << i));
}

static ssize_t show_mipi_lli_control(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct mipi_lli *lli = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "MIPI-LLI %x\n",
			lli->driver->get_status(lli));
}

static ssize_t store_mipi_lli_control(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct mipi_lli *lli = dev_get_drvdata(dev);
	int command;

	if (sscanf(buf, "%d", &command) != 1)
		return -EINVAL;

	device_lock(dev);

	if (command == 1)
		lli->driver->init(lli);
	else if (command == 2)
		lli->driver->set_master(lli, true);
	else if (command == 3)
		lli->driver->link_startup_mount(lli);
	else if (command == 4)
		lli->driver->exit(lli);
	else if (command == 5)
		mipi_lli_send_signal_test(lli);
	else
		dev_err(dev, "Un-support control command\n");

	device_unlock(dev);

	return count;
}
static DEVICE_ATTR(lli_control, 0644,
	show_mipi_lli_control, store_mipi_lli_control);

static irqreturn_t mipi_lli_sig_irq(int irq, void *_dev)
{
	struct mipi_lli *lli = _dev;
	u32 intr = 0;

	if (lli->driver->read_signal)
		intr = lli->driver->read_signal(lli);

	if (g_lli->hd.handler)
		g_lli->hd.handler(g_lli->hd.data, intr);

	return IRQ_HANDLED;
}

/**
 * mipi_lli_suspend must call by modem_if.
 */
void mipi_lli_suspend(void)
{
	if (!g_lli || !g_lli->driver || !g_lli->driver->suspend)
		return;

	g_lli->driver->suspend(g_lli);
}
EXPORT_SYMBOL(mipi_lli_suspend);

/**
 * mipi_lli_resume must call by modem_if.
 */
void mipi_lli_resume(void)
{
	if (!g_lli || !g_lli->driver || !g_lli->driver->resume)
		return;

	g_lli->driver->resume(g_lli);
}

EXPORT_SYMBOL(mipi_lli_resume);

int mipi_lli_add_driver(struct device *dev,
			const struct lli_driver *lli_driver,
			int irq)
{
	struct mipi_lli *lli;
	int ret;

	if (g_lli)
		return 0;

	lli = devm_kzalloc(dev, sizeof(*lli), GFP_KERNEL);

	if (!lli)
		return -ENOMEM;

	lli->driver = lli_driver;
	lli->dev = dev;
	lli->irq_sig = irq;

	ret = request_irq(irq, mipi_lli_sig_irq, 0, dev_name(dev), lli);
	if (ret < 0)
		return ret;

	lli->shdmem_addr = dmam_alloc_coherent(dev,
					       IPC_MEMSIZE,
					       &lli->phy_addr,
					       GFP_KERNEL);
	if (!lli->shdmem_addr)
		return -ENOMEM;

	lli->shdmem_size = IPC_MEMSIZE;

	dev_info(dev, "alloc share IPC memory addr = %p[%x]\n",
		 lli->shdmem_addr,
		 lli->phy_addr);

	dev_set_drvdata(dev, lli);
	g_lli = lli;

	device_create_file(dev, &dev_attr_lli_control);

	return 0;
}
EXPORT_SYMBOL(mipi_lli_add_driver);

void mipi_lli_remove_driver(struct mipi_lli *lli)
{
	free_irq(lli->irq_sig, lli);
	g_lli = NULL;
}
EXPORT_SYMBOL(mipi_lli_remove_driver);

MODULE_DESCRIPTION("MIPI LLI driver");
MODULE_AUTHOR("Yulgon Kim <yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
