/* linux/arch/arm/mach-exynos/cpufreq-4210.c
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4210 - CPU frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <linux/performance.h>
#include <linux/exynos-cpufreq.h>

#include <mach/regs-clock.h>
#include <mach/asv.h>

#include <plat/clock.h>

#define CPUFREQ_LEVEL_END	(L7 + 1)

#define SUPPORT_1400MHZ		(1<<31)
#define SUPPORT_1200MHZ		(1<<30)
#define SUPPORT_1000MHZ		(1<<29)
#define SUPPORT_FREQ_SHIFT	29
#define SUPPORT_FREQ_MASK	7

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv;
};

static struct cpufreq_frequency_table exynos4210_freq_table[] = {
	{L0, 1400*1000},
	{L1, 1200*1000},
	{L2, 1000*1000},
	{L3, 800*1000},
	{L4, 600*1000},
	{L5, 500*1000},
	{L6, 400*1000},
	{L7, 200*1000},
	{0, CPUFREQ_TABLE_END},
};

/*
 * Select any of the supported levels just u want.
 * the supported levels is in exynos4210_freq_table(L0~L7)
 * above. it's very simple :)
*/

static int cpufreq_profile_high[] = {
	L0, L1, L2, L3, L4, L5, L6, L7, L_END
};

static int cpufreq_profile_med[] = {
	L2, L3, L4, L5, L6, L7, L_END
};

static int cpufreq_profile_low[] = {
	L3, L4, L5, L6, L7, L_END
};

static int *cpufreq_profile_index[] = {
	cpufreq_profile_high,
	cpufreq_profile_med,
	cpufreq_profile_low,
};

static struct cpufreq_clkdiv exynos4210_clkdiv_table[CPUFREQ_LEVEL_END];

static unsigned int clkdiv_cpu0[CPUFREQ_LEVEL_END][7] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM0, DIVCOREM1, DIVPERIPH,
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL }
	 */
	/* ARM L0: 1400MHz */
	{ 0, 3, 7, 3, 4, 1, 7 },

	/* ARM L1: 1200MHz */
	{ 0, 3, 7, 3, 4, 1, 7 },

	/* ARM L2: 1000MHz */
	{ 0, 3, 7, 3, 4, 1, 7 },

	/* ARM L3: 800MHz */
	{ 0, 3, 7, 3, 3, 1, 7 },
	
	/* ARM L4: 600MHz */
	{ 0, 3, 7, 3, 3, 1, 7 },

	/* ARM L5: 500MHz */
	{ 0, 3, 7, 3, 3, 1, 7 },
	
	/* ARM L6: 400MHz */
	{ 0, 3, 7, 3, 3, 1, 7 },

	/* ARM L7: 200MHz */
	{ 0, 1, 3, 1, 3, 1, 0 },
};

static unsigned int clkdiv_cpu1[CPUFREQ_LEVEL_END][2] = {
	/* Clock divider value for following
	 * { DIVCOPY, DIVHPM }
	 */
	/* ARM L0: 1400MHz */
	{ 5, 0 },

	/* ARM L1: 1200MHz */
	{ 5, 0 },

	/* ARM L2: 1000MHz */
	{ 4, 0 },

	/* ARM L3: 800MHz */
	{ 3, 0 },

	/* ARM L4: 600MHz */
	{ 3, 0 },

	/* ARM L5: 500MHz */
	{ 3, 0 },

	/* ARM L6: 400MHz */
	{ 3, 0 },

	/* ARM L7: 200MHz */
	{ 3, 0 },
};

static unsigned int exynos4_apll_pms_table[CPUFREQ_LEVEL_END] = {
	/* APLL FOUT L0: 1400MHz */
	((350<<16)|(6<<8)|(0x1)),

	/* APLL FOUT L1: 1200MHz */
	((300<<16)|(6<<8)|(0x1)),

	/* APLL FOUT L2: 1000MHz */
	((250<<16)|(6<<8)|(0x1)),

	/* APLL FOUT L3: 800MHz */
	((200<<16)|(6<<8)|(0x1)),
	
	/* APLL FOUT L4: 600MHz */
	((300<<16)|(6<<8)|(0x2)),
	
	/* APLL FOUT L5: 500MHz */
	((250<<16)|(6<<8)|(0x2)),

	/* APLL FOUT L6: 400MHz */
	((200<<16)|(6<<8)|(0x2)),

	/* APLL FOUT L7: 200MHz */
	((200<<16)|(6<<8)|(0x3)),
};

