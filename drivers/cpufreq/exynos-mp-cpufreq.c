/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - MP CPU frequency scaling support for EXYNOS big.Little series
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpumask.h>

#include <asm/smp_plat.h>
#include <asm/cputype.h>

#include <mach/cpufreq.h>
#include <mach/asv-exynos.h>
#include <mach/regs-pmu.h>
#include <mach/tmu.h>
#include <plat/cpu.h>

#define POWER_COEFF_15P		57 /* percore param */
#define POWER_COEFF_7P		11 /* percore  param */

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long   ref;
	unsigned int    freq;
};

static struct lpj_info global_lpj_ref;
#endif

/* For switcher */
static unsigned int freq_min[CA_END] __read_mostly;	/* Minimum (Big/Little) clock frequency */
static unsigned int freq_max[CA_END] __read_mostly;	/* Maximum (Big/Little) clock frequency */

static struct exynos_dvfs_info *exynos_info[CA_END];
static struct exynos_dvfs_info exynos_info_CA7;
static struct exynos_dvfs_info exynos_info_CA15;

static struct regulator *arm_regulator;
static struct regulator *kfc_regulator;
static unsigned int volt_offset;

static struct cpufreq_freqs *freqs[CA_END];

static DEFINE_MUTEX(cpufreq_lock);
static DEFINE_MUTEX(cpufreq_scale_lock);

bool exynos_cpufreq_init_done;
static bool suspend_prepared = false;
static bool hmp_boosted = false;
static bool egl_hotplugged = false;
bool cluster_on[CA_END] = {true, };

/* Include CPU mask of each cluster */
cluster_type exynos_boot_cluster;
static cluster_type boot_cluster;
static struct cpumask cluster_cpus[CA_END];

DEFINE_PER_CPU(cluster_type, cpu_cur_cluster);

static struct pm_qos_constraints max_cpu_qos_const;
static struct pm_qos_constraints max_kfc_qos_const;

static struct pm_qos_request boot_min_cpu_qos;
static struct pm_qos_request boot_max_cpu_qos;
static struct pm_qos_request boot_min_kfc_qos;
static struct pm_qos_request boot_max_kfc_qos;
static struct pm_qos_request min_cpu_qos;
static struct pm_qos_request max_cpu_qos;
static struct pm_qos_request min_kfc_qos;
static struct pm_qos_request max_kfc_qos;
static struct pm_qos_request exynos_mif_qos_CA7;
static struct pm_qos_request exynos_mif_qos_CA15;
static struct pm_qos_request ipa_max_kfc_qos;
static struct pm_qos_request ipa_max_cpu_qos;

static struct workqueue_struct *cluster_monitor_wq;
static struct delayed_work monitor_cluster_on;
static bool CA7_cluster_on = false;
static bool CA15_cluster_on = false;

/*
 * CPUFREQ init notifier
 */
static BLOCKING_NOTIFIER_HEAD(exynos_cpufreq_init_notifier_list);

int exynos_cpufreq_init_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&exynos_cpufreq_init_notifier_list, nb);
}
EXPORT_SYMBOL(exynos_cpufreq_init_register_notifier);

int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&exynos_cpufreq_init_notifier_list, nb);
}
EXPORT_SYMBOL(exynos_cpufreq_init_unregister_notifier);

static int exynos_cpufreq_init_notify_call_chain(unsigned long val)
{
	int ret = blocking_notifier_call_chain(&exynos_cpufreq_init_notifier_list, val, NULL);

	return notifier_to_errno(ret);
}

static unsigned int get_limit_voltage(unsigned int voltage)
{
	BUG_ON(!voltage);
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	if (ENABLE_MIN_COLD && volt_offset
		&& (voltage + volt_offset) < MIN_COLD_VOLTAGE)
		return MIN_COLD_VOLTAGE;

	return voltage + volt_offset;
}

static void init_cpumask_cluster_set(cluster_type cluster)
{
	unsigned int i;

	for_each_cpu(i, cpu_possible_mask) {
		if (cluster == CA7) {
			if (i >= NR_CA7) {
				cpumask_set_cpu(i, &cluster_cpus[CA15]);
				per_cpu(cpu_cur_cluster, i) = CA15;
			} else {
				cpumask_set_cpu(i, &cluster_cpus[CA7]);
				per_cpu(cpu_cur_cluster, i) = CA7;
			}
		} else {
			if (i >= NR_CA15) {
				cpumask_set_cpu(i, &cluster_cpus[CA7]);
				per_cpu(cpu_cur_cluster, i) = CA7;
			} else {
				cpumask_set_cpu(i, &cluster_cpus[CA15]);
				per_cpu(cpu_cur_cluster, i) = CA15;
			}
		}
	}
}

/*
 * get_cur_cluster - return current cluster
 *
 * You may reference this fuction directly, but it cannot be
 * standard of judging current cluster. If you make a decision
 * of operation by this function, it occurs system hang.
 */
static cluster_type get_cur_cluster(unsigned int cpu)
{
	return per_cpu(cpu_cur_cluster, cpu);
}

static void set_boot_freq(void)
{
	int i;

	for (i = 0; i < CA_END; i++) {
		if (exynos_info[i] == NULL)
			continue;

		exynos_info[i]->boot_freq
				= clk_get_rate(exynos_info[i]->cpu_clk) / 1000;
	}
}

static void cluster_onoff_monitor(struct work_struct *work)
{
	struct cpufreq_frequency_table *CA15_freq_table = exynos_info[CA15]->freq_table;
	struct cpufreq_frequency_table *CA7_freq_table = exynos_info[CA7]->freq_table;
	unsigned int CA15_old_index = 0, CA7_old_index = 0;
	unsigned int freq;
	int i;

	if (exynos_info[CA15]->is_alive)
		cluster_on[CA15] = exynos_info[CA15]->is_alive();

	if (exynos_info[CA15]->bus_table && exynos_info[CA15]->is_alive) {
		if (!exynos_info[CA15]->is_alive() && CA15_cluster_on) {
			pm_qos_update_request(&exynos_mif_qos_CA15, 0);
			CA15_cluster_on = false;
		} else if (exynos_info[CA15]->is_alive() && !CA15_cluster_on) {
			for (i = 0; (CA15_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				freq = CA15_freq_table[i].frequency;
				if (freq == CPUFREQ_ENTRY_INVALID)
					continue;
				if (freqs[CA15]->old == freq) {
					CA15_old_index = i;
					break;
				}
			}

			pm_qos_update_request(&exynos_mif_qos_CA15,
					exynos_info[CA15]->bus_table[CA15_old_index]);
			CA15_cluster_on = true;
		}
	}

	if (exynos_info[CA7]->is_alive)
		cluster_on[CA7] = exynos_info[CA7]->is_alive();

	if (exynos_info[CA7]->bus_table && exynos_info[CA7]->is_alive) {
		if (!exynos_info[CA7]->is_alive() && CA7_cluster_on) {
			pm_qos_update_request(&exynos_mif_qos_CA7, 0);
			CA7_cluster_on = false;
		} else if (exynos_info[CA7]->is_alive() && !CA7_cluster_on) {
			for (i = 0; (CA7_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				freq = CA7_freq_table[i].frequency;
				if (freq == CPUFREQ_ENTRY_INVALID)
					continue;
				if (freqs[CA7]->old == freq) {
					CA7_old_index = i;
					break;
				}
			}

			pm_qos_update_request(&exynos_mif_qos_CA7,
					exynos_info[CA7]->bus_table[CA7_old_index]);
			CA7_cluster_on = true;
		}
	}

	queue_delayed_work_on(0, cluster_monitor_wq, &monitor_cluster_on, msecs_to_jiffies(100));
}

static unsigned int get_freq_volt(int cluster, unsigned int target_freq)
{
	int index;
	int i;

	struct cpufreq_frequency_table *table = exynos_info[cluster]->freq_table;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END) {
		if (exynos_info[cluster]->boot_freq_idx != -1 &&
			target_freq == exynos_info[cluster]->boot_freq)
			index = exynos_info[cluster]->boot_freq_idx;
		else
			return -EINVAL;
	}

	return exynos_info[cluster]->volt_table[index];
}

static unsigned int get_boot_freq(unsigned int cluster)
{
	if (exynos_info[cluster] == NULL)
		return 0;

	return exynos_info[cluster]->boot_freq;
}

static unsigned int get_boot_volt(int cluster)
{
	unsigned int boot_freq = get_boot_freq(cluster);

	return get_freq_volt(cluster, boot_freq);
}

int exynos_verify_speed(struct cpufreq_policy *policy)
{
	unsigned int cur = get_cur_cluster(policy->cpu);

	return cpufreq_frequency_table_verify(policy,
				exynos_info[cur]->freq_table);
}

unsigned int exynos_getspeed_cluster(cluster_type cluster)
{
	return clk_get_rate(exynos_info[cluster]->cpu_clk) / 1000;
}

unsigned int exynos_getspeed(unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);

	return exynos_getspeed_cluster(cur);
}

