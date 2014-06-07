/* sound/soc/samsung/fdma.c
 *
 * ALSA SoC Audio Layer - Samsung Fake DMA driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "fdma.h"

/* #define USE_SLEEP */
/* #define USE_TX_RECT_WAVE */
/* #define USE_RX_RECT_WAVE */

#define ST_RUNNING		(1<<0)
#define ST_OPENED		(1<<1)

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
	.buffer_bytes_max	= 128*1024,
	.period_bytes_min	= 128,
	.period_bytes_max	= 64*1024,
	.periods_min		= 2,
	.periods_max		= 128,
	.fifo_size		= 32,
};

struct runtime_data {
	spinlock_t lock;
	volatile int state;
	unsigned int dma_loaded;
	unsigned int dma_period;
	dma_addr_t dma_start;
	dma_addr_t dma_pos;
	dma_addr_t dma_end;
	struct snd_soc_dai *cpu_dai;
	u32 *dma_buf;
	struct task_struct *thread_id;
	char thread_name[128];
	struct samsung_fdma_cpu_ops *cpu_ops;
};

static struct samsung_fdma_cpu_ops *fdma_cpu_ops;

/* check_fdma_status
 *
 * FDMA status is checked for AP Power mode.
 * return 1 : FDMA use dram area and it is running.
 * return 0 : FDMA has a fine condition to enter Low Power Mode.
 */
int check_fdma_status(void)
{
	return atomic_read(&dram_usage_cnt) ? 1 : 0;
}

#if defined(USE_TX_RECT_WAVE) || defined(USE_RX_RECT_WAVE)
static u32 rect_wave[8] = {
	0x60006000, 0x60006000, 0x60006000, 0x60006000, 0, 0, 0, 0
};
#endif
static int dma_kthread_tx(void *arg)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)arg;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	struct samsung_fdma_cpu_ops *ops = prtd->cpu_ops;
	u32 val, fifo, pos;

	while (!kthread_should_stop()) {
		if (prtd->state & ST_RUNNING) {
			fifo = ops->get_fifo_cnt(substream, prtd->cpu_dai);
#ifdef USE_SLEEP
			if (fifo >= (64 - 8)) {
				usleep_range(20, 100);
				fifo = ops->get_fifo_cnt(substream,
							prtd->cpu_dai);
			}
#endif
			if (fifo < (64 - 4)) {
				pos = (prtd->dma_pos - prtd->dma_start) >> 2;
#ifdef USE_TX_RECT_WAVE
				val = rect_wave[pos & 0x07];
#else
				val = *(prtd->dma_buf + pos);
#endif
				ops->write_fifo(substream, prtd->cpu_dai, val);

				prtd->dma_pos += 4;
				if (prtd->dma_pos == prtd->dma_end)
					prtd->dma_pos = prtd->dma_start;

				pos = prtd->dma_pos - prtd->dma_start;
				if ((pos % prtd->dma_period) == 0)
					snd_pcm_period_elapsed(substream);
			}
		}
	}

	return 0;
}

static int dma_kthread_rx(void *arg)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)arg;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	struct samsung_fdma_cpu_ops *ops = prtd->cpu_ops;
	u32 val, fifo, pos;

	while (!kthread_should_stop()) {
		if (prtd->state & ST_RUNNING) {
			fifo = ops->get_fifo_cnt(substream, prtd->cpu_dai);
#ifdef USE_SLEEP
			if (fifo <= 8) {
				usleep_range(20, 100);
				fifo = ops->get_fifo_cnt(substream,
							prtd->cpu_dai);
			}
#endif
			if (fifo) {
				pos = (prtd->dma_pos - prtd->dma_start) >> 2;
#ifdef USE_RX_RECT_WAVE
				val = rect_wave[pos & 0x07];
#else
				val = ops->read_fifo(substream, prtd->cpu_dai);
#endif
				*(prtd->dma_buf + pos) = val;

				prtd->dma_pos += 4;
				if (prtd->dma_pos == prtd->dma_end)
					prtd->dma_pos = prtd->dma_start;

				pos = prtd->dma_pos - prtd->dma_start;
				if ((pos % prtd->dma_period) == 0)
					snd_pcm_period_elapsed(substream);
			}
		}
	}

	return 0;
}

