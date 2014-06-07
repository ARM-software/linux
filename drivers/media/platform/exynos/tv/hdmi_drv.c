/*
 * Samsung HDMI interface driver
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *
 * Tomasz Stanislawski, <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundiation. either version 2 of the License,
 * or (at your option) any later version
 */
#include "hdmi.h"

#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <media/v4l2-subdev.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/bug.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/sched.h>
#include <linux/of_gpio.h>
#include <linux/fb.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/gpio-cfg.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/exynos_mc.h>

#include "regs-hdmi.h"

#ifdef CONFIG_OF
static const struct of_device_id hdmi_device_table[] = {
	{ .compatible = "samsung,exynos5-hdmi_driver" },
	{},
};
MODULE_DEVICE_TABLE(of, hdmi_device_table);
#endif

MODULE_AUTHOR("Tomasz Stanislawski, <t.stanislaws@samsung.com>");
MODULE_DESCRIPTION("Samsung HDMI");
MODULE_LICENSE("GPL");

/* I2C module and id for HDMIPHY */
static struct i2c_board_info hdmiphy_info = {
	I2C_BOARD_INFO("hdmiphy", 0x38),
};

static struct hdmi_driver_data hdmi_driver_data[] = {
	{ .hdmiphy_bus = 3 },
	{ .hdmiphy_bus = 8 },
	{ .hdmiphy_bus = 8 },
};

static struct platform_device_id hdmi_driver_types[] = {
	{
		.name		= "s5pv210-hdmi",
		.driver_data	= (unsigned long)&hdmi_driver_data[0],
	}, {
		.name		= "exynos4-hdmi",
		.driver_data	= (unsigned long)&hdmi_driver_data[1],
	}, {
		.name		= "exynos5-hdmi",
		.driver_data	= (unsigned long)&hdmi_driver_data[2],
	}, {
		/* end node */
	}
};

static const struct v4l2_subdev_ops hdmi_sd_ops;

static struct hdmi_device *sd_to_hdmi_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct hdmi_device, sd);
}

bool hdmi_match_timings(const struct v4l2_dv_timings *t1,
			  const struct v4l2_dv_timings *t2,
			  unsigned pclock_delta)
{
	if (t1->type != t2->type)
		return false;
	if (t1->bt.width == t2->bt.width &&
	    t1->bt.height == t2->bt.height &&
	    t1->bt.interlaced == t2->bt.interlaced &&
	    t1->bt.polarities == t2->bt.polarities &&
	    t1->bt.pixelclock >= t2->bt.pixelclock - pclock_delta &&
	    t1->bt.pixelclock <= t2->bt.pixelclock + pclock_delta &&
	    t1->bt.hfrontporch == t2->bt.hfrontporch &&
	    t1->bt.vfrontporch == t2->bt.vfrontporch &&
	    t1->bt.vsync == t2->bt.vsync &&
	    t1->bt.vbackporch == t2->bt.vbackporch &&
	    (!t1->bt.interlaced ||
		(t1->bt.il_vfrontporch == t2->bt.il_vfrontporch &&
		 t1->bt.il_vsync == t2->bt.il_vsync &&
		 t1->bt.il_vbackporch == t2->bt.il_vbackporch)))
		return true;
	return false;
}

static const struct hdmi_timings *hdmi_timing2conf(struct v4l2_dv_timings *timings)
{
	int i;

	for (i = 0; i < hdmi_pre_cnt; ++i)
		if (hdmi_match_timings(&hdmi_conf[i].dv_timings,
					timings, 0))
			break;

	if (i == hdmi_pre_cnt)
		return NULL;

	return hdmi_conf[i].conf;
}

const struct hdmi_3d_info *hdmi_timing2info(struct v4l2_dv_timings *timings)
{
	int i;

	for (i = 0; i < hdmi_pre_cnt; ++i)
		if (hdmi_match_timings(&hdmi_conf[i].dv_timings,
					timings, 0))
			break;

	if (i == hdmi_pre_cnt)
		return NULL;

	return  hdmi_conf[i].info;
}

void s5p_v4l2_int_src_hdmi_hpd(struct hdmi_device *hdev)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(hdev->dev, "hdmi_hdmi_hpd");
	if (IS_ERR(pinctrl))
		dev_err(hdev->dev, "failed to set hdmi hpd interrupt");
}

void s5p_v4l2_int_src_ext_hpd(struct hdmi_device *hdev)
{
	struct pinctrl *pinctrl;

	pinctrl = devm_pinctrl_get_select(hdev->dev, "hdmi_ext_hpd");
	if (IS_ERR(pinctrl))
		dev_err(hdev->dev, "failed to set external hpd interrupt");
}

