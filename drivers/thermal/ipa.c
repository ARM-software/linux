/*
 *  ARM Intelligent Power Allocation
 *
 *  Copyright (C) 2013 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/uaccess.h>

#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/ipa.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/math64.h>

#include <mach/cpufreq.h>

#include "platform_tables.h"
#include "../cpufreq/cpu_load_metric.h"

#define MHZ_TO_KHZ(freq) ((freq) * 1000)
#define KHZ_TO_MHZ(freq) ((freq) / 1000)

#define NUM_CLUSTERS 2
#define MAX_GPU_FREQ 533
#define MAX_A15_FREQ 1900000
#define MAX_A7_FREQ 1400000

struct mali_utilisation_stats {
	int utilisation;
	int norm_utilisation;
	int freq_for_norm;
};

struct mali_debug_utilisation_stats {
	struct mali_utilisation_stats s;
	u32 time_busy;
	u32 time_idle;
	int time_tick;
};

struct cpu_power_info {
	unsigned int load[4];
	unsigned int freq;
	cluster_type cluster;
	unsigned int temp;
};

#define WEIGHT_SHIFT 8

struct ctlr {
	int k_po;
	int k_pu;
	int mult;
	int k_i;
	int k_d;
	int err_integral;
	int integral_reset_value;
	int err_prev;
	u32 feed_forward;
	int integral_cutoff;
	int integral_reset_threshold;
};

struct ipa_config {
	u32 control_temp;
	u32 temp_threshold;
	u32 enabled;
	u32 tdp;
	u32 boost;
	u32 ros_power;
	u32 a7_weight;
	u32 a15_weight;
	u32 gpu_weight;
	u32 a7_max_power;
	u32 a15_max_power;
	u32 gpu_max_power;
	u32 soc_max_power;
	u32 enable_ctlr;
	struct ctlr ctlr;
};

static struct ipa_config default_config = {
	.control_temp = 81,
	.temp_threshold = 30,
	.enabled = 1,
	.tdp = 3500,
	.boost = 1,
	.ros_power = 500, /* rest of soc */
	.a7_weight = 4 << WEIGHT_SHIFT,
	.a15_weight = 1 << WEIGHT_SHIFT,
	.gpu_weight = 1024,
	.a7_max_power = 250*4,
	.a15_max_power = 1638 * 4,
	.gpu_max_power = 3110,
	.enable_ctlr = 1,
	.ctlr = {
		.mult = 2,
		.k_i = 1,
		.k_d = 0,
		.feed_forward = 1,
		.integral_reset_value = 0,
		.integral_cutoff = 0,
		.integral_reset_threshold = 10,
	},
};

static BLOCKING_NOTIFIER_HEAD(thermal_notifier_list);

void gpu_ipa_dvfs_get_utilisation_stats(struct mali_debug_utilisation_stats *stats);
int gpu_ipa_dvfs_max_lock(int clock);
void gpu_ipa_dvfs_max_unlock(void);
int kbase_platform_dvfs_freq_to_power(int freq);
int kbase_platform_dvfs_power_to_freq(int power);
unsigned int get_power_value(struct cpu_power_info *power_info);
int get_ipa_dvfs_max_freq(void);

#define ARBITER_PERIOD_MSEC 100

static struct arbiter_data
{
	struct ipa_config config;
	struct ipa_sensor_conf *sensor;
	struct dentry *debugfs_root;
	bool active;
	bool initialised;

	struct mali_debug_utilisation_stats mali_stats;
	int gpu_freq, gpu_load;

	int cpu_freqs[NUM_CLUSTERS][NR_CPUS];
	struct cluster_stats cl_stats[NUM_CLUSTERS];

	int max_sensor_temp;
	int skin_temperature, cp_temperature;

	int gpu_freq_limit, cpu_freq_limits[NUM_CLUSTERS];

	struct delayed_work work;
} arbiter_data = {
	.initialised = false,
};

static void setup_cpusmasks(struct cluster_stats *cl_stats)
{
	if (!zalloc_cpumask_var(&cl_stats[CA15].mask, GFP_KERNEL))
		pr_warn("unable to allocate cpumask");
	if (!zalloc_cpumask_var(&cl_stats[CA7].mask, GFP_KERNEL))
		pr_warn("unable to allocate cpumask");

	cpumask_setall(cl_stats[CA15].mask);
	if (strlen(CONFIG_HMP_FAST_CPU_MASK))
		cpulist_parse(CONFIG_HMP_FAST_CPU_MASK, cl_stats[CA15].mask);
	else
		pr_warn("IPA: No CONFIG_HMP_FAST_CPU_MASK found.\n");

	cpumask_andnot(cl_stats[CA7].mask, cpu_present_mask, cl_stats[CA15].mask);
}

#define FRAC_BITS 8
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

static inline s64 mul_frac(s64 x, s64 y)
{
	/* note returned number is still fractional, hence one shift not two*/
	return (x * y) >> FRAC_BITS;
}

static inline int div_frac(int x, int y)
{
	return div_s64((s64)x << FRAC_BITS, y);
}

static void reset_controller(struct ctlr *ctlr_config)
{
	ctlr_config->err_integral = ctlr_config->integral_reset_value;
}