/*
 * ASV group voltage table
 */
static const unsigned int asv_voltage_A[CPUFREQ_LEVEL_END][8] = {
	/*
	 *	   SS, A1, A2, B1, B2, C1, C2, D
	 * @Dummy:
	 * @1200 :
	 * @1000 :
	 * @800	 :	ASV_VOLTAGE_TABLE
	 * @500  :
	 * @200  :
	 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1350000, 1350000, 1300000, 1275000, 1250000, 1250000, 1200000, 1175000 },
	{ 1300000, 1250000, 1200000, 1175000, 1150000, 1150000, 1100000, 1075000 },
	{ 1200000, 1150000, 1100000, 1075000, 1050000, 1050000, 1000000, 975000 },
	{ 1100000, 1050000, 1000000, 975000, 975000, 975000, 925000, 925000 },
	{ 1050000, 1000000, 975000, 950000, 950000, 925000, 925000, 925000 },

};

static const unsigned int asv_voltage_B[CPUFREQ_LEVEL_END][5] = {
	/*
	 *	   S, A, B, C, D
	 * @1400 :
	 * @1200 :
	 * @1000 :
	 * @800	 :	ASV_VOLTAGE_TABLE
	 * @600	 :
	 * @500	 :
	 * @400	 :
	 * @200	 :
	 */
	{ 1375000, 1375000, 1350000, 1300000, 1275000 },
	{ 1350000, 1300000, 1250000, 1200000, 1175000 },
	{ 1250000, 1200000, 1150000, 1100000, 1075000 },
	{ 1175000, 1125000, 1075000, 1025000, 1000000 },
	{ 1175000, 1125000, 1025000, 1025000, 1000000 },
	{ 1075000, 1025000, 1000000, 1000000,  975000 },
	{ 1075000, 1000000,  975000,  975000,  975000 },
	{ 1050000, 1000000,  975000,  975000,  975000 },
};

static void set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;
	/* Change Divider - CPU0 */

	tmp = exynos4210_clkdiv_table[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STATCPU);
	} while (tmp & 0x1111111);

	/* Change Divider - CPU1 */
	tmp = __raw_readl(EXYNOS4_CLKDIV_CPU1);

	tmp &= ~((0x7 << 4) | (0x7));

	tmp |= ((clkdiv_cpu1[div_index][0] << 4) |
		(clkdiv_cpu1[div_index][1] << 0));

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU1);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STATCPU1);
	} while (tmp & 0x11);
}

static void set_apll(unsigned int new_index,
			     unsigned int old_index)
{
	struct clk *moutcore, *mout_mpll, *mout_apll;
	unsigned int tmp;

	moutcore = clk_get(NULL, "moutcore");
	if (IS_ERR(moutcore)) {
		pr_err("%s: failed to clk_get moutcore\n", __func__);
		return;
	}

	mout_mpll = clk_get(NULL, "mout_mpll");
	if (IS_ERR(mout_mpll)) {
		pr_err("%s: failed to clk_get mout_mpll\n", __func__);
		goto err_mpll;
	}

	mout_apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(mout_apll)) {
		pr_err("%s: failed to clk_get mout_apll\n", __func__);
		goto err_apll;
	}

	/* 1. MUX_CORE_SEL = MPLL,
	 * ARMCLK uses MPLL for lock time */
	if (clk_set_parent(moutcore, mout_mpll))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				mout_mpll->name, moutcore->name);

	do {
		tmp = (__raw_readl(EXYNOS4_CLKMUX_STATCPU)
			>> EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set APLL Lock time */
	__raw_writel(EXYNOS4_APLL_LOCKTIME, EXYNOS4_APLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS4_APLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos4_apll_pms_table[new_index];
	__raw_writel(tmp, EXYNOS4_APLL_CON0);

	/* 4. wait_lock_time */
	do {
		tmp = __raw_readl(EXYNOS4_APLL_CON0);
	} while (!(tmp & (0x1 << EXYNOS4_APLLCON0_LOCKED_SHIFT)));

	/* 5. MUX_CORE_SEL = APLL */
	if (clk_set_parent(moutcore, mout_apll))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				mout_apll->name, moutcore->name);

	do {
		tmp = __raw_readl(EXYNOS4_CLKMUX_STATCPU);
		tmp &= EXYNOS4_CLKMUX_STATCPU_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT));


	clk_put(mout_apll);
err_apll:
	clk_put(mout_mpll);
err_mpll:
	clk_put(moutcore);
}

