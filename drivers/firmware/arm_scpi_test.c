/*
 * Test code for System Control and Power Interface (SCPI)
 *
 * Copyright (C) 2015 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/scpi_protocol.h>


static int stress_time;
module_param(stress_time, int, 0644);
MODULE_PARM_DESC(stress_time, "Number of seconds to run each stress test for, overides each test's default.");

static int run;
static struct kernel_param_ops run_ops;
module_param_cb(run, &run_ops, &run, 0644);
MODULE_PARM_DESC(run, "The number of the test case to run, or -1 for all, 0 to stop tests.");


static struct scpi_ops *scpi;

static struct task_struct *main_thread;

static DEFINE_MUTEX(main_thread_lock);


#define MAX_TEST_THREADS 4

static struct test_thread {
	struct task_struct *task;
	int thread_num;
} test_threads[MAX_TEST_THREADS];

static DEFINE_MUTEX(thread_lock);


#define MAX_POWER_DOMAINS 8

static u16 num_sensors;
static u8 num_pd;
static u8 num_opps[MAX_POWER_DOMAINS];
static struct mutex pd_lock[MAX_POWER_DOMAINS];


#define	FLAG_SERIAL_DVFS	(1<<0)
#define	FLAG_SERIAL_PD		(1<<1)

static int test_flags;


static int sensor_pmic = -1;


static u32 random_seed;

static u32 random(u32 range)
{
	random_seed = random_seed*69069+1;

	return ((u64)random_seed * (u64)range) >> 32;
}


static atomic_t passes;
static atomic_t failures;

static bool fail_on(bool fail)
{
	if (fail)
		atomic_inc(&failures);
	else
		atomic_inc(&passes);
	return fail;
}

static void show_results(const char *title)
{
	int fail = atomic_xchg(&failures, 0);
	int pass = atomic_xchg(&passes, 0);

	if (fail)
		pr_err("Results for '%s' is %d/%d (pass/fail)\n", title, pass, fail);
	else
		pr_info("Results for '%s' is %d/%d (pass/fail)\n", title, pass, fail);
}


static bool check_name(const char *name)
{
	char c;

	if (!isalpha(*name++))
		return false;

	while ((c = *name++))
		if (!isalnum(c) && c != '_')
			return false;

	return true;
}

static u32 get_sensor(u16 id)
{
	u32 val;
	int ret;

	ret = scpi->sensor_get_value(id, &val);
	if (fail_on(ret < 0))
		pr_err("FAILED sensor_get_value %d (%d)\n", id, ret);

	return val;
}

static void init_sensors(void)
{
	u16 id;
	int ret;

	ret = scpi->sensor_get_capability(&num_sensors);
	if (fail_on(ret))
		pr_err("FAILED sensor_get_capability (%d)\n", ret);

	pr_info("num_sensors: %d\n", num_sensors);

	for (id = 0; id < num_sensors; id++) {

		struct scpi_sensor_info info;
		char name[sizeof(info.name) + 1];

		ret = scpi->sensor_get_info(id, &info);
		if (fail_on(ret)) {
			pr_err("FAILED sensor_get_info (%d)\n", ret);
			continue;
		}

		/* Get sensor name, guarding against missing NUL terminator */
		memcpy(name, info.name, sizeof(info.name));
		name[sizeof(info.name)] = 0;

		pr_info("sensor[%d] id=%d class=%d trigger=%d name=%s\n", id,
			info.sensor_id, info.class, info.trigger_type, name);

		if (fail_on(id != info.sensor_id))
			pr_err("FAILED bad sensor id\n");
		if (fail_on(info.class > 4))
			pr_err("FAILED bad sensor class\n");
		if (fail_on(info.trigger_type > 3))
			pr_err("FAILED bad sensor trigger type\n");
		if (fail_on(strlen(name) >= sizeof(info.name) || !check_name(name)))
			pr_err("FAILED bad name\n");

		pr_info("sensor[%d] value is %u\n", id, get_sensor(id));
		if (strstr(name, "PMIC"))
			sensor_pmic = id;
	}
}

