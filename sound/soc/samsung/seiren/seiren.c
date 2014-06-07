/* sound/soc/samsung/seiren/seiren.c
 *
 * Exynos Seiren Audio driver for Exynos5430
 *
 * Copyright (c) 2013 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/serio.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/firmware.h>

#include <sound/exynos.h>

#include "../lpass.h"
#include "seiren.h"
#include "seiren_ioctl.h"
#include "seiren_error.h"


#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

static DEFINE_MUTEX(esa_mutex);
static DECLARE_WAIT_QUEUE_HEAD(esa_wq);

static struct seiren_info si;

static int esa_send_cmd_(u32 cmd_code, bool sram_only);
static irqreturn_t esa_isr(int irqno, void *id);

#define esa_send_cmd_sram(cmd)	esa_send_cmd_(cmd, true)
#define esa_send_cmd(cmd)	esa_send_cmd_(cmd, false)

int check_esa_status(void)
{
	return si.fw_use_dram ? 1 : 0;
}

static void esa_dump_fw_log(void)
{
	char log[256];
	char *addr = si.fw_log_buf;
	int n;

	esa_info("fw log:\n");
	for (n = 0; n < FW_LOG_LINE; n++, addr += 128) {
		memcpy(log, addr, 128);
		esa_info("%s", log);
	}
}

static struct esa_rtd *esa_alloc_rtd(void)
{
	struct esa_rtd *rtd = NULL;
	int idx;

	rtd = kzalloc(sizeof(struct esa_rtd), GFP_KERNEL);

	if (rtd) {
		spin_lock(&si.lock);

		for (idx = 0; idx < INSTANCE_MAX; idx++) {
			if (!si.rtd_pool[idx]) {
				si.rtd_cnt++;
				si.rtd_pool[idx] = rtd;
				rtd->idx = idx;
				break;
			}
		}

		spin_unlock(&si.lock);

		if (idx == INSTANCE_MAX) {
			kfree(rtd);
			rtd = NULL;
		}
	}

	return rtd;
}

static void esa_free_rtd(struct esa_rtd *rtd)
{
	spin_lock(&si.lock);

	si.rtd_cnt--;
	si.rtd_pool[rtd->idx] = NULL;

	spin_unlock(&si.lock);

	kfree(rtd);
}

static int esa_send_cmd_exe(struct esa_rtd *rtd, unsigned char *ibuf,
				unsigned char *obuf, size_t size)
{
	u32 ibuf_ca5_pa, obuf_ca5_pa;
	u32 ibuf_offset, obuf_offset;
	int out_size;
	int response;

	if (rtd->use_sram) {	/* SRAM buffer */
		ibuf_offset = SRAM_IO_BUF + SRAM_IBUF_OFFSET;
		obuf_offset = SRAM_IO_BUF + SRAM_OBUF_OFFSET;
		ibuf_ca5_pa = ibuf_offset;
		obuf_ca5_pa = obuf_offset;
		memcpy(si.sram + ibuf_offset, ibuf, size);
	} else {		/* DRAM buffer */
		ibuf_offset = ibuf - si.bufmem;
		obuf_offset = obuf - si.bufmem;
		ibuf_ca5_pa = ibuf_offset + BASEMEM_OFFSET;
		obuf_ca5_pa = obuf_offset + BASEMEM_OFFSET;
	}

	writel(rtd->handle_id, si.mailbox + HANDLE_ID);
	writel(size, si.mailbox + SIZE_OF_INDATA);
	writel(ibuf_ca5_pa, si.mailbox + PHY_ADDR_INBUF);
	writel(obuf_ca5_pa, si.mailbox + PHY_ADDR_OUTBUF);

	if (rtd->use_sram)
		esa_send_cmd_sram(CMD_EXE);
	else
		esa_send_cmd(CMD_EXE);

	/* check response of FW */
	response = readl(si.mailbox + RETURN_CMD);

	if (rtd->use_sram) {
		out_size = readl(si.mailbox + SIZE_OUT_DATA);
		if ((response == 0) && (out_size > 0))
			memcpy(obuf, si.sram + obuf_offset, out_size);
	}

	return response;
}

static void esa_fw_download(void)
{
	int n;

	esa_debug("%s: fw size = sram(%d) dram(%d)\n", __func__,
			si.fw_sbin_size, si.fw_dbin_size);

	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);
	udelay(20);

	memset(si.mailbox, 0, 128);

	if (si.fw_suspended) {
		esa_debug("%s: resume\n", __func__);
		/* Restore SRAM */
		memcpy(si.sram, si.fwmem_sram_bak, SRAM_FW_MAX);
		memset(si.sram + FW_ZERO_SET_BASE, 0, FW_ZERO_SET_SIZE);
	} else {
		esa_debug("%s: intialize\n", __func__);
		for (n = 0; n < FWAREA_NUM; n++)
			memset(si.fwarea[n], 0, FWAREA_SIZE);

		memset(si.sram, 0, SRAM_FW_MAX);	/* for ZI area */
		memcpy(si.sram, si.fwmem, si.fw_sbin_size);
		memcpy(si.fwarea[0], si.fwmem + si.fw_sbin_size,
					si.fw_dbin_size);
	}

	lpass_reset(LPASS_IP_CA5, LPASS_OP_NORMAL);

	esa_debug("%s: CA5 startup...\n", __func__);
}

