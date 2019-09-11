// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>
#include <linux/dma-buf.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>

#include "komeda_dev.h"
#include "komeda_kms.h"

void komeda_crtc_get_color_config(struct drm_crtc_state *crtc_st,
				  u32 *color_depths, u32 *color_formats)
{
	struct drm_connector *conn;
	struct drm_connector_state *conn_st;
	u32 conn_color_formats = ~0u;
	int i, min_bpc = 31, conn_bpc = 0;

	for_each_new_connector_in_state(crtc_st->state, conn, conn_st, i) {
		if (conn_st->crtc != crtc_st->crtc)
			continue;

		conn_bpc = conn->display_info.bpc ? conn->display_info.bpc : 8;
		conn_color_formats &= conn->display_info.color_formats;

		if (conn_bpc < min_bpc)
			min_bpc = conn_bpc;
	}

	/* connector doesn't config any color_format, use RGB444 as default */
	if (conn_color_formats == 0)
		conn_color_formats = DRM_COLOR_FORMAT_RGB444;

	*color_depths = GENMASK(conn_bpc, 0);
	*color_formats = conn_color_formats;
}

static void komeda_crtc_update_clock_ratio(struct komeda_crtc_state *kcrtc_st)
{
	u64 pxlclk, aclk;

	if (!kcrtc_st->base.active) {
		kcrtc_st->clock_ratio = 0;
		return;
	}

	pxlclk = kcrtc_st->base.adjusted_mode.crtc_clock * 1000;
	aclk = komeda_crtc_get_aclk(kcrtc_st);

	kcrtc_st->clock_ratio = div64_u64(aclk << 32, pxlclk);
}

/* if en_inv is true, save inv_cur_mat into cur_mat */
static bool
get_current_pose(struct komeda_sensor_buff *sb_buff,
		 struct malidp_position *cur_pos,
		 struct malidp_matrix4 *cur_mat,
		 bool en_inv)
{
	struct malidp_sensor_buffer_info *s_buf_info;
	struct malidp_sensor_data data;
	struct malidp_quaternion quat;
	u64 idx[2], offset;
	unsigned long flags;
	u8 *vaddr;

	spin_lock_irqsave(&sb_buff->spinlock, flags);
	if (!sb_buff->vaddr) {
		spin_unlock_irqrestore(&sb_buff->spinlock, flags);
		return false;
	}

	s_buf_info = sb_buff->sensor_buf_info_blob->data;
	vaddr = sb_buff->vaddr;
	komeda_sb_read_128bits((u64*)vaddr, idx);
	offset = s_buf_info->offset + sizeof(data) * (idx[0] & 0xffffffff);
	komeda_sb_read_128bits((u64*)(vaddr+offset), (u64*)&data);
	komeda_sb_read_128bits((u64*)(vaddr+offset+16), ((u64*)&data)+2);
	spin_unlock_irqrestore(&sb_buff->spinlock, flags);

	cur_pos->x = data.pos.x;
	cur_pos->y = data.pos.y;
	cur_pos->z = data.pos.z;
	quat.x = data.quat.x;
	quat.y = data.quat.y;
	quat.z = data.quat.z;
	quat.w = data.quat.w;
	/* convert quat to matrix */
	if (en_inv)
		inverse_quaternion(&quat, &quat);
	quaternion_to_matrix(&quat, cur_mat);

	return true;
}

static void matrix_to_q_format(struct malidp_matrix3 *m)
{
	int i;
	struct round_exception extra;

	for (i = 0; i < 9; i++) {
		extra.exception = 0;
		m->data[i] = to_q1_30(m->data[i], &extra);
		extra.exception &= ~float32_exception_inexact;
		WARN_ON(extra.exception != 0);
	}
}

static void
update_atu_vp_matrix(struct komeda_atu_vp_state *vp_st, enum komeda_atu_vp_type type,
		     struct malidp_position *cur_pos, struct malidp_matrix4 *inv_cur_mat,
		     struct malidp_position *ref_pos, struct malidp_matrix4 *ref_mat)
{
	struct round_exception extra_data;
	struct malidp_matrix4 tmp1, tmp2;

	if (!ref_mat)
		goto skip_trans_matrix;

	switch (type) {
	case ATU_VP_TYPE_PROJ:
		update_projection_layer_transform_matrix(&vp_st->m1,
					     &vp_st->m2,
					     ref_mat,
					     inv_cur_mat,
					     &tmp1);
		break;
	case ATU_VP_TYPE_QUAD:
		update_quad_layer_transform_matrix(cur_pos, inv_cur_mat, ref_pos, ref_mat,
			&vp_st->m1, &vp_st->m2, &tmp1);
		break;
	default:
		DRM_DEBUG_ATOMIC("ATU viewport enabled with bad mode!(should be proj or quad)\n");
		return;
	}