static void init_controller_coeffs(struct ipa_config *config)
{
	config->ctlr.k_po = int_to_frac(config->tdp) / config->temp_threshold;
	config->ctlr.k_pu = int_to_frac(config->ctlr.mult * config->tdp) / config->temp_threshold;
}

static void reset_arbiter_configuration(struct ipa_config *config)
{
	memcpy(config, &default_config, sizeof(*config));

	init_controller_coeffs(config);
	config->ctlr.k_i = int_to_frac(default_config.ctlr.k_i);
	config->ctlr.k_d = int_to_frac(default_config.ctlr.k_d);

	config->ctlr.integral_reset_value =
		int_to_frac(default_config.ctlr.integral_reset_value);
	reset_controller(&config->ctlr);
}

static int queue_arbiter_poll(void)
{
	int cpu, ret;

	cpu = cpumask_any(arbiter_data.cl_stats[CA7].mask);
	ret = queue_delayed_work_on(cpu, system_freezable_wq,
				&arbiter_data.work,
				msecs_to_jiffies(ARBITER_PERIOD_MSEC));

	arbiter_data.active = true;

	return ret;
}

static int get_humidity_sensor_temp(void)
{
	return 0;
}

/* Placeholder function, as sensor not present on SMDK */
static int sec_therm_get_ap_temperature(void)
{
	/* Assumption that 80 degrees at SoC = 68 degrees at AP = 55 degrees at skin */
	return (arbiter_data.max_sensor_temp - 12) * 10;
}

static void arbiter_set_gpu_freq_limit(int freq)
{
	if (arbiter_data.gpu_freq_limit == freq)
		return;

	arbiter_data.gpu_freq_limit = freq;

	gpu_ipa_dvfs_max_lock(freq);

	/*Mali DVFS code will apply changes on next DVFS tick (100ms)*/
}

static int thermal_call_chain(int freq, int idx)
{
	struct thermal_limits limits = {
		.max_freq = freq,
		.cur_freq = arbiter_data.cl_stats[idx].freq,
	};

	cpumask_copy(&limits.cpus, arbiter_data.cl_stats[idx].mask);

	return blocking_notifier_call_chain(&thermal_notifier_list, THERMAL_NEW_MAX_FREQ, &limits);
}

static void arbiter_set_cpu_freq_limit(int freq, int idx)
{
	int i, cpu, max_freq;
	/* if (arbiter_data.cpu_freq_limits[idx] == freq) */
	/*	return; */

	arbiter_data.cpu_freq_limits[idx] = freq;

	i = 0;
	max_freq = 0;
	for_each_cpu(cpu, arbiter_data.cl_stats[idx].mask) {
		if (arbiter_data.cpu_freqs[idx][i] > max_freq)
			max_freq = arbiter_data.cpu_freqs[idx][i];

		i++;
	}
	ipa_set_clamp(cpumask_any(arbiter_data.cl_stats[idx].mask), freq, max_freq);

	thermal_call_chain(freq, idx);
}

static int freq_to_power(int freq, int max, struct coefficients *coeff)
{
	int i = 0;
	while (i < (max - 1)) {
		if (coeff[i + 1].frequency > freq)
			break;
		i++;
	}

	return coeff[i].power;
}

static int power_to_freq(int power, int max, struct coefficients *coeff)
{
	int i = 0;
	while (i < (max - 1)) {
		if (coeff[i + 1].power > power)
			break;
		i++;
	}

	return coeff[i].frequency;
}

static int get_cpu_freq_limit(cluster_type cl, int power_out, int util)
{
	int nr_coeffs;
	struct coefficients *coeffs;

	if (cl == CA15) {
		coeffs = a15_cpu_coeffs;
		nr_coeffs = NR_A15_COEFFS;
	} else {
		coeffs = a7_cpu_coeffs;
		nr_coeffs = NR_A7_COEFFS;
	}

	return MHZ_TO_KHZ(power_to_freq((power_out * 100) / util, nr_coeffs, coeffs));
}

/* Powers in mW
 * TODO get rid of PUNCONSTRAINED and replace with config->aX_power_max
 */
#define PUNCONSTRAINED  (8000)

static void release_power_caps(void)
{
	int cl_idx;

	/* TODO get rid of PUNCONSTRAINED and replace with config->aX_power_max*/
	for (cl_idx = 0; cl_idx < NUM_CLUSTERS; cl_idx++) {
		int freq = get_cpu_freq_limit(cl_idx, PUNCONSTRAINED, 1);

		arbiter_set_cpu_freq_limit(freq, cl_idx);
	}

	gpu_ipa_dvfs_max_unlock();
}

