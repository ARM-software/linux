// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai (jonathan.chai@arm.com)
 *
 */
#include <linux/slab.h>
#include "mali_aeu_dev.h"
#include "mali_aeu_hw.h"
#include <media/videobuf2-vmalloc.h>

#define IRQ_NAME	"AEU"


/* TODO: should add ctrl handler, buffer queue, etc */
typedef struct {
	struct v4l2_fh		fh;
	struct mali_aeu_device	*adev;
	struct v4l2_ctrl_handler	hdl;
	/* protect access to the buffer status */
	spinlock_t		buf_lock;
	/* how many buffers per transactoin */
	u32	trans_num;

	mali_aeu_hw_ctx_t	*hw_ctx;

} aeu_ctx_t;

/* buffer queue implementation */
static int aeu_queue_setup(struct vb2_queue *q,
			   unsigned int *nbuf,
			   unsigned int *nplans, unsigned int sizes[],
			   struct device *alloc_devs[])
{

	/* TODO (DISPLAYSW-550): need set data according to fmt */
	*nplans = 1;
	sizes[0] = 100 * 1024;

	return 0;
}

static int aeu_buf_prepare(struct vb2_buffer *buf)
{
	return 0;
}

static void aeu_buf_queue(struct vb2_buffer *buf)
{
	aeu_ctx_t *ctx = vb2_get_drv_priv(buf->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(buf);
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vbuf);
}

static int aeu_start_streaming(struct vb2_queue *q, unsigned int count)
{
	return 0;
}

static void aeu_stop_streaming(struct vb2_queue *q)
{
	aeu_ctx_t *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vbuf;
	unsigned long flags;

	do {
		if (V4L2_TYPE_IS_OUTPUT(q->type))
			vbuf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
		else
			vbuf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);

		if (vbuf) {
			spin_lock_irqsave(&ctx->buf_lock, flags);
			vb2_buffer_done(&vbuf->vb2_buf, VB2_BUF_STATE_ERROR);
			spin_unlock_irqrestore(&ctx->buf_lock, flags);
		}

	} while (vbuf != NULL);
	dev_info(ctx->adev->dev, "%s:stop\n", __func__);
}

