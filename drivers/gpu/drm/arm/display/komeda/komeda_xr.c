// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 *
 */
#include <drm/drm_print.h>

#include "komeda_dev.h"
#include "komeda_kms.h"
#include "komeda_pipeline.h"

void komeda_atu_job_put(struct komeda_atu_async_rp_job *job)
{
	if (refcount_dec_and_test(&job->ref))
		kfree(job);
}

struct komeda_atu_async_rp_job* komeda_atu_job_get(struct komeda_atu *atu)
{
	struct komeda_atu_async_rp_job *job = NULL;
	unsigned long flags;

	spin_lock_irqsave(&atu->job_lock, flags);
	if (!list_empty(&atu->atu_job_queue)) {
		job = list_first_entry(&atu->atu_job_queue,
				       struct komeda_atu_async_rp_job,
				       list_entry);
		refcount_inc(&job->ref);
	}
	spin_unlock_irqrestore(&atu->job_lock, flags);

	return job;
}

void komeda_atu_clean_job(struct komeda_atu *atu)
{
	struct komeda_atu_async_rp_job *job = NULL;
	unsigned long flags;

	spin_lock_irqsave(&atu->job_lock, flags);
	if (!list_empty(&atu->atu_job_queue)) {
		job = list_first_entry(&atu->atu_job_queue,
				       struct komeda_atu_async_rp_job,
				       list_entry);
		list_del(&job->list_entry);
	}
	spin_unlock_irqrestore(&atu->job_lock, flags);

	if (job)
		komeda_atu_job_put(job);
}

