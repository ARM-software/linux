/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */
#include <linux/version.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_graph.h>
#include <linux/debugfs.h>
#include <uapi/drm/drm_mode.h>

#include "coproc.h"
#include "komeda_drm.h"

struct test_coproc_client {
	struct device *dev;
	u8 dpms_state;
	struct drm_mode_modeinfo mode;
	struct coproc_client *co_client;
	struct dentry *dbg_folder;
	struct komeda_hdr_metadata hdr_data;
};

static int tcc_modeset_cb(struct coproc_client *client,
		const struct drm_mode_modeinfo *mode)
{
	struct test_coproc_client *tcc = coproc_get_drvdata(client);

	tcc->mode = *mode;

	dev_info(tcc->dev, "coprocessor: mode=%ux%u@%u\n",
			mode->hdisplay, mode->vdisplay, mode->vrefresh);
	return 0;
}

static int tcc_dpms_cb(struct coproc_client *client, u8 state)
{
	struct test_coproc_client *tcc = coproc_get_drvdata(client);
	const char *state_string;

	switch (state) {
	case DRM_MODE_DPMS_ON:
		state_string = "DRM_MODE_DPMS_ON";
		break;
	case DRM_MODE_DPMS_STANDBY:
		state_string = "DRM_MODE_DPMS_STANDBY";
		break;
	case DRM_MODE_DPMS_SUSPEND:
		state_string = "DRM_MODE_DPMS_SUSPEND";
		break;
	case DRM_MODE_DPMS_OFF:
		state_string = "DRM_MODE_DPMS_OFF";
		break;
	default:
		dev_err(tcc->dev, "invalid DPMS state!\n");
		return -EINVAL;
	}

	tcc->dpms_state = state;
	dev_info(tcc->dev, "coprocessor: state=%s\n", state_string);
	return 0;
}

static int tcc_frame_data_cb(struct coproc_client *client, const void *data,
				size_t size)
{
	struct test_coproc_client *tcc = coproc_get_drvdata(client);

	if (data != NULL && size != 0)
		tcc->hdr_data = *((struct komeda_hdr_metadata *)data);
	else if (tcc->hdr_data.roi.left != 0xffffffff)
		memset(&tcc->hdr_data, -1,
			sizeof(struct komeda_hdr_metadata));

	dev_info(tcc->dev, "coprocessor: receive %lu bytes!\n", size);
	return 0;
}

static int coproc_dbg_dump_mode(struct seq_file *dump_file, void *unused)
{
	struct coproc_client *client = dump_file->private;
	struct test_coproc_client *tcc = coproc_get_drvdata(client);

	seq_printf(dump_file, "mode=%ux%u @%u\n", tcc->mode.hdisplay,
		   tcc->mode.vdisplay, tcc->mode.vrefresh);
	return 0;
}

static int coproc_dbg_drmmode_open(struct inode *inode, struct file *pfile)
{
	return single_open(pfile, coproc_dbg_dump_mode, inode->i_private);
}

