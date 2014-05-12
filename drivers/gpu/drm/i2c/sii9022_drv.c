/*
 * Copyright (C) 2013 ARM Limited
 * Author: Liviu Dudau <Liviu.Dudau@arm.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 *  Silicon Image SiI9022 driver for DRM I2C encoder slave
 */

#include <linux/module.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder_slave.h>

#define SII9022_INFORMAT_REG		0x09
#define SII9022_OUTFORMAT_REG		0x0a
#define SII9022_SYS_CONTROL_REG		0x1a
#  define SII9022_DDC_OUT_MODE		(1 << 0)
#  define SII9022_DDC_BUS_STAT		(1 << 1)
#  define SII9022_DDC_BUS_GRANT		(1 << 2)
#  define SII9022_AV_MUTE		(1 << 3)
#  define SII9022_OUTPUT_EN		(1 << 4)
#define SII9022_ID_REG			0x1b
#define SII9022_POWER_REG		0x1e
#define SII9022_SEC_CTRL_REG		0x2a
#define SII9022_SEC_STATUS_REG		0x29
#define SII9022_SEC_VERSION_REG		0x30
#define SII9022_INTR_REG		0x3c
#define SII9022_INTR_STATUS		0x3d
#  define SII9022_HOTPLUG_EVENT		(1 << 0)
#  define SII9022_RECEIVER_EVENT	(1 << 1)
#  define SII9022_HOTPLUG_STATE		(1 << 2)
#  define SII9022_RECEIVER_STATE	(1 << 3)
#define SII9022_VENDOR_ID		0xb0
#define SII9022_INTERNAL_PAGE		0xbc
#define SII9022_INTERNAL_INDEX		0xbd
#define SII9022_INTERNAL_REG		0xbe
#define SII9022_CTRL_REG		0xc7


struct sii9022_video_regs {
	uint16_t	pixel_clock;
	uint16_t	vrefresh;
	uint16_t	cols;
	uint16_t	lines;
	uint8_t		pixel_data;
};

static void sii9022_write(struct i2c_client *client, uint8_t addr, uint8_t val)
{
	uint8_t buf[] = { addr, val };
	int ret;

	ret = i2c_master_send(client, buf, ARRAY_SIZE(buf));
	if (ret < 0)
		dev_err(&client->dev, "Error writing to subaddress 0x%x: %d\n",	addr, ret);
}

static uint8_t sii9022_read(struct i2c_client *client, uint8_t addr)
{
	uint8_t val;
	int ret;

	ret = i2c_master_send(client, &addr, sizeof(addr));
	if (ret < 0)
		goto fail;

	ret = i2c_master_recv(client, &val, sizeof(val));
	if (ret < 0)
		goto fail;

	return val;

fail:
	dev_err(&client->dev, "Error reading from subaddress 0x%x: %d\n", addr, ret);
	return 0;
}

static void sii9022_encoder_set_config(struct drm_encoder *encoder, void *params)
{
}

static void sii9022_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct i2c_client *client = drm_i2c_encoder_get_client(encoder);
	uint8_t val;

	switch (mode) {
	case DRM_MODE_DPMS_OFF:
		val = sii9022_read(client, SII9022_SYS_CONTROL_REG);
		val |= SII9022_OUTPUT_EN;
		sii9022_write(client, SII9022_SYS_CONTROL_REG, val);
		/* wait for AVI InfoFrames to flush */
		mdelay(128);
		/* use D2 for OFF as we cannot control the reset pin */
		sii9022_write(client, SII9022_POWER_REG, 0x2);
		break;
	case DRM_MODE_DPMS_ON:
		val = sii9022_read(client, SII9022_SYS_CONTROL_REG);
		val &= ~SII9022_OUTPUT_EN;
		sii9022_write(client, SII9022_SYS_CONTROL_REG, val);
		/* fall through */
	default:
		sii9022_write(client, SII9022_POWER_REG, mode);
		break;
	}
}

static enum drm_connector_status
sii9022_encoder_detect(struct drm_encoder *encoder,
			struct drm_connector *connector)
{
	struct i2c_client *client = drm_i2c_encoder_get_client(encoder);
	enum drm_connector_status con_status = connector_status_unknown;
	uint8_t status = sii9022_read(client, SII9022_INTR_STATUS);

