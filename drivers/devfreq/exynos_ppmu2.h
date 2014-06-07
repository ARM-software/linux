/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *              Sangkyu Kim <skwith.kim@samsung.com>
 *
 * EXYNOS - PPMU v2.x support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DEVFREQ_EXYNOS_PPMU2_H
#define __DEVFREQ_EXYNOS_PPMU2_H __FILE__

#include <linux/io.h>

#define REG_VER				(0x0000)
#define REG_PMNC			(0x0004)
#define REG_CNTENS			(0x0008)
#define REG_CNTENC			(0x000C)
#define REG_INTENS			(0x0010)
#define REG_INTENC			(0x0014)
#define REG_FLAG			(0x0018)
#define REG_CIG_CFG0			(0x001C)
#define REG_CIG_CFG1			(0x0020)
#define REG_CIG_CFG2			(0x0024)
#define REG_CIG_RESULT			(0x0028)
#define REG_CNT_RESET			(0x002C)
#define REG_CNT_AUTO			(0x0030)
#define REG_PMCNT0			(0x0034)
#define REG_PMCNT1			(0x0038)
#define REG_PMCNT2			(0x003C)
#define REG_PMCNT3_LOW			(0x0040)
#define REG_PMCNT3_HIGH			(0x0044)
#define REG_CCNT			(0x0048)
#define REG_EV0_TYPE			(0x0200)
#define REG_EV1_TYPE			(0x0204)
#define REG_EV2_TYPE			(0x0208)
#define REG_EV3_TYPE			(0x020C)
#define REG_SM_ID_V			(0x0220)
#define REG_SM_ID_A			(0x0224)
#define REG_SM_OTHERS_V			(0x0228)
#define REG_SM_OTHERS_A			(0x022C)
#define REG_TEST_INTERRUPT		(0x0260)

#define VER_MASK			(0xF)
#define VER_MAJOR_SHIFT			(16)
#define VER_MINOR_SHIFT			(12)
#define VER_REV_SHIFT			(8)
#define VER_RTL_SHIFT			(0)

enum CNT_MODE {
	CNT_DISABLE,
	CNT_ENABLE,
};

#define PMNC_MODE_OPERATION_MASK	(0x3)
#define PMNC_MODE_OPERATION_SHIFT	(20)
enum PMNC_MODE_OPERATION {
	PMNC_MODE_OPERATION_MANUAL,
	PMNC_MODE_OPERATION_AUTO,
	PMNC_MODE_OPERATION_CIG,
	PMNC_MODE_OPERATION_NONE,
};
#define PMNC_MODE_START_MASK		(0x1)
#define PMNC_MODE_START_SHIFT		(16)
enum PMNC_MODE_START {
	PMNC_MODE_START_APB,
	PMNC_MODE_START_EXTERNAL,
};
#define PMNC_MASK			(0x1)
#define PMNC_CCNT_DIVIDING_SHIFT	(3)
#define PMNC_CCNT_RESET_SHIFT		(2)
#define PMNC_CCNT_RESET_PMCNT_SHIFT	(1)
#define PMNC_GLB_CNT_EN_SHIFT		(0)

#define CNT_MASK			(0x1)
#define CCNT_SHIFT			(31)
#define PMCNT3_SHIFT			(3)
#define PMCNT2_SHIFT			(2)
#define PMCNT1_SHIFT			(1)
#define PMCNT0_SHIFT			(0)

#define CIG_CFG0_LOGIC_REL_MASK		(0x1)
#define CIG_CFG0_LOGIC_REL_UPPER_SHIFT	(20)
#define CIG_CFG0_LOGIC_REL_LOWER_SHIFT	(16)
#define CIG_CFG0_REPEAT_MASK		(0xF)
#define CIG_CFG0_REPEAT_UPPER_SHIFT	(8)
#define CIG_CFG0_REPEAT_LOWER_SHIFT	(4)
#define CIG_CFG0_PMCNT_MASK		(0x1)
#define CIG_CFG0_PMCNT2_SHIFT		(2)
#define CIG_CFG0_PMCNT1_SHIFT		(1)
#define CIG_CFG0_PMCNT0_SHIFT		(0)

