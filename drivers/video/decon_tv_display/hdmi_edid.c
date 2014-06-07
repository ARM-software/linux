/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <uapi/linux/v4l2-dv-timings.h>

#include "hdmi.h"

#define EDID_SEGMENT_ADDR	(0x60 >> 1)
#define EDID_ADDR		(0xA0 >> 1)
#define EDID_BLOCK_SIZE		128
#define EDID_SEGMENT(x)		((x) >> 1)
#define EDID_OFFSET(x)		(((x) & 1) * EDID_BLOCK_SIZE)
#define EDID_EXTENSION_FLAG	0x7E
#define EDID_NATIVE_FORMAT	0x83
#define EDID_BASIC_AUDIO	(1 << 6)
#define EDID_3D_STRUCTURE_ALL	0x1
#define EDID_3D_STRUCTURE_MASK	0x2
#define EDID_3D_FP_MASK		(1)
#define EDID_3D_TB_MASK		(1 << 6)
#define EDID_3D_SBS_MASK	(1 << 8)
#define EDID_3D_FP		0
#define EDID_3D_TB		6
#define EDID_3D_SBS		8

#ifdef CONFIG_OF
static const struct of_device_id edid_device_table[] = {
	        { .compatible = "samsung,exynos5-edid_driver" },
		{},
};
MODULE_DEVICE_TABLE(of, edid_device_table);
#endif

static struct i2c_client *edid_client;

/* Structure for Checking 3D Mandatory Format in EDID */
static const struct edid_3d_mandatory_preset {
	struct v4l2_dv_timings dv_timings;
	u16 xres;
	u16 yres;
	u16 refresh;
	u32 vmode;
	u32 s3d;
} edid_3d_mandatory_presets[] = {
	{ V4L2_DV_BT_CEA_1280X720P60_FP,	1280, 720, 60, FB_VMODE_NONINTERLACED, EDID_3D_FP },
	{ V4L2_DV_BT_CEA_1280X720P60_TB,	1280, 720, 60, FB_VMODE_NONINTERLACED, EDID_3D_TB },
	{ V4L2_DV_BT_CEA_1280X720P50_FP,	1280, 720, 50, FB_VMODE_NONINTERLACED, EDID_3D_FP },
	{ V4L2_DV_BT_CEA_1280X720P50_TB,	1280, 720, 50, FB_VMODE_NONINTERLACED, EDID_3D_TB },
	{ V4L2_DV_BT_CEA_1920X1080P24_FP,	1920, 1080, 24, FB_VMODE_NONINTERLACED, EDID_3D_FP },
	{ V4L2_DV_BT_CEA_1920X1080P24_TB,	1920, 1080, 24, FB_VMODE_NONINTERLACED, EDID_3D_TB },
};

static struct edid_preset {
	struct v4l2_dv_timings dv_timings;
	u16 xres;
	u16 yres;
	u16 refresh;
	u32 vmode;
	char *name;
	bool supported;
} edid_presets[] = {
	{ V4L2_DV_BT_CEA_720X480P59_94,	720,  480,  59, FB_VMODE_NONINTERLACED, "480p@59.94" },
	{ V4L2_DV_BT_CEA_720X576P50,	720,  576,  50, FB_VMODE_NONINTERLACED, "576p@50" },
	{ V4L2_DV_BT_CEA_1280X720P50,	1280, 720,  50, FB_VMODE_NONINTERLACED, "720p@50" },
	{ V4L2_DV_BT_CEA_1280X720P60,	1280, 720,  60, FB_VMODE_NONINTERLACED, "720p@60" },
	{ V4L2_DV_BT_CEA_1920X1080P24,	1920, 1080, 24, FB_VMODE_NONINTERLACED, "1080p@24" },
	{ V4L2_DV_BT_CEA_1920X1080P25,	1920, 1080, 25, FB_VMODE_NONINTERLACED, "1080p@25" },
	{ V4L2_DV_BT_CEA_1920X1080P30,	1920, 1080, 30, FB_VMODE_NONINTERLACED, "1080p@30" },
	{ V4L2_DV_BT_CEA_1920X1080P50,	1920, 1080, 50, FB_VMODE_NONINTERLACED, "1080p@50" },
	{ V4L2_DV_BT_CEA_1920X1080P60,	1920, 1080, 60, FB_VMODE_NONINTERLACED, "1080p@60" },
	{ V4L2_DV_BT_CEA_3840X2160P24,  3840, 2160, 24, FB_VMODE_NONINTERLACED, "2160p@24" },
	{ V4L2_DV_BT_CEA_1920X1080I50,	1920, 1080, 50, FB_VMODE_INTERLACED, "1080i@50" },
	{ V4L2_DV_BT_CEA_1920X1080I60,	1920, 1080, 60, FB_VMODE_INTERLACED, "1080i@60" },
};

