/*
 * linux/drivers/media/video/exynos/hevc/hevc_ctrl.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/jiffies.h>

#include <linux/firmware.h>
#include <linux/err.h>
#include <linux/sched.h>

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <config/exynos/iovmm.h>
#endif

#include "hevc_common.h"

#include "hevc_mem.h"
#include "hevc_intr.h"
#include "hevc_debug.h"
#include "hevc_reg.h"
#include "hevc_cmd.h"
#include "hevc_pm.h"

static void *hevc_bitproc_buf;
static dma_addr_t hevc_bitproc_phys;
static unsigned char *hevc_bitproc_virt;

/* Allocate firmware */
int hevc_alloc_firmware(struct hevc_dev *dev)
{
	unsigned int firmware_size;
	void *alloc_ctx;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	hevc_debug(3, "fw size %d\n", dev->variant->buf_size->firmware_code);

	firmware_size = dev->variant->buf_size->firmware_code;
	alloc_ctx = dev->alloc_ctx[HEVC_FW_ALLOC_CTX];

	if (hevc_bitproc_buf)
		return 0;

	hevc_info("Allocating memory for firmware.\n");

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->num_drm_inst)
		alloc_ctx = dev->alloc_ctx_fw;
#endif

	hevc_bitproc_buf = hevc_mem_alloc_priv(alloc_ctx, firmware_size);
	if (IS_ERR(hevc_bitproc_buf)) {
		hevc_bitproc_buf = 0;
		printk(KERN_ERR "Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	hevc_bitproc_phys = hevc_mem_daddr_priv(hevc_bitproc_buf);

	if (!dev->num_drm_inst) {
		hevc_bitproc_virt =
				hevc_mem_vaddr_priv(hevc_bitproc_buf);
		hevc_debug(2, "Virtual address for FW: %08lx\n",
				(long unsigned int)hevc_bitproc_virt);
		if (!hevc_bitproc_virt) {
			hevc_err("Bitprocessor memory remap failed\n");
			hevc_mem_free_priv(hevc_bitproc_buf);
			hevc_bitproc_phys = 0;
			hevc_bitproc_buf = 0;
			return -EIO;
		}
	}

	dev->port_a = hevc_bitproc_phys;

	dev->port_b = hevc_bitproc_phys;

	hevc_info("Port A: %08x Port B: %08x (FW: %08x size: %08x)\n",
			dev->port_a, dev->port_b,
			hevc_bitproc_phys,
			firmware_size);

	hevc_debug_leave();

	return 0;
}

/* Load firmware to HEVC */
int hevc_load_firmware(struct hevc_dev *dev)
{
	struct firmware *fw_blob;
	unsigned int firmware_size;
	int err;

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	firmware_size = dev->variant->buf_size->firmware_code;

	/* Firmare has to be present as a separate file or compiled
	 * into kernel. */
	hevc_debug_enter();
	hevc_info("Requesting fw\n");
	err = request_firmware((const struct firmware **)&fw_blob,
					HEVC_FW_NAME, dev->v4l2_dev.dev);

	if (err != 0) {
		hevc_err("Firmware is not present in the /lib/firmware directory nor compiled in kernel.\n");
		return -EINVAL;
	}

	hevc_debug(2, "Ret of request_firmware: %d Size: %d\n", err, fw_blob->size);

	if (fw_blob->size > firmware_size) {
		hevc_err("HEVC firmware is too big to be loaded.\n");
		release_firmware(fw_blob);
		return -ENOMEM;
	}

	if (hevc_bitproc_buf == 0 || hevc_bitproc_phys == 0) {
		hevc_err("HEVC firmware is not allocated or was not mapped correctly.\n");
		release_firmware(fw_blob);
		return -EINVAL;
	}
	memcpy(hevc_bitproc_virt, fw_blob->data, fw_blob->size);
	hevc_mem_clean_priv(hevc_bitproc_buf, hevc_bitproc_virt, 0,
			fw_blob->size);

	release_firmware(fw_blob);
	hevc_debug_leave();
	return 0;
}

/* Release firmware memory */
int hevc_release_firmware(struct hevc_dev *dev)
{
	/* Before calling this function one has to make sure
	 * that HEVC is no longer processing */
	if (!hevc_bitproc_buf)
		return -EINVAL;
	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	hevc_mem_free_priv(hevc_bitproc_buf);

	hevc_bitproc_virt =  0;
	hevc_bitproc_phys = 0;
	hevc_bitproc_buf = 0;

	return 0;
}

static inline int hevc_bus_reset(struct hevc_dev *dev)
{
	unsigned int status;
	unsigned long timeout;

	hevc_debug_enter();

	/* Reset */
	hevc_write_reg(0x0, HEVC_RISC_ON);
	hevc_write_reg(0x1FFF, HEVC_DEC_RESET);

	hevc_write_reg(0x0, HEVC_DEC_RESET);

	timeout = jiffies + msecs_to_jiffies(HEVC_BW_TIMEOUT);
	/* Check bus status */
	do {
		if (time_after(jiffies, timeout)) {
			hevc_err("Timeout while resetting HEVC.\n");
			return -EIO;
		}
		status = hevc_read_reg(HEVC_BUS_RESET_CTRL);
	} while ((status & 0x2) == 0);

	hevc_debug_leave();

	return 0;
}

/* Reset the device */
static int hevc_reset(struct hevc_dev *dev)
{
	int i;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	/* Stop procedure */
	/* Reset VI */
	/*
	hevc_write_reg(0x3f7, HEVC_DEC_RESET);
	*/

	/* Zero Initialization of HEVC registers */
	hevc_write_reg(0, HEVC_RISC2HOST_CMD);
	hevc_write_reg(0, HEVC_HOST2RISC_CMD);
	hevc_write_reg(0, HEVC_FW_VERSION);

	for (i = 0; i < HEVC_REG_CLEAR_COUNT; i++)
		hevc_write_reg(0, HEVC_REG_CLEAR_BEGIN + (i*4));

#if 0
	if (hevc_bus_reset(dev)) {
		hevc_err("hevc bus reset timeout\n");
		return -EIO;
	}
#endif

	hevc_write_reg(0, HEVC_RISC_ON);
	hevc_write_reg(0x1FFF, HEVC_DEC_RESET);
	hevc_write_reg(0, HEVC_DEC_RESET);

	hevc_debug_leave();

	return 0;
}

static inline void hevc_init_memctrl(struct hevc_dev *dev)
{
	hevc_write_reg(dev->port_a, HEVC_RISC_BASE_ADDRESS);
	hevc_debug(2, "Base Address : %08x\n", dev->port_a);
}

static inline void hevc_clear_cmds(struct hevc_dev *dev)
{
	hevc_write_reg(0, HEVC_RISC2HOST_CMD);
	hevc_write_reg(0, HEVC_HOST2RISC_CMD);
}

/* Initialize hardware */
int hevc_init_hw(struct hevc_dev *dev)
{
	char fimv_info;
	int ret = 0;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	/* RMVME: */
	if (!hevc_bitproc_buf) {
		hevc_err("no hevc device to run@@@@\n");
		return -EINVAL;
	}


	/* 0. HEVC reset */
	hevc_info("HEVC reset...\n");

	hevc_clock_on();
	ret = hevc_reset(dev);
	if (ret) {
		hevc_err("Failed to reset HEVC - timeout.\n");
		goto err_init_hw;
	}
	hevc_info("Done HEVC reset...\n");

	/* 1. Set DRAM base Addr */
	hevc_init_memctrl(dev);

	/* 2. Initialize registers of channel I/F */
	hevc_clear_cmds(dev);
	hevc_clean_dev_int_flags(dev);

	/* 3. Release reset signal to the RISC */
	hevc_write_reg(0x1, HEVC_RISC_ON);

	hevc_debug(2, "@^@^@ Will now wait for completion of firmware transfer.\n");


	if (hevc_wait_for_done_dev(dev, HEVC_R2H_CMD_FW_STATUS_RET)) {
		hevc_err("Failed to load firmware.\n");
		hevc_clean_dev_int_flags(dev);
		ret = -EIO;
		goto err_init_hw;
	}

	hevc_clean_dev_int_flags(dev);

	/* 4. Initialize firmware */
	ret = hevc_sys_init_cmd(dev);
	if (ret) {
		hevc_err("Failed to send command to HEVC - timeout.\n");
		goto err_init_hw;
	}
	hevc_debug(2, "Ok, now will write a command to init the system\n");
	if (hevc_wait_for_done_dev(dev, HEVC_R2H_CMD_SYS_INIT_RET)) {
		hevc_err("Failed to load firmware\n");
		ret = -EIO;
		goto err_init_hw;
	}

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						HEVC_R2H_CMD_SYS_INIT_RET) {
		/* Failure. */
		hevc_err("Failed to init firmware - error: %d"
				" int: %d.\n", dev->int_err, dev->int_type);
		ret = -EIO;
		goto err_init_hw;
	}

	fimv_info = HEVC_GET_REG(SYS_FW_FIMV_INFO);
	if (fimv_info != 'D' && fimv_info != 'E')
		fimv_info = 'N';

	hevc_info("HEVC v%x.%x, F/W: %02xyy, %02xmm, %02xdd (%c)\n",
		 HEVC_VER_MAJOR(dev),
		 HEVC_VER_MINOR(dev),
		 HEVC_GET_REG(SYS_FW_VER_YEAR),
		 HEVC_GET_REG(SYS_FW_VER_MONTH),
		 HEVC_GET_REG(SYS_FW_VER_DATE),
		 fimv_info);

	dev->fw.date = HEVC_GET_REG(SYS_FW_VER_ALL);
	/* Check HEVC version and F/W version */

err_init_hw:
	hevc_clock_off();
	hevc_debug_leave();

	return ret;
}


