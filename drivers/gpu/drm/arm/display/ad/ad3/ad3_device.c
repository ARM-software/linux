// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: luffy.yuan <luffy.yuan@arm.com>
 *
 */
#include <linux/device.h>
#include <linux/log2.h>
#include <linux/pm_runtime.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/string.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif
#include "../ad_device.h"
#include "ad3_device.h"
#include "ad3_regs.h"
#include "ad3_firmware.h"

struct ad3_data {
	struct ad3_firmware_data fw_data;
	u32 ad_op_mode;
	u32 ad_coproc;
	u32 assertive;
	u32 calc_calibration_a;
	u32 calc_calibration_b;
	u32 calc_calibration_c;
	u32 calc_calibration_d;
	u32 ad_strength;
	u32 ad_input_size;
	u32 ad_drc;
	u32 amb_light_value;
	u32 back_light_input;
};

static void ad3_write_strength(struct regmap *ad_regmap, u32 s)
{
	ad_register_regmap_write(ad_regmap,
				 CALC_STRENGTH_REG_OFFSET,
				 CALC_STRENGTH_REG_MASK, s);
}

/* write calibration a, c, d to registers */
static void ad3_write_calibration(struct regmap *ad_regmap,
				  u32 a, u32 c, u32 d)
{
	ad_register_regmap_write(ad_regmap,
				 CALC_CALIBRATION_A_REG_OFFSET,
				 CALC_CALIBRATION_A_REG_MASK, a);

	ad_register_regmap_write(ad_regmap,
				 CALC_CALIBRATION_C_REG_OFFSET,
				 CALC_CALIBRATION_C_REG_MASK, c);

	ad_register_regmap_write(ad_regmap,
				 CALC_CALIBRATION_D_REG_OFFSET,
				 CALC_CALIBRATION_D_REG_MASK, d);
}

static void ad3_write_drc(struct regmap *ad_regmap, u32 d)
{
	ad_register_regmap_write(ad_regmap,
				 CALC_DRC_REG_OFFSET,
				 CALC_DRC_REG_MASK, d);
}

static void ad3_runtime_suspend(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	ad3_save_hw_stat(ad_dev);
	clk_disable_unprepare(ad_dev->aclk);
}

static void ad3_runtime_resume(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad3_data *ad_data = ad_dev->private_data;
	struct ad3_firmware_data *fw_data = &ad_data->fw_data;

	clk_prepare_enable(ad_dev->aclk);

	/*For AD3, always reload status*/
	ad3_firmware_load_data(dev, fw_data);
	ad3_reload_hw_stat(ad_dev);
}

static void ad3_load_firmware(struct device *dev, const u8 *data, size_t size)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad3_data *ad_data = ad_dev->private_data;
	struct ad3_firmware_data *fw_data = &ad_data->fw_data;

	if (size < sizeof (struct ad3_firmware_data)) {
		dev_err(dev, "The size of firmware is invalid.\n");
		return;
	}

	memcpy(fw_data, data, size);

	if (fw_data->firmware_version != AD3_FIRMWARE_VERSION) {
		dev_err(dev, "The firmware version do not match, 0x%x != 0x%x.\n",
		        fw_data->firmware_version, AD3_FIRMWARE_VERSION);
		return;
	}

	if (fw_data->arch_id != ad_dev->chip_info.arch_id) {
		dev_err(dev, "The HW version do not match, 0x%x != 0x%x.\n",
		        fw_data->arch_id, ad_dev->chip_info.arch_id);
		return;
	}

	ad3_firmware_load_data(dev, fw_data);
	ad3_save_hw_stat(ad_dev);
}

static void ad3_save_hw_status(struct device *dev,
			       unsigned int offset,
			       unsigned int value)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad3_data *ad_data = ad_dev->private_data;
	struct ad3_firmware_data *fw_data = &ad_data->fw_data;

	ad3_firmware_save_change(dev,fw_data, offset, value);
	ad3_save_hw_stat(ad_dev);
}

static ssize_t ad3_debugfs_sw_lut_read(struct file *file,
				       char __user *buff,
				       size_t count,
				       loff_t *ppos)
{
	u32 r, i, len = 0;
	char buffer[512];
	u16 *value = (u16 *)file->private_data;

	for (i = 0; i < AD3_LUT_SIZE; i++) {
		r = snprintf(&buffer[len], 64, "0x%x \n", value[i]);
		len += r;
	}

	return simple_read_from_buffer(buff, count, ppos, buffer, len);
}

