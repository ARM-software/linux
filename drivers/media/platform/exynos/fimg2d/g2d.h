/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos Scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef G2D__H_
#define G2D__H_

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/io.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#include <linux/exynos_iovmm.h>
#include <linux/sizes.h>

#include <plat/fimg2d.h>

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif defined(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
extern const struct g2d_vb2 g2d_vb2_cma;
#elif defined(CONFIG_VIDEOBUF2_ION)
extern const struct g2d_vb2 g2d_vb2_ion;
#endif

extern int g2d_log_level;
#define g2d_dbg(fmt, args...)						\
	do {								\
		if (g2d_log_level)					\
			printk(KERN_INFO "[%s:%d] "			\
			fmt, __func__, __LINE__, ##args);		\
	} while (0)


/* #define G2D_PERF */

#define MODULE_NAME		"s5p-fimg2d"

#define G2D_MAX_PBUF		2
#define G2D_TIMEOUT		(2 * HZ)	/* 2 seconds */
#define G2D_MAX_CTRL_NUM		11

/* G2D flip direction */
#define G2D_VFLIP	(1 << 1)
#define G2D_HFLIP	(1 << 2)

/* G2D m2m context state */
#define CTX_PARAMS	1
#define CTX_STREAMING	2
#define CTX_RUN		3
#define CTX_ABORT	4
#define CTX_SRC_FMT	5
#define CTX_DST_FMT	6

/* G2D hardware device state */
#define DEV_RUN		1
#define DEV_SUSPEND	2

/* G2D m2m context state */
#define CTX_PARAMS	1
#define CTX_STREAMING	2
#define CTX_RUN		3
#define CTX_ABORT	4
#define CTX_SRC_FMT	5
#define CTX_DST_FMT	6

/* CSC equation */
#define G2D_CSC_NARROW	0
#define G2D_CSC_WIDE	1
#define G2D_CSC_601	0
#define G2D_CSC_709	1

/* G2D CCI enable/disable */
#define G2D_CCI_OFF	0
#define G2D_CCI_ON	1

#define fh_to_g2d_ctx(__fh)	container_of(__fh, struct g2d_ctx, fh)

#define g2d_fmt_is_yuv422(x)	((x == V4L2_PIX_FMT_YUYV) || \
		(x == V4L2_PIX_FMT_UYVY) || (x == V4L2_PIX_FMT_YVYU) || \
		(x == V4L2_PIX_FMT_YUV422P) || (x == V4L2_PIX_FMT_NV16) || \
		(x == V4L2_PIX_FMT_NV61))
#define g2d_fmt_is_yuv420(x)	((x == V4L2_PIX_FMT_YUV420) || \
		(x == V4L2_PIX_FMT_YVU420) || (x == V4L2_PIX_FMT_NV12) || \
		(x == V4L2_PIX_FMT_NV21) || (x == V4L2_PIX_FMT_NV12M) || \
		(x == V4L2_PIX_FMT_NV21M) || (x == V4L2_PIX_FMT_YUV420M) || \
		(x == V4L2_PIX_FMT_YVU420M) || (x == V4L2_PIX_FMT_NV12MT_16X16))

#ifdef CONFIG_VIDEOBUF2_ION
#define g2d_buf_sync_prepare vb2_ion_buf_prepare
#define g2d_buf_sync_finish vb2_ion_buf_finish
#else
int g2d_buf_sync_finish(struct vb2_buffer *vb);
int g2d_buf_sync_prepare(struct vb2_buffer *vb);
#endif

/**
 * @ADDR_PHYS: physical address
 * @ADDR_USER: user virtual address (physically Non-contiguous)
 * @ADDR_USER_CONTIG: user virtual address (physically Contiguous)
 * @ADDR_DEVICE: specific device virtual address
 */
enum addr_space {
	ADDR_NONE,
	ADDR_PHYS,
	ADDR_KERN,
	ADDR_USER,
	ADDR_USER_CONTIG,
	ADDR_DEVICE,
};


/**
 * @IMG_MEMORY: read from external memory
 * @IMG_FGCOLOR: read from foreground color
 * @IMG_BGCOLOR: read from background color
 */
enum image_sel {
	IMG_MEMORY,
	IMG_FGCOLOR,
	IMG_BGCOLOR,
};

/**
 * @FORWARD_ADDRESSING: read data in forward direction
 * @REVERSE_ADDRESSING: read data in reverse direction
 */