static unsigned int exynos_get_safe_volt(unsigned int old_index,
					unsigned int new_index,
					unsigned int cur)
{
	unsigned int safe_arm_volt = 0;
	struct cpufreq_frequency_table *freq_table
					= exynos_info[cur]->freq_table;
	unsigned int *volt_table = exynos_info[cur]->volt_table;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * reguired voltage level
	 */
	if (exynos_info[cur]->need_apll_change != NULL) {
		if (exynos_info[cur]->need_apll_change(old_index, new_index) &&
			(freq_table[new_index].frequency < exynos_info[cur]->mpll_freq_khz) &&
			(freq_table[old_index].frequency < exynos_info[cur]->mpll_freq_khz)) {
				safe_arm_volt = volt_table[exynos_info[cur]->pll_safe_idx];
		}
	}

	return safe_arm_volt;
}

/* Determine valid target frequency using freq_table */
int exynos5_frequency_table_target(struct cpufreq_policy *policy,
				   struct cpufreq_frequency_table *table,
				   unsigned int target_freq,
				   unsigned int relation,
				   unsigned int *index)
{
	unsigned int i;

	if (!cpu_online(policy->cpu))
		return -EINVAL;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			*index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END) {
		unsigned int cur = get_cur_cluster(policy->cpu);
		if (exynos_info[cur]->boot_freq_idx != -1 &&
			target_freq == exynos_info[cur]->boot_freq)
			*index = exynos_info[cur]->boot_freq_idx;
		else
			return -EINVAL;
	}

	return 0;
}

static int exynos_cpufreq_scale(unsigned int target_freq,
				unsigned int curr_freq, unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);
	struct cpufreq_frequency_table *freq_table = exynos_info[cur]->freq_table;
	unsigned int *volt_table = exynos_info[cur]->volt_table;
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	struct regulator *regulator = exynos_info[cur]->regulator;
	unsigned int new_index, old_index;
	unsigned int volt, safe_volt = 0;
	bool set_abb_first_than_volt = false;
	int ret = 0;

	if (!policy) {
		ret = -EINVAL;
		goto no_policy;
	}

	freqs[cur]->cpu = cpu;
	freqs[cur]->new = target_freq;

	if (exynos5_frequency_table_target(policy, freq_table,
				curr_freq, CPUFREQ_RELATION_L, &old_index)) {
		ret = -EINVAL;
		goto out;
	}

	if (exynos5_frequency_table_target(policy, freq_table,
				freqs[cur]->new, CPUFREQ_RELATION_L, &new_index)) {
		ret = -EINVAL;
		goto out;
	}

	if (old_index == new_index)
		goto out;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * required voltage level
	 */
	safe_volt = exynos_get_safe_volt(old_index, new_index, cur);
	if (safe_volt)
		safe_volt = get_limit_voltage(safe_volt);

	volt = get_limit_voltage(volt_table[new_index]);

	/* Update policy current frequency */
	cpufreq_notify_transition(policy, freqs[cur], CPUFREQ_PRECHANGE);

	if (freqs[cur]->new > freqs[cur]->old) {
		if (exynos_info[cur]->set_int_skew)
			exynos_info[cur]->set_int_skew(new_index);
	}

	if (exynos_info[cur]->abb_table) {
		if (cur == CA7)
			set_abb_first_than_volt = is_set_abb_first(ID_KFC, curr_freq, target_freq);
		else
			set_abb_first_than_volt = is_set_abb_first(ID_ARM, curr_freq, target_freq);
	}

	/* When the new frequency is higher than current frequency */
	if ((freqs[cur]->new > freqs[cur]->old) && !safe_volt){
		/* Firstly, voltage up to increase frequency */
		if (!set_abb_first_than_volt)
			regulator_set_voltage(regulator, volt, volt);

		if (exynos_info[cur]->abb_table) {
			if (cur == CA7)
				set_match_abb(ID_KFC, exynos_info[cur]->abb_table[new_index]);
			else
				set_match_abb(ID_ARM, exynos_info[cur]->abb_table[new_index]);
		}

		if (set_abb_first_than_volt)
			regulator_set_voltage(regulator, volt, volt);

		if (exynos_info[cur]->set_ema)
			exynos_info[cur]->set_ema(volt);
	}

	if (safe_volt) {
		if (!set_abb_first_than_volt)
			regulator_set_voltage(regulator, safe_volt, safe_volt);

		if (exynos_info[cur]->abb_table) {
			if (cur == CA7)
				set_match_abb(ID_KFC, exynos_info[cur]->abb_table[new_index]);
			else
				set_match_abb(ID_ARM, exynos_info[cur]->abb_table[new_index]);
		}

		if (set_abb_first_than_volt)
			regulator_set_voltage(regulator, volt, volt);

		if (exynos_info[cur]->set_ema)
			exynos_info[cur]->set_ema(safe_volt);
	}

	if (old_index > new_index) {
		if (cur == CA15) {
			if (pm_qos_request_active(&exynos_mif_qos_CA15))
				pm_qos_update_request(&exynos_mif_qos_CA15,
						exynos_info[cur]->bus_table[new_index]);
		} else {
			if (pm_qos_request_active(&exynos_mif_qos_CA7))
				pm_qos_update_request(&exynos_mif_qos_CA7,
						exynos_info[cur]->bus_table[new_index]);
		}
	}

	exynos_info[cur]->set_freq(old_index, new_index);

	if (old_index < new_index) {
		if (cur == CA15) {
			if (pm_qos_request_active(&exynos_mif_qos_CA15))
				pm_qos_update_request(&exynos_mif_qos_CA15,
						exynos_info[cur]->bus_table[new_index]);
		} else {
			if (pm_qos_request_active(&exynos_mif_qos_CA7))
				pm_qos_update_request(&exynos_mif_qos_CA7,
						exynos_info[cur]->bus_table[new_index]);
		}
	}