	matrix_mul_4X4(&tmp1, &vp_st->m1, &tmp2);
	matrix_4X4_to_3X3(&tmp2, &vp_st->A);
	matrix_4X4_to_3X3(&tmp2, &vp_st->B);
	normalize_matrix3(&vp_st->A, &extra_data);
	if (extra_data.exception & float32_exception_invalid) {
		DRM_DEBUG_ATOMIC("Normalize Matrix A error!\n");
		return;
	}
	normalize_matrix3(&vp_st->B, &extra_data);
	if (extra_data.exception & float32_exception_invalid) {
		DRM_DEBUG_ATOMIC("Normalize Matrix B error!\n");
		return;
	}

	matrix_to_q_format(&vp_st->A);
	matrix_to_q_format(&vp_st->B);
	return;

skip_trans_matrix:
	identity_matrix3(&vp_st->A);
	identity_matrix3(&vp_st->B);
}

static void
update_pipeline_atu_matrix(struct komeda_dev *mdev, struct komeda_pipeline *pipe, u64 evt,
			   struct malidp_position *cur_pos, struct malidp_matrix4 *inv_cur_mat,
			   struct malidp_position *ref_pos, struct malidp_matrix4 *ref_mat)
{
	struct komeda_pipeline_state *pipe_st;
	u32 active_atus, i;

	pipe_st = &pipe->last_st;
	active_atus = pipe_st->active_comps & KOMEDA_PIPELINE_ATUS;
	dp_for_each_set_bit(i, active_atus) {
		struct komeda_atu *atu;
		struct komeda_atu_state *st;

		atu = to_atu(komeda_pipeline_get_component(pipe, i));
		st = &atu->last_st;

		if (evt == KOMEDA_EVENT_ASYNC_RP) {
			/* PL2 */
			if (st->left.vp_type != ATU_VP_TYPE_NONE)
				update_atu_vp_matrix(&st->left, st->left.vp_type,
					     cur_pos, inv_cur_mat, ref_pos, ref_mat);
			if ((st->right.vp_type != ATU_VP_TYPE_NONE &&
			    st->mode != ATU_MODE_VP0_VP1_SEQ) ||
			    (st->right.vp_type != ATU_VP_TYPE_NONE &&
			    st->mode == ATU_MODE_VP0_VP1_SEQ && !ref_mat))
				update_atu_vp_matrix(&st->right, st->right.vp_type,
					     cur_pos, inv_cur_mat, ref_pos, ref_mat);
		} else 	if (st->mode == ATU_MODE_VP0_VP1_SEQ) {
			/* PL1 only flush right vp under SEQ mode */
			if (st->right.vp_type != ATU_VP_TYPE_NONE)
				update_atu_vp_matrix(&st->right, st->right.vp_type,
					     cur_pos, inv_cur_mat, ref_pos, ref_mat);
		}

		mdev->funcs->latch_matrix(atu);
	}
}

static void
update_crtc_atu_matrix(struct komeda_dev *mdev, struct komeda_crtc *kcrtc,
		       u64 evt)
{
	struct komeda_pipeline *pipe;
	struct malidp_position cur_pos;
	struct malidp_matrix4 inv_cur_mat;
	struct malidp_position *ref_pos = NULL;
	struct malidp_matrix4 *ref_mat = NULL;

	if (get_current_pose(&kcrtc->s_buff, &cur_pos, &inv_cur_mat, true)) {
		ref_pos = &kcrtc->reference_pos;
		ref_mat = &kcrtc->reference_mat;
	}

	pipe = kcrtc->master;
	update_pipeline_atu_matrix(mdev, pipe, evt, &cur_pos, &inv_cur_mat, ref_pos, ref_mat);
	if (kcrtc->slave) {
		pipe = kcrtc->slave;
		update_pipeline_atu_matrix(mdev, pipe, evt, &cur_pos, &inv_cur_mat, ref_pos, ref_mat);
	}
}

void komeda_crtc_handle_atu_event(struct komeda_dev *mdev,
				  struct komeda_crtc *kcrtc,
				  struct komeda_events *evts)
{
	u64 events = evts->pipes[kcrtc->master->id];

	if (events & KOMEDA_EVENT_ASYNC_RP) {
		struct komeda_pipeline *pipe = kcrtc->master;

		if (pipe->postponed_cval) {
			kcrtc->reference_pos = kcrtc->new_pos;
			kcrtc->reference_mat = kcrtc->new_mat;
			komeda_pipeline_state_backup(kcrtc->master);

			if (kcrtc->slave &&
			    has_bit(kcrtc->slave->id,
			    to_kcrtc_st(kcrtc->base.state)->active_pipes))
				komeda_pipeline_state_backup(kcrtc->slave);
		}
		update_crtc_atu_matrix(mdev, kcrtc, KOMEDA_EVENT_ASYNC_RP);
		if (pipe->postponed_cval) {
			pipe->postponed_cval = 0;
			pipe->funcs->flush(pipe, 0);
		}
	}