struct trace_data {
	int gpu_freq_in;
	int gpu_util;
	int gpu_nutil;
	int a15_freq_in;
	int a15_util;
	int a15_nutil;
	int a7_freq_in;
	int a7_util;
	int a7_nutil;
	int Pgpu_in;
	int Pa15_in;
	int Pa7_in;
	int Pcpu_in;
	int Ptot_in;
	int Pgpu_out;
	int Pa15_out;
	int Pa7_out;
	int Pcpu_out;
	int Ptot_out;
	int t0;
	int t1;
	int t2;
	int t3;
	int t4;
	int skin_temp;
	int cp_temp;
	int currT;
	int deltaT;
	int gpu_freq_out;
	int a15_freq_out;
	int a7_freq_out;
	int gpu_freq_req;
	int a15_0_freq_in;
	int a15_1_freq_in;
	int a15_2_freq_in;
	int a15_3_freq_in;
	int a7_0_freq_in;
	int a7_1_freq_in;
	int a7_2_freq_in;
	int a7_3_freq_in;
	int Pgpu_req;
	int Pa15_req;
	int Pa7_req;
	int Pcpu_req;
	int Ptot_req;
	int extra;
};
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
static void print_trace(struct trace_data *td)
{
	trace_printk("gpu_freq_in=%d gpu_util=%d gpu_nutil=%d "
		"a15_freq_in=%d a15_util=%d a15_nutil=%d "
		"a7_freq_in=%d a7_util=%d a7_nutil=%d "
		"Pgpu_in=%d Pa15_in=%d Pa7_in=%d Pcpu_in=%d Ptot_in=%d "
		"Pgpu_out=%d Pa15_out=%d Pa7_out=%d Pcpu_out=%d Ptot_out=%d "
		"t0=%d t1=%d t2=%d t3=%d t4=%d ap_temp=%d cp_temp=%d currT=%d deltaT=%d "
		"gpu_freq_out=%d a15_freq_out=%d a7_freq_out=%d "
		"gpu_freq_req=%d "
		"a15_0_freq_in=%d "
		"a15_1_freq_in=%d "
		"a15_2_freq_in=%d "
		"a15_3_freq_in=%d "
		"a7_0_freq_in=%d "
		"a7_1_freq_in=%d "
		"a7_2_freq_in=%d "
		"a7_3_freq_in=%d "
		"Pgpu_req=%d Pa15_req=%d Pa7_req=%d Pcpu_req=%d Ptot_req=%d extra=%d\n",
		td->gpu_freq_in, td->gpu_util, td->gpu_nutil,
		td->a15_freq_in, td->a15_util, td->a15_nutil,
		td->a7_freq_in, td->a7_util, td->a7_nutil,
		td->Pgpu_in, td->Pa15_in, td->Pa7_in, td->Pcpu_in, td->Ptot_in,
		td->Pgpu_out, td->Pa15_out, td->Pa7_out, td->Pcpu_out, td->Ptot_out,
		td->t0, td->t1, td->t2, td->t3, td->t4, td->skin_temp, td->cp_temp, td->currT, td->deltaT,
		td->gpu_freq_out, td->a15_freq_out, td->a7_freq_out,
		td->gpu_freq_req,
		td->a15_0_freq_in,
		td->a15_1_freq_in,
		td->a15_2_freq_in,
		td->a15_3_freq_in,
		td->a7_0_freq_in,
		td->a7_1_freq_in,
		td->a7_2_freq_in,
		td->a7_3_freq_in,
		td->Pgpu_req, td->Pa15_req, td->Pa7_req, td->Pcpu_req, td->Ptot_req, td->extra);
}

static void print_only_temp_trace(int skin_temp)
{
	struct trace_data trace_data;
	int currT = skin_temp / 10;

	/* Initialize every int to -1 */
	memset(&trace_data, 0xff, sizeof(trace_data));

	trace_data.skin_temp = skin_temp;
	trace_data.cp_temp = get_humidity_sensor_temp();
	trace_data.currT = currT;
	trace_data.deltaT = arbiter_data.config.control_temp - currT;

	print_trace(&trace_data);
}
#else
static void print_trace(struct trace_data *td)
{
}

static void print_only_temp_trace(int skin_temp)
{
}
#endif
static void check_switch_ipa_off(int skin_temp)
{
	int currT, threshold_temp;

	currT = skin_temp / 10;
	threshold_temp = arbiter_data.config.control_temp - arbiter_data.config.temp_threshold;

	if (arbiter_data.active && currT < threshold_temp) {
		/* Switch Off */
		release_power_caps();
		arbiter_data.active = false;
		/* The caller should dequeue arbiter_poll() *if* it's queued */
	}

	if (!arbiter_data.active)
		print_only_temp_trace(skin_temp);
}

void check_switch_ipa_on(int max_temp)
{
	int skin_temp, currT, threshold_temp;

	/*
	 * IPA initialization is deferred until exynos_cpufreq is
	 * initialised, so we can't start queueing ourselves until we
	 * are initialised.
	 */
	if (!arbiter_data.initialised)
		return;

	arbiter_data.max_sensor_temp = max_temp;
	skin_temp = arbiter_data.sensor->read_skin_temperature();
	currT = skin_temp / 10;
	threshold_temp = arbiter_data.config.control_temp - arbiter_data.config.temp_threshold;

	if (!arbiter_data.active && currT > threshold_temp) {
		/* Switch On */
		/* Reset the controller before re-starting */
		reset_controller(&arbiter_data.config.ctlr);
		queue_arbiter_poll();
	}

	if (!arbiter_data.active)
		print_only_temp_trace(skin_temp);
}

#define PLIMIT_SCALAR	(20)
#define PLIMIT_NONE	(5 * PLIMIT_SCALAR)

