/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include "fimc-is-companion.h"

#define COMP_FW				"companion_fw.bin"
#define COMP_SETFILE_MASTER		"companion_master_setfile.bin"
#define COMP_SETFILE_MODE		"companion_mode_setfile.bin"
#define COMP_SETFILE_VIRSION_SIZE	16
#define COMP_MAGIC_NUMBER		(0x73c1)

static int fimc_is_comp_spi_read(struct spi_device *spi,
		void *buf, u32 rx_addr, size_t size)
{
	unsigned char req_data[4] = { 0x03,  };
	int ret;

	struct spi_transfer t_c;
	struct spi_transfer t_r;

	struct spi_message m;

	memset(&t_c, 0x00, sizeof(t_c));
	memset(&t_r, 0x00, sizeof(t_r));

	req_data[1] = (rx_addr & 0xFF00) >> 8;
	req_data[2] = (rx_addr & 0xFF);

	t_c.tx_buf = req_data;
	t_c.len = 4;
	t_c.cs_change = 1;
	t_c.bits_per_word = 32;

	t_r.rx_buf = buf;
	t_r.len = size;

	spi_message_init(&m);
	spi_message_add_tail(&t_c, &m);
	spi_message_add_tail(&t_r, &m);

	ret = spi_sync(spi, &m);
	if (ret) {
		err("spi sync error - can't read data");
		return -EIO;
	} else
		return 0;
}

static int fimc_is_comp_spi_single_write(struct spi_device *spi, u32 data)
{
	int ret = 0;
	u8 tx_buf[5];

	tx_buf[0] = 0x02; /* write cmd */
	tx_buf[1] = (data >> 24) & 0xFF; /* address */
	tx_buf[2] = (data >> 16) & 0xFF; /* address */
	tx_buf[3] = (data >>  8) & 0xFF; /* data */
	tx_buf[4] = (data >>  0) & 0xFF; /* data */

	ret = spi_write(spi, &tx_buf[0], 5);
	if (ret)
		err("spi sync error - can't read data");

	return ret;
}

/* Burst mode: <command><address><data data data ...>
 * Burst width: Maximun value is 512.
 */
static int fimc_is_comp_spi_burst_write(struct spi_device *spi,
	u8 *buf, size_t size, size_t burst_width)
{
	int ret = 0;
	u32 i, j;
	u8 tx_buf[512];
	size_t burst_size;

	/* check multiples of 2 */
	burst_width = (burst_width + 2 - 1) / 2 * 2;

	burst_size = size / burst_width * burst_width;

	for (i = 0; i < burst_size; i += burst_width) {
		tx_buf[0] = 0x02; /* write cmd */
		tx_buf[1] = 0x6F; /* address */
		tx_buf[2] = 0x12; /* address */

		for (j = 0; j < burst_width; j++)
			tx_buf[j + 3] = *(buf + i + j); /* data */

		ret = spi_write(spi, &tx_buf[0], j + 3);
		if (ret) {
			err("spi write error - can't write data");
			goto p_err;
			break;
		}
	}

	tx_buf[0] = 0x02; /* write cmd */
	tx_buf[1] = 0x6F; /* address */
	tx_buf[2] = 0x12; /* address */

	for (j = 0; j < (size - burst_size); j++)
		tx_buf[j + 3] = *(buf + i + j); /* data */

	ret = spi_write(spi, &tx_buf[0], j + 3);
	if (ret)
		err("spi write error - can't write data");


p_err:
	return ret;
}