	if (events & KOMEDA_EVENT_PL3)
		update_crtc_atu_matrix(mdev, kcrtc, KOMEDA_EVENT_PL3);

}

/**
 * komeda_crtc_atomic_check - build display output data flow
 * @crtc: DRM crtc
 * @state: the crtc state object
 *
 * crtc_atomic_check is the final check stage, so beside build a display data
 * pipeline according to the crtc_state, but still needs to release or disable
 * the unclaimed pipeline resources.
 *
 * RETURNS:
 * Zero for success or -errno
 */
static int
komeda_crtc_atomic_check(struct drm_crtc *crtc,
			 struct drm_crtc_state *state)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(state);
	int err;

	if (drm_atomic_crtc_needs_modeset(state))
		komeda_crtc_update_clock_ratio(kcrtc_st);

	if (state->active) {
		err = komeda_build_display_data_flow(kcrtc, kcrtc_st);
		if (err)
			return err;
		/* update reference position and quat matrix */
		get_current_pose(&kcrtc->s_buff, &kcrtc->new_pos, &kcrtc->new_mat, false);
	}

	/* release unclaimed pipeline resources */
	err = komeda_release_unclaimed_resources(kcrtc->slave, kcrtc_st);
	if (err)
		return err;

	err = komeda_release_unclaimed_resources(kcrtc->master, kcrtc_st);
	if (err)
		return err;

	return 0;
}

/* For active a crtc, mainly need two parts of preparation
 * 1. adjust display operation mode.
 * 2. enable needed clk
 */
static int
komeda_crtc_prepare(struct komeda_crtc *kcrtc)
{
	struct komeda_dev *mdev = kcrtc->base.dev->dev_private;
	struct komeda_pipeline *master = kcrtc->master;
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(kcrtc->base.state);
	struct drm_display_mode *mode = &kcrtc_st->base.adjusted_mode;
	u32 new_mode;
	int err;

	mutex_lock(&mdev->lock);

	new_mode = mdev->dpmode | BIT(master->id);
	if (WARN_ON(new_mode == mdev->dpmode)) {
		err = 0;
		goto unlock;
	}

	err = mdev->funcs->change_opmode(mdev, new_mode);
	if (err) {
		DRM_ERROR("failed to change opmode: 0x%x -> 0x%x.\n,",
			  mdev->dpmode, new_mode);
		goto unlock;
	}

	mdev->dpmode = new_mode;
	/* Only need to enable aclk on single display mode, but no need to
	 * enable aclk it on dual display mode, since the dual mode always
	 * switch from single display mode, the aclk already enabled, no need
	 * to enable it again.
	 */
	if (new_mode != KOMEDA_MODE_DUAL_DISP) {
		err = clk_set_rate(mdev->aclk, komeda_crtc_get_aclk(kcrtc_st));
		if (err)
			DRM_ERROR("failed to set aclk.\n");
		err = clk_prepare_enable(mdev->aclk);
		if (err)
			DRM_ERROR("failed to enable aclk.\n");
	}

	err = clk_set_rate(master->pxlclk, mode->crtc_clock * 1000);
	if (err)
		DRM_ERROR("failed to set pxlclk for pipe%d\n", master->id);
	err = clk_prepare_enable(master->pxlclk);
	if (err)
		DRM_ERROR("failed to enable pxl clk for pipe%d.\n", master->id);

	if (kcrtc_st->en_coproc) {
		err = komeda_ad_enable(master, &kcrtc_st->base.adjusted_mode);
		if (err) {
			DRM_ERROR("failed to enable AD for pipe%d.\n", master->id);
			kcrtc_st->en_coproc = false;
		}
	}

unlock:
	mutex_unlock(&mdev->lock);

	return err;
}

static int
komeda_crtc_unprepare(struct komeda_crtc *kcrtc)
{
	struct komeda_dev *mdev = kcrtc->base.dev->dev_private;
	struct komeda_pipeline *master = kcrtc->master;
	u32 new_mode;
	int err;

	mutex_lock(&mdev->lock);

	new_mode = mdev->dpmode & (~BIT(master->id));

	if (WARN_ON(new_mode == mdev->dpmode)) {
		err = 0;
		goto unlock;
	}

	err = mdev->funcs->change_opmode(mdev, new_mode);
	if (err) {
		DRM_ERROR("failed to change opmode: 0x%x -> 0x%x.\n,",
			  mdev->dpmode, new_mode);
		goto unlock;
	}

	mdev->dpmode = new_mode;
	komeda_ad_disable(master);

	clk_disable_unprepare(master->pxlclk);
	if (new_mode == KOMEDA_MODE_INACTIVE)
		clk_disable_unprepare(mdev->aclk);

unlock:
	mutex_unlock(&mdev->lock);

	return err;
}