static int get_dvfs(u8 pd)
{
	int ret = scpi->dvfs_get_idx(pd);

	if (fail_on(ret < 0))
		pr_err("FAILED get_dvfs %d (%d)\n", pd, ret);
	else if (fail_on(ret >= num_opps[pd]))
		pr_err("FAILED get_dvfs %d returned out of range index (%d)\n", pd, ret);

	return ret;
}

static int set_dvfs(u8 pd, u8 opp)
{
	int ret;

	if (test_flags & FLAG_SERIAL_DVFS)
		mutex_lock(&pd_lock[0]);
	else if (test_flags & FLAG_SERIAL_PD)
		mutex_lock(&pd_lock[pd]);

	ret = scpi->dvfs_set_idx(pd, opp);

	if (test_flags & FLAG_SERIAL_DVFS)
		mutex_unlock(&pd_lock[0]);
	else if (test_flags & FLAG_SERIAL_PD)
		mutex_unlock(&pd_lock[pd]);

	if (fail_on(ret < 0))
		pr_err("FAILED set_dvfs %d %d (%d)\n", pd, opp, ret);

	return ret;
}

static void init_dvfs(void)
{
	u8 pd;

	for (pd = 0; pd < MAX_POWER_DOMAINS; ++pd) {
		struct scpi_dvfs_info *info;
		int opp;

		info = scpi->dvfs_get_info(pd);
		if (IS_ERR(info)) {
			pr_info("dvfs_get_info %d failed with %d assume because no more power domains\n",
				pd, (int)PTR_ERR(info));
			break;
		}

		num_opps[pd] = info->count;
		mutex_init(&pd_lock[pd]);
		pr_info("pd[%d] count=%u latency=%u\n",
			pd, info->count, info->latency);

		opp = get_dvfs(pd);
		pr_info("pd[%d] current opp=%d\n", pd, opp);

		for (opp = 0; opp < info->count; ++opp) {
			pr_info("pd[%d].opp[%d] freq=%u m_volt=%u\n", pd, opp,
				info->opps[opp].freq, info->opps[opp].m_volt);
			/*
			 * Try setting each opp. Note, failure is not necessarily
			 * an error because cpufreq may be setting values too.
			 */
			set_dvfs(pd, opp);
			if (get_dvfs(pd) == opp)
				pr_info("pd[%d] set to opp %d OK\n", pd, opp);
			else
				pr_warn("pd[%d] failed to set opp to %d\n", pd, opp);
		}
	}

	if (!pd) {
		/* Assume device should have at least one DVFS power domain */
		pr_err("FAILED no power domains\n");
		fail_on(true);
	}
	num_pd = pd;
}

static int stress_pmic(void *data)
{
	int sensor, pd, opp;

	while (!kthread_should_stop()) {
		sensor = sensor_pmic;
		pd = random(num_pd);
		opp = random(num_opps[pd]);

		switch (random(3)) {
		case 0:
			if (sensor >= 0) {
				get_sensor(sensor);
				break;
			}
			/* If no sensor, do DFVS... */
		case 1:
			set_dvfs(pd, opp);
			break;
		default:
			msleep(random(20));
			break;
		}
	}

	return 0;
}

static int stress_all(void *data)
{
	int sensor, pd, opp;

	while (!kthread_should_stop()) {
		sensor = random(num_sensors);
		pd = random(num_pd);
		opp = random(num_opps[pd]);

		switch (random(4)) {
		case 0:
			set_dvfs(pd, opp);
			break;
		case 1:
			opp = get_dvfs(pd);
			break;
		case 2:
			get_sensor(sensor);
			break;
		default:
			msleep(random(20));
			break;
		}
	}

	return 0;
}

struct test {
	const char *title;
	int (*thread_fn)(void *);
	int flags;
	int num_threads;
	int duration;
};

static void stop_test_threads(void)
{
	int t, ret;

	for (t = 0; t < MAX_TEST_THREADS; ++t) {
		struct test_thread *thread = &test_threads[t];

		mutex_lock(&thread_lock);
		if (thread->task) {
			ret = kthread_stop(thread->task);
			thread->task = NULL;
			if (ret)
				pr_warn("Test thread %d exited with status %d\n", t, ret);
		}
		mutex_unlock(&thread_lock);
	}
}