#ifdef CONFIG_SMP
	if (!global_lpj_ref.freq) {
		global_lpj_ref.ref = loops_per_jiffy;
		global_lpj_ref.freq = freqs[cur]->old;
	}

	loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref,
			global_lpj_ref.freq, freqs[cur]->new);
#endif

	cpufreq_notify_transition(policy, freqs[cur], CPUFREQ_POSTCHANGE);

	/* When the new frequency is lower than current frequency */
	if ((freqs[cur]->new < freqs[cur]->old) ||
		((freqs[cur]->new > freqs[cur]->old) && safe_volt)) {
		/* down the voltage after frequency change */
		if (exynos_info[cur]->set_ema)
			 exynos_info[cur]->set_ema(volt);

		if (exynos_info[cur]->abb_table) {
			if (cur == CA7)
				set_abb_first_than_volt = is_set_abb_first(ID_KFC, freqs[cur]->old, freqs[cur]->new);
			else
				set_abb_first_than_volt = is_set_abb_first(ID_ARM, freqs[cur]->old, freqs[cur]->new);
		}

		if (!set_abb_first_than_volt)
			regulator_set_voltage(regulator, volt, volt);
		if (exynos_info[cur]->abb_table) {
			if (cur == CA7)
				set_match_abb(ID_KFC, exynos_info[cur]->abb_table[new_index]);
			else
				set_match_abb(ID_ARM, exynos_info[cur]->abb_table[new_index]);
		}
		if (set_abb_first_than_volt)
			regulator_set_voltage(regulator, volt, volt);
	}

	if (freqs[cur]->new < freqs[cur]->old) {
		if (exynos_info[cur]->set_int_skew)
			exynos_info[cur]->set_int_skew(new_index);
	}

out:
	cpufreq_cpu_put(policy);
no_policy:
	return ret;
}

static void __exynos_set_max_freq(struct pm_qos_request *pm_qos_request, int class, int max_freq)
{
	if (pm_qos_request_active(pm_qos_request))
		pm_qos_update_request(pm_qos_request, max_freq);
	else
		pm_qos_add_request(pm_qos_request, class, max_freq);
}

void exynos_set_max_freq(int max_freq, unsigned int cpu)
{
	struct pm_qos_request *req;
	int class;

	if (get_cur_cluster(cpu) == CA7) {
		req = &ipa_max_kfc_qos;
		class = PM_QOS_KFC_FREQ_MAX;
	} else {
		req = &ipa_max_cpu_qos;
		class = PM_QOS_CPU_FREQ_MAX;
	}

	__exynos_set_max_freq(req, class, max_freq);
}

struct cpu_power_info {
	unsigned int load[4];
	unsigned int freq;
	cluster_type cluster;
	unsigned int temp;
};

unsigned int get_power_value(struct cpu_power_info *power_info)
{
	/* ps : power_static (mW)
         * pd : power_dynamic (mW)
	 */
	u64 temp;
	u64 pd_tot;
	unsigned int ps, total_power;
	unsigned int volt, maxfreq;
	int i, coeff;
	struct cpufreq_frequency_table *freq_table;
	unsigned int *volt_table;
	unsigned int freq = power_info->freq / 10000;

	if (power_info->cluster == CA15) {
		coeff = POWER_COEFF_15P;
		freq_table = exynos_info[CA15]->freq_table;
		volt_table = exynos_info[CA15]->volt_table;
		maxfreq = exynos_info[CA15]->freq_table[exynos_info[CA15]->max_support_idx].frequency;
	} else if (power_info->cluster == CA7) {
		coeff = POWER_COEFF_7P;
		freq_table = exynos_info[CA7]->freq_table;
		volt_table = exynos_info[CA7]->volt_table;
		maxfreq = exynos_info[CA7]->freq_table[exynos_info[CA7]->max_support_idx].frequency;
	} else {
		BUG_ON(1);
	}

	if (power_info->freq < freq_min[power_info->cluster]) {
		pr_warn("%s: freq: %d for cluster: %d is less than min. freq: %d\n",
			__func__, power_info->freq, power_info->cluster,
			freq_min[power_info->cluster]);
		power_info->freq = freq_min[power_info->cluster];
	}

	if (power_info->freq > maxfreq) {
		pr_warn("%s: freq: %d for cluster: %d is greater than max. freq: %d\n",
			__func__, power_info->freq, power_info->cluster,
			maxfreq);
		power_info->freq = maxfreq;
	}

	for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (power_info->freq == freq_table[i].frequency)
			break;
	}

	volt = volt_table[i] / 10000;

	/* TODO: pde_p is not linear to load, need to update equation */
	pd_tot = 0;
	temp = (u64)coeff * (u64)freq * (u64)volt * (u64)volt;
	do_div(temp, 100000);
	for (i = 0; i < 4; i++) {
		if (power_info->load[i] > 100) {
			pr_err("%s: Load should be a percent value\n", __func__);
			BUG_ON(1);
		}
		pd_tot += temp * (power_info->load[i]+1);
	}
	total_power = (unsigned int)pd_tot / (unsigned int)100;

	/* pse = alpha ~ volt ~ temp */
	/* TODO: Update voltage, temperature variant PSE */
	ps = 0;

	total_power += ps;

	return total_power;
}

unsigned int g_cpufreq;
unsigned int g_kfcfreq;
unsigned int g_clamp_cpufreqs[CA_END];

/* Set clock frequency */
static int exynos_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	cluster_type cur = get_cur_cluster(policy->cpu);
	struct cpufreq_frequency_table *freq_table = exynos_info[cur]->freq_table;
	unsigned int index;
	int ret = 0;
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("IPA:%s:%d Called by %x, with target_freq %d", __PRETTY_FUNCTION__, __LINE__,
			(unsigned int) __builtin_return_address (0), target_freq);
#endif
	mutex_lock(&cpufreq_lock);

	if (exynos_info[cur]->blocked)
		goto out;

	if (target_freq == 0)
		target_freq = policy->min;

	/* if PLL bypass, frequency scale is skip */
	if (exynos_getspeed(policy->cpu) <= 24000)
		goto out;

	/* verify old frequency */
	if (freqs[cur]->old != exynos_getspeed(policy->cpu)) {
		printk("oops, sombody change clock  old clk:%d, cur clk:%d \n", freqs[cur]->old, exynos_getspeed(policy->cpu));
		BUG_ON(freqs[cur]->old != exynos_getspeed(policy->cpu));
	}

	if (cur == CA15) {
		target_freq = max((unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MIN), target_freq);
		target_freq = min((unsigned int)pm_qos_request(PM_QOS_CPU_FREQ_MAX), target_freq);
		target_freq = min(g_clamp_cpufreqs[CA15], target_freq); /* add IPA clamp */
	} else {
		target_freq = max((unsigned int)pm_qos_request(PM_QOS_KFC_FREQ_MIN), target_freq);
		target_freq = min((unsigned int)pm_qos_request(PM_QOS_KFC_FREQ_MAX), target_freq);
		target_freq = min(g_clamp_cpufreqs[CA7], target_freq); /* add IPA clamp */
	}

