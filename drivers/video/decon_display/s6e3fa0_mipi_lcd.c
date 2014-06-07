/* drivers/video/decon_display/s6e3fa0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "decon_mipi_dsi.h"
#include "s6e3fa0_gamma.h"
#include "decon_display_driver.h"

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

struct decon_lcd s6e3fa0_lcd_info = {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	.mode = COMMAND_MODE,
#else
	.mode = VIDEO_MODE,
#endif
	.vfp = 1,
	.vbp = 10,
	.hfp = 1,
	.hbp = 1,

	.vsa = 1,
	.hsa = 1,

	.xres = 1080,
	.yres = 1920,

	.width = 70,
	.height = 121,

	/* Mhz */
	.hs_clk = 1100,
	.esc_clk = 20,

	.fps = 60,
};

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;

static const unsigned char ED[] = {0xed, 0x01, 0x00};
static const unsigned char FD[] = {0xfd, 0x16, 0x80};
static const unsigned char F6[] = {0xf6, 0x08};
static const unsigned char E7[] = {0xE7, 0xED, 0xC7, 0x23, 0x57, 0xA5};
static const unsigned char EB[] = {0xeb, 0x01, 0x00};
static const unsigned char C0[] = {0xc0, 0x63, 0x02, 0x03, 0x32, 0xFF, 0x44, 0x44, 0xC0, 0x00, 0x40};
static const unsigned char D29[] = {0x29, 0x00, 0x00};

static const unsigned char SEQ_READ_ID[] = {
	0x04,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SEQ_GAMMA_CONTROL_SET_300CD[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x06, 0x06, 0x06, 0x48, 0x18, 0x3F, 0xFF,
	0xFF,
};

static const unsigned char SEQ_ELVSS_CONDITION_SET[] = {
	0xB6,
	0x88, 0x0A,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03, 0x00
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_ACL_CONTROL[] = {
	0xB5,
	0x03, 0x98, 0x26, 0x36, 0x45,
};

static const unsigned char SEQ_ETC_PENTILE_SETTING[] = {
	0xC0,
	0x00, 0x02, 0x03, 0x32, 0xD8, 0x44, 0x44, 0xC0, 0x00, 0x48,
	0x20, 0xD8,
};

static const unsigned char SEQ_GLOBAL_PARAM_SOURCE_AMP[] = {
	0xB0,
	0x24,
};

static const unsigned char SEQ_ETC_SOURCE_AMP[] = {
	0xD7,
	0xA5,
};

static const unsigned char SEQ_GLOBAL_PARAM_BIAS_CURRENT[] = {
	0xB0,
	0x1F,
};

static const unsigned char SEQ_ETC_BIAS_CURRENT[] = {
	0xD7,
	0x0A,
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00,  0x00
};

static const unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00,  0x00
};

static const unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};

static const unsigned char SEQ_TOUCHKEY_OFF[] = {
	0xFF,
	0x00,
};

static const unsigned char SEQ_TOUCHKEY_ON[] = {
	0xFF,
	0x01,
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55, 0x00,
	0x00
};

static const unsigned char SEQ_ACL_40[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char SEQ_ACL_40_RE_LOW[] = {
	0x55, 0x02,
	0x00
};

static const unsigned char SEQ_DISPCTL[] = {
	0xF2,
	0x02, 0x03, 0xC, 0xA0, 0x01, 0x48
};

struct decon_lcd * decon_get_lcd_info(void)
{
	return &s6e3fa0_lcd_info;
}
EXPORT_SYMBOL(decon_get_lcd_info);

static int s6e3fa0_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0:
		backlightlevel = 0;
		break;
	case 1 ... 29:
		backlightlevel = 0;
		break;
	case 30 ... 34:
		backlightlevel = 1;
		break;
	case 35 ... 39:
		backlightlevel = 2;
		break;
	case 40 ... 44:
		backlightlevel = 3;
		break;
	case 45 ... 49:
		backlightlevel = 4;
		break;
	case 50 ... 54:
		backlightlevel = 5;
		break;
	case 55 ... 64:
		backlightlevel = 6;
		break;
	case 65 ... 74:
		backlightlevel = 7;
		break;
	case 75 ... 83:
		backlightlevel = 8;
		break;
	case 84 ... 93:
		backlightlevel = 9;
		break;
	case 94 ... 103:
		backlightlevel = 10;
		break;
	case 104 ... 113:
		backlightlevel = 11;
		break;
	case 114 ... 122:
		backlightlevel = 12;
		break;
	case 123 ... 132:
		backlightlevel = 13;
		break;
	case 133 ... 142:
		backlightlevel = 14;
		break;
	case 143 ... 152:
		backlightlevel = 15;
		break;
	case 153 ... 162:
		backlightlevel = 16;
		break;
	case 163 ... 171:
		backlightlevel = 17;
		break;
	case 172 ... 181:
		backlightlevel = 18;
		break;
	case 182 ... 191:
		backlightlevel = 19;
		break;
	case 192 ... 201:
		backlightlevel = 20;
		break;
	case 202 ... 210:
		backlightlevel = 21;
		break;
	case 211 ... 220:
		backlightlevel = 22;
		break;
	case 221 ... 230:
		backlightlevel = 23;
		break;
	case 231 ... 240:
		backlightlevel = 24;
		break;
	case 241 ... 250:
		backlightlevel = 25;
		break;
	case 251 ... 255:
		backlightlevel = 26;
		break;
	default:
		backlightlevel = 12;
		break;
	}

	return backlightlevel;
}

