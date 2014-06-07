/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *	Taekki Kim <taekki.kim@samsung.com>
 *
 * EXYNOS - PPMU polling support
 *	This version supports EXYNOS5422 only. This changes bus frequencies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <mach/map.h>

#include "exynos_ppmu.h"
#include "exynos5422_ppmu.h"

#define CREATE_TRACE_POINTS

#define FIXED_POINT_OFFSET 8
#define FIXED_POINT_MASK ((1 << FIXED_POINT_OFFSET) - 1)

enum exynos5_ppmu_list {
	PPMU_DMC_0_0,
	PPMU_DMC_1_0,
	PPMU_DMC_0_1,
	PPMU_DMC_1_1,
	PPMU_END,
};

static DEFINE_SPINLOCK(exynos5_ppmu_lock);
static LIST_HEAD(exynos5_ppmu_handle_list);

struct exynos5_ppmu_handle {
	struct list_head node;
	struct exynos_ppmu ppmu[PPMU_END];
};

static const char *exynos5_ppmu_name[PPMU_END] = {
	[PPMU_DMC_0_0] = "DMC_0_0",
	[PPMU_DMC_1_0] = "DMC_1_0",
	[PPMU_DMC_0_1] = "DMC_0_1",
	[PPMU_DMC_1_1] = "DMC_1_1",
};

static struct exynos_ppmu ppmu[PPMU_END] = {
	[PPMU_DMC_0_0] = {
		.hw_base = S5P_VA_PPMU_DREX0_0,
	},
	[PPMU_DMC_1_0] = {
		.hw_base = S5P_VA_PPMU_DREX1_0,
	},
	[PPMU_DMC_0_1] = {
		.hw_base = S5P_VA_PPMU_DREX0_1,
	},
	[PPMU_DMC_1_1] = {
		.hw_base = S5P_VA_PPMU_DREX1_1,
	},
};

static struct exynos5_ppmu_handle *exynos5_ppmu_trace_handle;

static void exynos5_ppmu_reset(struct exynos_ppmu *ppmu)
{
	unsigned long flags;
	void __iomem *ppmu_base = ppmu->hw_base;

	/* Reset PPMU */
	exynos_ppmu_reset(ppmu_base);

	/* Set PPMU Event */
	ppmu->event[PPMU_PMNCNT0] = RD_DATA_COUNT;
	exynos_ppmu_setevent(ppmu_base, PPMU_PMNCNT0,
			ppmu->event[PPMU_PMNCNT0]);

	ppmu->event[PPMU_PMCCNT1] = WR_DATA_COUNT;
	exynos_ppmu_setevent(ppmu_base, PPMU_PMCCNT1,
			ppmu->event[PPMU_PMCCNT1]);

	ppmu->event[PPMU_PMNCNT3] = RDWR_DATA_COUNT;
	exynos_ppmu_setevent(ppmu_base, PPMU_PMNCNT3,
			ppmu->event[PPMU_PMNCNT3]);

	local_irq_save(flags);
	ppmu->reset_time = ktime_get();
	/* Start PPMU */
	exynos_ppmu_start(ppmu_base);
	local_irq_restore(flags);
}

static void exynos5_ppmu_read(struct exynos_ppmu *ppmu)
{
	int j;
	unsigned long flags;
	ktime_t read_time;
	ktime_t t;
	u32 reg;

	void __iomem *ppmu_base = ppmu->hw_base;

	local_irq_save(flags);
	read_time = ktime_get();

	/* Stop PPMU */
	exynos_ppmu_stop(ppmu_base);
	local_irq_restore(flags);

	/* Update local data from PPMU */
	ppmu->ccnt = __raw_readl(ppmu_base + PPMU_CCNT);
	reg = __raw_readl(ppmu_base + PPMU_FLAG);
	ppmu->ccnt_overflow = reg & PPMU_CCNT_OVERFLOW;

	for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++) {
		if (ppmu->event[j] == 0)
			ppmu->count[j] = 0;
		else
			ppmu->count[j] = exynos_ppmu_read(ppmu_base, j);
	}
	t = ktime_sub(read_time, ppmu->reset_time);
	ppmu->ns = ktime_to_ns(t);
}

static void exynos5_ppmu_add(struct exynos_ppmu *to, struct exynos_ppmu *from,
			enum exynos5_ppmu_list start, enum exynos5_ppmu_list end)
{
	int i;
	int j;

	for (i = start; i <= end; i++) {
		for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++)
			to[i].count[j] += from[i].count[j];

		to[i].ccnt += from[i].ccnt;
		if (to[i].ccnt < from[i].ccnt)
			to[i].ccnt_overflow = true;

		to[i].ns += from[i].ns;