static const struct vb2_ops aeu_qops = {
	.queue_setup		= aeu_queue_setup,
	.buf_prepare		= aeu_buf_prepare,
	.buf_queue		= aeu_buf_queue,
	.start_streaming	= aeu_start_streaming,
	.stop_streaming		= aeu_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static int init_vb2_queue(struct vb2_queue *q, unsigned int type, void *priv)
{
	int ret;
	aeu_ctx_t *ctx = priv;

	q->type = type;
	q->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = ctx;
	q->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	q->ops = &aeu_qops;
	q->mem_ops = &vb2_vmalloc_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	q->lock = &ctx->adev->aeu_mutex;

	ret = vb2_queue_init(q);
	if (ret) {
		struct device *dev = ctx->adev->dev;
		dev_err(dev, "%s: init queue (tyep: %u) error!\n",
			__func__, type);
	}
	return ret;
}

static int
aeu_queue_init(void *priv, struct vb2_queue *s_vq, struct vb2_queue *d_vq)
{
	if (init_vb2_queue(s_vq, V4L2_BUF_TYPE_VIDEO_OUTPUT, priv))
		return -1;
	return init_vb2_queue(d_vq, V4L2_BUF_TYPE_VIDEO_CAPTURE, priv);
}

/* TODO: implement user control data structures and function */
#define V4L2_CID_TRANS_NUM_BUFS         (V4L2_CID_USER_BASE + 0x1000)

static int aeu_s_ctrl(struct v4l2_ctrl *ctrl)
{
	aeu_ctx_t *actx = container_of(ctrl->handler, aeu_ctx_t, hdl);

	switch (ctrl->id) {
	case V4L2_CID_TRANS_NUM_BUFS:
		actx->trans_num = ctrl->val;
		break;
	default:
		dev_warn(actx->adev->dev, "Unsupported ctrl (%u)\n", ctrl->id);
		return -EINVAL;
	}
	return 0;
}

static const struct v4l2_ctrl_ops aeu_ctrl_ops = {
	.s_ctrl			= aeu_s_ctrl,
};

static const struct v4l2_ctrl_config aeu_ctrl_trans_buf_num = {
	.ops = &aeu_ctrl_ops,
	.id = V4L2_CID_TRANS_NUM_BUFS,
	.name = "Number of buffers Per Transaction",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.def = 1,
	.min = 1,
	.max = VIDEO_MAX_FRAME,
	.step = 1,
};

/* file operation implementation */
static aeu_ctx_t *file2ctx(struct file *f)
{
	return container_of(f->private_data, aeu_ctx_t, fh);
}

static int aeu_open(struct file *file)
{
	struct mali_aeu_device *adev = video_drvdata(file);
	int ret = 0;
	aeu_ctx_t *ctx = NULL;

	if (mutex_lock_interruptible(&adev->aeu_mutex))
		return -ERESTARTSYS;

	ctx = kzalloc(sizeof(aeu_ctx_t), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto exit_aeu_open;
	}

	spin_lock_init(&ctx->buf_lock);

	ctx->hw_ctx = mali_aeu_hw_init_ctx(adev->hw_dev);
	if (!ctx->hw_ctx) {
		ret = -ENOMEM;
		goto exit_aeu_ctx;
	}

	ctx->adev = adev;
	v4l2_fh_init(&ctx->fh, &adev->vdev);
	file->private_data = &ctx->fh;

	v4l2_ctrl_handler_init(&ctx->hdl, 1);
	v4l2_ctrl_new_custom(&ctx->hdl, &aeu_ctrl_trans_buf_num, NULL);
	if (ctx->hdl.error) {
		ret = ctx->hdl.error;
		goto exit_aeu_hw_ctx;
	}
	ctx->fh.ctrl_handler = &ctx->hdl;
	v4l2_ctrl_handler_setup(&ctx->hdl);

	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(adev->m2mdev, ctx, aeu_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto exit_aeu_hw_ctx;
	}

	v4l2_fh_add(&ctx->fh);
	mutex_unlock(&adev->aeu_mutex);
	return 0;

exit_aeu_hw_ctx:
	v4l2_ctrl_handler_free(&ctx->hdl);
	v4l2_fh_exit(&ctx->fh);
	mali_aeu_hw_free_ctx(ctx->hw_ctx);
exit_aeu_ctx:
	kfree(ctx);
exit_aeu_open:
	mutex_unlock(&adev->aeu_mutex);
	return ret;
}

static int aeu_release(struct file *file)
{
	struct mali_aeu_device *adev = video_drvdata(file);
	aeu_ctx_t *ctx = file2ctx(file);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->hdl);
	mali_aeu_hw_free_ctx(ctx->hw_ctx);
	mutex_lock(&adev->aeu_mutex);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	mutex_unlock(&adev->aeu_mutex);
	kfree(ctx);

	return 0;
}

static const struct v4l2_file_operations mali_aeu_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open		= aeu_open,
	.release	= aeu_release,
	.poll		= v4l2_m2m_fop_poll,
	.mmap		= v4l2_m2m_fop_mmap,
};