static int hdmi_set_infoframe(struct hdmi_device *hdev)
{
	struct hdmi_infoframe infoframe;
	const struct hdmi_3d_info *info;

	info = hdmi_timing2info(&hdev->cur_timings);

	if (info->is_3d == HDMI_VIDEO_FORMAT_3D) {
		infoframe.type = HDMI_PACKET_TYPE_VSI;
		infoframe.ver = HDMI_VSI_VERSION;
		infoframe.len = HDMI_VSI_LENGTH;
		hdmi_reg_infoframe(hdev, &infoframe);
	} else
		hdmi_reg_stop_vsi(hdev);

	infoframe.type = HDMI_PACKET_TYPE_AVI;
	infoframe.ver = HDMI_AVI_VERSION;
	infoframe.len = HDMI_AVI_LENGTH;
	hdmi_reg_infoframe(hdev, &infoframe);

	if (hdev->audio_enable) {
		infoframe.type = HDMI_PACKET_TYPE_AUI;
		infoframe.ver = HDMI_AUI_VERSION;
		infoframe.len = HDMI_AUI_LENGTH;
		hdmi_reg_infoframe(hdev, &infoframe);
	}

	return 0;
}

static int hdmi_set_packets(struct hdmi_device *hdev)
{
	hdmi_reg_set_acr(hdev);
	return 0;
}

static void hdmi_audio_information(struct hdmi_device *hdev, u32 value)
{
	struct device *dev = hdev->dev;
	u8 ch = 0;
	u8 br = 0;
	u8 sr = 0;
	int cnt;

	ch = value & AUDIO_CHANNEL_MASK;
	br = (value & AUDIO_BIT_RATE_MASK) >> 16;
	sr = (value & AUDIO_SAMPLE_RATE_MASK) >> 19;

	for (cnt = 0; cnt < 8; cnt++) {
		if (ch & (1 << cnt))
			hdev->audio_channel_count = cnt + 1;
	}
	if (br & FB_AUDIO_16BIT)
		hdev->bits_per_sample = 16;
	else if (br & FB_AUDIO_20BIT)
		hdev->bits_per_sample = 20;
	else if (br & FB_AUDIO_24BIT)
		hdev->bits_per_sample = 24;
	else
		dev_err(dev, "invalid bit per sample\n");

	if (sr & FB_AUDIO_32KHZ)
		hdev->sample_rate = 32000;
	else if (sr & FB_AUDIO_44KHZ)
		hdev->sample_rate = 44100;
	else if (sr & FB_AUDIO_48KHZ)
		hdev->sample_rate = 48000;
	else if (sr & FB_AUDIO_88KHZ)
		hdev->sample_rate = 88000;
	else if (sr & FB_AUDIO_96KHZ)
		hdev->sample_rate = 96000;
	else if (sr & FB_AUDIO_176KHZ)
		hdev->sample_rate = 176000;
	else if (sr & FB_AUDIO_192KHZ)
		hdev->sample_rate = 192000;
	else
		dev_err(dev, "invalid sample rate\n");

	dev_info(dev, "HDMI audio : %d-ch, %dHz, %dbit\n",
			ch, hdev->sample_rate, hdev->bits_per_sample);
}

static int hdmi_streamon(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	int ret;
	u32 val0, val1, val2;

	dev_dbg(dev, "%s\n", __func__);

	/* 3D test */
	hdmi_set_infoframe(hdev);

	/* set packets for audio */
	hdmi_set_packets(hdev);

	/* init audio */
#if defined(CONFIG_VIDEO_EXYNOS_HDMI_AUDIO_I2S)
	hdmi_reg_i2s_audio_init(hdev);
#elif defined(CONFIG_VIDEO_EXYNOS_HDMI_AUDIO_SPDIF)
	hdmi_reg_spdif_audio_init(hdev);
#endif
	/* enbale HDMI audio */
	if (hdev->audio_enable)
		hdmi_audio_enable(hdev, 1);

	hdmi_set_dvi_mode(hdev);

	/* controls the pixel value limitation */
	hdmi_reg_set_limits(hdev);

	/* enable HDMI and timing generator */
	hdmi_enable(hdev, 1);
	hdmi_tg_enable(hdev, 1);

	hdev->streaming = HDMI_STREAMING;

	/* change the HPD interrupt: External -> Internal */
	disable_irq(hdev->ext_irq);
	cancel_delayed_work_sync(&hdev->hpd_work_ext);
	hdmi_reg_set_int_hpd(hdev);
	enable_irq(hdev->int_irq);
	dev_info(hdev->dev, "HDMI interrupt changed to internal\n");

	/* start HDCP if enabled */
	if (hdev->hdcp_info.hdcp_enable) {
		ret = hdcp_start(hdev);
		if (ret)
			return ret;
	}

	val0 = hdmi_read(hdev, HDMI_ACR_MCTS0);
	val1 = hdmi_read(hdev, HDMI_ACR_MCTS1);
	val2 = hdmi_read(hdev, HDMI_ACR_MCTS2);
	dev_dbg(dev, "HDMI_ACR_MCTS0 : 0x%08x\n", val0);
	dev_dbg(dev, "HDMI_ACR_MCTS1 : 0x%08x\n", val1);
	dev_dbg(dev, "HDMI_ACR_MCTS2 : 0x%08x\n", val2);

	hdmi_dumpregs(hdev, "streamon");
	return 0;
}

