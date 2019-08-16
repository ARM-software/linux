// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/string.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

#include "ad_regs.h"
#include "ad_debugfs.h"

struct ad_debugfs_data {
	struct device *dev;
	const struct ad_reg *reg;
};

static ssize_t ad_debugfs_register_read(struct file *file,
					char __user *buff,
					size_t count,
					loff_t *ppos)
{
	u32 val, r, i, offset, mask, len;
	char buffer[512];
	struct ad_debugfs_data *debugfs_data =
			(struct ad_debugfs_data *)file->private_data;
	struct ad_dev *ad_dev = dev_get_drvdata(debugfs_data->dev);
	const struct ad_reg *reg = debugfs_data->reg;

	offset = reg->offset;
	mask = reg->mask;
	len = 0;

	for (i = 0; i < reg->number; i++) {
		ad_register_regmap_read(ad_dev->ad_regmap,
				        offset, mask, &val);
		r = snprintf(&buffer[len], 64, "0x%x ", val);
		len += r;
		offset += (reg->bits >> 3);
	}

	buffer[len] = '\n';

	return simple_read_from_buffer(buff, count, ppos, buffer, len);
}

static ssize_t ad_debugfs_register_write(struct file *file,
				         const char __user *user_buf,
				         size_t count,
				         loff_t *ppos)
{
	char *token;
	u32 val, r, offset, mask;
	char buffer[512];
	int written_reg_num = 0;
	char delim[] = " ,;";
	char *s;

	struct ad_debugfs_data *debugfs_data =
			(struct ad_debugfs_data *)file->private_data;
	struct ad_dev *ad_dev = dev_get_drvdata(debugfs_data->dev);
	const struct ad_reg *reg =  debugfs_data->reg;

	if (count > sizeof(buffer))
		return -ENOMEM;

	if (copy_from_user(&buffer[0], user_buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	s = buffer;
	offset = reg->offset;
	mask = reg->mask;

	for (token = strsep(&s, delim); token != NULL;
		 token = strsep(&s, delim)) {
		if (!*token)
			continue;

		r = kstrtou32(token, 10, &val);
		if (0 != r)
			r = kstrtou32(token, 16, &val);

		if ( 0 != r || written_reg_num > reg->number)
			return -EINVAL;

		ad_register_regmap_write(ad_dev->ad_regmap,
					 offset, mask, val);
		ad_dev->ad_dev_funcs->ad_save_hw_status(ad_dev->dev,
							offset,
							val);
		offset += (reg->bits >> 3);
		written_reg_num += 1;
	}

	*ppos += count;
	return count;
}

static const struct file_operations ad_register_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ad_debugfs_register_read,
	.write = ad_debugfs_register_write,
};

int ad_debugfs_register(struct ad_dev *ad_dev)
{
	unsigned int reg_num;
	umode_t mode;
	struct ad_debugfs_data *debugfs_data;
	const struct ad_reg *regs;
	struct device *dev = ad_dev->dev;

	ad_dev->ad_debugfs_dir = debugfs_create_dir(ad_dev->name, NULL);

	if (!ad_dev->ad_debugfs_dir)
			return -1;

	reg_num = ad_dev->ad_dev_funcs->ad_reg_get_all(&regs);
	if (0 != reg_num && regs) {
		struct dentry *registers_dir;
		registers_dir = debugfs_create_dir("registers",
					           ad_dev->ad_debugfs_dir);
		if (registers_dir) {
			int i;
			for (i = 0; i < reg_num; i++) {
				mode = S_IRUGO;
				debugfs_data = devm_kzalloc(dev,
							    sizeof(*debugfs_data),
							    GFP_KERNEL);
				if (!debugfs_data) {
					dev_err(dev, "Failed to alloc ad_dev!\n");
					return -1;
				}

				debugfs_data->dev = dev;
				debugfs_data->reg = &regs[i];
				if (debugfs_data->reg->is_writable)
					mode |= S_IWUSR;

				debugfs_create_file(regs[i].name,
						    mode,
						    registers_dir,
						    debugfs_data,
						    &ad_register_fops);
			}
		}
	}

	ad_dev->ad_dev_funcs->ad_create_debugfs_sw(dev);

	return 0;
}

void ad_debugfs_unregister(struct ad_dev *ad_dev)
{
	if (ad_dev->ad_debugfs_dir)
		debugfs_remove_recursive(ad_dev->ad_debugfs_dir);
}