static struct edid_3d_preset {
	struct v4l2_dv_timings dv_timings;
	u16 xres;
	u16 yres;
	u16 refresh;
	u32 vmode;
	u32 s3d;
	char *name;
	bool supported;
} edid_3d_presets[] = {
	{ V4L2_DV_BT_CEA_1280X720P60_SB_HALF,	1280, 720, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "720p@60_SBS" },
	{ V4L2_DV_BT_CEA_1280X720P60_TB,	1280, 720, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "720p@60_TB" },
	{ V4L2_DV_BT_CEA_1280X720P50_SB_HALF,	1280, 720, 50,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "720p@50_SBS" },
	{ V4L2_DV_BT_CEA_1280X720P50_TB,	1280, 720, 50,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "720p@50_TB" },
	{ V4L2_DV_BT_CEA_1920X1080P24_FP,	1920, 1080, 24,
		FB_VMODE_NONINTERLACED, EDID_3D_FP, "1080p@24_FP" },
	{ V4L2_DV_BT_CEA_1920X1080P24_SB_HALF,	1920, 1080, 24,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "1080p@24_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P24_TB,	1920, 1080, 24,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "1080p@24_TB" },
	{ V4L2_DV_BT_CEA_1920X1080I60_SB_HALF,	1920, 1080, 60,
		FB_VMODE_INTERLACED, EDID_3D_SBS, "1080i@60_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080I50_SB_HALF,	1920, 1080, 50,
		FB_VMODE_INTERLACED, EDID_3D_SBS, "1080i@50_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P60_SB_HALF,	1920, 1080, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "1080p@60_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P60_TB,	1920, 1080, 60,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "1080p@60_TB" },
	{ V4L2_DV_BT_CEA_1920X1080P30_SB_HALF,	1920, 1080, 30,
		FB_VMODE_NONINTERLACED, EDID_3D_SBS, "1080p@30_SBS" },
	{ V4L2_DV_BT_CEA_1920X1080P30_TB,	1920, 1080, 30,
		FB_VMODE_NONINTERLACED, EDID_3D_TB, "1080p@30_TB" },
};

static struct v4l2_dv_timings preferred_preset;
static u32 edid_misc;
static int max_audio_channels;
static int audio_bit_rates;
static int audio_sample_rates;
static u32 source_phy_addr;

static int edid_i2c_read(struct hdmi_device *hdev, u8 segment, u8 offset,
						   u8 *buf, size_t len)
{
	struct device *dev = hdev->dev;
	struct i2c_client *i2c = edid_client;
	int cnt = 0;
	int ret;
	struct i2c_msg msg[] = {
		{
			.addr = EDID_SEGMENT_ADDR,
			.flags = segment ? 0 : I2C_M_IGNORE_NAK,
			.len = 1,
			.buf = &segment
		},
		{
			.addr = EDID_ADDR,
			.flags = 0,
			.len = 1,
			.buf = &offset
		},
		{
			.addr = EDID_ADDR,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf
		}
	};

	if (!i2c)
		return -ENODEV;

	do {
		ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
		if (ret == ARRAY_SIZE(msg))
			break;

		dev_dbg(dev, "%s: can't read data, retry %d\n", __func__, cnt);
		msleep(25);
		cnt++;
	} while (cnt < 5);

	if (cnt == 5) {
		dev_err(dev, "%s: can't read data, timeout\n", __func__);
		return -ETIME;
	}

	return 0;
}

static int
edid_read_block(struct hdmi_device *hdev, int block, u8 *buf, size_t len)
{
	struct device *dev = hdev->dev;
	int ret, i;
	u8 segment = EDID_SEGMENT(block);
	u8 offset = EDID_OFFSET(block);
	u8 sum = 0;

	if (len < EDID_BLOCK_SIZE)
		return -EINVAL;

	ret = edid_i2c_read(hdev, segment, offset, buf, EDID_BLOCK_SIZE);
	if (ret)
		return ret;

	for (i = 0; i < EDID_BLOCK_SIZE; i++)
		sum += buf[i];

	if (sum) {
		dev_err(dev, "%s: checksum error block=%d sum=%d\n", __func__,
								  block, sum);
		return -EPROTO;
	}

	return 0;
}

static int edid_read(struct hdmi_device *hdev, u8 **data)
{
	u8 block0[EDID_BLOCK_SIZE];
	u8 *edid;
	int block = 0;
	int block_cnt, ret;

	ret = edid_read_block(hdev, 0, block0, sizeof(block0));
	if (ret)
		return ret;

	block_cnt = block0[EDID_EXTENSION_FLAG] + 1;

	edid = kmalloc(block_cnt * EDID_BLOCK_SIZE, GFP_KERNEL);
	if (!edid)
		return -ENOMEM;

	memcpy(edid, block0, sizeof(block0));

	while (++block < block_cnt) {
		ret = edid_read_block(hdev, block,
				edid + block * EDID_BLOCK_SIZE,
					       EDID_BLOCK_SIZE);
		if ((edid[EDID_NATIVE_FORMAT] & EDID_BASIC_AUDIO) >> 6)
			edid_misc = FB_MISC_HDMI;
		if (ret) {
			kfree(edid);
			return ret;
		}
	}

	*data = edid;
	return block_cnt;
}

static struct edid_preset *edid_find_preset(struct fb_videomode *mode)
{
	struct edid_preset *preset = edid_presets;
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_presets); i++, preset++) {
		if (mode->refresh == preset->refresh &&
			mode->xres	== preset->xres &&
			mode->yres	== preset->yres &&
			mode->vmode	== preset->vmode) {
			return preset;
		}
	}

	return NULL;
}