	if (status & SII9022_HOTPLUG_STATE)
		con_status = connector_status_connected;
	else
		con_status = connector_status_disconnected;

	/* clear the event status bits */
	sii9022_write(client, SII9022_INTR_STATUS,
		0xff /*SII9022_HOTPLUG_EVENT | SII9022_RECEIVER_EVENT */);
	status = sii9022_read(client, SII9022_INTR_STATUS);

	return con_status;
}

static int sii9022_encoder_get_modes(struct drm_encoder *encoder,
				struct drm_connector *connector)
{
	struct i2c_client *client = drm_i2c_encoder_get_client(encoder);
	struct edid *edid = NULL;
	uint8_t status;
	int ret = 0, timeout = 10;

	/* Disable HDCP link security */
	do {
		sii9022_write(client, SII9022_SEC_CTRL_REG, 0);
		status = sii9022_read(client, SII9022_SEC_CTRL_REG);
	} while (status);
	status = sii9022_read(client, SII9022_SEC_STATUS_REG);

	/* first, request the pass-through mode in order to read the edid */
	status = sii9022_read(client, SII9022_SYS_CONTROL_REG);
	status |= SII9022_DDC_BUS_GRANT;
	sii9022_write(client, SII9022_SYS_CONTROL_REG, status);
	do {
		/* wait for state change */
		status = sii9022_read(client, SII9022_SYS_CONTROL_REG);
		--timeout;
	} while (((status & SII9022_DDC_BUS_STAT) != SII9022_DDC_BUS_STAT) && timeout);

	if (!timeout) {
		dev_warn(&client->dev, "timeout waiting for DDC bus grant\n");
		goto release_ddc;
	}

	/* write back the value read in order to close the i2c switch */
	sii9022_write(client, SII9022_SYS_CONTROL_REG, status);

	edid = drm_get_edid(connector, client->adapter);
	if (!edid) {
		dev_err(&client->dev, "failed to get EDID data\n");
		ret = -1;
	}

release_ddc:
	timeout = 10;
	do {
		status &= ~(SII9022_DDC_BUS_STAT | SII9022_DDC_BUS_GRANT);
		sii9022_write(client, SII9022_SYS_CONTROL_REG, status);
		status = sii9022_read(client, SII9022_SYS_CONTROL_REG);
		--timeout;
	} while ((status & (SII9022_DDC_BUS_STAT | SII9022_DDC_BUS_GRANT)) && timeout);

	if (edid) {
		drm_mode_connector_update_edid_property(connector, edid);
		ret = drm_add_edid_modes(connector, edid);
		if (drm_detect_hdmi_monitor(edid))
			sii9022_write(client, SII9022_SYS_CONTROL_REG, status | 1);
		else
			sii9022_write(client, SII9022_SYS_CONTROL_REG, status & 0xfe);
		kfree(edid);
	}

	return ret;
}