static int hdmi_streamoff(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (hdev->hdcp_info.hdcp_enable && hdev->hdcp_info.hdcp_start)
		hdcp_stop(hdev);

	hdmi_audio_enable(hdev, 0);
	hdmi_enable(hdev, 0);
	hdmi_tg_enable(hdev, 0);

	hdev->streaming = HDMI_STOP;

	/* change the HPD interrupt: Internal -> External */
	disable_irq(hdev->int_irq);
	cancel_work_sync(&hdev->hpd_work);
	hdmi_reg_set_ext_hpd(hdev);
	enable_irq(hdev->ext_irq);
	dev_info(hdev->dev, "HDMI interrupt changed to external\n");

	hdmi_dumpregs(hdev, "streamoff");
	return 0;
}

static int hdmi_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s(%d)\n", __func__, enable);
	if (enable)
		return hdmi_streamon(hdev);
	return hdmi_streamoff(hdev);
}

static int hdmi_runtime_resume(struct device *dev);
static int hdmi_runtime_suspend(struct device *dev);

static int hdmi_s_power(struct v4l2_subdev *sd, int on)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	int ret = 0;

	/* If runtime PM is not implemented, hdmi_runtime_resume
	 * and hdmi_runtime_suspend functions are directly called.
	 */
	if (on) {
		clk_prepare_enable(hdev->res.hdmi);
#ifdef CONFIG_PM_RUNTIME
		ret = pm_runtime_get_sync(hdev->dev);
#else
		hdmi_runtime_resume(hdev->dev);
#endif
	} else {
#ifdef CONFIG_PM_RUNTIME
		ret = pm_runtime_put_sync(hdev->dev);
#else
		hdmi_runtime_suspend(hdev->dev);
#endif
		clk_disable_unprepare(hdev->res.hdmi);
	}

	/* only values < 0 indicate errors */
	return IS_ERR_VALUE(ret) ? ret : 0;
}

int hdmi_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;
	int ret = 0;

	dev_dbg(dev, "%s start\n", __func__);

	switch (ctrl->id) {
	case V4L2_CID_TV_SET_DVI_MODE:
		hdev->dvi_mode = ctrl->value;
		break;
	case V4L2_CID_TV_SET_ASPECT_RATIO:
		hdev->aspect = ctrl->value;
		break;
	case V4L2_CID_TV_ENABLE_HDMI_AUDIO:
		mutex_lock(&hdev->mutex);
		hdev->audio_enable = !!ctrl->value;
		if (is_hdmi_streaming(hdev)) {
			hdmi_set_infoframe(hdev);
			hdmi_audio_enable(hdev, hdev->audio_enable);
		}
		mutex_unlock(&hdev->mutex);
		break;
	case V4L2_CID_TV_SET_NUM_CHANNELS:
		mutex_lock(&hdev->mutex);
		hdmi_audio_information(hdev, ctrl->value);
		if (is_hdmi_streaming(hdev)) {
			hdmi_set_infoframe(hdev);
			hdmi_reg_i2s_audio_init(hdev);
		}
		mutex_unlock(&hdev->mutex);
		break;
	case V4L2_CID_TV_SET_COLOR_RANGE:
		hdev->color_range = ctrl->value;
		break;
	case V4L2_CID_TV_HDCP_ENABLE:
		hdev->hdcp_info.hdcp_enable = ctrl->value;
		dev_dbg(hdev->dev, "HDCP %s\n",
				ctrl->value ? "enable" : "disable");
		break;
	default:
		dev_err(dev, "invalid control id\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int hdmi_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	switch (ctrl->id) {
	case V4L2_CID_TV_GET_DVI_MODE:
		ctrl->value = hdev->dvi_mode;
		break;
	case V4L2_CID_TV_HPD_STATUS:
		ctrl->value = switch_get_state(&hdev->hpd_switch);
		break;
	case V4L2_CID_TV_HDMI_STATUS:
		ctrl->value = (hdev->streaming |
				switch_get_state(&hdev->hpd_switch));
		break;
	case V4L2_CID_TV_MAX_AUDIO_CHANNELS:
		ctrl->value = edid_audio_informs(hdev);
		break;
	case V4L2_CID_TV_SOURCE_PHY_ADDR:
		ctrl->value = edid_source_phy_addr(hdev);
		break;
	default:
		dev_err(dev, "invalid control id\n");
		return -EINVAL;
	}

	return 0;
}