bool exynos4210_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = (exynos4_apll_pms_table[old_index] >> 8);
	unsigned int new_pm = (exynos4_apll_pms_table[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos4210_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	unsigned int tmp;

	if (old_index > new_index) {
		if (!exynos4210_pms_change(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			set_clkdiv(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS4_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos4_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS4_APLL_CON0);

		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			set_clkdiv(new_index);
			/* 2. Change the apll m,p,s value */
			set_apll(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos4210_pms_change(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS4_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos4_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS4_APLL_CON0);
			/* 2. Change the system clock divider values */
			set_clkdiv(new_index);
		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the apll m,p,s value */
			set_apll(new_index, old_index);
			/* 2. Change the system clock divider values */
			set_clkdiv(new_index);
		}
	}
}

static void __init exynos4210_set_param(struct exynos_dvfs_info *info)
{
	unsigned int asv_group = 0;
	bool for_1400 = false, for_1200 = false, for_1000 = false;
	unsigned int tmp;
	unsigned int i;

	tmp = exynos_result_of_asv;

	asv_group = (tmp & 0xF);

	switch (tmp  & (SUPPORT_FREQ_MASK << SUPPORT_FREQ_SHIFT)) {
	case SUPPORT_1400MHZ:
		for_1400 = true;
		break;
	case SUPPORT_1200MHZ:
		for_1200 = true;
		break;
	case SUPPORT_1000MHZ:
		for_1000 = true;
		break;
	default:
		for_1000 = true;
		break;
	}

	/*
	 * If ASV group is S, can not support 1.4GHz
	 * Disabling table entry
	 */
	if ((asv_group == 0) || !for_1400)
		exynos4210_freq_table[L0].frequency = CPUFREQ_ENTRY_INVALID;

	if (for_1000)
		exynos4210_freq_table[L1].frequency = CPUFREQ_ENTRY_INVALID;

	pr_info("DVFS : EXYNOS4210 VDD_ARM Voltage table set with asv %d Group\n", asv_group);

	if (for_1400) {
		for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++) 
			info->full_volt_table[i] = asv_voltage_B[i][asv_group];
	} else {
		for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++) 
			info->full_volt_table[i] = asv_voltage_A[i][asv_group];
	}
}

static void __init exynos4210_cpufreq_div_init(void)
{
	unsigned int tmp, i;

	for (i = L0; i <  CPUFREQ_LEVEL_END; i++) {
		exynos4210_clkdiv_table[i].index = i;
	
		tmp = __raw_readl(EXYNOS4_CLKDIV_CPU);
	
		tmp &= ~(EXYNOS4_CLKDIV_CPU0_CORE_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM0_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM1_MASK |
			EXYNOS4_CLKDIV_CPU0_PERIPH_MASK |
			EXYNOS4_CLKDIV_CPU0_ATB_MASK |
			EXYNOS4_CLKDIV_CPU0_PCLKDBG_MASK |
			EXYNOS4_CLKDIV_CPU0_APLL_MASK);

		tmp |= ((clkdiv_cpu0[i][0] << EXYNOS4_CLKDIV_CPU0_CORE_SHIFT) |
			(clkdiv_cpu0[i][1] << EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT) |
			(clkdiv_cpu0[i][2] << EXYNOS4_CLKDIV_CPU0_COREM1_SHIFT) |
			(clkdiv_cpu0[i][3] << EXYNOS4_CLKDIV_CPU0_PERIPH_SHIFT) |
			(clkdiv_cpu0[i][4] << EXYNOS4_CLKDIV_CPU0_ATB_SHIFT) |
			(clkdiv_cpu0[i][5] << EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT) |
			(clkdiv_cpu0[i][6] << EXYNOS4_CLKDIV_CPU0_APLL_SHIFT));

		exynos4210_clkdiv_table[i].clkdiv = tmp;
	}
}

int __init exynos4210_cpufreq_init(struct exynos_dvfs_info *info)
{
	int i;
	int ret;
	unsigned long rate;
	struct clk *cpu_clk = NULL;
	struct clk *mpll_clk = NULL;
	
	info->full_volt_table = kzalloc(sizeof(*info->full_volt_table) *
					 CPUFREQ_LEVEL_END, GFP_KERNEL);

	if (IS_ERR_OR_NULL(info->full_volt_table)) {
		pr_err("failed to kzalloc full_volt_table\n");
		return PTR_ERR(info->full_volt_table);
	}
	info->volt_table_num = CPUFREQ_LEVEL_END;

#ifdef CONFIG_REGULATOR_MAX8997
	if (info->gpio_dvfs) {	
		info->update_dvs_voltage = max8997_update_dvs_voltage;
		/*max8997 support 8 level voltage setting only*/
		info->volt_table_num = 8;
	}
#endif

	info->used_volt_table = kzalloc(sizeof(*info->used_volt_table) * \
				info->volt_table_num, GFP_KERNEL);

	if (IS_ERR_OR_NULL(info->used_volt_table)) {
		pr_err("failed to kzalloc used_volt_table\n");
		ret = PTR_ERR(info->used_volt_table);
		goto err_free0;
	}
	
	info->used_freq_table = kzalloc(sizeof(struct cpufreq_frequency_table) * \
				 ARRAY_SIZE(exynos4210_freq_table), GFP_KERNEL);

	if (IS_ERR_OR_NULL(info->used_freq_table)) {
		pr_err("failed to kzalloc used_freq_table\n");
		ret = PTR_ERR(info->used_freq_table);
		goto err_free1;
	}
	
	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk)) {
		pr_err("failed to clk_get: armclk\n");
		ret = PTR_ERR(cpu_clk);
		goto err_clk0;
	}

	mpll_clk = clk_get(NULL, "mout_mpll");
	if (IS_ERR(mpll_clk)) {
		pr_err("failed to clk_get: mout_mpll\n");
		ret = PTR_ERR(mpll_clk);
		goto err_clk1;
	}

	rate = clk_get_rate(mpll_clk)/1000;
	info->mpll_freq_khz = rate;
	rate = clk_get_rate(cpu_clk)/1000;
	info->pm_lock_freq = rate;
	
	for (i=0; i<ARRAY_SIZE(exynos4210_freq_table); i++) {
		/*800MHZ*/
		if (exynos4210_freq_table[i].frequency == 800*1000) {
			info->pll_safe_idx = exynos4210_freq_table[i].index;
			break;
		}
	}

	info->full_freq_table = exynos4210_freq_table;
	info->full_freq_num = ARRAY_SIZE(exynos4210_freq_table);
	info->freq_profile_index = cpufreq_profile_index;
	info->pfm_id = INIT_CPU_PFM;
	info->set_freq = exynos4210_set_frequency;
	info->need_apll_change = exynos4210_pms_change;

	exynos4210_set_param(info);
	exynos4210_cpufreq_div_init();

	return 0;

err_clk1:
	clk_put(cpu_clk);
err_clk0:
	kfree(info->used_freq_table);
err_free1:	
	kfree(info->used_volt_table);
err_free0:
	kfree(info->full_volt_table);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(exynos4210_cpufreq_init);
