#include <linux/module.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"

int fimc_is_subdev_open(struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx,
	const struct param_control *init_ctl)
{
	int ret = 0;

	if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state)) {
		warn("subdev%d already open", subdev->entry);
		goto p_err;
	}

	mutex_init(&subdev->mutex_state);
	subdev->vctx = vctx;
	subdev->input.width = 0;
	subdev->input.height = 0;
	subdev->output.width = 0;
	subdev->output.height = 0;

	if (init_ctl) {
		if (init_ctl->cmd != CONTROL_COMMAND_START) {
			if ((subdev->entry == ENTRY_DIS) ||
					(subdev->entry == ENTRY_TDNR)) {
#if defined(ENABLE_VDIS) || defined(ENABLE_TDNR)
				err("%d entry is not start", subdev->entry);
#endif
			} else {
				err("%d entry is not start", subdev->entry);
			}
			ret = -EINVAL;
			goto p_err;
		}

		if (init_ctl->bypass == CONTROL_BYPASS_ENABLE)
			clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);
		else if (init_ctl->bypass == CONTROL_BYPASS_DISABLE)
			set_bit(FIMC_IS_SUBDEV_START, &subdev->state);
		else {
			err("%d entry has invalid bypass value(%d)",
				subdev->entry, init_ctl->bypass);
			ret = -EINVAL;
			goto p_err;
		}
	} else {
		/* isp, scc, scp do not use bypass(memory interface)*/
		clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);
	}

	set_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

int fimc_is_subdev_close(struct fimc_is_subdev *subdev)
{
	clear_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);

	return 0;
}

int fimc_is_subdev_start(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue)
{
	return 0;
}

int fimc_is_subdev_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!queue);

	framemgr = &queue->framemgr;

/*
	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mwarn("already stop", device);
		goto p_err;
	}
*/

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_4, flags);

	if (framemgr->frame_pro_cnt > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_4, flags);
		merr("being processed, can't stop", device);
		ret = -EINVAL;
		goto p_err;
	}

	fimc_is_frame_complete_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_com_to_fre(framemgr, frame);
		fimc_is_frame_complete_head(framemgr, &frame);
	}

	fimc_is_frame_request_head(framemgr, &frame);
	while (frame) {
		fimc_is_frame_trans_req_to_fre(framemgr, frame);
		fimc_is_frame_request_head(framemgr, &frame);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_4, flags);

//	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

int fimc_is_subdev_s_format(struct fimc_is_subdev *subdev,
	u32 width, u32 height)
{
	int ret = 0;

	BUG_ON(!subdev);

	subdev->output.width = width;
	subdev->output.height = height;

	return ret;
}

int fimc_is_subdev_buffer_queue(struct fimc_is_subdev *subdev,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(!subdev->vctx);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	vctx = subdev->vctx;
	framemgr = GET_DST_FRAMEMGR(vctx);

	/* 1. check frame validation */
	frame = &framemgr->frame[index];
	if (!frame) {
		merr("frame is null\n", vctx);
		ret = EINVAL;
		goto p_err;
	}

	if (unlikely(frame->memory == FRAME_UNI_MEM)) {
		merr("frame %d is NOT init", vctx, index);
		ret = EINVAL;
		goto p_err;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, index, flags);

	if (frame->state == FIMC_IS_FRAME_STATE_FREE) {
		if (frame->req_flag) {
			warn("%d request flag is not clear(%08X)\n",
				frame->index, (u32)frame->req_flag);
			frame->req_flag = 0;
		}

		fimc_is_frame_trans_fre_to_req(framemgr, frame);
	} else {
		merr("frame(%d) is invalid state(%d)\n", vctx, index, frame->state);
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, index, flags);

p_err:
	return ret;
}

int fimc_is_subdev_buffer_finish(struct fimc_is_subdev *subdev,
	u32 index)
{
	int ret = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(index >= FRAMEMGR_MAX_REQUEST);

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		err("framemgr is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irq(framemgr, index);

	fimc_is_frame_complete_head(framemgr, &frame);
	if (frame) {
		if (frame->index == index) {
			fimc_is_frame_trans_com_to_fre(framemgr, frame);
		} else {
			err("buffer index is NOT matched(%d != %d)\n",
				index, frame->index);
			fimc_is_frame_print_all(framemgr);
			ret = -EINVAL;
		}
	} else {
		err("frame is empty from complete");
		fimc_is_frame_print_all(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, index);

p_err:
	return ret;
}

const struct fimc_is_queue_ops fimc_is_ischain_sub_ops = {
	.start_streaming	= fimc_is_subdev_start,
	.stop_streaming		= fimc_is_subdev_stop
};

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

#ifdef ENABLE_VDIS
	param->control.cmd = CONTROL_COMMAND_START;
	param->control.bypass = CONTROL_BYPASS_DISABLE;
	param->control.buffer_number = SIZE_DIS_INTERNAL_BUF * NUM_DIS_INTERNAL_BUF;
	param->control.buffer_address = device->imemory.dvaddr_shared + 300 * sizeof(u32);
	device->is_region->shared[300] = device->imemory.dvaddr_dis;
#else
	merr("can't start hw vdis", device);
	BUG();
#endif

	*lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	param->control.cmd = CONTROL_COMMAND_STOP;
	param->control.bypass = CONTROL_BYPASS_DISABLE;

	*lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct dis_param *param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	if (device->pdata->subip_info->_dis.full_bypass)
		param->control.cmd = CONTROL_COMMAND_STOP;
	else
		param->control.cmd = CONTROL_COMMAND_START;
	/*
	 * special option
	 * bypass command should be 2 for enabling software dis
	 * software dis is not active because output format of software
	 * can't support multi-plane.
	 */
	param->control.bypass = CONTROL_BYPASS_ENABLE;

	*lindex |= LOWBIT_OF(PARAM_DIS_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DIS_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!ctl_param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param->cmd = CONTROL_COMMAND_START;
	ctl_param->bypass = CONTROL_BYPASS_DISABLE;
	ctl_param->buffer_number = SIZE_DNR_INTERNAL_BUF * NUM_DNR_INTERNAL_BUF;
	ctl_param->buffer_address = device->imemory.dvaddr_shared + 350 * sizeof(u32);
	device->is_region->shared[350] = device->imemory.dvaddr_3dnr;

	*lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!ctl_param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param->cmd = CONTROL_COMMAND_STOP;
	ctl_param->bypass = CONTROL_BYPASS_DISABLE;

	*lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!ctl_param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	if (device->pdata->subip_info->_dnr.full_bypass)
		ctl_param->cmd = CONTROL_COMMAND_STOP;
	else
		ctl_param->cmd = CONTROL_COMMAND_START;

	ctl_param->bypass = CONTROL_BYPASS_ENABLE;

	*lindex |= LOWBIT_OF(PARAM_TDNR_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_TDNR_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_drc_start(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!ctl_param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param->cmd = CONTROL_COMMAND_START;
	ctl_param->bypass = CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
}

void fimc_is_subdev_drc_bypass(struct fimc_is_device_ischain *device,
	struct param_control *ctl_param, u32 *lindex, u32 *hindex, u32 *indexes)
{
	BUG_ON(!device);
	BUG_ON(!ctl_param);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	if (device->pdata->subip_info->_drc.full_bypass)
		ctl_param->cmd = CONTROL_COMMAND_STOP;
	else
		ctl_param->cmd = CONTROL_COMMAND_START;

	ctl_param->bypass = CONTROL_BYPASS_ENABLE;

	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
}
