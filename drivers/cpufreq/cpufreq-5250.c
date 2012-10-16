/* linux/arch/arm/mach-exynos/cpufreq-5250.c
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5250 - CPU frequency scaling support
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

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/regs-pmu-5250.h>
#include <mach/cpufreq.h>

#include <plat/clock.h>
#include <plat/cpu.h>

#define CPUFREQ_LEVEL_END	(L20 + 1)

#undef PRINT_DIV_VAL

#undef ENABLE_CLKOUT

static int max_support_idx;
static int min_support_idx = (CPUFREQ_LEVEL_END - 1);
static struct clk *cpu_clk;
static struct clk *moutcore;
static struct clk *mout_mpll;
static struct clk *mout_apll;

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv;
	unsigned int	clkdiv1;
};

static unsigned int exynos5250_volt_table[CPUFREQ_LEVEL_END];

static struct cpufreq_frequency_table exynos5250_freq_table[] = {
	{L0, 2200*1000},
	{L1, 2100*1000},
	{L2, 2000*1000},
	{L3, 1900*1000},
	{L4, 1800*1000},
	{L5, 1700*1000},
	{L6, 1600*1000},
	{L7, 1500*1000},
	{L8, 1400*1000},
	{L9, 1300*1000},
	{L10, 1200*1000},
	{L11, 1100*1000},
	{L12, 1000*1000},
	{L13, 900*1000},
	{L14, 800*1000},
	{L15, 700*1000},
	{L16, 600*1000},
	{L17, 500*1000},
	{L18, 400*1000},
	{L19, 300*1000},
	{L20, 200*1000},
	{0, CPUFREQ_TABLE_END},
};

static struct cpufreq_clkdiv exynos5250_clkdiv_table[CPUFREQ_LEVEL_END];

static unsigned int clkdiv_cpu0_5250[CPUFREQ_LEVEL_END][8] = {
	/*
	 * Clock divider value for following
	 * { ARM, CPUD, ACP, PERIPH, ATB, PCLK_DBG, APLL, ARM2 }
	 */
	{ 0, 5, 7, 7, 7, 1, 5, 0 },	/* L0: 2200Mhz */
	{ 0, 5, 7, 7, 7, 1, 5, 0 },	/* L1: 2100Mhz */
	{ 0, 5, 7, 7, 7, 1, 5, 0 },	/* L2: 2000Mhz */
	{ 0, 4, 7, 7, 7, 1, 5, 0 },	/* L3: 1900Mhz */
	{ 0, 4, 7, 7, 7, 1, 4, 0 },	/* L4: 1800Mhz */
	{ 0, 4, 7, 7, 7, 1, 4, 0 },	/* L5: 1700Mhz */
	{ 0, 4, 7, 7, 7, 1, 4, 0 },	/* L6: 1600MHz */
	{ 0, 3, 7, 7, 7, 1, 4, 0 },	/* L7: 1500Mhz */
	{ 0, 3, 7, 7, 6, 1, 3, 0 },	/* L8: 1400Mhz */
	{ 0, 3, 7, 7, 6, 1, 3, 0 },	/* L9: 1300Mhz */
	{ 0, 3, 7, 7, 5, 1, 3, 0 },	/* L10: 1200Mhz */
	{ 0, 2, 7, 7, 5, 1, 2, 0 },	/* L11: 1100MHz */
	{ 0, 2, 7, 7, 4, 1, 2, 0 },	/* L12: 1000MHz */
	{ 0, 2, 7, 7, 4, 1, 2, 0 },	/* L13: 900MHz */
	{ 0, 2, 7, 7, 3, 1, 1, 0 },	/* L14: 800MHz */
	{ 0, 1, 7, 7, 3, 1, 1, 0 },	/* L15: 700MHz */
	{ 0, 1, 7, 7, 2, 1, 1, 0 },	/* L16: 600MHz */
	{ 0, 1, 7, 7, 2, 1, 1, 0 },	/* L17: 500MHz */
	{ 0, 1, 7, 7, 1, 1, 1, 0 },	/* L18: 400MHz */
	{ 0, 1, 7, 7, 1, 1, 1, 0 },	/* L19: 300MHz */
	{ 0, 1, 7, 7, 1, 1, 1, 0 },	/* L20: 200MHz */
};

