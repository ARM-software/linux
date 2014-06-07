/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_TIME_H
#define FIMC_IS_TIME_H

/* #define MEASURE_TIME */
/* #define INTERNAL_TIME */
/* #define EXTERNAL_TIME */
/* #define INTERFACE_TIME */

#define INSTANCE_MASK   0x3

#define TM_FLITE_STR	0
#define TM_FLITE_END	1
#define TM_SHOT		2
#define TM_SHOT_D	3
#define TM_META_D	4
#define TM_MAX_INDEX	5

struct fimc_is_time {
	u32 instance;
	u32 group_id;
	u32 report_period;
	u32 time_count;
	u32 time1_min;
	u32 time1_max;
	u32 time1_tot;
	u32 time2_min;
	u32 time2_max;
	u32 time2_tot;
	u32 time3_min;
	u32 time3_max;
	u32 time3_tot;
	u32 time4_cur;
	u32 time4_old;
	u32 time4_tot;
};

void TIME_STR1(void);
void TIME_END1(void);
uint64_t fimc_is_get_timestamp(void);

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME
void measure_init(struct fimc_is_time *time,
	u32 instance,
	u32 group_id,
	u32 report_period);
void measure_period(struct fimc_is_time *time,
	u32 report_period);
void measure_time(
	struct fimc_is_time *time,
	struct timeval *time_queued,
	struct timeval *time_shot,
	struct timeval *time_shotdone,
	struct timeval *time_dequeued);
#endif
#ifdef INTERFACE_TIME
struct fimc_is_interface_time {
	u32				cmd;
	u32				time_tot;
	u32				time_min;
	u32				time_max;
	u32				time_cnt;
};

void measure_init(struct fimc_is_interface_time *time, u32 cmd);
void measure_time(struct fimc_is_interface_time *time,
	u32 instance,
	u32 group,
	struct timeval *start,
	struct timeval *end);
#endif
#endif

#endif