static bool sii9022_encoder_mode_fixup(struct drm_encoder *encoder,
				const struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int sii9022_encoder_mode_valid(struct drm_encoder *encoder,
				struct drm_display_mode *mode)
{
	return MODE_OK;
}

static void sii9022_encoder_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct i2c_client *client = drm_i2c_encoder_get_client(encoder);
	/* SiI9022 clock is pixclock / 10000 Hz */
	int clk = adjusted_mode->crtc_clock / 10;
	int i, vrefresh = adjusted_mode->vrefresh * 100;
	uint8_t buf[15];   /* start address reg + 14 bytes max */

	/* Set Video Mode (8 registers block) */
	buf[0] = 0;		/* start register */
	buf[1] = clk & 0xff;
	buf[2] = (clk & 0xff00) >> 8;
	buf[3] = vrefresh & 0xff;
	buf[4] = (vrefresh & 0xff00) >> 8;
	buf[5] = adjusted_mode->crtc_hdisplay & 0xff;
	buf[6] = (adjusted_mode->crtc_hdisplay & 0xff00) >> 8;
	buf[7] = adjusted_mode->crtc_vdisplay & 0xff;
	buf[8] = (adjusted_mode->crtc_vdisplay & 0xff00) >> 8;

	if (i2c_master_send(client, buf, 9) < 0) {
		dev_err(&client->dev, "Could not write video mode data\n");
		return;
	}

	/* input is full range RGB */
	sii9022_write(client, SII9022_INFORMAT_REG, 0x04);
	/* output is full range digital RGB */
	sii9022_write(client, SII9022_OUTFORMAT_REG, 0x17);

	/* set the AVI InfoFrame (14 registers block */
	buf[0] = 0x0c;		/* start register */
	buf[1] = 0x0e;		/* AVI_DBYTE0 = checksum */
	buf[2] = 0x10;		/* AVI_DBYTE1 */
	buf[3] = 0x50;		/* AVI_DBYTE2 (colorimetry) */
	buf[4] = 0;		/* AVI_DBYTE3 (scaling) */
	buf[5] = drm_match_cea_mode(adjusted_mode);
	buf[6] = 0;		/* AVI_DBYTE5 (pixel repetition factor) */
	buf[7] = 0;
	buf[8] = 0;
	buf[9] = 0;
	buf[10] = 0;
	buf[11] = 0;
	buf[12] = 0;
	buf[13] = 0;
	buf[14] = 0;

	/* calculate checksum */
	buf[1] = 0x82 + 0x02 + 13;	/* Identifier code for AVI InfoFrame, length */
	for (i = 2; i < 15; i++)
		buf[1] += buf[i];
	buf[1] = 0x100 - buf[1];

	if (i2c_master_send(client, buf, ARRAY_SIZE(buf)) < 0) {
		dev_err(&client->dev, "Could not write video mode data\n");
		return;
	}
}

static struct drm_encoder_slave_funcs sii9022_encoder_funcs = {
	.set_config	= sii9022_encoder_set_config,
	.dpms		= sii9022_encoder_dpms,
	.detect		= sii9022_encoder_detect,
	.get_modes	= sii9022_encoder_get_modes,
	.mode_fixup	= sii9022_encoder_mode_fixup,
	.mode_valid	= sii9022_encoder_mode_valid,
	.mode_set	= sii9022_encoder_mode_set,
};

static int sii9022_encoder_init(struct i2c_client *client,
				struct drm_device *dev,
				struct drm_encoder_slave *encoder)
{
	encoder->slave_funcs = &sii9022_encoder_funcs;

	return 0;
}

static struct i2c_device_id sii9022_ids[] = {
	{ "sii9022-tpi", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sii9022_ids);

static int sii9022_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int dev_id, dev_rev;

	/* first step is to enable the TPI mode */
	sii9022_write(client, SII9022_CTRL_REG, 0x00);
	dev_id = sii9022_read(client, SII9022_ID_REG);
	dev_rev = sii9022_read(client, SII9022_ID_REG+1);
	if (dev_id != SII9022_VENDOR_ID) {
		printk(KERN_INFO "sii9022 not found\n");
		return -ENODEV;
	}
	dev_id = sii9022_read(client, SII9022_SEC_VERSION_REG);
	dev_info(&client->dev, "found %s chip (rev %01u.%01u)\n",
		dev_id ? "SiI9024" : "SiI9022",
		(dev_rev >> 4) & 0xf, dev_rev & 0xf);

	/* disable interrupts */
	sii9022_write(client, SII9022_INTR_REG, 0x00);

	return 0;
}

static int sii9022_remove(struct i2c_client *client)
{
	return 0;
}

static struct drm_i2c_encoder_driver sii9022_driver = {
	.i2c_driver = {
		.probe	= sii9022_probe,
		.remove	= sii9022_remove,
		.driver	= {
			.name = "sii9022-tpi",
		},
		.id_table = sii9022_ids,
		.class	= I2C_CLASS_DDC,
	},
	.encoder_init = sii9022_encoder_init,
};

static int __init sii9022_init(void)
{
	return drm_i2c_encoder_register(THIS_MODULE, &sii9022_driver);
}

static void __exit sii9022_exit(void)
{
	drm_i2c_encoder_unregister(&sii9022_driver);
}

module_init(sii9022_init);
module_exit(sii9022_exit);

MODULE_ALIAS(I2C_MODULE_PREFIX "sii9022-tpi");
MODULE_AUTHOR("Liviu Dudau <Liviu.Dudau@arm.com>");
MODULE_DESCRIPTION("Silicon Image SiI9022 HDMI transmitter driver");
MODULE_LICENSE("GPL v2");