#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("IPA:%s:%d will apply %d ", __PRETTY_FUNCTION__, __LINE__, target_freq);
#endif

	if (cpufreq_frequency_table_target(policy, freq_table,
				target_freq, relation, &index)) {
		ret = -EINVAL;
		goto out;
	}

	target_freq = freq_table[index].frequency;

	pr_debug("%s[%d]: new_freq[%d], index[%d]\n",
				__func__, cur, target_freq, index);

	/* frequency and volt scaling */
	ret = exynos_cpufreq_scale(target_freq, freqs[cur]->old, policy->cpu);

	/* save current frequency */
	freqs[cur]->old = target_freq;

out:
	mutex_unlock(&cpufreq_lock);

	if (cur == CA15)
		g_cpufreq = target_freq;
	else
		g_kfcfreq = target_freq;

	return ret;
}

void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target)
{
	unsigned int freq = 0;
	unsigned int new_freq = 0;
	unsigned int cur = get_cur_cluster(cpu);
	struct cpufreq_policy *policy;

	g_clamp_cpufreqs[cur] = clamp_freq;
	new_freq = min(clamp_freq, gov_target);
	freq = exynos_getspeed(cpu);

	if (freq <= clamp_freq)
		return;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		return;
	}
#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		return;
	}
#endif
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("IPA: %s:%d: set clamps for cpu %d to %d (curr was %d)",
		     __PRETTY_FUNCTION__, __LINE__, cpu, clamp_freq, freq);
#endif

	__cpufreq_driver_target(policy, new_freq, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);
}

#ifdef CONFIG_PM
static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	freqs[CA7]->old = exynos_getspeed_cluster(CA7);
	freqs[CA15]->old = exynos_getspeed_cluster(CA15);
	return 0;
}
#endif

static int __cpuinit exynos_cpufreq_cpu_notifier(struct notifier_block *notifier,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;

	if (suspend_prepared)
		return NOTIFY_OK;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action) {
		case CPU_DOWN_PREPARE:
		case CPU_DOWN_PREPARE_FROZEN:
			mutex_lock(&cpufreq_lock);
			exynos_info[CA7]->blocked = true;
			exynos_info[CA15]->blocked = true;
			mutex_unlock(&cpufreq_lock);
			break;
		case CPU_DOWN_FAILED:
		case CPU_DOWN_FAILED_FROZEN:
		case CPU_DEAD:
			mutex_lock(&cpufreq_lock);
			exynos_info[CA7]->blocked = false;
			exynos_info[CA15]->blocked = false;
			mutex_unlock(&cpufreq_lock);
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata exynos_cpufreq_cpu_nb = {
	.notifier_call = exynos_cpufreq_cpu_notifier,
};

/*
 * exynos_cpufreq_pm_notifier - block CPUFREQ's activities in suspend-resume
 *			context
 * @notifier
 * @pm_event
 * @v
 *
 * While cpufreq_disable == true, target() ignores every frequency but
 * boot_freq. The boot_freq value is the initial frequency,
 * which is set by the bootloader. In order to eliminate possible
 * inconsistency in clock values, we save and restore frequencies during
 * suspend and resume and block CPUFREQ activities. Note that the standard
 * suspend/resume cannot be used as they are too deep (syscore_ops) for
 * regulator actions.
 */
static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	struct cpufreq_frequency_table *CA15_freq_table = exynos_info[CA15]->freq_table;
	struct cpufreq_frequency_table *CA7_freq_table = exynos_info[CA7]->freq_table;
	unsigned int freqCA7, freqCA15;
	unsigned int bootfreqCA7, bootfreqCA15;
	unsigned int abb_freqCA7 = 0, abb_freqCA15 = 0;
	bool set_abb_first_than_volt = false;
	int volt, i;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&cpufreq_lock);
		exynos_info[CA7]->blocked = true;
		exynos_info[CA15]->blocked = true;
		mutex_unlock(&cpufreq_lock);

		bootfreqCA7 = get_boot_freq(CA7);
		bootfreqCA15 = get_boot_freq(CA15);

		freqCA7 = freqs[CA7]->old;
		freqCA15 = freqs[CA15]->old;

		volt = max(get_boot_volt(CA7),
				get_freq_volt(CA7, freqCA7));
		BUG_ON(volt <= 0);
		volt = get_limit_voltage(volt);

		if (exynos_info[CA7]->abb_table)
			set_abb_first_than_volt = is_set_abb_first(ID_KFC, freqCA7, bootfreqCA7);

		if (!set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt))
				goto err;

		if (exynos_info[CA7]->abb_table) {
			abb_freqCA7 = max(bootfreqCA7, freqCA7);
			for (i = 0; (CA7_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				if (CA7_freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
					continue;
				if (CA7_freq_table[i].frequency == abb_freqCA7) {
					set_match_abb(ID_KFC, exynos_info[CA7]->abb_table[i]);
					break;
				}
			}
		}

		if (!set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt))
				goto err;

		volt = max(get_boot_volt(CA15),
				get_freq_volt(CA15, freqCA15));
		if ( volt <= 0) {
			printk("oops, strange voltage CA15 -> boot volt:%d, get_freq_volt:%d, freqCA15:%d \n",
				get_boot_volt(CA15), get_freq_volt(CA15, freqCA15), freqCA15);
			BUG_ON(volt <= 0);
		}
		volt = get_limit_voltage(volt);

		set_abb_first_than_volt = false;
		if (exynos_info[CA15]->abb_table)
			set_abb_first_than_volt = is_set_abb_first(ID_ARM, freqCA15, bootfreqCA15);

		if (!set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt))
				goto err;

		if (exynos_info[CA15]->abb_table) {
			abb_freqCA15 = max(bootfreqCA15, freqCA15);
			for (i = 0; (CA15_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				if (CA15_freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
					continue;
				if (CA15_freq_table[i].frequency == abb_freqCA15) {
					set_match_abb(ID_ARM, exynos_info[CA15]->abb_table[i]);
					break;
				}
			}
		}

		if (set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt))
				goto err;

		suspend_prepared = true;

		pr_debug("PM_SUSPEND_PREPARE for CPUFREQ\n");

		break;
	case PM_POST_SUSPEND:
		pr_debug("PM_POST_SUSPEND for CPUFREQ\n");

		mutex_lock(&cpufreq_lock);
		exynos_info[CA7]->blocked = false;
		exynos_info[CA15]->blocked = false;
		mutex_unlock(&cpufreq_lock);

		suspend_prepared = false;

		break;
	}
	return NOTIFY_OK;
err:
	pr_err("%s: failed to set voltage\n", __func__);

	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos_cpufreq_pm_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos_cpufreq_tmu_notifier(struct notifier_block *notifier,
				       unsigned long event, void *v)
{
	int volt;
	int *on = v;

	if (event != TMU_COLD)
		return NOTIFY_OK;

	mutex_lock(&cpufreq_lock);
	if (*on) {
		if (volt_offset)
			goto out;
		else
			volt_offset = COLD_VOLT_OFFSET;

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA15]->regulator));
		regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt);
		if (exynos_info[CA15]->set_ema)
			exynos_info[CA15]->set_ema(volt);

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA7]->regulator));
		regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt);
		if (exynos_info[CA7]->set_ema)
			exynos_info[CA7]->set_ema(volt);
	} else {
		if (!volt_offset)
			goto out;
		else
			volt_offset = 0;

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA15]->regulator)
							- COLD_VOLT_OFFSET);
		if (exynos_info[CA15]->set_ema)
			exynos_info[CA15]->set_ema(volt);
		regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt);

		volt = get_limit_voltage(regulator_get_voltage(exynos_info[CA7]->regulator)
							- COLD_VOLT_OFFSET);
		regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt);
		if (exynos_info[CA7]->set_ema)
			exynos_info[CA7]->set_ema(volt);
	}