static int hdmi_s_dv_timings(struct v4l2_subdev *sd,
	struct v4l2_dv_timings *timings)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	hdev->cur_conf = hdmi_timing2conf(timings);
	if (hdev->cur_conf == NULL) {
		dev_err(dev, "timings not supported\n");
		return -EINVAL;
	}
	hdev->cur_timings = *timings;

	if (hdev->streaming == HDMI_STREAMING) {
		hdmi_streamoff(hdev);
		hdmi_s_power(sd, 0);
		hdev->streaming = HDMI_STOP;
	}

	return 0;
}

static int hdmi_g_dv_timings(struct v4l2_subdev *sd,
	struct v4l2_dv_timings *timings)
{
	*timings = sd_to_hdmi_dev(sd)->cur_timings;
	return 0;
}

static int hdmi_enum_dv_timings(struct v4l2_subdev *sd,
	struct v4l2_enum_dv_timings *timings)
{
	if (timings->index >= hdmi_pre_cnt)
		return -EINVAL;
	timings->timings = hdmi_conf[timings->index].dv_timings;

	return 0;
}

static int hdmi_g_mbus_fmt(struct v4l2_subdev *sd,
	  struct v4l2_mbus_framefmt *fmt)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s\n", __func__);
	if (!hdev->cur_conf)
		return -EINVAL;
	*fmt = hdev->cur_conf->mbus_fmt;
	return 0;
}

static int hdmi_s_mbus_fmt(struct v4l2_subdev *sd,
	  struct v4l2_mbus_framefmt *fmt)
{
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct device *dev = hdev->dev;

	dev_dbg(dev, "%s\n", __func__);
	if (fmt->code == V4L2_MBUS_FMT_YUV8_1X24)
		hdev->output_fmt = HDMI_OUTPUT_YUV444;
	else
		hdev->output_fmt = HDMI_OUTPUT_RGB888;

	return 0;
}

static const struct v4l2_subdev_core_ops hdmi_sd_core_ops = {
	.s_power = hdmi_s_power,
	.s_ctrl = hdmi_s_ctrl,
	.g_ctrl = hdmi_g_ctrl,
};

static const struct v4l2_subdev_video_ops hdmi_sd_video_ops = {
	.s_dv_timings = hdmi_s_dv_timings,
	.g_dv_timings = hdmi_g_dv_timings,
	.enum_dv_timings = hdmi_enum_dv_timings,
	.g_mbus_fmt = hdmi_g_mbus_fmt,
	.s_mbus_fmt = hdmi_s_mbus_fmt,
	.s_stream = hdmi_s_stream,
};

static const struct v4l2_subdev_ops hdmi_sd_ops = {
	.core = &hdmi_sd_core_ops,
	.video = &hdmi_sd_video_ops,
};

static int hdmi_runtime_suspend(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct platform_device *pdev = to_platform_device(hdev->dev);
	struct s5p_hdmi_platdata *pdata = hdev->pdata;
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(dev, "%s\n", __func__);

	/* HDMI PHY off sequence
	 * LINK off -> PHY off -> HDMI_PHY_CONTROL disable */

	if (is_ip_ver_5a || is_ip_ver_5s || is_ip_ver_5s2)
		hdmiphy_set_power(hdev, 0);

	/* turn clocks off */
	clk_disable_unprepare(res->sclk_hdmi);

	if (is_ip_ver_5g)
		v4l2_subdev_call(hdev->phy_sd, core, s_power, 0);

	/* power-off hdmiphy */
	if (pdata->hdmiphy_enable)
		pdata->hdmiphy_enable(pdev, 0);

	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdev = sd_to_hdmi_dev(sd);
	struct platform_device *pdev = to_platform_device(hdev->dev);
	struct s5p_hdmi_platdata *pdata = hdev->pdata;
	struct hdmi_resources *res = &hdev->res;
	int ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	/* power-on hdmiphy */
	if (pdata->hdmiphy_enable)
		pdata->hdmiphy_enable(pdev, 1);

	clk_prepare_enable(res->sclk_hdmi);

	if (is_ip_ver_5a || is_ip_ver_5s || is_ip_ver_5s2) {
		hdmiphy_set_power(hdev, 1);
	} else {
		ret = v4l2_subdev_call(hdev->phy_sd, core, s_power, 1);
		if (ret) {
			dev_err(dev, "failed to turn on hdmiphy\n");
			goto fail;
		}
	}

	hdmi_sw_reset(hdev);
	hdmi_phy_sw_reset(hdev);

	ret = hdmi_conf_apply(hdev);
	if (ret)
		goto fail;

	dev_dbg(dev, "poweron succeed\n");

	return 0;

fail:
	clk_disable_unprepare(res->sclk_hdmi);
	v4l2_subdev_call(hdev->phy_sd, core, s_power, 0);
	if (pdata->hdmiphy_enable)
		pdata->hdmiphy_enable(pdev, 0);
	dev_err(dev, "poweron failed\n");

	return ret;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend	= hdmi_runtime_suspend,
	.runtime_resume		= hdmi_runtime_resume,
};