#define CIG_RESULT_RECENT_MASK		(0x3)
#define CIG_RESULT_RECENT_UPPER_SHIFT	(12)
#define CIG_RESULT_RECENT_LOWER_SHIFT	(8)
#define CIG_RESULT_OOB_MASK		(0x1)
#define CIG_RESULT_OOB_UPPER_SHIFT	(4)
#define CIG_RESULT_OOB_LOWER_SHIFT	(0)

#define PMCNT3_VALUE_MASK		(0xFF)
#define PMCNT3_VALUE_SHIFT		(0)

#define EV_TYPE_MASK			(0xFF)
#define EV_TYPE_SHIFT			(0)
enum EVENT_TYPE {
	EV_RD_REQ_HS = 0x2,
	EV_WR_REQ_HS,
	EV_RD_DATA_HS,
	EV_WR_DATA_HS,
	EV_RD_RESP_HS,
	EV_RDWR_REQ_HS = 0x21,
	EV_RDWR_DATA_HS,
	EV_RD_LATENCY_SUM,
	EV_WR_LATENCY_SUM,
};

#define SM_ID_V_VALUE_MASK		(0xFFFF)
#define SM_ID_V_VALUE_SHIFT		(0)
#define SM_ID_A_MASK			(0x1)
#define SM_ID_A_PMCNT3_SHIFT		(3)
#define SM_ID_A_PMCNT2_SHIFT		(2)
#define SM_ID_A_PMCNT1_SHIFT		(1)
#define SM_ID_A_PMCNT0_SHIFT		(0)
#define SM_OTHERS_V_TYPE_MASK		(0x7)
#define SM_OTHERS_V_TYPE1_SHIFT		(24)
#define SM_OTHERS_V_TYPE0_SHIFT		(8)
enum SM_OTHERS_TYPE {
	SM_OTHERS_AXLEN,
	SM_OTHERS_AXSIZE,
	SM_OTHERS_AXBURST,
	SM_OTHERS_AXLOCK,
	SM_OTHERS_AXCACHE,
	SM_OTHERS_AXPROT,
	SM_OTHERS_RRESP,
	SM_OTHERS_BRESP,
};

#define INTERRUPT_TEST_MASK		(0x1)
#define INTERRUPT_TEST_CLEAR_SHIFT	(4)
#define INTERRUPT_TEST_CLEAR_SET	(0)

struct ppmu_info {
	void __iomem *base;
};

struct ppmu_version {
	unsigned int major;
	unsigned int minor;
	unsigned int revision;
	unsigned int rtl;
};

struct ppmu_mode {
	enum PMNC_MODE_OPERATION	operation_mode;
	enum PMNC_MODE_START		start_mode;
	enum CNT_MODE			dividing_enable;
	enum CNT_MODE			ccnt_reset;
	enum CNT_MODE			pmcnt_reset;
	enum CNT_MODE			count_enable;
};

struct ppmu_status {
	enum CNT_MODE			ccnt;
	enum CNT_MODE			pmcnt3;
	enum CNT_MODE			pmcnt2;
	enum CNT_MODE			pmcnt1;
	enum CNT_MODE			pmcnt0;
};

struct ppmu_cig_info {
	int				upper_use_pmcnt_all;
	int				lower_use_pmcnt_all;
	unsigned int			upper_repeat_count;
	unsigned int			lower_repeat_count;
	enum CNT_MODE			use_pmcnt2;
	enum CNT_MODE			use_pmcnt1;
	enum CNT_MODE			use_pmcnt0;
	unsigned int			upper_bound;
	unsigned int			lower_bound;
};

struct ppmu_cig_result {
	unsigned int			recent_compare_upper_index;
	unsigned int			recent_compare_lower_index;
	int				oob_upper_interrupt;
	int				oob_lower_interrupt;
};

