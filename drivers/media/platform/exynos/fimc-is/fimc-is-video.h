#ifndef FIMC_IS_VIDEO_H
#define FIMC_IS_VIDEO_H

#include <linux/version.h>
#include <media/v4l2-ioctl.h>
#include "fimc-is-type.h"
#include "fimc-is-mem.h"
#include "fimc-is-framemgr.h"
#include "fimc-is-metadata.h"
#include "fimc-is-config.h"

/* configuration by linux kernel version */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
#define VFL_DIR_RX		0
#define VFL_DIR_TX		1
#define VFL_DIR_M2M		2
#endif

#define FIMC_IS_MAX_NODES			(3)
#define FIMC_IS_MAX_BUFS			(12)
#define FIMC_IS_MAX_PLANES			(4)
#define FIMC_IS_INVALID_BUF_INDEX		(0xFF)

#define VIDEO_SENSOR_READY_BUFFERS		0
#define VIDEO_3AA_READY_BUFFERS			0
#define VIDEO_3AAC_READY_BUFFERS		0
#define VIDEO_3AAP_READY_BUFFERS		0
#define VIDEO_ISP_READY_BUFFERS			0
#define VIDEO_SCC_READY_BUFFERS			0
#define VIDEO_SCP_READY_BUFFERS			0
#define VIDEO_VDISC_READY_BUFFERS		1
#define VIDEO_VDISO_READY_BUFFERS		0

#define FIMC_IS_VIDEO_NAME(name)		("exynos-fimc-is-"name)
#define FIMC_IS_VIDEO_SENSOR_NAME		FIMC_IS_VIDEO_NAME("sensor")
#define FIMC_IS_VIDEO_3AA_NAME(id)		FIMC_IS_VIDEO_NAME("3aa."#id)
#define FIMC_IS_VIDEO_3AAC_NAME(id)		FIMC_IS_VIDEO_NAME("3aa."#id"c")
#define FIMC_IS_VIDEO_3AAP_NAME(id)		FIMC_IS_VIDEO_NAME("3aa."#id"p")
#define FIMC_IS_VIDEO_ISP_NAME			FIMC_IS_VIDEO_NAME("isp")
#define FIMC_IS_VIDEO_SCC_NAME			FIMC_IS_VIDEO_NAME("scalerc")
#define FIMC_IS_VIDEO_SCP_NAME			FIMC_IS_VIDEO_NAME("scalerp")
#define FIMC_IS_VIDEO_VDC_NAME			FIMC_IS_VIDEO_NAME("vdisc")
#define FIMC_IS_VIDEO_VDO_NAME			FIMC_IS_VIDEO_NAME("vdiso")

struct fimc_is_device_ischain;
struct fimc_is_subdev;
struct fimc_is_queue;

enum fimc_is_video_dev_num {
	FIMC_IS_VIDEO_SS0_NUM	= 0,
	FIMC_IS_VIDEO_SS1_NUM	= 1,
	FIMC_IS_VIDEO_SS2_NUM	= 2,
	FIMC_IS_VIDEO_SS3_NUM	= 3,
	FIMC_IS_VIDEO_3A0_NUM	= 10,
	FIMC_IS_VIDEO_3A0C_NUM	= 11,
	FIMC_IS_VIDEO_3A0P_NUM	= 12,
	FIMC_IS_VIDEO_3A1_NUM	= 14,
	FIMC_IS_VIDEO_3A1C_NUM	= 15,
	FIMC_IS_VIDEO_3A1P_NUM	= 16,
	FIMC_IS_VIDEO_ISP_NUM	= 30,
	FIMC_IS_VIDEO_SCC_NUM	= 34,
	FIMC_IS_VIDEO_SCP_NUM	= 37,
	FIMC_IS_VIDEO_VDC_NUM	= 40,
	FIMC_IS_VIDEO_VDO_NUM	= 41,
	FIMC_IS_VIDEO_FD_NUM	= 46,
	FIMC_IS_VIDEO_MAX_NUM	= 49
};

enum fimc_is_video_type {
	FIMC_IS_VIDEO_TYPE_CAPTURE,
	FIMC_IS_VIDEO_TYPE_OUTPUT,
	FIMC_IS_VIDEO_TYPE_M2M,
};

enum fimc_is_queue_state {
	FIMC_IS_QUEUE_BUFFER_PREPARED,
	FIMC_IS_QUEUE_BUFFER_READY,
	FIMC_IS_QUEUE_STREAM_ON
};

struct fimc_is_frame_cfg {
	struct fimc_is_fmt		format;
	u32				width;
	u32				height;
	u32				width_stride[FIMC_IS_MAX_PLANES];
	u32				size[FIMC_IS_MAX_PLANES];
	u32				bytesperline[FIMC_IS_MAX_PLANES];
};

