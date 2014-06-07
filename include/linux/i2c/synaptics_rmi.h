/*
 * Copyright (c) 2011, 2012 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef _SYNAPTICS_RMI4_GENERIC_H_
#define _SYNAPTICS_RMI4_GENERIC_H_

#ifndef CONFIG_KEYBOARD_CYPRESS_TOUCH
#define NO_0D_WHILE_2D
#endif

struct synaptics_rmi_f1a_button_map {
	unsigned char nbuttons;
	unsigned char *map;
};

struct synaptics_rmi_callbacks {
	void (*inform_charger)(struct synaptics_rmi_callbacks *, bool);
};

/**
 * struct synaptics_rmi4_platform_data - rmi4 platform data
 * @x_flip: x flip flag
 * @y_flip: y flip flag
 * @regulator_en: regulator enable flag
 * @gpio: attention interrupt gpio
 * @irq_type: irq type
 * @gpio_config: pointer to gpio configuration function
 * @f1a_button_map: pointer to 0d button map
 */
struct synaptics_rmi4_platform_data {
	bool x_flip;
	bool y_flip;
	unsigned int sensor_max_x;
	unsigned int sensor_max_y;
	unsigned char max_touch_width;
	unsigned char panel_revision;	/* to identify panel info */
	bool regulator_en;
	unsigned gpio;
	int irq_type;
	int (*gpio_config)(unsigned interrupt_gpio, bool configure);
	int (*power)(bool on);
#ifdef NO_0D_WHILE_2D
	int (*led_power_on) (bool);
#endif
	unsigned char (*get_ddi_type)(void);	/* to indentify ddi type */
	void (*enable_sync)(bool on);
	const char *firmware_name;
	const char *fac_firmware_name;
    int num_of_rx;
    int num_of_tx;
#ifdef CONFIG_SEC_TSP_FACTORY
	int bootmode;
#endif
	struct synaptics_rmi_f1a_button_map *f1a_button_map;
};

extern unsigned int lcdtype;

#endif
