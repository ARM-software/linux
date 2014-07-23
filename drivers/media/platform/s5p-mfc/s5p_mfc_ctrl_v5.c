
/*
 * linux/drivers/media/platform/s5p-mfc/s5p_mfc_ctrl_v5.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include "s5p_mfc_cmd.h"
#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_intr.h"
#include "s5p_mfc_opr.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_ctrl.h"

static int s5p_mfc_reset_v5(struct s5p_mfc_dev *dev)
{
	unsigned int mc_status;
	unsigned long timeout;

	mfc_debug_enter();

	/* Stop procedure */
	/*  reset RISC */
	mfc_write(dev, 0x3f6, S5P_FIMV_SW_RESET);
	/*  All reset except for MC */
	mfc_write(dev, 0x3e2, S5P_FIMV_SW_RESET);
	mdelay(10);

	timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);
	/* Check MC status */
	do {
		if (time_after(jiffies, timeout)) {
			mfc_err("Timeout while resetting MFC\n");
			return -EIO;
		}
		mc_status = mfc_read(dev, S5P_FIMV_MC_STATUS);
	} while (mc_status & 0x3);

	mfc_write(dev, 0x0, S5P_FIMV_SW_RESET);
	mfc_write(dev, 0x3fe, S5P_FIMV_SW_RESET);

	mfc_debug_leave();
	return 0;
}

static inline void s5p_mfc_clear_cmds(struct s5p_mfc_dev *dev)
{
	mfc_write(dev, 0xffffffff, S5P_FIMV_SI_CH0_INST_ID);
	mfc_write(dev, 0xffffffff, S5P_FIMV_SI_CH1_INST_ID);
	mfc_write(dev, 0, S5P_FIMV_RISC2HOST_CMD);
	mfc_write(dev, 0, S5P_FIMV_HOST2RISC_CMD);
}

static inline void s5p_mfc_init_memctrl_v5(struct s5p_mfc_dev *dev)
{
	mfc_write(dev, dev->bank1, S5P_FIMV_MC_DRAMBASE_ADR_A);
	mfc_write(dev, dev->bank2, S5P_FIMV_MC_DRAMBASE_ADR_B);
	mfc_debug(2, "Bank1: %08x, Bank2: %08x\n",
			dev->bank1, dev->bank2);
}

/* Initialize MFC V5 hardware */
static int s5p_mfc_init_hw_v5(struct s5p_mfc_dev *dev)
{
	unsigned int ver;
	int ret;

	mfc_debug_enter();
	ret = s5p_mfc_load_firmware(dev);
	if (ret) {
		mfc_err("Failed to reload FW\n");
		return ret;
	}

	/* 0. MFC reset */
	mfc_debug(2, "MFC reset..\n");
	s5p_mfc_clock_on(dev);
	ret = s5p_mfc_ctrl_ops_call(dev, reset, dev);
	if (ret) {
		mfc_err("Failed to reset MFC - timeout\n");
		s5p_mfc_clock_off(dev);
		return ret;
	}
	mfc_debug(2, "Done MFC reset..\n");
	/* 1. Set DRAM base Addr */
	s5p_mfc_init_memctrl_v5(dev);
	/* 2. Initialize registers of channel I/F */
	s5p_mfc_clear_cmds(dev);
	/* 3. Release reset signal to the RISC */
	s5p_mfc_clean_dev_int_flags(dev);
	mfc_write(dev, 0x3ff, S5P_FIMV_SW_RESET);

	ret = s5p_mfc_init_fw(dev);
	if (ret) {
		s5p_mfc_clock_off(dev);
		return ret;
	}

	ver = mfc_read(dev, S5P_FIMV_FW_VERSION);
	mfc_debug(2, "MFC F/W version : %02xyy, %02xmm, %02xdd\n",
		(ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF);
	s5p_mfc_clock_off(dev);
	mfc_debug_leave();
	return 0;
}


static int s5p_mfc_wakeup_v5(struct s5p_mfc_dev *dev)
{
	int ret;

	mfc_debug_enter();
	/* 0. MFC reset */
	mfc_debug(2, "MFC reset..\n");
	s5p_mfc_clock_on(dev);
	ret = s5p_mfc_ctrl_ops_call(dev, reset, dev);
	if (ret) {
		mfc_err("Failed to reset MFC - timeout\n");
		s5p_mfc_clock_off(dev);
		return ret;
	}
	mfc_debug(2, "Done MFC reset..\n");
	/* 1. Set DRAM base Addr */
	s5p_mfc_init_memctrl_v5(dev);
	/* 2. Initialize registers of channel I/F */
	s5p_mfc_clear_cmds(dev);
	s5p_mfc_clean_dev_int_flags(dev);
	/* 3. Send MFC wakeup command and wait for completion*/
	ret = s5p_mfc_hw_call(dev->mfc_cmds, wakeup_cmd, dev);
	if (ret) {
		mfc_err("Failed to send command to MFC - timeout\n");
		s5p_mfc_clock_off(dev);
		return ret;
	}

	/* Release reset signal to the RISC */
	mfc_write(dev, 0x3ff, S5P_FIMV_SW_RESET);

	if (s5p_mfc_wait_for_done_dev(dev, S5P_MFC_R2H_CMD_WAKEUP_RET)) {
		mfc_err("Failed to wakeup MFC\n");
		s5p_mfc_clock_off(dev);
		return -EIO;
	}
	s5p_mfc_clock_off(dev);

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						S5P_MFC_R2H_CMD_WAKEUP_RET) {
		/* Failure. */
		mfc_err("Failed to wakeup - error: %d int: %d\n", dev->int_err,
								dev->int_type);
		return -EIO;
	}
	mfc_debug_leave();
	return 0;
}

/* Initialize hw ctrls function pointers for MFC v5 */
static struct s5p_mfc_hw_ctrl_ops s5p_mfc_hw_ctrl_ops_v5 = {
	.init_hw = s5p_mfc_init_hw_v5,
	.deinit_hw = s5p_mfc_deinit_hw,
	.reset = s5p_mfc_reset_v5,
	.wakeup = s5p_mfc_wakeup_v5,
	.sleep = s5p_mfc_sleep,
};

struct s5p_mfc_hw_ctrl_ops *s5p_mfc_init_hw_ctrl_ops_v5(void)
{
	return &s5p_mfc_hw_ctrl_ops_v5;
}