static void hdmi_resources_cleanup(struct hdmi_device *hdev)
{
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(hdev->dev, "HDMI resource cleanup\n");
	/* put clocks */
	if (!IS_ERR_OR_NULL(res->sclk_hdmiphy))
		clk_put(res->sclk_hdmiphy);
	if (!IS_ERR_OR_NULL(res->sclk_pixel))
		clk_put(res->sclk_pixel);
	if (!IS_ERR_OR_NULL(res->sclk_hdmi))
		clk_put(res->sclk_hdmi);
	if (!IS_ERR_OR_NULL(res->hdmi))
		clk_put(res->hdmi);
	memset(res, 0, sizeof *res);
}

static int hdmi_resources_init(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;

	dev_dbg(dev, "HDMI resource init\n");

	memset(res, 0, sizeof *res);
	/* get clocks, power */
	res->hdmi = clk_get(dev, "clk_hdmi");
	if (IS_ERR_OR_NULL(res->hdmi)) {
		dev_err(dev, "failed to get clock 'clk_hdmi'\n");
		goto fail;
	}
	res->sclk_hdmi = clk_get(dev, "sclk_hdmi");
	if (IS_ERR_OR_NULL(res->sclk_hdmi)) {
		dev_err(dev, "failed to get clock 'sclk_hdmi'\n");
		goto fail;
	}
	res->sclk_pixel = clk_get(dev, "sclk_pixel");
	if (IS_ERR_OR_NULL(res->sclk_pixel)) {
		dev_err(dev, "failed to get clock 'sclk_pixel'\n");
		goto fail;
	}

	res->sclk_hdmiphy = clk_get(dev, "sclk_hdmiphy");
	if (IS_ERR_OR_NULL(res->sclk_hdmiphy)) {
		dev_err(dev, "failed to get clock 'sclk_hdmiphy'\n");
		goto fail;
	}

	clk_set_parent(res->sclk_hdmi, res->sclk_hdmiphy);

	return 0;
fail:
	dev_err(dev, "HDMI resource init - failed\n");
	hdmi_resources_cleanup(hdev);
	return -ENODEV;
}

static int hdmi_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	return 0;
}

/* hdmi entity operations */
static const struct media_entity_operations hdmi_entity_ops = {
	.link_setup = hdmi_link_setup,
};

static int hdmi_register_entity(struct hdmi_device *hdev)
{
	struct v4l2_subdev *sd = &hdev->sd;
	struct v4l2_device *v4l2_dev;
	struct media_pad *pads = &hdev->pad;
	struct media_entity *me = &sd->entity;
	struct device *dev = hdev->dev;
	struct exynos_md *md;
	int ret;

	dev_dbg(dev, "HDMI entity init\n");

	/* init hdmi subdev */
	v4l2_subdev_init(sd, &hdmi_sd_ops);
	sd->owner = THIS_MODULE;
	strlcpy(sd->name, "s5p-hdmi", sizeof(sd->name));

	dev_set_drvdata(dev, sd);

	/* init hdmi sub-device as entity */
	pads[HDMI_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	me->ops = &hdmi_entity_ops;
	ret = media_entity_init(me, HDMI_PADS_NUM, pads, 0);
	if (ret) {
		dev_err(dev, "failed to initialize media entity\n");
		return ret;
	}

	/* get output media ptr for registering hdmi's sd */
	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		dev_err(dev, "failed to get output media device\n");
		return -ENODEV;
	}

	v4l2_dev = &md->v4l2_dev;

	/* regiser HDMI subdev as entity to v4l2_dev pointer of
	 * output media device
	 */
	ret = v4l2_device_register_subdev(v4l2_dev, sd);
	if (ret) {
		dev_err(dev, "failed to register HDMI subdev\n");
		return ret;
	}

	return 0;
}

