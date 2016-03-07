/*
 *
 * (C) COPYRIGHT 2013-2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * ARM Mali DP hardware manipulation routines.
 */

#include <linux/types.h>
#include <asm/io.h>
#include <drm/drmP.h>
#include <video/videomode.h>
#include <video/display_timing.h>

#include "malidp_drv.h"
#include "malidp_hw.h"
#include "malidp_regs.h"

static const struct malidp_input_format malidp500_de_formats[] = {
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  0, DRM_FORMAT_ARGB2101010 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  1, DRM_FORMAT_ABGR2101010 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  2, DRM_FORMAT_ARGB8888 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  3, DRM_FORMAT_ABGR8888 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  4, DRM_FORMAT_XRGB8888 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  5, DRM_FORMAT_XBGR8888 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  6, DRM_FORMAT_RGB888 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  7, DRM_FORMAT_BGR888 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  8, DRM_FORMAT_RGBA5551 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2,  9, DRM_FORMAT_ABGR1555 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2, 10, DRM_FORMAT_RGB565 },
	{ DE_VIDEO1 | DE_GRAPHICS | DE_GRAPHICS2, 11, DRM_FORMAT_BGR565 },
	{ DE_VIDEO1, 12, DRM_FORMAT_UYVY },
	{ DE_VIDEO1, 13, DRM_FORMAT_YUYV },
	{ DE_VIDEO1, 14, DRM_FORMAT_NV12 },
	{ DE_VIDEO1, 15, DRM_FORMAT_YUV420 },
	{ DE_VIDEO1, 16, MALIDP_FORMAT_XYUV }, /* [31:0] X:Y:Cb:Cr 8:8:8:8 little endian */
	{ DE_VIDEO1, 17, MALIDP_FORMAT_VYU30 },/* [31:0] X:Cr:Y:Cb 2:10:10:10 little endian */
	{ DE_VIDEO1, 18, MALIDP_FORMAT_YUV10_420AFBC }, /* AFBC compressed YUV 4:2:0, 10 bits per component */
	{ DE_VIDEO1, 19, MALIDP_FORMAT_NV12AFBC }, /* AFBC compressed YUV 4:2:0. Maps to NV12 internally */
};

#define MALIDP_ID(__group, __format) \
	((((__group) & 0x7) << 3) | ((__format) & 0x7))

#define MALIDP_COMMON_FORMATS \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(0, 0), DRM_FORMAT_ARGB2101010 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(0, 1), DRM_FORMAT_ABGR2101010 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(0, 2), DRM_FORMAT_RGBA1010102 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(0, 3), DRM_FORMAT_BGRA1010102 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(1, 0), DRM_FORMAT_ARGB8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(1, 1), DRM_FORMAT_ABGR8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(1, 2), DRM_FORMAT_RGBA8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(1, 3), DRM_FORMAT_BGRA8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(2, 0), DRM_FORMAT_XRGB8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(2, 1), DRM_FORMAT_XBGR8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(2, 2), DRM_FORMAT_RGBX8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2 | DE_SMART, MALIDP_ID(2, 3), DRM_FORMAT_BGRX8888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(3, 0), DRM_FORMAT_RGB888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(3, 1), DRM_FORMAT_BGR888 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(4, 0), DRM_FORMAT_RGBA5551 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(4, 1), DRM_FORMAT_ABGR1555 }, \
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(4, 2), DRM_FORMAT_RGB565 },	\
	{ DE_VIDEO1 | DE_GRAPHICS | DE_VIDEO2, MALIDP_ID(4, 3), DRM_FORMAT_BGR565 },	\
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 0), MALIDP_FORMAT_XYUV }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 1), MALIDP_FORMAT_NV16AFBC }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 2), DRM_FORMAT_YUYV }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 3), DRM_FORMAT_UYVY }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 6), DRM_FORMAT_NV12 }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 7), DRM_FORMAT_YUV420 }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(6, 0), MALIDP_FORMAT_VYU30 }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(6, 6), MALIDP_FORMAT_Y0L2 }, \
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(6, 7), MALIDP_FORMAT_P010 }