/* Deinitialize hardware */
void hevc_deinit_hw(struct hevc_dev *dev)
{
	hevc_info("hevc deinit start\n");

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return;
	}
/*
	hevc_clock_on();
	hevc_reset(dev);
	hevc_clock_off();
*/
	hevc_info("hevc deinit completed\n");
}

int hevc_sleep(struct hevc_dev *dev)
{
	struct hevc_ctx *ctx;
	int ret;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		hevc_err("no hevc context to run\n");
		return -EINVAL;
	}

	ret = wait_event_interruptible_timeout(ctx->queue,
			(test_bit(ctx->num, &dev->hw_lock) == 0),
			msecs_to_jiffies(HEVC_INT_TIMEOUT));
	if (ret == 0) {
		hevc_err("Waiting for hardware to finish timed out\n");
		ret = -EIO;
		return ret;
	}

	spin_lock(&dev->condlock);
	set_bit(ctx->num, &dev->hw_lock);
	spin_unlock(&dev->condlock);

	hevc_clock_on();
	hevc_clean_dev_int_flags(dev);
	ret = hevc_sleep_cmd(dev);
	if (ret) {
		hevc_err("Failed to send command to HEVC - timeout.\n");
		goto err_hevc_sleep;
	}
	if (hevc_wait_for_done_dev(dev, HEVC_R2H_CMD_SLEEP_RET)) {
		hevc_err("Failed to sleep\n");
		ret = -EIO;
		goto err_hevc_sleep;
	}

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						HEVC_R2H_CMD_SLEEP_RET) {
		/* Failure. */
		hevc_err("Failed to sleep - error: %d"
				" int: %d.\n", dev->int_err, dev->int_type);
		ret = -EIO;
		goto err_hevc_sleep;
	}