static void hdmi_entity_info_print(struct hdmi_device *hdev)
{
	struct v4l2_subdev *sd = &hdev->sd;
	struct media_entity *me = &sd->entity;

	dev_dbg(hdev->dev, "\n************* HDMI entity info **************\n");
	dev_dbg(hdev->dev, "[SUB DEVICE INFO]\n");
	entity_info_print(me, hdev->dev);
	dev_dbg(hdev->dev, "*********************************************\n\n");
}

irqreturn_t hdmi_irq_handler_ext(int irq, void *dev_data)
{
	struct hdmi_device *hdev = dev_data;
	queue_delayed_work(system_nrt_wq, &hdev->hpd_work_ext, 0);

	return IRQ_HANDLED;
}

static void hdmi_hpd_changed(struct hdmi_device *hdev, int state)
{
	int ret;

	if (state == switch_get_state(&hdev->hpd_switch))
		return;

	if (state) {
		ret = edid_update(hdev);
		if (ret == -ENODEV) {
			dev_err(hdev->dev, "failed to update edid\n");
			return;
		}

		hdev->dvi_mode = !edid_supports_hdmi(hdev);
		hdev->cur_timings = edid_preferred_preset(hdev);
		hdev->cur_conf = hdmi_timing2conf(&hdev->cur_timings);
	}

	switch_set_state(&hdev->hpd_switch, state);

	dev_info(hdev->dev, "%s\n", state ? "plugged" : "unplugged");
}

static void hdmi_hpd_work_ext(struct work_struct *work)
{
	int state = 0;
	struct hdmi_device *hdev = container_of(work, struct hdmi_device,
						hpd_work_ext.work);

	state = gpio_get_value(hdev->res.gpio_hpd);
	hdmi_hpd_changed(hdev, state);
}

static void hdmi_hpd_work(struct work_struct *work)
{
	struct hdmi_device *hdev = container_of(work, struct hdmi_device,
						hpd_work);

	hdmi_hpd_changed(hdev, 0);
}

static int hdmi_set_gpio(struct hdmi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hdmi_resources *res = &hdev->res;
	int ret = 0;

	if (of_get_property(dev->of_node, "gpios", NULL) != NULL) {
		/* HPD */
		res->gpio_hpd = of_get_gpio(dev->of_node, 0);
		if (res->gpio_hpd < 0) {
			dev_err(dev, "failed to get gpio hpd\n");
			return -ENODEV;
		}
		if (gpio_request(res->gpio_hpd, "hdmi-hpd")) {
			dev_err(dev, "failed to request hdmi-hpd\n");
			ret = -ENODEV;
		} else {
			gpio_direction_input(res->gpio_hpd);
			s5p_v4l2_int_src_ext_hpd(hdev);
			dev_info(dev, "success request GPIO for hdmi-hpd\n");
		}
		/* Level shifter */
		res->gpio_ls = of_get_gpio(dev->of_node, 1);
		if (res->gpio_ls < 0) {
			dev_err(dev, "failed to get gpio ls\n");
			return -ENODEV;
		}
		if (gpio_request(res->gpio_ls, "hdmi-ls")) {
			dev_err(dev, "failed to request hdmi-ls\n");
			ret = -ENODEV;
		} else {
			gpio_direction_output(res->gpio_ls, 1);
			gpio_set_value(res->gpio_ls, 1);
			dev_info(dev, "success request GPIO for hdmi-ls\n");
		}
		/* DDC */
		res->gpio_dcdc = of_get_gpio(dev->of_node, 2);
		if (res->gpio_dcdc < 0) {
			dev_err(dev, "failed to get gpio dcdc\n");
			return -ENODEV;
		}
		if (gpio_request(res->gpio_dcdc, "hdmi-dcdc")) {
			dev_err(dev, "failed to request hdmi-dcdc\n");
			ret = -ENODEV;
		} else {
			gpio_direction_output(res->gpio_dcdc, 1);
			gpio_set_value(res->gpio_dcdc, 1);
			dev_info(dev, "success request GPIO for hdmi-dcdc\n");
		}
	}

	return ret;
}