static int esa_fw_startup(void)
{
	unsigned int dec_ver;
	int ret;

	if (si.fw_ready)
		return 0;

	if (!si.fwmem_loaded)
		return -EAGAIN;

	/* power on */
	si.fw_use_dram = true;
	esa_debug("Turn on CA5...\n");
	esa_fw_download();

	/* wait for fw ready */
	ret = wait_event_interruptible_timeout(esa_wq, si.fw_ready, HZ / 2);
	if (!ret) {
		esa_err("%s: fw not ready!!!\n", __func__);
		si.fw_use_dram = false;
		return -EBUSY;
	}

	/* check decoder version */
	esa_send_cmd(SYS_GET_STATUS);
	dec_ver = readl(si.mailbox) & 0xFF00;
	dec_ver = dec_ver >> 8;
	esa_debug("Decoder version : %x\n", dec_ver);

	return 0;
}

static void esa_fw_shutdown(void)
{
	u32 cnt, val;

	if (!si.fw_ready)
		return;

	if (!si.fwmem_loaded)
		return;

	/* SUSPEND & IDLE */
	esa_send_cmd(SYS_SUSPEND);

	si.fw_suspended = false;
	cnt = msecs_to_loops(100);
	while (--cnt) {
		val = readl(si.regs + CA5_STATUS);
		if (val & CA5_STATUS_WFI) {
			si.fw_suspended = true;
			break;
		}
		cpu_relax();
	}
	esa_debug("CA5_STATUS: %X\n", val);

	/* Backup SRAM */
	memcpy(si.fwmem_sram_bak, si.sram, SRAM_FW_MAX);

	/* power off */
	esa_debug("Turn off CA5...\n");
	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);
	si.fw_ready = false;
	si.fw_use_dram = false;
}

static void esa_buffer_init(struct file *file)
{
	struct esa_rtd *rtd = file->private_data;
	unsigned long ibuf_size, obuf_size;
	unsigned int ibuf_count, obuf_count;
	unsigned char *buf;
	bool use_sram = false;

	esa_debug("iptype: %d", rtd->ip_type);

	switch (rtd->ip_type) {
	case ADEC_MP3:
		ibuf_size = DEC_IBUF_SIZE;
		obuf_size = DEC_OBUF_SIZE;
		ibuf_count = DEC_IBUF_NUM;
		obuf_count = DEC_OBUF_NUM;
		use_sram = true;
		break;
	case ADEC_AAC:
		ibuf_size = DEC_AAC_IBUF_SIZE;
		obuf_size = DEC_AAC_OBUF_SIZE;
		ibuf_count = DEC_IBUF_NUM;
		obuf_count = DEC_OBUF_NUM;
		break;
	case ADEC_FLAC:
		ibuf_size = DEC_FLAC_IBUF_SIZE;
		obuf_size = DEC_FLAC_OBUF_SIZE;
		ibuf_count = DEC_IBUF_NUM;
		obuf_count = DEC_OBUF_NUM;
		break;
	case SOUND_EQ:
		ibuf_size = SP_IBUF_SIZE;
		obuf_size = SP_OBUF_SIZE;
		ibuf_count = SP_IBUF_NUM;
		obuf_count = SP_OBUF_NUM;
		break;
	default:
		ibuf_size = 0x10000;
		obuf_size = 0x10000;
		ibuf_count = 1;
		obuf_count = 1;
		break;
	}

	rtd->ibuf_size = ibuf_size;
	rtd->obuf_size = obuf_size;
	rtd->ibuf_count = ibuf_count;
	rtd->obuf_count = obuf_count;
	rtd->use_sram = use_sram;

	buf = si.bufmem + rtd->idx * BUF_SIZE_MAX;
	rtd->ibuf0 = buf;

	buf += ibuf_count == 2 ? ibuf_size : 0;
	rtd->ibuf1 = buf;

	buf += ibuf_size;
	rtd->obuf0 = buf;

	buf += obuf_count == 2 ? obuf_size : 0;
	rtd->obuf1 = buf;
}

static int esa_send_cmd_(u32 cmd_code, bool sram_only)
{
	u32 cnt, val;
	int ret;

	si.isr_done = 0;
	writel(cmd_code, si.mailbox + CMD_CODE);	/* command */
	writel(1, si.regs + SW_INTR_CA5);		/* trigger ca5 */

	si.fw_use_dram = sram_only ? false : true;
	ret = wait_event_interruptible_timeout(esa_wq, si.isr_done, HZ / 2);
	if (!ret) {
		esa_err("%s: CMD(%08X) timed out!!!\n",
					__func__, cmd_code);
		esa_dump_fw_log();
		si.fw_use_dram = true;
		return -EBUSY;
	}

	cnt = msecs_to_loops(10);
	while (--cnt) {
		val = readl(si.regs + CA5_STATUS);
		if (val & CA5_STATUS_WFI)
			break;
		cpu_relax();
	}
	si.fw_use_dram = false;

	return 0;
}

