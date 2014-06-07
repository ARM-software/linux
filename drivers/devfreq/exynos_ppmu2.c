/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *		Sangkyu Kim(skwith.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/printk.h>

#include "exynos_ppmu2.h"

static int ppmu_get_check_null(struct ppmu_info *ppmu)
{
	if (ppmu == NULL) {
		pr_err("DEVFREQ(PPMU) : ppmu has not address\n");
		return -EINVAL;
	}

	return 0;
}

static int ppmu_set_status(struct ppmu_info *ppmu,
				struct ppmu_status *status,
				unsigned int offset)
{
	unsigned int flags = 0, tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (status == NULL) {
		pr_err("DEVFREQ(PPMU) : status argument is NULL\n");
		return -EINVAL;
	}

	if (status->ccnt == CNT_ENABLE)
		flags |= (PMNC_MASK << CCNT_SHIFT);

	if (status->pmcnt3 == CNT_ENABLE)
		flags |= (PMNC_MASK << PMCNT3_SHIFT);

	if (status->pmcnt2 == CNT_ENABLE)
		flags |= (PMNC_MASK << PMCNT2_SHIFT);

	if (status->pmcnt1 == CNT_ENABLE)
		flags |= (PMNC_MASK << PMCNT1_SHIFT);

	if (status->pmcnt0 == CNT_ENABLE)
		flags |= (PMNC_MASK << PMCNT0_SHIFT);

	tmp = __raw_readl(ppmu->base + offset);
	tmp |= flags;
	__raw_writel(tmp, ppmu->base + offset);

	return 0;
}

static int ppmu_get_status(struct ppmu_info *ppmu,
				struct ppmu_status *status,
				unsigned int offset)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (status == NULL) {
		pr_err("DEVFREQ(PPMU) : status argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + offset);

	status->ccnt	= (tmp >> CCNT_SHIFT) & PMNC_MASK;
	status->pmcnt3	= (tmp >> PMCNT3_SHIFT) & PMNC_MASK;
	status->pmcnt2	= (tmp >> PMCNT2_SHIFT) & PMNC_MASK;
	status->pmcnt1	= (tmp >> PMCNT1_SHIFT) & PMNC_MASK;
	status->pmcnt0	= (tmp >> PMCNT0_SHIFT) & PMNC_MASK;

	return 0;
}

static int ppmu_set_cnt(struct ppmu_info *ppmu,
			unsigned int value,
			unsigned int offset)
{
	if (ppmu_get_check_null(ppmu))
		return false;

	__raw_writel(value, ppmu->base + offset);

	return 0;
}

static int ppmu_get_cnt(struct ppmu_info *ppmu,
			unsigned int *value,
			unsigned int offset)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (value == NULL) {
		pr_err("DEVFREQ(PPMU) : cnt argument is NULL\n");
		return -EINVAL;
	}

	*value = __raw_readl(ppmu->base + offset);

	return 0;
}

static int ppmu_set_event(struct ppmu_info *ppmu,
				enum EVENT_TYPE event,
				unsigned int offset)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	__raw_writel((event & EV_TYPE_MASK) << EV_TYPE_SHIFT, ppmu->base + offset);

	return 0;
}

static int ppmu_get_event(struct ppmu_info *ppmu,
				enum EVENT_TYPE *event,
				unsigned int offset)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (event == NULL) {
		pr_err("DEVFREQ(PPMU) : get event argument is NULL\n");
		return -EINVAL;
	}

	*event = (__raw_readl(ppmu->base + offset) & EV_TYPE_MASK) >> EV_TYPE_SHIFT;

	return 0;
}

int ppmu_get_version(struct ppmu_info *ppmu,
			struct ppmu_version *version)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	tmp = __raw_readl(ppmu->base + REG_VER);

	version->major		= (tmp >> VER_MAJOR_SHIFT) & VER_MASK;
	version->minor		= (tmp >> VER_MINOR_SHIFT) & VER_MASK;
	version->revision	= (tmp >> VER_REV_SHIFT) & VER_MASK;
	version->rtl		= (tmp >> VER_RTL_SHIFT) & VER_MASK;

	return 0;
}