static int F(int deltaT)
{
	/* All notional values multiplied by PLIMIT_SCALAR to avoid float */
	const int limits[] = {
		// Limit                    Temp
		// =====                    ====
		0.75 * PLIMIT_SCALAR,  //  -3 degrees or lower
		0.8  * PLIMIT_SCALAR,  //  -2
		0.9  * PLIMIT_SCALAR,  //  -1
		1    * PLIMIT_SCALAR,  //   0
		1.2  * PLIMIT_SCALAR,  //   1
		1.4  * PLIMIT_SCALAR,  //   2
		1.7  * PLIMIT_SCALAR,  //   3
		2    * PLIMIT_SCALAR,  //   4
		2.4  * PLIMIT_SCALAR,  //   5
		2.8  * PLIMIT_SCALAR,  //   6
		3.4  * PLIMIT_SCALAR,  //   7
		4    * PLIMIT_SCALAR,  //   8
		4    * PLIMIT_SCALAR,  //   9
		PLIMIT_NONE,  //  No limit  10 degrees or higher
	};

	if (deltaT < -3)
		deltaT = -3;
	if (deltaT > 10)
		deltaT = 10;

	return limits[deltaT+3];
}

static int F_ctlr(int curr)
{
	struct ipa_config *config = &arbiter_data.config;
	int setpoint = config->control_temp;
	struct ctlr *ctlr = &config->ctlr;
	s64 p, i, d, out;
	int err_int = setpoint - curr;
	int err = int_to_frac(err_int);

	/*
	 * Only integrate error if its <= integral reset threshold
	 * otherwise reset integral to reset value
	 */
	if (err_int > ctlr->integral_reset_threshold) {
		ctlr->err_integral = ctlr->integral_reset_value;
		i = mul_frac(ctlr->k_i, ctlr->err_integral);
	} else {
		/*
		 * if error less than cut off allow integration but integral
		 * is limited to max power
		 */
		i = mul_frac(ctlr->k_i, ctlr->err_integral);

		if (err_int < config->ctlr.integral_cutoff) {
			s64 tmpi = mul_frac(ctlr->k_i, err);
			tmpi += i;
			if (tmpi <= int_to_frac(config->soc_max_power)) {
				i = tmpi;
				ctlr->err_integral += err;
			}
		}
	}

	if (err_int < 0)
		p = mul_frac(ctlr->k_po, err);
	else
		p = mul_frac(ctlr->k_pu, err);

	/*
	 * dterm does err - err_prev, so with a +ve k_d, a decreasing
	 * error (which is driving closer to the line) results in less
	 * power being applied (slowing down the controller
	 */
	d = mul_frac(ctlr->k_d, err - ctlr->err_prev);
	ctlr->err_prev = err;

	out = p + i + d;
	out = frac_to_int(out);

	if (ctlr->feed_forward)
		out += config->tdp;

	/* output power must not be negative */
	if (out < 0)
		out = 0;

	/* output should not exceed max power */
	if (out > config->soc_max_power)
		out = config->soc_max_power;
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("curr=%d err=%d err_integral=%d p=%d i=%d d=%d out=%d\n",
		     curr, frac_to_int(err), frac_to_int(ctlr->err_integral),
		     (int) frac_to_int(p), (int) frac_to_int(i), (int) frac_to_int(d),
		     (int) out);
#endif
	/* the casts here should be safe */
	return (int)out;
}