/* Implement adu v4l2 ioctl operations */
static int aeu_drv_cap(struct file *file, void *priv,
		       struct v4l2_capability *cap)
{
	strncpy(cap->driver, AEU_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, AEU_NAME, sizeof(cap->card) - 1);
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", AEU_NAME);
	cap->device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

/* TODO (DISPLAYSW-550): format implementation */
static int aeu_enum_fmt_vid_cap(struct file *file, void *p,
			struct v4l2_fmtdesc *f)
{
	return 0;
}

static int aeu_enum_fmt_vid_out(struct file *file, void *p,
			struct v4l2_fmtdesc *f)
{
	return 0;
}

static int aeu_try_fmt_vid_cap(struct file *file, void *p,
			struct v4l2_format *f)
{
	return 0;
}

static int aeu_try_fmt_vid_out(struct file *file, void *p,
			struct v4l2_format *f)
{
	return 0;
}

static int aeu_g_fmt_vid_cap(struct file *file, void *p,
			struct v4l2_format *f)
{
	return 0;
}

static int aeu_g_fmt_vid_out(struct file *file, void *p,
			struct v4l2_format *f)
{
	return 0;
}

static int aeu_s_fmt_vid_cap(struct file *file, void *p,
			struct v4l2_format *f)
{
	return 0;
}

static int aeu_s_fmt_vid_out(struct file *file, void *p,
			struct v4l2_format *f)
{
	return 0;
}

static const struct v4l2_ioctl_ops mali_aeu_ioctl_ops = {
	.vidioc_querycap		= aeu_drv_cap,

	.vidioc_enum_fmt_vid_cap	= aeu_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap		= aeu_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap		= aeu_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap		= aeu_s_fmt_vid_cap,

	.vidioc_enum_fmt_vid_out	= aeu_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out		= aeu_g_fmt_vid_out,
	.vidioc_try_fmt_vid_out		= aeu_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out		= aeu_s_fmt_vid_out,

	.vidioc_reqbufs		= v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf	= v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf		= v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf		= v4l2_m2m_ioctl_dqbuf,
	.vidioc_create_bufs	= v4l2_m2m_ioctl_create_bufs,
	.vidioc_expbuf		= v4l2_m2m_ioctl_expbuf,

	.vidioc_streamon	= v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff	= v4l2_m2m_ioctl_streamoff,

	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

/* m2m ops functions */
static void aeu_m2m_device_run(void *priv)
{
	aeu_ctx_t *ctx = priv;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	unsigned long flags;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);

	/* TODO: commit to hw */
	pr_info("%s: is running \n", __func__);
	/* TODO(DISPLYSW-1269): move these to irq handler */
	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	spin_lock_irqsave(&ctx->buf_lock, flags);
	vb2_buffer_done(&dst_buf->vb2_buf, VB2_BUF_STATE_DONE);
	vb2_buffer_done(&src_buf->vb2_buf, VB2_BUF_STATE_DONE);
	spin_unlock_irqrestore(&ctx->buf_lock, flags);
	v4l2_m2m_job_finish(ctx->adev->m2mdev, ctx->fh.m2m_ctx);
}

static int aeu_m2m_job_ready(void *priv)
{
	aeu_ctx_t *ctx = priv;

	if ((v4l2_m2m_num_src_bufs_ready(ctx->fh.m2m_ctx) < ctx->trans_num) ||
	    (v4l2_m2m_num_dst_bufs_ready(ctx->fh.m2m_ctx) < ctx->trans_num))
		return 0;
	return 1;
}

static void aeu_m2m_job_abort(void *priv)
{
}

/* TODO: Implement m2m device operations */
static const struct v4l2_m2m_ops mali_aeu_m2m_ops = {
	.device_run	= aeu_m2m_device_run,
	.job_ready	= aeu_m2m_job_ready,
	.job_abort	= aeu_m2m_job_abort,
};

irqreturn_t mali_aeu_irq_thread_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

int mali_aeu_device_init(struct mali_aeu_device *adev,
			 struct platform_device *pdev)
{
	int ret, irq;

	ret = v4l2_device_register(&pdev->dev, &adev->v4l2_dev);
	if (ret)
		return ret;

	irq = platform_get_irq_byname(pdev, IRQ_NAME);
	if (irq < 0) {
		dev_err(&pdev->dev, "%s: failed to get irq number\n",
			__func__);
		return irq;
	}

	ret = devm_request_threaded_irq(&pdev->dev, irq,
					mali_aeu_hw_irq_handler,
					mali_aeu_irq_thread_handler,
					IRQF_SHARED, "mali-aeu", adev->hw_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to request aeu irq\n",
			__func__);
		return ret;
	}

	snprintf(adev->vdev.name, sizeof(adev->vdev.name), "%s", AEU_NAME);
	adev->vdev.vfl_dir	= VFL_DIR_M2M;
	adev->vdev.release	= video_device_release_empty;
	adev->vdev.v4l2_dev	= &adev->v4l2_dev;
	adev->vdev.fops		= &mali_aeu_fops;
	adev->vdev.ioctl_ops	= &mali_aeu_ioctl_ops;
	adev->vdev.minor	= -1;

	adev->dev = &pdev->dev;
	mutex_init(&adev->aeu_mutex);
	adev->vdev.lock = &adev->aeu_mutex;

	ret = video_register_device(&adev->vdev, VFL_TYPE_GRABBER, 0);
	if (ret) {
		dev_err(&pdev->dev, "%s: video device register error\n",
			__func__);
		return ret;
	}
	dev_info(&pdev->dev, "%s: Device registered as /dev/video%d\n",
		 __func__, adev->vdev.num);
	video_set_drvdata(&adev->vdev, adev);

	adev->m2mdev = v4l2_m2m_init(&mali_aeu_m2m_ops);
	if (IS_ERR(adev->m2mdev)) {
		dev_err(&pdev->dev, "%s: failed to init m2m device\n",
			__func__);
		ret = PTR_ERR(adev->m2mdev);
	}
	return ret;
}

int mali_aeu_device_destroy(struct mali_aeu_device *adev)
{
	v4l2_m2m_release(adev->m2mdev);
	video_unregister_device(&adev->vdev);
	v4l2_device_unregister(&adev->v4l2_dev);
	return 0;
}
