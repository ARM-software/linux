/*
 * dma.c  --  ALSA Soc Audio Layer
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * Copyright 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/iommu.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/exynos.h>

#include <asm/dma.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include <mach/map.h>

#include "dma.h"

#define ST_RUNNING		(1<<0)
#define ST_OPENED		(1<<1)

#define SRAM_END		(0x04000000)

static atomic_t dram_usage_cnt;

static const struct snd_pcm_hardware dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER |
				  SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_U16_LE |
				  SNDRV_PCM_FMTBIT_U8 |
				  SNDRV_PCM_FMTBIT_S8,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 256*1024,
	.period_bytes_min	= 128,
	.period_bytes_max	= 32*1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

struct runtime_data {
	spinlock_t lock;
	int state;
	unsigned int dma_loaded;
	unsigned int dma_period;
	dma_addr_t dma_start;
	dma_addr_t dma_pos;
	dma_addr_t dma_end;
	struct s3c_dma_params *params;
	struct snd_pcm_hardware hw;
	bool dram_used;
};

static void audio_buffdone(void *data);

/* check_adma_status
 *
 * ADMA status is checked for AP Power mode.
 * return 1 : ADMA use dram area and it is running.
 * return 0 : ADMA has a fine condition to enter Low Power Mode.
 */
int check_adma_status(void)
{
	return atomic_read(&dram_usage_cnt) ? 1 : 0;
}

/* dma_enqueue
 *
 * place a dma buffer onto the queue for the dma system
 * to handle.
 */
static void dma_enqueue(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	dma_addr_t pos = prtd->dma_pos;
	unsigned int limit;
	struct samsung_dma_prep dma_info;

	pr_debug("Entered %s\n", __func__);

	limit = (prtd->dma_end - prtd->dma_start) / prtd->dma_period;

	pr_debug("%s: loaded %d, limit %d\n",
				__func__, prtd->dma_loaded, limit);

	dma_info.cap = (samsung_dma_has_circular() ? DMA_CYCLIC : DMA_SLAVE);
	dma_info.direction =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK
		? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM);
	dma_info.fp = audio_buffdone;
	dma_info.fp_param = substream;
	dma_info.period = prtd->dma_period;
	dma_info.len = prtd->dma_period*limit;

	if (samsung_dma_has_infiniteloop()) {
		dma_info.buf = prtd->dma_pos;
		dma_info.infiniteloop = limit;
		prtd->params->ops->prepare(prtd->params->ch, &dma_info);
	} else {
		dma_info.infiniteloop = 0;
		while (prtd->dma_loaded < limit) {
			pr_debug("dma_loaded: %d\n", prtd->dma_loaded);

			if ((pos + dma_info.period) > prtd->dma_end) {
				dma_info.period  = prtd->dma_end - pos;
				pr_debug("%s: corrected dma len %ld\n",
						__func__, dma_info.period);
			}

			dma_info.buf = pos;
			prtd->params->ops->prepare(prtd->params->ch, &dma_info);

			prtd->dma_loaded++;
			pos += prtd->dma_period;
			if (pos >= prtd->dma_end)
				pos = prtd->dma_start;
		}
		prtd->dma_pos = pos;
	}
}

static void audio_buffdone(void *data)
{
	struct snd_pcm_substream *substream = data;
	struct runtime_data *prtd;

	pr_debug("Entered %s\n", __func__);

	if (!substream)
		return;

	prtd = substream->runtime->private_data;
	if (prtd->state & ST_RUNNING) {
		prtd->dma_pos += prtd->dma_period;
		if (prtd->dma_pos >= prtd->dma_end)
			prtd->dma_pos = prtd->dma_start;

		snd_pcm_period_elapsed(substream);

		spin_lock(&prtd->lock);
		if (!samsung_dma_has_circular()) {
			prtd->dma_loaded--;
			if (!samsung_dma_has_infiniteloop())
				dma_enqueue(substream);
		}
		spin_unlock(&prtd->lock);
	}
}