void komeda_crtc_handle_event(struct komeda_crtc *kcrtc,
			      struct komeda_events *evts)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct komeda_wb_connector *wb_conn = kcrtc->wb_conn;
	u32 events = evts->pipes[kcrtc->master->id];

	if (events & KOMEDA_EVENT_VSYNC)
		drm_crtc_handle_vblank(crtc);

	/* handles writeback event */
	if (events & KOMEDA_EVENT_EOW)
		wb_conn->complete_pipes |= BIT(kcrtc->master->id);

	if (kcrtc->side_by_side &&
	    (evts->pipes[kcrtc->slave->id] & KOMEDA_EVENT_EOW))
		wb_conn->complete_pipes |= BIT(kcrtc->slave->id);

	if (wb_conn->expected_pipes == wb_conn->complete_pipes) {
		wb_conn->complete_pipes = 0;
		drm_writeback_signal_completion(&wb_conn->base, 0);
	}

	if (events & KOMEDA_EVENT_FLIP) {
		unsigned long flags;
		struct drm_pending_vblank_event *event;

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		if (kcrtc->disable_done) {
			complete_all(kcrtc->disable_done);
			kcrtc->disable_done = NULL;
		} else if (crtc->state->event) {
			event = crtc->state->event;
			/*
			 * Consume event before notifying drm core that flip
			 * happened.
			 */
			crtc->state->event = NULL;
			drm_crtc_send_vblank_event(crtc, event);
		} else {
			DRM_WARN("CRTC[%d]: FLIP happen but no pending commit.\n",
				 drm_crtc_index(&kcrtc->base));
		}
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void
komeda_crtc_do_flush(struct drm_crtc *crtc,
		     struct drm_crtc_state *old)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(crtc->state);
	struct komeda_pipeline *master = kcrtc->master;
	struct komeda_pipeline *slave = kcrtc->slave;
	struct komeda_wb_connector *wb_conn = kcrtc->wb_conn;
	struct drm_connector_state *conn_st;

	DRM_DEBUG_ATOMIC("CRTC%d_FLUSH: active_pipes: 0x%x, affected: 0x%x.\n",
			 drm_crtc_index(crtc),
			 kcrtc_st->active_pipes, kcrtc_st->affected_pipes);

	/* step 1: update the pipeline/component state to HW */
	if (has_bit(master->id, kcrtc_st->affected_pipes))
		komeda_pipeline_update(master, old->state);

	if (slave && has_bit(slave->id, kcrtc_st->affected_pipes))
		komeda_pipeline_update(slave, old->state);

	conn_st = wb_conn ? wb_conn->base.base.state : NULL;
	if (conn_st && conn_st->writeback_job)
		drm_writeback_queue_job(&wb_conn->base, conn_st);

	/* step 2: notify the HW to kickoff the update */
	master->funcs->flush(master, kcrtc_st->active_pipes);
}

static void
komeda_crtc_atomic_enable(struct drm_crtc *crtc,
			  struct drm_crtc_state *old)
{
	pm_runtime_get_sync(crtc->dev->dev);
	komeda_crtc_prepare(to_kcrtc(crtc));
	drm_crtc_vblank_on(crtc);
	komeda_crtc_do_flush(crtc, old);
}

static void
komeda_crtc_flush_and_wait_for_flip_done(struct komeda_crtc *kcrtc,
					 struct completion *input_flip_done)
{
	struct drm_device *drm = kcrtc->base.dev;
	struct komeda_pipeline *master = kcrtc->master;
	struct completion *flip_done;
	struct completion temp;
	int timeout;

	/* if caller doesn't send a flip_done, use a private flip_done */
	if (input_flip_done) {
		flip_done = input_flip_done;
	} else {
		init_completion(&temp);
		kcrtc->disable_done = &temp;
		flip_done = &temp;
	}

	master->funcs->flush(master, 0);

	/* wait the flip take affect.*/
	timeout = wait_for_completion_timeout(flip_done, HZ);
	if (timeout == 0) {
		DRM_ERROR("wait pipe%d flip done timeout\n", kcrtc->master->id);
		if (input_flip_done == NULL) {
			unsigned long flags;

			spin_lock_irqsave(&drm->event_lock, flags);
			kcrtc->disable_done = NULL;
			spin_unlock_irqrestore(&drm->event_lock, flags);
		}
	}
}