static ssize_t ad3_debugfs_sw_lut_write(struct file *file,
				        const char __user *user_buf,
				        size_t count,
				        loff_t *ppos)
{
	char *token;
	u32 val, r;
	char buffer[512];
	int written_num = 0;
	char delim[] = " ,;";
	char *s;
	u16 *value = (u16 *)file->private_data;

	if (count > sizeof(buffer))
		return -ENOMEM;

	if (copy_from_user(&buffer[0], user_buf, count))
		return -EFAULT;

	buffer[count] = '\0';
	s = buffer;
	for (token = strsep(&s, delim); token != NULL;
		 token = strsep(&s, delim)) {
		if (!*token)
			continue;

		r = kstrtou32(token, 10, &val);
		if (0 != r)
			r = kstrtou32(token, 16, &val);

		if ( 0 != r || written_num >= AD3_LUT_SIZE)
			return -EINVAL;

		value[written_num] = (u16)val;
		written_num += 1;
	}

	*ppos += count;
	return count;
}

static const struct file_operations ad3_sw_lut_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ad3_debugfs_sw_lut_read,
	.write = ad3_debugfs_sw_lut_write,
};

static ssize_t ad3_debugfs_read_u32(struct file *file,
				    char __user *buff,
				    size_t count,
				    loff_t *ppos)
{
	int r;
	char buffer[512];
	u32 *value = file->private_data;

	r = snprintf(buffer, 64, "%d \n", *value);

	return simple_read_from_buffer(buff, count, ppos, buffer, r);
}

static ssize_t ad3_debugfs_write_u32(struct file *file,
				     const char __user *buff,
				     size_t count,
				     loff_t *ppos)
{
	int ret;
	char buffer[32];
	u32 *value = file->private_data;

	count = min(count, sizeof(buffer) - 1);
	if (copy_from_user(buffer, buff, count))
		return -EFAULT;

	buffer[count] = '\0';

	ret = kstrtou32(buffer, 10, value);
	if (ret) {
		ret = kstrtou32(buffer, 16, value);
		if (ret)
			return ret;
	}

	return count;
}

static const struct file_operations ad3_assertiveness_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ad3_debugfs_read_u32,
};

static const struct file_operations ad3_alpha_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ad3_debugfs_read_u32,
	.write = ad3_debugfs_write_u32,
};

static ssize_t ad3_debugfs_firmware_read(struct file *file,
					 char __user *buff,
					 size_t count,
					 loff_t *ppos)
{
	struct ad3_firmware_data *fw_data = file->private_data;

	return simple_read_from_buffer(buff, count, ppos, fw_data,
				       sizeof(struct ad3_firmware_data));
}

static const struct file_operations ad3_firmware_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = ad3_debugfs_firmware_read,
};

static void ad3_create_debugfs_sw(struct device *dev)
{
	struct dentry *fw_dir;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad3_data *ad_data = ad_dev->private_data;
	struct ad3_firmware_data *fw_data = &ad_data->fw_data;
	struct dentry *sw_dir = debugfs_create_dir("sw_parameters",
					ad_dev->ad_debugfs_dir);

	if (sw_dir) {
		/* The debugfs node for backlight linearization LUT*/
		debugfs_create_file("bl_linearity_lut", S_IRUGO | S_IWUSR,
				    sw_dir, &fw_data->bl_linearity_lut,
				    &ad3_sw_lut_fops);

		/* The debugfs node for backlight attenuation LUT*/
		debugfs_create_file("bl_att_lut", S_IRUGO | S_IWUSR,
				    sw_dir, &fw_data->bl_att_lut,
				    &ad3_sw_lut_fops);

		/* The debugfs node for backlight linearization inverse LUT*/
		debugfs_create_file("bl_linearity_inverse_lut",
				    S_IRUGO | S_IWUSR,
				    sw_dir, &fw_data->bl_linearity_inverse_lut,
				    &ad3_sw_lut_fops);

		/* The debugfs node for assertiveness*/
		debugfs_create_file("assertiveness", S_IRUGO,
				    sw_dir, &ad_data->assertive,
				    &ad3_assertiveness_fops);

		/* The debugfs node for alpha*/
		debugfs_create_file("alpha", S_IRUGO | S_IWUSR,
				    sw_dir, &fw_data->alpha,
				    &ad3_alpha_fops);
	}

	fw_dir = debugfs_create_dir("firmware", ad_dev->ad_debugfs_dir);
	if (fw_dir) {
		/* The debugfs node for firmware downloading*/
		debugfs_create_file("firmware_data", S_IRUGO,
				    fw_dir, &ad_data->fw_data,
				    &ad3_firmware_fops);
	}

}

static int ad3_init(struct device *dev)
{
	struct ad_dev *ad_dev = dev_get_drvdata(dev);
	struct ad3_data *ad_data = ad_dev->private_data;
	struct ad3_firmware_data *fw_data = &ad_data->fw_data;

	ad3_firmware_data_reset(dev, fw_data);
	ad3_save_hw_stat(ad_dev);

	return 0;
}

static void ad3_destroy(struct device *dev)
{
	return;
}

static struct ad_dev_funcs ad3_dev_func = {
	.ad_init = ad3_init,
	.ad_destroy = ad3_destroy,
	.ad_runtime_suspend = ad3_runtime_suspend,
	.ad_runtime_resume= ad3_runtime_resume,
	.ad_load_firmware = ad3_load_firmware,
	.ad_reg_get_all = ad3_register_get_all,
	.ad_save_hw_status = ad3_save_hw_status,
	.ad_create_debugfs_sw = ad3_create_debugfs_sw,
};

