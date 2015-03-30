/*
 * Driver for the Message Handling Unit (MHU) which is the peripheral
 * in any SoC providing a mechanism for inter-processor communication
 * between two processors. For example System Control Processor (SCP)
 * with Cortex-M3 processor and Application Processors (AP). SCP
 * controls most of the power management on the AP.
 *
 * The MHU peripheral provides a mechanism to assert interrupt signals
 * to facilitate inter-processor message passing between the SCP and the
 * AP. The message payload can be deposited into main memory or on-chip
 * memories and MHU expects the payload to be ready before asserting the
 * signals. The payload is handled by protocol drivers and is out of
 * scope of this controller driver.
 *
 * The MHU supports three bi-directional channels - low priority, high
 * priority and secure(can't be used in non-secure execution modes)
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * Author: Sudeep Holla <sudeep.holla@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#define CONTROLLER_NAME		"mhu_ctlr"
#define CHANNEL_MAX		2

/*
 * +--------------------+-------+---------------+
 * |  Hardware Register | Offset|  Driver View  |
 * +--------------------+-------+---------------+
 * |  SCP_INTR_L_STAT   | 0x000 |  RX_STATUS(L) |
 * |  SCP_INTR_L_SET    | 0x008 |  RX_SET(L)    |
 * |  SCP_INTR_L_CLEAR  | 0x010 |  RX_CLEAR(L)  |
 * +--------------------+-------+---------------+
 * |  SCP_INTR_H_STAT   | 0x020 |  RX_STATUS(H) |
 * |  SCP_INTR_H_SET    | 0x028 |  RX_SET(H)    |
 * |  SCP_INTR_H_CLEAR  | 0x030 |  RX_CLEAR(H)  |
 * +--------------------+-------+---------------+
 * |  CPU_INTR_L_STAT   | 0x100 |  TX_STATUS(L) |
 * |  CPU_INTR_L_SET    | 0x108 |  TX_SET(L)    |
 * |  CPU_INTR_L_CLEAR  | 0x110 |  TX_CLEAR(L)  |
 * +--------------------+-------+---------------+
 * |  CPU_INTR_H_STAT   | 0x120 |  TX_STATUS(H) |
 * |  CPU_INTR_H_SET    | 0x128 |  TX_SET(H)    |
 * |  CPU_INTR_H_CLEAR  | 0x130 |  TX_CLEAR(H)  |
 * +--------------------+-------+---------------+
*/
#define RX_OFFSET(chan)		((chan) * 0x20)
#define TX_OFFSET(chan)		(0x100 + (chan) * 0x20)

#define REG_STATUS		0x00
#define REG_SET			0x08
#define REG_CLEAR		0x10

struct mhu_chan {
	void __iomem *tx_offset;
	void __iomem *rx_offset;
	int rx_irq;
};

struct mhu_ctlr {
	void __iomem *mbox_base;
	struct mbox_controller mbox_con;
};

static irqreturn_t mbox_rx_handler(int irq, void *p)
{
	struct mbox_chan *mchan = p;
	struct mhu_chan *chan = mchan->con_priv;
	u32 rx_status;

	rx_status = readl_relaxed(chan->rx_offset + REG_STATUS);
	if (rx_status) {
		mbox_chan_received_data(mchan, &rx_status);
		writel_relaxed(rx_status, chan->rx_offset + REG_CLEAR);
	}

	return IRQ_HANDLED;
}

static int mhu_send_data(struct mbox_chan *mchan, void *msg)
{
	struct mhu_chan *chan = mchan->con_priv;
	u32 cmd = *(u32 *)msg;

	writel_relaxed(cmd, chan->tx_offset + REG_SET);
	return 0;
}

static bool mhu_last_tx_done(struct mbox_chan *mchan)
{
	struct mhu_chan *chan = mchan->con_priv;

	return readl_relaxed(chan->tx_offset + REG_STATUS) == 0;
}

static int mhu_startup(struct mbox_chan *mchan)
{
	return mhu_last_tx_done(mchan) ? 0 : -EBUSY;
}

static void mhu_shutdown(struct mbox_chan *mchan)
{
}

static struct mbox_chan_ops mhu_ops = {
	.send_data = mhu_send_data,
	.startup = mhu_startup,
	.shutdown = mhu_shutdown,
	.last_tx_done = mhu_last_tx_done,
};