static void
komeda_crtc_atomic_disable(struct drm_crtc *crtc,
			   struct drm_crtc_state *old)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *old_st = to_kcrtc_st(old);
	struct komeda_pipeline *master = kcrtc->master;
	struct komeda_pipeline *slave  = kcrtc->slave;
	struct completion *disable_done = &crtc->state->commit->flip_done;
	bool needs_phase2 = false;

	DRM_DEBUG_ATOMIC("CRTC%d_DISABLE: active_pipes: 0x%x, affected: 0x%x\n",
			 drm_crtc_index(crtc),
			 old_st->active_pipes, old_st->affected_pipes);

	if (slave && has_bit(slave->id, old_st->active_pipes))
		komeda_pipeline_disable(slave, old->state);

	if (has_bit(master->id, old_st->active_pipes))
		needs_phase2 = komeda_pipeline_disable(master, old->state);

	/* crtc_disable has two scenarios according to the state->active switch.
	 * 1. active -> inactive
	 *    this commit is a disable commit. and the commit will be finished
	 *    or done after the disable operation. on this case we can directly
	 *    use the crtc->state->event to tracking the HW disable operation.
	 * 2. active -> active
	 *    the crtc->commit is not for disable, but a modeset operation when
	 *    crtc is active, such commit actually has been completed by 3
	 *    DRM operations:
	 *    crtc_disable, update_planes(crtc_flush), crtc_enable
	 *    so on this case the crtc->commit is for the whole process.
	 *    we can not use it for tracing the disable, we need a temporary
	 *    flip_done for tracing the disable. and crtc->state->event for
	 *    the crtc_enable operation.
	 *    That's also the reason why skip modeset commit in
	 *    komeda_crtc_atomic_flush()
	 */
	disable_done = (needs_phase2 || crtc->state->active) ?
		       NULL : &crtc->state->commit->flip_done;

	/* wait phase 1 disable done */
	komeda_crtc_flush_and_wait_for_flip_done(kcrtc, disable_done);

	/* phase 2 */
	if (needs_phase2) {
		komeda_pipeline_disable(kcrtc->master, old->state);

		disable_done = crtc->state->active ?
			       NULL : &crtc->state->commit->flip_done;

		komeda_crtc_flush_and_wait_for_flip_done(kcrtc, disable_done);
	}

	drm_crtc_vblank_off(crtc);
	komeda_crtc_unprepare(kcrtc);
	pm_runtime_put(crtc->dev->dev);
	komeda_sensor_buff_put(&kcrtc->s_buff);
}

static void
komeda_crtc_atomic_flush(struct drm_crtc *crtc,
			 struct drm_crtc_state *old)
{
	/* commit with modeset will be handled in enable/disable */
	if (drm_atomic_crtc_needs_modeset(crtc->state))
		return;

	komeda_crtc_do_flush(crtc, old);
}

/*
 * Returns the minimum frequency of the aclk rate (main engine clock) in Hz.
 *
 * The DPU output can be split into two halves, to stay within the bandwidth
 * capabilities of the external link (dual-link mode).
 * In these cases, each output link runs at half the pixel clock rate of the
 * combined display, and has half the number of pixels.
 * Beside split the output, the DPU internal pixel processing also can be split
 * into two halves (LEFT/RIGHT) and handles by two pipelines simultaneously.
 * So if side by side, the pipeline (main engine clock) also can run at half
 * the clock rate of the combined display.
 */
static unsigned long
komeda_calc_min_aclk_rate(struct komeda_crtc *kcrtc,
			  unsigned long pxlclk)
{
	if (kcrtc->master->dual_link && !kcrtc->side_by_side)
		return pxlclk * 2;
	else
		return pxlclk;
}

/* Get current aclk rate that specified by state */
unsigned long komeda_crtc_get_aclk(struct komeda_crtc_state *kcrtc_st)
{
	struct drm_crtc *crtc = kcrtc_st->base.crtc;
	struct komeda_dev *mdev = crtc->dev->dev_private;
	unsigned long pxlclk = kcrtc_st->base.adjusted_mode.crtc_clock * 1000;
	unsigned long min_aclk;

	min_aclk = komeda_calc_min_aclk_rate(to_kcrtc(crtc), pxlclk);

	return clk_round_rate(mdev->aclk, min_aclk);
}