struct ad_dev_funcs *ad3_identify(struct device *dev,
				  u32 __iomem *reg)
{
	u32 arch_id, core_id;
	struct ad3_data *ad_data;
	struct ad_dev *ad_dev = dev_get_drvdata(dev);

	ad_dev->ad_regmap = ad3_register_regmap_init(dev, reg);
	if (IS_ERR(ad_dev->ad_regmap))
		return NULL;

	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_ARCH_ID_REG_OFFSET,
				AD_ARCH_ID_REG_MASK, &arch_id);

	if (AD3_ARCH_ID != arch_id) {
		dev_err(dev, "The hardware arch id not match.\n");
		return NULL;
	}

	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_CORE_ID_REG_OFFSET,
				AD_CORE_ID_REG_MASK, &core_id);

	dev_info(dev, "arch_id 0x%x, core_id 0x%x\n", arch_id, core_id);

	ad_dev->chip_info.arch_id = arch_id;
	ad_dev->chip_info.core_id = core_id;

	ad_data = devm_kzalloc(dev, sizeof(*ad_data), GFP_KERNEL);
	if (!ad_data) {
		dev_err(dev, "Failed to alloc ad3_dev_data!\n");
		return NULL;
	}

	ad_dev->private_data = ad_data;

	return  &ad3_dev_func;
}

void ad3_reload_hw_stat(struct ad_dev *ad_dev)
{
	struct ad3_data *ad_data = ad_dev->private_data;

	/*reload the saved HW status*/
	ad_register_regmap_write(ad_dev->ad_regmap,
				AD_CONTROL_REG_OFFSET,
				AD_CONTROL_REG_MODE_MASK,
				ad_data->ad_op_mode);

	ad_register_regmap_write(ad_dev->ad_regmap,
				AD_CONTROL_REG_OFFSET,
				AD_CONTROL_REG_COPR_MASK,
				ad_data->ad_coproc);

	ad_register_regmap_write(ad_dev->ad_regmap,
				AD_INPUT_SIZE_REG_OFFSET,
				AD_INPUT_SIZE_REG_MASK,
				ad_data->ad_input_size);

	ad_register_regmap_write(ad_dev->ad_regmap,
				CALC_CALIBRATION_B_REG_OFFSET,
				CALC_CALIBRATION_B_REG_MASK,
				ad_data->calc_calibration_b);

	ad3_write_calibration(ad_dev->ad_regmap,
			      ad_data->calc_calibration_a,
			      ad_data->calc_calibration_c,
			      ad_data->calc_calibration_d);

	ad3_write_strength(ad_dev->ad_regmap, ad_data->ad_strength);
	ad3_write_drc(ad_dev->ad_regmap, ad_data->ad_drc);
}

void ad3_save_hw_stat(struct ad_dev *ad_dev)
{
	struct ad3_data *ad_data = ad_dev->private_data;

	/*save some HW status*/
	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_CONTROL_REG_OFFSET,
				AD_CONTROL_REG_MODE_MASK,
				&ad_data->ad_op_mode);

	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_CONTROL_REG_OFFSET,
				AD_CONTROL_REG_COPR_MASK,
				&ad_data->ad_coproc);

	ad_register_regmap_read(ad_dev->ad_regmap,
				AD_INPUT_SIZE_REG_OFFSET,
				AD_INPUT_SIZE_REG_MASK,
				&ad_data->ad_input_size);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_CALIBRATION_A_REG_OFFSET,
				CALC_CALIBRATION_A_REG_MASK,
				&ad_data->calc_calibration_a);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_CALIBRATION_B_REG_OFFSET,
				CALC_CALIBRATION_B_REG_MASK,
				&ad_data->calc_calibration_b);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_CALIBRATION_C_REG_OFFSET,
				CALC_CALIBRATION_C_REG_MASK,
				&ad_data->calc_calibration_c);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_CALIBRATION_D_REG_OFFSET,
				CALC_CALIBRATION_D_REG_MASK,
				&ad_data->calc_calibration_d);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_LIGHT_REG_OFFSET,
				CALC_LIGHT_REG_AL_MASK,
				&ad_data->amb_light_value);

	ad_data->amb_light_value = ad_data->amb_light_value >>
				CALC_LIGHT_REG_AL_SHIFT;

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_LIGHT_REG_OFFSET,
				CALC_LIGHT_REG_BL_MASK,
				&ad_data->back_light_input);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_STRENGTH_REG_OFFSET,
				CALC_STRENGTH_REG_MASK,
				&ad_data->ad_strength);

	ad_register_regmap_read(ad_dev->ad_regmap,
				CALC_DRC_REG_OFFSET,
				CALC_DRC_REG_MASK,
				&ad_data->ad_drc);
}
