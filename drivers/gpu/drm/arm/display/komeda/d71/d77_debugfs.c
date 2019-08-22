// SPDX-License-Identifier: GPL-2.0
/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 *
 */
#include <linux/debugfs.h>
#include "d77_debugfs.h"
#include "malidp_io.h"
#include "d71_dev.h"

#define FULL_32		0
#define LOW_16		1
#define HIGH_AND_LOW	2

struct perf_desc {
	char *name;
	u32 flag;
};

static const struct perf_desc pf_desc[] = {
	{"perf_c0_l0p01_axi_min", HIGH_AND_LOW},
	{"perf_c1_l0p2_axi_min", LOW_16},
	{"perf_c2_l0p01_axi_max", HIGH_AND_LOW},
	{"perf_c3_l0p2_axi_max", LOW_16},
	{"perf_c4_l1p01_axi_min", HIGH_AND_LOW},
	{"perf_c5_l1p01_axi_max", HIGH_AND_LOW},
	{"perf_c6_l2p01_axi_min", HIGH_AND_LOW},
	{"perf_c7_l2p2_axi_min", LOW_16},
	{"perf_c8_l2p01_axi_max", HIGH_AND_LOW},
	{"perf_c9_l2p2_axi_max", LOW_16},
	{"perf_c10_l3p01_axi_min", HIGH_AND_LOW},
	{"perf_c11_l3p01_axi_max", HIGH_AND_LOW},
	{"perf_c12_lwp01_axi_min", HIGH_AND_LOW},
	{"perf_c13_lwp01_axi_max", HIGH_AND_LOW},
	{"perf_c14_l0p01_dti_min", HIGH_AND_LOW},
	{"perf_c15_l0p2_dti_min", LOW_16},
	{"perf_c16_l0p01_dti_max", HIGH_AND_LOW},
	{"perf_c17_l0p2_dti_max", LOW_16},
	{"perf_c18_l1p01_dti_min", HIGH_AND_LOW},
	{"perf_c19_l1p01_dti_max", HIGH_AND_LOW},
	{"perf_c20_l2p01_dti_min", HIGH_AND_LOW},
	{"perf_c21_l2p2_dti_min", LOW_16},
	{"perf_c22_l2p01_dti_max", HIGH_AND_LOW},
	{"perf_c23_l2p2_dti_max", LOW_16},
	{"perf_c24_l3p01_dti_min", HIGH_AND_LOW},
	{"perf_c25_l3p01_dti_max", HIGH_AND_LOW},
	{"perf_c26_lwp01_dti_min", HIGH_AND_LOW},
	{"perf_c27_lwp01_dti_max", HIGH_AND_LOW},
	{"perf_c28_l0p0_axi_transf", FULL_32},
	{"perf_c29_l0p1_axi_transf", FULL_32},
	{"perf_c30_l0p2_axi_transf", FULL_32},
	{"perf_c31_l1p0_axi_transf", FULL_32},
	{"perf_c32_l1p1_axi_transf", FULL_32},
	{"perf_c33_l2p0_axi_transf", FULL_32},
	{"perf_c34_l2p1_axi_transf", FULL_32},
	{"perf_c35_l2p2_axi_transf", FULL_32},
	{"perf_c36_l3p0_axi_transf", FULL_32},
	{"perf_c37_l3p1_axi_transf", FULL_32},
	{"perf_c38_lwp0_axi_transf", FULL_32},
	{"perf_c39_lwp1_axi_transf", FULL_32},
	{"perf_c40_l0_fifo_lvl_min", HIGH_AND_LOW},
	{"perf_c41_l1_fifo_lvl_min", HIGH_AND_LOW},
	{"perf_c42_l2_fifo_lvl_min", HIGH_AND_LOW},
	{"perf_c43_l3_fifo_lvl_min", HIGH_AND_LOW},
	{"perf_c44_lw_fifo_lvl_man", HIGH_AND_LOW},
	{"perf_c45_atua_vp0_num_ext_req", FULL_32},
	{"perf_c46_atua_vp1_num_ext_req", FULL_32},
	{"perf_c47_atub_vp0_num_ext_req", FULL_32},
	{"perf_c48_atub_vp1_num_ext_req", FULL_32},
	{"perf_c49_atua_vp0_num_int_req", FULL_32},
	{"perf_c50_atua_vp1_num_int_req", FULL_32},
	{"perf_c51_atub_vp0_num_int_req", FULL_32},
	{"perf_c52_atub_vp1_num_int_req", FULL_32},
	{"perf_c53_atua_num_evict", HIGH_AND_LOW},
	{"perf_c54_atub_num_evict", HIGH_AND_LOW},
	{"perf_c55_atua_tc_lvl_max", HIGH_AND_LOW},
	{"perf_c56_atub_tc_lvl_max", HIGH_AND_LOW}
};

static int d77_perf_counters_show(struct seq_file *sf, void *x)
{
	struct d77_perf *perf = sf->private;
	u32 v;
	int i;

	for (i = 0; i < ARRAY_SIZE(pf_desc); i++) {
		if (!has_bit(i, perf->perf_mask))
			continue;

		seq_printf(sf, "%s: ", pf_desc[i].name);
		switch (pf_desc[i].flag) {
		case FULL_32:
			seq_printf(sf, "0x%08X\n", perf->perf_counters[i]);
			break;
		case LOW_16:
			seq_printf(sf, "0x%04X\n", perf->perf_counters[i]);
			break;
		case HIGH_AND_LOW:
			v = perf->perf_counters[i];
			v = (v >> 16) & 0xFFFF;
			seq_printf(sf, "0x%04X, 0x%04X\n", v,
				   perf->perf_counters[i] & 0xFFFF);
			break;
		}
	}

	return 0;
}

static int d77_perf_counters_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, d77_perf_counters_show, inode->i_private);
}

static const struct file_operations perf_counters_fops = {
	.owner = THIS_MODULE,
	.open = d77_perf_counters_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#if defined(CONFIG_DEBUG_FS)
int d77_setup_perf_counters(struct d71_pipeline *pipe)
{
	struct dentry *d, *root;
	char str[32];
	int perf_num;

	if (!pipe->base.mdev->debugfs_root)
		return -1;

	snprintf(str, 32, "performance@%d", pipe->base.id);
	root = debugfs_create_dir(str,  pipe->base.mdev->debugfs_root);
	if (!root) {
		return -ENOMEM;
	}

	d = debugfs_create_x64("mask", S_IRUSR | S_IWUSR,
				root, &pipe->perf->perf_mask);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("perf_counters", S_IROTH | S_IRUSR | S_IRGRP,
				root, pipe->perf, &perf_counters_fops);
	if (!d)
		return -ENOMEM;

	perf_num = malidp_read32(pipe->lpu_perf, PERF_INFO);
	pipe->perf->perf_valid_mask = (1 << perf_num) - 1;

	return 0;
}
#endif