static unsigned int clkdiv_cpu1_5250[CPUFREQ_LEVEL_END][2] = {
	/* Clock divider value for following
	 * { COPY, HPM }
	 */
	{ 0, 2 },	/* L0: 2200Mhz */
	{ 0, 2 },	/* L1: 2100Mhz */
	{ 0, 2 },	/* L2: 2000Mhz */
	{ 0, 2 },	/* L3: 1900Mhz */
	{ 0, 2 },	/* L4: 1800Mhz */
	{ 0, 2 },	/* L5: 1700Mhz */
	{ 0, 2 },	/* L6: 1600MHz */
	{ 0, 2 },	/* L7: 1500Mhz */
	{ 0, 2 },	/* L8: 1400Mhz */
	{ 0, 2 },	/* L9: 1300Mhz */
	{ 0, 2 },	/* L10: 1200Mhz */
	{ 0, 2 },	/* L11: 1100MHz */
	{ 0, 2 },	/* L12: 1000MHz */
	{ 0, 2 },	/* L13: 900MHz */
	{ 0, 2 },	/* L14: 800MHz */
	{ 0, 2 },	/* L15: 700MHz */
	{ 0, 2 },	/* L16: 600MHz */
	{ 0, 2 },	/* L17: 500MHz */
	{ 0, 2 },	/* L18: 400MHz */
	{ 0, 2 },	/* L19: 300MHz */
	{ 0, 2 },	/* L20: 200MHz */
};

static unsigned int exynos5_apll_pms_table[CPUFREQ_LEVEL_END] = {
	((275<<16)|(3<<8)|(0)),	/* L0: 2200Mhz */
	((350<<16)|(4<<8)|(0)),	/* L1: 2100Mhz */
	((250<<16)|(3<<8)|(0)),	/* L2: 2000Mhz */
	((475<<16)|(6<<8)|(0)),	/* L3: 1900Mhz */
	((225<<16)|(3<<8)|(0)),	/* L4: 1800Mhz */
	((425<<16)|(6<<8)|(0)),	/* L5: 1700Mhz */
	((200<<16)|(3<<8)|(0)),	/* L6: 1600MHz */
	((250<<16)|(4<<8)|(0)),	/* L7: 1500Mhz */
	((175<<16)|(3<<8)|(0)),	/* L8: 1400Mhz */
	((325<<16)|(6<<8)|(0)),	/* L9: 1300Mhz */
	((200<<16)|(4<<8)|(0)),	/* L10: 1200Mhz */
	((275<<16)|(6<<8)|(0)),	/* L11: 1100MHz */
	((125<<16)|(3<<8)|(0)),	/* L12: 1000MHz */
	((150<<16)|(4<<8)|(0)),	/* L13: 900MHz */
	((100<<16)|(3<<8)|(0)),	/* L14: 800MHz */
	((175<<16)|(3<<8)|(1)),	/* L15: 700MHz */
	((200<<16)|(4<<8)|(1)),	/* L16: 600MHz */
	((125<<16)|(3<<8)|(1)),	/* L17: 500MHz */
	((100<<16)|(3<<8)|(1)),	/* L18: 400MHz */
	((200<<16)|(4<<8)|(2)),	/* L19: 300MHz */
	((100<<16)|(3<<8)|(2)),	/* L20: 200MHz */
};

/*
 * ASV group voltage table
 */

#define NUM_ASV_GROUP	1

static const unsigned int asv_voltage[CPUFREQ_LEVEL_END][NUM_ASV_GROUP] = {
	{ 0 },	/* L0 */
	{ 0 },	/* L1 */
	{ 0 },	/* L2 */
	{ 0 },	/* L3 */
	{ 0 },	/* L4 */
	{ 0 },	/* L5 */
	{ 0 },	/* L6 */
	{ 0 },	/* L7 */
	{ 1250000 },	/* L8 */
	{ 1200000 },	/* L9 */
	{ 1150000 },	/* L10 */
	{ 1100000 },	/* L11 */
	{ 1175000 },	/* L12 */
	{ 1125000 },	/* L13 */
	{ 1075000 },	/* L14 */
	{ 1050000 },	/* L15 */
	{ 1000000 },	/* L16 */
	{ 950000 },	/* L17 */
	{ 925000 },	/* L18 */
	{ 925000 },	/* L19 */
	{ 900000 },	/* L20 */
};

#if defined(CONFIG_EXYNOS5250_ABB_WA)
#define ARM_RBB		6	/* +300mV */
unsigned int exynos5250_arm_volt;

