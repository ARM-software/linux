/*
 * driver for FIMC-IS SPI
 *
 * Copyright (c) 2011, Samsung Electronics. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include "fimc-is-core.h"
#include "fimc-is-regs.h"

#define STREAM_TO_U16(var16, p)	{(var16) = ((u16)(*((u8 *)p+1)) + \
				((u8)(*((u8 *)p) << 8))); }

static struct spi_device *g_spi;

int fimc_is_spi_reset(void *buf, u32 rx_addr, size_t size)
{
	unsigned char req_rst[1] = { 0x99 };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	t_c.tx_buf = req_rst;
	t_c.len = 1;
	t_c.cs_change = 0;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);

	ret = spi_sync(g_spi, &m);
	if (ret) {
		err("spi sync error - can't get device information");
		return -EIO;
	}

	return 0;
}

int fimc_is_spi_read(void *buf, u32 rx_addr, size_t size)
{
	unsigned char req_data[4] = { 0x03,  };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[1] = (rx_addr & 0xFF0000) >> 16;
	req_data[2] = (rx_addr & 0xFF00) >> 8;
	req_data[3] = (rx_addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = size;
	t_r.cs_change = 0;
	t_r.bits_per_word = 32;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	ret = spi_sync(g_spi, &m);
	if (ret) {
		err("spi sync error - can't read data");
		return -EIO;
	} else
		return 0;
}

static int __devinit fimc_is_spi_probe(struct spi_device *spi)
{
	int ret = 0;

	dbg_core("%s\n", __func__);

	/* spi->bits_per_word = 16; */
	if (spi_setup(spi)) {
		pr_err("failed to setup spi for fimc_is_spi\n");
		ret = -EINVAL;
		goto exit;
	}

	g_spi = spi;

exit:
	return ret;
}

static int __devexit fimc_is_spi_remove(struct spi_device *spi)
{
	return 0;
}

static
struct spi_driver fimc_is_spi_driver = {
	.driver = {
		.name = "fimc_is_spi",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},
	.probe = fimc_is_spi_probe,
	.remove = __devexit_p(fimc_is_spi_remove),
};

static
int __init fimc_is_spi_init(void)
{
	int ret;

	ret = spi_register_driver(&fimc_is_spi_driver);

	if (ret)
		pr_err("failed to register imc_is_spi- %x\n", ret);

	return ret;
}

static
void __exit fimc_is_spi_exit(void)
{
	spi_unregister_driver(&fimc_is_spi_driver);
}

module_init(fimc_is_spi_init);
module_exit(fimc_is_spi_exit);

MODULE_DESCRIPTION("FIMC-IS SPI driver");
MODULE_LICENSE("GPL");