static void run_test(struct test *test)
{
	int num_threads = min(test->num_threads, MAX_TEST_THREADS);
	int duration = stress_time;
	int t;

	if (test->duration <= 0)
		duration = 0;
	else if (duration <= 0)
		duration = test->duration;

	pr_info("Running test '%s' for %d seconds\n", test->title, duration);

	test_flags = test->flags;

	for (t = 0; t < num_threads; ++t) {
		struct test_thread *thread = &test_threads[t];
		struct task_struct *task;

		mutex_lock(&thread_lock);
		thread->thread_num = t;
		task = kthread_run(test->thread_fn, thread, "scpi-test-%d", t);
		if (IS_ERR(task))
			pr_warn("Failed to create test thread %d\n", t);
		else
			thread->task = task;
		mutex_unlock(&thread_lock);
	}

	schedule_timeout_interruptible(msecs_to_jiffies(duration * 1000));

	stop_test_threads();

	show_results(test->title);
}

static struct test tests[] = {
	{"Stress All, concurrent DVFS",
		stress_all,	0,			MAX_TEST_THREADS, 60},
	{"Stress All, concurrent DVFS on different PDs",
		stress_all,	FLAG_SERIAL_PD,		MAX_TEST_THREADS, 60},
	{"Stress All, no concurrent DVFS",
		stress_all,	FLAG_SERIAL_DVFS,	MAX_TEST_THREADS, 60},
	{"Stress PMIC, concurrent DVFS",
		stress_pmic,	0,			MAX_TEST_THREADS, 60},
	{"Stress PMIC, concurrent DVFS on different PDs",
		stress_pmic,	FLAG_SERIAL_PD,		MAX_TEST_THREADS, 60},
	{"Stress PMIC, no concurrent DVFS",
		stress_pmic,	FLAG_SERIAL_DVFS,	MAX_TEST_THREADS, 60},
	{}
};

static int main_thread_fn(void *data)
{
	struct test *test = tests;
	int i = 1;

	for (; test->title && !kthread_should_stop(); ++test, ++i)
		if (run < 0 || run == i)
			run_test(test);

	run = 0;
	return 0;
}

static DEFINE_MUTEX(setup_lock);

static int setup(void)
{
	int ret = 0;

	mutex_lock(&setup_lock);

	if (!scpi) {
		int tries = 12;

		pr_info("Initial setup\n");
		while ((scpi = get_scpi_ops()) == 0 && --tries) {
			pr_info("Waiting for get_scpi_ops\n");
			msleep(5000);
		}

		if (scpi) {
			init_sensors();
			init_dvfs();
			show_results("Initial setup");
		} else {
			pr_err("Given up on get_scpi_ops\n");
			ret = -ENODEV;
		}
	}

	mutex_unlock(&setup_lock);

	return ret;
}

static int start_tests(void)
{
	struct task_struct *task;
	int ret;

	ret = setup();
	if (ret) {
		run = 0;
		return ret;
	}

	pr_info("Creating main thread\n");
	mutex_lock(&main_thread_lock);
	if (main_thread) {
		ret = -EBUSY;
	} else {
		task = kthread_run(main_thread_fn, 0, "scpi-test-main");
		if (IS_ERR(task))
			ret = PTR_ERR(task);
		else
			main_thread = task;
	}
	mutex_unlock(&main_thread_lock);

	if (ret) {
		pr_err("Failed to create main thread (%d)\n", ret);
		run = 0;
	}

	return ret;
}

static void stop_tests(void)
{
	pr_info("Stopping tests\n");
	mutex_lock(&main_thread_lock);
	if (main_thread)
		kthread_stop(main_thread);
	main_thread = NULL;
	mutex_unlock(&main_thread_lock);
}

static int param_set_running(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (!ret) {
		if (run)
			ret = start_tests();
		else
			stop_tests();
	}

	return ret;
}

static struct kernel_param_ops run_ops = {
	.set = param_set_running,
	.get = param_get_int,
};


static int scpi_test_init(void)
{
	return 0;
}

static void scpi_test_exit(void)
{
	stop_tests();
}

module_init(scpi_test_init);
module_exit(scpi_test_exit);


MODULE_AUTHOR("Jon Medhurst (Tixy) <tixy@linaro.org>");
MODULE_DESCRIPTION("ARM SCPI driver tests");
MODULE_LICENSE("GPL v2");
