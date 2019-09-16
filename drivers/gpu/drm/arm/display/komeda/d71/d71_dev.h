/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2018 ARM Limited. All rights reserved.
 * Author: James.Qian.Wang <james.qian.wang@arm.com>
 *
 */
#ifndef _D71_DEV_H_
#define _D71_DEV_H_

#include "komeda_dev.h"
#include "komeda_pipeline.h"
#include "d71_regs.h"

#define MAX_PERF_COUNTERS	64

struct d77_perf {
	u64	perf_mask;
	u64	perf_valid_mask;
	union {
		u32 perf_counters[MAX_PERF_COUNTERS];
		struct {
			u32 perf_c0_l0p01_axi_min;
			u32 perf_c1_l0p2_axi_min;
			u32 perf_c2_l0p01_axi_max;
			u32 perf_c3_l0p2_axi_max;

			u32 perf_c4_l1p01_axi_min;
			u32 perf_c5_l1p01_axi_max;

			u32 perf_c6_l2p01_axi_min;
			u32 perf_c7_l2p2_axi_min;
			u32 perf_c8_l2p01_axi_max;
			u32 perf_c9_l2p2_axi_max;

			u32 perf_c10_l3p01_axi_min;
			u32 perf_c11_l3p01_axi_max;

			u32 perf_c12_lwp01_axi_min;
			u32 perf_c13_lwp01_axi_max;

			u32 perf_c14_l0p01_dti_min;
			u32 perf_c15_l0p2_dti_min;
			u32 perf_c16_l0p01_dti_max;
			u32 perf_c17_l0p2_dti_max;

			u32 perf_c18_l1p01_dti_min;
			u32 perf_c19_l1p01_dti_max;

			u32 perf_c20_l2p01_dti_min;
			u32 perf_c21_l2p2_dti_min;
			u32 perf_c22_l2p01_dti_max;
			u32 perf_c23_l2p2_dti_max;

			u32 perf_c24_l3p01_dti_min;
			u32 perf_c25_l3p01_dti_max;

			u32 perf_c26_lwp01_dti_min;
			u32 perf_c27_lwp01_dti_max;

			u32 perf_c28_l0p0_axi_transf;
			u32 perf_c29_l0p1_axi_transf;
			u32 perf_c30_l0p2_axi_transf;

			u32 perf_c31_l1p0_axi_transf;
			u32 perf_c32_l1p1_axi_transf;

			u32 perf_c33_l2p0_axi_transf;
			u32 perf_c34_l2p1_axi_transf;
			u32 perf_c35_l2p2_axi_transf;

			u32 perf_c36_l3p0_axi_transf;
			u32 perf_c37_l3p1_axi_transf;

			u32 perf_c38_lwp0_axi_transf;
			u32 perf_c39_lwp1_axi_transf;

			u32 perf_c40_l0_fifo_lvl_min;
			u32 perf_c41_l1_fifo_lvl_min;
			u32 perf_c42_l2_fifo_lvl_min;
			u32 perf_c43_l3_fifo_lvl_min;
			u32 perf_c44_lw_fifo_lvl_man;

			u32 perf_c45_atua_vp0_num_ext_req;
			u32 perf_c46_atua_vp1_num_ext_req;
			u32 perf_c47_atub_vp0_num_ext_req;
			u32 perf_c48_atub_vp1_num_ext_req;

			u32 perf_c49_atua_vp0_num_int_req;
			u32 perf_c50_atua_vp1_num_int_req;
			u32 perf_c51_atub_vp0_num_int_req;
			u32 perf_c52_atub_vp1_num_int_req;

			u32 perf_c53_atua_num_evict;
			u32 perf_c54_atub_num_evict;
			u32 perf_c55_atua_tc_lvl_max;
			u32 perf_c56_atub_tc_lvl_max;
		};
	};
};

struct d71_pipeline {
	struct komeda_pipeline base;
	struct d77_perf	*perf;

	/* d71 private pipeline blocks */
	u32 __iomem	*lpu_addr;
	u32 __iomem	*atu_addr[D77_PIPELINE_MAX_ATU];
	u32 __iomem	*cu_addr;
	u32 __iomem	*dou_addr;
	u32 __iomem	*dou_ft_coeff_addr; /* forward transform coeffs table */
	u32 __iomem	*lpu_perf;
};

struct d71_dev {
	struct komeda_dev *mdev;

	int	num_blocks;
	int	num_pipelines;
	int	num_rich_layers;
	u32	max_line_size;
	u32	max_vsize;
	u32	supports_dual_link : 1;
	u32	integrates_tbu : 1;

	/* global register blocks */
	u32 __iomem	*gcu_addr;
	/* scaling coeffs table */
	u32 __iomem	*glb_scl_coeff_addr[D71_MAX_GLB_SCL_COEFF];
	u32 __iomem	*glb_sc_coeff_addr[D77_MAX_GLB_SC_COEFF];
	u32 __iomem	*periph_addr;

	struct d71_pipeline *pipes[D71_MAX_PIPELINE];

	struct komeda_coeffs_manager *it_mgr;
	struct komeda_coeffs_manager *ft_mgr;
	struct komeda_coeffs_manager *it_s_mgr;
};

#define to_d71_pipeline(x)	container_of(x, struct d71_pipeline, base)

extern const struct komeda_pipeline_funcs d71_pipeline_funcs;

int d71_probe_block(struct d71_dev *d71,
		    struct block_header *blk, u32 __iomem *reg);
void d71_read_block_header(u32 __iomem *reg, struct block_header *blk);

void d71_dump(struct komeda_dev *mdev, struct seq_file *sf);

int d71_pipeline_config_axi(struct d71_pipeline *d71_pipe);

#endif /* !_D71_DEV_H_ */
