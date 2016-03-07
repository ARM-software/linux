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

#ifndef __MALIDP_HW_H__
#define __MALIDP_HW_H__

#include <drm/drm_fourcc.h>

struct videomode;
struct clk;

/* Mali DP IP blocks */
enum {
	MALIDP_DE_BLOCK = 0,
	MALIDP_SE_BLOCK,
	MALIDP_DC_BLOCK
};

/* Mali DP layers */
enum {
	DE_VIDEO1 = 1,
	DE_GRAPHICS = 2,
	DE_GRAPHICS2 = 4, /* used only in DP500 */
	DE_VIDEO2 = 4,
	DE_SMART = 8,
};

struct malidp_input_format {
	u8 layer;		/* bitmask of layers supporting it */
	u8 id;			/* used internally */
	u32 format;		/* DRM fourcc */
};

/*
 * hide the differences between register maps
 * by using a common structure to hold the
 * base register offsets
 */

struct malidp_irq_map {
	u32 irq_mask;		/* mask of IRQs that can be enabled in the block */
	u32 vsync_irq;		/* IRQ bit used for signaling during VSYNC */
};

struct malidp_layer {
	u8 id;			/* layer ID */
	u16 base;		/* address offset for the register bank */
	u16 ptr;		/* address offset for the pointer register */
};

/* regmap features */
#define MALIDP_REGMAP_HAS_CLEARIRQ	(1 << 0)

struct malidp_hw_regmap {
	/* address offset of the DE register bank */
	/* is always 0x0000 */
	/* address offset of the SE registers bank */
	const u16 se_base;
	/* address offset of the DC registers bank */
	const u16 dc_base;

	const struct malidp_irq_map de_irq_map;
	const struct malidp_irq_map se_irq_map;
	const struct malidp_irq_map dc_irq_map;

	/* list of supported layers */
	const struct malidp_layer *layers;
	const u8 n_layers;
	/* list of supported input formats for each layer */
	const struct malidp_input_format *input_formats;
	const u8 n_input_formats;

	/* address offset for the output depth register */
	const u16 out_depth_base;

	/* bitmap with register map features */
	const u8 features;
};

/* hardware features */
#define MALIDP_HW_FEATURE_DS		(1 << 0)	/* split screen */

struct malidp_hw_device {
	const struct malidp_hw_regmap map;
	void __iomem *regs;

	/* APB clock */
	struct clk *pclk;
	/* AXI clock */
	struct clk *aclk;
	/* main clock for display core */
	struct clk *mclk;
	/* pixel clock for display core */
	struct clk *pxlclk;

	/*
	 * Validate the driver instance against the hardware bits
	 */
	int  (*query_configuration)(struct malidp_hw_device *hwdev);
	/*
	 * Set the hardware into config mode, ready to accept mode changes
	 */
	void (*enter_config_mode)(struct malidp_hw_device *hwdev);
	/*
	 * Tell hardware to exit configuration mode
	 */
	void (*leave_config_mode)(struct malidp_hw_device *hwdev);
	/*
	 * Query if hardware is in configuration mode
	 */
	bool (*in_config_mode)(struct malidp_hw_device *hwdev);
	/*
	 * Set configuration valid flag for hardware parameters that can
	 * be changed outside the configuration mode. Hardware will use
	 * the new settings when config valid is set after the end of the
	 * current buffer scanout
	 */
	void (*set_config_valid)(struct malidp_hw_device *hwdev);
	/*
	 * Set a new mode in hardware. Requires the hardware to be in
	 * configuration mode before this function is called.
	 */
	void (*modeset)(struct malidp_hw_device *hwdev, struct videomode *m);

	u8 features;

	u8 min_line_size;
	u16 max_line_size;
};

/* Supported variants of the hardware */
enum {
	MALIDP_500 = 0,
	MALIDP_550,
	MALIDP_650,
	/* keep the next entry last */
	MALIDP_MAX_DEVICES
};

extern const struct malidp_hw_device malidp_device[MALIDP_MAX_DEVICES];

u32 malidp_hw_read(struct malidp_hw_device *hwdev, u32 reg);
void malidp_hw_write(struct malidp_hw_device *hwdev, u32 value, u32 reg);
void malidp_hw_setbits(struct malidp_hw_device *hwdev, u32 mask, u32 reg);
void malidp_hw_clearbits(struct malidp_hw_device *hwdev, u32 mask, u32 reg);
void malidp_hw_clear_irq(struct malidp_hw_device *hwdev, u8 block, u32 irq);
void malidp_hw_disable_irq(struct malidp_hw_device *hwdev, u8 block, u32 irq);
void malidp_hw_enable_irq(struct malidp_hw_device *hwdev, u8 block, u32 irq);

int malidp_de_irq_init(struct drm_device *drm, int irq);
int malidp_se_irq_init(struct drm_device *drm, int irq);
void malidp_de_irq_cleanup(struct drm_device *drm);
void malidp_se_irq_cleanup(struct drm_device *drm);

u8 malidp_hw_get_format_id(const struct malidp_hw_regmap *map,
			   u8 layer_id, u32 format);

/* Custom pixel formats */
#define MALIDP_FORMAT_XYUV		fourcc_code('M', 'X', 'Y', 'V') /* [31:0] X:Y:Cb:Cr 8:8:8:8 little endian */
#define MALIDP_FORMAT_VYU30		fourcc_code('M', 'V', '3', '0') /* [31:0] X:Cr:Y:Cb 2:10:10:10 little endian */
#define MALIDP_FORMAT_NV12AFBC		fourcc_code('M', '1', '2', 'A') /* AFBC compressed YUV 4:2:0, 8 bits per component */
#define MALIDP_FORMAT_NV16AFBC		fourcc_code('M', '1', '6', 'A') /* AFBC compressed YUV 4:2:2, 8 bits per component */
#define MALIDP_FORMAT_YUV10_420AFBC	fourcc_code('M', '3', '0', 'A') /* AFBC compressed YUV 4:2:0, 10 bits per component */
#define MALIDP_FORMAT_Y0L2		fourcc_code('Y', '0', 'L', '2') /* YUV 4:2:0, ARM linear 10-bit packed format */
#define MALIDP_FORMAT_Y0L0		fourcc_code('Y', '0', 'L', '0') /* YUV 4:2:0, ARM linear 8-bit packed format */
/* YUV 4:2:0, 10-bit per component, 2 plane.
 * Each sample packed into the top 10 bits of a 16-bit word.
 * Plane 0: [63:0] Y3:x:Y2:x:Y1:x:Y0, 10:6:10:6:10:6:10:6
 * Plane 1: [63:0] V02:x:U02:x:V00:x:U00, 10:6:10:6:10:6:10:6
 */
#define MALIDP_FORMAT_P010		fourcc_code('P', '0', '1', '0')

/* background color components are defined as 12bits values */
#define MALIDP_BGND_COLOR_R		0x000
#define MALIDP_BGND_COLOR_G		0x000
#define MALIDP_BGND_COLOR_B		0x000

#endif  /* __MALIDP_HW_H__ */