static int dma_kthread_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct runtime_data *prtd = runtime->private_data;
	int (*thread_fn)(void *data);

	pr_info("%s\n", __func__);

	prtd->cpu_dai = rtd->cpu_dai;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		thread_fn = dma_kthread_tx;
		snprintf(prtd->thread_name, 128, "%s: Tx",
						&prtd->cpu_dai->name[8]);
	} else {
		thread_fn = dma_kthread_rx;
		snprintf(prtd->thread_name, 128, "%s: Rx",
						&prtd->cpu_dai->name[8]);
	}

	prtd->thread_id = (struct task_struct *)kthread_run(thread_fn,
					substream, prtd->thread_name);

	return 0;
}

static void dma_kthread_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	if (prtd->thread_id) {
		kthread_stop(prtd->thread_id);
		prtd->thread_id = NULL;
	}

	pr_info("%s\n", __func__);
}

static void dma_enqueue(struct snd_pcm_substream *substream)
{
	pr_debug("Entered %s\n", __func__);
}

static int dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;
	unsigned long totbytes = params_buffer_bytes(params);

	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	runtime->dma_bytes = totbytes;

	spin_lock_irq(&prtd->lock);
	prtd->dma_loaded = 0;
	prtd->dma_period = params_period_bytes(params);
	prtd->dma_start = runtime->dma_addr;
	prtd->dma_pos = prtd->dma_start;
	prtd->dma_end = prtd->dma_start + totbytes;
	prtd->dma_buf = (u32 *)(runtime->dma_area);
	spin_unlock_irq(&prtd->lock);

	pr_info("FDMA:%s:DmaAddr=@%x Total=%d PrdSz=%d #Prds=%d area=0x%x\n",
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? "P" : "C",
		prtd->dma_start, runtime->dma_bytes,
		params_period_bytes(params), params_periods(params),
		(unsigned int)runtime->dma_area);

	return 0;
}

static int dma_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("Entered %s\n", __func__);

	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int dma_prepare(struct snd_pcm_substream *substream)
{
	struct runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

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
		atomic_inc(&dram_usage_cnt);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		prtd->state &= ~ST_RUNNING;
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
	struct runtime_data *prtd;

	pr_debug("Entered %s\n", __func__);

	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	snd_soc_set_runtime_hwparams(substream, &dma_hardware);

	prtd = kzalloc(sizeof(struct runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;
	prtd->cpu_ops = fdma_cpu_ops;

	dma_kthread_open(substream);

	return 0;
}

static int dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct runtime_data *prtd = runtime->private_data;

	pr_debug("Entered %s\n", __func__);

	dma_kthread_close(substream);

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
	buf->area = kmalloc(size, GFP_KERNEL);
	buf->addr = virt_to_phys(buf->area);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static void dma_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	pr_debug("Entered %s\n", __func__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		kfree(buf->area);
		buf->area = NULL;
	}
}

static u64 dma_mask = DMA_BIT_MASK(32);

static int dma_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret = 0;

	pr_debug("Entered %s\n", __func__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
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

int asoc_fdma_platform_register(struct device *dev,
					struct samsung_fdma_cpu_ops *ops)
{
	fdma_cpu_ops = ops;
	return snd_soc_register_platform(dev, &samsung_asoc_platform);
}
EXPORT_SYMBOL_GPL(asoc_fdma_platform_register);

void asoc_fdma_platform_unregister(struct device *dev)
{
	snd_soc_unregister_platform(dev);
}
EXPORT_SYMBOL_GPL(asoc_fdma_platform_unregister);

MODULE_AUTHOR("Yeongman Seo, <yman.seo@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC FakeDMA Driver");
MODULE_LICENSE("GPL");