static struct edid_3d_preset *edid_find_3d_mandatory_preset(const struct
				edid_3d_mandatory_preset *mandatory)
{
	struct edid_3d_preset *s3d_preset = edid_3d_presets;
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
		if (mandatory->refresh == s3d_preset->refresh &&
			mandatory->xres	== s3d_preset->xres &&
			mandatory->yres	== s3d_preset->yres &&
			mandatory->s3d	== s3d_preset->s3d) {
			return s3d_preset;
		}
	}

	return NULL;
}

static void edid_find_3d_preset(struct fb_video *vic, struct fb_vendor *vsdb)
{
	struct edid_3d_preset *s3d_preset = edid_3d_presets;
	int i;

	if ((vsdb->s3d_structure_all & EDID_3D_FP_MASK) >> EDID_3D_FP) {
		s3d_preset = edid_3d_presets;
		for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
			if (vic->refresh == s3d_preset->refresh &&
				vic->xres	== s3d_preset->xres &&
				vic->yres	== s3d_preset->yres &&
				vic->vmode	== s3d_preset->vmode &&
				EDID_3D_FP	== s3d_preset->s3d) {
				if (s3d_preset->supported == false) {
					s3d_preset->supported = true;
					pr_info("EDID: found %s",
							s3d_preset->name);
				}
			}
		}
	}
	if ((vsdb->s3d_structure_all & EDID_3D_TB_MASK) >> EDID_3D_TB) {
		s3d_preset = edid_3d_presets;
		for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
			if (vic->refresh == s3d_preset->refresh &&
				vic->xres	== s3d_preset->xres &&
				vic->yres	== s3d_preset->yres &&
				EDID_3D_TB	== s3d_preset->s3d) {
				if (s3d_preset->supported == false) {
					s3d_preset->supported = true;
					pr_info("EDID: found %s",
							s3d_preset->name);
				}
			}
		}
	}
	if ((vsdb->s3d_structure_all & EDID_3D_SBS_MASK) >> EDID_3D_SBS) {
		s3d_preset = edid_3d_presets;
		for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
			if (vic->refresh == s3d_preset->refresh &&
				vic->xres	== s3d_preset->xres &&
				vic->yres	== s3d_preset->yres &&
				EDID_3D_SBS	== s3d_preset->s3d) {
				if (s3d_preset->supported == false) {
					s3d_preset->supported = true;
					pr_info("EDID: found %s",
							s3d_preset->name);
				}
			}
		}
	}
}