#define INT_VOLT	1050000
#endif

static void set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - CPU0 */

	tmp = exynos5250_clkdiv_table[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS5_CLKDIV_CPU0);

	do {
		tmp = __raw_readl(EXYNOS5_CLKDIV_STATCPU0);
	} while (tmp & 0x11111111);

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5_CLKDIV_CPU0);
	pr_info("DIV_CPU0[0x%x]\n", tmp);

#endif

	/* Change Divider - CPU1 */
	tmp = exynos5250_clkdiv_table[div_index].clkdiv1;

	__raw_writel(tmp, EXYNOS5_CLKDIV_CPU1);

	do {
		tmp = __raw_readl(EXYNOS5_CLKDIV_STATCPU1);
	} while (tmp & 0x11);
#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS5_CLKDIV_CPU1);
	pr_info("DIV_CPU1[0x%x]\n", tmp);
#endif
}

static void set_apll(unsigned int new_index,
			     unsigned int old_index)
{
	unsigned int tmp, pdiv;

	/* 1. MUX_CORE_SEL = MPLL,
	 * ARMCLK uses MPLL for lock time */
	if (clk_set_parent(moutcore, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_mpll->name, moutcore->name);

	do {
		tmp = (__raw_readl(EXYNOS5_CLKMUX_STATCPU) >> 16);
		tmp &= 0x7;
	} while (tmp != 0x2);

	/* 2. Set APLL Lock time */
	pdiv = ((exynos5_apll_pms_table[new_index] >> 8) & 0x3f);

	__raw_writel((pdiv * 250), EXYNOS5_APLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS5_APLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos5_apll_pms_table[new_index];
	__raw_writel(tmp, EXYNOS5_APLL_CON0);

	/* 4. wait_lock_time */
	do {
		tmp = __raw_readl(EXYNOS5_APLL_CON0);
	} while (!(tmp & (0x1 << 29)));

	/* 5. MUX_CORE_SEL = APLL */
	if (clk_set_parent(moutcore, mout_apll))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_apll->name, moutcore->name);

	do {
		tmp = __raw_readl(EXYNOS5_CLKMUX_STATCPU);
		tmp &= (0x7 << 16);
	} while (tmp != (0x1 << 16));

}

bool exynos5250_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = (exynos5_apll_pms_table[old_index] >> 8);
	unsigned int new_pm = (exynos5_apll_pms_table[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}

#if defined(CONFIG_EXYNOS5250_ABB_WA)
static DEFINE_SPINLOCK(abb_lock);
void exynos5250_set_arm_abbg(unsigned int arm_volt, unsigned int int_volt)
{
	unsigned int setbits = 8;
	unsigned int tmp, diff_volt;
	unsigned long flag;

	spin_lock_irqsave(&abb_lock, flag);
	if (arm_volt >= int_volt) {
		diff_volt = arm_volt - int_volt;
		setbits += diff_volt / 50000;
	} else {
		diff_volt = int_volt - arm_volt;
		setbits -= diff_volt / 50000;
	}
	tmp = __raw_readl(EXYNOS5_ABBG_ARM_CONTROL);
	tmp &= ~(0x1f | (1 << 31) | (1 << 7));
	tmp |= ((setbits + ARM_RBB) | (1 << 31) | (1 << 7));
	__raw_writel(tmp, EXYNOS5_ABBG_ARM_CONTROL);
	spin_unlock_irqrestore(&abb_lock, flag);
	pr_info("%s: ARM ABB[%d]\n", __func__, (setbits + ARM_RBB));
}
EXPORT_SYMBOL(exynos5250_set_arm_abbg);
#endif

static void exynos5250_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	unsigned int tmp;
#if defined(CONFIG_EXYNOS5250_ABB_WA)
	unsigned int voltage;

	voltage = asv_voltage[new_index][0];
	exynos5250_set_arm_abbg(voltage, INT_VOLT);
#endif
	if (old_index > new_index) {
		if (!exynos5250_pms_change(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			set_clkdiv(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_APLL_CON0);

		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			set_clkdiv(new_index);
			/* 2. Change the apll m,p,s value */
			set_apll(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos5250_pms_change(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS5_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos5_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS5_APLL_CON0);
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

static void __init set_volt_table(void)
{
	unsigned int asv_group = 0;
	unsigned int i;

	if (soc_is_exynos5250()) {
		exynos5250_freq_table[L0].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L1].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L2].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L3].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L4].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L5].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L6].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L7].frequency = CPUFREQ_ENTRY_INVALID;
#ifdef CONFIG_EXYNOS5250_1400MHZ_SUPPORT
		max_support_idx = L8;
#elif defined(CONFIG_EXYNOS5250_1200MHZ_SUPPORT)
		exynos5250_freq_table[L8].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L9].frequency = CPUFREQ_ENTRY_INVALID;

		max_support_idx = L10;
