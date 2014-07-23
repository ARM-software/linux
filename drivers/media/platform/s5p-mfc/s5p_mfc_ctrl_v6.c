
/*
 * linux/drivers/media/platform/s5p-mfc/s5p_mfc_ctrl_v6.c
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


static inline void s5p_mfc_init_memctrl_v6(struct s5p_mfc_dev *dev)
{
	mfc_write(dev, dev->bank1, S5P_FIMV_RISC_BASE_ADDRESS_V6);
	mfc_debug(2, "Base Address : %08x\n", dev->bank1);
}

/* Reset the device */
static int s5p_mfc_reset_v6(struct s5p_mfc_dev *dev)
{
	int i;
	mfc_debug_enter();

	/* Zero Initialization of MFC registers */
	mfc_write(dev, 0, S5P_FIMV_RISC2HOST_CMD_V6);
	mfc_write(dev, 0, S5P_FIMV_HOST2RISC_CMD_V6);
	mfc_write(dev, 0, S5P_FIMV_FW_VERSION_V6);

	for (i = 0; i < S5P_FIMV_REG_CLEAR_COUNT_V6; i++)
		mfc_write(dev, 0, S5P_FIMV_REG_CLEAR_BEGIN_V6 + (i*4));

	/* check bus reset control before reset */
	if (dev->risc_on)
		if (s5p_mfc_bus_reset(dev))
			return -EIO;
	/* Reset
	 * set RISC_ON to 0 during power_on & wake_up.
	 * V6 needs RISC_ON set to 0 during reset also.
	 */
	if ((!dev->risc_on) || (!IS_MFCV7(dev)))
		mfc_write(dev, 0, S5P_FIMV_RISC_ON_V6);

	mfc_write(dev, 0x1FFF, S5P_FIMV_MFC_RESET_V6);
	mfc_write(dev, 0, S5P_FIMV_MFC_RESET_V6);

	mfc_debug_leave();
	return 0;
}

/* Initialize MFC V6 hardware */
static int s5p_mfc_init_hw_v6(struct s5p_mfc_dev *dev)
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
	WARN_ON(dev->risc_on);
	s5p_mfc_clock_on(dev);
	ret = s5p_mfc_ctrl_ops_call(dev, reset, dev);
	if (ret) {
		mfc_err("Failed to reset MFC - timeout\n");
		s5p_mfc_clock_off(dev);
		return ret;
	}
	mfc_debug(2, "Done MFC reset..\n");
	/* 1. Set DRAM base Addr */
	s5p_mfc_init_memctrl_v6(dev);
	/* 2. Release reset signal to the RISC */
	s5p_mfc_clean_dev_int_flags(dev);
	mfc_write(dev, 0x1, S5P_FIMV_RISC_ON_V6);

	ret = s5p_mfc_init_fw(dev);
	if (ret) {
		s5p_mfc_clock_off(dev);
		return ret;
	}

	ver = mfc_read(dev, S5P_FIMV_FW_VERSION_V6);
	mfc_debug(2, "MFC F/W version : %02xyy, %02xmm, %02xdd\n",
		(ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF);
	s5p_mfc_clock_off(dev);
	dev->risc_on = 1;
	mfc_debug_leave();
	return ret;
}

static int s5p_mfc_wait_wakeup_v8(struct s5p_mfc_dev *dev)
{
	int ret;

	/* Release reset signal to the RISC */
	mfc_write(dev, 0x1, S5P_FIMV_RISC_ON_V6);

	if (s5p_mfc_wait_for_done_dev(dev, S5P_MFC_R2H_CMD_FW_STATUS_RET)) {
		mfc_err("Failed to reset MFCV8\n");
		return -EIO;
	}
	mfc_debug(2, "Write command to wakeup MFCV8\n");
	ret = s5p_mfc_hw_call(dev->mfc_cmds, wakeup_cmd, dev);
	if (ret) {
		mfc_err("Failed to send command to MFCV8 - timeout\n");
		return ret;
	}

	if (s5p_mfc_wait_for_done_dev(dev, S5P_MFC_R2H_CMD_WAKEUP_RET)) {
		mfc_err("Failed to wakeup MFC\n");
		return -EIO;
	}
	return ret;
}