static int hdmi_probe(struct platform_device *pdev)
{
	struct s5p_hdmi_platdata *pdata = NULL;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct i2c_adapter *phy_adapter;
	struct hdmi_device *hdmi_dev = NULL;
	struct hdmi_driver_data *drv_data;
	int ret;

	dev_info(dev, "probe start\n");

	hdmi_dev = devm_kzalloc(&pdev->dev, sizeof(struct hdmi_device), GFP_KERNEL);
	if (!hdmi_dev) {
		dev_err(&pdev->dev, "no memory for hdmi device\n");
		return -ENOMEM;
	}
	hdmi_dev->dev = dev;

	hdmi_dev->pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!hdmi_dev->pdata) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	/* store platform data ptr to mixer context */
	if (dev->of_node) {
		of_property_read_u32(dev->of_node, "ip_ver", &hdmi_dev->pdata->ip_ver);
		pdata = hdmi_dev->pdata;
	} else {
		hdmi_dev->pdata = dev->platform_data;
		pdata = hdmi_dev->pdata;
	}
	dev_info(dev, "HDMI ip version %d\n", pdata->ip_ver);

	/* mapping HDMI registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "get hdmi memory resource failed.\n");
		return -ENXIO;
	}
	hdmi_dev->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (hdmi_dev->regs == NULL) {
		dev_err(dev, "failed to claim register region for hdmi\n");
		return -ENOENT;
	}

	/* mapping HDMIPHY_APB registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_err(dev, "get hdmiphy memory resource failed.\n");
		return -ENXIO;
	}
	hdmi_dev->phy_regs = devm_request_and_ioremap(&pdev->dev, res);
	if (hdmi_dev->phy_regs == NULL) {
		dev_err(dev, "failed to claim register region for hdmiphy\n");
		return -ENOENT;
	}

	/* Internal hpd */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "get internal interrupt resource failed.\n");
		return -ENXIO;
	}
	ret = devm_request_irq(dev, res->start, hdmi_irq_handler,
			0, "hdmi-int", hdmi_dev);
	if (ret) {
		dev_err(dev, "request int interrupt failed.\n");
		return ret;
	} else {
		hdmi_dev->int_irq = res->start;
		disable_irq(hdmi_dev->int_irq);
		dev_info(dev, "success request hdmi-int irq\n");
	}

	/* setting v4l2 name to prevent WARN_ON in v4l2_device_register */
	strlcpy(hdmi_dev->v4l2_dev.name, dev_name(dev),
		sizeof(hdmi_dev->v4l2_dev.name));
	/* passing NULL owner prevents driver from erasing drvdata */
	ret = v4l2_device_register(NULL, &hdmi_dev->v4l2_dev);
	if (ret) {
		dev_err(dev, "could not register v4l2 device.\n");
		goto fail;
	}

	INIT_WORK(&hdmi_dev->hpd_work, hdmi_hpd_work);
	INIT_DELAYED_WORK(&hdmi_dev->hpd_work_ext, hdmi_hpd_work_ext);

	/* setting the clocks */
	ret = hdmi_resources_init(hdmi_dev);
	if (ret)
		goto fail_vdev;

	/* setting the GPIO */
	ret = hdmi_set_gpio(hdmi_dev);
	if (ret) {
		dev_err(dev, "failed to get GPIO\n");
		goto fail_clk;
	}

	/* External hpd */
	hdmi_dev->ext_irq = gpio_to_irq(hdmi_dev->res.gpio_hpd);
	ret = devm_request_irq(dev, hdmi_dev->ext_irq, hdmi_irq_handler_ext,
			IRQ_TYPE_EDGE_BOTH, "hdmi-ext", hdmi_dev);
	if (ret) {
		dev_err(dev, "request ext interrupt failed.\n");
		goto fail_gpio;
	} else {
		dev_info(dev, "success request hdmi-ext irq\n");
	}

	hdmi_dev->hpd_switch.name = "hdmi";
	ret = switch_dev_register(&hdmi_dev->hpd_switch);
	if (ret) {
		dev_err(dev, "request switch class failed.\n");
		goto fail_gpio;
	}

	mutex_init(&hdmi_dev->mutex);

	if (soc_is_exynos5250()) {
		drv_data = (struct hdmi_driver_data *)
			platform_get_device_id(pdev)->driver_data;
		dev_info(dev, "hdmiphy i2c bus number = %d\n", drv_data->hdmiphy_bus);

		phy_adapter = i2c_get_adapter(drv_data->hdmiphy_bus);
		if (phy_adapter == NULL) {
			dev_err(dev, "adapter request failed\n");
			ret = -ENXIO;
			goto fail_switch;
		}

		hdmi_dev->phy_sd = v4l2_i2c_new_subdev_board(&hdmi_dev->v4l2_dev,
				phy_adapter, &hdmiphy_info, NULL);
		/* on failure or not adapter is no longer useful */
		i2c_put_adapter(phy_adapter);
		if (hdmi_dev->phy_sd == NULL) {
			dev_err(dev, "missing subdev for hdmiphy\n");
			ret = -ENODEV;
			goto fail_switch;
		}
	}

	hdmi_dev->cur_timings =
		hdmi_conf[HDMI_DEFAULT_TIMINGS_IDX].dv_timings;
	/* FIXME: missing fail preset is not supported */
	hdmi_dev->cur_conf = hdmi_conf[HDMI_DEFAULT_TIMINGS_IDX].conf;

	/* default audio configuration : disable audio */
	hdmi_dev->audio_enable = 0;
	hdmi_dev->audio_channel_count = 2;
	hdmi_dev->sample_rate = DEFAULT_SAMPLE_RATE;
	hdmi_dev->color_range = HDMI_RGB709_0_255;
	hdmi_dev->bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
	hdmi_dev->audio_codec = DEFAULT_AUDIO_CODEC;

	/* default aspect ratio is 16:9 */
	hdmi_dev->aspect = HDMI_ASPECT_RATIO_16_9;

	/* default HDMI streaming is stoped */
	hdmi_dev->streaming = HDMI_STOP;

	/* register hdmi subdev as entity */
	ret = hdmi_register_entity(hdmi_dev);
	if (ret)
		goto fail_switch;

	hdmi_entity_info_print(hdmi_dev);

	pm_runtime_enable(dev);

	/* initialize hdcp resource */
	ret = hdcp_prepare(hdmi_dev);
	if (ret)
		goto fail_switch;

	/* work after booting */
	queue_delayed_work(system_nrt_wq, &hdmi_dev->hpd_work_ext,
					msecs_to_jiffies(1500));
	/* TODO : Check the PHY power off is implemented at pm_domains
	 * If not, PHY power off should be applied at here */

	dev_info(dev, "probe sucessful\n");

	hdmi_debugfs_init(hdmi_dev);

	return 0;

