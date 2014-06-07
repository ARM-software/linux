/**
 * otg.c - DesignWare USB3 DRD Controller OTG
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Ido Shayevitz <idos@codeaurora.org>
 *	    Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_USB_DWC3_OTG_H
#define __LINUX_USB_DWC3_OTG_H

#include <linux/wakelock.h>

#include "../phy/phy-fsm-usb.h"

struct dwc3_ext_otg_ops {
	int	(*setup)(struct device *dev, struct otg_fsm *fsm);
	void	(*exit)(struct device *dev);
	int	(*start) (struct device *dev);
	void	(*stop)(struct device *dev);
};

/**
 * struct dwc3_otg: OTG driver data. Shared by HCD and DCD.
 * @otg: USB OTG Transceiver structure.
 * @fsm: OTG Final State Machine.
 * @dwc: pointer to our controller context structure.
 * @irq: IRQ number assigned for HSUSB controller.
 * @regs: ioremapped register base address.
 * @wakelock: prevents the system from entering suspend while
 * 	      host or peripheral mode is active.
 * @vbus_reg: Vbus regulator.
 * @ext_otg_ops: external OTG engine ops.
 */
struct dwc3_otg {
	struct usb_otg          otg;
	struct otg_fsm		fsm;
	struct dwc3             *dwc;
	int                     irq;
	void __iomem            *regs;
	struct wake_lock	wakelock;

	struct regulator	*vbus_reg;

	struct dwc3_ext_otg_ops	*ext_otg_ops;
};

static inline int dwc3_ext_otg_setup(struct dwc3_otg *dotg)
{
	struct device *dev = dotg->dwc->dev->parent;

	if (!dotg->ext_otg_ops->setup)
		return -EOPNOTSUPP;
	return dotg->ext_otg_ops->setup(dev, &dotg->fsm);
}

static inline int dwc3_ext_otg_exit(struct dwc3_otg *dotg)
{
	struct device *dev = dotg->dwc->dev->parent;

	if (!dotg->ext_otg_ops->exit)
		return -EOPNOTSUPP;
	dotg->ext_otg_ops->exit(dev);
	return 0;
}

static inline int dwc3_ext_otg_start(struct dwc3_otg *dotg)
{
	struct device *dev = dotg->dwc->dev->parent;

	if (!dotg->ext_otg_ops->start)
		return -EOPNOTSUPP;
	return dotg->ext_otg_ops->start(dev);
}

static inline int dwc3_ext_otg_stop(struct dwc3_otg *dotg)
{
	struct device *dev = dotg->dwc->dev->parent;

	if (!dotg->ext_otg_ops->stop)
		return -EOPNOTSUPP;
	dotg->ext_otg_ops->stop(dev);
	return 0;
}

/* prototypes */
#if IS_ENABLED(CONFIG_USB_DWC3_EXYNOS)
bool dwc3_exynos_rsw_available(struct device *dev);
int dwc3_exynos_rsw_setup(struct device *dev, struct otg_fsm *fsm);
void dwc3_exynos_rsw_exit(struct device *dev);
int dwc3_exynos_rsw_start(struct device *dev);
void dwc3_exynos_rsw_stop(struct device *dev);
#endif

#endif /* __LINUX_USB_DWC3_OTG_H */