static enum drm_mode_status
komeda_crtc_mode_valid(struct drm_crtc *crtc, const struct drm_display_mode *m)
{
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_pipeline *master = kcrtc->master;
	struct komeda_compiz *compiz = master->compiz;
	unsigned long min_pxlclk, min_aclk, delta, full_frame;
	int hdisplay = m->hdisplay;

	if (m->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	full_frame = m->htotal * m->vtotal;
	delta = abs(m->clock * 1000 - m->vrefresh * full_frame);
	if (m->vrefresh && (delta > full_frame)) {
		DRM_DEBUG_ATOMIC("mode clock check error!\n");
		return MODE_CLOCK_RANGE;
	}

	if (kcrtc->side_by_side)
		hdisplay /= 2;

	if (!in_range(&compiz->hsize, hdisplay)) {
		DRM_DEBUG_ATOMIC("hdisplay[%u] is out of range[%u, %u]!\n",
				 hdisplay, compiz->hsize.start,
				 compiz->hsize.end);
		return MODE_BAD_HVALUE;
	}

	if (!in_range(&compiz->vsize, m->vdisplay)) {
		DRM_DEBUG_ATOMIC("vdisplay[%u] is out of range[%u, %u]!\n",
				 m->vdisplay, compiz->vsize.start,
				 compiz->vsize.end);
		return MODE_BAD_VVALUE;
	}

	min_pxlclk = m->clock * 1000;
	if (master->dual_link)
		min_pxlclk /= 2;

	if (min_pxlclk != clk_round_rate(master->pxlclk, min_pxlclk)) {
		DRM_DEBUG_ATOMIC("pxlclk doesn't support %lu Hz\n", min_pxlclk);

		return MODE_NOCLOCK;
	}

	min_aclk = komeda_calc_min_aclk_rate(to_kcrtc(crtc), min_pxlclk);
	if (clk_round_rate(mdev->aclk, min_aclk) < min_aclk) {
		DRM_DEBUG_ATOMIC("engine clk can't satisfy the requirement of %s-clk: %lu.\n",
				 m->name, min_pxlclk);

		return MODE_CLOCK_HIGH;
	}

	return MODE_OK;
}

static bool komeda_crtc_mode_fixup(struct drm_crtc *crtc,
				   const struct drm_display_mode *m,
				   struct drm_display_mode *adjusted_mode)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	unsigned long clk_rate;

	drm_mode_set_crtcinfo(adjusted_mode, 0);
	/* In dual link half the horizontal settings */
	if (kcrtc->master->dual_link) {
		adjusted_mode->crtc_clock /= 2;
		adjusted_mode->crtc_hdisplay /= 2;
		adjusted_mode->crtc_hsync_start /= 2;
		adjusted_mode->crtc_hsync_end /= 2;
		adjusted_mode->crtc_htotal /= 2;
	}

	clk_rate = adjusted_mode->crtc_clock * 1000;
	/* crtc_clock will be used as the komeda output pixel clock */
	adjusted_mode->crtc_clock = clk_round_rate(kcrtc->master->pxlclk,
						   clk_rate) / 1000;

	return true;
}

static const struct drm_crtc_helper_funcs komeda_crtc_helper_funcs = {
	.atomic_check	= komeda_crtc_atomic_check,
	.atomic_flush	= komeda_crtc_atomic_flush,
	.atomic_enable	= komeda_crtc_atomic_enable,
	.atomic_disable	= komeda_crtc_atomic_disable,
	.mode_valid	= komeda_crtc_mode_valid,
	.mode_fixup	= komeda_crtc_mode_fixup,
};

static void komeda_crtc_reset(struct drm_crtc *crtc)
{
	struct komeda_crtc_state *state;

	if (crtc->state)
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

	kfree(to_kcrtc_st(crtc->state));
	crtc->state = NULL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state) {
		state->en_coproc = false;
		state->strength_limit = 1;
		state->drc = AD_MAX_DRC;
		crtc->state = &state->base;
		crtc->state->crtc = crtc;
		state->en_protected_mode = false;
	}
}

static struct drm_crtc_state *
komeda_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct komeda_crtc_state *old = to_kcrtc_st(crtc->state);
	struct komeda_crtc_state *new;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &new->base);

	new->affected_pipes = old->active_pipes;
	new->clock_ratio = old->clock_ratio;
	new->max_slave_zorder = old->max_slave_zorder;
	new->en_protected_mode = old->en_protected_mode;
	new->en_coproc = old->en_coproc;
	new->assertiveness = old->assertiveness;
	new->strength_limit = old->strength_limit;
	new->drc = old->drc;
	new->pl3 = old->pl3;

	return &new->base;
}

static void komeda_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					     struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_kcrtc_st(state));
}

static int komeda_crtc_vblank_enable(struct drm_crtc *crtc)
{
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);

	mdev->funcs->on_off_vblank(mdev, kcrtc->master->id, true);
	return 0;
}

static void komeda_crtc_vblank_disable(struct drm_crtc *crtc)
{
	struct komeda_dev *mdev = crtc->dev->dev_private;
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);

	mdev->funcs->on_off_vblank(mdev, kcrtc->master->id, false);
}