int ppmu_set_mode(struct ppmu_info *ppmu,
			struct ppmu_mode *mode)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (mode == NULL) {
		pr_err("DEVFREQ(PPMU) : set mode argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + REG_PMNC);

	tmp &= ~((PMNC_MODE_OPERATION_MASK << PMNC_MODE_OPERATION_SHIFT) |
		(PMNC_MODE_START_MASK << PMNC_MODE_START_SHIFT) |
		(PMNC_MASK << PMNC_CCNT_DIVIDING_SHIFT) |
		(PMNC_MASK << PMNC_CCNT_RESET_SHIFT) |
		(PMNC_MASK << PMNC_CCNT_RESET_PMCNT_SHIFT) |
		(PMNC_MASK << PMNC_GLB_CNT_EN_SHIFT));

	tmp |= (((mode->operation_mode & PMNC_MODE_OPERATION_MASK) << PMNC_MODE_OPERATION_SHIFT) |
		((mode->start_mode & PMNC_MODE_START_MASK) << PMNC_MODE_START_SHIFT) |
		((mode->dividing_enable & PMNC_MASK) << PMNC_CCNT_DIVIDING_SHIFT) |
		((mode->ccnt_reset & PMNC_MASK) << PMNC_CCNT_RESET_SHIFT) |
		((mode->pmcnt_reset & PMNC_MASK) << PMNC_CCNT_RESET_PMCNT_SHIFT) |
		((mode->count_enable & PMNC_MASK) << PMNC_GLB_CNT_EN_SHIFT));

	__raw_writel(tmp, ppmu->base + REG_PMNC);

	return 0;
}

int ppmu_get_mode(struct ppmu_info *ppmu,
			struct ppmu_mode *mode)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (mode == NULL) {
		pr_err("DEVFREQ(PPMU) : get mode argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + REG_PMNC);

	mode->operation_mode	= (tmp >> PMNC_MODE_OPERATION_SHIFT) & PMNC_MODE_OPERATION_MASK;
	mode->start_mode	= (tmp >> PMNC_MODE_START_SHIFT) & PMNC_MODE_START_MASK;
	mode->dividing_enable	= (tmp >> PMNC_CCNT_DIVIDING_SHIFT) & PMNC_MASK;
	mode->ccnt_reset	= (tmp >> PMNC_CCNT_RESET_SHIFT) & PMNC_MASK;
	mode->pmcnt_reset	= (tmp >> PMNC_CCNT_RESET_PMCNT_SHIFT) & PMNC_MASK;
	mode->count_enable	= (tmp >> PMNC_GLB_CNT_EN_SHIFT) & PMNC_MASK;

	return 0;
}

int ppmu_counter_enable(struct ppmu_info *ppmu,
			struct ppmu_status *status)
{
	return ppmu_set_status(ppmu, status, REG_CNTENS);
}

int ppmu_counter_disable(struct ppmu_info *ppmu,
			struct ppmu_status *status)
{
	return ppmu_set_status(ppmu, status, REG_CNTENC);
}

int ppmu_get_counter_status(struct ppmu_info *ppmu,
				struct ppmu_status *status)
{
	return ppmu_get_status(ppmu, status, REG_CNTENS);
}

int ppmu_interrupt_enable(struct ppmu_info *ppmu,
			struct ppmu_status *status)
{
	return ppmu_set_status(ppmu, status, REG_INTENS);
}

int ppmu_interrupt_disable(struct ppmu_info *ppmu,
			struct ppmu_status *status)
{
	return ppmu_set_status(ppmu, status, REG_INTENS);
}

int ppmu_get_interrupt_status(struct ppmu_info *ppmu,
				struct ppmu_status *status)
{
	return ppmu_get_status(ppmu, status, REG_INTENS);
}

int ppmu_set_interrupt_flag(struct ppmu_info *ppmu,
				struct ppmu_status *status)
{
	return ppmu_set_status(ppmu, status, REG_FLAG);
}

int ppmu_get_interrupt_flag(struct ppmu_info *ppmu,
				struct ppmu_status *status)
{
	return ppmu_get_status(ppmu, status, REG_FLAG);
}

int ppmu_set_cig_info(struct ppmu_info *ppmu,
			struct ppmu_cig_info *info)
{
	unsigned int tmp = 0;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (info == NULL) {
		pr_err("DEVFREQ(PPMU) : set cig info argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + REG_CIG_CFG0);

	tmp &= ~((CIG_CFG0_LOGIC_REL_MASK << CIG_CFG0_LOGIC_REL_UPPER_SHIFT) |
		(CIG_CFG0_LOGIC_REL_MASK << CIG_CFG0_LOGIC_REL_LOWER_SHIFT) |
		(CIG_CFG0_REPEAT_MASK << CIG_CFG0_REPEAT_UPPER_SHIFT) |
		(CIG_CFG0_REPEAT_MASK << CIG_CFG0_REPEAT_LOWER_SHIFT) |
		(CIG_CFG0_PMCNT_MASK << CIG_CFG0_PMCNT2_SHIFT) |
		(CIG_CFG0_PMCNT_MASK << CIG_CFG0_PMCNT1_SHIFT) |
		(CIG_CFG0_PMCNT_MASK << CIG_CFG0_PMCNT0_SHIFT));

	tmp |= (((info->upper_use_pmcnt_all & CIG_CFG0_LOGIC_REL_MASK) << CIG_CFG0_LOGIC_REL_UPPER_SHIFT) |
		((info->lower_use_pmcnt_all & CIG_CFG0_LOGIC_REL_MASK) << CIG_CFG0_LOGIC_REL_LOWER_SHIFT) |
		((info->upper_repeat_count & CIG_CFG0_REPEAT_MASK) << CIG_CFG0_REPEAT_UPPER_SHIFT) |
		((info->lower_repeat_count & CIG_CFG0_REPEAT_MASK) << CIG_CFG0_REPEAT_LOWER_SHIFT) |
		((info->use_pmcnt2 & CIG_CFG0_PMCNT_MASK) << CIG_CFG0_PMCNT2_SHIFT) |
		((info->use_pmcnt1 & CIG_CFG0_PMCNT_MASK) << CIG_CFG0_PMCNT1_SHIFT) |
		((info->use_pmcnt0 & CIG_CFG0_PMCNT_MASK) << CIG_CFG0_PMCNT0_SHIFT));

	__raw_writel(tmp, ppmu->base + REG_CIG_CFG0);

	__raw_writel(info->lower_bound, ppmu->base + REG_CIG_CFG1);
	__raw_writel(info->upper_bound, ppmu->base + REG_CIG_CFG2);

	return 0;
}

int ppmu_get_cig_info(struct ppmu_info *ppmu,
			struct ppmu_cig_info *info)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (info == NULL) {
		pr_err("DEVFREQ(PPMU) : get cig info argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + REG_CIG_CFG0);

	info->upper_use_pmcnt_all = (tmp >> CIG_CFG0_LOGIC_REL_UPPER_SHIFT) & CIG_CFG0_LOGIC_REL_MASK;
	info->lower_use_pmcnt_all = (tmp >> CIG_CFG0_LOGIC_REL_LOWER_SHIFT) & CIG_CFG0_LOGIC_REL_MASK;
	info->upper_repeat_count = (tmp >> CIG_CFG0_REPEAT_UPPER_SHIFT) & CIG_CFG0_REPEAT_MASK;
	info->lower_repeat_count = (tmp >> CIG_CFG0_REPEAT_LOWER_SHIFT) & CIG_CFG0_REPEAT_MASK;
	info->use_pmcnt2 = (tmp >> CIG_CFG0_PMCNT2_SHIFT) & CIG_CFG0_PMCNT_MASK;
	info->use_pmcnt1 = (tmp >> CIG_CFG0_PMCNT1_SHIFT) & CIG_CFG0_PMCNT_MASK;
	info->use_pmcnt0 = (tmp >> CIG_CFG0_PMCNT0_SHIFT) & CIG_CFG0_PMCNT_MASK;
	info->lower_bound = __raw_readl(ppmu->base + REG_CIG_CFG1);
	info->upper_bound = __raw_readl(ppmu->base + REG_CIG_CFG2);

	return 0;
}

int ppmu_set_cig_result(struct ppmu_info *ppmu,
				struct ppmu_cig_result *result)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (result == NULL) {
		pr_err("DEVFREQ(PPMU) : set cig result argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + REG_CIG_RESULT);

	tmp &= ~((CIG_RESULT_OOB_MASK << CIG_RESULT_OOB_UPPER_SHIFT) |
		(CIG_RESULT_OOB_MASK << CIG_RESULT_OOB_LOWER_SHIFT));

	tmp |= (((result->oob_upper_interrupt & CIG_RESULT_OOB_MASK) << CIG_RESULT_OOB_UPPER_SHIFT) |
		((result->oob_lower_interrupt & CIG_RESULT_OOB_MASK) << CIG_RESULT_OOB_LOWER_SHIFT));

	return 0;
}

int ppmu_get_cig_result(struct ppmu_info *ppmu,
				struct ppmu_cig_result *result)
{
	unsigned int tmp;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (result == NULL) {
		pr_err("DEVFREQ(PPMU) : get cig result argument is NULL\n");
		return -EINVAL;
	}

	tmp = __raw_readl(ppmu->base + REG_CIG_RESULT);

	result->recent_compare_upper_index = (tmp >> CIG_RESULT_RECENT_UPPER_SHIFT) & CIG_RESULT_RECENT_MASK;
	result->recent_compare_lower_index = (tmp >> CIG_RESULT_RECENT_LOWER_SHIFT) & CIG_RESULT_RECENT_MASK;
	result->oob_upper_interrupt = (tmp >> CIG_RESULT_OOB_UPPER_SHIFT) & CIG_RESULT_OOB_MASK;
	result->oob_lower_interrupt = (tmp >> CIG_RESULT_OOB_LOWER_SHIFT) & CIG_RESULT_OOB_MASK;

	return 0;
}

int ppmu_reset_counter(struct ppmu_info *ppmu,
			struct ppmu_status *status)
{
	unsigned int tmp = 0;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (status == NULL) {
		pr_err("DEVFREQ(PPMU) : reset counter argument is NULL\n");
		return -EINVAL;
	}

	tmp |= (((status->ccnt & CNT_MASK) << CCNT_SHIFT) |
		((status->pmcnt3 & CNT_MASK) << PMCNT3_SHIFT) |
		((status->pmcnt2 & CNT_MASK) << PMCNT2_SHIFT) |
		((status->pmcnt1 & CNT_MASK) << PMCNT1_SHIFT) |
		((status->pmcnt0 & CNT_MASK) << PMCNT0_SHIFT));

	__raw_writel(tmp, ppmu->base + REG_CNT_RESET);

	return 0;
}

int ppmu_set_auto(struct ppmu_info *ppmu,
			struct ppmu_status *status)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (status == NULL) {
		pr_err("DEVFREQ(PPMU) : set auto argument is NULL\n");
		return -EINVAL;
	}

	return ppmu_set_status(ppmu, status, REG_CNT_AUTO);
}

int ppmu_set_pmcnt0(struct ppmu_info *ppmu,
			unsigned int value)
{
	return ppmu_set_cnt(ppmu, value, REG_PMCNT0);
}

int ppmu_get_pmcnt0(struct ppmu_info *ppmu,
			unsigned int *value)
{
	return ppmu_get_cnt(ppmu, value, REG_PMCNT0);
}

int ppmu_set_pmcnt1(struct ppmu_info *ppmu,
			unsigned int value)
{
	return ppmu_set_cnt(ppmu, value, REG_PMCNT1);
}

int ppmu_get_pmcnt1(struct ppmu_info *ppmu,
			unsigned int *value)
{
	return ppmu_get_cnt(ppmu, value, REG_PMCNT1);
}

int ppmu_set_pmcnt2(struct ppmu_info *ppmu,
			unsigned int value)
{
	return ppmu_set_cnt(ppmu, value, REG_PMCNT2);
}

int ppmu_get_pmcnt2(struct ppmu_info *ppmu,
			unsigned int *value)
{
	return ppmu_get_cnt(ppmu, value, REG_PMCNT2);
}

int ppmu_set_pmcnt3(struct ppmu_info *ppmu,
			unsigned long long value)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	__raw_writel((value & 0xFFFFFFFF), ppmu->base + REG_PMCNT3_LOW);
	__raw_writel((value >> 32), ppmu->base + REG_PMCNT3_HIGH);

	return 0;
}

int ppmu_get_pmcnt3(struct ppmu_info *ppmu,
			unsigned long long *value)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (value == NULL) {
		pr_err("DEVFREQ(PPMU) : get pmcnt3 argument is NULL\n");
		return -EINVAL;
	}

	*value = __raw_readl(ppmu->base + REG_PMCNT3_HIGH);
	*value = *value << 32 | __raw_readl(ppmu->base + REG_PMCNT3_LOW);

	return 0;
}

int ppmu_get_ccnt(struct ppmu_info *ppmu,
			unsigned int *value)
{
	return ppmu_get_cnt(ppmu, value, REG_CCNT);
}

int ppmu_set_event0(struct ppmu_info *ppmu,
			enum EVENT_TYPE event)
{
	if (event >= EV_RDWR_REQ_HS) {
		pr_err("DEVFREQ(PPMU) : event0 do not support %d event\n", event);
		return -EINVAL;
	}

	return ppmu_set_event(ppmu, event, REG_EV0_TYPE);
}

int ppmu_get_event0(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event)
{
	return ppmu_get_event(ppmu, event, REG_EV0_TYPE);
}

int ppmu_set_event1(struct ppmu_info *ppmu,
			enum EVENT_TYPE event)
{
	if (event >= EV_RDWR_REQ_HS) {
		pr_err("DEVFREQ(PPMU) : event1 do not support %d event\n", event);
		return -EINVAL;
	}

	return ppmu_set_event(ppmu, event, REG_EV1_TYPE);
}

int ppmu_get_event1(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event)
{
	return ppmu_get_event(ppmu, event, REG_EV1_TYPE);
}

int ppmu_set_event2(struct ppmu_info *ppmu,
			enum EVENT_TYPE event)
{
	if (event >= EV_RDWR_REQ_HS) {
		pr_err("DEVFREQ(PPMU) : event2 do not support %d event\n", event);
		return -EINVAL;
	}

	return ppmu_set_event(ppmu, event, REG_EV2_TYPE);
}

int ppmu_get_event2(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event)
{
	return ppmu_get_event(ppmu, event, REG_EV2_TYPE);
}

int ppmu_set_event3(struct ppmu_info *ppmu,
			enum EVENT_TYPE event)
{
	return ppmu_set_event(ppmu, event, REG_EV3_TYPE);
}

int ppmu_get_event3(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event)
{
	return ppmu_get_event(ppmu, event, REG_EV3_TYPE);
}

/*
 * Helper Function
 */
int ppmu_init(struct ppmu_info *ppmu)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (ppmu->base == NULL) {
		pr_err("DEVFREQ(PPMU) : ppmu base address is null\n");
		return -EINVAL;
	}

	ppmu->base = ioremap((unsigned long)ppmu->base, SZ_1K);
	if (ppmu->base == NULL) {
		pr_err("DEVFREQ(PPMU) : ppmu base remap failed\n");
		return -EINVAL;
	}

	return 0;
}

int ppmu_term(struct ppmu_info *ppmu)
{
	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	iounmap(ppmu->base);

	return 0;
}

int ppmu_reset(struct ppmu_info *ppmu)
{
	int ret;
	struct ppmu_mode mode;
	struct ppmu_status status;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	ret = ppmu_get_mode(ppmu, &mode);
	if (ret)
		return ret;

	mode.ccnt_reset		= CNT_ENABLE;
	mode.pmcnt_reset	= CNT_ENABLE;
	mode.count_enable	= CNT_ENABLE;

	ret = ppmu_set_mode(ppmu, &mode);
	if (ret)
		return ret;

	ret = ppmu_set_event0(ppmu, EV_RD_DATA_HS);
	if (ret)
		return ret;

	ret = ppmu_set_event1(ppmu, EV_WR_DATA_HS);
	if (ret)
		return ret;

	ret = ppmu_set_event3(ppmu, EV_RDWR_DATA_HS);
	if (ret)
		return ret;

	status.ccnt		= CNT_ENABLE;
	status.pmcnt3		= CNT_ENABLE;
	status.pmcnt1		= CNT_ENABLE;
	status.pmcnt0		= CNT_ENABLE;

	ret = ppmu_counter_enable(ppmu, &status);
	if (ret)
		return ret;

	return ppmu_set_interrupt_flag(ppmu, &status);
}

int ppmu_disable(struct ppmu_info *ppmu)
{
	struct ppmu_status status;

	status.ccnt		= CNT_ENABLE;
	status.pmcnt3		= CNT_ENABLE;
	status.pmcnt2		= CNT_ENABLE;
	status.pmcnt1		= CNT_ENABLE;
	status.pmcnt0		= CNT_ENABLE;

	return ppmu_counter_disable(ppmu, &status);
}

int ppmu_reset_total(struct ppmu_info *ppmu,
		unsigned int size)
{
	unsigned int i;

	for (i = 0; i < size; ++i) {
		if (ppmu_reset(ppmu + i))
			return -EINVAL;
	}

	return 0;
}

int ppmu_count(struct ppmu_info *ppmu,
		unsigned long *ccnt,
		unsigned long *pmcnt)
{
	unsigned int val_ccnt, val_pmcnt0, val_pmcnt1;
	unsigned long long val_pmcnt3;

	if (ppmu_get_check_null(ppmu))
		return -EINVAL;

	if (ppmu_get_ccnt(ppmu, &val_ccnt))
		return -EINVAL;

	if (ppmu_get_pmcnt0(ppmu, &val_pmcnt0))
		return -EINVAL;

	if (ppmu_get_pmcnt1(ppmu, &val_pmcnt1))
		return -EINVAL;

	if (ppmu_get_pmcnt3(ppmu, &val_pmcnt3))
		return -EINVAL;

	*ccnt = val_ccnt;
	*pmcnt = val_pmcnt0 + val_pmcnt1 + val_pmcnt3;

	return 0;
}

int ppmu_count_total(struct ppmu_info *ppmu,
			unsigned int size,
			unsigned long *ccnt,
			unsigned long *pmcnt)
{
	unsigned int i;
	unsigned long val_ccnt = 0;
	unsigned long val_pmcnt = 0;

	if (ccnt == NULL ||
		pmcnt == NULL) {
		pr_err("DEVFREQ(PPMU) : count argument is NULL\n");
		return -EINVAL;
	}

	*ccnt = 0;
	*pmcnt = 0;

	for (i = 0; i < size; ++i)
		ppmu_disable(ppmu + i);

	for (i = 0; i < size; ++i) {
		if (ppmu_count(ppmu + i, &val_ccnt, &val_pmcnt))
			return -EINVAL;

		if (*ccnt < val_ccnt)
			*ccnt = val_ccnt;

		if (*pmcnt < val_pmcnt)
			*pmcnt = val_pmcnt;
	}

	return 0;
}
