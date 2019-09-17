// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Mihail Atanassov <mihail.atanassov@arm.com>
 *
 */

#include <linux/pm_runtime.h>

#include <drm/drm_debugfs.h>
#include <drm/drm_file.h>

#include "komeda_debugfs.h"
#include "komeda_dev.h"
#include "komeda_kms.h"

static struct komeda_dev *
node_to_mdev(struct drm_info_node *node)
{
	struct drm_device *drm = node->minor->dev;

	return drm->dev_private;
}

static const char *
bool_to_onoff(bool b)
{
	return (b) ? "ON" : "OFF";
}

static const char *
vp_mode_to_str(enum komeda_atu_mode mode)
{
	switch (mode) {
	case ATU_MODE_VP0_VP1_SEQ:	return "ATU_MODE_VP0_VP1_SEQ";
	case ATU_MODE_VP0:		return "ATU_MODE_VP0";
	case ATU_MODE_VP1:		return "ATU_MODE_VP1";
	case ATU_MODE_VP0_VP1_SIMULT:	return "ATU_MODE_VP0_VP1_SIMULT";
	case ATU_MODE_VP0_VP1_INT:	return "ATU_MODE_VP0_VP1_INT";
	case ATU_MODE_INVAL_OVERLAP:	return "ATU_MODE_INVAL_OVERLAP";
	case ATU_MODE_INVAL_ORDER:	return "ATU_MODE_INVAL_ORDER";
	}
	return "Undefined";
}

static void dump_vp_state(struct seq_file *m, const char *vp_name,
			struct komeda_atu_vp_state *st)
{
	u32 interp = 0;

	seq_printf(m, "\t%s:\n", vp_name);

	seq_printf(m, "\t\tVP_OUTPUT_SIZE=%dx%d\n",
			st->out_hsize, st->out_vsize);
	seq_printf(m, "\t\tNODES=%d, %d\n", st->hnodes, st->vnodes);
	seq_printf(m, "\t\tDNORM=%d, %d\n", st->hdnorm, st->vdnorm);
	seq_printf(m, "\t\tSTEP=%d, %d\n", st->hstep, st->vstep);
	seq_printf(m, "\t\tRSHIFT=%d, %d\n", st->hrshift, st->vrshift);
	if (st->out_vsize != 1)
		interp = (2 << 15) / (st->out_vsize - 1);
	seq_printf(m, "\t\tHSCALE=0x%X\n", st->hscale);
	seq_printf(m, "\t\tINTERP=%u\n", interp);
	seq_printf(m, "\t\tLDC: %s\n", bool_to_onoff(st->lc_enabled));
	seq_printf(m, "\t\tCAC: %s\n", bool_to_onoff(st->cac_enabled));
	seq_printf(m, "\t\tCLE: %s\n", bool_to_onoff(st->clamp_enabled));
	seq_printf(m, "\t\tRPJ: %s\n", bool_to_onoff(st->vp_type != ATU_VP_TYPE_NONE));
}

static void
dump_atu_state(struct seq_file *m, struct komeda_atu *atu, int idx)
{
	struct komeda_component_state *c_st =
					priv_to_comp_st(atu->base.obj.state);
	struct komeda_atu_state *st = to_atu_st(c_st);

	seq_printf(m, "ATU%d:\n", idx);
	seq_printf(m, "\tATU_OUTPUT_SIZE=%dx%d\n", st->hsize, st->vsize);
	seq_printf(m, "\tATU_VP_MODE: %s\n", vp_mode_to_str(st->mode));

	dump_vp_state(m, "Left VP", &st->left);
	dump_vp_state(m, "Right VP", &st->right);

}

static int komeda_atu_status(struct seq_file *m, void *data)
{
	struct komeda_dev *mdev = node_to_mdev(m->private);
	int i, j, idx;

	for(i = 0; i < mdev->n_pipelines; i++) {
		struct komeda_pipeline *p = mdev->pipelines[i];
		struct komeda_pipeline_state *st;

		if (p == NULL || p->n_atus == 0)
			continue;

		st = priv_to_pipe_st(p->obj.state);
		if (st->crtc == NULL)
			continue;

		if (to_kcrtc(st->crtc)->master == p)
			idx = 0;
		else
			idx = p->n_atus;

		for(j = 0; j < p->n_atus; j++) {
			struct komeda_atu *atu = p->atu[j];
			if (st->active_comps & BIT(atu->base.id))
				dump_atu_state(m, atu, idx + j);
		}
	}

	return 0;
}

static const struct drm_info_list komeda_debugfs_list[] = {
	{"komeda_atu_status", komeda_atu_status, 0},
};
#define KOMEDA_DEBUGFS_ITEMS ARRAY_SIZE(komeda_debugfs_list)

int komeda_kms_debugfs_register(struct drm_minor *minor)
{
	int ret;

	ret = drm_debugfs_create_files(komeda_debugfs_list,
				       KOMEDA_DEBUGFS_ITEMS,
				       minor->debugfs_root, minor);
	if (ret)
		DRM_WARN("drm_debugfs_create_files failed!\n");
	return ret;
}