static int komeda_crtc_atomic_get_property(struct drm_crtc *crtc,
		const struct drm_crtc_state *state,
		struct drm_property *property, uint64_t *val)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(state);
	struct drm_property_blob *b;

	if (property == kcrtc->protected_mode_property)
		*val = kcrtc_st->en_protected_mode;
	else if (property == kcrtc->coproc_property)
		*val = kcrtc_st->en_coproc;
	else if (property == kcrtc->assertiveness_property)
		*val = kcrtc_st->assertiveness;
	else if (property == kcrtc->strength_limit_property)
		*val = kcrtc_st->strength_limit;
	else if (property == kcrtc->drc_property)
		*val = kcrtc_st->drc;
	else if (property == kcrtc->sensor_buf_property) {
		b = kcrtc->s_buff.sensor_buf_info_blob;
		*val = (b) ? b->base.id : 0;
	} else {
		DRM_DEBUG_DRIVER("Unknown property %s\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int komeda_crtc_atomic_set_property(struct drm_crtc *crtc,
		struct drm_crtc_state *state,
		struct drm_property *property, uint64_t val)
{
	struct komeda_crtc *kcrtc = to_kcrtc(crtc);
	struct komeda_crtc_state *kcrtc_st = to_kcrtc_st(state);
	bool replaced = false;
	int ret = 0;

	if (property == kcrtc->protected_mode_property)
		kcrtc_st->en_protected_mode = !!val;
	else if (property == kcrtc->coproc_property)
		kcrtc_st->en_coproc = !!val;
	else if (property == kcrtc->assertiveness_property) {
		if (kcrtc_st->assertiveness != val) {
			kcrtc_st->assertiveness = val;
			kcrtc_st->assertive_changed = true;
		}
	} else if (property == kcrtc->strength_limit_property) {
		if (kcrtc_st->strength_limit != val) {
			kcrtc_st->strength_limit = val;
			kcrtc_st->strength_changed = true;
		}
	} else if (property == kcrtc->drc_property) {
		if (kcrtc_st->drc != val) {
			kcrtc_st->drc = val;
			kcrtc_st->drc_changed = true;
		}
	} else if (property == kcrtc->sensor_buf_property) {
		komeda_sensor_buff_put(&kcrtc->s_buff);
		ret = drm_property_replace_blob_from_id(crtc->dev,
					&kcrtc->s_buff.sensor_buf_info_blob,
					val,
					sizeof(struct malidp_sensor_buffer_info),
					sizeof(struct malidp_sensor_buffer_info),
					&replaced);
		if (!ret)
			ret = komeda_sensor_buff_get(&kcrtc->s_buff);
	} else {
		DRM_DEBUG_DRIVER("Unknown property %s\n", property->name);
		return -EINVAL;
	}

	return 0;
}

static int create_coprocessor_property(struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_property *prop;

	prop = drm_property_create_bool(crtc->dev, DRM_MODE_PROP_ATOMIC,
					"coprocessor");
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, 0);
	kcrtc->coproc_property = prop;

	return 0;
}

static int komeda_crtc_create_protected_mode_property(struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_property *prop;

	prop = drm_property_create_bool(crtc->dev, DRM_MODE_PROP_ATOMIC,
					"PROTECTED_MODE");
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, 0);
	kcrtc->protected_mode_property = prop;

	return 0;
}

static const struct drm_crtc_funcs komeda_crtc_funcs = {
	.gamma_set		= drm_atomic_helper_legacy_gamma_set,
	.destroy		= drm_crtc_cleanup,
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= komeda_crtc_reset,
	.atomic_duplicate_state	= komeda_crtc_atomic_duplicate_state,
	.atomic_destroy_state	= komeda_crtc_atomic_destroy_state,
	.enable_vblank		= komeda_crtc_vblank_enable,
	.disable_vblank		= komeda_crtc_vblank_disable,
	.atomic_get_property	= komeda_crtc_atomic_get_property,
	.atomic_set_property	= komeda_crtc_atomic_set_property,
};

static int create_ad_assertiveness_property(struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_property *prop;

	if (!kcrtc->master->ad->funcs->assertiveness_set)
		return 0;

	prop = drm_property_create_range(crtc->dev, DRM_MODE_PROP_ATOMIC,
				         "assertiveness", 0,
					 AD_MAX_ASSERTIVENESS);
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, 0);
	kcrtc->assertiveness_property = prop;

	return 0;
}

