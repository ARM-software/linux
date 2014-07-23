/*
 * linux/drivers/media/platform/s5p-mfc/s5p_mfc_ctrl.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef S5P_MFC_CTRL_H
#define S5P_MFC_CTRL_H

#include "s5p_mfc_common.h"

/**
 * struct s5p_mfc_hw_ctrl_ops - ctrl ops function pointers
 * @init_hw:		function to init mfc hw
 * @deinit_hw:		function to de-init mfc hw
 * @reset:		function to reset mfc
 * @wakeup:		function to wakeup mfc
 * @sleep:		function to suspend mfc
 * @mem_req_disable	function to stop memory request from mfc
 * @mem_req_enable	function to start mfc memory request
 */
struct s5p_mfc_hw_ctrl_ops {
	int (*init_hw)(struct s5p_mfc_dev *dev);
	void (*deinit_hw)(struct s5p_mfc_dev *dev);
	int (*reset)(struct s5p_mfc_dev *dev);
	int (*wakeup)(struct s5p_mfc_dev *dev);
	int (*sleep)(struct s5p_mfc_dev *dev);
	void (*mem_req_disable)(struct s5p_mfc_dev *dev);
	void (*mem_req_enable)(struct s5p_mfc_dev *dev);
};

int s5p_mfc_release_firmware(struct s5p_mfc_dev *dev);
int s5p_mfc_alloc_firmware(struct s5p_mfc_dev *dev);
int s5p_mfc_load_firmware(struct s5p_mfc_dev *dev);
void s5p_mfc_deinit_hw(struct s5p_mfc_dev *dev);
int s5p_mfc_sleep(struct s5p_mfc_dev *dev);
int s5p_mfc_bus_reset(struct s5p_mfc_dev *dev);
int s5p_mfc_init_fw(struct s5p_mfc_dev *dev);

struct s5p_mfc_hw_ctrl_ops *s5p_mfc_init_hw_ctrl_ops_v5(void);
struct s5p_mfc_hw_ctrl_ops *s5p_mfc_init_hw_ctrl_ops_v6_plus(void);
int s5p_mfc_open_mfc_inst(struct s5p_mfc_dev *dev, struct s5p_mfc_ctx *ctx);
void s5p_mfc_close_mfc_inst(struct s5p_mfc_dev *dev, struct s5p_mfc_ctx *ctx);

#endif /* S5P_MFC_CTRL_H */