fail_switch:
	switch_dev_unregister(&hdmi_dev->hpd_switch);
	mutex_destroy(&hdmi_dev->mutex);

fail_gpio:
	gpio_free(hdmi_dev->res.gpio_hpd);
	gpio_free(hdmi_dev->res.gpio_ls);
	gpio_free(hdmi_dev->res.gpio_dcdc);

fail_clk:
	hdmi_resources_cleanup(hdmi_dev);

fail_vdev:
	v4l2_device_unregister(&hdmi_dev->v4l2_dev);

fail:
	dev_err(dev, "probe failed\n");
	return ret;
}

static int hdmi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct hdmi_device *hdmi_dev = sd_to_hdmi_dev(sd);

	pm_runtime_disable(dev);
	v4l2_device_unregister(&hdmi_dev->v4l2_dev);
	free_irq(hdmi_dev->ext_irq, hdmi_dev);
	free_irq(hdmi_dev->int_irq, hdmi_dev);
	switch_dev_unregister(&hdmi_dev->hpd_switch);
	mutex_destroy(&hdmi_dev->mutex);
	iounmap(hdmi_dev->regs);
	hdmi_resources_cleanup(hdmi_dev);
	flush_workqueue(hdmi_dev->hdcp_wq);
	destroy_workqueue(hdmi_dev->hdcp_wq);
	kfree(hdmi_dev);
	dev_info(dev, "remove sucessful\n");

	return 0;
}

static struct platform_driver hdmi_driver __refdata = {
	.probe = hdmi_probe,
	.remove = hdmi_remove,
	.id_table = hdmi_driver_types,
	.driver = {
		.name = "s5p-hdmi",
		.owner = THIS_MODULE,
		.pm = &hdmi_pm_ops,
		.of_match_table = of_match_ptr(hdmi_device_table),
	}
};

/* D R I V E R   I N I T I A L I Z A T I O N */

static int __init hdmi_init(void)
{
	int ret;
	static const char banner[] __initdata = KERN_INFO \
		"Samsung HDMI output driver, "
		"(c) 2010-2011 Samsung Electronics Co., Ltd.\n";
	printk(banner);

	ret = platform_driver_register(&hdmi_driver);
	if (ret)
		printk(KERN_ERR "HDMI platform driver register failed\n");

	return ret;
}
module_init(hdmi_init);

static void __exit hdmi_exit(void)
{
	platform_driver_unregister(&hdmi_driver);
}
module_exit(hdmi_exit);

MODULE_DESCRIPTION("Samsung EXYNOS5 Soc series HDMI driver");
MODULE_LICENSE("GPL");
