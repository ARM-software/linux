/*
 *  fdma.h --
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  ALSA PCM interface for the Samsung SoC
 */

#ifndef _SAMSUNG_AUDIO_FDMA_H
#define _SAMSUNG_AUDIO_FDMA_H

struct samsung_fdma_cpu_ops {
	int (*get_fifo_cnt)(struct snd_pcm_substream * substream,
				struct snd_soc_dai *dai);
	void (*write_fifo)(struct snd_pcm_substream * substream,
				struct snd_soc_dai *dai, u32 val);
	u32 (*read_fifo)(struct snd_pcm_substream * substream,
				struct snd_soc_dai *dai);
};

int asoc_fdma_platform_register(struct device *dev,
				struct samsung_fdma_cpu_ops *ops);
void asoc_fdma_platform_unregister(struct device *dev);

#endif