static void edid_find_3d_more_preset(struct fb_video *vic, char s3d_structure)
{
	struct edid_3d_preset *s3d_preset = edid_3d_presets;
	int i;

	for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++, s3d_preset++) {
		if (vic->refresh == s3d_preset->refresh &&
			vic->xres	== s3d_preset->xres &&
			vic->yres	== s3d_preset->yres &&
			vic->vmode	== s3d_preset->vmode &&
			s3d_structure	== s3d_preset->s3d) {
			if (s3d_preset->supported == false) {
				s3d_preset->supported = true;
				pr_info("EDID: found %s", s3d_preset->name);
			}
		}
	}
}

static void edid_use_default_preset(void)
{
	int i;

	preferred_preset = hdmi_conf[HDMI_DEFAULT_TIMINGS_IDX].dv_timings;
	for (i = 0; i < ARRAY_SIZE(edid_presets); i++)
		edid_presets[i].supported =
			v4l_match_dv_timings(&edid_presets[i].dv_timings,
					&preferred_preset, 0);
	max_audio_channels = 2;
}

void edid_extension_update(struct fb_monspecs *specs)
{
	struct edid_3d_preset *s3d_preset;
	const struct edid_3d_mandatory_preset *s3d_mandatory
					= edid_3d_mandatory_presets;
	int i;

	if (!specs->vsdb)
		return;

	/* number of 128bytes blocks to follow */
	source_phy_addr = specs->vsdb->phy_addr;

	/* find 3D mandatory preset */
	if (specs->vsdb->s3d_present) {
		for (i = 0; i < ARRAY_SIZE(edid_3d_mandatory_presets);
				i++, s3d_mandatory++) {
			s3d_preset = edid_find_3d_mandatory_preset(s3d_mandatory);
			if (s3d_preset) {
				pr_info("EDID: found %s", s3d_preset->name);
				s3d_preset->supported = true;
			}
		}
	}

	/* find 3D multi preset */
	if (specs->vsdb->s3d_multi_present == EDID_3D_STRUCTURE_ALL)
		for (i = 0; i < specs->videodb_len + 1; i++)
			edid_find_3d_preset(&specs->videodb[i], specs->vsdb);
	else if (specs->vsdb->s3d_multi_present == EDID_3D_STRUCTURE_MASK)
		for (i = 0; i < specs->videodb_len + 1; i++)
			if ((specs->vsdb->s3d_structure_mask & (1 << i)) >> i)
				edid_find_3d_preset(&specs->videodb[i],
						specs->vsdb);

	/* find 3D more preset */
	if (specs->vsdb->s3d_field) {
		for (i = 0; i < specs->videodb_len + 1; i++) {
			edid_find_3d_more_preset(&specs->videodb
					[specs->vsdb->vic_order[i]],
					specs->vsdb->s3d_structure[i]);
			if (specs->vsdb->s3d_structure[i] > EDID_3D_TB + 1)
				i++;
		}
	}
}