static ssize_t esa_write(struct file *file, const char *buffer,
					size_t size, loff_t *pos)
{
	struct esa_rtd *rtd = file->private_data;
	unsigned char *ibuf;
	unsigned char *obuf;
	unsigned int *obuf_filled_size;
	bool *obuf_filled;
	int response, consumed_size = 0;

	mutex_lock(&esa_mutex);
	pm_runtime_get_sync(&si.pdev->dev);

	if (rtd->obuf0_filled && rtd->obuf1_filled) {
		esa_err("%s: There is no unfilled obuf\n", __func__);
		goto err;
	}

	/* select IBUF0 or IBUF1 */
	if (rtd->select_ibuf == 0) {
		ibuf = rtd->ibuf0;
		obuf = rtd->obuf0;
		obuf_filled = &rtd->obuf0_filled;
		obuf_filled_size = &rtd->obuf0_filled_size;
		esa_debug("%s: use ibuf0\n", __func__);
	} else {
		ibuf = rtd->ibuf1;
		obuf = rtd->obuf1;
		obuf_filled = &rtd->obuf1_filled;
		obuf_filled_size = &rtd->obuf1_filled_size;
		esa_debug("%s: use ibuf1\n", __func__);
	}

	/* receive stream data from user */
	if (copy_from_user(ibuf, buffer, size)) {
		esa_err("%s: failed to copy_from_user\n", __func__);
		goto err;
	}

	/* select IBUF0 or IBUF1 for next writing */
	rtd->select_ibuf = !rtd->select_ibuf;

	/* send execute command to FW for decoding */
	response = esa_send_cmd_exe(rtd, ibuf, obuf, size);

	/* filled size in OBUF */
	*obuf_filled_size = readl(si.mailbox + SIZE_OUT_DATA);

	/* consumed size */
	consumed_size = readl(si.mailbox + CONSUMED_BYTE_IN);

	if (response == 0 && *obuf_filled_size > 0) {
		*obuf_filled = true;
	} else {
		if (consumed_size <= 0)
			consumed_size = response;
		if (rtd->need_config)
			rtd->need_config = false;
		else if (size != 0)
			esa_debug("%s: No output? response:%x\n", __func__, response);
	}

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	esa_debug("%s: handle_id[%x], idx:[%d], consumed:[%d], filled_size:[%d], ibuf:[%d]\n",
			__func__, rtd->handle_id, rtd->idx, consumed_size,
			*obuf_filled_size, !rtd->select_ibuf);

	return consumed_size;
err:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);
	return -EFAULT;
}

static ssize_t esa_read(struct file *file, char *buffer,
				size_t size, loff_t *pos)
{
	struct esa_rtd *rtd = file->private_data;
	unsigned char *obuf;
	unsigned int *obuf_filled_size;
	bool *obuf_filled;

	unsigned char *obuf_;
	unsigned int *obuf_filled_size_;
	bool *obuf_filled_;

	mutex_lock(&esa_mutex);
	pm_runtime_get_sync(&si.pdev->dev);

	/* select OBUF0 or OBUF1 */
	if (rtd->select_obuf == 0) {
		obuf = rtd->obuf0;
		obuf_filled = &rtd->obuf0_filled;
		obuf_filled_size = &rtd->obuf0_filled_size;

		obuf_ = rtd->obuf1;
		obuf_filled_ = &rtd->obuf1_filled;
		obuf_filled_size_ = &rtd->obuf1_filled_size;
		esa_debug("%s: use obuf0\n", __func__);
	} else {
		obuf = rtd->obuf1;
		obuf_filled = &rtd->obuf1_filled;
		obuf_filled_size = &rtd->obuf1_filled_size;

		obuf_ = rtd->obuf0;
		obuf_filled_ = &rtd->obuf0_filled;
		obuf_filled_size_ = &rtd->obuf0_filled_size;
		esa_debug("%s: use obuf1\n", __func__);
	}

	/* select OBUF0 or OBUF1 for next reading */
	rtd->select_obuf = !rtd->select_obuf;

	/* later... invalidate obuf cache */

	/* send pcm data to user */
	if (copy_to_user((void *)buffer, obuf, *obuf_filled_size)) {
		esa_err("%s: failed to copy_to_user\n", __func__);
		goto err;
	}

	/* if meet eos, it sholud also collect data of another buff */
	if (rtd->get_eos && !*obuf_filled_) {
		rtd->get_eos = EOS_FINAL;
	}

	esa_debug("%s: handle_id[%x], idx:[%d], obuf:[%d], obuf_filled_size:[%d]\n",
			__func__, rtd->handle_id, rtd->idx, !rtd->select_obuf,
			(u32)*obuf_filled_size);
	*obuf_filled = false;

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return *obuf_filled_size;
err:
	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);
	return -EFAULT;
}

