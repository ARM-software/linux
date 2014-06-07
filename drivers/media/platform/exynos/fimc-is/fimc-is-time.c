#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/time.h>

#include "fimc-is-time.h"

static struct timeval itime1;

void TIME_STR1(void)
{
	do_gettimeofday(&itime1);
}

void TIME_END1(void)
{
	u32 time;
	struct timeval temp;

	do_gettimeofday(&temp);
	time = (temp.tv_sec - itime1.tv_sec)*1000000 +
		(temp.tv_usec - itime1.tv_usec);

	pr_info("TIME_MEASURE : %dus\n", time);
}

uint64_t fimc_is_get_timestamp(void)
{
	struct timespec curtime;

	do_posix_clock_monotonic_gettime(&curtime);

	return (uint64_t)curtime.tv_sec*1000000000 + curtime.tv_nsec;
}

#ifdef MEASURE_TIME
#ifdef INTERNAL_TIME

void measure_init(struct fimc_is_time *time,
	u32 instance,
	u32 group_id,
	u32 report_period)
{
	time->instance = instance;
	time->group_id = group_id;
	time->report_period = report_period;
	time->time_count = 0;
	time->time1_min = 0;
	time->time1_max = 0;
	time->time1_tot = 0;
	time->time2_min = 0;
	time->time2_max = 0;
	time->time2_tot = 0;
	time->time3_min = 0;
	time->time3_max = 0;
	time->time3_tot = 0;
	time->time4_cur = 0;
	time->time4_old = 0;
	time->time4_tot = 0;
}

void measure_period(struct fimc_is_time *time,
	u32 report_period)
{
	time->report_period = report_period;
}

void measure_time(
	struct fimc_is_time *time,
	struct timeval *time_queued,
	struct timeval *time_shot,
	struct timeval *time_shotdone,
	struct timeval *time_dequeued)
{
	u32 temp1, temp2, temp3;

	temp1 = (time_shot->tv_sec - time_queued->tv_sec)*1000000 +
		(time_shot->tv_usec - time_queued->tv_usec);
	temp2 = (time_shotdone->tv_sec - time_shot->tv_sec)*1000000 +
		(time_shotdone->tv_usec - time_shot->tv_usec);
	temp3 = (time_dequeued->tv_sec - time_shotdone->tv_sec)*1000000 +
		(time_dequeued->tv_usec - time_shotdone->tv_usec);

	if (!time->time_count) {
		time->time1_min = temp1;
		time->time1_max = temp1;
		time->time2_min = temp2;
		time->time2_max = temp2;
		time->time3_min = temp3;
		time->time3_max = temp3;
	} else {
		if (time->time1_min > temp1)
			time->time1_min = temp1;

		if (time->time1_max < temp1)
			time->time1_max = temp1;

		if (time->time2_min > temp2)
			time->time2_min = temp2;

		if (time->time2_max < temp2)
			time->time2_max = temp2;

		if (time->time3_min > temp3)
			time->time3_min = temp3;

		if (time->time3_max < temp3)
			time->time3_max = temp3;
	}

	time->time1_tot += temp1;
	time->time2_tot += temp2;
	time->time3_tot += temp3;

	time->time4_cur = time_queued->tv_sec*1000000 + time_queued->tv_usec;
	time->time4_tot += (time->time4_cur - time->time4_old);
	time->time4_old = time->time4_cur;

	time->time_count++;

	if (time->time_count % time->report_period)
		return;

	pr_info("I%dG%d t1(%05d,%05d,%05d), t2(%05d,%05d,%05d), t3(%05d,%05d,%05d) : %d(%dfps)",
		time->instance, time->group_id,
		temp1, time->time1_max, time->time1_tot / time->time_count,
		temp2, time->time2_max, time->time2_tot / time->time_count,
		temp3, time->time3_max, time->time3_tot / time->time_count,
		time->time4_tot / time->report_period,
		(1000000 * time->report_period) / time->time4_tot);

	time->time_count = 0;
	time->time1_tot = 0;
	time->time2_tot = 0;
	time->time3_tot = 0;
	time->time4_tot = 0;
}

#endif

#ifdef INTERFACE_TIME
void measure_init(struct fimc_is_interface_time *time, u32 cmd)
{
	time->cmd = cmd;
	time->time_max = 0;
	time->time_min = 0;
	time->time_tot = 0;
	time->time_cnt = 0;
}

void measure_time(struct fimc_is_interface_time *time,
	u32 instance,
	u32 group,
	struct timeval *start,
	struct timeval *end)
{
	u32 temp;

	temp = (end->tv_sec - start->tv_sec)*1000000 + (end->tv_usec - start->tv_usec);

	if (time->time_cnt) {
		time->time_max = temp;
		time->time_min = temp;
	} else {
		if (time->time_min > temp)
			time->time_min = temp;

		if (time->time_max < temp)
			time->time_max = temp;
	}

	time->time_tot += temp;
	time->time_cnt++;

	pr_info("cmd[%d][%d](%d) : curr(%d), max(%d), avg(%d)\n",
		instance, group, time->cmd, temp, time->time_max, time->time_tot / time->time_cnt);
}
#endif

#endif