static const struct malidp_input_format malidp550_de_formats[] = {
	MALIDP_COMMON_FORMATS,
};

static const struct malidp_input_format malidp650_de_formats[] = {
	MALIDP_COMMON_FORMATS,
	{ DE_VIDEO1 | DE_VIDEO2, MALIDP_ID(5, 4), MALIDP_FORMAT_Y0L0 },
};

static const struct malidp_layer malidp500_layers[] = {
	{ DE_VIDEO1, MALIDP500_DE_LV_BASE, MALIDP500_DE_LV_PTR_BASE },
	{ DE_GRAPHICS, MALIDP500_DE_LG1_BASE, MALIDP500_DE_LG1_PTR_BASE },
	{ DE_GRAPHICS2, MALIDP500_DE_LG2_BASE, MALIDP500_DE_LG2_PTR_BASE },
};

static const struct malidp_layer malidp550_layers[] = {
	{ DE_VIDEO1, MALIDP550_DE_LV1_BASE, MALIDP550_DE_LV1_PTR_BASE },
	{ DE_GRAPHICS, MALIDP550_DE_LG_BASE, MALIDP550_DE_LG_PTR_BASE },
	{ DE_VIDEO2, MALIDP550_DE_LV2_BASE, MALIDP550_DE_LV2_PTR_BASE },
	{ DE_SMART, MALIDP550_DE_LS_BASE, MALIDP550_DE_LS_PTR_BASE },
};

#define MALIDP_DE_DEFAULT_PREFETCH_START	5

int malidp500_query_configuration(struct malidp_hw_device *hwdev)
{
	u32 conf = malidp_hw_read(hwdev, MALIDP500_CONFIG_ID);
	if (conf & MALIDP_HW_FEATURE_DS)
		hwdev->features |= MALIDP_HW_FEATURE_DS;
	hwdev->max_line_size = SZ_2K * (1 << ((conf >> 4) & 0x1));

	return 0;
}

void malidp500_enter_config_mode(struct malidp_hw_device *hwdev)
{
	u32 status, count = 30;

	malidp_hw_setbits(hwdev, MALIDP500_DC_CONFIG_REQ, MALIDP500_DC_CONTROL);
	while (count) {
		status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
		if ((status & MALIDP500_DC_CONFIG_REQ) == MALIDP500_DC_CONFIG_REQ)
			break;
		/*
		 * entering config mode can take as long as the rendering
		 * of a full frame, hence the long sleep here
		 */
		usleep_range(1000, 10000);
		count--;
	}
	WARN(count == 0, "timeout while entering config mode");
}

void malidp500_leave_config_mode(struct malidp_hw_device *hwdev)
{
	u32 status, count = 30;

	malidp_hw_clearbits(hwdev, MALIDP500_DC_CONFIG_REQ, MALIDP500_DC_CONTROL);
	while (count) {
		status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
		if ((status & MALIDP500_DC_CONFIG_REQ) == 0)
			break;
		usleep_range(100, 1000);
		count--;
	}
	WARN(count == 0, "timeout while leaving config mode");
}

bool malidp500_in_config_mode(struct malidp_hw_device *hwdev)
{
	u32 status;

	status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
	if ((status & MALIDP500_DC_CONFIG_REQ) == MALIDP500_DC_CONFIG_REQ)
		return true;

	return false;
}

void malidp500_set_config_valid(struct malidp_hw_device *hwdev)
{
	malidp_hw_setbits(hwdev, MALIDP_CFG_VALID, MALIDP500_CONFIG_VALID);
}

