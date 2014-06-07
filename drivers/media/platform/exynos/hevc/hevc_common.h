/*S_BUS_RESET
 * Samsung Exynos HEVC 1.0
 *
 * This file contains definitions of enums and structs used by the codec
 * driver.
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * Kamil Debski, <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version
 */

#ifndef HEVC_COMMON_H_
#define HEVC_COMMON_H_

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>

#include <media/videobuf2-core.h>

#include <mach/exynos-hevc.h>

/*
 * CONFIG_HEVC_USE_BUS_DEVFREQ might be defined in exynos-hevc.h.
 * So pm_qos.h should be checked after exynos-hevc.h
 */
#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
#include <linux/pm_qos.h>
#endif

#define HEVC_MAX_BUFFERS		32
#define HEVC_MAX_PLANES		3
#define HEVC_MAX_DPBS		32
#define HEVC_INFO_INIT_FD	-1

#define HEVC_NUM_CONTEXTS	16
#define HEVC_MAX_DRM_CTX		2

/* Interrupt timeout */
#define HEVC_INT_TIMEOUT		20000
/* Busy wait timeout */
#define HEVC_BW_TIMEOUT		500
/* Watchdog interval */
#define HEVC_WATCHDOG_INTERVAL   1000
/* After how many executions watchdog should assume lock up */
#define HEVC_WATCHDOG_CNT        10

#define HEVC_NO_INSTANCE_SET	-1

#define HEVC_FW_NAME		"hevc_fw.bin"

#define STUFF_BYTE		4

#define FLAG_LAST_FRAME		0x80000000

/**
 * enum hevc_inst_type - The type of an HEVC device node.
 */
enum hevc_node_type {
	HEVCNODE_INVALID = -1,
	HEVCNODE_DECODER = 0,
};

/**
 * enum hevc_inst_type - The type of an HEVC instance.
 */
enum hevc_inst_type {
	HEVCINST_INVALID = 0,
	HEVCINST_DECODER = 1,
};

/**
 * enum hevc_inst_state - The state of an HEVC instance.
 */
enum hevc_inst_state {
	HEVCINST_FREE = 0,
	HEVCINST_INIT = 100,
	HEVCINST_GOT_INST,
	HEVCINST_HEAD_PARSED,
	HEVCINST_BUFS_SET,
	HEVCINST_RUNNING,
	HEVCINST_FINISHING,
	HEVCINST_FINISHED,
	HEVCINST_RETURN_INST,
	HEVCINST_ERROR,
	HEVCINST_ABORT,
	HEVCINST_RES_CHANGE_INIT,
	HEVCINST_RES_CHANGE_FLUSH,
	HEVCINST_RES_CHANGE_END,
	HEVCINST_RUNNING_NO_OUTPUT,
	HEVCINST_ABORT_INST,
	HEVCINST_DPB_FLUSHING,
	HEVCINST_VPS_PARSED_ONLY,
};

/**
 * enum hevc_queue_state - The state of buffer queue.
 */
enum hevc_queue_state {
	QUEUE_FREE = 0,
	QUEUE_BUFS_REQUESTED,
	QUEUE_BUFS_QUERIED,
	QUEUE_BUFS_MMAPED,
};

enum hevc_dec_wait_state {
	WAIT_NONE = 0,
	WAIT_DECODING,
	WAIT_INITBUF_DONE,
};

/**
 * enum hevc_check_state - The state for user notification
 */
enum hevc_check_state {
	HEVCSTATE_PROCESSING = 0,
	HEVCSTATE_DEC_RES_DETECT,
	HEVCSTATE_DEC_TERMINATING,
	HEVCSTATE_DEC_S3D_REALLOC,
};

/**
 * enum hevc_buf_cacheable_mask - The mask for cacheble setting
 */
enum hevc_buf_cacheable_mask {
	HEVCMASK_DST_CACHE = (1 << 0),
	HEVCMASK_SRC_CACHE = (1 << 1),
};

struct hevc_ctx;
struct hevc_extra_buf;

/**
 * struct hevc_buf - HEVC buffer
 *
 */
struct hevc_buf {
	struct vb2_buffer vb;
	struct list_head list;
	union {
		dma_addr_t raw[3];
		dma_addr_t stream;
	} planes;
	int used;
};

#define vb_to_hevc_buf(x)	\
	container_of(x, struct hevc_buf, vb)

struct hevc_pm {
	struct clk	*clock;
	atomic_t	power;
	struct device	*device;
	spinlock_t	clklock;
};

struct hevc_fw {
	const struct firmware	*info;
	int			state;
	int			date;
};

struct hevc_buf_align {
	unsigned int hevc_base_align;
};

struct hevc_buf_size_v6 {
	unsigned int dev_ctx;
	unsigned int dec_ctx;
};

