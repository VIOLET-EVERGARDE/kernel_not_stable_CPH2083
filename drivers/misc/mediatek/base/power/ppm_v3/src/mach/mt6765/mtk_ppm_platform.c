/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/notifier.h>
#include <mt-plat/sync_write.h>

#include "mtk_ppm_platform.h"
#include "mtk_ppm_internal.h"
#include "mtk_common_static_power.h"
#ifdef CONFIG_MTK_UNIFY_POWER
#include "mtk_upower.h"
#endif
#ifdef CONFIG_THERMAL
#include "mach/mtk_thermal.h"
#endif


unsigned int __attribute__((weak)) mt_cpufreq_get_cur_volt(
		enum mt_cpu_dvfs_id id)
{
	return 0;
}
int __attribute__((weak)) mt_spower_get_leakage(
	int dev, unsigned int voltage, int degree)
{
	return 0;
}

#ifdef PPM_SSPM_SUPPORT
static void *online_core;
#endif
static void ppm_get_cluster_status(struct ppm_cluster_status *cl_status)
{
#ifndef NO_SCHEDULE_API
	struct cpumask cluster_cpu, online_cpu;
#endif
	int i;

	for_each_ppm_clusters(i) {
#ifndef NO_SCHEDULE_API
		arch_get_cluster_cpus(&cluster_cpu, i);
		cpumask_and(&online_cpu, &cluster_cpu, cpu_online_mask);
		cl_status[i].core_num = cpumask_weight(&online_cpu);
#else
		cl_status[i].core_num = get_cluster_max_cpu_core(i);
#endif
		cl_status[i].volt = mt_cpufreq_get_cur_volt(i) / 100;
		cl_status[i].freq_idx = ppm_main_freq_to_idx(i,
				mt_cpufreq_get_cur_phy_freq_no_lock(i),
				CPUFREQ_RELATION_L);
	}
}

#ifdef CONFIG_CPU_FREQ
static int ppm_cpu_freq_callback(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS];
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
	int i, is_root_cpu = 0;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	/* we only need to notify DLPT when root cpu's freq is changed */
	for_each_ppm_clusters(i) {
		if (cpu == ppm_main_info.cluster_info[i].cpu_id) {
			is_root_cpu = 1;
			break;
		}
	}
	if (!is_root_cpu)
		return NOTIFY_OK;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		ppm_dbg(DLPT, "%s: POSTCHANGE!, cpu = %d\n", __func__, cpu);
		ppm_get_cluster_status(cl_status);
		mt_ppm_dlpt_kick_PBM(cl_status, ppm_main_info.cluster_num);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block ppm_cpu_freq_notifier = {
	.notifier_call = ppm_cpu_freq_callback,
};
#endif

static int ppm_cpu_hotplug_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu)
{
	struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS];
#ifdef PPM_SSPM_SUPPORT
	int i;
#endif

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
	case CPU_DEAD:
		ppm_dbg(DLPT, "%s: action = %lu\n", __func__, action);
		ppm_get_cluster_status(cl_status);
#ifdef PPM_SSPM_SUPPORT
		for_each_ppm_clusters(i)
			mt_reg_sync_writel(cl_status[i].core_num,
			online_core + 4 * i);
#endif
		mt_ppm_dlpt_kick_PBM(cl_status, ppm_main_info.cluster_num);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata ppm_cpu_hotplug_notifier = {
	.notifier_call = ppm_cpu_hotplug_callback,
};

#ifdef CONFIG_THERMAL
static unsigned int ppm_get_cpu_temp(enum ppm_cluster cluster)
{
	unsigned int temp = 85;

	switch (cluster) {
	case PPM_CLUSTER_LL:
		temp = get_immediate_cpuLL_wrap() / 1000;
		break;
	case PPM_CLUSTER_L:
		temp = get_immediate_cpuL_wrap() / 1000;
		break;
	default:
		ppm_err("@%s: invalid cluster id = %d\n", __func__, cluster);
		break;
	}

	return temp;
}
#endif

static int ppm_get_spower_devid(enum ppm_cluster cluster)
{
	int devid = -1;

	switch (cluster) {
	case PPM_CLUSTER_LL:
		devid = MTK_SPOWER_CPULL;
		break;
	case PPM_CLUSTER_L:
		devid = MTK_SPOWER_CPUL;
		break;
	default:
		ppm_err("@%s: invalid cluster id = %d\n", __func__, cluster);
		break;
	}

	return devid;
}


int ppm_platform_init(void)
{
#ifdef PPM_SSPM_SUPPORT
	/* map sram to update online core */
	online_core = ioremap_nocache(
		PPM_ONLINE_CORE_SRAM_ADDR, 4 * NR_PPM_CLUSTERS);
	if (!online_core) {
		ppm_err("remap online_core failed!\n");
		WARN_ON(1);
	}
#endif
#ifdef CONFIG_CPU_FREQ
	cpufreq_register_notifier(&ppm_cpu_freq_notifier,
		CPUFREQ_TRANSITION_NOTIFIER);
#endif
	register_hotcpu_notifier(&ppm_cpu_hotplug_notifier);

	return 0;
}

void ppm_update_req_by_pwr(struct ppm_policy_req *req)
{
	ppm_cobra_update_limit(req);
}