static int update_brightness(int brightness)
{
	int backlightlevel;

	if(1) return 1;
	backlightlevel = get_backlight_level(brightness);

	while (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)gamma22_table[backlightlevel],
				GAMMA_PARAM_SIZE) == -1)
		printk(KERN_ERR "fail to write gamma value.\n");

	while (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_GAMMA_UPDATE,
				ARRAY_SIZE(SEQ_GAMMA_UPDATE)) == -1)
		printk(KERN_ERR "fail to update gamma value.\n");
	return 1;
}

static int s6e3fa0_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 1;
}

static const struct backlight_ops s6e3fa0_backlight_ops = {
	.get_brightness = s6e3fa0_get_brightness,
	.update_status = s6e3fa0_set_brightness,
};

static int s6e3fa0_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &s6e3fa0_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

#ifdef CONFIG_DECON_MIC
int init_lcd_mic(struct mipi_dsim_device *dsim)
{
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)SEQ_TEST_KEY_ON_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_ON_F0 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_F1,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F1)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_ON_FC command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_ON_FC command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)ED,
				ARRAY_SIZE(ED)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TOUCHKEY_OFF command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)FD,
				ARRAY_SIZE(FD)) == -1)
		dev_err(dsim->dev, "fail to send FD command.\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		0xF6, 0x08);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x35, 0x0);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		0xF9, 0x2B);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x11, 0);

	msleep(120);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)EB,
				ARRAY_SIZE(EB)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_OFF_FC command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)C0,
				ARRAY_SIZE(C0)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_DISPCTL command.\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x29, 0);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x35, 0x0);

	return 1;
}
#else
static void init_lcd(struct mipi_dsim_device *dsim)
{
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)SEQ_TEST_KEY_ON_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_ON_F0 command.\n");

	msleep(12);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_F1,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F1)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_ON_FC command.\n");

	msleep(12);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_ON_FC command.\n");

	msleep(12);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)ED,
				ARRAY_SIZE(ED)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TOUCHKEY_OFF command.\n");
	msleep(12);
#ifdef CONFIG_FB_I80_COMMAND_MODE
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)FD,
				ARRAY_SIZE(FD)) == -1)
		dev_err(dsim->dev, "fail to send FD command.\n");
	msleep(12);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		0xF6, 0x08);

	mdelay(12);
#else
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)E7,
				ARRAY_SIZE(E7)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_GLOBAL_PARAM_SOURCE_AMP command.\n");

	msleep(120);
#endif
#ifdef CONFIG_DECON_MIC
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		0xF9, 0x2B);
#endif
	mdelay(20);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x11, 0);

	mdelay(20);
#ifndef CONFIG_FB_I80_COMMAND_MODE
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x29, 0);

	mdelay(120);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		0xF2, 0x02);

	mdelay(12);
#endif
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)EB,
				ARRAY_SIZE(EB)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_TEST_KEY_OFF_FC command.\n");

	mdelay(12);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)C0,
				ARRAY_SIZE(C0)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_DISPCTL command.\n");

	mdelay(12);
#ifdef CONFIG_FB_I80_COMMAND_MODE
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x35, 0x0);

	mdelay(12);

#else
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned int)D29,
				ARRAY_SIZE(D29)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_DISPCTL command.\n");
#endif
	mdelay(12);

	update_brightness(bd->props.brightness);
}
#endif

static int s6e3fa0_displayon(struct mipi_dsim_device *dsim)
{
#ifdef CONFIG_DECON_MIC
	init_lcd_mic(dsim);
#else
	init_lcd(dsim);
#endif
	return 1;
}

static int s6e3fa0_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int s6e3fa0_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6e3fa0_mipi_lcd_driver = {
	.probe		= s6e3fa0_probe,
	.displayon	= s6e3fa0_displayon,
	.suspend	= s6e3fa0_suspend,
	.resume		= s6e3fa0_resume,
};
