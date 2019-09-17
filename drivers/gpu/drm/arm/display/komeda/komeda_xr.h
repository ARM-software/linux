/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 *
 */
#ifndef _KOMEDA_XR_H_
#define _KOMEDA_XR_H_

#include "malidp_math.h"

struct komeda_atu;
struct komeda_crtc;
struct komeda_pipeline;
struct komeda_dev;
struct komeda_events;

enum komeda_atu_mode {
	ATU_MODE_INVAL_OVERLAP	= -2,
	ATU_MODE_INVAL_ORDER	= -1,
	ATU_MODE_VP0_VP1_SEQ	= 0,
	ATU_MODE_VP0		= 1,
	ATU_MODE_VP1		= 2,
	ATU_MODE_VP0_VP1_SIMULT	= 3,
	ATU_MODE_VP0_VP1_INT	= 4
};

enum komeda_atu_vp_type {
	ATU_VP_TYPE_NONE = 0,
	ATU_VP_TYPE_PROJ = 1,
	ATU_VP_TYPE_QUAD =2
};

/**
 * struct komeda_atu_vp_calc
 *
 * komeda_atu_vp_calc is the core structure for viewport
 * reprojection calculaiton.
 */
struct komeda_atu_vp_calc {
	/**
	 * @vp_type
	 * viewport type
	 */
	enum komeda_atu_vp_type vp_type;
	/**
	 * @m1 and @m2
	 * input matrics for reprojection
	 */
	struct malidp_matrix4 m1, m2;
	/**
	 * @A and @B
	 * the result matrix for viewport matrix A and B
	 */
	struct malidp_matrix3 A, B;
};

/**
 * struct komeda_atu_async_rp_job
 *
 * komeda_atu_async_rp_job is a data block for asynchronous reprojection.
 * The job is added into job queue which is in komeda_atu and is used in
 * asynchronous reporject thread.
 */
struct komeda_atu_async_rp_job {
	/**
	 * &ref
	 *
	 * Reference counter
	 */
	refcount_t ref;
	/**
	 * @atu
	 *
	 * Back-pointer to the komeda_atu
	 */
	struct komeda_atu *atu;

	/**
	 * @list_entry
	 *
	 * List item for the komeda_atu's @atu_job_queue
	 */
	struct list_head list_entry;

	struct malidp_matrix4 reference_mat;
	struct malidp_position reference_pos;

	enum komeda_atu_mode mode;

	struct komeda_atu_vp_calc left, right;
};

struct komeda_atu_async_rp_job* komeda_atu_job_get(struct komeda_atu *atu);
void komeda_atu_job_put(struct komeda_atu_async_rp_job *job);
int komeda_atu_create_rp_job(struct komeda_atu *atu);
void komeda_atu_clean_job(struct komeda_atu *atu);

bool komeda_pipeline_prepare_atu_job(struct komeda_pipeline *pipe);
bool komeda_pipeline_has_atu_enabled(struct komeda_pipeline *pipe);

void komeda_crtc_handle_atu_event(struct komeda_dev *mdev,
				  struct komeda_crtc *kcrtc,
				  struct komeda_events *evts);
bool komeda_crtc_get_current_pose(struct komeda_crtc *kcrtc,
			struct malidp_position *cur_pos,
			struct malidp_matrix4 *cur_mat,
			bool en_inv);
#endif /* _KOMEDA_XR_H_*/