static int dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned long totbytes = params_buffer_bytes(params);
	struct s3c_dma_params *dma =
		snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct samsung_dma_req req;
	struct samsung_dma_config config;

	pr_debug("Entered %s\n", __func__);

	/* return if this is a bufferless transfer e.g.
	 * codec <--> BT codec or GSM modem -- lg FIXME */
	if (!dma)
		return 0;

	/* this may get called several times by oss emulation
	 * with different params -HW */
	if (prtd->params == NULL) {
		/* prepare DMA */
		prtd->params = dma;

		pr_debug("params %p, client %p, channel %d\n", prtd->params,
			prtd->params->client, prtd->params->channel);

		prtd->params->ops = samsung_dma_get_ops();

		req.cap = (samsung_dma_has_circular() ?
			DMA_CYCLIC : DMA_SLAVE);
		req.client = prtd->params->client;
		config.direction =
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM);
		config.width = prtd->params->dma_size;
		/* config.maxburst = 1; */
		config.fifo = prtd->params->dma_addr;
		prtd->params->ch = prtd->params->ops->request(
				prtd->params->channel, &req, rtd->cpu_dai->dev,
				prtd->params->ch_name);
		pr_debug("dma_request: ch %d, req %p, dev %p, ch_name [%s]\n",
			prtd->params->channel, &req, rtd->cpu_dai->dev,
			prtd->params->ch_name);
		prtd->params->ops->config(prtd->params->ch, &config);
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totbytes;

	spin_lock_irq(&prtd->lock);
	prtd->dma_loaded = 0;
	prtd->dma_period = params_period_bytes(params);
	prtd->dma_start = runtime->dma_addr;
	prtd->dma_pos = prtd->dma_start;
	prtd->dma_end = prtd->dma_start + totbytes;
	prtd->dram_used = runtime->dma_addr < SRAM_END ? false : true;
	spin_unlock_irq(&prtd->lock);

	pr_debug("ADMA:%s:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d dma_area=0x%x\n",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
		prtd->dma_start, runtime->dma_bytes,
		params_period_bytes(params), params_periods(params),
		(unsigned int)runtime->dma_area);

	return 0;
}

static int dma_hw_free(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

	if (prtd->params) {
		prtd->params->ops->flush(prtd->params->ch);
		prtd->params->ops->release(prtd->params->ch,
					prtd->params->client);
		prtd->params = NULL;
	}

	return 0;
}

static int dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	/* return if this is a bufferless transfer e.g.
	 * codec <--> BT codec or GSM modem -- lg FIXME */
	if (!prtd->params)
		return 0;

	/* flush the DMA channel */
	prtd->params->ops->flush(prtd->params->ch);
	prtd->dma_loaded = 0;
	prtd->dma_pos = prtd->dma_start;

	/* enqueue dma buffers */
	dma_enqueue(substream);

	return ret;
}

static int dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->state |= ST_RUNNING;
		prtd->params->ops->trigger(prtd->params->ch);
		if (prtd->dram_used)
			atomic_inc(&dram_usage_cnt);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		prtd->state &= ~ST_RUNNING;
		prtd->params->ops->stop(prtd->params->ch);
		if (prtd->dram_used)
			atomic_dec(&dram_usage_cnt);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long res;

	pr_debug("Entered %s\n", __func__);

	res = prtd->dma_pos - prtd->dma_start;

	pr_debug("Pointer offset: %lu\n", res);

	/* we seem to be getting the odd error from the pcm library due
	 * to out-of-bounds pointers. this is maybe due to the dma engine
	 * not having loaded the new values for the channel before being
	 * called... (todo - fix )
	 */

	if (res >= snd_pcm_lib_buffer_bytes(substream)) {
		if (res == snd_pcm_lib_buffer_bytes(substream))
			res = 0;
	}

	return bytes_to_frames(substream->runtime, res);
}

static int dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	struct runtime_data *prtd;
	size_t period_bytes;

	pr_debug("Entered %s\n", __func__);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	memcpy(&prtd->hw, &dma_hardware, sizeof(struct snd_pcm_hardware));

	prtd->hw.buffer_bytes_max = buf->bytes;
	period_bytes = buf->bytes / prtd->hw.periods_min;
	if (period_bytes < prtd->hw.period_bytes_max)
		prtd->hw.period_bytes_max = period_bytes;

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	runtime->private_data = prtd;
	snd_soc_set_runtime_hwparams(substream, &prtd->hw);

	return 0;
}

static int dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	if (!prtd)
		pr_debug("dma_close called with prtd == NULL\n");

	kfree(prtd);

	return 0;
}

