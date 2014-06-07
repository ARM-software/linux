/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_USB_SWITCH_H_
#define _EXYNOS_USB_SWITCH_H_

struct s5p_usbswitch_platdata {
	unsigned gpio_host_detect;
	unsigned gpio_device_detect;
	unsigned gpio_host_vbus;

	struct device *ehci_dev;
	struct device *ohci_dev;

	struct device *s3c_udc_dev;
};

#endif /* _EXYNOS_USB_SWITCH_H_ */
