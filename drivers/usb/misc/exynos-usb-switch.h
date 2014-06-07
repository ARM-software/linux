/*
 * exynos-usb-switch.h - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_USB_SWITCH
#define __EXYNOS_USB_SWITCH

#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

enum usb_cable_status {
	USB_DEVICE_ATTACHED,
	USB_HOST_ATTACHED,
	USB_DEVICE_DETACHED,
	USB_HOST_DETACHED,
};

struct exynos_usb_switch {
	unsigned long connect;

	int host_detect_irq;
	int device_detect_irq;
	int gpio_host_detect;
	int gpio_device_detect;
	int gpio_host_vbus;

	struct device *ehci_dev;
	struct device *ohci_dev;

	struct device *s3c_udc_dev;

	struct workqueue_struct	*workqueue;
	struct work_struct switch_work;
	struct mutex mutex;
	struct wake_lock wake_lock;
	atomic_t usb_status;
	int (*get_usb_mode)(void);
	int (*change_usb_mode)(int mode);
};

#endif