static int debugfs_k_set(void *data, u64 val)
{
	*(int *)data = int_to_frac(val);
	return 0;
}
static int debugfs_k_get(void *data, u64 *val)
{
	*val = frac_to_int(*(int *)data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(k_fops, debugfs_k_get, debugfs_k_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(k_fops_ro, debugfs_k_get, NULL, "%llu\n");

static int debugfs_u32_get(void *data, u64 *val)
{
	*val = *(u32 *)data;
	return 0;
}

static void setup_debugfs_ctlr(struct ipa_config *config, struct dentry *parent)
{
	struct dentry *ctlr_d, *dentry_f;

	ctlr_d = debugfs_create_dir("ctlr", parent);
	if (IS_ERR_OR_NULL(ctlr_d))
		pr_warn("unable to create debugfs directory: ctlr\n");

	dentry_f = debugfs_create_u32("k_po", 0644, ctlr_d, &config->ctlr.k_po);

	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_po\n");

	dentry_f = debugfs_create_u32("k_pu", 0644, ctlr_d, &config->ctlr.k_pu);

	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_pu\n");

	dentry_f = debugfs_create_file("k_i", 0644, ctlr_d, &config->ctlr.k_i,
				       &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_i\n");

	dentry_f = debugfs_create_file("k_d", 0644, ctlr_d, &config->ctlr.k_d,
				       &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_d\n");

	dentry_f = debugfs_create_u32("integral_cutoff", 0644, ctlr_d,
				       &config->ctlr.integral_cutoff);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: integral_cutoff\n");

	dentry_f = debugfs_create_file("err_integral", 0644, ctlr_d,
				       &config->ctlr.err_integral, &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: err_integral\n");

	dentry_f = debugfs_create_file("integral_reset_value", 0644, ctlr_d,
				       &config->ctlr.integral_reset_value,
				       &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: integral_reset_value\n");

	dentry_f = debugfs_create_u32("integral_reset_threshold", 0644, ctlr_d,
				       &config->ctlr.integral_reset_threshold);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: integral_reset_threshold\n");
}

static int debugfs_power_set(void *data, u64 val)
{
	struct ipa_config *config = &arbiter_data.config;
	u32 old = *(u32 *)data;

	*(u32 *)data = val;

	if (config->tdp < config->ros_power) {
		*(u32 *)data = old;
		pr_warn("tdp < rest_of_soc. Reverting to previous value\n");
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(power_fops, debugfs_u32_get, debugfs_power_set, "%llu\n");

/* Shamelessly ripped from fs/debugfs/file.c */
static ssize_t read_file_bool(struct file *file, char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	char buf[3];
	u32 *val = file->private_data;

	if (*val)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

/* Shamelessly ripped from fs/debugfs/file.c */
static ssize_t __write_file_bool(struct file *file, const char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	bool bv;
	u32 *val = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0)
		*val = bv;

	return count;
}

static ssize_t debugfs_enabled_set(struct file *file, const char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret = __write_file_bool(file, user_buf, count, ppos);

	if (!arbiter_data.config.enabled)
		release_power_caps();

	return ret;
}

static const struct file_operations enabled_fops = {
	.open = simple_open,
	.read = read_file_bool,
	.write = debugfs_enabled_set,
	.llseek = default_llseek,
};

static struct dentry * setup_debugfs(struct ipa_config *config)
{
	struct dentry *ipa_d, *dentry_f;

	ipa_d = debugfs_create_dir("ipa", NULL);
	if (!ipa_d)
		pr_warn("unable to create debugfs directory: ipa\n");

	dentry_f = debugfs_create_u32("control_temp", 0644, ipa_d,
				      &config->control_temp);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: control_temp\n");

	dentry_f = debugfs_create_u32("temp_threshold", 0644, ipa_d,
				       &config->temp_threshold);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: temp_threshold\n");

	dentry_f = debugfs_create_file("enabled", 0644, ipa_d,
				&config->enabled, &enabled_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: enabled\n");

	dentry_f = debugfs_create_file("tdp", 0644, ipa_d, &config->tdp,
				       &power_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: tdp\n");

	dentry_f = debugfs_create_bool("boost", 0644, ipa_d, &config->boost);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: boost\n");

	dentry_f = debugfs_create_file("rest_of_soc_power", 0644, ipa_d,
				      &config->ros_power, &power_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: rest_of_soc_power\n");

	dentry_f = debugfs_create_u32("a7_weight", 0644, ipa_d,
				      &config->a7_weight);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: a7_weight\n");

	dentry_f = debugfs_create_u32("a15_weight", 0644, ipa_d,
				      &config->a15_weight);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: a15_weight\n");

	dentry_f = debugfs_create_u32("gpu_weight", 0644, ipa_d,
				      &config->gpu_weight);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: gpu_weight\n");

	dentry_f = debugfs_create_u32("a7_max_power", 0644, ipa_d,
				      &config->a7_max_power);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: a7_max_power\n");

	dentry_f = debugfs_create_u32("a15_max_power", 0644, ipa_d,
				      &config->a15_max_power);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: a15_max_power\n");

	dentry_f = debugfs_create_u32("gpu_max_power", 0644, ipa_d,
				      &config->gpu_max_power);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: gpu_max_power\n");

	dentry_f = debugfs_create_bool("enable_ctlr", 0644, ipa_d,
				       &config->enable_ctlr);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: enable_ctlr\n");

	setup_debugfs_ctlr(config, ipa_d);
	return ipa_d;
}

static void setup_power_tables(void)
{
	struct cpu_power_info t;
	int i;

	t.load[0] = 100; t.load[1] = t.load[2] = t.load[3] = 0;
	t.cluster = CA7;
	for (i = 0; i < NR_A7_COEFFS; i++) {
		t.freq = MHZ_TO_KHZ(a7_cpu_coeffs[i].frequency);
		a7_cpu_coeffs[i].power = get_power_value(&t);
		pr_info("cluster: %d freq: %d power=%d\n", CA7, t.freq, a7_cpu_coeffs[i].power);
	}

	t.cluster = CA15;
	for (i = 0; i < NR_A15_COEFFS; i++) {
		t.freq = MHZ_TO_KHZ(a15_cpu_coeffs[i].frequency);
		a15_cpu_coeffs[i].power = get_power_value(&t);
		pr_info("cluster: %d freq: %d power=%d\n", CA15, t.freq, a15_cpu_coeffs[i].power);
	}
}

static int read_soc_temperature(void)
{
	void *pdata = arbiter_data.sensor->private_data;

	return arbiter_data.sensor->read_soc_temperature(pdata);
}

static int get_cpu_power_req(int cl_idx, struct coefficients *coeffs, int nr_coeffs)
{
	int cpu, i, power;

	power = 0;
	i = 0;
	for_each_cpu(cpu, arbiter_data.cl_stats[cl_idx].mask) {
		int util = arbiter_data.cl_stats[cl_idx].utils[i];
		int freq = KHZ_TO_MHZ(arbiter_data.cpu_freqs[cl_idx][i]);

		power += freq_to_power(freq, nr_coeffs, coeffs) * util;

		i++;
	}

	return power;
}

static void arbiter_calc(int currT)
{
	int Pa7_req, Pa15_req;
	int Pgpu_req, Pcpu_req;
	int Ptot_req;
	int Pa7_in, Pa15_in;
	int Pgpu_in, Pcpu_in;
	int Ptot_in;
	int Prange;
	u32 Pgpu_out, Pcpu_out, Pa15_out, Pa7_out;
	int power_limit_factor = PLIMIT_NONE;
	int deltaT;
	int extra;
	int a15_util, a7_util;
	int gpu_freq_limit, cpu_freq_limits[NUM_CLUSTERS];
	struct trace_data trace_data;

	struct ipa_config *config = &arbiter_data.config;

	/*
	 * P*req are in mW
	 * In addition, P*in are weighted based on configuration as well
	 * NOTE RISK OF OVERFLOW NOT CHECKING FOR NOW BUT WILL NEED FIXING
	 */
	Pa15_req = (config->a15_weight *
		get_cpu_power_req(CA15, a15_cpu_coeffs, NR_A15_COEFFS)) >> WEIGHT_SHIFT;
	Pa7_req = (config->a7_weight *
		get_cpu_power_req(CA7, a7_cpu_coeffs, NR_A7_COEFFS)) >> WEIGHT_SHIFT;

	Pgpu_req = (config->gpu_weight
		   * kbase_platform_dvfs_freq_to_power(arbiter_data.gpu_freq)
		   * arbiter_data.gpu_load) >> WEIGHT_SHIFT;

	Pcpu_req = Pa7_req + Pa15_req;
	Ptot_req = Pcpu_req + Pgpu_req;

	BUG_ON(Pa15_req < 0);
	BUG_ON(Pa7_req < 0);
	BUG_ON(Pgpu_req < 0);

	/*
	 * Calculate the model values to observe power in the previous window
	 */
	Pa15_in = freq_to_power(KHZ_TO_MHZ(arbiter_data.cl_stats[CA15].freq), NR_A15_COEFFS, a15_cpu_coeffs) * arbiter_data.cl_stats[CA15].util;
	Pa7_in = freq_to_power(KHZ_TO_MHZ(arbiter_data.cl_stats[CA7].freq), NR_A7_COEFFS, a7_cpu_coeffs) * arbiter_data.cl_stats[CA7].util;
	Pgpu_in = kbase_platform_dvfs_freq_to_power(arbiter_data.mali_stats.s.freq_for_norm) * arbiter_data.gpu_load;

	Pcpu_in = Pa7_in + Pa15_in;
	Ptot_in = Pcpu_in + Pgpu_in;

	deltaT = config->control_temp - currT;

	extra = 0;

	if (config->enable_ctlr) {
		Prange = F_ctlr(currT);

		Prange = max(Prange - (int)config->ros_power, 0);
	} else {
		power_limit_factor = F(deltaT);
		BUG_ON(power_limit_factor < 0);
		if (!config->boost && (power_limit_factor > PLIMIT_SCALAR))
			power_limit_factor = PLIMIT_SCALAR;

		/*
		 * If within the control zone, or boost is disabled
		 * constrain power.
		 *
		 * Prange (mW) = tdp (mW) - ROS (mW)
		 */
		Prange = config->tdp - config->ros_power;

		/*
		 * Prange divided to compensate for PLIMIT_SCALAR
		 */
		Prange = (Prange * power_limit_factor) / PLIMIT_SCALAR;
	}

	BUG_ON(Prange < 0);

	if (!config->enable_ctlr && power_limit_factor == PLIMIT_NONE) {
		/*
		 * If we are outside the control zone, and boost is enabled
		 * run un-constrained.
		 */
		/* TODO get rid of PUNCONSTRAINED and replace with config->aX_power_max*/
		Pa15_out = PUNCONSTRAINED;
		Pa7_out  = PUNCONSTRAINED;
		Pgpu_out = PUNCONSTRAINED;
	} else {
		/*
		 * Pcpu_out (mw) = Prange (mW) * (Pcpu_req / Ptot_req) (10uW) * power_limit_factor
		 * Pgpu_out (mw) = Prange (mW) * (Pgpu_req / Ptot_req) (10uW) * power_limit_factor
		 *
		 * Note: the (10uW) variables get cancelled and result is in mW
		 */
		if (Ptot_req != 0) {
			u64 tmp;
			/*
			 * Divvy-up the available power (Prange) in proportion to
			 * the weighted request.
			 */
			tmp = Pa15_req;
			tmp *= Prange;
			tmp = div_u64(tmp, (u32) Ptot_req);
			Pa15_out = (int) tmp;

			tmp = Pa7_req;
			tmp *= Prange;
			tmp = div_u64(tmp, (u32) Ptot_req);
			Pa7_out = (int) tmp;

			tmp = Pgpu_req;
			tmp *= Prange;
			tmp = div_u64(tmp, (u32) Ptot_req);
			Pgpu_out = (int) tmp;

			/*
			 * Do we exceed the max for any of the actors?
			 * Reclaim the extra and update the denominator to
			 * exclude that actor
			 */
			if (Pa7_out > config->a7_max_power) {
				extra += Pa7_out - config->a7_max_power;
				Pa7_out = config->a7_max_power;
			}

			if (Pa15_out > config->a15_max_power) {
				extra += Pa15_out - config->a15_max_power;
				Pa15_out = config->a15_max_power;
			}

			if (Pgpu_out > config->gpu_max_power) {
				extra += Pgpu_out - config->gpu_max_power;
				Pgpu_out = config->gpu_max_power;
			}

			/*
			 * Re-divvy the reclaimed extra among actors base on
			 * how far they are from the max
			 */
			if (extra > 0) {
				int dista7  = config->a7_max_power - Pa7_out;
				int dista15 = config->a15_max_power - Pa15_out;
				int distgpu = config->gpu_max_power - Pgpu_out;
				int capped_extra = dista7 + dista15 + distgpu;

				extra = min(extra, capped_extra);
				if (capped_extra > 0) {
					Pa7_out += (dista7 * extra) / capped_extra;
					Pa15_out += (dista15 * extra) / capped_extra;
					Pgpu_out += (distgpu * extra) / capped_extra;
				}
			}

		} else {	/* Avoid divide by zero */
			Pa15_out = config->a15_max_power;
			Pa7_out  = config->a7_max_power;
			Pgpu_out = config->gpu_max_power;
		}
	}

	Pa7_out = min(Pa7_out, config->a7_max_power);
	Pa15_out = min(Pa15_out, config->a15_max_power);
	Pgpu_out = min(Pgpu_out, config->gpu_max_power);

	Pcpu_out = Pa15_out + Pa7_out;

	a15_util = arbiter_data.cl_stats[CA15].util > 0 ? arbiter_data.cl_stats[CA15].util : 1;
	a7_util  = arbiter_data.cl_stats[CA7].util > 0 ? arbiter_data.cl_stats[CA7].util : 1;
	/*
	 * Output Power per cpu (mW) = Pcpu_out (mw) * ( 100 / util ) - where 0 < util < 400
	 */
	cpu_freq_limits[CA15] = get_cpu_freq_limit(CA15, Pa15_out, a15_util);
	cpu_freq_limits[CA7] = get_cpu_freq_limit(CA7, Pa7_out, a7_util);

	gpu_freq_limit = kbase_platform_dvfs_power_to_freq(Pgpu_out);

	trace_data.gpu_freq_in = arbiter_data.mali_stats.s.freq_for_norm;
	trace_data.gpu_util = arbiter_data.mali_stats.s.utilisation;
	trace_data.gpu_nutil = arbiter_data.mali_stats.s.norm_utilisation;
	trace_data.a15_freq_in = KHZ_TO_MHZ(arbiter_data.cl_stats[CA15].freq);
	trace_data.a15_util = arbiter_data.cl_stats[CA15].util;
	trace_data.a15_nutil = (arbiter_data.cl_stats[CA15].util * arbiter_data.cl_stats[CA15].freq) / MAX_A15_FREQ;
	trace_data.a7_freq_in = KHZ_TO_MHZ(arbiter_data.cl_stats[CA7].freq);
	trace_data.a7_util = arbiter_data.cl_stats[CA7].util;
	trace_data.a7_nutil = (arbiter_data.cl_stats[CA7].util * arbiter_data.cl_stats[CA7].freq) / MAX_A7_FREQ;
	trace_data.Pgpu_in = Pgpu_in / 100;
	trace_data.Pa15_in =  Pa15_in / 100;
	trace_data.Pa7_in = Pa7_in / 100;
	trace_data.Pcpu_in = Pcpu_in / 100;
	trace_data.Ptot_in = Ptot_in / 100;
	trace_data.Pgpu_out = Pgpu_out;
	trace_data.Pa15_out =  Pa15_out;
	trace_data.Pa7_out = Pa7_out;
	trace_data.Pcpu_out = Pcpu_out;
	trace_data.Ptot_out = Pgpu_out + Pcpu_out;
	trace_data.skin_temp = arbiter_data.skin_temperature;
	trace_data.t0 = -1;
	trace_data.t1 = -1;
	trace_data.t2 = -1;
	trace_data.t3 = -1;
	trace_data.t4 = -1;
	trace_data.cp_temp = arbiter_data.cp_temperature;
	trace_data.currT = currT;
	trace_data.deltaT = deltaT;
	trace_data.gpu_freq_out = gpu_freq_limit;
	trace_data.a15_freq_out = KHZ_TO_MHZ(cpu_freq_limits[CA15]);
	trace_data.a7_freq_out = KHZ_TO_MHZ(cpu_freq_limits[CA7]);
	trace_data.gpu_freq_req = arbiter_data.gpu_freq;
	trace_data.a15_0_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA15][0]);
	trace_data.a15_1_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA15][1]);
	trace_data.a15_2_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA15][2]);
	trace_data.a15_3_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA15][3]);
	trace_data.a7_0_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA7][0]);
	trace_data.a7_1_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA7][1]);
	trace_data.a7_2_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA7][2]);
	trace_data.a7_3_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CA7][3]);
	trace_data.Pgpu_req = Pgpu_req;
	trace_data.Pa15_req = Pa15_req;
	trace_data.Pa7_req = Pa7_req;
	trace_data.Pcpu_req = Pcpu_req;
	trace_data.Ptot_req = Ptot_req;
	trace_data.extra = extra;