struct hevc_buf_size {
	unsigned int firmware_code;
	unsigned int cpb_buf;
	void *buf;
};

struct hevc_variant {
	struct hevc_buf_size *buf_size;
	struct hevc_buf_align *buf_align;
};

/**
 * struct hevc_extra_buf - represents internal used buffer
 * @alloc:		allocation-specific contexts for each buffer
 *			(videobuf2 allocator)
 * @ofs:		offset of each buffer, will be used for HEVC
 * @virt:		kernel virtual address, only valid when the
 *			buffer accessed by driver
 * @dma:		DMA address, only valid when kernel DMA API used
 */
struct hevc_extra_buf {
	void		*alloc;
	unsigned long	ofs;
	void		*virt;
	dma_addr_t	dma;
};

#define OTO_BUF_FW		(1 << 0)
#define OTO_BUF_COMMON_CTX	(1 << 1)
/**
 * struct hevc_dev - The struct containing driver internal parameters.
 */
struct hevc_dev {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd_dec;
	struct video_device	*vfd_enc;
	struct device		*device;
#ifdef CONFIG_ION_EXYNOS
	struct ion_client	*hevc_ion_client;
#endif

	void __iomem		*regs_base;
	int			irq;
	struct resource		*hevc_mem;

	struct hevc_pm	pm;
	struct hevc_fw	fw;
	struct hevc_variant	*variant;
	struct hevc_platdata	*pdata;

	int num_inst;
	spinlock_t irqlock;
	spinlock_t condlock;

	struct mutex hevc_mutex;

	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;

	size_t port_a;
	size_t port_b;

	unsigned long hw_lock;

	/* For 6.x, Added for SYS_INIT context buffer */
	struct hevc_extra_buf ctx_buf;

	struct hevc_ctx *ctx[HEVC_NUM_CONTEXTS];
	int curr_ctx;
	int preempt_ctx;
	unsigned long ctx_work_bits;

	atomic_t watchdog_cnt;
	atomic_t watchdog_run;
	struct timer_list watchdog_timer;
	struct workqueue_struct *watchdog_wq;
	struct work_struct watchdog_work;

	struct vb2_alloc_ctx **alloc_ctx;

	unsigned long clk_state;

	/* for DRM */
	int curr_ctx_drm;
	int fw_status;
	int num_drm_inst;
	struct hevc_extra_buf drm_info;
	struct vb2_alloc_ctx *alloc_ctx_fw;
	struct vb2_alloc_ctx *alloc_ctx_sh;
	struct vb2_alloc_ctx *alloc_ctx_drm;

	struct workqueue_struct *sched_wq;
	struct work_struct sched_work;

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	struct list_head qos_queue;
	atomic_t qos_req_cur;
	atomic_t *qos_req_cnt;
	struct pm_qos_request qos_req_int;
	struct pm_qos_request qos_req_mif;
	struct pm_qos_request qos_req_cpu;
	struct pm_qos_request qos_req_kfc;

	/* for direct clock control */
	int min_rate;
	int curr_rate;
#endif
};

enum hevc_ctrl_type {
	HEVC_CTRL_TYPE_GET_SRC	= 0x1,
	HEVC_CTRL_TYPE_GET_DST	= 0x2,
	HEVC_CTRL_TYPE_SET	= 0x4,
};

#define	HEVC_CTRL_TYPE_GET	(HEVC_CTRL_TYPE_GET_SRC | HEVC_CTRL_TYPE_GET_DST)
#define	HEVC_CTRL_TYPE_SRC	(HEVC_CTRL_TYPE_SET | HEVC_CTRL_TYPE_GET_SRC)
#define	HEVC_CTRL_TYPE_DST	(HEVC_CTRL_TYPE_GET_DST)

enum hevc_ctrl_mode {
	HEVC_CTRL_MODE_NONE	= 0x0,
	HEVC_CTRL_MODE_SFR	= 0x1,
	HEVC_CTRL_MODE_SHM	= 0x2,
	HEVC_CTRL_MODE_CST	= 0x4,
};

struct hevc_buf_ctrl;

struct hevc_ctrl_cfg {
	enum hevc_ctrl_type type;
	unsigned int id;
	unsigned int is_volatile;	/* only for HEVC_CTRL_TYPE_SET */
	unsigned int mode;
	unsigned int addr;
	unsigned int mask;
	unsigned int shft;
	unsigned int flag_mode;		/* only for HEVC_CTRL_TYPE_SET */
	unsigned int flag_addr;		/* only for HEVC_CTRL_TYPE_SET */
	unsigned int flag_shft;		/* only for HEVC_CTRL_TYPE_SET */
	int (*read_cst) (struct hevc_ctx *ctx,
			struct hevc_buf_ctrl *buf_ctrl);
	void (*write_cst) (struct hevc_ctx *ctx,
			struct hevc_buf_ctrl *buf_ctrl);
};

