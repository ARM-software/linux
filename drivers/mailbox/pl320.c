/*
 * Copyright 2012 Calxeda, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/types.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/amba/bus.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>

#define IPCMSOURCE(m)		((m) * 0x40)
#define IPCMDSET(m)		(((m) * 0x40) + 0x004)
#define IPCMDCLEAR(m)		(((m) * 0x40) + 0x008)
#define IPCMDSTATUS(m)		(((m) * 0x40) + 0x00C)
#define IPCMMODE(m)		(((m) * 0x40) + 0x010)
#define IPCMMSET(m)		(((m) * 0x40) + 0x014)
#define IPCMMCLEAR(m)		(((m) * 0x40) + 0x018)
#define IPCMMSTATUS(m)		(((m) * 0x40) + 0x01C)
#define IPCMSEND(m)		(((m) * 0x40) + 0x020)
#define IPCMDR(m, dr)		(((m) * 0x40) + ((dr) * 4) + 0x024)

#define IPCMMIS(irq)		(((irq) * 8) + 0x800)
#define IPCMRIS(irq)		(((irq) * 8) + 0x804)

#define MBOX_MASK(n)		(1 << (n))
#define IPC_TX_MBOX		1

#define CHAN_MASK(n)		(1 << (n))
#define A9_SOURCE		1
#define M3_SOURCE		0

struct pl320_con {
	u32 *data;
	int mbox_irq;
	struct device *dev;
	struct mbox_link link;
	void __iomem *mbox_base;
	struct mbox_controller mbox_con;
};

static inline struct pl320_con *to_pl320(struct mbox_link *l)
{
	if (!l)
		return NULL;

	return container_of(l, struct pl320_con, link);
}

static irqreturn_t mbox_handler(int irq, void *p)
{
	struct mbox_link *link = (struct mbox_link *)p;
	struct pl320_con *pl320 = to_pl320(link);
	void __iomem *mbox_base = pl320->mbox_base;
	u32 irq_stat;

	irq_stat = __raw_readl(mbox_base + IPCMMIS(1));
	if (irq_stat & MBOX_MASK(IPC_TX_MBOX)) {
		u32 *data = pl320->data;
		int i;

		__raw_writel(0, mbox_base + IPCMSEND(IPC_TX_MBOX));

		/*
		 * The PL320 driver specifies that the send buffer
		 * will be overwritten by same fifo upon TX ACK.
		 */
		for (i = 0; i < 7; i++)
			data[i] = __raw_readl(mbox_base
					 + IPCMDR(IPC_TX_MBOX, i));

		mbox_link_txdone(link, MBOX_OK);

		pl320->data = NULL;
	}

	return IRQ_HANDLED;
}

static int pl320_send_data(struct mbox_link *link, void *msg)
{
	struct pl320_con *pl320 = to_pl320(link);
	void __iomem *mbox_base = pl320->mbox_base;
	u32 *data = (u32 *)msg;
	int i;

	pl320->data = data;

	for (i = 0; i < 7; i++)
		__raw_writel(data[i], mbox_base + IPCMDR(IPC_TX_MBOX, i));

	__raw_writel(0x1, mbox_base + IPCMSEND(IPC_TX_MBOX));

	return 0;
}

static int pl320_startup(struct mbox_link *link, void *ignored)
{
	struct pl320_con *pl320 = to_pl320(link);
	void __iomem *mbox_base = pl320->mbox_base;
	int err, mbox_irq = pl320->mbox_irq;

	__raw_writel(0, mbox_base + IPCMSEND(IPC_TX_MBOX));

	err = request_irq(mbox_irq, mbox_handler,
				0, dev_name(pl320->dev), link);
	if (err)
		return err;

	/* Init slow mailbox */
	__raw_writel(CHAN_MASK(A9_SOURCE),
			mbox_base + IPCMSOURCE(IPC_TX_MBOX));
	__raw_writel(CHAN_MASK(M3_SOURCE),
			mbox_base + IPCMDSET(IPC_TX_MBOX));
	__raw_writel(CHAN_MASK(M3_SOURCE) | CHAN_MASK(A9_SOURCE),
		     mbox_base + IPCMMSET(IPC_TX_MBOX));

	pl320->data = NULL;
	return 0;
}

static void pl320_shutdown(struct mbox_link *link)
{
	struct pl320_con *pl320 = to_pl320(link);

	pl320->data = NULL;
	free_irq(pl320->mbox_irq, link);
}

static struct mbox_link_ops pl320_ops = {
	.send_data = pl320_send_data,
	.startup = pl320_startup,
	.shutdown = pl320_shutdown,
};

static int pl320_probe(struct amba_device *adev, const struct amba_id *id)
{
	struct pl320_con *pl320;
	struct mbox_link *l[2];
	int ret;

	pl320 = kzalloc(sizeof(struct pl320_con), GFP_KERNEL);
	if (!pl320)
		return -ENOMEM;

	pl320->mbox_base = ioremap(adev->res.start, resource_size(&adev->res));
	if (pl320->mbox_base == NULL) {
		kfree(pl320);
		return -ENOMEM;
	}

	pl320->dev = &adev->dev;
	pl320->mbox_irq = adev->irq[0];
	amba_set_drvdata(adev, pl320);

	l[0] = &pl320->link;
	l[1] = NULL;
	pl320->mbox_con.links = l;
	pl320->mbox_con.txdone_irq = true;
	pl320->mbox_con.ops = &pl320_ops;
	snprintf(pl320->link.link_name, 16, "A9_to_M3");
	snprintf(pl320->mbox_con.controller_name, 16, "pl320");
	pl320->mbox_con.dev = &adev->dev;

	ret = mbox_controller_register(&pl320->mbox_con);
	if (ret) {
		iounmap(pl320->mbox_base);
		kfree(pl320);
	}

	return ret;
}

static struct amba_id pl320_ids[] = {
	{
		.id	= 0x00041320,
		.mask	= 0x000fffff,
	},
	{ 0, 0 },
};

static struct amba_driver pl320_driver = {
	.drv = {
		.name	= "pl320",
	},
	.id_table	= pl320_ids,
	.probe		= pl320_probe,
};

static int __init mbox_init(void)
{
	return amba_driver_register(&pl320_driver);
}
module_init(mbox_init);
