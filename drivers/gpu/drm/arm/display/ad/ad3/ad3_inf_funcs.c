// SPDX-License-Identifier: GPL-2.0
/*
 * (C) COPYRIGHT 2018-2019 ARM Limited. All rights reserved.
 * Author: Jonathan Chai <jonathan.chai@arm.com>
 *
 */
#include <linux/pm_runtime.h>
#include "../ad_device.h"
#include "ad3_regs.h"
#include "ad3_assertive_lut.h"
#include "ad3_device.h"

static int
ad3_mode_set(struct ad_coprocessor *ad, struct drm_display_mode *mode)
{
	struct ad_dev *ad_dev = dev_get_drvdata(ad->dev);
	u32 val = mode->crtc_hdisplay;

	if (mode->crtc_hdisplay < AD_INPUT_SIZE_H_MIN ||
		mode->crtc_hdisplay > AD_INPUT_SIZE_H_MAX) {
		dev_err(ad->dev, "display hsize is out of range. [%u]\n",
			mode->crtc_hdisplay);
		return -EINVAL;
	}

	if (mode->crtc_vdisplay < AD_INPUT_SIZE_V_MIN ||
		mode->crtc_vdisplay > AD_INPUT_SIZE_V_MAX) {
		dev_err(ad->dev, "display vsize is out of range. [%u]\n",
			mode->crtc_vdisplay);
		return -EINVAL;
	}

	val |= mode->vdisplay << 16;
	return ad_register_regmap_write(ad_dev->ad_regmap,
					AD_INPUT_SIZE_REG_OFFSET,
					AD_INPUT_SIZE_REG_MASK, val);
}

static int ad3_enable(struct ad_coprocessor *ad)
{
	int ret = 0;
	struct ad_dev *ad_dev = dev_get_drvdata(ad->dev);

	if (ad_dev->is_enabled == true)
		return ret;

	ad_dev->is_enabled = true;

	ret = pm_runtime_get_sync(ad_dev->dev);
	if (ret < 0) {
		dev_err(ad->dev, "Failed to get PM runtime\n");
		return ret;
	}

	return ad_register_regmap_write(ad_dev->ad_regmap,
					AD_CONTROL_REG_OFFSET,
					AD_CONTROL_REG_COPR_MASK, 1);
}

static int ad3_disable(struct ad_coprocessor *ad)
{
	int ret = 0;
	struct ad_dev *ad_dev = dev_get_drvdata(ad->dev);

	if (ad_dev->is_enabled == false)
		return ret;

	ad_dev->is_enabled = false;

	ret = ad_register_regmap_write(ad_dev->ad_regmap,
					AD_CONTROL_REG_OFFSET,
					AD_CONTROL_REG_COPR_MASK, 0);

	pm_runtime_put(ad_dev->dev);

	return ret;
}

static int ad3_assertiveness_set(struct ad_coprocessor *ad, u32 val)
{
	struct ad_dev *ad_dev = dev_get_drvdata(ad->dev);
	uint32_t assertive;

	if (val > AD_MAX_ASSERTIVENESS) {
		dev_err(ad_dev->dev, "assertiveness is out of range.[%u]\n",
			val);
		return -EINVAL;
	}

	assertive = assertive_lut[val];
	ad3_update_calibration(ad_dev, assertive);
	return 0;
}

static int ad3_strength_set(struct ad_coprocessor *ad, u32 val)
{
	struct ad_dev *ad_dev = dev_get_drvdata(ad->dev);

	if (val < AD_STRENGTH_MIN || val > AD_MAX_STRENGTH_LIMIT) {
		dev_err(ad_dev->dev, "strength_limit is out of range: [%u].\n",
			val);
		return -EINVAL;
	}

	ad3_update_strength(ad_dev, val);
	return 0;
}

static int ad3_drc_set(struct ad_coprocessor *ad, u16 drc)
{
	struct ad_dev *ad_dev = dev_get_drvdata(ad->dev);

	ad3_update_drc(ad_dev, drc);
	return 0;
}

struct ad_coprocessor_funcs ad3_intf_funcs = {
	.mode_set		= ad3_mode_set,
	.enable			= ad3_enable,
	.disable		= ad3_disable,
	.assertiveness_set	= ad3_assertiveness_set,
	.strength_set		= ad3_strength_set,
	.drc_set		= ad3_drc_set,
};