static int create_ad_stength_property(struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_property *prop;

	if (!kcrtc->master->ad->funcs->strength_set)
		return 0;

	prop = drm_property_create_range(crtc->dev, DRM_MODE_PROP_ATOMIC,
				         "strength_limit", 1,
					 AD_MAX_STRENGTH_LIMIT);
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, 1);
	kcrtc->strength_limit_property = prop;

	return 0;
}

static int create_ad_drc_property(struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_property *prop;

	if (!kcrtc->master->ad->funcs->drc_set)
		return 0;

	prop = drm_property_create_range(crtc->dev, DRM_MODE_PROP_ATOMIC,
				         "drc", 0, AD_MAX_DRC);
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, AD_MAX_DRC);
	kcrtc->drc_property = prop;

	return 0;
}

static int
komeda_crtc_create_ad_properties(struct komeda_crtc *kcrtc)
{
	int ret;

	if (!kcrtc->master->ad)
		return 0;

	ret = create_ad_assertiveness_property(kcrtc);
	if (ret)
		return ret;

	ret = create_ad_stength_property(kcrtc);
	if (ret)
		return ret;

	ret = create_ad_drc_property(kcrtc);
	if (ret)
		return ret;

	return create_coprocessor_property(kcrtc);
}


int komeda_kms_setup_crtcs(struct komeda_kms_dev *kms,
			   struct komeda_dev *mdev)
{
	struct komeda_crtc *crtc;
	struct komeda_pipeline *master;
	char str[16];
	int i;

	kms->n_crtcs = 0;

	for (i = 0; i < mdev->n_pipelines; i++) {
		crtc = &kms->crtcs[kms->n_crtcs];
		master = mdev->pipelines[i];

		crtc->master = master;
		crtc->slave  = komeda_pipeline_get_slave(master);
		crtc->side_by_side = mdev->side_by_side;

		if (crtc->slave)
			sprintf(str, "pipe-%d", crtc->slave->id);
		else
			sprintf(str, "None");

		DRM_INFO("CRTC-%d: master(pipe-%d) slave(%s) sbs(%s).\n",
			 kms->n_crtcs, master->id, str,
			 crtc->side_by_side ? "On" : "Off");

		kms->n_crtcs++;

		if (mdev->side_by_side)
			break;
	}

	return 0;
}

static struct drm_plane *
get_crtc_primary(struct komeda_kms_dev *kms, struct komeda_crtc *crtc)
{
	struct komeda_plane *kplane;
	struct drm_plane *plane;

	drm_for_each_plane(plane, &kms->base) {
		if (plane->type != DRM_PLANE_TYPE_PRIMARY)
			continue;

		kplane = to_kplane(plane);
		/* only master can be primary */
		if (kplane->layer->base.pipeline == crtc->master)
			return plane;
	}

	return NULL;
}

static int komeda_crtc_create_sensor_buf_property(struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	struct drm_property *prop;

	prop = drm_property_create(crtc->dev,
			DRM_MODE_PROP_ATOMIC | DRM_MODE_PROP_BLOB,
			"sensor_buf_info", 0);
	if (!prop)
		return -ENOMEM;

	kcrtc->sensor_buf_property = prop;
	drm_object_attach_property(&crtc->base, prop, 0);
	return 0;
}

static int komeda_crtc_add(struct komeda_kms_dev *kms,
			   struct komeda_crtc *kcrtc)
{
	struct drm_crtc *crtc = &kcrtc->base;
	int err;

	err = drm_crtc_init_with_planes(&kms->base, crtc,
					get_crtc_primary(kms, kcrtc), NULL,
					&komeda_crtc_funcs, NULL);
	if (err)
		return err;

	drm_crtc_helper_add(crtc, &komeda_crtc_helper_funcs);
	drm_crtc_vblank_reset(crtc);

	crtc->port = kcrtc->master->of_output_port;

	drm_crtc_enable_color_mgmt(crtc, 0, true, KOMEDA_COLOR_LUT_SIZE);

	err = komeda_crtc_create_protected_mode_property(kcrtc);
	if (err)
		return err;

	if (kcrtc->master->n_atus > 0)
		err = komeda_crtc_create_sensor_buf_property(kcrtc);

	return err;
}

int komeda_kms_add_crtcs(struct komeda_kms_dev *kms, struct komeda_dev *mdev)
{
	int i, err;

	for (i = 0; i < kms->n_crtcs; i++) {
		err = komeda_crtc_add(kms, &kms->crtcs[i]);
		if (err)
			return err;
	}

	return 0;
}

int komeda_kms_crtcs_add_ad_properties(struct komeda_kms_dev *kms,
				       struct komeda_dev *mdev)
{
	int i, err;

	for (i = 0; i < kms->n_crtcs; i++) {
		err = komeda_crtc_create_ad_properties(&kms->crtcs[i]);
		if (err)
			return err;
	}

	return 0;
}