enum addressing {
	FORWARD_ADDRESSING,
	REVERSE_ADDRESSING,
};

/**
 * The other addressing modes can cause data corruption,
 * if src and dst are overlapped.
 */
enum dir_addressing {
	UP_FORWARD,
	DOWN_REVERSE,
	LEFT_FORWARD,
	RIGHT_REVERSE,
	VALID_ADDRESSING_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum g2d_max_burst_len {
	MAX_BURST_2 = 0,
	MAX_BURST_4,
	MAX_BURST_8,	/* initial value */
	MAX_BURST_16,
};

#define DEFAULT_MAX_BURST_LEN		MAX_BURST_8

/**
 * mask operation type for 16-bpp, 32-bpp mask image
 * @MSK_ALPHA: use mask alpha for src argb
 * @MSK_ARGB: use mask argb for src argb
 * @MSK_MIXED: use mask alpha for src alpha and mask rgb for src rgb
 */
enum mask_opr {
	MSK_ALPHA,	/* initial value */
	MSK_ARGB,
	MSK_MIXED,
};

#define DEFAULT_MSK_OPR		MSK_ALPHA

/**
 * @ALPHA_PERPIXEL: perpixel alpha
 * @ALPHA_PERPIXEL_SUM_GLOBAL: perpixel + global
 * @ALPHA_PERPIXEL_MUL_GLOBAL: perpixel x global
 *
 * DO NOT CHANGE THIS ORDER
 */
enum alpha_opr {
	ALPHA_PERPIXEL = 0,	/* initial value */
	ALPHA_PERPIXEL_SUM_GLOBAL,
	ALPHA_PERPIXEL_MUL_GLOBAL,
};

#define DEFAULT_ALPHA_OPR	ALPHA_PERPIXEL

/**
 * sampling policy at boundary for bilinear scaling
 * @FOLLOW_REPEAT_MODE: sampling 1 or 2 pixels within bounds
 * @IGNORE_REPEAT_MODE: sampling 4 pixels according to repeat mode
 */
enum boundary_sampling_policy {
	FOLLOW_REPEAT_MODE,
	IGNORE_REPEAT_MODE,
};

#define DEFAULT_BOUNDARY_SAMPLING	FOLLOW_REPEAT_MODE

/**
 * @COEFF_ONE: 1
 * @COEFF_ZERO: 0
 * @COEFF_SA: src alpha
 * @COEFF_SC: src color
 * @COEFF_DA: dst alpha
 * @COEFF_DC: dst color
 * @COEFF_GA: global(constant) alpha
 * @COEFF_GC: global(constant) color
 * @COEFF_DISJ_S:
 * @COEFF_DISJ_D:
 * @COEFF_CONJ_S:
 * @COEFF_CONJ_D:
 *
 * DO NOT CHANGE THIS ORDER
 */
enum fimg2d_coeff {
	COEFF_ONE = 0,
	COEFF_ZERO,
	COEFF_SA,
	COEFF_SC,
	COEFF_DA,
	COEFF_DC,
	COEFF_GA,
	COEFF_GC,
	COEFF_DISJ_S,
	COEFF_DISJ_D,
	COEFF_CONJ_S,
	COEFF_CONJ_D,
};

/**
 * @PREMULT_ROUND_0: (A*B) >> 8
 * @PREMULT_ROUND_1: (A+1)*B) >> 8
 * @PREMULT_ROUND_2: (A+(A>>7))* B) >> 8
 * @PREMULT_ROUND_3: TMP= A*8 + 0x80, (TMP + (TMP >> 8)) >> 8
 *
 * DO NOT CHANGE THIS ORDER
 */
enum premult_round {
	PREMULT_ROUND_0 = 0,
	PREMULT_ROUND_1,
	PREMULT_ROUND_2,
	PREMULT_ROUND_3,	/* initial value */
};

#define DEFAULT_PREMULT_ROUND_MODE	PREMULT_ROUND_3

/**
 * @BLEND_ROUND_0: (A+1)*B) >> 8
 * @BLEND_ROUND_1: (A+(A>>7))* B) >> 8
 * @BLEND_ROUND_2: TMP= A*8 + 0x80, (TMP + (TMP >> 8)) >> 8
 * @BLEND_ROUND_3: TMP= (A*B + C*D + 0x80), (TMP + (TMP >> 8)) >> 8
 *
 * DO NOT CHANGE THIS ORDER
 */
enum blend_round {
	BLEND_ROUND_0 = 0,
	BLEND_ROUND_1,
	BLEND_ROUND_2,
	BLEND_ROUND_3,	/* initial value */
};

#define DEFAULT_BLEND_ROUND_MODE	BLEND_ROUND_3

struct g2d_blend_coeff {
	bool s_coeff_inv;
	enum fimg2d_coeff s_coeff;
	bool d_coeff_inv;
	enum fimg2d_coeff d_coeff;
};

/* -------------------------------------------------- */

enum g2d_csc_idx {
	NO_CSC,
	CSC_Y2R,
	CSC_R2Y,
};

enum g2d_clk_status {
	G2D_CLK_ON,
	G2D_CLK_OFF,
};

enum g2d_clocks {
	G2D_GATE_CLK,
	G2D_CHLD1_CLK,
	G2D_PARN1_CLK,
	G2D_CHLD2_CLK,
	G2D_PARN2_CLK,
};

enum g2d_color_fmt {
	G2D_COLOR_RGB = 0x10,
	G2D_COLOR_YUV = 0x20,
};

enum g2d_dith {
	G2D_DITH_NO,
	G2D_DITH_8BIT,
	G2D_DITH_6BIT,
	G2D_DITH_5BIT,
	G2D_DITH_4BIT,
};

/*
 * blending operation
 * The order is from Android PorterDuff.java
 */
enum g2d_blend_op {
	/* [0, 0] */
	BL_OP_CLR = 1,
	/* [Sa, Sc] */
	BL_OP_SRC,
	/* [Da, Dc] */
	BL_OP_DST,
	/* [Sa + (1 - Sa)*Da, Rc = Sc + (1 - Sa)*Dc] */
	BL_OP_SRC_OVER,
	/* [Sa + (1 - Sa)*Da, Rc = Dc + (1 - Da)*Sc] */
	BL_OP_DST_OVER,
	/* [Sa * Da, Sc * Da] */
	BL_OP_SRC_IN,
	/* [Sa * Da, Sa * Dc] */
	BL_OP_DST_IN,
	/* [Sa * (1 - Da), Sc * (1 - Da)] */
	BL_OP_SRC_OUT,
	/* [Da * (1 - Sa), Dc * (1 - Sa)] */
	BL_OP_DST_OUT,
	/* [Da, Sc * Da + (1 - Sa) * Dc] */
	BL_OP_SRC_ATOP,
	/* [Sa, Sc * (1 - Da) + Sa * Dc ] */
	BL_OP_DST_ATOP,
	/* [-(Sa * Da), Sc * (1 - Da) + (1 - Sa) * Dc] */
	BL_OP_XOR,
	/* [Sa + Da - Sa*Da, Sc*(1 - Da) + Dc*(1 - Sa) + min(Sc, Dc)] */
	BL_OP_DARKEN,
	/* [Sa + Da - Sa*Da, Sc*(1 - Da) + Dc*(1 - Sa) + max(Sc, Dc)] */
	BL_OP_LIGHTEN,
	/** [Sa * Da, Sc * Dc] */
	BL_OP_MULTIPLY,
	/* [Sa + Da - Sa * Da, Sc + Dc - Sc * Dc] */
	BL_OP_SCREEN,
	/* Saturate(S + D) */
	BL_OP_ADD,
	BL_OP_SOLID_FILL,
	/* TODO */
	BL_OP_USER_COEFF,
};

/**
 * Pixel order complies with little-endian style
 *
 * DO NOT CHANGE THIS ORDER
 */
enum pixel_order {
	AX_RGB = 0,
	RGB_AX,
	AX_BGR,
	BGR_AX,
	ARGB_ORDER_END,