struct hevc_ctx_ctrl {
	struct list_head list;
	enum hevc_ctrl_type type;
	unsigned int id;
	unsigned int addr;
	int has_new;
	int val;
};

struct hevc_buf_ctrl {
	struct list_head list;
	unsigned int id;
	enum hevc_ctrl_type type;
	int has_new;
	int val;
	unsigned int old_val;		/* only for HEVC_CTRL_TYPE_SET */
	unsigned int is_volatile;	/* only for HEVC_CTRL_TYPE_SET */
	unsigned int updated;
	unsigned int mode;
	unsigned int addr;
	unsigned int mask;
	unsigned int shft;
	unsigned int flag_mode;		/* only for HEVC_CTRL_TYPE_SET */
	unsigned int flag_addr;		/* only for HEVC_CTRL_TYPE_SET */
	unsigned int flag_shft;		/* only for HEVC_CTRL_TYPE_SET */
	int (*read_cst) (struct hevc_ctx *ctx,
			struct hevc_buf_ctrl *buf_ctrl);
	void (*write_cst) (struct hevc_ctx *ctx,
			struct hevc_buf_ctrl *buf_ctrl);
};

#define call_bop(b, op, args...)	\
	(b->op ? b->op(args) : 0)

struct hevc_codec_ops {
	/* initialization routines */
	int (*alloc_ctx_buf) (struct hevc_ctx *ctx);
	int (*alloc_desc_buf) (struct hevc_ctx *ctx);
	int (*get_init_arg) (struct hevc_ctx *ctx, void *arg);
	int (*pre_seq_start) (struct hevc_ctx *ctx);
	int (*post_seq_start) (struct hevc_ctx *ctx);
	int (*set_init_arg) (struct hevc_ctx *ctx, void *arg);
	int (*set_codec_bufs) (struct hevc_ctx *ctx);
	int (*set_dpbs) (struct hevc_ctx *ctx);		/* decoder */
	/* execution routines */
	int (*get_exe_arg) (struct hevc_ctx *ctx, void *arg);
	int (*pre_frame_start) (struct hevc_ctx *ctx);
	int (*post_frame_start) (struct hevc_ctx *ctx);
	int (*multi_data_frame) (struct hevc_ctx *ctx);
	int (*set_exe_arg) (struct hevc_ctx *ctx, void *arg);
	/* configuration routines */
	int (*get_codec_cfg) (struct hevc_ctx *ctx, unsigned int type,
			int *value);
	int (*set_codec_cfg) (struct hevc_ctx *ctx, unsigned int type,
			int *value);
	/* controls per buffer */
	int (*init_ctx_ctrls) (struct hevc_ctx *ctx);
	int (*cleanup_ctx_ctrls) (struct hevc_ctx *ctx);
	int (*init_buf_ctrls) (struct hevc_ctx *ctx,
			enum hevc_ctrl_type type, unsigned int index);
	int (*cleanup_buf_ctrls) (struct hevc_ctx *ctx,
			enum hevc_ctrl_type type, unsigned int index);
	int (*to_buf_ctrls) (struct hevc_ctx *ctx, struct list_head *head);
	int (*to_ctx_ctrls) (struct hevc_ctx *ctx, struct list_head *head);
	int (*set_buf_ctrls_val) (struct hevc_ctx *ctx,
			struct list_head *head);
	int (*get_buf_ctrls_val) (struct hevc_ctx *ctx,
			struct list_head *head);
	int (*recover_buf_ctrls_val) (struct hevc_ctx *ctx,
			struct list_head *head);
	int (*get_buf_update_val) (struct hevc_ctx *ctx,
			struct list_head *head, unsigned int id, int value);
};

#define call_cop(c, op, args...)				\
	(((c)->c_ops->op) ?					\
		((c)->c_ops->op(args)) : 0)

struct stored_dpb_info {
	int fd[HEVC_MAX_PLANES];
};

struct dec_dpb_ref_info {
	int index;
	struct stored_dpb_info dpb[HEVC_MAX_DPBS];
};

struct hevc_user_shared_handle {
	int fd;
	struct ion_handle *ion_handle;
	void *virt;
};

struct hevc_raw_info {
	int num_planes;
	int stride[3];
	int plane_size[3];
};

struct hevc_dec {
	int total_dpb_count;

	struct list_head dpb_queue;
	unsigned int dpb_queue_cnt;

	size_t src_buf_size;

	int loop_filter_mpeg4;
	int display_delay;
	int immediate_display;
	int is_packedpb;
	int slice_enable;
	int mv_count;
	int idr_decoding;
	int is_interlaced;
	int is_dts_mode;