out:
	mutex_unlock(&cpufreq_lock);

	return NOTIFY_OK;
}

static struct notifier_block exynos_tmu_nb = {
	.notifier_call = exynos_cpufreq_tmu_notifier,
};
#endif

static int exynos_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	unsigned int cur = get_cur_cluster(policy->cpu);

	pr_debug("%s: cpu[%d]\n", __func__, policy->cpu);

	policy->cur = policy->min = policy->max = exynos_getspeed(policy->cpu);

	cpufreq_frequency_table_get_attr(exynos_info[cur]->freq_table, policy->cpu);

	/* set the transition latency value */
	policy->cpuinfo.transition_latency = 100000;

	if (cpumask_test_cpu(policy->cpu, &cluster_cpus[CA15])) {
		cpumask_copy(policy->cpus, &cluster_cpus[CA15]);
		cpumask_copy(policy->related_cpus, &cluster_cpus[CA15]);
	} else {
		cpumask_copy(policy->cpus, &cluster_cpus[CA7]);
		cpumask_copy(policy->related_cpus, &cluster_cpus[CA7]);
	}

	return cpufreq_frequency_table_cpuinfo(policy, exynos_info[cur]->freq_table);
}

static struct cpufreq_driver exynos_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= exynos_verify_speed,
	.target		= exynos_target,
	.get		= exynos_getspeed,
	.init		= exynos_cpufreq_cpu_init,
	.name		= "exynos_cpufreq",
#ifdef CONFIG_PM
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
#endif
	.have_governor_per_policy = true,
};

/************************** sysfs interface ************************/
static ssize_t show_cpufreq_table(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table_CA15 = exynos_info[CA15]->freq_table;
	struct cpufreq_frequency_table *freq_table_CA7 = exynos_info[CA7]->freq_table;

	for (i = 0; freq_table_CA15[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;
	for (i = 0; freq_table_CA7[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;

	if (tbl_sz == 0)
		return -EINVAL;

	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table_CA15[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table_CA15[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
						freq_table_CA15[i].frequency);
	}

	for (i = 0; freq_table_CA7[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table_CA7[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
					freq_table_CA7[i].frequency / 2);
	}

	count += snprintf(&buf[count - 1], 2, "\n");

	return count - 1;
}

static ssize_t show_cpufreq_min_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int cpu_qos_min = pm_qos_request(PM_QOS_CPU_FREQ_MIN);
	unsigned int kfc_qos_min = pm_qos_request(PM_QOS_KFC_FREQ_MIN);

	if (cpu_qos_min > 0 && get_hmp_boost()) {
		if (cpu_qos_min > freq_max[CA15])
			cpu_qos_min = freq_max[CA15];
		else if (cpu_qos_min < freq_min[CA15])
			cpu_qos_min = freq_min[CA15];
		return snprintf(buf, 10, "%u\n", cpu_qos_min);
	} else if (kfc_qos_min > 0) {
		if (kfc_qos_min > freq_max[CA7])
			kfc_qos_min = freq_max[CA7];
		if (kfc_qos_min < freq_min[CA7])
			kfc_qos_min = freq_min[CA7];
		return snprintf(buf, 10, "%u\n", kfc_qos_min / 2);
	} else if (kfc_qos_min == 0) {
		kfc_qos_min = freq_min[CA7];
		return snprintf(buf, 10, "%u\n", kfc_qos_min / 2);
	}

	return 0;
}

static ssize_t store_cpufreq_min_limit(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int cpu_input, kfc_input;

	if (!sscanf(buf, "%d", &cpu_input))
		return -EINVAL;

	if (cpu_input >= (int)freq_min[CA15]) {
		if (!hmp_boosted) {
			if (set_hmp_boost(1) < 0)
				pr_err("%s: failed HMP boost enable\n",
							__func__);
			else
				hmp_boosted = true;
		}

		cpu_input = min(cpu_input, (int)freq_max[CA15]);
		kfc_input = max_kfc_qos_const.default_value;
	} else if (cpu_input < (int)freq_min[CA15]) {
		if (hmp_boosted) {
			if (set_hmp_boost(0) < 0)
				pr_err("%s: failed HMP boost disable\n",
							__func__);
			else
				hmp_boosted = false;
		}

		if (cpu_input < 0) {
			cpu_input = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;
			kfc_input = PM_QOS_KFC_FREQ_MIN_DEFAULT_VALUE;
		} else {
			kfc_input = cpu_input * 2;
			if (kfc_input > 0)
				kfc_input = min(kfc_input, (int)freq_max[CA7]);
			cpu_input = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;
		}
	}

	if (pm_qos_request_active(&min_cpu_qos))
		pm_qos_update_request(&min_cpu_qos, cpu_input);
	if (pm_qos_request_active(&min_kfc_qos))
		pm_qos_update_request(&min_kfc_qos, kfc_input);

	return count;
}

static ssize_t show_cpufreq_max_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int cpu_qos_max = pm_qos_request(PM_QOS_CPU_FREQ_MAX);
	unsigned int kfc_qos_max = pm_qos_request(PM_QOS_KFC_FREQ_MAX);

	if (cpu_qos_max > 0) {
		if (cpu_qos_max < freq_min[CA15])
			cpu_qos_max = freq_min[CA15];
		else if (cpu_qos_max > freq_max[CA15])
			cpu_qos_max = freq_max[CA15];
		return snprintf(buf, 10, "%u\n", cpu_qos_max);
	} else if (kfc_qos_max > 0) {
		if (kfc_qos_max < freq_min[CA7])
			kfc_qos_max = freq_min[CA7];
		if (kfc_qos_max > freq_max[CA7])
			kfc_qos_max = freq_max[CA7];
		return snprintf(buf, 10, "%u\n", kfc_qos_max / 2);
	} else if (kfc_qos_max == 0) {
		kfc_qos_max = freq_min[CA7];
		return snprintf(buf, 10, "%u\n", kfc_qos_max / 2);
	}

	return 0;
}

static ssize_t store_cpufreq_max_limit(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int cpu_input, kfc_input;

	if (!sscanf(buf, "%d", &cpu_input))
		return -EINVAL;

	if (cpu_input >= (int)freq_min[CA15]) {
		if (egl_hotplugged) {
			if (big_cores_hotplug(false))
				pr_err("%s: failed big cores hotplug in\n",
							__func__);
			else
				egl_hotplugged = false;
		}

		cpu_input = max(cpu_input, (int)freq_min[CA15]);
		kfc_input = max_kfc_qos_const.default_value;
	} else if (cpu_input < (int)freq_min[CA15]) {
		if (cpu_input < 0) {
			if (egl_hotplugged) {
				if (big_cores_hotplug(false))
					pr_err("%s: failed big cores hotplug in\n",
							__func__);
				else
					egl_hotplugged = false;
			}

			cpu_input = max_cpu_qos_const.default_value;
			kfc_input = max_kfc_qos_const.default_value;
		} else {
			kfc_input = cpu_input * 2;
			if (kfc_input > 0)
				kfc_input = max(kfc_input, (int)freq_min[CA7]);
			cpu_input = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE;

			if (!egl_hotplugged) {
				if (big_cores_hotplug(true))
					pr_err("%s: failed big cores hotplug out\n",
							__func__);
				else
					egl_hotplugged = true;
			}
		}
	}

	if (pm_qos_request_active(&max_cpu_qos))
		pm_qos_update_request(&max_cpu_qos, cpu_input);
	if (pm_qos_request_active(&max_kfc_qos))
		pm_qos_update_request(&max_kfc_qos, kfc_input);

	return count;
}

static ssize_t show_cpu_freq_table(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	int i, count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table = exynos_info[CA15]->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;

	if (tbl_sz == 0)
		return -EINVAL;

	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
					freq_table[i].frequency);
	}

	count += snprintf(&buf[count], 2, "\n");
	return count;
}

static ssize_t show_cpu_min_freq(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int cpu_qos_min = pm_qos_request(PM_QOS_CPU_FREQ_MIN);

	if (cpu_qos_min == 0)
		cpu_qos_min = freq_min[CA15];

	return snprintf(buf, PAGE_SIZE, "%u\n", cpu_qos_min);
}

static ssize_t show_cpu_max_freq(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int cpu_qos_max = pm_qos_request(PM_QOS_CPU_FREQ_MAX);

	if (cpu_qos_max == 0)
		cpu_qos_max = freq_min[CA15];

	return snprintf(buf, PAGE_SIZE, "%u\n", cpu_qos_max);
}

static ssize_t store_cpu_min_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = min(input, (int)freq_max[CA15]);

	if (pm_qos_request_active(&min_cpu_qos))
		pm_qos_update_request(&min_cpu_qos, input);

	return count;
}