static int fimc_is_comp_load_binary(struct fimc_is_core *core, char *name)
{
	int ret = 0;
	u32 size = 0;
	const struct firmware *fw_blob;
	static char fw_name[100];
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 1;
	u32 i;
	u8 *buf = NULL;
	u32 data;
	char version_str[60];

	BUG_ON(!core);
	BUG_ON(!core->pdev);
	BUG_ON(!name);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(fw_name, sizeof(fw_name), "/data/%s", name);
	fp = filp_open(fw_name, O_RDONLY, 0);
	if (IS_ERR(fp))
		goto request_fw;

	fw_requested = 0;
	size = fp->f_path.dentry->d_inode->i_size;
	pr_info("start, file path %s, size %d Bytes\n",	fw_name, size);
	buf = vmalloc(size);
	if (!buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}
	nread = vfs_read(fp, (char __user *)buf, size, &fp->f_pos);
	if (nread != size) {
		err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto p_err;
	}

request_fw:
	if (fw_requested) {
		snprintf(fw_name, sizeof(fw_name), "%s", name);
		set_fs(old_fs);
		ret = request_firmware(&fw_blob, fw_name, &core->pdev->dev);
		if (ret) {
			err("request_firmware is fail(ret:%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob) {
			err("fw_blob is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob->data) {
			err("fw_blob->data is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		size = fw_blob->size;
		buf = vmalloc(size);
		if (!buf) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto p_err;
		}
		memcpy((void *)buf, fw_blob->data, size);

		release_firmware(fw_blob);
	}

	if (!strcmp(name, COMP_FW)) {
		ret = fimc_is_comp_spi_burst_write(core->t_spi, buf, size, 256);
		if (ret) {
			err("fimc_is_comp_spi_write() fail");
			goto p_err;
		}
	} else {
		u32 offset = size - COMP_SETFILE_VIRSION_SIZE;

		for (i = 0; i < offset; i += 4) {
			data =	*(buf + i + 0) << 24 |
				*(buf + i + 1) << 16 |
				*(buf + i + 2) << 8 |
				*(buf + i + 3) << 0;

			ret = fimc_is_comp_spi_single_write(core->t_spi, data);
			if (ret) {
				err("fimc_is_comp_spi_setf_write() fail");
				break;
			}
		}

		memcpy(version_str, buf + offset, COMP_SETFILE_VIRSION_SIZE);
		version_str[COMP_SETFILE_VIRSION_SIZE] = '\0';

		info("%s version : %s\n", name, version_str);
	}

p_err:
	if (buf)
		vfree(buf);

	return ret;
}

int fimc_is_comp_is_valid(struct fimc_is_core *core)
{
	int ret = 0;
	u8 read_data[2];
	u16 _read_data;
	u32 read_addr;

	if (!core->t_spi) {
		pr_info("t_spi device is not available");
		goto exit;
	}

	/* check validation(Read data must be 0x73C1) */
	read_addr = 0x0;
	fimc_is_comp_spi_read(core->t_spi, (void *)read_data, read_addr, 2);
	_read_data = read_data[0] << 8 | read_data[1] << 0;
	pr_info("Companion vaildation: 0x%04x\n", _read_data);

	if (_read_data != COMP_MAGIC_NUMBER)
		ret = -EINVAL;

exit:
	return ret;
}

static unsigned int fimc_is_comp_version(struct fimc_is_core *core)
{
	u8 read_data[2];
	u16 comp_ver = 0;
	u32 read_addr;

	if (!core->t_spi) {
		pr_debug("spi1 device is not available");
		goto p_err;
	}

	read_addr = 0x0002;
	fimc_is_comp_spi_read(core->t_spi, (void *)read_data, read_addr, 2);
	comp_ver = read_data[0] << 8 | read_data[1] << 0;
	pr_info("Companion Version: 0x%04X\n", comp_ver);

p_err:
	return comp_ver;
}

int fimc_is_comp_loadfirm(struct fimc_is_core *core)
{
	int ret = 0;
	u32 comp_ver;

	if (!core->t_spi) {
		pr_debug("t_spi device is not available");
		goto p_err;
	}

	fimc_is_comp_spi_single_write(core->t_spi, 0x00000000);

	comp_ver = fimc_is_comp_version(core);

	if (comp_ver == 0x00A0)
		fimc_is_comp_spi_single_write(core->t_spi, 0x02560001);
	else if (comp_ver == 0x00B0)
		fimc_is_comp_spi_single_write(core->t_spi, 0x01220001);
	else
		err("Invlide companion version(%04X)", comp_ver);

	usleep_range(1000, 1000);

	fimc_is_comp_spi_single_write(core->t_spi, 0x60420001);
	fimc_is_comp_spi_single_write(core->t_spi, 0x64280000);
	fimc_is_comp_spi_single_write(core->t_spi, 0x642A0000);

	ret = fimc_is_comp_load_binary(core, COMP_FW);
	if (ret) {
		err("fimc_is_comp_load_binary(%s) fail", COMP_FW);
		goto p_err;
	}

	fimc_is_comp_spi_single_write(core->t_spi, 0x60140001);

	usleep_range(1000, 1000);
	ret = fimc_is_comp_is_valid(core);
	if (ret) {
		err("fimc_is_comp_load_binary(%s) fail", COMP_FW);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_comp_loadsetf(struct fimc_is_core *core)
{
	int ret = 0;

	if (!core->t_spi) {
		pr_debug("t_spi device is not available");
		goto p_err;
	}

	ret = fimc_is_comp_load_binary(core, COMP_SETFILE_MASTER);
	if (ret) {
		err("fimc_is_comp_load_binary(%s) fail", COMP_SETFILE_MASTER);
		goto p_err;
	}

	ret = fimc_is_comp_load_binary(core, COMP_SETFILE_MODE);
	if (ret) {
		err("fimc_is_comp_load_binary(%s) fail", COMP_SETFILE_MODE);
		goto p_err;
	}

p_err:
	return ret;
}