static const struct file_operations f_ops_drmmode = {
	.owner = THIS_MODULE,
	.open = coproc_dbg_drmmode_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static char const* get_hdr_func_str(enum komeda_hdr_eotf eotf)
{
	switch (eotf) {
	case MALIDP_HDR_HLG:
		return "HLG";
	case MALIDP_HDR_ST2084:
		return "ST2084";
	case MALIDP_HDR_NONE:
		/* fall through */
	default:
		return "NONE";
	}
}

static int coproc_dbg_dump_famedata(struct seq_file *dump_file, void *unused)
{
	struct coproc_client *client = dump_file->private;
	struct test_coproc_client *tcc = coproc_get_drvdata(client);
	struct komeda_hdr_metadata d = tcc->hdr_data;
	char const *func = get_hdr_func_str(d.eotf);

	seq_printf(dump_file, "ROI(%u, %u, %u, %u)\n"
		   "EOTF: %s\n"
		   "Primaries red(%u, %u) green(%u, %u) blue(%u, %u)\n"
		   "White point(%u, %u)\n"
		   "Lum(max, min): %u, %u\n"
		   "Light level(max_content, max_frame_average): %u, %u\n",
		d.roi.left, d.roi.top, d.roi.width, d.roi.height,
		func, d.display_primaries_red.x, d.display_primaries_red.y,
		d.display_primaries_green.x, d.display_primaries_green.y,
		d.display_primaries_blue.x, d.display_primaries_blue.y,
		d.white_point.x, d.white_point.y,
		d.max_display_mastering_lum, d.min_display_mastering_lum,
		d.max_content_light_level, d.max_frame_average_light_level);

	return 0;
}

static int coproc_dbg_framedata_open(struct inode *inode, struct file *pfile)
{
	return single_open(pfile, coproc_dbg_dump_famedata, inode->i_private);
}

static const struct file_operations f_ops_framedata = {
	.owner = THIS_MODULE,
	.open = coproc_dbg_framedata_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int init_debugfs(struct test_coproc_client *tcc, u32 id)
{
	struct dentry *dbg_file;
	char dbg_folder_name[64];

	if (!debugfs_initialized())
		return 0;

	snprintf(dbg_folder_name, 64, "test_coproc_client%u", id);
	tcc->dbg_folder = debugfs_create_dir(dbg_folder_name, NULL);
	if (IS_ERR_OR_NULL(tcc->dbg_folder))
		return -EINVAL;

	dbg_file = debugfs_create_u8("dpms_state", S_IRUSR, tcc->dbg_folder,
			&tcc->dpms_state);
	if (dbg_file == NULL)
		return -EINVAL;

	dbg_file = debugfs_create_file("drm_mode", S_IRUSR, tcc->dbg_folder,
				tcc->co_client, &f_ops_drmmode);
	if (dbg_file == NULL)
		return -EINVAL;

	dbg_file = debugfs_create_file("frame_data", S_IRUSR, tcc->dbg_folder,
				tcc->co_client, &f_ops_framedata);
	if (dbg_file == NULL)
		return -EINVAL;

	return 0;
}

static struct coproc_client_callbacks tcc_callbacks = {
	.modeset_cb = tcc_modeset_cb,
	.dpms_cb = tcc_dpms_cb,
	.frame_data_cb = tcc_frame_data_cb
};

static int coproc_client_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct test_coproc_client *tcc;
	struct coproc_client *client;
	u32 reg;
	int ret;

	ret = of_property_read_u32(np, "reg", &reg);
	if (ret) {
		dev_err(&pdev->dev, "read reg property error!\n");
		return -EINVAL;
	}

	tcc = devm_kzalloc(&pdev->dev, sizeof(*tcc), GFP_KERNEL);
	if (tcc == NULL) {
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, tcc);

	client = coproc_register_client(&pdev->dev, &tcc_callbacks);
	if (IS_ERR_OR_NULL(client)) {
		dev_err(&pdev->dev, "test coprocessor client register error!\n");
		return -EINVAL;
	}

	tcc->dev = &pdev->dev;
	tcc->dpms_state = DRM_MODE_DPMS_OFF;
	tcc->co_client = client;

	memset(&tcc->hdr_data, -1, sizeof(struct komeda_hdr_metadata));

	ret = init_debugfs(tcc, reg);
	if (ret) {
		dev_err(tcc->dev, "initialize debugfs error!\n");
		goto tcc_debugfs_err;
	}

	coproc_set_drvdata(client, tcc);

	dev_info(tcc->dev, "test coprocessor client is loaded.\n");
	return 0;

tcc_debugfs_err:
	coproc_unregister_client(client);
	devm_kfree(&pdev->dev, tcc);
	return ret;
}

static int coproc_client_remove(struct platform_device *pdev)
{
	struct test_coproc_client *tcc = dev_get_drvdata(&pdev->dev);

	coproc_unregister_client(tcc->co_client);
	debugfs_remove_recursive(tcc->dbg_folder);
	devm_kfree(&pdev->dev, tcc);

	dev_info(&pdev->dev, "test coprocessor client is removed.\n");
	return 0;
}

static const struct of_device_id test_coproc_ids[] = {
	{ .compatible = "arm,coprocessor_client" },
	{ /* sentinel*/ }
};

static struct platform_driver coproc_test_client = {
	.probe = coproc_client_probe,
	.remove = coproc_client_remove,
	.driver = {
		.name = "coproc test client driver",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(test_coproc_ids),
	}
};

module_platform_driver(coproc_test_client);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Co-processor test client driver");