struct fimc_is_queue_ops {
	int (*start_streaming)(struct fimc_is_device_ischain *device,
		struct fimc_is_subdev *subdev,
		struct fimc_is_queue *queue);
	int (*stop_streaming)(struct fimc_is_device_ischain *device,
		struct fimc_is_subdev *subdev,
		struct fimc_is_queue *queue);
};

struct fimc_is_queue {
	struct vb2_queue		*vbq;
	const struct fimc_is_queue_ops	*qops;
	struct fimc_is_framemgr		framemgr;
	struct fimc_is_frame_cfg	framecfg;

	u32				buf_maxcount;
	u32				buf_rdycount;
	u32				buf_refcount;
	u32				buf_dva[FIMC_IS_MAX_BUFS][FIMC_IS_MAX_PLANES];
	u32				buf_kva[FIMC_IS_MAX_BUFS][FIMC_IS_MAX_PLANES];

	u32				id;
	unsigned long			state;
};

struct fimc_is_video_ctx {
	struct fimc_is_queue		q_src;
	struct fimc_is_queue		q_dst;
	struct mutex			lock;
	u32				type;
	u32				instance;

	void				*device;
	struct fimc_is_video		*video;

	const struct vb2_ops		*vb2_ops;
	const struct vb2_mem_ops	*mem_ops;
};

struct fimc_is_video {
	u32				id;
	atomic_t			refcount;
	struct mutex			lock;

	struct video_device		vd;
	struct media_pad		pads;
	const struct fimc_is_vb2	*vb2;
	void				*alloc_ctx;
};

struct fimc_is_core *fimc_is_video_ctx_2_core(struct fimc_is_video_ctx *vctx);

/* video context operation */
int open_vctx(struct file *file,
	struct fimc_is_video *video,
	struct fimc_is_video_ctx **vctx,
	u32 id_src, u32 id_dst);
int close_vctx(struct file *file,
	struct fimc_is_video *video,
	struct fimc_is_video_ctx *vctx);

/* queue operation */
int fimc_is_queue_setup(struct fimc_is_queue *queue,
	void *alloc_ctx,
	unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[]);
int fimc_is_queue_buffer_queue(struct fimc_is_queue *queue,
	const struct fimc_is_vb2 *vb2,
	struct vb2_buffer *vb);
void fimc_is_queue_wait_prepare(struct vb2_queue *vbq);
void fimc_is_queue_wait_finish(struct vb2_queue *vbq);
int fimc_is_queue_start_streaming(struct fimc_is_queue *queue,
	struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx);
int fimc_is_queue_stop_streaming(struct fimc_is_queue *queue,
	struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx);

/* video operation */
int fimc_is_video_probe(struct fimc_is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct fimc_is_mem *mem,
	struct v4l2_device *v4l2_dev,
	struct mutex *lock,
	const struct v4l2_file_operations *fops,
	const struct v4l2_ioctl_ops *ioctl_ops);
int fimc_is_video_open(struct fimc_is_video_ctx *vctx,
	void *device,
	u32 buf_rdycount,
	struct fimc_is_video *video,
	u32 video_type,
	const struct vb2_ops *vb2_ops,
	const struct fimc_is_queue_ops *src_qops,
	const struct fimc_is_queue_ops *dst_qops);
int fimc_is_video_close(struct fimc_is_video_ctx *vctx);
u32 fimc_is_video_poll(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct poll_table_struct *wait);
int fimc_is_video_mmap(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct vm_area_struct *vma);
int fimc_is_video_reqbufs(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_requestbuffers *request);
int fimc_is_video_querybuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int fimc_is_video_set_format_mplane(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_format *format);
int fimc_is_video_qbuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int fimc_is_video_dqbuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf);
int fimc_is_video_streamon(struct file *file,
	struct fimc_is_video_ctx *vctx,
	enum v4l2_buf_type type);
int fimc_is_video_streamoff(struct file *file,
	struct fimc_is_video_ctx *vctx,
	enum v4l2_buf_type type);

int queue_done(struct fimc_is_video_ctx *vctx,
	struct fimc_is_queue *queue,
	u32 index, u32 state);
int buffer_done(struct fimc_is_video_ctx *vctx, u32 index);
long video_ioctl3(struct file *file, unsigned int cmd, unsigned long arg);

#define GET_QUEUE(vctx, type) \
	(V4L2_TYPE_IS_OUTPUT((type)) ? &vctx->q_src : &vctx->q_dst)
#define GET_VCTX_QUEUE(vctx, vbq) (GET_QUEUE(vctx, vbq->type))

#define GET_SRC_QUEUE(vctx) (&vctx->q_src)
#define GET_DST_QUEUE(vctx) (&vctx->q_dst)

#define GET_SRC_FRAMEMGR(vctx) (&vctx->q_src.framemgr)
#define GET_DST_FRAMEMGR(vctx) (&vctx->q_dst.framemgr)

#define CALL_QOPS(q, op, args...) (((q)->qops->op) ? ((q)->qops->op(args)) : 0)

#endif