static int esa_exe(struct file *file, unsigned int param,
							unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	struct audio_mem_info_t ibuf_info, obuf_info;
	int response, obuf_filled_size;
	unsigned char *ibuf = rtd->ibuf0;
	unsigned char *obuf = rtd->obuf0;
	int ret = 0;

	mutex_lock(&esa_mutex);

	/* receive ibuf_info from user */
	if (copy_from_user(&ibuf_info, (struct audio_mem_info_t *)arg,
					sizeof(struct audio_mem_info_t))) {
		esa_err("%s: failed to copy_from_user ibuf_info\n", __func__);
		goto err;
	}

	/* receive obuf_info from user */
	arg += sizeof(struct audio_mem_info_t);
	if (copy_from_user(&obuf_info, (struct audio_mem_info_t *)arg,
					sizeof(struct audio_mem_info_t))) {
		esa_err("%s: failed to copy_from_user obuf_info\n", __func__);
		goto err;
	}

	/* prevent test alarmed about tainted data */
	if (ibuf_info.mem_size > rtd->ibuf_size) {
		ibuf_info.mem_size = rtd->ibuf_size;
		esa_err("%s: There is too much input data", __func__);
	}

	/* receive pcm data from user */
	if (copy_from_user(ibuf, (void *)ibuf_info.virt_addr,
					ibuf_info.mem_size)) {
		esa_err("%s: failed to copy_from_user\n", __func__);
		goto err;
	}

	/* send execute command to FW for decoding */
	response = esa_send_cmd_exe(rtd, ibuf, obuf, ibuf_info.mem_size);

	/* filled size in OBUF */
	obuf_filled_size = readl(si.mailbox + SIZE_OUT_DATA);

	/* later... invalidate obuf cache */

	if (!response && obuf_filled_size > 0) {
		esa_debug("%s: cmd_exe OK!\n", __func__);
		/* send pcm data to user */
		if (copy_to_user((void *)obuf_info.virt_addr,
					obuf, obuf_filled_size)) {
			esa_err("%s: failed to copy_to_user obuf pcm\n",
					__func__);
			goto err;
		}
	} else {
		esa_debug("%s: cmd_exe Fail: %d\n", __func__, response);
	}

	esa_debug("%s: handle_id[%x], idx:[%d], filled_size:[%d]\n",
			__func__, rtd->handle_id, rtd->idx, obuf_filled_size);

	return ret;
err:
	mutex_unlock(&esa_mutex);
	return -EFAULT;
}