	P1_CRY1CBY0,
	P1_CBY1CRY0,
	P1_Y1CRY0CB,
	P1_Y1CBY0CR,
	P1_ORDER_END,

	P2_CRCB,
	P2_CBCR,
	P2_ORDER_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum color_format {
	CF_XRGB_8888 = 0,
	CF_ARGB_8888,
	CF_RGB_565,
	CF_XRGB_1555,
	CF_ARGB_1555,
	CF_XRGB_4444,
	CF_ARGB_4444,
	CF_RGB_888,
	CF_YCBCR_444,
	CF_YCBCR_422,
	CF_YCBCR_420,
	CF_A8,
	CF_L8,
	SRC_DST_FORMAT_END,

	CF_MSK_1BIT,
	CF_MSK_4BIT,
	CF_MSK_8BIT,
	CF_MSK_16BIT_565,
	CF_MSK_16BIT_1555,
	CF_MSK_16BIT_4444,
	CF_MSK_32BIT_8888,
	MSK_FORMAT_END,
};

enum rotation {
	ORIGIN,
	ROT_90,	/* clockwise */
	ROT_180,
	ROT_270,
	XFLIP,	/* x-axis flip */
	YFLIP,	/* y-axis flip */
};

/**
 * @NO_REPEAT: no effect
 * @REPEAT_NORMAL: repeat horizontally and vertically
 * @REPEAT_PAD: pad with pad color
 * @REPEAT_REFLECT: reflect horizontally and vertically
 * @REPEAT_CLAMP: pad with edge color of original image
 *
 * DO NOT CHANGE THIS ORDER
 */
enum repeat {
	NO_REPEAT = 0,
	REPEAT_NORMAL,	/* default setting */
	REPEAT_PAD,
	REPEAT_REFLECT, REPEAT_MIRROR = REPEAT_REFLECT,
	REPEAT_CLAMP,
};

enum scaling {
	NO_SCALING,
	SCALING_NEAREST,
	SCALING_BILINEAR,
};

/**
 * premultiplied alpha
 */
enum premultiplied {
	PREMULTIPLIED,
	NON_PREMULTIPLIED,
};

/**
 * @TRANSP: discard bluescreen color
 * @BLUSCR: replace bluescreen color with background color
 */
enum bluescreen {
	OPAQUE,
	TRANSP,
	BLUSCR,
	BS_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum blit_op {
	BLIT_OP_SOLID_FILL = 0,

	BLIT_OP_CLR,
	BLIT_OP_SRC, BLIT_OP_SRC_COPY = BLIT_OP_SRC,
	BLIT_OP_DST,
	BLIT_OP_SRC_OVER,
	BLIT_OP_DST_OVER, BLIT_OP_OVER_REV = BLIT_OP_DST_OVER,
	BLIT_OP_SRC_IN,
	BLIT_OP_DST_IN, BLIT_OP_IN_REV = BLIT_OP_DST_IN,
	BLIT_OP_SRC_OUT,
	BLIT_OP_DST_OUT, BLIT_OP_OUT_REV = BLIT_OP_DST_OUT,
	BLIT_OP_SRC_ATOP,
	BLIT_OP_DST_ATOP, BLIT_OP_ATOP_REV = BLIT_OP_DST_ATOP,
	BLIT_OP_XOR,

	BLIT_OP_ADD,
	BLIT_OP_MULTIPLY,
	BLIT_OP_SCREEN,
	BLIT_OP_DARKEN,
	BLIT_OP_LIGHTEN,

	BLIT_OP_DISJ_SRC_OVER,
	BLIT_OP_DISJ_DST_OVER, BLIT_OP_SATURATE = BLIT_OP_DISJ_DST_OVER,
	BLIT_OP_DISJ_SRC_IN,
	BLIT_OP_DISJ_DST_IN, BLIT_OP_DISJ_IN_REV = BLIT_OP_DISJ_DST_IN,
	BLIT_OP_DISJ_SRC_OUT,
	BLIT_OP_DISJ_DST_OUT, BLIT_OP_DISJ_OUT_REV = BLIT_OP_DISJ_DST_OUT,
	BLIT_OP_DISJ_SRC_ATOP,
	BLIT_OP_DISJ_DST_ATOP, BLIT_OP_DISJ_ATOP_REV = BLIT_OP_DISJ_DST_ATOP,
	BLIT_OP_DISJ_XOR,

	BLIT_OP_CONJ_SRC_OVER,
	BLIT_OP_CONJ_DST_OVER, BLIT_OP_CONJ_OVER_REV = BLIT_OP_CONJ_DST_OVER,
	BLIT_OP_CONJ_SRC_IN,
	BLIT_OP_CONJ_DST_IN, BLIT_OP_CONJ_IN_REV = BLIT_OP_CONJ_DST_IN,
	BLIT_OP_CONJ_SRC_OUT,
	BLIT_OP_CONJ_DST_OUT, BLIT_OP_CONJ_OUT_REV = BLIT_OP_CONJ_DST_OUT,
	BLIT_OP_CONJ_SRC_ATOP,
	BLIT_OP_CONJ_DST_ATOP, BLIT_OP_CONJ_ATOP_REV = BLIT_OP_CONJ_DST_ATOP,
	BLIT_OP_CONJ_XOR,

	/* user select coefficient manually */
	BLIT_OP_USER_COEFF,

	BLIT_OP_USER_SRC_GA,

	/* Add new operation type here */

	/* end of blit operation */
	BLIT_OP_END,
};
#define MAX_FIMG2D_BLIT_OP	((int)BLIT_OP_END)

/**
 * @TMP: temporary buffer for 2-step blit at a single command
 *
 * DO NOT CHANGE THIS ORDER
 */
enum image_object {
	IMAGE_SRC = 0,
	IMAGE_MSK,
	IMAGE_TMP,
	IMAGE_DST,
	IMAGE_END,
};
#define MAX_IMAGES		IMAGE_END
#define ISRC			IMAGE_SRC
#define IMSK			IMAGE_MSK
#define ITMP			IMAGE_TMP
#define IDST			IMAGE_DST

/**
 * @size: dma size of image
 * @cached: cached dma size of image
 */
struct fimg2d_dma {
	unsigned long addr;
	size_t size;
	size_t cached;
};

struct fimg2d_dma_group {
	struct fimg2d_dma base;
	struct fimg2d_dma plane2;
};

/**
 * @start: start address or unique id of image
 */
struct fimg2d_addr {
	enum addr_space type;
	unsigned long start;
};

struct fimg2d_rect {
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2; /* y1 + height */
};

/**
 * pixels can be different from src, dst or clip rect
 */
struct fimg2d_scale {
	enum scaling mode;