#ifdef CONFIG_CPU_THERMAL_IPA_CONTROL
	if (config->enabled) {
		arbiter_set_cpu_freq_limit(cpu_freq_limits[CA15], CA15);
		arbiter_set_cpu_freq_limit(cpu_freq_limits[CA7], CA7);
		arbiter_set_gpu_freq_limit(gpu_freq_limit);
	}
#else
#error "Turn on CONFIG_CPU_THERMAL_IPA_CONTROL to enable IPA control"
#endif

	print_trace(&trace_data);
}

static void arbiter_poll(struct work_struct *work)
{
	gpu_ipa_dvfs_get_utilisation_stats(&arbiter_data.mali_stats);

	arbiter_data.gpu_load = arbiter_data.mali_stats.s.utilisation;

	get_cluster_stats(arbiter_data.cl_stats);

	arbiter_data.max_sensor_temp = read_soc_temperature();
	arbiter_data.skin_temperature = arbiter_data.sensor->read_skin_temperature();
	arbiter_data.cp_temperature = get_humidity_sensor_temp();

	check_switch_ipa_off(arbiter_data.skin_temperature);
	if (!arbiter_data.active)
		return;

	arbiter_calc(arbiter_data.skin_temperature/10);

	queue_arbiter_poll();
}

static int get_cluster_from_cpufreq_policy(struct cpufreq_policy *policy)
{
	return policy->cpu > 3 ? CA15 : CA7;
}