static int esa_set_params(struct file *file, unsigned int param,
							unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	int ret = 0;

	switch (param) {
	case PCM_PARAM_MAX_SAMPLE_RATE:
		esa_debug("PCM_PARAM_MAX_SAMPLE_RATE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_NUM_OF_CH:
		esa_debug("PCM_PARAM_MAX_NUM_OF_CH: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_MAX_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_SAMPLE_RATE:
		esa_debug("%s: sampling rate: %ld\n", __func__, arg);
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		writel((unsigned int)arg, si.mailbox + PARAMS_VAL1);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		break;
	case PCM_PARAM_NUM_OF_CH:
		esa_debug("%s: channel: %ld\n", __func__, arg);
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		writel((unsigned int)arg, si.mailbox + PARAMS_VAL1);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		break;
	case PCM_PARAM_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_MAX_CONFIG_INFO:
		esa_debug("PCM_MAX_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case PCM_CONFIG_INFO:
		esa_debug("PCM_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case EQ_PARAM_NUM_OF_PRESETS:
		esa_debug("EQ_PARAM_NUM_OF_PRESETS: arg:%ld\n", arg);
		break;
	case EQ_PARAM_MAX_NUM_OF_BANDS:
		esa_debug("EQ_PARAM_MAX_NUM_OF_BANDS: arg:%ld\n", arg);
		break;
	case EQ_PARAM_RANGE_OF_BANDLEVEL:
		esa_debug("EQ_PARAM_RANGE_OF_BANDLEVEL: arg:%ld\n", arg);
		break;
	case EQ_PARAM_RANGE_OF_FREQ:
		esa_debug("EQ_PARAM_RANGE_OF_FREQ: arg:%ld\n", arg);
		break;
	case EQ_PARAM_PRESET_ID:
		esa_debug("%s: eq preset: %ld\n", __func__, arg);
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		writel((unsigned int)arg, si.mailbox + PARAMS_VAL1);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		break;
	case EQ_PARAM_NUM_OF_BANDS:
		esa_debug("EQ_PARAM_NUM_OF_BANDS: arg:%ld\n", arg);
		break;
	case EQ_PARAM_CENTER_FREQ:
		esa_debug("EQ_PARAM_CENTER_FREQ: arg:%ld\n", arg);
		break;
	case EQ_PARAM_BANDLEVEL:
		esa_debug("EQ_PARAM_BANDLEVEL: arg:%ld\n", arg);
		break;
	case EQ_PARAM_BANDWIDTH:
		esa_debug("EQ_PARAM_BANDWIDTH: arg:%ld\n", arg);
		break;
	case EQ_MAX_CONFIG_INFO:
		esa_debug("EQ_MAX_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case EQ_CONFIG_INFO:
		esa_debug("EQ_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case EQ_BAND_INFO:
		esa_debug("EQ_BAND_INFO: arg:%ld\n", arg);
		break;
	case ADEC_PARAM_SET_EOS:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_SET_PARAMS);
		esa_debug("ADEC_PARAM_SET_EOS: handle_id:%x\n", rtd->handle_id);
		rtd->get_eos = EOS_YET;
		break;
	case SET_IBUF_POOL_INFO:
		esa_debug("SET_IBUF_POOL_INFO: arg:%ld\n", arg);
		break;
	case SET_OBUF_POOL_INFO:
		esa_debug("SET_OBUF_POOL_INFO: arg:%ld\n", arg);
		break;
	default:
		esa_err("%s: Unknown %ld\n", __func__, arg);
		break;
	}

	return ret;
}

static int esa_get_params(struct file *file, unsigned int param,
							unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	struct audio_mem_info_t *argp = (struct audio_mem_info_t *)arg;
	struct audio_pcm_config_info_t *argp_pcm_info;
	unsigned long val;
	int ret = 0;

	switch (param) {
	case PCM_PARAM_MAX_SAMPLE_RATE:
		esa_debug("PCM_PARAM_MAX_SAMPLE_RATE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_NUM_OF_CH:
		esa_debug("PCM_PARAM_MAX_NUM_OF_CH: arg:%ld\n", arg);
		break;
	case PCM_PARAM_MAX_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_MAX_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_PARAM_SAMPLE_RATE:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_GET_PARAMS);
		val = readl(si.mailbox + RETURN_CMD);
		esa_debug("%s: sampling rate:%ld\n", __func__, val);

		if (val >= SAMPLE_RATE_MIN) {
			esa_debug("SAMPLE_RATE: SUCCESS: arg:%ld\n", arg);
			ret = copy_to_user((unsigned long *)arg, &val,
						sizeof(unsigned long));
		} else
			esa_debug("SAMPLE_RATE: FAILED: arg:%ld\n", arg);
		break;
	case PCM_PARAM_NUM_OF_CH:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_GET_PARAMS);
		val = readl(si.mailbox + RETURN_CMD);
		esa_debug("%s: Channel:%ld\n", __func__, val);

		if (val >= CH_NUM_MIN) {
			esa_debug("PCM_CONFIG_NUM_CH: SUCCESS: arg:%ld\n", arg);
			ret = copy_to_user((unsigned long *)arg, &val,
						sizeof(unsigned long));
		} else
			esa_debug("PCM_PARAM_NUM_CH: FAILED: arg:%ld\n", arg);
		break;
	case PCM_PARAM_BIT_PER_SAMPLE:
		esa_debug("PCM_PARAM_BIT_PER_SAMPLE: arg:%ld\n", arg);
		break;
	case PCM_MAX_CONFIG_INFO:
		esa_debug("PCM_MAX_CONFIG_INFO: arg:%ld\n", arg);
		break;
	case PCM_CONFIG_INFO:
		argp_pcm_info = (struct audio_pcm_config_info_t *)arg;
		rtd->pcm_info.sample_rate = readl(si.mailbox + FREQ_SAMPLE);
		rtd->pcm_info.num_of_channel = readl(si.mailbox + CH_NUM);
		esa_debug("%s: rate:%d, ch:%d\n", __func__,
					rtd->pcm_info.sample_rate,
					rtd->pcm_info.num_of_channel);
		if (rtd->pcm_info.sample_rate >= 8000 &&
			rtd->pcm_info.num_of_channel > 0) {
			esa_debug("PCM_CONFIG_INFO: SUCCESS: arg:%ld\n", arg);
			ret = copy_to_user(argp_pcm_info, &rtd->ibuf_info,
					sizeof(struct audio_pcm_config_info_t));
		} else
			esa_debug("PCM_CONFIG_INFO: FAILED: arg:%ld\n", arg);
		break;
	case ADEC_PARAM_GET_OUTPUT_STATUS:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd((param << 16) | CMD_GET_PARAMS);
		val = readl(si.mailbox + RETURN_CMD);
		esa_debug("OUTPUT_STATUS:%ld, handle_id:%x\n", val, rtd->handle_id);
		if (val && rtd->get_eos == EOS_YET)
			val = 0;
		ret = copy_to_user((unsigned long *)arg, &val,
					sizeof(unsigned long));
		break;
	case GET_IBUF_POOL_INFO:
		esa_debug("GET_IBUF_POOL_INFO: arg:%ld\n", arg);
		rtd->ibuf_info.phy_addr = (u32) rtd->ibuf0;
		rtd->ibuf_info.mem_size = rtd->ibuf_size;
		rtd->ibuf_info.block_count = rtd->ibuf_count;
		ret = copy_to_user(argp, &rtd->ibuf_info,
					sizeof(struct audio_mem_info_t));
		break;
	case GET_OBUF_POOL_INFO:
		esa_debug("GET_OBUF_POOL_INFO: arg:%ld\n", arg);
		rtd->obuf_info.phy_addr = (u32) rtd->obuf0;
		rtd->obuf_info.mem_size = rtd->obuf_size;
		rtd->obuf_info.block_count = rtd->obuf_count;
		ret = copy_to_user(argp, &rtd->obuf_info,
					sizeof(struct audio_mem_info_t));
		break;
	default:
		esa_err("%s: Unknown %ld\n", __func__, arg);
		break;
	}

	if (ret)
		esa_err("%s: failed to copy_to_user\n", __func__);

	return ret;
}

static irqreturn_t esa_isr(int irqno, void *id)
{
	unsigned int fw_stat, log_size, val;
	bool wakeup = false;

	fw_stat = readl(si.mailbox + RETURN_CMD) >> 16;
	switch (fw_stat) {
	case INTR_WAKEUP:	/* handle wakeup interrupt from firmware  */
		si.isr_done = 1;
		wakeup = true;
		break;
	case INTR_READY:
		pr_debug("FW is ready!\n");
		si.fw_ready = true;
		wakeup = true;
		break;
	case INTR_FW_LOG:	/* print debug message from firmware */
		log_size = readl(si.mailbox + RETURN_CMD) & 0x00FF;
		if (!log_size) {
			esa_debug("FW: %s", si.fw_log);
			si.fw_log_pos = 0;
		} else {
			val = readl(si.mailbox + FW_LOG_VAL1);
			memcpy(si.fw_log + si.fw_log_pos, &val, 4);
			val = readl(si.mailbox + FW_LOG_VAL2);
			memcpy(si.fw_log + si.fw_log_pos + 4, &val, 4);
			si.fw_log_pos += log_size;
			si.fw_log[si.fw_log_pos] = '\0';
		}
		writel(0, si.mailbox + RETURN_CMD);
		break;
	default:
		esa_err("%s: unknown intr_stat = 0x%08X\n",
						__func__, fw_stat);
		break;
	}

	if (wakeup && waitqueue_active(&esa_wq))
		wake_up_interruptible(&esa_wq);

	writel(0, si.regs + SW_INTR_CPU);

	return IRQ_HANDLED;
}

static long esa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct esa_rtd *rtd = file->private_data;
	int ret = 0;
	unsigned int param = cmd >> 16;

	cmd = cmd & 0xffff;

	esa_debug("%s: idx:%d, param:%x, cmd:%x\n", __func__,
				rtd->idx, param, cmd);

	mutex_lock(&esa_mutex);
	pm_runtime_get_sync(&si.pdev->dev);

	switch (cmd) {
	case SEIREN_IOCTL_CH_CREATE:
		rtd->ip_type = (unsigned int) arg;
		arg = arg << 16;
		writel(arg, si.mailbox + IP_TYPE);
		ret = esa_send_cmd(CMD_CREATE);
		if (ret == -EBUSY)
			break;
		ret = readl(si.mailbox + RETURN_CMD);
		if (ret != 0)
			break;
		rtd->handle_id = readl(si.mailbox + IP_ID);
		esa_buffer_init(file);
		esa_debug("CH_CREATE: ret_val:%x, handle_id:%x\n",
				readl(si.mailbox + RETURN_CMD),
				rtd->handle_id);
		break;
	case SEIREN_IOCTL_CH_DESTROY:
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		ret = esa_send_cmd(CMD_DESTROY);
		if (ret == -EBUSY)
			break;
		ret = readl(si.mailbox + RETURN_CMD);
		if (ret != 0)
			break;
		esa_debug("CH_DESTROY: ret_val:%x, handle_id:%x\n",
				readl(si.mailbox + RETURN_CMD),
				rtd->handle_id);
		break;
	case SEIREN_IOCTL_CH_EXE:
		esa_debug("CH_EXE\n");
		ret = esa_exe(file, param, arg);
		break;
	case SEIREN_IOCTL_CH_SET_PARAMS:
		esa_debug("CH_SET_PARAMS\n");
		ret = esa_set_params(file, param, arg);
		break;
	case SEIREN_IOCTL_CH_GET_PARAMS:
		esa_debug("CH_GET_PARAMS\n");
		ret = esa_get_params(file, param, arg);
		break;
	case SEIREN_IOCTL_CH_RESET:
		esa_debug("CH_RESET\n");
		break;
	case SEIREN_IOCTL_CH_FLUSH:
		arg = arg << 16;
		writel(rtd->handle_id, si.mailbox + HANDLE_ID);
		esa_send_cmd(CMD_RESET);
		esa_debug("CH_FLUSH: val: %x, handle_id : %x\n",
				readl(si.mailbox + RETURN_CMD),
				rtd->handle_id);
		rtd->get_eos = EOS_NO;
		break;
	case SEIREN_IOCTL_CH_CONFIG:
		esa_debug("CH_CONFIG\n");
		rtd->need_config = true;
		break;
	}

	pm_runtime_mark_last_busy(&si.pdev->dev);
	pm_runtime_put_sync_autosuspend(&si.pdev->dev);
	mutex_unlock(&esa_mutex);

	return ret;
}

static int esa_open(struct inode *inode, struct file *file)
{
	struct esa_rtd *rtd;

	if (!si.fwmem_loaded) {
		esa_err("Firmware not ready\n");
		return -ENXIO;
	}

	/* alloc rtd */
	rtd = esa_alloc_rtd();
	if (!rtd) {
		esa_debug("%s: Not enough memory\n", __func__);
		return -EFAULT;
	}

	esa_debug("%s: idx:%d\n", __func__, rtd->idx);

	/* initialize */
	file->private_data = rtd;
	rtd->get_eos = EOS_NO;
	rtd->need_config = false;
	rtd->select_ibuf = 0;
	rtd->select_obuf = 0;
	rtd->obuf0_filled = false;
	rtd->obuf1_filled = false;

	return 0;
}

static int esa_release(struct inode *inode, struct file *file)
{
	struct esa_rtd *rtd = file->private_data;

	esa_debug("%s: idx:%d\n", __func__, rtd->idx);

	/* de-initialize */
	file->private_data = NULL;

	esa_free_rtd(rtd);

	return 0;
}

static int esa_mmap(struct file *filep, struct vm_area_struct *vma)
{
	esa_debug("%s: called\n", __func__);

	return 0;
}

static const struct file_operations esa_fops = {
	.owner		= THIS_MODULE,
	.open		= esa_open,
	.release	= esa_release,
	.read		= esa_read,
	.write		= esa_write,
	.unlocked_ioctl	= esa_ioctl,
	.mmap		= esa_mmap,
};

static struct miscdevice esa_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "seiren",
	.fops		= &esa_fops,
};

static int esa_proc_show(struct seq_file *m, void *v)
{
	char log[256];
	char *addr = si.fw_log_buf;
	int n;

	seq_printf(m, "fw is %s\n", si.fw_ready ? "ready" : "off");
	seq_printf(m, "rtd cnt: %d\n", si.rtd_cnt);

	if (si.fw_ready) {
		seq_printf(m, "fw_log:\n");
		for (n = 0; n < FW_LOG_LINE; n++, addr += 128) {
			memcpy(log, addr, 128);
			seq_printf(m, "%s", log);
		}
	}

	return 0;
}

static int esa_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, esa_proc_show, NULL);
}

