/* include/linux/exynos-cpufreq.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPUFreq support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __H_EXYNOS_CPUFREQ_H__
#define __H_EXYNOS_CPUFREQ_H__

#include <linux/cpufreq.h>

enum cpufreq_level_index {
	L0, L1, L2, L3, L4,
	L5, L6, L7, L8, L9,
	L10, L11, L12, L13, L14,
	L15, L16, L17, L18, L19,
	L20, L_END,
};

enum exynos4_cpufreq_type {
	TYPE_CPUFREQ_EXYNOS4210,
	TYPE_CPUFREQ_EXYNOS4212,
	TYPE_CPUFREQ_EXYNOS4412,
};

struct regulator_dev;
struct exynos_dvfs_info {
	struct device *dev;
	struct regulator *vdd_arm;
	struct regulator_dev *r_dev;
	struct cpufreq_driver driver;
	struct cpufreq_freqs freqs;
	struct cpufreq_frequency_table *full_freq_table;
	struct cpufreq_frequency_table *used_freq_table;

	unsigned int full_freq_num;		//supported max cpufreq level
	unsigned int *full_volt_table;
	unsigned int *used_volt_table;
	unsigned int volt_table_num;
	int **freq_profile_index;
	unsigned int pfm_id;
	bool gpio_dvfs;		// To support adjust voltage by gpio

	struct blocking_notifier_head exynos_dvfs;
	struct notifier_block reboot_notifier;
	struct notifier_block pm_notifier;
	struct notifier_block pfm_notifier;

	struct mutex freq_lock;
	struct mutex pfm_lock;

	enum exynos4_cpufreq_type cputype;

	unsigned long	pm_lock_freq;
	unsigned long	mpll_freq_khz;
	unsigned int	pll_safe_idx;
	bool cpufreq_accessable;

	void (*set_freq)(unsigned int, unsigned int);
	bool (*need_apll_change)(unsigned int, unsigned int);
	int (*update_dvs_voltage)(struct regulator_dev *, const int *,const unsigned int);
};

struct exynos_cpufreq_platdata {
	char *regulator;
	bool gpio_dvfs;
	unsigned int polling_ms; /* 0 to use default(50) */
};

extern void exynos_cpufreq_set_platdata(struct exynos_cpufreq_platdata *);

/*Update PMIC DVS table*/
#if defined(CONFIG_REGULATOR_MAX8997)
extern int max8997_update_dvs_voltage(struct regulator_dev *rdev,
				const int * volt_table,const unsigned int size);
#endif

#ifdef CONFIG_REGULATOR_MAX77686
extern int max77686_update_dvs_voltage(struct regulator_dev *rdev, 
				const int *volt_table, const unsigned int size);
#endif

#endif