		if (from[i].ccnt_overflow)
			to[i].ccnt_overflow = true;
	}
}

static void exynos5_ppmu_handle_clear(struct exynos5_ppmu_handle *handle, int handle_size)
{
	memset(&handle->ppmu, 0, sizeof(struct exynos_ppmu) * handle_size);
}

static int exynos5_ppmu_update(enum exynos5_ppmu_list start, enum exynos5_ppmu_list end)
{
	int i, ret = 0;
	struct exynos5_ppmu_handle *handle;

	for (i = start; i <= end; i++) {
		exynos5_ppmu_read(&ppmu[i]);
		exynos5_ppmu_reset(&ppmu[i]);
	}

	list_for_each_entry(handle, &exynos5_ppmu_handle_list, node)
		exynos5_ppmu_add(handle->ppmu, ppmu, start, end);

	return ret;
}

static int exynos5_ppmu_get_filter(enum exynos_ppmu_sets filter,
	enum exynos5_ppmu_list *start, enum exynos5_ppmu_list *end)
{
	switch (filter) {
	case PPMU_SET_DDR:
		*start = PPMU_DMC_0_0;
		*end = PPMU_DMC_1_1;
		break;
	case PPMU_SET_TOP:
		*start = PPMU_DMC_0_0;
		*end = PPMU_DMC_1_0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int exynos5_ppmu_get_busy(struct exynos5_ppmu_handle *handle,
	enum exynos_ppmu_sets filter, unsigned int *ccnt,
	unsigned long *pmcnt)
{
	unsigned long flags;
	int i;
	int busy = 0;
	int temp;
	enum exynos5_ppmu_list start, end;
	int ret;
	unsigned int max_ccnt = 0, temp_ccnt = 0;
	unsigned long max_pmcnt = 0, temp_pmcnt = 0;

	ret = exynos5_ppmu_get_filter(filter, &start, &end);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&exynos5_ppmu_lock, flags);

	exynos5_ppmu_update(start, end);

	for (i = start; i <= end; i++) {
		if (handle->ppmu[i].ccnt_overflow) {
			busy = -EOVERFLOW;
			break;
		}

		temp_ccnt = handle->ppmu[i].ccnt;
		if (temp_ccnt > max_ccnt)
			max_ccnt = temp_ccnt;

		temp_pmcnt = handle->ppmu[i].count[PPMU_PMNCNT3];
		if (temp_pmcnt > max_pmcnt)
			max_pmcnt = temp_pmcnt;

		temp = handle->ppmu[i].count[PPMU_PMNCNT3] * 100;
		if (handle->ppmu[i].count > 0 && temp > 0)
			temp /= handle->ppmu[i].ccnt;

		if (temp > busy)
			busy = temp;

	}

	*ccnt = max_ccnt;
	*pmcnt = max_pmcnt;

	exynos5_ppmu_handle_clear(handle, (end - start) + 1);

	spin_unlock_irqrestore(&exynos5_ppmu_lock, flags);

	return busy;
}

void exynos5_ppmu_put(struct exynos5_ppmu_handle *handle)
{
	unsigned long flags;

	spin_lock_irqsave(&exynos5_ppmu_lock, flags);

	list_del(&handle->node);

	spin_unlock_irqrestore(&exynos5_ppmu_lock, flags);

	kfree(handle);
}

struct exynos5_ppmu_handle *exynos5_ppmu_get(void)
{
	struct exynos5_ppmu_handle *handle;
	unsigned long flags;

	handle = kzalloc(sizeof(struct exynos5_ppmu_handle), GFP_KERNEL);
	if (!handle)
		return NULL;

	spin_lock_irqsave(&exynos5_ppmu_lock, flags);

	exynos5_ppmu_update(0, PPMU_END - 1);
	list_add_tail(&handle->node, &exynos5_ppmu_handle_list);

	spin_unlock_irqrestore(&exynos5_ppmu_lock, flags);

	return handle;
}

void exynos5_ppmu_trace(void)
{
	unsigned long flags;

	spin_lock_irqsave(&exynos5_ppmu_lock, flags);

	exynos5_ppmu_update(0, PPMU_END - 1);
	spin_unlock_irqrestore(&exynos5_ppmu_lock, flags);
}

static int exynos5_ppmu_trace_init(void)
{
	exynos5_ppmu_trace_handle = exynos5_ppmu_get();
	return 0;
}
late_initcall(exynos5_ppmu_trace_init);

static void exynos5_ppmu_debug_compute(struct exynos_ppmu *ppmu,
	enum ppmu_counter i, u32 *sat, u32 *freq, u32 *bw)
{
	u64 ns = ppmu->ns;
	u32 busy = ppmu->count[i];
	u32 total = ppmu->ccnt;