	int crc_enable;
	int crc_luma0;
	int crc_chroma0;
	int crc_luma1;
	int crc_chroma1;

	struct hevc_extra_buf dsc;
	unsigned long consumed;
	unsigned long dpb_status;
	unsigned int dpb_flush;

	enum v4l2_memory dst_memtype;
	int sei_parse;
	int stored_tag;
	dma_addr_t y_addr_for_pb;

	int internal_dpb;
	int cr_left, cr_right, cr_top, cr_bot;

	/* For 6.x */
	int remained;

	/* For dynamic DPB */
	int is_dynamic_dpb;
	unsigned int dynamic_set;
	unsigned int dynamic_used;
	struct list_head ref_queue;
	unsigned int ref_queue_cnt;
	struct dec_dpb_ref_info *ref_info;
	int assigned_fd[HEVC_MAX_DPBS];
	struct hevc_user_shared_handle sh_handle;
};


/**
 * struct hevc_ctx - This struct contains the instance context
 */
struct hevc_ctx {
	struct hevc_dev *dev;
	struct v4l2_fh fh;
	int num;

	int int_cond;
	int int_type;
	unsigned int int_err;
	wait_queue_head_t queue;

	struct hevc_fmt *src_fmt;
	struct hevc_fmt *dst_fmt;

	struct vb2_queue vq_src;
	struct vb2_queue vq_dst;

	struct list_head src_queue;
	struct list_head dst_queue;

	unsigned int src_queue_cnt;
	unsigned int dst_queue_cnt;

	enum hevc_inst_type type;
	enum hevc_inst_state state;
	int inst_no;

	int img_width;
	int img_height;
	int buf_width;
	int buf_height;
	int dpb_count;

	struct hevc_raw_info raw_buf;
	int mv_size;

	/* Buffers */
	void *port_a_buf;
	size_t port_a_phys;
	size_t port_a_size;

	void *port_b_buf;
	size_t port_b_phys;
	size_t port_b_size;

	enum hevc_queue_state capture_state;
	enum hevc_queue_state output_state;

	struct list_head ctrls;

	struct list_head src_ctrls[HEVC_MAX_BUFFERS];
	struct list_head dst_ctrls[HEVC_MAX_BUFFERS];

	unsigned long src_ctrls_avail;
	unsigned long dst_ctrls_avail;

	unsigned int sequence;

	/* Control values */
	int codec_mode;
	__u32 pix_format;
	int cacheable;

	/* Extra Buffers */
	unsigned int ctx_buf_size;
	struct hevc_extra_buf ctx;
	struct hevc_extra_buf shm;

	struct hevc_dec *dec_priv;

	struct hevc_codec_ops *c_ops;

	/* For 6.x */
	size_t scratch_buf_size;

	/* for DRM */
	int is_drm;

	int is_dpb_realloc;
	enum hevc_dec_wait_state wait_state;

#ifdef CONFIG_HEVC_USE_BUS_DEVFREQ
	int qos_req_step;
	struct list_head qos_list;
#endif
	int qos_ratio;
	int framerate;
	int last_framerate;
	int avg_framerate;
	int frame_count;
	struct timeval last_timestamp;
	int lcu_size;
};

#define fh_to_hevc_ctx(x)	\
	container_of(x, struct hevc_ctx, fh)

#define HEVC_FMT_DEC	0
#define HEVC_FMT_RAW	2

static inline unsigned int hevc_version(struct hevc_dev *dev)
{
	unsigned int version = 0x10;
	return version;
}
#define HEVC_VER_MAJOR(dev)	((hevc_version(dev) >> 4) & 0xF)
#define HEVC_VER_MINOR(dev)	(hevc_version(dev) & 0xF)

#define IS_TWOPORT(dev)		(hevc_version(dev) == 0x51)
#define NUM_OF_PORT(dev)	(IS_TWOPORT(dev) ? 2 : 1)
#define NUM_OF_ALLOC_CTX(dev)	(NUM_OF_PORT(dev) + 1)

#define FW_HAS_DYNAMIC_DPB(dev)		(dev->fw.date >= 0x131030)

#define HW_LOCK_CLEAR_MASK		(0xFFFFFFFF)

/* Extra information for Decoder */
#define	DEC_SET_DYNAMIC_DPB		(1 << 1)

struct hevc_fmt {
	char *name;
	u32 fourcc;
	u32 codec_mode;
	u32 type;
	u32 num_planes;
};

int hevc_get_framerate(struct timeval *to, struct timeval *from);
inline int hevc_clear_hw_bit(struct hevc_ctx *ctx);

#ifdef CONFIG_ION_EXYNOS
extern struct ion_device *ion_exynos;
#endif

#include "regs-hevc.h"
#include "hevc_opr.h"

#endif /* HEVC_COMMON_H_ */