int ppm_find_pwr_idx(struct ppm_cluster_status *cluster_status)
{
	unsigned int pwr_idx = 0;
	int i;

	for_each_ppm_clusters(i) {
		int core = cluster_status[i].core_num;
		int opp = cluster_status[i].freq_idx;

#ifdef CONFIG_MTK_UNIFY_POWER
		if (core > 0 && opp >= 0 && opp < DVFS_OPP_NUM)
			pwr_idx += cobra_tbl.ptbl[4*i+core-1][opp].power_idx;
#else
		pwr_idx += 100;
#endif

		ppm_ver("[%d] core = %d, opp = %d\n", i, core, opp);
	}

	if (!pwr_idx) {
		ppm_warn("@%s: pwr_idx is 0!\n", __func__);
		return -1; /* not found */
	}

	ppm_ver("@%s: pwr_idx = %d\n", __func__, pwr_idx);

	return pwr_idx;
}

int ppm_get_min_pwr_idx(void)
{
	struct ppm_cluster_status status[NR_PPM_CLUSTERS];
	int i;

	for_each_ppm_clusters(i) {
		/* use max core for ACAO */
		status[i].core_num = get_cluster_max_cpu_core(i);
		status[i].freq_idx = get_cluster_min_cpufreq_idx(i);
	}

	return ppm_find_pwr_idx(status);
}

int ppm_get_max_pwr_idx(void)
{
	struct ppm_cluster_status status[NR_PPM_CLUSTERS];
	int i;

	for_each_ppm_clusters(i) {
		status[i].core_num = get_cluster_max_cpu_core(i);
		status[i].freq_idx = get_cluster_max_cpufreq_idx(i);
	}

	return ppm_find_pwr_idx(status);
}

unsigned int ppm_calc_total_power(struct ppm_cluster_status *cluster_status,
			unsigned int cluster_num, unsigned int percentage)
{
	unsigned int dynamic, lkg, total, budget = 0;
	int i;
	ktime_t now, delta;

	for (i = 0; i < cluster_num; i++) {
		int core = cluster_status[i].core_num;
		int opp = cluster_status[i].freq_idx;

		if (core != 0 && opp >= 0 && opp < DVFS_OPP_NUM) {
			now = ktime_get();
#ifdef CONFIG_MTK_UNIFY_POWER
			dynamic = upower_get_power(i, opp, UPOWER_DYN) / 1000;
			lkg = mt_ppm_get_leakage_mw((enum ppm_cluster_lkg)i);
			total = ((((dynamic * 100 + (percentage - 1))
				/ percentage) + lkg) * core)
				+ ((upower_get_power(i+NR_PPM_CLUSTERS,
				opp, UPOWER_DYN) + upower_get_power(
				i+NR_PPM_CLUSTERS, opp, UPOWER_LKG)) / 1000);
#else
			dynamic = 100;
			lkg = 50;
			total = dynamic + lkg;
#endif
			budget += total;
			delta = ktime_sub(ktime_get(), now);

			ppm_dbg(DLPT,
				"%d:OP/V/C/Lkg/TO=%d/%d/%d/%d/%d(t=%lldus)\n",
				i, opp, cluster_status[i].volt,
				cluster_status[i].core_num,
				lkg, total, ktime_to_us(delta));
		}
	}

	if (!budget) {
		ppm_warn("@%s: pwr_idx is 0!\n", __func__);
		return -1; /* not found */
	}

	ppm_dbg(DLPT, "@%s: total budget = %d\n", __func__, budget);

	return budget;
}

unsigned int mt_ppm_get_leakage_mw(enum ppm_cluster_lkg cluster)
{
	int temp, dev_id, volt;
	unsigned int mw = 0;

	/* read total leakage */
	if (cluster >= TOTAL_CLUSTER_LKG) {
		struct ppm_cluster_status cl_status[NR_PPM_CLUSTERS];
		int i;

		ppm_get_cluster_status(cl_status);

		for_each_ppm_clusters(i) {
			if (!cl_status[i].core_num)
				continue;
#ifdef CONFIG_THERMAL
			temp = ppm_get_cpu_temp((enum ppm_cluster)i);
#else
			temp = 85;
#endif
			volt = mt_cpufreq_get_cur_volt(
				(enum mt_cpu_dvfs_id)i) / 100;
			dev_id = ppm_get_spower_devid((enum ppm_cluster)i);
			if (dev_id < 0)
				return 0;

			mw += mt_spower_get_leakage(dev_id, volt, temp);
		}
	} else {
#ifdef CONFIG_THERMAL
		temp = ppm_get_cpu_temp((enum ppm_cluster)cluster);
#else
		temp = 85;
#endif
		volt = mt_cpufreq_get_cur_volt(
			(enum mt_cpu_dvfs_id)cluster) / 100;
		dev_id = ppm_get_spower_devid((enum ppm_cluster)cluster);
		if (dev_id < 0)
			return 0;

		mw = mt_spower_get_leakage(dev_id, volt, temp);
	}

	return mw;
}

unsigned int get_cluster_ptpod_fix_freq_idx(unsigned int id)
{
	int val = mt_cpufreq_get_cpu_level();

	if (val == 5)
		return PTPOD_FREQ_IDX_LY;
	else
		return PTPOD_FREQ_IDX;
}
