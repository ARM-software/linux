/*
 * ALSA SoC TDA998X CODEC
 *
 * Copyright (C) 2014 Jean-Francois Moine
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <drm/drm_encoder_slave.h>
#include <drm/i2c/tda998x.h>

#include "tda998x_drv.h"

#define TDA998X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

static int tda_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct tda998x_priv *priv = snd_soc_codec_get_drvdata(dai->codec);
	u8 *eld = priv->eld;
	struct snd_pcm_runtime *runtime = substream->runtime;
	u8 *sad;
	int sad_count;
	unsigned eld_ver, mnl, rate_mask;
	unsigned max_channels, fmt;
	u64 formats;
	struct snd_pcm_hw_constraint_list *rate_constraints =
			&priv->rate_constraints;
	static const u32 hdmi_rates[] = {
		32000, 44100, 48000, 88200, 96000, 176400, 192000
	};

	/* check if streaming is already active */
	if (priv->dai_id != AFMT_NO_AUDIO)
		return -EBUSY;
	priv->dai_id = dai->id;

	if (!eld)
		return 0;

	/* adjust the hw params from the ELD (EDID) */
	eld_ver = eld[0] >> 3;
	if (eld_ver != 2 && eld_ver != 31)
		return 0;

	mnl = eld[4] & 0x1f;
	if (mnl > 16)
		return 0;

	sad_count = eld[5] >> 4;
	sad = eld + 20 + mnl;

	/* Start from the basic audio settings */
	max_channels = 2;
	rate_mask = 0;
	fmt = 0;
	while (sad_count--) {
		switch (sad[0] & 0x78) {
		case 0x08: /* PCM */
			max_channels = max(max_channels, (sad[0] & 7) + 1u);
			rate_mask |= sad[1];
			fmt |= sad[2] & 0x07;
			break;
		}
		sad += 3;
	}

	/* set the constraints */
	rate_constraints->list = hdmi_rates;
	rate_constraints->count = ARRAY_SIZE(hdmi_rates);
	rate_constraints->mask = rate_mask;
	snd_pcm_hw_constraint_list(runtime, 0,
					SNDRV_PCM_HW_PARAM_RATE,
					rate_constraints);

	formats = 0;
	if (fmt & 1)
		formats |= SNDRV_PCM_FMTBIT_S16_LE;
	if (fmt & 2)
		formats |= SNDRV_PCM_FMTBIT_S20_3LE;
	if (fmt & 4)
		formats |= SNDRV_PCM_FMTBIT_S24_LE;
	snd_pcm_hw_constraint_mask64(runtime,
				SNDRV_PCM_HW_PARAM_FORMAT,
				formats);

	snd_pcm_hw_constraint_minmax(runtime,
				SNDRV_PCM_HW_PARAM_CHANNELS,
				1, max_channels);
	return 0;
}

static int tda_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct tda998x_priv *priv = snd_soc_codec_get_drvdata(dai->codec);

	/* Requires an attached display */
	if (!priv->encoder->crtc)
		return -ENODEV;

	/* if same input and same parameters, do not do a full switch */
	if (dai->id == priv->params.audio_format &&
	    params_format(params) == priv->audio_sample_format) {
		tda998x_audio_start(priv, 0);
		return 0;
	}
	priv->params.audio_sample_rate = params_rate(params);
	priv->params.audio_format = dai->id;
	priv->audio_sample_format = params_format(params);
	priv->params.audio_cfg =
		priv->audio_ports[dai->id == AFMT_I2S ? 0 : 1];
	tda998x_audio_start(priv, 1);
	return 0;
}

static void tda_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct tda998x_priv *priv = snd_soc_codec_get_drvdata(dai->codec);

	tda998x_audio_stop(priv);
	priv->dai_id = AFMT_NO_AUDIO;
}

static const struct snd_soc_dai_ops tda_ops = {
	.startup = tda_startup,
	.hw_params = tda_hw_params,
	.shutdown = tda_shutdown,
};

static struct snd_soc_dai_driver tda998x_dai[] = {
	{
		.name = "i2s-hifi",
		.id = AFMT_I2S,
		.playback = {
			.stream_name	= "HDMI I2S Playback",
			.channels_min	= 1,
			.channels_max	= 8,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 5512,
			.rate_max	= 192000,
			.formats	= TDA998X_FORMATS,
		},
		.ops = &tda_ops,
	},
	{
		.name = "spdif-hifi",
		.id = AFMT_SPDIF,
		.playback = {
			.stream_name	= "HDMI SPDIF Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min	= 22050,
			.rate_max	= 192000,
			.formats	= TDA998X_FORMATS,
		},
		.ops = &tda_ops,
	},
};

static const struct snd_soc_dapm_widget tda_widgets[] = {
	SND_SOC_DAPM_OUTPUT("hdmi-out"),
};
static const struct snd_soc_dapm_route tda_routes[] = {
	{ "hdmi-out", NULL, "HDMI I2S Playback" },
	{ "hdmi-out", NULL, "HDMI SPDIF Playback" },
};

static int tda_probe(struct snd_soc_codec *codec)
{
	struct i2c_client *i2c_client = to_i2c_client(codec->dev);
	struct tda998x_priv *priv = i2c_get_clientdata(i2c_client);
	struct device_node *np = codec->dev->of_node;
	int i, j, ret;
	const char *p;

	if (!priv)
		return -ENODEV;
	snd_soc_codec_set_drvdata(codec, priv);

	if (!np)
		return 0;

	/* get the audio input ports*/
	for (i = 0; i < 2; i++) {
		u32 port;

		ret = of_property_read_u32_index(np, "audio-ports", i, &port);
		if (ret) {
			if (i == 0)
				dev_err(codec->dev,
					"bad or missing audio-ports\n");
			break;
		}
		ret = of_property_read_string_index(np, "audio-port-names",
						i, &p);
		if (ret) {
			dev_err(codec->dev,
				"missing audio-port-names[%d]\n", i);
			break;
		}
		if (strcmp(p, "i2s") == 0) {
			j = 0;
		} else if (strcmp(p, "spdif") == 0) {
			j = 1;
		} else {
			dev_err(codec->dev,
				"bad audio-port-names '%s'\n", p);
			break;
		}
		priv->audio_ports[j] = port;
	}
	return 0;
}

static const struct snd_soc_codec_driver soc_codec_tda998x = {
	.probe = tda_probe,
	.dapm_widgets = tda_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tda_widgets),
	.dapm_routes = tda_routes,
	.num_dapm_routes = ARRAY_SIZE(tda_routes),
};

int tda998x_codec_register(struct device *dev)
{
	return snd_soc_register_codec(dev,
				&soc_codec_tda998x,
				tda998x_dai, ARRAY_SIZE(tda998x_dai));
}

void tda998x_codec_unregister(struct device *dev)
{
	snd_soc_unregister_codec(dev);
}