	/* ratio in pixels */
	int src_w;
	int src_h;
	int dst_w;
	int dst_h;
};

struct fimg2d_clip {
	bool enable;
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2; /* y1 + height */
};

struct fimg2d_repeat {
	enum repeat mode;
	unsigned long pad_color;
};

/**
 * @bg_color: bg_color is valid only if bluescreen mode is BLUSCR.
 */
struct fimg2d_bluscr {
	enum bluescreen mode;
	unsigned long bs_color;
	unsigned long bg_color;
};

/**
 * @plane2: address info for CbCr in YCbCr 2plane mode
 * @rect: crop/clip rect
 * @need_cacheopr: true if cache coherency is required
 */
struct fimg2d_image {
	int stride;
	enum pixel_order order;
	enum color_format fmt;
	struct fimg2d_addr addr;
	struct fimg2d_addr plane2;
};

struct fimg2d_blend_coeff {
	bool s_coeff_inv;
	enum fimg2d_coeff s_coeff;
	bool d_coeff_inv;
	enum fimg2d_coeff d_coeff;
};

/*
 * struct g2d_size_limit - Scaler variant size information
 *
 * @min_w: minimum pixel width size
 * @min_h: minimum pixel height size
 * @max_w: maximum pixel width size
 * @max_h: maximum pixel height size
 * @align_w: pixel width align
 * @align_h: pixel height align
 */
struct g2d_size_limit {
	u32 min_w;
	u32 min_h;
	u32 max_w;
	u32 max_h;
	u32 align_w;
	u32 align_h;
};

struct g2d_variant {
	struct g2d_size_limit limit_input;
	struct g2d_size_limit limit_output;
	int g2d_up_max;
	int g2d_down_max;
};

/*
 * struct g2d_fmt - the driver's internal color format data
 * @name: format description
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @num_planes: number of physically non-contiguous data planes
 * @num_comp: number of color components(ex. RGB, Y, Cb, Cr)
 * @bitperpixel: bits per pixel
 * @color: the corresponding sc_color_fmt
 */
struct g2d_fmt {
	char	*name;
	u32	pixelformat;
	u16	num_planes;
	u16	num_comp;
	u32	bitperpixel[VIDEO_MAX_PLANES];
	u32	color;
	enum	pixel_order order;
};

struct g2d_addr {
	dma_addr_t	y;
	dma_addr_t	c;
};


/*
 * struct g2d_frame - source/target frame properties
 * @fmt:	buffer format(like virtual screen)
 * @crop:	image size / position
 * @addr:	buffer start address(access using SC_ADDR_XXX)
 * @bytesused:	image size in bytes (w x h x bpp)
 */
struct g2d_frame {
	struct g2d_fmt			*g2d_fmt;
	struct v4l2_pix_format_mplane	pix_mp;
	struct v4l2_rect		crop;
	struct v4l2_rect		clip;
	struct g2d_addr			addr;
	struct fimg2d_repeat		rep;
	unsigned long			bytesused[VIDEO_MAX_PLANES];
	bool				clip_enable;
};


/*
 * struct g2d_m2m_device - v4l2 memory-to-memory device data
 * @v4l2_dev: v4l2 device
 * @vfd: the video device node
 * @m2m_dev: v4l2 memory-to-memory device data
 * @in_use: the open count
 */
struct g2d_m2m_device {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd;
	struct v4l2_m2m_dev	*m2m_dev;
	atomic_t		in_use;
};

struct g2d_wdt {
	struct timer_list	timer;
	atomic_t		cnt;
};

struct g2d_csc {
	bool			csc_mode;
	bool			csc_eq;
	bool			csc_range;
};

struct g2d_ctx;
struct g2d_vb2;

/*
 * struct g2d_dev - the abstraction for G2D device
 * @dev:	pointer to the Rotator device
 * @pdata:	pointer to the device platform data
 * @variant:	the IP variant information
 * @m2m:	memory-to-memory V4L2 device information
 * @id:		G2D device index (0..G2D_MAX_DEVS)
 * @clk:	clk required for g2d operation
 * @regs:	the mapped hardware registers
 * @regs_res:	the resource claimed for IO registers
 * @wait:	interrupt handler waitqueue
 * @ws:		work struct
 * @state:	device state flags
 * @alloc_ctx:	videobuf2 memory allocator context
 * @sc_vb2:	videobuf2 memory allocator callbacks
 * @slock:	the spinlock pscecting this data structure
 * @lock:	the mutex pscecting this data structure
 * @wdt:	watchdog timer information
 * @clk_cnt:	scator clock on/off count
 */
struct g2d_dev {
	struct device			*dev;
	struct fimg2d_platdata		*pdata;
	struct g2d_variant		*variant;
	struct g2d_m2m_device		m2m;
	int				id;
	int				ver;
	struct clk			*clk;
	struct clk			*clk_parn1;
	struct clk			*clk_chld1;
	struct clk			*clk_parn2;
	struct clk			*clk_chld2;
	void __iomem			*regs;
	struct resource			*regs_res;
	wait_queue_head_t		wait;
	unsigned long			state;
	struct vb2_alloc_ctx		*alloc_ctx;
	struct vb2_alloc_ctx		*alloc_ctx_cci;
	const struct g2d_vb2		*vb2;
	spinlock_t			slock;
	struct mutex			lock;
	struct g2d_wdt			wdt;
	atomic_t			clk_cnt;
	unsigned long long		start_time;
	unsigned long long		end_time;
};

/*
 * g2d_ctx - the abstration for G2D open context
 * @g2d_dev:		the G2D device this context applies to
 * @m2m_ctx:		memory-to-memory device context
 * @frame:		source frame properties
 * @ctrl_handler:	v4l2 controls handler
 * @fh:			v4l2 file handle
 * @fence_work:		work struct for sync fence work
 * @fence_wq:		workqueue for sync fence work
 * @fence_wait_list:	wait list for sync fence
 * @rotation:		image clockwise scation in degrees
 * @flip:		image flip mode
 * @bl_op:		image blend mode
 * @dith:		image dithering mode
 * @g_alpha:		global alpha value
 * @color_fill:		color fill value
 * @flags:		context state flags
 * @slock:		spinlock pscecting this data structure
 * @cacheable:		cacheability of current frame
 * @pre_multi:		pre-multiplied format
 * @csc:		csc equation value
 */
struct g2d_ctx {
	struct g2d_dev			*g2d_dev;
	struct v4l2_m2m_ctx		*m2m_ctx;
	struct g2d_frame		s_frame;
	struct g2d_frame		d_frame;
	struct v4l2_ctrl_handler	ctrl_handler;
	struct v4l2_fh			fh;
	struct work_struct		fence_work;
	struct workqueue_struct		*fence_wq;
	struct list_head		fence_wait_list;
	struct fimg2d_repeat		rep;
	struct fimg2d_scale		scale;
	struct fimg2d_bluscr		bluesc;
	int				rotation;
	u32				flip;
	/* enum g2d_blend_op		bl_op; */
	enum blit_op			op;
	u32				dith;
	u32				g_alpha;
	bool				color_fill;
	unsigned int			color;
	unsigned long			flags;
	spinlock_t			slock;
	bool				cacheable;
	bool				pre_multi;
	struct g2d_csc			csc;
	unsigned long			solid_color;
	enum scaling			scale_mode;
	enum repeat			repeat_mode;
	unsigned long			pad_color;
	bool				cci_on;
};

struct g2d_vb2 {
	const struct vb2_mem_ops *ops;
	void *(*init)(struct g2d_dev *g2d, bool cacheable);
	void (*cleanup)(void *alloc_ctx);