static int s5p_mfc_wait_wakeup_v6(struct s5p_mfc_dev *dev)
{
	int ret;

	/* Send MFC wakeup command */
	ret = s5p_mfc_hw_call(dev->mfc_cmds, wakeup_cmd, dev);
	if (ret) {
		mfc_err("Failed to send command to MFC - timeout\n");
		return ret;
	}

	/* Release reset signal to the RISC */
	mfc_write(dev, 0x1, S5P_FIMV_RISC_ON_V6);

	if (s5p_mfc_wait_for_done_dev(dev, S5P_MFC_R2H_CMD_WAKEUP_RET)) {
		mfc_err("Failed to wakeup MFC\n");
		return -EIO;
	}
	return ret;
}

static int s5p_mfc_wakeup_v6(struct s5p_mfc_dev *dev)
{
	int ret;

	mfc_debug_enter();
	/* 0. MFC reset */
	mfc_debug(2, "MFC reset..\n");
	WARN_ON(dev->risc_on);
	s5p_mfc_clock_on(dev);
	ret = s5p_mfc_ctrl_ops_call(dev, reset, dev);
	if (ret) {
		mfc_err("Failed to reset MFC - timeout\n");
		s5p_mfc_clock_off(dev);
		return ret;
	}
	mfc_debug(2, "Done MFC reset..\n");
	/* 1. Set DRAM base Addr */
	s5p_mfc_init_memctrl_v6(dev);
	/* 2. Initialize registers of channel I/F */
	s5p_mfc_clean_dev_int_flags(dev);
	/* 3. Send MFC wakeup command and wait for completion*/
	if (IS_MFCV8(dev))
		ret = s5p_mfc_wait_wakeup_v8(dev);
	else
		ret = s5p_mfc_wait_wakeup_v6(dev);

	s5p_mfc_clock_off(dev);
	if (ret)
		return ret;

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						S5P_MFC_R2H_CMD_WAKEUP_RET) {
		/* Failure. */
		mfc_err("Failed to wakeup - error: %d int: %d\n", dev->int_err,
								dev->int_type);
		return -EIO;
	}
	dev->risc_on = 1;
	mfc_debug_leave();
	return 0;
}

static void s5p_mfc_mem_req_disable_v6(struct s5p_mfc_dev *dev)
{
	if (dev->risc_on)
		s5p_mfc_bus_reset(dev);
}

static void s5p_mfc_mem_req_enable_v6(struct s5p_mfc_dev *dev)
{
	unsigned int bus_reset_ctrl;
	if (dev->risc_on) {
		bus_reset_ctrl = mfc_read(dev, S5P_FIMV_MFC_BUS_RESET_CTRL);
		bus_reset_ctrl &= S5P_FIMV_MFC_BUS_RESET_CTRL_MASK;
		mfc_write(dev, bus_reset_ctrl, S5P_FIMV_MFC_BUS_RESET_CTRL);
	}
}

/* Initialize hw ctrls function pointers for MFC v6 */
static struct s5p_mfc_hw_ctrl_ops s5p_mfc_hw_ctrl_ops_v6_plus = {
	.init_hw = s5p_mfc_init_hw_v6,
	.deinit_hw = s5p_mfc_deinit_hw,
	.reset = s5p_mfc_reset_v6,
	.wakeup = s5p_mfc_wakeup_v6,
	.sleep = s5p_mfc_sleep,
	.mem_req_disable = s5p_mfc_mem_req_disable_v6,
	.mem_req_enable = s5p_mfc_mem_req_enable_v6,
};

struct s5p_mfc_hw_ctrl_ops *s5p_mfc_init_hw_ctrl_ops_v6_plus(void)
{
	return &s5p_mfc_hw_ctrl_ops_v6_plus;
}