int edid_update(struct hdmi_device *hdev)
{
	struct fb_monspecs specs;
	struct edid_preset *preset;
	bool first = true;
	u8 *edid = NULL;
	int channels_max = 0, support_bit_rates = 0, support_sample_rates = 0;
	int block_cnt = 0;
	int ret = 0;
	int i;

	edid_misc = 0;

	block_cnt = edid_read(hdev, &edid);
	if (block_cnt < 0)
		goto out;

	fb_edid_to_monspecs(edid, &specs);
	for (i = 1; i < block_cnt; i++) {
		ret = fb_edid_add_monspecs(edid + i * EDID_BLOCK_SIZE, &specs);
		if (ret < 0)
			goto out;
	}

	preferred_preset = hdmi_conf[HDMI_DEFAULT_TIMINGS_IDX].dv_timings;
	for (i = 0; i < ARRAY_SIZE(edid_presets); i++)
		edid_presets[i].supported = false;
	for (i = 0; i < ARRAY_SIZE(edid_3d_presets); i++)
		edid_3d_presets[i].supported = false;

	/* find 2D preset */
	for (i = 0; i < specs.modedb_len; i++) {
		preset = edid_find_preset(&specs.modedb[i]);
		if (preset) {
			if (preset->supported == false) {
				pr_info("EDID: found %s", preset->name);
				preset->supported = true;
			}
			if (first) {
				preferred_preset = preset->dv_timings;
				first = false;
			}
		}
	}

	/* number of 128bytes blocks to follow */
	if (block_cnt > 1)
		edid_extension_update(&specs);

	if (!edid_misc)
		edid_misc = specs.misc;
	pr_info("EDID: misc flags %08x", edid_misc);

	if (!specs.audiodb)
		goto out;

	for (i = 0; i < specs.audiodb_len; i++) {
		if (specs.audiodb[i].format != FB_AUDIO_LPCM)
			continue;
		if (specs.audiodb[i].channel_count > channels_max) {
			channels_max = specs.audiodb[i].channel_count;
			support_sample_rates = specs.audiodb[i].sample_rates;
			support_bit_rates = specs.audiodb[i].bit_rates;
		}
	}

	if (edid_misc & FB_MISC_HDMI) {
		if (channels_max) {
			max_audio_channels = channels_max;
			audio_sample_rates = support_sample_rates;
			audio_bit_rates = support_bit_rates;
		} else {
			max_audio_channels = 2;
			audio_sample_rates = FB_AUDIO_44KHZ; /*default audio info*/
			audio_bit_rates = FB_AUDIO_16BIT;
		}
	} else {
		max_audio_channels = 0;
		audio_sample_rates = 0;
		audio_bit_rates = 0;
	}
	pr_info("EDID: Audio channels %d", max_audio_channels);

out:
	/* No supported preset found, use default */
	if (first)
		edid_use_default_preset();

	if (block_cnt == -EPROTO)
		edid_misc = FB_MISC_HDMI;

	kfree(edid);
	return block_cnt;
}

struct v4l2_dv_timings edid_preferred_preset(struct hdmi_device *hdev)
{
	return preferred_preset;
}

bool edid_supports_hdmi(struct hdmi_device *hdev)
{
	return edid_misc & FB_MISC_HDMI;
}

u32 edid_audio_informs(struct hdmi_device *hdev)
{
	u32 value = 0, ch_info = 0;

	if (max_audio_channels > 0)
		ch_info |= (1 << (max_audio_channels - 1));
	if (max_audio_channels > 6)
		ch_info |= (1 << 5);
	value = ((audio_sample_rates << 19) | (audio_bit_rates << 16) |
			ch_info);
	return value;
}

int edid_source_phy_addr(struct hdmi_device *hdev)
{
	return source_phy_addr;
}

static int edid_probe(struct i2c_client *client,
				const struct i2c_device_id *dev_id)
{
	edid_client = client;
	edid_use_default_preset();
	dev_info(&client->adapter->dev, "probed exynos edid\n");
	return 0;
}

static int edid_remove(struct i2c_client *client)
{
	edid_client = NULL;
	dev_info(&client->adapter->dev, "removed exynos edid\n");
	return 0;
}

static struct i2c_device_id edid_idtable[] = {
	{"exynos_edid", 2},
};
MODULE_DEVICE_TABLE(i2c, edid_idtable);

static struct i2c_driver edid_driver = {
	.driver = {
		.name = "exynos_edid",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(edid_device_table),
	},
	.id_table	= edid_idtable,
	.probe		= edid_probe,
	.remove         = edid_remove,
};

static int __init edid_init(void)
{
	return i2c_add_driver(&edid_driver);
}

static void __exit edid_exit(void)
{
	i2c_del_driver(&edid_driver);
}
module_init(edid_init);
module_exit(edid_exit);