static const struct file_operations esa_proc_fops = {
	.owner = THIS_MODULE,
	.open = esa_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int esa_do_suspend(struct device *dev)
{
	esa_fw_shutdown();
	clk_disable_unprepare(si.clk_ca5);
	lpass_put_sync(dev);

	return 0;
}

static int esa_do_resume(struct device *dev)
{
	lpass_get_sync(dev);
	clk_prepare_enable(si.clk_ca5);
	esa_fw_startup();

	return 0;
}

#if !defined(CONFIG_PM_RUNTIME) && defined(CONFIG_PM_SLEEP)
static int esa_suspend(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_do_suspend(dev);
}

static int esa_resume(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_do_resume(dev);
}
#else
#define esa_suspend	NULL
#define esa_resume	NULL
#endif

#ifdef CONFIG_SND_SAMSUNG_IOMMU
static int esa_prepare_buffer(struct device *dev)
{
	unsigned long iova;
	int n, ret;

	/* Original firmware */
	si.fwmem = devm_kzalloc(dev, FWMEM_SIZE, GFP_KERNEL);
	if (!si.fwmem) {
		esa_err("Failed to alloc fwmem\n");
		goto err;
	}
	si.fwmem_pa = virt_to_phys(si.fwmem);

	/* Firmware backup for SRAM */
	si.fwmem_sram_bak = devm_kzalloc(dev, SRAM_FW_MAX, GFP_KERNEL);
	if (!si.fwmem) {
		esa_err("Failed to alloc fwmem\n");
		goto err;
	}

	/* Firmware for DRAM */
	for (n = 0; n < FWAREA_NUM; n++) {
		si.fwarea[n] = dma_alloc_writecombine(dev, FWAREA_SIZE,
					&si.fwarea_pa[n], GFP_KERNEL);
		if (!si.fwarea[n]) {
			esa_err("Failed to alloc fwarea\n");
			goto err0;
		}
	}

	for (n = 0, iova = FWAREA_IOVA;
			n < FWAREA_NUM; n++, iova += FWAREA_SIZE) {
		ret = iommu_map(si.domain, iova,
				si.fwarea_pa[n], FWAREA_SIZE, 0);
		if (ret) {
			esa_err("Failed to map iommu\n");
			goto err1;
		}
	}

	/* Base address for IBUF, OBUF and FW LOG  */
	si.bufmem = si.fwarea[0] + BASEMEM_OFFSET;
	si.bufmem_pa = si.fwarea_pa[0];
	si.fw_log_buf = si.sram + FW_LOG_ADDR;

	return 0;
err1:
	for (n = 0, iova = FWAREA_IOVA;
			n < FWAREA_NUM; n++, iova += FWAREA_SIZE) {
		iommu_unmap(si.domain, iova, FWAREA_SIZE);
	}
err0:
	for (n = 0; n < FWAREA_NUM; n++) {
		if (si.fwarea[n])
			dma_free_writecombine(dev, FWAREA_SIZE,
					si.fwarea[n], si.fwarea_pa[n]);
	}
err:
	return -ENOMEM;
}
#endif

static void esa_fw_request_complete(const struct firmware *fw_sram, void *ctx)
{
	const struct firmware *fw_dram;
	struct device *dev = ctx;

	if (!fw_sram) {
		esa_err("Failed to requset firmware[%s]\n", FW_SRAM_NAME);
		return;
	}

	if (request_firmware(&fw_dram, FW_DRAM_NAME, dev)) {
		esa_err("Failed to requset firmware[%s]\n", FW_DRAM_NAME);
		return;
	}

	si.fwmem_loaded = true;
	si.fw_sbin_size = fw_sram->size;
	si.fw_dbin_size = fw_dram->size;

	memcpy(si.fwmem, fw_sram->data, si.fw_sbin_size);
	memcpy(si.fwmem + si.fw_sbin_size, fw_dram->data, si.fw_dbin_size);

	esa_info("FW Loaded (SRAM = %d, DRAM = %d)\n",
			si.fw_sbin_size, si.fw_dbin_size);

	return;
}

static const char banner[] =
	KERN_INFO "Exynos Seiren Audio driver, (c)2013 Samsung Electronics\n";

static int esa_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	int ret = 0;

	printk(banner);

	spin_lock_init(&si.lock);

	si.pdev = pdev;

	ret = misc_register(&esa_miscdev);
	if (ret) {
		esa_err("Cannot register miscdev\n");
		return -ENODEV;
	}

	si.regs = lpass_get_regs();
	if (!si.regs) {
		esa_err("Failed to get lpass regs\n");
		return -ENODEV;
	}

	si.sram = lpass_get_mem();
	if (!si.sram) {
		esa_err("Failed to get lpass sram\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	si.irq_ca5 = res->start;

	si.mailbox = si.regs + 0x80;

	si.clk_ca5 = clk_get(dev, "ca5");
	if (IS_ERR(si.clk_ca5)) {
		dev_err(dev, "ca5 clk not found\n");
		return -ENODEV;
	}

	si.opclk_ca5 = clk_get(dev, "ca5_opclk");
	if (IS_ERR(si.opclk_ca5)) {
		dev_err(dev, "ca5 opclk not found\n");
		clk_put(si.clk_ca5);
		return -ENODEV;
	}

	if (np) {
		if (of_find_property(np, "samsung,lpass-subip", NULL))
			lpass_register_subip(dev, "ca5");
	}

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	si.domain = lpass_get_iommu_domain();
	if (!si.domain) {
		dev_err(dev, "iommu not available\n");
		goto err;
	}

	/* prepare buffer */
	if (esa_prepare_buffer(dev))
		goto err;
#else
	dev_err(dev, "iommu not available\n");
	goto err;
#endif

	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      FW_SRAM_NAME,
				      dev,
				      GFP_KERNEL,
				      dev,
				      esa_fw_request_complete);
	if (ret) {
		dev_err(dev, "could not load firmware\n");
		goto err;
	}

	ret = request_irq(si.irq_ca5, esa_isr, 0, "lpass-ca5", 0);
	if (ret < 0) {
		esa_err("Failed to claim CA5 irq\n");
		goto err;
	}

#ifdef CONFIG_PROC_FS
	si.proc_file = proc_create("driver/seiren", 0, NULL, &esa_proc_fops);
	if (!si.proc_file)
		esa_info("Failed to register /proc/driver/seiren\n");
#endif
	/* hold reset */
	lpass_reset(LPASS_IP_CA5, LPASS_OP_RESET);

	esa_debug("regs       = %08X\n", (u32)si.regs);
	esa_debug("mailbox    = %08X\n", (u32)si.mailbox);
	esa_debug("bufmem_pa  = %08X\n", si.bufmem_pa);
	esa_debug("fwmem_pa   = %08X\n", si.fwmem_pa);
	esa_debug("ca5 opclk  = %ldHz\n", clk_get_rate(si.opclk_ca5));

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, 300);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	pm_runtime_put_sync(dev);
#else
	esa_do_resume(dev);
#endif

	return 0;

err:
	clk_put(si.opclk_ca5);
	clk_put(si.clk_ca5);
	return -ENODEV;
}