#else
		exynos5250_freq_table[L8].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L9].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L10].frequency = CPUFREQ_ENTRY_INVALID;
		exynos5250_freq_table[L11].frequency = CPUFREQ_ENTRY_INVALID;

		max_support_idx = L12;
#endif
	}

	pr_info("DVFS : VDD_ARM Voltage table set with %d Group\n", asv_group);

	for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++)
		exynos5250_volt_table[i] = asv_voltage[i][asv_group];
}

int exynos5250_cpufreq_init(struct exynos_dvfs_info *info)
{
	int i;
	unsigned int tmp;
	unsigned long rate;

	set_volt_table();

	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	moutcore = clk_get(NULL, "moutcpu");
	if (IS_ERR(moutcore))
		goto err_moutcore;

	mout_mpll = clk_get(NULL, "mout_mpll");
	if (IS_ERR(mout_mpll))
		goto err_mout_mpll;

	rate = clk_get_rate(mout_mpll) / 1000;

	mout_apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(mout_apll))
		goto err_mout_apll;

	for (i = L0; i <  CPUFREQ_LEVEL_END; i++) {

		exynos5250_clkdiv_table[i].index = i;

		tmp = __raw_readl(EXYNOS5_CLKDIV_CPU0);

		tmp &= ~((0x7 << 0) | (0x7 << 4) | (0x7 << 8) |
			(0x7 << 12) | (0x7 << 16) | (0x7 << 20) |
			(0x7 << 24) | (0x7 << 28));

		tmp |= ((clkdiv_cpu0_5250[i][0] << 0) |
			(clkdiv_cpu0_5250[i][1] << 4) |
			(clkdiv_cpu0_5250[i][2] << 8) |
			(clkdiv_cpu0_5250[i][3] << 12) |
			(clkdiv_cpu0_5250[i][4] << 16) |
			(clkdiv_cpu0_5250[i][5] << 20) |
			(clkdiv_cpu0_5250[i][6] << 24) |
			(clkdiv_cpu0_5250[i][7] << 28));

		exynos5250_clkdiv_table[i].clkdiv = tmp;

		tmp = __raw_readl(EXYNOS5_CLKDIV_CPU1);

		tmp &= ~((0x7 << 0) | (0x7 << 4));

		tmp |= ((clkdiv_cpu1_5250[i][0] << 0) |
			(clkdiv_cpu1_5250[i][1] << 4));

		exynos5250_clkdiv_table[i].clkdiv1 = tmp;
	}

	info->mpll_freq_khz = rate;
	/* 1000Mhz */
	info->pm_lock_idx = L12;
	/* 800Mhz */
	info->pll_safe_idx = L14;
	info->max_support_idx = max_support_idx;
	info->min_support_idx = min_support_idx;
	info->cpu_clk = cpu_clk;
	info->volt_table = exynos5250_volt_table;
	info->freq_table = exynos5250_freq_table;
	info->set_freq = exynos5250_set_frequency;
	info->need_apll_change = exynos5250_pms_change;

#ifdef ENABLE_CLKOUT
	tmp = __raw_readl(EXYNOS5_CLKOUT_CMU_CPU);
	p &= ~0xffff;
	tmp |= 0x1904;
	__raw_writel(tmp, EXYNOS5_CLKOUT_CMU_CPU);

	tmp = __raw_readl(S5P_PMU_DEBUG);
	tmp &= ~0xf00;
	tmp |= 0x400;
	__raw_writel(tmp, S5P_PMU_DEBUG);

#endif
	return 0;

err_mout_apll:
	if (!IS_ERR(mout_mpll))
		clk_put(mout_mpll);
err_mout_mpll:
	if (!IS_ERR(moutcore))
		clk_put(moutcore);
err_moutcore:
	if (!IS_ERR(cpu_clk))
		clk_put(cpu_clk);

	pr_err("%s: failed initialization\n", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(exynos5250_cpufreq_init);