void ipa_cpufreq_requested(struct cpufreq_policy *policy, unsigned int *freqs)
{
	int cl_idx, i;
	unsigned int cpu;

	cl_idx = get_cluster_from_cpufreq_policy(policy);

	i = 0;
	for_each_cpu(cpu, arbiter_data.cl_stats[cl_idx].mask) {
		arbiter_data.cpu_freqs[cl_idx][i] = freqs[i];

		i++;
	}
}

int ipa_register_thermal_sensor(struct ipa_sensor_conf *sensor)
{
	if (!sensor || !sensor->read_soc_temperature) {
		pr_err("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	arbiter_data.sensor = sensor;

	if (!arbiter_data.sensor->read_skin_temperature)
		arbiter_data.sensor->read_skin_temperature = sec_therm_get_ap_temperature;

	return 0;
}

void ipa_mali_dvfs_requested(unsigned int freq)
{
	arbiter_data.gpu_freq = freq;
}

int thermal_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&thermal_notifier_list, nb);
}

int thermal_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&thermal_notifier_list, nb);
}

extern bool exynos_cpufreq_init_done;
static struct delayed_work init_work;

static void arbiter_init(struct work_struct *work)
{
	int i;

	if (!exynos_cpufreq_init_done) {
		pr_info("exynos_cpufreq not initialized. Deferring again...\n");
		queue_delayed_work(system_freezable_wq, &init_work,
				msecs_to_jiffies(500));
		return;
	}

	arbiter_data.gpu_freq_limit = MAX_GPU_FREQ;
	arbiter_data.cpu_freq_limits[CA15] = MAX_A15_FREQ;
	arbiter_data.cpu_freq_limits[CA7] = MAX_A7_FREQ;
	for (i = 0; i < NR_CPUS; i++) {
		arbiter_data.cpu_freqs[CA15][i] = 1900000;
		arbiter_data.cpu_freqs[CA7][i] = 1400000;
	}

	setup_cpusmasks(arbiter_data.cl_stats);

	reset_arbiter_configuration(&arbiter_data.config);
	arbiter_data.debugfs_root = setup_debugfs(&arbiter_data.config);
	setup_power_tables();

	/* reconfigure max */
	arbiter_data.config.a7_max_power = freq_to_power(KHZ_TO_MHZ(arbiter_data.cpu_freq_limits[CA7]),
							NR_A7_COEFFS, a7_cpu_coeffs) * cpumask_weight(arbiter_data.cl_stats[CA7].mask);

	arbiter_data.config.a15_max_power = freq_to_power(KHZ_TO_MHZ(arbiter_data.cpu_freq_limits[CA15]),
							NR_A15_COEFFS, a15_cpu_coeffs) * cpumask_weight(arbiter_data.cl_stats[CA15].mask);

	arbiter_data.config.gpu_max_power = kbase_platform_dvfs_freq_to_power(arbiter_data.gpu_freq_limit);

	arbiter_data.config.soc_max_power = arbiter_data.config.gpu_max_power +
		arbiter_data.config.a15_max_power +
		arbiter_data.config.gpu_max_power;
	/* TODO when we introduce dynamic RoS power we need
	   to add a ros_max_power !! */
	arbiter_data.config.soc_max_power += arbiter_data.config.ros_power;

	INIT_DELAYED_WORK(&arbiter_data.work, arbiter_poll);

	arbiter_data.initialised = true;
	queue_arbiter_poll();

	pr_info("Intelligent Power Allocation initialised.\n");
}

static int ipa_init(void)
{
	INIT_DELAYED_WORK(&init_work, arbiter_init);
	queue_delayed_work(system_freezable_wq, &init_work, msecs_to_jiffies(500));

	pr_info("Deferring initialisation...");

	return 0;
}
late_initcall(ipa_init);

MODULE_DESCRIPTION("Intelligent Power Allocation Driver");
