/*
 *  smdk5422_wm8994.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include "../codecs/wm8994.h"
#include <sound/pcm_params.h>
#include <linux/module.h>
#include <linux/of.h>

#include "i2s.h"
#include "i2s-regs.h"
#include "spdif.h"

 /*
  * Default CFG switch settings to use this driver:
  *	SMDKV310: CFG5-1000, CFG7-111111
  */

 /*
  * Configure audio route as :-
  * $ amixer sset 'DAC1' on,on
  * $ amixer sset 'Right Headphone Mux' 'DAC'
  * $ amixer sset 'Left Headphone Mux' 'DAC'
  * $ amixer sset 'DAC1R Mixer AIF1.1' on
  * $ amixer sset 'DAC1L Mixer AIF1.1' on
  * $ amixer sset 'IN2L' on
  * $ amixer sset 'IN2L PGA IN2LN' on
  * $ amixer sset 'MIXINL IN2L' on
  * $ amixer sset 'AIF1ADC1L Mixer ADC/DMIC' on
  * $ amixer sset 'IN2R' on
  * $ amixer sset 'IN2R PGA IN2RN' on
  * $ amixer sset 'MIXINR IN2R' on
  * $ amixer sset 'AIF1ADC1R Mixer ADC/DMIC' on
  */

/* SMDK has a 16.934MHZ crystal attached to WM8994 */
#define SMDK_WM8994_FREQ 16934000

static int smdk_codec_fll_enable(struct snd_soc_dai *codec_dai,
				 unsigned int pll_out)
{
	static bool fll_enabled = false;
	bool on = pll_out ? true : false;
	int ret;

	if (fll_enabled == on)
		return 0;

	pr_debug("%s: %s\n", __func__, on ? "ON" : "OFF");

	ret = snd_soc_dai_set_pll(codec_dai, WM8994_FLL1, WM8994_FLL_SRC_MCLK1,
					SMDK_WM8994_FREQ, pll_out);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(codec_dai, WM8994_SYSCLK_FLL1,
					pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	fll_enabled = on;

	return 0;
}

static int smdk_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int pll_out;
	int ret;

	/* Set Codec DAI configuration */
	/* AIF1CLK should be >=3MHz for optimal performance */
	if (params_format(params) == SNDRV_PCM_FORMAT_S24_LE)
		pll_out = params_rate(params) * 384;
	else if (params_rate(params) == 8000 || params_rate(params) == 11025)
		pll_out = params_rate(params) * 512;
	else
		pll_out = params_rate(params) * 256;

	ret = smdk_codec_fll_enable(codec_dai, pll_out);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set CPU DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_OPCLK,
					0, MOD_OPCLK_PCLK);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * SMDK WM8994 DAI operations.
 */
static struct snd_soc_ops smdk_ops = {
	.hw_params = smdk_hw_params,
};

static int smdk_wm8994_init_paiftx(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* HeadPhone */
	snd_soc_dapm_enable_pin(dapm, "HPOUT1R");
	snd_soc_dapm_enable_pin(dapm, "HPOUT1L");

	/* MicIn */
	snd_soc_dapm_enable_pin(dapm, "IN1LN");
	snd_soc_dapm_enable_pin(dapm, "IN1RN");

	/* LineIn */
	snd_soc_dapm_enable_pin(dapm, "IN2LN");
	snd_soc_dapm_enable_pin(dapm, "IN2RN");

	/* Other pins NC */
	snd_soc_dapm_nc_pin(dapm, "HPOUT2P");
	snd_soc_dapm_nc_pin(dapm, "HPOUT2N");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTLN");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTLP");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRP");
	snd_soc_dapm_nc_pin(dapm, "SPKOUTRN");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT1P");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2N");
	snd_soc_dapm_nc_pin(dapm, "LINEOUT2P");
	snd_soc_dapm_nc_pin(dapm, "IN1LP");
	snd_soc_dapm_nc_pin(dapm, "IN2LP:VXRN");
	snd_soc_dapm_nc_pin(dapm, "IN1RP");
	snd_soc_dapm_nc_pin(dapm, "IN2RP:VXRP");

	return 0;
}