	unsigned long (*plane_addr)(struct vb2_buffer *vb, u32 plane_no);

	int (*resume)(void *alloc_ctx);
	void (*suspend)(void *alloc_ctx);

	int (*cache_flush)(struct vb2_buffer *vb, u32 num_planes);
	void (*set_cacheable)(void *alloc_ctx, bool cacheable);
	void (*set_sharable)(void *alloc_ctx, bool sharable);
};

static inline struct g2d_frame *ctx_get_frame(struct g2d_ctx *ctx,
						enum v4l2_buf_type type)
{
	struct g2d_frame *frame;

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			frame = &ctx->s_frame;
		else
			frame = &ctx->d_frame;
	} else {
		dev_err(ctx->g2d_dev->dev,
				"Wrong V4L2 buffer type %d\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

static inline int g2d_ip_ver(struct g2d_dev *g2d)
{
	int ver = 0;

	ver = g2d->pdata->ip_ver;

	return ver;
}

static inline bool is_cci_on(struct g2d_ctx *ctx, unsigned int size)
{
	return (size >= SZ_1M * 0) ? true : false;
}

void g2d_hwset_int_enable(struct g2d_dev *g2d);
void g2d_hwset_int_disable(struct g2d_dev *g2d);
void g2d_hwset_int_clear(struct g2d_dev *g2d);
void g2d_hwset_soft_reset(struct g2d_dev *g2d);
void g2d_hwset_init(struct g2d_dev *g2d);
void g2d_hwset_cci_on(struct g2d_dev *g2d);
void g2d_hwset_start_blit(struct g2d_dev *g2d);
int g2d_hwset_src_image_format(struct g2d_dev *g2d, u32 pixelformat);
int g2d_hwset_dst_image_format(struct g2d_dev *g2d, u32 pixelformat);
void g2d_hwset_src_type(struct g2d_dev *g2d, enum image_sel type);
void g2d_hwset_dst_type(struct g2d_dev *g2d, enum image_sel type);
void g2d_hwset_color_fill(struct g2d_dev *g2d, unsigned long color);
void g2d_hwset_premultiplied(struct g2d_dev *g2d);
void g2d_hwset_pre_multi_format(struct g2d_dev *g2d);
void g2d_hwset_src_addr(struct g2d_dev *g2d, struct g2d_frame *s_frame);
void g2d_hwset_dst_addr(struct g2d_dev *g2d, struct g2d_frame *d_frame);
void g2d_hwset_src_stride(struct g2d_dev *g2d, int stride);
void g2d_hwset_dst_stride(struct g2d_dev *g2d, int stride);
void g2d_hwset_src_rect(struct g2d_dev *g2d, struct v4l2_rect *rect);
void g2d_hwset_dst_rect(struct g2d_dev *g2d, struct v4l2_rect *rect);
void g2d_hwset_bluescreen(struct g2d_dev *g2d, struct fimg2d_bluscr *bluscr);
void g2d_hwset_enable_clipping(struct g2d_dev *g2d, struct v4l2_rect *clip);
void g2d_hwset_enable_dithering(struct g2d_dev *g2d);
void g2d_hwset_src_repeat(struct g2d_dev *g2d, struct fimg2d_repeat *rep);
void g2d_hwset_rotation(struct g2d_dev *g2d, u32 direction, int degree);
void g2d_hwset_fgcolor(struct g2d_dev *g2d, unsigned long fg);
void g2d_hwset_enable_alpha(struct g2d_dev *g2d, unsigned char g_alpha);
void g2d_hwset_alpha_composite(struct g2d_dev *g2d
		, enum blit_op op, unsigned char g_alpha);
void g2d_hwset_src_scaling(struct g2d_dev *g2d
		, struct fimg2d_scale *scl, struct fimg2d_repeat *rep);
void g2d_hwset_dump_regs(struct g2d_dev *g2d);
#endif /* G2D__H_ */