static int esa_remove(struct platform_device *pdev)
{
	int ret = 0;

	free_irq(si.irq_ca5, 0);
	ret = misc_deregister(&esa_miscdev);
	if (ret)
		esa_err("Cannot deregister miscdev\n");

#ifndef CONFIG_PM_RUNTIME
	clk_disable_unprepare(si.clk_ca5);
	lpass_put_sync(&pdev->dev);
#endif
	clk_put(si.clk_ca5);
	clk_put(si.opclk_ca5);

	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int esa_runtime_suspend(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_do_suspend(dev);
}

static int esa_runtime_resume(struct device *dev)
{
	esa_debug("%s entered\n", __func__);

	return esa_do_resume(dev);
}
#endif

static struct platform_device_id esa_driver_ids[] = {
	{
		.name	= "samsung-seiren",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, esa_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id exynos_esa_match[] = {
	{
		.compatible = "samsung,exynos5430-seiren",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_esa_match);
#endif

static const struct dev_pm_ops esa_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		esa_suspend,
		esa_resume
	)
	SET_RUNTIME_PM_OPS(
		esa_runtime_suspend,
		esa_runtime_resume,
		NULL
	)
};

static struct platform_driver esa_driver = {
	.probe		= esa_probe,
	.remove		= esa_remove,
	.id_table	= esa_driver_ids,
	.driver		= {
		.name	= "samsung-seiren",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_esa_match),
		.pm	= &esa_pmops,
	},
};

module_platform_driver(esa_driver);

MODULE_AUTHOR("Yongjin Jo <yjin.jo@samsung.com>, "
              "Yeongman Seo <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Exynos Seiren Audio driver");
MODULE_LICENSE("GPL");