static int dma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("Entered %s\n", __func__);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

static struct snd_pcm_ops dma_ops = {
	.open		= dma_open,
	.close		= dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= dma_hw_params,
	.hw_free	= dma_hw_free,
	.prepare	= dma_prepare,
	.trigger	= dma_trigger,
	.pointer	= dma_pointer,
	.mmap		= dma_mmap,
};

static int preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = dma_hardware.buffer_bytes_max;

	pr_debug("Entered %s\n", __func__);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static const char *dma_prop_addr[2] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "samsung,tx-buf",
	[SNDRV_PCM_STREAM_CAPTURE]  = "samsung,rx-buf"
};
static const char *dma_prop_size[2] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "samsung,tx-size",
	[SNDRV_PCM_STREAM_CAPTURE]  = "samsung,rx-size"
};
static const char *dma_prop_iommu[2] = {
	[SNDRV_PCM_STREAM_PLAYBACK] = "samsung,tx-iommu",
	[SNDRV_PCM_STREAM_CAPTURE]  = "samsung,rx-iommu"
};

static int preallocate_dma_buffer_of(struct snd_pcm *pcm, int stream,
					struct device_node *np)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	dma_addr_t dma_addr;
	size_t size;
#ifdef CONFIG_SND_SAMSUNG_IOMMU
	struct iommu_domain *domain = lpass_get_iommu_domain();
	dma_addr_t dma_buf_pa;
	int ret;
#endif
	pr_debug("Entered %s\n", __func__);

	if (of_property_read_u32(np, dma_prop_addr[stream], &dma_addr))
		return -ENOMEM;

	if (of_property_read_u32(np, dma_prop_size[stream], &size))
		return -ENOMEM;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->addr = dma_addr;

#ifdef CONFIG_SND_SAMSUNG_IOMMU
	if (of_find_property(np, dma_prop_iommu[stream], NULL)) {
		buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &dma_buf_pa, GFP_KERNEL);
		if (!buf->area)
			return -ENOMEM;

		ret = iommu_map(domain, dma_addr, dma_buf_pa, size, 0);
		if (ret) {
			dma_free_writecombine(pcm->card->dev, size,
						buf->area, dma_buf_pa);
			pr_err("%s: Failed to iommu_map: %d\n", __func__, ret);
			return -ENOMEM;
		}
		pr_info("%s: DmaAddr-iommu %08X dma_buf_pa %08X\n",
				__func__, dma_addr, dma_buf_pa);
	} else {
		buf->area = ioremap(buf->addr, size);
	}
#else
	if (of_find_property(np, dma_prop_iommu[stream], NULL))
		return -ENOMEM;
	else
		buf->area = ioremap(buf->addr, size);
#endif

	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static void dma_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	struct runtime_data *prtd;
	int stream;

	pr_debug("Entered %s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		prtd = substream->runtime->private_data;
		if (prtd->dram_used)
			dma_free_writecombine(pcm->card->dev, buf->bytes,
						buf->area, buf->addr);
		else
			iounmap(buf->area);

		buf->area = NULL;
	}
}

static u64 dma_mask = DMA_BIT_MASK(32);

static int dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct device_node *np = rtd->cpu_dai->dev->of_node;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = 0;
		if (np)
			ret = preallocate_dma_buffer_of(pcm,
				SNDRV_PCM_STREAM_PLAYBACK, np);
		if (ret)
			ret = preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = 0;
		if (np)
			ret = preallocate_dma_buffer_of(pcm,
				SNDRV_PCM_STREAM_CAPTURE, np);
		if (ret)
			ret = preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
out:
	return ret;
}

static struct snd_soc_platform_driver samsung_asoc_platform = {
	.ops		= &dma_ops,
	.pcm_new	= dma_new,
	.pcm_free	= dma_free_dma_buffers,
};

int asoc_dma_platform_register(struct device *dev)
{
	return snd_soc_register_platform(dev, &samsung_asoc_platform);
}
EXPORT_SYMBOL_GPL(asoc_dma_platform_register);

void asoc_dma_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(asoc_dma_platform_unregister);

MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_DESCRIPTION("Samsung ASoC DMA Driver");
MODULE_LICENSE("GPL");