int komeda_atu_create_rp_job(struct komeda_atu *atu)
{
	struct komeda_atu_async_rp_job *job = NULL;
	struct komeda_atu_state *state = priv_to_atu_st(atu);
	unsigned long flags;

	job = kzalloc(sizeof(*job), GFP_KERNEL);
	if (!job)
		return -ENOMEM;

	refcount_set(&job->ref, 1);
	job->atu = atu;
	job->reference_mat = atu->new_mat;
	job->reference_pos = atu->new_pos;
	job->mode = state->mode;
	job->left.vp_type = state->left.vp_type;
	job->left.m1 = state->left.m1;
	job->left.m2 = state->left.m2;
	job->right.vp_type = state->right.vp_type;
	job->right.m1 = state->right.m1;
	job->right.m2 = state->right.m2;
	/* only one job should be queued */
	komeda_atu_clean_job(atu);
	spin_lock_irqsave(&atu->job_lock, flags);
	list_add_tail(&job->list_entry, &atu->atu_job_queue);
	spin_unlock_irqrestore(&atu->job_lock, flags);

	return 0;
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
update_atu_vp_matrix(struct komeda_atu_async_rp_job *job, bool is_left,
		     struct malidp_position *cur_pos,
		     struct malidp_matrix4 *inv_cur_mat, bool is_new_pos)
{
	struct komeda_atu_vp_calc *calc = (is_left) ? &job->left : &job->right;
	struct round_exception extra_data;
	struct malidp_matrix4 tmp1, tmp2;

	if (!is_new_pos)
		goto skip_trans_matrix;

	switch (calc->vp_type) {
	case ATU_VP_TYPE_PROJ:
		update_projection_layer_transform_matrix(&calc->m1,
					     &calc->m2,
					     &job->reference_mat,
					     inv_cur_mat,
					     &tmp1);
		break;
	case ATU_VP_TYPE_QUAD:
		update_quad_layer_transform_matrix(cur_pos, inv_cur_mat,
			&job->reference_pos, &job->reference_mat,
			&calc->m1, &calc->m2, &tmp1);
		break;
	default:
		DRM_DEBUG_ATOMIC("ATU viewport enabled with bad mode!(should be proj or quad)\n");
		return;
	}

	matrix_mul_4X4(&tmp1, &calc->m1, &tmp2);
	matrix_4X4_to_3X3(&tmp2, &calc->A);
	matrix_4X4_to_3X3(&tmp2, &calc->B);
	normalize_matrix3(&calc->A, &extra_data);
	if (extra_data.exception & float32_exception_invalid) {
		DRM_DEBUG_ATOMIC("Normalize Matrix A error!\n");
		return;
	}
	normalize_matrix3(&calc->B, &extra_data);
	if (extra_data.exception & float32_exception_invalid) {
		DRM_DEBUG_ATOMIC("Normalize Matrix B error!\n");
		return;
	}

	matrix_to_q_format(&calc->A);
	matrix_to_q_format(&calc->B);
	return;

skip_trans_matrix:
	identity_matrix3(&calc->A);
	identity_matrix3(&calc->B);
}


/* atu functions for komeda pipeline */
bool komeda_pipeline_prepare_atu_job(struct komeda_pipeline *pipe)
{
	struct komeda_pipeline_state *pipe_st = priv_to_pipe_st(pipe->obj.state);
	u32 i, active_atu = 0;

	if (!pipe->n_atus)
		return false;

	for (i = 0; i < pipe->n_atus; i++) {
		struct komeda_atu *atu = pipe->atu[i];

		if (!atu)
			continue;
		/* only active ATU has job in queue */
		if (has_bit(atu->base.id, pipe_st->active_comps)) {
			if (komeda_atu_create_rp_job(atu)) {
				DRM_DEBUG_ATOMIC("Create async reprojection job failed!\n");
				return false;
			}
			active_atu++;
		} else
			komeda_atu_clean_job(atu);
	}

	return !!active_atu;
}

bool komeda_pipeline_has_atu_enabled(struct komeda_pipeline *pipe)
{
	struct komeda_pipeline_state *pipe_st = priv_to_pipe_st(pipe->obj.state);

	return !!(pipe_st->active_comps & KOMEDA_PIPELINE_ATUS);
}

static void
update_pipeline_atu_matrix(struct komeda_dev *mdev, struct komeda_pipeline *pipe, u64 evt,
			   struct malidp_position *cur_pos, struct malidp_matrix4 *inv_cur_mat,
			   bool is_new_pos)
{
	u32 i;

	for (i = 0; i < pipe->n_atus; i++)  {
		struct komeda_atu *atu = pipe->atu[i];
		struct komeda_atu_async_rp_job *job;

		if (evt == KOMEDA_EVENT_ASYNC_RP) {
			/* PL2 */
			job = komeda_atu_job_get(atu);
			atu->curr_job = job;
			if (!job)
				continue;

			if (job->left.vp_type != ATU_VP_TYPE_NONE)
				update_atu_vp_matrix(job, true,
					     cur_pos, inv_cur_mat, is_new_pos);
			if ((job->right.vp_type != ATU_VP_TYPE_NONE &&
			    job->mode != ATU_MODE_VP0_VP1_SEQ) ||
			    (job->right.vp_type != ATU_VP_TYPE_NONE &&
			    job->mode == ATU_MODE_VP0_VP1_SEQ && !is_new_pos))
				update_atu_vp_matrix(job, false,
					     cur_pos, inv_cur_mat, is_new_pos);
		} else 	{
			job = atu->curr_job;
			if (!job)
				continue;

			if (job->mode == ATU_MODE_VP0_VP1_SEQ) {
				/* PL3 only flush right vp under SEQ mode */
				if (job->right.vp_type != ATU_VP_TYPE_NONE)
					update_atu_vp_matrix(job, false,
						cur_pos, inv_cur_mat, is_new_pos);
			}
		}

		mdev->funcs->latch_matrix(job);
		if (evt == KOMEDA_EVENT_PL3) {
			komeda_atu_job_put(job);
			atu->curr_job = NULL;
		}
	}
}

/* atu functions for komeda crtc */
static void
update_crtc_atu_matrix(struct komeda_dev *mdev, struct komeda_crtc *kcrtc,
		       u64 evt)
{
	struct komeda_pipeline *pipe;
	struct malidp_position cur_pos;
	struct malidp_matrix4 inv_cur_mat;
	bool is_new_pos = komeda_crtc_get_current_pose(kcrtc, &cur_pos,
					   &inv_cur_mat, true);

	pipe = kcrtc->master;
	update_pipeline_atu_matrix(mdev, pipe, evt, &cur_pos,
				   &inv_cur_mat, is_new_pos);
	if (kcrtc->slave) {
		pipe = kcrtc->slave;
		update_pipeline_atu_matrix(mdev, pipe, evt, &cur_pos,
					   &inv_cur_mat, is_new_pos);
	}
}

void komeda_crtc_handle_atu_event(struct komeda_dev *mdev,
				  struct komeda_crtc *kcrtc,
				  struct komeda_events *evts)
{
	u64 events = evts->pipes[kcrtc->master->id];

	if (events & KOMEDA_EVENT_ASYNC_RP) {
		struct komeda_pipeline *pipe = kcrtc->master;

		update_crtc_atu_matrix(mdev, kcrtc, KOMEDA_EVENT_ASYNC_RP);
		if (pipe->postponed_cval) {
			pipe->postponed_cval = 0;
			pipe->funcs->flush(pipe, 0);
		}
	}

	if (events & KOMEDA_EVENT_PL3)
		update_crtc_atu_matrix(mdev, kcrtc, KOMEDA_EVENT_PL3);

}

/* if en_inv is true, save inv_cur_mat into cur_mat */
bool komeda_crtc_get_current_pose(struct komeda_crtc *kcrtc,
		 struct malidp_position *cur_pos,
		 struct malidp_matrix4 *cur_mat,
		 bool en_inv)
{
	struct komeda_sensor_buff *sb_buff = &kcrtc->s_buff;
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