	u64 s;
	u64 f;
	u64 b;

	s = (u64)busy * 100 * (1 << FIXED_POINT_OFFSET);
	s += total / 2;
	do_div(s, total);

	f = (u64)total * 1000 * (1 << FIXED_POINT_OFFSET);
	f += ns / 2;
	f = div64_u64(f, ns);

	b = (u64)busy * (128 / 8) * 1000 * (1 << FIXED_POINT_OFFSET);
	b += ns / 2;
	b = div64_u64(b, ns);

	*sat = s;
	*freq = f;
	*bw = b;
}

static void exynos5_ppmu_debug_show_one_counter(struct seq_file *s,
	const char *name, const char *type, struct exynos_ppmu *ppmu,
	enum ppmu_counter i, u32 *bw_total)
{
	u32 sat;
	u32 freq;
	u32 bw;

	exynos5_ppmu_debug_compute(ppmu, i, &sat, &freq, &bw);

	seq_printf(s, "%-10s %-10s %4u.%02u MBps %3u.%02u MHz %2u.%02u%%\n",
		name, type,
		bw >> FIXED_POINT_OFFSET,
		(bw & FIXED_POINT_MASK) * 100 / (1 << FIXED_POINT_OFFSET),
		freq >> FIXED_POINT_OFFSET,
		(freq & FIXED_POINT_MASK) * 100 / (1 << FIXED_POINT_OFFSET),
		sat >> FIXED_POINT_OFFSET,
		(sat & FIXED_POINT_MASK) * 100 / (1 << FIXED_POINT_OFFSET));

	*bw_total += bw;
}

static void exynos5_ppmu_debug_show_one(struct seq_file *s,
	const char *name, struct exynos_ppmu *ppmu,
	u32 *bw_total)
{
	exynos5_ppmu_debug_show_one_counter(s, name, "read+write",
		ppmu, PPMU_PMNCNT3, &bw_total[PPMU_PMNCNT3]);
	exynos5_ppmu_debug_show_one_counter(s, "", "read",
		ppmu, PPMU_PMNCNT0, &bw_total[PPMU_PMNCNT0]);
	exynos5_ppmu_debug_show_one_counter(s, "", "write",
		ppmu, PPMU_PMCCNT1, &bw_total[PPMU_PMCCNT1]);
}

static int exynos5_ppmu_debug_show(struct seq_file *s, void *d)
{
	int i, ret = 0;
	u32 bw_total[PPMU_PMNCNT_MAX];
	struct exynos5_ppmu_handle *handle;
	unsigned long flags;

	memset(bw_total, 0, sizeof(bw_total));

	handle = exynos5_ppmu_get();
	msleep(100);

	spin_lock_irqsave(&exynos5_ppmu_lock, flags);

	exynos5_ppmu_update(0, PPMU_END - 1);

	for (i = 0; i < PPMU_END; i++)
		exynos5_ppmu_debug_show_one(s, exynos5_ppmu_name[i],
				&handle->ppmu[i], bw_total);

	seq_printf(s, "%-10s %-10s %4u.%02u MBps\n", "total", "read+write",
		bw_total[PPMU_PMNCNT3] >> FIXED_POINT_OFFSET,
		(bw_total[PPMU_PMNCNT3] & FIXED_POINT_MASK) *
				100 / (1 << FIXED_POINT_OFFSET));
	seq_printf(s, "%-10s %-10s %4u.%02u MBps\n", "", "read",
		bw_total[PPMU_PMNCNT0] >> FIXED_POINT_OFFSET,
		(bw_total[PPMU_PMNCNT0] & FIXED_POINT_MASK) *
				100 / (1 << FIXED_POINT_OFFSET));
	seq_printf(s, "%-10s %-10s %4u.%02u MBps\n", "", "write",
		bw_total[PPMU_PMCCNT1] >> FIXED_POINT_OFFSET,
		(bw_total[PPMU_PMCCNT1] & FIXED_POINT_MASK) *
				100 / (1 << FIXED_POINT_OFFSET));

	seq_printf(s, "\n");

	spin_unlock_irqrestore(&exynos5_ppmu_lock, flags);

	exynos5_ppmu_put(handle);

	return ret;
}

static int exynos5_ppmu_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos5_ppmu_debug_show, inode->i_private);
}

const static struct file_operations exynos5_ppmu_debug_fops = {
	.open		= exynos5_ppmu_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init exynos5_ppmu_debug_init(void)
{
	debugfs_create_file("exynos5_bus_ppmu", S_IRUGO, NULL, NULL,
		&exynos5_ppmu_debug_fops);
	return 0;
}
late_initcall(exynos5_ppmu_debug_init);