int ppmu_get_version(struct ppmu_info *ppmu,
			struct ppmu_version *version);
int ppmu_set_mode(struct ppmu_info *ppmu,
			struct ppmu_mode *mode);
int ppmu_get_mode(struct ppmu_info *ppmu,
			struct ppmu_mode *mode);
int ppmu_counter_enable(struct ppmu_info *ppmu,
			struct ppmu_status *status);
int ppmu_counter_disable(struct ppmu_info *ppmu,
			struct ppmu_status *status);
int ppmu_get_counter_status(struct ppmu_info *ppmu,
				struct ppmu_status *status);
int ppmu_interrupt_enable(struct ppmu_info *ppmu,
			struct ppmu_status *status);
int ppmu_interrupt_disable(struct ppmu_info *ppmu,
			struct ppmu_status *status);
int ppmu_get_interrupt_status(struct ppmu_info *ppmu,
				struct ppmu_status *status);
int ppmu_set_interrupt_flag(struct ppmu_info *ppmu,
				struct ppmu_status *status);
int ppmu_get_unterrupt_flag(struct ppmu_info *ppmu,
				struct ppmu_status *status);
int ppmu_set_cig_info(struct ppmu_info *ppmu,
			struct ppmu_cig_info *info);
int ppmu_get_cig_info(struct ppmu_info *ppmu,
			struct ppmu_cig_info *info);
int ppmu_set_cig_result(struct ppmu_info *ppmu,
				struct ppmu_cig_result *result);
int ppmu_get_cig_result(struct ppmu_info *ppmu,
				struct ppmu_cig_result *result);
int ppmu_reset_counter(struct ppmu_info *ppmu,
			struct ppmu_status *status);
int ppmu_set_auto(struct ppmu_info *ppmu,
			struct ppmu_status *status);
int ppmu_set_pmcnt0(struct ppmu_info *ppmu,
			unsigned int value);
int ppmu_get_pmcnt0(struct ppmu_info *ppmu,
			unsigned int *value);
int ppmu_set_pmcnt1(struct ppmu_info *ppmu,
			unsigned int value);
int ppmu_get_pmcnt1(struct ppmu_info *ppmu,
			unsigned int *value);
int ppmu_set_pmcnt2(struct ppmu_info *ppmu,
			unsigned int value);
int ppmu_get_pmcnt2(struct ppmu_info *ppmu,
			unsigned int *value);
int ppmu_set_pmcnt3(struct ppmu_info *ppmu,
			unsigned long long value);
int ppmu_get_pmcnt3(struct ppmu_info *ppmu,
			unsigned long long *value);
int ppmu_get_ccnt(struct ppmu_info *ppmu,
			unsigned int *value);
int ppmu_set_event0(struct ppmu_info *ppmu,
			enum EVENT_TYPE event);
int ppmu_get_event0(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event);
int ppmu_set_event1(struct ppmu_info *ppmu,
			enum EVENT_TYPE event);
int ppmu_get_event1(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event);
int ppmu_set_event2(struct ppmu_info *ppmu,
                        enum EVENT_TYPE event);
int ppmu_get_event2(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event);
int ppmu_set_event3(struct ppmu_info *ppmu,
			enum EVENT_TYPE event);
int ppmu_get_event3(struct ppmu_info *ppmu,
			enum EVENT_TYPE *event);

/* Helper Function */
int ppmu_init(struct ppmu_info *ppmu);
int ppmu_term(struct ppmu_info *ppmu);
int ppmu_disable(struct ppmu_info *ppmu);
int ppmu_reset(struct ppmu_info *ppmu);
int ppmu_reset_total(struct ppmu_info *ppmu,
			unsigned int size);
int ppmu_count(struct ppmu_info *ppmu,
		unsigned long *ccnt,
		unsigned long *pmcnt);
int ppmu_count_total(struct ppmu_info *ppmu,
			unsigned int size,
			unsigned long *ccnt,
			unsigned long *pmcnt);
#endif /* __DEVFREQ_EXYNOS_PPMU2_H */