err_hevc_sleep:
	hevc_clock_off();
	hevc_debug_leave();

	return ret;
}

int hevc_wakeup(struct hevc_dev *dev)
{
	int ret;

	hevc_debug_enter();

	if (!dev) {
		hevc_err("no hevc device to run\n");
		return -EINVAL;
	}

	/* 0. HEVC reset */
	hevc_debug(2, "HEVC reset...\n");

	hevc_clock_on();

	ret = hevc_reset(dev);
	if (ret) {
		hevc_err("Failed to reset HEVC - timeout.\n");
		goto err_hevc_wakeup;
	}
	hevc_debug(2, "Done HEVC reset...\n");

	/* 1. Set DRAM base Addr */
	hevc_init_memctrl(dev);

	/* 2. Initialize registers of channel I/F */
	hevc_clear_cmds(dev);

	hevc_clean_dev_int_flags(dev);
	/* 3. Initialize firmware */
	ret = hevc_wakeup_cmd(dev);
	if (ret) {
		hevc_err("Failed to send command to HEVC - timeout.\n");
		goto err_hevc_wakeup;
	}

	/* 4. Release reset signal to the RISC */
	hevc_write_reg(0x1, HEVC_RISC_ON);

	hevc_debug(2, "Ok, now will write a command to wakeup the system\n");
	if (hevc_wait_for_done_dev(dev, HEVC_R2H_CMD_WAKEUP_RET)) {
		hevc_err("Failed to load firmware\n");
		ret = -EIO;
		goto err_hevc_wakeup;
	}

	dev->int_cond = 0;
	if (dev->int_err != 0 || dev->int_type !=
						HEVC_R2H_CMD_WAKEUP_RET) {
		/* Failure. */
		hevc_err("Failed to wakeup - error: %d"
				" int: %d.\n", dev->int_err, dev->int_type);
		ret = -EIO;
		goto err_hevc_wakeup;
	}

err_hevc_wakeup:
	hevc_clock_off();
	hevc_debug_leave();

	return 0;
}