void malidp500_modeset(struct malidp_hw_device *hwdev, struct videomode *mode)
{
	u32 val = 0;

	if (mode->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		val |= MALIDP500_HSYNCPOL;
	if (mode->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		val |= MALIDP500_VSYNCPOL;
	val |= MALIDP_DE_DEFAULT_PREFETCH_START;
	malidp_hw_setbits(hwdev, val, MALIDP500_DC_CONTROL);

	/*
	 * Mali-DP500 encodes the background color like this:
	 *    - red   @ MALIDP500_BGND_COLOR[12:0]
	 *    - green @ MALIDP500_BGND_COLOR[27:16]
	 *    - blue  @ (MALIDP500_BGND_COLOR + 4)[12:0]
	 */
	val = ((MALIDP_BGND_COLOR_G & 0xfff) << 16) |
	      (MALIDP_BGND_COLOR_R & 0xfff);
	malidp_hw_write(hwdev, val, MALIDP500_BGND_COLOR);
	malidp_hw_write(hwdev, MALIDP_BGND_COLOR_B, MALIDP500_BGND_COLOR + 4);

	val = MALIDP_DE_H_FRONTPORCH(mode->hfront_porch) |
		MALIDP_DE_H_BACKPORCH(mode->hback_porch);
	malidp_hw_write(hwdev, val, MALIDP500_TIMINGS_BASE + MALIDP_DE_H_TIMINGS);

	val = MALIDP500_DE_V_FRONTPORCH(mode->vfront_porch) |
		MALIDP_DE_V_BACKPORCH(mode->vback_porch);
	malidp_hw_write(hwdev, val, MALIDP500_TIMINGS_BASE + MALIDP_DE_V_TIMINGS);

	val = MALIDP_DE_H_SYNCWIDTH(mode->hsync_len) |
		MALIDP_DE_V_SYNCWIDTH(mode->vsync_len);
	malidp_hw_write(hwdev, val, MALIDP500_TIMINGS_BASE + MALIDP_DE_SYNC_WIDTH);

	val = MALIDP_DE_H_ACTIVE(mode->hactive) | MALIDP_DE_V_ACTIVE(mode->vactive);
	malidp_hw_write(hwdev, val, MALIDP500_TIMINGS_BASE + MALIDP_DE_HV_ACTIVE);
}

void malidp550_enter_config_mode(struct malidp_hw_device *hwdev)
{
	u32 status, count = 30;

	malidp_hw_setbits(hwdev, MALIDP550_DC_CONFIG_REQ, MALIDP550_DC_CONTROL);
	while (count) {
		status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
		if ((status & MALIDP550_DC_CONFIG_REQ) == MALIDP550_DC_CONFIG_REQ)
			break;
		usleep_range(100, 1000);
		count--;
	}
	WARN(count == 0, "timeout while entering config mode");
}

void malidp550_leave_config_mode(struct malidp_hw_device *hwdev)
{
	u32 status, count = 30;

	malidp_hw_clearbits(hwdev, MALIDP550_DC_CONFIG_REQ, MALIDP550_DC_CONTROL);
	while (count) {
		status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
		if ((status & MALIDP550_DC_CONFIG_REQ) == 0)
			break;
		usleep_range(100, 1000);
		count--;
	}
	WARN(count == 0, "timeout while leaving config mode");
}

bool malidp550_in_config_mode(struct malidp_hw_device *hwdev)
{
	u32 status;

	status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
	if ((status & MALIDP550_DC_CONFIG_REQ) == MALIDP550_DC_CONFIG_REQ)
		return true;

	return false;
}

void malidp550_set_config_valid(struct malidp_hw_device *hwdev)
{
	malidp_hw_setbits(hwdev, MALIDP_CFG_VALID, MALIDP550_CONFIG_VALID);
}

void malidp550_modeset(struct malidp_hw_device *hwdev, struct videomode *mode)
{
	u32 val = MALIDP_DE_DEFAULT_PREFETCH_START;

	malidp_hw_write(hwdev, val, MALIDP550_DE_CONTROL);
	/*
	 * Mali-DP550 and Mali-DP650 encode the background color like this:
	 *   - red   @ MALIDP550_DE_BGND_COLOR[23:16]
	 *   - green @ MALIDP550_DE_BGND_COLOR[15:8]
	 *   - blue  @ MALIDP550_DE_BGND_COLOR[7:0]
	 *
	 * We need to truncate the least significant 4 bits from the default
	 * MALIDP_BGND_COLOR_x values
	 */
	val = (((MALIDP_BGND_COLOR_R >> 4) & 0xff) << 16) |
	      (((MALIDP_BGND_COLOR_G >> 4) & 0xff) << 8) |
	      ((MALIDP_BGND_COLOR_B >> 4) & 0xff);
	malidp_hw_write(hwdev, val, MALIDP550_DE_BGND_COLOR);

	val = MALIDP_DE_H_FRONTPORCH(mode->hfront_porch) |
		MALIDP_DE_H_BACKPORCH(mode->hback_porch);
	malidp_hw_write(hwdev, val, MALIDP550_TIMINGS_BASE + MALIDP_DE_H_TIMINGS);

	val = MALIDP550_DE_V_FRONTPORCH(mode->vfront_porch) |
		MALIDP_DE_V_BACKPORCH(mode->vback_porch);
	malidp_hw_write(hwdev, val, MALIDP550_TIMINGS_BASE + MALIDP_DE_V_TIMINGS);

	val = MALIDP_DE_H_SYNCWIDTH(mode->hsync_len) |
		MALIDP_DE_V_SYNCWIDTH(mode->vsync_len);
	if (mode->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		val |= MALIDP550_HSYNCPOL;
	if (mode->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		val |= MALIDP550_VSYNCPOL;
	malidp_hw_write(hwdev, val, MALIDP550_TIMINGS_BASE + MALIDP_DE_SYNC_WIDTH);

	val = MALIDP_DE_H_ACTIVE(mode->hactive) | MALIDP_DE_V_ACTIVE(mode->vactive);
	malidp_hw_write(hwdev, val, MALIDP550_TIMINGS_BASE + MALIDP_DE_HV_ACTIVE);
}

int malidp650_query_configuration(struct malidp_hw_device *hwdev)
{
	u32 conf = malidp_hw_read(hwdev, MALIDP550_CONFIG_ID);
	u8 ln_size = (conf >> 4) & 0x3;

	if (conf & MALIDP_HW_FEATURE_DS)
		hwdev->features |= MALIDP_HW_FEATURE_DS;

	hwdev->min_line_size = 4;

	switch (ln_size) {
	case 0:
	case 2:
		/* reserved values */
		hwdev->max_line_size = 0;
		return -EINVAL;
	case 1:
		hwdev->max_line_size = SZ_4K;
		break;
	case 3:
		hwdev->max_line_size = 2560;
	}

	return 0;
}

const struct malidp_hw_device malidp_device[MALIDP_MAX_DEVICES] = {
	[MALIDP_500] = {
		.map = {
			.se_base = MALIDP500_SE_BASE,
			.dc_base = MALIDP500_DC_BASE,
			.de_irq_map = {
				.irq_mask = MALIDP_DE_IRQ_UNDERRUN |
					    MALIDP500_DE_IRQ_AXI_ERR |
					    MALIDP500_DE_IRQ_VSYNC |
					    MALIDP500_DE_IRQ_GLOBAL,
				.vsync_irq = MALIDP500_DE_IRQ_VSYNC,
			},
			.se_irq_map = {
				.irq_mask = MALIDP500_SE_IRQ_CONF_MODE,
				.vsync_irq = 0,
			},
			.dc_irq_map = {
				.irq_mask = MALIDP500_DE_IRQ_CONF_VALID,
				.vsync_irq = MALIDP500_DE_IRQ_CONF_VALID,
			},
			.layers = malidp500_layers,
			.n_layers = ARRAY_SIZE(malidp500_layers),
			.input_formats = malidp500_de_formats,
			.n_input_formats = ARRAY_SIZE(malidp500_de_formats),
			.out_depth_base = MALIDP500_OUTPUT_DEPTH,
			.features = 0,	/* no CLEARIRQ register */
		},
		.query_configuration = malidp500_query_configuration,
		.enter_config_mode = malidp500_enter_config_mode,
		.leave_config_mode = malidp500_leave_config_mode,
		.in_config_mode = malidp500_in_config_mode,
		.set_config_valid = malidp500_set_config_valid,
		.modeset = malidp500_modeset,
	},
	[MALIDP_550] = {
		.map = {
			.se_base = MALIDP550_SE_BASE,
			.dc_base = MALIDP550_DC_BASE,
			.de_irq_map = {
				.irq_mask = MALIDP_DE_IRQ_UNDERRUN |
					    MALIDP550_DE_IRQ_VSYNC,
				.vsync_irq = MALIDP550_DE_IRQ_VSYNC,
			},
			.se_irq_map = {
				.irq_mask = MALIDP550_SE_IRQ_EOW |
					    MALIDP550_SE_IRQ_AXI_ERR,
			},
			.dc_irq_map = {
				.irq_mask = MALIDP550_DC_IRQ_CONF_VALID,
				.vsync_irq = MALIDP550_DC_IRQ_CONF_VALID,
			},
			.layers = malidp550_layers,
			.n_layers = ARRAY_SIZE(malidp550_layers),
			.input_formats = malidp550_de_formats,
			.n_input_formats = ARRAY_SIZE(malidp550_de_formats),
			.out_depth_base = MALIDP550_DE_OUTPUT_DEPTH,
			.features = MALIDP_REGMAP_HAS_CLEARIRQ,
		},
		.query_configuration = malidp500_query_configuration,
		.enter_config_mode = malidp550_enter_config_mode,
		.leave_config_mode = malidp550_leave_config_mode,
		.in_config_mode = malidp550_in_config_mode,
		.set_config_valid = malidp550_set_config_valid,
		.modeset = malidp550_modeset,
	},
	[MALIDP_650] = {
		.map = {
			.se_base = MALIDP550_SE_BASE,
			.dc_base = MALIDP550_DC_BASE,
			.de_irq_map = {
				.irq_mask = MALIDP_DE_IRQ_UNDERRUN |
					    MALIDP650_DE_IRQ_DRIFT |
					    MALIDP550_DE_IRQ_VSYNC,
				.vsync_irq = MALIDP550_DE_IRQ_VSYNC,
			},
			.se_irq_map = {
				.irq_mask = MALIDP550_SE_IRQ_EOW |
					    MALIDP550_SE_IRQ_AXI_ERR,
			},
			.dc_irq_map = {
				.irq_mask = MALIDP550_DC_IRQ_CONF_VALID,
				.vsync_irq = MALIDP550_DC_IRQ_CONF_VALID,
			},
			.layers = malidp550_layers,
			.n_layers = ARRAY_SIZE(malidp550_layers),
			.input_formats = malidp650_de_formats,
			.n_input_formats = ARRAY_SIZE(malidp650_de_formats),
			.out_depth_base = MALIDP550_DE_OUTPUT_DEPTH,
			.features = MALIDP_REGMAP_HAS_CLEARIRQ,
		},
		.query_configuration = malidp650_query_configuration,
		.enter_config_mode = malidp550_enter_config_mode,
		.leave_config_mode = malidp550_leave_config_mode,
		.in_config_mode = malidp550_in_config_mode,
		.set_config_valid = malidp550_set_config_valid,
		.modeset = malidp550_modeset,
	},
};

u8 malidp_hw_get_format_id(const struct malidp_hw_regmap *map,
			   u8 layer_id, u32 format)
{
	u8 i;

	for (i = 0; i < map->n_input_formats; i++) {
		if (((map->input_formats[i].layer & layer_id) == layer_id) &&
		    (map->input_formats[i].format == format))
			return map->input_formats[i].id;
	}

	return (u8)-1;
}


u32 malidp_hw_read(struct malidp_hw_device *hwdev, u32 reg)
{
	u32 value = readl(hwdev->regs + reg);
	return value;
}

void malidp_hw_write(struct malidp_hw_device *hwdev, u32 value, u32 reg)
{
	writel(value, hwdev->regs + reg);
}

void malidp_hw_setbits(struct malidp_hw_device *hwdev, u32 mask, u32 reg)
{
	u32 data = malidp_hw_read(hwdev, reg);
	data |= mask;
	malidp_hw_write(hwdev, data, reg);
}

void malidp_hw_clearbits(struct malidp_hw_device *hwdev, u32 mask, u32 reg)
{
	u32 data = malidp_hw_read(hwdev, reg);
	data &= ~mask;
	malidp_hw_write(hwdev, data, reg);
}

void malidp_hw_clear_irq(struct malidp_hw_device *hwdev, u8 block, u32 irq)
{
	u32 base = 0;

	switch (block) {
	case MALIDP_DE_BLOCK:
		base = 0;
		break;
	case MALIDP_SE_BLOCK:
		base = hwdev->map.se_base;
		break;
	case MALIDP_DC_BLOCK:
		base = hwdev->map.dc_base;
		break;
	}

	if (hwdev->map.features & MALIDP_REGMAP_HAS_CLEARIRQ)
		malidp_hw_write(hwdev, irq, base + MALIDP_REG_CLEARIRQ);
	else
		malidp_hw_write(hwdev, irq, base + MALIDP_REG_STATUS);
}

void malidp_hw_disable_irq(struct malidp_hw_device *hwdev, u8 block, u32 irq)
{
	u32 base = 0;

	switch (block) {
	case MALIDP_DE_BLOCK:
		base = 0;
		break;
	case MALIDP_SE_BLOCK:
		base = hwdev->map.se_base;
		break;
	case MALIDP_DC_BLOCK:
		base = hwdev->map.dc_base;
		break;
	}

	malidp_hw_clearbits(hwdev, irq, base + MALIDP_REG_MASKIRQ);
}

void malidp_hw_enable_irq(struct malidp_hw_device *hwdev, u8 block, u32 irq)
{
	u32 base = 0;

	switch (block) {
	case MALIDP_DE_BLOCK:
		base = 0;
		break;
	case MALIDP_SE_BLOCK:
		base = hwdev->map.se_base;
		break;
	case MALIDP_DC_BLOCK:
		base = hwdev->map.dc_base;
		break;
	}

	malidp_hw_setbits(hwdev, irq, base + MALIDP_REG_MASKIRQ);
}

static irqreturn_t malidp_de_irq(int irq, void *arg)
{
	struct drm_device *drm = arg;
	struct malidp_drm *malidp = drm->dev_private;
	struct malidp_hw_device *hwdev;
	const struct malidp_irq_map *de;
	u32 status, mask, dc_status;
	irqreturn_t ret = IRQ_NONE;

	if (!drm->dev_private)
		return IRQ_HANDLED;

	hwdev = malidp->dev;
	de = &hwdev->map.de_irq_map;

	/* first handle the config valid IRQ */
	dc_status = malidp_hw_read(hwdev, hwdev->map.dc_base + MALIDP_REG_STATUS);
	if (dc_status & hwdev->map.dc_irq_map.vsync_irq) {
		/* we have a page flip event */
		atomic_set(&malidp->config_valid, 1);
		malidp_hw_clear_irq(hwdev, MALIDP_DC_BLOCK, dc_status);
		ret = IRQ_WAKE_THREAD;
	}

	status = malidp_hw_read(hwdev, MALIDP_REG_STATUS);
	if (!(status & de->irq_mask))
		return ret;

	mask = malidp_hw_read(hwdev, MALIDP_REG_MASKIRQ);
	status &= mask;
	if (status & de->vsync_irq)
		drm_crtc_handle_vblank(&malidp->crtc);

	malidp_hw_clear_irq(hwdev, MALIDP_DE_BLOCK, status);

	return (ret == IRQ_NONE) ? IRQ_HANDLED : ret;
}

static irqreturn_t malidp_de_irq_thread_handler(int irq, void *arg)
{
	struct drm_device *drm = arg;
	struct malidp_drm *malidp = drm->dev_private;

	wake_up(&malidp->wq);

	return IRQ_HANDLED;
}

int malidp_de_irq_init(struct drm_device *drm, int irq)
{
	struct malidp_drm *malidp = drm->dev_private;
	struct malidp_hw_device *hwdev = malidp->dev;
	int ret;

	/* ensure interrupts are disabled */
	malidp_hw_disable_irq(hwdev, MALIDP_DC_BLOCK, ~0);
	malidp_hw_clear_irq(hwdev, MALIDP_DC_BLOCK, ~0);
	malidp_hw_disable_irq(hwdev, MALIDP_DE_BLOCK, ~0);
	malidp_hw_clear_irq(hwdev, MALIDP_DE_BLOCK, ~0);

	ret = devm_request_threaded_irq(drm->dev, irq, malidp_de_irq,
					malidp_de_irq_thread_handler,
					IRQF_SHARED, "malidp-de", drm);
	if (ret < 0) {
		DRM_ERROR("failed to install DE IRQ handler\n");
		return ret;
	}

	/* first enable the DC block IRQs */
	malidp_hw_enable_irq(hwdev, MALIDP_DC_BLOCK,
			     hwdev->map.dc_irq_map.irq_mask);

	/* now enable the DE block IRQs */
	malidp_hw_enable_irq(hwdev, MALIDP_DE_BLOCK,
			     hwdev->map.de_irq_map.irq_mask);

	return 0;
}

void malidp_de_irq_cleanup(struct drm_device *drm)
{
	struct malidp_drm *malidp = drm->dev_private;
	struct malidp_hw_device *hwdev = malidp->dev;

	malidp_hw_disable_irq(hwdev, MALIDP_DE_BLOCK,
			      hwdev->map.de_irq_map.irq_mask);
	malidp_hw_disable_irq(hwdev, MALIDP_DC_BLOCK,
			      hwdev->map.dc_irq_map.irq_mask);
}

static irqreturn_t malidp_se_irq(int irq, void *arg)
{
	struct drm_device *drm = arg;
	struct malidp_drm *malidp = drm->dev_private;
	struct malidp_hw_device *hwdev = malidp->dev;
	u32 status, mask;

	status = malidp_hw_read(hwdev, hwdev->map.se_base + MALIDP_REG_STATUS);
	if (!(status & hwdev->map.se_irq_map.irq_mask))
		return IRQ_NONE;

	mask = malidp_hw_read(hwdev, hwdev->map.se_base + MALIDP_REG_MASKIRQ);
	status = malidp_hw_read(hwdev, hwdev->map.se_base + MALIDP_REG_STATUS);
	status &= mask;
	/* ToDo: status decoding and firing up of VSYNC and page flip events */

	malidp_hw_clear_irq(hwdev, MALIDP_SE_BLOCK, status);
	/* return IRQ_WAKE_THREAD; */
	return IRQ_HANDLED;
}

static irqreturn_t malidp_se_irq_thread_handler(int irq, void *arg)
{
	return IRQ_HANDLED;
}

int malidp_se_irq_init(struct drm_device *drm, int irq)
{
	struct malidp_drm *malidp = drm->dev_private;
	struct malidp_hw_device *hwdev = malidp->dev;
	int ret;

	/* ensure interrupts are disabled */
	malidp_hw_disable_irq(hwdev, MALIDP_SE_BLOCK, ~0);
	malidp_hw_clear_irq(hwdev, MALIDP_SE_BLOCK, ~0);

	ret = devm_request_threaded_irq(drm->dev, irq, malidp_se_irq,
					malidp_se_irq_thread_handler,
					IRQF_SHARED, "malidp-se", drm);
	if (ret < 0) {
		DRM_ERROR("failed to install SE IRQ handler\n");
		return ret;
	}

	malidp_hw_enable_irq(hwdev, MALIDP_SE_BLOCK,
			     hwdev->map.se_irq_map.irq_mask);

	return 0;
}

void malidp_se_irq_cleanup(struct drm_device *drm)
{
	struct malidp_drm *malidp = drm->dev_private;
	struct malidp_hw_device *hwdev = malidp->dev;

	malidp_hw_disable_irq(hwdev, MALIDP_SE_BLOCK,
			      hwdev->map.se_irq_map.irq_mask);
}