static int mhu_probe(struct platform_device *pdev)
{
	int idx, ret;
	struct mhu_ctlr *ctlr;
	struct mbox_chan *mbox_chans;
	struct resource *res;
	struct device *dev = &pdev->dev;
	const char *const rx_irq_names[] = {
		"mhu_lpri_rx",
		"mhu_hpri_rx",
	};

	ctlr = devm_kzalloc(dev, sizeof(*ctlr), GFP_KERNEL);
	if (!ctlr) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get mailbox memory resource\n");
		return -ENXIO;
	}

	ctlr->mbox_base = devm_ioremap_resource(dev, res);
	if (!ctlr->mbox_base) {
		dev_err(dev, "failed to request or ioremap mailbox control\n");
		return -EADDRNOTAVAIL;
	}

	mbox_chans = devm_kcalloc(dev, CHANNEL_MAX, sizeof(*mbox_chans),
				  GFP_KERNEL);
	if (!mbox_chans) {
		dev_err(dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ctlr);

	ctlr->mbox_con.dev = dev;
	ctlr->mbox_con.ops = &mhu_ops;
	ctlr->mbox_con.num_chans = CHANNEL_MAX;
	ctlr->mbox_con.txdone_irq = false;
	ctlr->mbox_con.txdone_poll = true;
	ctlr->mbox_con.txpoll_period = 1;
	ctlr->mbox_con.chans = mbox_chans;

	for (idx = 0; idx < CHANNEL_MAX; idx++, mbox_chans++) {
		struct mhu_chan *chan;

		chan = devm_kzalloc(dev, sizeof(*chan), GFP_KERNEL);
		if (!chan) {
			dev_err(dev, "failed to allocate memory\n");
			return -ENOMEM;
		}

		chan->tx_offset = ctlr->mbox_base + TX_OFFSET(idx);
		chan->rx_offset = ctlr->mbox_base + RX_OFFSET(idx);

		chan->rx_irq = platform_get_irq_byname(pdev, rx_irq_names[idx]);
		if (chan->rx_irq < 0) {
			dev_err(dev, "failed to get interrupt for %s\n",
				rx_irq_names[idx]);
			return -ENXIO;
		}

		ret = devm_request_threaded_irq(dev, chan->rx_irq, NULL,
						mbox_rx_handler, IRQF_ONESHOT,
						rx_irq_names[idx], mbox_chans);
		if (ret) {
			dev_err(dev, "failed to register '%s' irq\n",
				rx_irq_names[idx]);
			return ret;
		}

		mbox_chans->con_priv = chan;
	}

	if (mbox_controller_register(&ctlr->mbox_con)) {
		dev_err(dev, "failed to register mailbox controller\n");
		return -ENOMEM;
	}
	_dev_info(dev, "registered mailbox controller %s\n", CONTROLLER_NAME);
	return 0;
}

static int mhu_remove(struct platform_device *pdev)
{
	int idx;
	struct device *dev = &pdev->dev;
	struct mhu_ctlr *ctlr = platform_get_drvdata(pdev);
	struct mbox_chan *mbox_chans = ctlr->mbox_con.chans;

	for (idx = 0; idx < CHANNEL_MAX; idx++, mbox_chans++) {
		struct mhu_chan *chan = mbox_chans->con_priv;

		devm_free_irq(dev, chan->rx_irq, mbox_chans);
		devm_kfree(dev, chan);
	}

	mbox_controller_unregister(&ctlr->mbox_con);
	_dev_info(dev, "unregistered mailbox controller %s\n",
		  CONTROLLER_NAME);
	devm_kfree(dev, ctlr->mbox_con.chans);

	devm_iounmap(dev, ctlr->mbox_base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(dev, ctlr);
	return 0;
}

static const struct of_device_id mhu_of_match[] = {
	{.compatible = "arm,mhu"},
	{},
};

MODULE_DEVICE_TABLE(of, mhu_of_match);

static struct platform_driver mhu_driver = {
	.probe = mhu_probe,
	.remove = mhu_remove,
	.driver = {
		   .name = CONTROLLER_NAME,
		   .of_match_table = mhu_of_match,
		   },
};
module_platform_driver(mhu_driver);

MODULE_AUTHOR("Sudeep Holla <sudeep.holla@arm.com>");
MODULE_DESCRIPTION("ARM MHU mailbox driver");
MODULE_LICENSE("GPL");