static int smdk_set_bias_level_post(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	struct snd_soc_dai *aif1_dai = card->rtd[0].codec_dai;

	if (dapm->dev != aif1_dai->dev)
		return 0;

	if ((level == SND_SOC_BIAS_OFF) && !aif1_dai->active) {
		pr_debug("%s: SND_SOC_BIAS_OFF\n", __func__);
		smdk_codec_fll_enable(aif1_dai, 0);
	}

	return 0;
}

static struct snd_soc_dai_link smdk_dai[] = {
	{ /* Primary DAI i/f */
		.name = "WM8994 PRI",
		.stream_name = "i2s0-pri",
		.codec_name = "wm8994-codec",
		.codec_dai_name = "wm8994-aif1",
		.init = smdk_wm8994_init_paiftx,
		.ops = &smdk_ops,
	}, { /* Secondary DAI i/f */
		.name = "WM8994 SEC",
		.stream_name = "i2s0-sec",
		.cpu_dai_name = "samsung-i2s-sec",
		.platform_name = "samsung-i2s-sec",
		.codec_name = "wm8994-codec",
		.codec_dai_name = "wm8994-aif1",
		.ops = &smdk_ops,
#ifdef CONFIG_SND_SAMSUNG_AUX_SPDIF
	}, { /* Aux DAI i/f */
		.name = "S/PDIF",
		.stream_name = "spdif",
		.codec_dai_name = "dummy-aif2",
		.ops = &smdk_spdif_ops,
#endif
	}
};

static struct snd_soc_card smdk = {
	.name = "SMDK-I2S",
	.owner = THIS_MODULE,
	.dai_link = smdk_dai,
	.num_links = ARRAY_SIZE(smdk_dai),
	.set_bias_level_post = smdk_set_bias_level_post,
};

static int smdk_audio_probe(struct platform_device *pdev)
{
	int n, ret;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &smdk;
	bool hdmi_avail = false;
	bool spdif_avail = false;

#ifdef CONFIG_SND_SAMSUNG_AUX_HDMI
	hdmi_avail = true;
#endif
#ifdef CONFIG_SND_SAMSUNG_AUX_SPDIF
	spdif_avail = true;
#endif
	card->dev = &pdev->dev;

	for (n = 0; np && n < ARRAY_SIZE(smdk_dai); n++) {
		if (!smdk_dai[n].cpu_dai_name) {
			smdk_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu", n);

			if (!smdk_dai[n].cpu_of_node && hdmi_avail) {
				smdk_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu-hdmi", 0);
				hdmi_avail = false;
			}

			if (!smdk_dai[n].cpu_of_node && spdif_avail) {
				smdk_dai[n].cpu_of_node = of_parse_phandle(np,
					"samsung,audio-cpu-spdif", 0);
				spdif_avail = false;
			}

			if (!smdk_dai[n].cpu_of_node) {
				dev_err(&pdev->dev, "Property "
				"'samsung,audio-cpu' missing or invalid\n");
				ret = -EINVAL;
			}
		}

		if (!smdk_dai[n].platform_name)
			smdk_dai[n].platform_of_node = smdk_dai[n].cpu_of_node;

		if (!smdk_dai[n].codec_name) {
			smdk_dai[n].codec_of_node = of_parse_phandle(np,
				"samsung,audio-codec", n);
			if (!smdk_dai[0].codec_of_node) {
				dev_err(&pdev->dev, "Property "
				"'samsung,audio-codec' missing or invalid\n");
				ret = -EINVAL;
			}
		}
	}

	ret = snd_soc_register_card(card);

	if (ret)
		dev_err(&pdev->dev, "snd_soc_register_card() failed:%d\n", ret);

	return ret;
}

static int smdk_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id samsung_wm8994_of_match[] = {
	{ .compatible = "samsung,smdk-wm8994", },
	{},
};
MODULE_DEVICE_TABLE(of, samsung_wm8994_of_match);
#endif /* CONFIG_OF */

static struct platform_driver smdk_audio_driver = {
	.driver		= {
		.name	= "smdk-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(samsung_wm8994_of_match),
	},
	.probe		= smdk_audio_probe,
	.remove		= smdk_audio_remove,
};

module_platform_driver(smdk_audio_driver);

MODULE_DESCRIPTION("ALSA SoC SMDK WM8994");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:smdk-audio");