static ssize_t store_cpu_max_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = max(input, (int)freq_min[CA15]);

	if (pm_qos_request_active(&max_cpu_qos))
		pm_qos_update_request(&max_cpu_qos, input);

	return count;
}

static ssize_t show_kfc_freq_table(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	int i, count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table = exynos_info[CA7]->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;

	if (tbl_sz == 0)
		return -EINVAL;

	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
						freq_table[i].frequency);
        }

        count += snprintf(&buf[count], 2, "\n");
        return count;
}

static ssize_t show_kfc_min_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	unsigned int kfc_qos_min = pm_qos_request(PM_QOS_KFC_FREQ_MIN);

	if (kfc_qos_min == 0)
		kfc_qos_min = freq_min[CA7];

	return snprintf(buf, PAGE_SIZE, "%u\n", kfc_qos_min);
}

static ssize_t show_kfc_max_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	unsigned int kfc_qos_max = pm_qos_request(PM_QOS_KFC_FREQ_MAX);

	if (kfc_qos_max == 0)
		kfc_qos_max = freq_min[CA7];

	return snprintf(buf, PAGE_SIZE, "%u\n", kfc_qos_max);
}

static ssize_t store_kfc_min_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = min(input, (int)freq_max[CA7]);

	if (pm_qos_request_active(&min_kfc_qos))
		pm_qos_update_request(&min_kfc_qos, input);

	return count;
}

static ssize_t store_kfc_max_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0)
		input = max(input, (int)freq_min[CA7]);

	if (pm_qos_request_active(&max_kfc_qos))
		pm_qos_update_request(&max_kfc_qos, input);

	return count;
}

define_one_global_ro(cpu_freq_table);
define_one_global_rw(cpu_min_freq);
define_one_global_rw(cpu_max_freq);
define_one_global_ro(kfc_freq_table);
define_one_global_rw(kfc_min_freq);
define_one_global_rw(kfc_max_freq);

static struct attribute *mp_attributes[] = {
	&cpu_freq_table.attr,
	&cpu_min_freq.attr,
	&cpu_max_freq.attr,
	&kfc_freq_table.attr,
	&kfc_min_freq.attr,
	&kfc_max_freq.attr,
	NULL
};

static struct attribute_group mp_attr_group = {
	.attrs = mp_attributes,
	.name = "mp-cpufreq",
};

#ifdef CONFIG_PM
static struct global_attr cpufreq_table =
		__ATTR(cpufreq_table, S_IRUGO, show_cpufreq_table, NULL);
static struct global_attr cpufreq_min_limit =
		__ATTR(cpufreq_min_limit, S_IRUGO | S_IWUSR,
			show_cpufreq_min_limit, store_cpufreq_min_limit);
static struct global_attr cpufreq_max_limit =
		__ATTR(cpufreq_max_limit, S_IRUGO | S_IWUSR,
			show_cpufreq_max_limit, store_cpufreq_max_limit);
#endif

/************************** sysfs end ************************/

static int exynos_cpufreq_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	struct cpufreq_frequency_table *CA15_freq_table = exynos_info[CA15]->freq_table;
	struct cpufreq_frequency_table *CA7_freq_table = exynos_info[CA7]->freq_table;
	unsigned int freqCA7, freqCA15;
	unsigned int bootfreqCA7, bootfreqCA15;
	unsigned int abb_freqCA7 = 0, abb_freqCA15 = 0;
	bool set_abb_first_than_volt = false;
	int volt, i;

	mutex_lock(&cpufreq_lock);
	exynos_info[CA7]->blocked = true;
	exynos_info[CA15]->blocked = true;
	mutex_unlock(&cpufreq_lock);

	bootfreqCA7 = get_boot_freq(CA7);
	bootfreqCA15 = get_boot_freq(CA15);

	freqCA7 = freqs[CA7]->old;
	freqCA15 = freqs[CA15]->old;

	volt = max(get_boot_volt(CA7),
			get_freq_volt(CA7, freqCA7));
	volt = get_limit_voltage(volt);

	if (exynos_info[CA7]->abb_table)
		set_abb_first_than_volt = is_set_abb_first(ID_ARM, freqCA7, max(bootfreqCA7, freqCA7));

	if (!set_abb_first_than_volt)
		if (regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt))
			goto err;

	if (exynos_info[CA7]->abb_table) {
		abb_freqCA7 = max(bootfreqCA7, freqCA7);
		for (i = 0; (CA7_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
			if (CA7_freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;
			if (CA7_freq_table[i].frequency == abb_freqCA7) {
				set_match_abb(ID_KFC, exynos_info[CA7]->abb_table[i]);
				break;
			}
		}
	}

	if (set_abb_first_than_volt)
		if (regulator_set_voltage(exynos_info[CA7]->regulator, volt, volt))
			goto err;

	if (exynos_info[CA7]->set_ema)
		exynos_info[CA7]->set_ema(volt);

	volt = max(get_boot_volt(CA15),
			get_freq_volt(CA15, freqCA15));
	volt = get_limit_voltage(volt);


	set_abb_first_than_volt = false;
	if (exynos_info[CA15]->abb_table)
		set_abb_first_than_volt = is_set_abb_first(ID_ARM, freqCA15, max(bootfreqCA15, freqCA15));

	if (!set_abb_first_than_volt)
		if (regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt))
			goto err;

	if (exynos_info[CA15]->abb_table) {
		abb_freqCA15 = max(bootfreqCA15, freqCA15);
		for (i = 0; (CA15_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
			if (CA15_freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;
			if (CA15_freq_table[i].frequency == abb_freqCA15) {
				set_match_abb(ID_ARM, exynos_info[CA15]->abb_table[i]);
				break;
			}
		}
	}

	if (set_abb_first_than_volt)
		if (regulator_set_voltage(exynos_info[CA15]->regulator, volt, volt))
			goto err;

	if (exynos_info[CA15]->set_ema)
		exynos_info[CA15]->set_ema(volt);

	return NOTIFY_DONE;
err:
	pr_err("%s: failed to set voltage\n", __func__);

	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_reboot_notifier = {
	.notifier_call = exynos_cpufreq_reboot_notifier_call,
};

void (*disable_c3_idle)(bool disable);

static void exynos_qos_nop(void *info)
{
}

static int exynos_cpu_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? 0 : NR_CA7;

	if (val)
		event_hotplug_in();

	freq = exynos_getspeed(cpu);
	if (freq >= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	if (disable_c3_idle)
		disable_c3_idle(true);

	smp_call_function_single(cpu, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	if (disable_c3_idle)
		disable_c3_idle(false);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpu_min_qos_notifier = {
	.notifier_call = exynos_cpu_min_qos_handler,
};

static int exynos_cpu_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? 0 : NR_CA7;

	freq = exynos_getspeed(cpu);
	if (freq <= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	if (disable_c3_idle)
		disable_c3_idle(true);

	smp_call_function_single(cpu, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	if (disable_c3_idle)
		disable_c3_idle(false);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpu_max_qos_notifier = {
	.notifier_call = exynos_cpu_max_qos_handler,
};

static int exynos_kfc_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? NR_CA15 : 0;
	unsigned int threshold_freq;

#if defined(CONFIG_CPU_FREQ_GOV_INTERACTIVE)
	threshold_freq = cpufreq_interactive_get_hispeed_freq(0);
	if (!threshold_freq)
		threshold_freq = 1000000;	/* 1.0GHz */
#else
	threshold_freq = 1000000;	/* 1.0GHz */
#endif

	if (val > threshold_freq)
		event_hotplug_in();

	freq = exynos_getspeed(cpu);
	if (freq >= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	smp_call_function_single(cpu, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_kfc_min_qos_notifier = {
	.notifier_call = exynos_kfc_min_qos_handler,
};

static int exynos_kfc_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? NR_CA15 : 0;

	freq = exynos_getspeed(cpu);
	if (freq <= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	smp_call_function_single(cpu, exynos_qos_nop, NULL, 0);

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);

	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_kfc_max_qos_notifier = {
	.notifier_call = exynos_kfc_max_qos_handler,
};

static int __init exynos_cpufreq_init(void)
{
	int i, ret = -EINVAL;
	cluster_type cluster;
	struct cpufreq_frequency_table *freq_table;

	g_clamp_cpufreqs[CA15] = UINT_MAX;
	g_clamp_cpufreqs[CA7] = UINT_MAX;

	boot_cluster = 0;

	exynos_info[CA7] = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info[CA7]) {
		ret = -ENOMEM;
		goto err_alloc_info_CA7;
	}

	exynos_info[CA15] = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info[CA15]) {
		ret = -ENOMEM;
		goto err_alloc_info_CA15;
	}

	freqs[CA7] = kzalloc(sizeof(struct cpufreq_freqs), GFP_KERNEL);
	if (!freqs[CA7]) {
		ret = -ENOMEM;
		goto err_alloc_freqs_CA7;
	}

	freqs[CA15] = kzalloc(sizeof(struct cpufreq_freqs), GFP_KERNEL);
	if (!freqs[CA15]) {
		ret = -ENOMEM;
		goto err_alloc_freqs_CA15;
	}

	/* Get to boot_cluster_num - 0 for CA7; 1 for CA15 */
	boot_cluster = !MPIDR_AFFINITY_LEVEL(cpu_logical_map(0), 1);
	pr_debug("%s: boot_cluster is %s\n", __func__,
					boot_cluster == CA7 ? "CA7" : "CA15");
	exynos_boot_cluster = boot_cluster;

	init_cpumask_cluster_set(boot_cluster);

	ret = exynos5_cpufreq_CA7_init(&exynos_info_CA7);
	if (ret)
		goto err_init_cpufreq;

	ret = exynos5_cpufreq_CA15_init(&exynos_info_CA15);
	if (ret)
		goto err_init_cpufreq;

	arm_regulator = regulator_get(NULL, "vdd_eagle");
	if (IS_ERR(arm_regulator)) {
		pr_err("%s: failed to get resource vdd_eagle\n", __func__);
		goto err_vdd_eagle;
	}

	kfc_regulator = regulator_get(NULL, "vdd_kfc");
	if (IS_ERR(kfc_regulator)) {
		pr_err("%s:failed to get resource vdd_kfc\n", __func__);
		goto err_vdd_kfc;
	}

	memcpy(exynos_info[CA7], &exynos_info_CA7,
				sizeof(struct exynos_dvfs_info));
	exynos_info[CA7]->regulator = kfc_regulator;

	memcpy(exynos_info[CA15], &exynos_info_CA15,
				sizeof(struct exynos_dvfs_info));
	exynos_info[CA15]->regulator = arm_regulator;

	if (exynos_info[CA7]->set_freq == NULL) {
		pr_err("%s: No set_freq function (ERR)\n", __func__);
		goto err_set_freq;
	}

	freq_max[CA15] = exynos_info[CA15]->
		freq_table[exynos_info[CA15]->max_support_idx].frequency;
	freq_min[CA15] = exynos_info[CA15]->
		freq_table[exynos_info[CA15]->min_support_idx].frequency;
	freq_max[CA7] = exynos_info[CA7]->
		freq_table[exynos_info[CA7]->max_support_idx].frequency;
	freq_min[CA7] = exynos_info[CA7]->
		freq_table[exynos_info[CA7]->min_support_idx].frequency;

	set_boot_freq();

	/* set initial old frequency */
	freqs[CA7]->old = exynos_getspeed_cluster(CA7);
	freqs[CA15]->old = exynos_getspeed_cluster(CA15);

	/*
	 * boot freq index is needed if minimum supported frequency
	 * greater than boot freq
	 */
	for (cluster = 0; cluster < CA_END; cluster++) {
		freq_table = exynos_info[cluster]->freq_table;
		exynos_info[cluster]->boot_freq_idx = -1;
		pr_debug("%s Core Max-Min frequency[%d - %d]KHz\n",
			cluster ? "CA15" : "CA7", freq_max[cluster], freq_min[cluster]);

		for (i = L0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
			if (exynos_info[cluster]->boot_freq == freq_table[i].frequency) {
				exynos_info[cluster]->boot_freq_idx = i;
				pr_debug("boot frequency[%d] index %d\n",
						exynos_info[cluster]->boot_freq, i);
			}

			if (freq_table[i].frequency > freq_max[cluster] ||
				freq_table[i].frequency < freq_min[cluster])
				freq_table[i].frequency = CPUFREQ_ENTRY_INVALID;
		}
	}

	register_hotcpu_notifier(&exynos_cpufreq_cpu_nb);
	register_pm_notifier(&exynos_cpufreq_nb);
	register_reboot_notifier(&exynos_cpufreq_reboot_notifier);
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&exynos_tmu_nb);
#endif

	/* setup default qos constraints */
	max_cpu_qos_const.target_value = freq_max[CA15];
	max_cpu_qos_const.default_value = freq_max[CA15];
	pm_qos_update_constraints(PM_QOS_CPU_FREQ_MAX, &max_cpu_qos_const);

	max_kfc_qos_const.target_value = freq_max[CA7];
	max_kfc_qos_const.default_value = freq_max[CA7];
	pm_qos_update_constraints(PM_QOS_KFC_FREQ_MAX, &max_kfc_qos_const);

	pm_qos_add_notifier(PM_QOS_CPU_FREQ_MIN, &exynos_cpu_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CPU_FREQ_MAX, &exynos_cpu_max_qos_notifier);
	pm_qos_add_notifier(PM_QOS_KFC_FREQ_MIN, &exynos_kfc_min_qos_notifier);
	pm_qos_add_notifier(PM_QOS_KFC_FREQ_MAX, &exynos_kfc_max_qos_notifier);

	if (cpufreq_register_driver(&exynos_driver)) {
		pr_err("%s: failed to register cpufreq driver\n", __func__);
		goto err_cpufreq;
	}

	ret = sysfs_create_group(cpufreq_global_kobject, &mp_attr_group);
	if (ret) {
		pr_err("%s: failed to create iks-cpufreq sysfs interface\n", __func__);
		goto err_mp_attr;
	}

#ifdef CONFIG_PM
	ret = sysfs_create_file(power_kobj, &cpufreq_table.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_table sysfs interface\n", __func__);
		goto err_cpu_table;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_min_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_min_limit sysfs interface\n", __func__);
		goto err_cpufreq_min_limit;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_max_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_max_limit sysfs interface\n", __func__);
		goto err_cpufreq_max_limit;
	}
#endif

	pm_qos_add_request(&min_kfc_qos, PM_QOS_KFC_FREQ_MIN,
					PM_QOS_KFC_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&max_kfc_qos, PM_QOS_KFC_FREQ_MAX,
					max_kfc_qos_const.default_value);
	pm_qos_add_request(&min_cpu_qos, PM_QOS_CPU_FREQ_MIN,
					PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&max_cpu_qos, PM_QOS_CPU_FREQ_MAX,
					max_cpu_qos_const.default_value);

	if (exynos_info[CA7]->boot_cpu_min_qos) {
		pm_qos_add_request(&boot_min_kfc_qos, PM_QOS_KFC_FREQ_MIN,
					PM_QOS_KFC_FREQ_MIN_DEFAULT_VALUE);
		pm_qos_update_request_timeout(&boot_min_kfc_qos,
					exynos_info[CA7]->boot_cpu_min_qos, 40000 * 1000);
	}

	if (exynos_info[CA7]->boot_cpu_max_qos) {
		pm_qos_add_request(&boot_max_kfc_qos, PM_QOS_KFC_FREQ_MAX,
					max_kfc_qos_const.default_value);
		pm_qos_update_request_timeout(&boot_max_kfc_qos,
					exynos_info[CA7]->boot_cpu_max_qos, 40000 * 1000);
	}

	if (exynos_info[CA15]->boot_cpu_min_qos) {
		pm_qos_add_request(&boot_min_cpu_qos, PM_QOS_CPU_FREQ_MIN,
					max_cpu_qos_const.default_value);
		pm_qos_update_request_timeout(&boot_min_cpu_qos,
					exynos_info[CA15]->boot_cpu_min_qos, 40000 * 1000);
	}

	if (exynos_info[CA15]->boot_cpu_max_qos) {
		pm_qos_add_request(&boot_max_cpu_qos, PM_QOS_CPU_FREQ_MAX,
					PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
		pm_qos_update_request_timeout(&boot_max_cpu_qos,
					exynos_info[CA15]->boot_cpu_max_qos, 40000 * 1000);
	}

	if (exynos_info[CA7]->bus_table)
		pm_qos_add_request(&exynos_mif_qos_CA7, PM_QOS_BUS_THROUGHPUT, 0);
	if (exynos_info[CA15]->bus_table)
		pm_qos_add_request(&exynos_mif_qos_CA15, PM_QOS_BUS_THROUGHPUT, 0);

	if (exynos_info[CA7]->bus_table || exynos_info[CA15]->bus_table) {
		INIT_DEFERRABLE_WORK(&monitor_cluster_on, cluster_onoff_monitor);

		cluster_monitor_wq = create_workqueue("cluster_monitor");
		if (!cluster_monitor_wq) {
			pr_err("%s: failed to create cluster_monitor_wq\n", __func__);
			goto err_workqueue;
		}

		queue_delayed_work_on(0, cluster_monitor_wq, &monitor_cluster_on,
						msecs_to_jiffies(1000));
	}

	exynos_cpufreq_init_done = true;
	exynos_cpufreq_init_notify_call_chain(CPUFREQ_INIT_COMPLETE);

	return 0;

err_workqueue:
	if (exynos_info[CA15]->bus_table)
		pm_qos_remove_request(&exynos_mif_qos_CA15);
	if (exynos_info[CA7]->bus_table)
		pm_qos_remove_request(&exynos_mif_qos_CA7);

	if (pm_qos_request_active(&boot_max_cpu_qos))
		pm_qos_remove_request(&boot_max_cpu_qos);
	if (pm_qos_request_active(&boot_min_cpu_qos))
		pm_qos_remove_request(&boot_min_cpu_qos);
	if (pm_qos_request_active(&boot_max_kfc_qos))
		pm_qos_remove_request(&boot_max_kfc_qos);
	if (pm_qos_request_active(&boot_min_kfc_qos))
		pm_qos_remove_request(&boot_min_kfc_qos);
#ifdef CONFIG_PM
err_cpufreq_max_limit:
	sysfs_remove_file(power_kobj, &cpufreq_min_limit.attr);
err_cpufreq_min_limit:
	sysfs_remove_file(power_kobj, &cpufreq_table.attr);
err_cpu_table:
	sysfs_remove_group(cpufreq_global_kobject, &mp_attr_group);
#endif
err_mp_attr:
	cpufreq_unregister_driver(&exynos_driver);
err_cpufreq:
	pm_qos_remove_notifier(PM_QOS_CPU_FREQ_MIN, &exynos_cpu_min_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_CPU_FREQ_MAX, &exynos_cpu_max_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_KFC_FREQ_MIN, &exynos_kfc_min_qos_notifier);
	pm_qos_remove_notifier(PM_QOS_KFC_FREQ_MAX, &exynos_kfc_max_qos_notifier);
	unregister_reboot_notifier(&exynos_cpufreq_reboot_notifier);
	unregister_pm_notifier(&exynos_cpufreq_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_nb);
err_set_freq:
	regulator_put(kfc_regulator);
err_vdd_kfc:
	regulator_put(arm_regulator);
err_vdd_eagle:
err_init_cpufreq:
	kfree(freqs[CA15]);
err_alloc_freqs_CA15:
	kfree(freqs[CA7]);
err_alloc_freqs_CA7:
	kfree(exynos_info[CA15]);
err_alloc_info_CA15:
	kfree(exynos_info[CA7]);
err_alloc_info_CA7:
	pr_err("%s: failed initialization\n", __func__);

	return ret;
}

late_initcall(exynos_cpufreq_init);
