/* linux/arch/arm/mach-exynos/cpufreq-4x12.c
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4X12 - CPU frequency scaling support
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
#include <linux/exynos-cpufreq.h>
#include <linux/performance.h>

#include <mach/regs-clock.h>
#include <mach/asv.h>

#include <plat/cpu.h>
#include <plat/clock.h>

#undef PRINT_DIV_VAL

struct cpufreq_clkdiv {
	unsigned int	index;
	unsigned int	clkdiv;
	unsigned int	clkdiv1;
};

static int abb_arm = ABB_MODE_130V;
module_param_named(abb_arm, abb_arm, uint, 0644);

static int arm_bias = 0;	//sub 0.1v all level voltage
module_param_named(arm_bias, arm_bias, int, 0644);

/* Full freq table */
static struct cpufreq_frequency_table exynos4x12_freq_table[] = {
	{L0, 1600*1000},
	{L1, 1500*1000},
	{L2, 1400*1000},
	{L3, 1300*1000},
	{L4, 1200*1000},
	{L5, 1100*1000},
	{L6, 1000*1000},
	{L7, 900*1000},
	{L8, 800*1000},
	{L9, 700*1000},
	{L10, 600*1000},
	{L11, 500*1000},
	{L12, 400*1000},
	{L13, 300*1000},
	{L14, 200*1000},
	{L15, 100*1000},
	{0, CPUFREQ_TABLE_END},
};

#define CPUFREQ_LEVEL_END	(L15 + 1)

/*
 * Select any of the supported levels just u want.
 * the supported levels is in exynos4x12_freq_table(L0~L13)
 * above. it's very simple :)
*/

static int exynos4212_profile_high[] = {
	L1, L2, L4, L6, L8, L10, L12, L14, L_END
};

static int exynos4412_v2_profile_high[] = {
	L0, L2, L4, L6, L8, L10, L12, L14, L_END
};

static int exynos4412_profile_high[] = {
	L2, L4, L6, L8, L10, L12, L13, L14, L_END
};

static int exynos4412_profile_med[] = {
	L6, L8, L9, L10, L11, L12, L13, L14, L_END
};

static int exynos4412_profile_low[] = {
	L8, L9, L10, L11, L12, L13, L14, L15, L_END
};

/*Default for exynos4412*/
static int *cpufreq_profile_index[] = {
	exynos4412_profile_high, /*4412 high*/
	exynos4412_profile_med,
	exynos4412_profile_low,
};

static struct cpufreq_clkdiv exynos4x12_clkdiv_table[CPUFREQ_LEVEL_END];

static unsigned int clkdiv_cpu0_4212[CPUFREQ_LEVEL_END][8] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM0, DIVCOREM1, DIVPERIPH,
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL, DIVCORE2 }
	 */
	/* ARM L0: 1600Mhz */
	{ 0, 3, 7, 0, 6, 1, 2, 0 },

	/* ARM L1: 1500Mhz */
	{ 0, 3, 7, 0, 6, 1, 2, 0 },

	/* ARM L2: 1400Mhz */
	{ 0, 3, 7, 0, 6, 1, 2, 0 },

	/* ARM L3: 1300Mhz */
	{ 0, 3, 7, 0, 5, 1, 2, 0 },

	/* ARM L4: 1200Mhz */
	{ 0, 3, 7, 0, 5, 1, 2, 0 },

	/* ARM L5: 1100MHz */
	{ 0, 3, 6, 0, 4, 1, 2, 0 },

	/* ARM L6: 1000MHz */
	{ 0, 2, 5, 0, 4, 1, 1, 0 },

	/* ARM L7: 900MHz */
	{ 0, 2, 5, 0, 3, 1, 1, 0 },

	/* ARM L8: 800MHz */
	{ 0, 2, 5, 0, 3, 1, 1, 0 },

	/* ARM L9: 700MHz */
	{ 0, 2, 4, 0, 3, 1, 1, 0 },

	/* ARM L10: 600MHz */
	{ 0, 2, 4, 0, 3, 1, 1, 0 },

	/* ARM L11: 500MHz */
	{ 0, 2, 4, 0, 3, 1, 1, 0 },

	/* ARM L12: 400MHz */
	{ 0, 2, 4, 0, 3, 1, 1, 0 },

	/* ARM L13: 300MHz */
	{ 0, 2, 4, 0, 2, 1, 1, 0 },

	/* ARM L14: 200MHz */
	{ 0, 1, 3, 0, 1, 1, 1, 0 },

	/* ARM L15: 100MHz */
	{ 0, 1, 3, 0, 1, 1, 1, 0 },
};

static unsigned int clkdiv_cpu0_4412[CPUFREQ_LEVEL_END][8] = {
	/*
	 * Clock divider value for following
	 * { DIVCORE, DIVCOREM0, DIVCOREM1, DIVPERIPH,
	 *		DIVATB, DIVPCLK_DBG, DIVAPLL, DIVCORE2 }
	 */
	/* ARM L0: 1600Mhz */
	{ 0, 3, 7, 0, 6, 1, 7, 0 },

	/* ARM L1: 1500Mhz */
	{ 0, 3, 7, 0, 6, 1, 7, 0 },

	/* ARM L2: 1400Mhz */
	{ 0, 3, 7, 0, 6, 1, 6, 0 },

	/* ARM L3: 1300Mhz */
	{ 0, 3, 7, 0, 5, 1, 6, 0 },

	/* ARM L4: 1200Mhz */
	{ 0, 3, 7, 0, 5, 1, 5, 0 },

	/* ARM L5: 1100MHz */
	{ 0, 3, 6, 0, 4, 1, 5, 0 },

	/* ARM L6: 1000MHz */
	{ 0, 2, 5, 0, 4, 1, 4, 0 },

	/* ARM L7: 900MHz */
	{ 0, 2, 5, 0, 3, 1, 4, 0 },

	/* ARM L8: 800MHz */
	{ 0, 2, 5, 0, 3, 1, 3, 0 },

	/* ARM L9: 700MHz */
	{ 0, 2, 4, 0, 3, 1, 3, 0 },

	/* ARM L10: 600MHz */
	{ 0, 2, 4, 0, 3, 1, 2, 0 },

	/* ARM L11: 500MHz */
	{ 0, 2, 4, 0, 3, 1, 2, 0 },

	/* ARM L12: 400MHz */
	{ 0, 2, 4, 0, 3, 1, 1, 0 },

	/* ARM L13: 300MHz */
	{ 0, 2, 4, 0, 2, 1, 1, 0 },

	/* ARM L14: 200MHz */
	{ 0, 1, 3, 0, 1, 1, 1, 0 },

	/* ARM L15: 100MHz */
	{ 0, 1, 3, 0, 1, 1, 1, 0 },
};

static unsigned int clkdiv_cpu1_4212[CPUFREQ_LEVEL_END][2] = {
	/* Clock divider value for following
	 * { DIVCOPY, DIVHPM }
	 */
	/* ARM L0: 1600MHz */
	{ 6, 0 },

	/* ARM L1: 1500MHz */
	{ 6, 0 },

	/* ARM L2: 1400MHz */
	{ 6, 0 },

	/* ARM L3: 1300MHz */
	{ 5, 0 },

	/* ARM L4: 1200MHz */
	{ 5, 0 },

	/* ARM L5: 1100MHz */
	{ 4, 0 },

	/* ARM L6: 1000MHz */
	{ 4, 0 },

	/* ARM L7: 900MHz */
	{ 3, 0 },

	/* ARM L8: 800MHz */
	{ 3, 0 },

	/* ARM L9: 700MHz */
	{ 3, 0 },

	/* ARM L10: 600MHz */
	{ 3, 0 },

	/* ARM L11: 500MHz */
	{ 3, 0 },

	/* ARM L12: 400MHz */
	{ 3, 0 },

	/* ARM L13: 300MHz */
	{ 3, 0 },

	/* ARM L14: 200MHz */
	{ 3, 0 },

	/* ARM L15: 100MHz */
	{ 3, 0 },
};

static unsigned int clkdiv_cpu1_4412[CPUFREQ_LEVEL_END][3] = {
	/* Clock divider value for following
	 * { DIVCOPY, DIVHPM, DIVCORES }
	 */
	/* ARM L0: 1600MHz */
	{ 6, 0, 7 },

	/* ARM L1: 1500MHz */
	{ 6, 0, 7 },

	/* ARM L2: 1400MHz */
	{ 6, 0, 6 },

	/* ARM L3: 1300MHz */
	{ 5, 0, 6 },

	/* ARM L4: 1200MHz */
	{ 5, 0, 5 },

	/* ARM L5: 1100MHz */
	{ 4, 0, 5 },

	/* ARM L6: 1000MHz */
	{ 4, 0, 4 },

	/* ARM L7: 900MHz */
	{ 3, 0, 4 },

	/* ARM L8: 800MHz */
	{ 3, 0, 3 },

	/* ARM L9: 700MHz */
	{ 3, 0, 3 },

	/* ARM L10: 600MHz */
	{ 3, 0, 2 },

	/* ARM L11: 500MHz */
	{ 3, 0, 2 },

	/* ARM L12: 400MHz */
	{ 3, 0, 1 },

	/* ARM L13: 300MHz */
	{ 3, 0, 1 },

	/* ARM L14: 200MHz */
	{ 3, 0, 0 },

	/* ARM L15: 100MHz */
	{ 3, 0, 0 },
};

static unsigned int exynos4x12_apll_pms_table[CPUFREQ_LEVEL_END] = {
	/* APLL FOUT L0: 1600MHz */
	((200<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L1: 1500MHz */
	((250<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L2: 1400MHz */
	((175<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L3: 1300MHz */
	((325<<16)|(6<<8)|(0x0)),

	/* APLL FOUT L4: 1200MHz */
	((200<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L5: 1100MHz */
	((275<<16)|(6<<8)|(0x0)),

	/* APLL FOUT L6: 1000MHz */
	((125<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L7: 900MHz */
	((150<<16)|(4<<8)|(0x0)),

	/* APLL FOUT L8: 800MHz */
	((100<<16)|(3<<8)|(0x0)),

	/* APLL FOUT L9: 700MHz */
	((175<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L10: 600MHz */
	((200<<16)|(4<<8)|(0x1)),

	/* APLL FOUT L11: 500MHz */
	((125<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L12 400MHz */
	((100<<16)|(3<<8)|(0x1)),

	/* APLL FOUT L13: 300MHz */
	((200<<16)|(4<<8)|(0x2)),

	/* APLL FOUT L14: 200MHz */
	((100<<16)|(3<<8)|(0x2)),

	/* APLL FOUT L14: 100MHz */
	((100<<16)|(3<<8)|(0x3)),
};

/*
 * ASV group voltage table
 */

#define NO_ABB_LIMIT	L8

static const unsigned int asv_voltage_4212[CPUFREQ_LEVEL_END][12] = {
	/*   ASV0,    ASV1,    ASV2,    ASV3,	 ASV4,	  ASV5,	   ASV6,    ASV7,    ASV8,    ASV9,   ASV10,   ASV11 */
	{	0, 1300000, 1300000, 1275000, 1300000, 1287500,	1275000, 1250000, 1237500, 1225000, 1225000, 1212500 }, /* L0 */
	{	0, 1300000, 1300000, 1275000, 1300000, 1287500,	1275000, 1250000, 1237500, 1225000, 1225000, 1212500 }, /* L1 */
	{ 1300000, 1287500, 1250000, 1225000, 1237500, 1237500,	1225000, 1200000, 1187500, 1175000, 1175000, 1162500 }, /* L1 */
	{ 1237500, 1225000, 1200000, 1175000, 1187500, 1187500,	1162500, 1150000, 1137500, 1125000, 1125000, 1112500 }, /* L2 */
	{ 1187500, 1175000, 1150000, 1137500, 1150000, 1137500,	1125000, 1100000, 1087500, 1075000, 1075000, 1062500 }, /* L3 */
	{ 1137500, 1125000, 1112500, 1087500, 1112500, 1112500,	1075000, 1062500, 1050000, 1025000, 1025000, 1012500 }, /* L4 */
	{ 1100000, 1087500, 1075000, 1050000, 1075000, 1062500,	1037500, 1025000, 1012500, 1000000,  987500,  975000 }, /* L5 */
	{ 1050000, 1037500, 1025000, 1000000, 1025000, 1025000,	 987500,  975000,  962500,  950000,  937500,  925000 }, /* L6 */
	{ 1012500, 1000000,  987500,  962500,  987500,	975000,	 962500,  937500,  925000,  912500,  912500,  900000 }, /* L7 */
	{  962500,  950000,  937500,  912500,  937500,	937500,	 925000,  900000,  900000,  900000,  900000,  900000 }, /* L8 */
	{  925000,  912500,  912500,  900000,  912500,	900000,	 900000,  900000,  900000,  900000,  900000,  900000 }, /* L9 */
	{  912500,  900000,  900000,  900000,  900000,	900000,	 900000,  900000,  900000,  900000,  900000,  900000 }, /* L10 */
	{  912500,  900000,  900000,  900000,  900000,	900000,	 900000,  900000,  900000,  900000,  900000,  900000 }, /* L11 */
	{  912500,  900000,  900000,  900000,  900000,	900000,	 900000,  900000,  900000,  900000,  900000,  900000 }, /* L12 */
	{  912500,  900000,  900000,  900000,  900000,	900000,	 900000,  900000,  900000,  900000,  900000,  900000 }, /* L13 */
	{  900000,  887500,  887500,  887500,  887500,	887500,	 887500,  887500,  887500,  887500,  887500,  887500 }, /* L15 */
};

static const unsigned int asv_voltage_s[CPUFREQ_LEVEL_END] = {
	1300000, 1300000, 1300000, 1250000, 1200000, 1175000, 1100000,
	1050000, 1025000, 1000000, 1000000, 1000000, 950000, 950000, 950000, 900000,
};

/* ASV table for 12.5mV step */
static const unsigned int asv_voltage_4412[CPUFREQ_LEVEL_END][12] = {
	/*   ASV0,    ASV1,    ASV2,    ASV3,	 ASV4,	  ASV5,	   ASV6,    ASV7,    ASV8,    ASV9,   ASV10,   ASV11 */
	{	0,       0,	  0,	   0,	    0,	     0,	      0,       0,       0,       0,	  0,       0 },	/* L0 - Not used */
	{	0,       0,	  0,	   0,	    0,	     0,	      0,       0,       0,       0,	  0,       0 },	/* L1 - Not used */
	{ 1300000, 1300000, 1300000, 1287500, 1300000, 1287500, 1275000, 1250000, 1250000, 1237500, 1225000, 1212500 },	/* L2 */
	{ 1300000, 1275000, 1237500, 1237500, 1250000, 1250000,	1237500, 1212500, 1200000, 1200000, 1187500, 1175000 },	/* L3 */
	{ 1225000, 1212500, 1200000, 1187500, 1200000, 1187500,	1175000, 1150000, 1137500, 1125000, 1125000, 1112500 },	/* L4 */
	{ 1175000, 1162500, 1150000, 1137500, 1150000, 1137500,	1125000, 1100000, 1100000, 1075000, 1075000, 1062500 },	/* L5 */
	{ 1125000, 1112500, 1100000, 1087500, 1100000, 1087500,	1075000, 1050000, 1037500, 1025000, 1025000, 1012500 },	/* L6 */
	{ 1075000, 1062500, 1050000, 1050000, 1050000, 1037500,	1025000, 1012500, 1000000,  987500,  987500,  975000 },	/* L7 */
	{ 1037500, 1025000, 1000000, 1000000, 1000000,  987500,	 975000,  962500,  962500,  962500,  962500,  950000 },		/* L8 */
	{ 1012500, 1000000,  975000,  975000,  975000,  975000,	 962500,  962500,  950000,  950000,  950000,  937500 },		/* L9 */
	{ 1000000,  987500,  962500,  962500,  962500,  962500,	 950000,  950000,  937500,  937500,  937500,  925000 },		/* L10 */
	{  987500,  975000,  950000,  937500,  950000,  937500,	 937500,  937500,  912500,  912500,  912500,  900000 },			/* L11 */
	{  975000,  962500,  950000,  925000,  950000,  925000,	 925000,  925000,  900000,  900000,  900000,  887500 },			/* L12 */
	{  950000,  937500,  925000,  900000,  925000,  900000,	 900000,  900000,  900000,  887500,  875000,  862500 },			/* L13 */
	{  925000,  912500,  900000,  900000,  900000,  900000,	 900000,  900000,  887500,  875000,  875000,  862500 },			/* L14 */
	{  912500,  900000,  887500,  887500,  887500,  887500,	 887500,  887500,  875000,  862500,  862500,  850000 },			/* L15 */
};

static const unsigned int asv_voltage_4412_rev2[CPUFREQ_LEVEL_END][12] = {
	/*   ASV0,    ASV1,    ASV2,    ASV3,	 ASV4,	  ASV5,	   ASV6,    ASV7,    ASV8,    ASV9,   ASV10,   ASV11 */
	{ 1325000, 1312500, 1300000, 1287500, 1300000, 1287500, 1275000, 1250000, 1250000, 1237500, 1225000, 1212500 },	/* L0 */
	{ 1325000, 1312500, 1300000, 1287500, 1300000, 1287500,	1275000, 1250000, 1250000, 1237500, 1225000, 1212500 },	/* L1 */
	{ 1300000, 1275000, 1237500, 1237500, 1250000, 1250000, 1237500, 1212500, 1200000, 1200000, 1187500, 1175000 },	/* L2 */
	{ 1225000, 1212500, 1200000, 1187500, 1200000, 1187500, 1175000, 1150000, 1137500, 1125000, 1125000, 1112500 },	/* L3 */
	{ 1175000, 1162500, 1150000, 1137500, 1150000, 1137500, 1125000, 1100000, 1100000, 1075000, 1075000, 1062500 },	/* L4 */
	{ 1125000, 1112500, 1100000, 1087500, 1100000, 1087500, 1075000, 1050000, 1037500, 1025000, 1025000, 1012500 },	/* L5 */
	{ 1075000, 1062500, 1050000, 1050000, 1050000, 1037500, 1025000, 1012500, 1000000,  987500,  987500,  975000 },	/* L6 */
	{ 1037500, 1025000, 1000000, 1000000, 1000000,  987500,  975000,  962500,  962500,  962500,  962500,  950000 },	/* L7 */
	{ 1012500, 1000000,  975000,  975000,  975000,  975000,  962500,  962500,  950000,  950000,  950000,  937500 },	/* L8 */
	{ 1000000,  987500,  962500,  962500,  962500,  962500,  950000,  950000,  937500,  937500,  937500,  925000 },	/* L9 */
	{  987500,  975000,  950000,  937500,  950000,  937500,  937500,  937500,  912500,  912500,  912500,  900000 },	/* L10 */
	{  975000,  962500,  950000,  925000,  950000,  925000,  925000,  925000,  900000,  900000,  900500,  887500 },	/* L11 */
	{  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  875000,  875000,  862500 },	/* L12 */
	{  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  875000,  875000,  862500 },	/* L13 */
	{  925000,  912500,  900000,  900000,  900000,  900000,  900000,  900000,  887500,  875000,  875000,  862500 },	/* L14 */
	{  912500,  900000,  887500,  887500,  887500,  887500,	 887500,  887500,  875000,  862500,  862500,  850000 },		/* L15 */
};

static inline void set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;
	unsigned int stat_cpu1;

	/* Change Divider - CPU0 */

	tmp = exynos4x12_clkdiv_table[div_index].clkdiv;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STATCPU);
	} while (tmp & 0x11111111);

#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS4_CLKDIV_CPU);
	pr_info("DIV_CPU0[0x%x]\n", tmp);

#endif

	/* Change Divider - CPU1 */
	tmp = exynos4x12_clkdiv_table[div_index].clkdiv1;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU1);
	if (soc_is_exynos4212())
		stat_cpu1 = 0x11;
	else
		stat_cpu1 = 0x111;

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STATCPU1);
	} while (tmp & stat_cpu1);
#ifdef PRINT_DIV_VAL
	tmp = __raw_readl(EXYNOS4_CLKDIV_CPU1);
	pr_info("DIV_CPU1[0x%x]\n", tmp);
#endif
}

static inline void set_apll(unsigned int new_index,
			     unsigned int old_index)
{
	struct clk *moutcore, *mout_mpll, *mout_apll;
	unsigned int tmp, pdiv;

	moutcore = clk_get(NULL, "moutcore");
	if (IS_ERR(moutcore)) {
		pr_err("%s: failed to clk_get moutcore\n", __func__);
		return;
	}

	mout_mpll = clk_get(NULL, "mout_mpll");
	if (IS_ERR(mout_mpll)) {
		pr_err("%s: failed to clk_get mout_mpll\n", __func__);
		clk_put(moutcore);
		return;
	}

	mout_apll = clk_get(NULL, "mout_apll");
	if (IS_ERR(mout_mpll)) {
		pr_err("%s: failed to clk_get mout_apll\n", __func__);
		clk_put(moutcore);
		clk_put(mout_mpll);
		return;
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
	pdiv = ((exynos4x12_apll_pms_table[new_index] >> 8) & 0x3f);

	__raw_writel((pdiv * 250), EXYNOS4_APLL_LOCK);

	/* 3. Change PLL PMS values */
	tmp = __raw_readl(EXYNOS4_APLL_CON0);
	tmp &= ~((0x3ff << 16) | (0x3f << 8) | (0x7 << 0));
	tmp |= exynos4x12_apll_pms_table[new_index];
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

	clk_put(moutcore);
	clk_put(mout_mpll);
	clk_put(mout_apll);
}

static bool exynos4x12_pms_change(unsigned int old_index, unsigned int new_index)
{
	unsigned int old_pm = (exynos4x12_apll_pms_table[old_index] >> 8);
	unsigned int new_pm = (exynos4x12_apll_pms_table[new_index] >> 8);

	return (old_pm == new_pm) ? 0 : 1;
}

static void exynos4x12_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	unsigned int tmp;

	if (old_index == new_index) {
		WARN(1, "index = %d\n", old_index);
		return;
	}

	if (old_index > new_index) {
		if (!exynos4x12_pms_change(old_index, new_index)) {
			/* 1. Change the system clock divider values */
			set_clkdiv(new_index);
			/* 2. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS4_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos4x12_apll_pms_table[new_index] & 0x7);
			__raw_writel(tmp, EXYNOS4_APLL_CON0);

		} else {
			/* Clock Configuration Procedure */
			/* 1. Change the system clock divider values */
			set_clkdiv(new_index);
			/* 2. Change the apll m,p,s value */
			set_apll(new_index, old_index);
		}
	} else if (old_index < new_index) {
		if (!exynos4x12_pms_change(old_index, new_index)) {
			/* 1. Change just s value in apll m,p,s value */
			tmp = __raw_readl(EXYNOS4_APLL_CON0);
			tmp &= ~(0x7 << 0);
			tmp |= (exynos4x12_apll_pms_table[new_index] & 0x7);
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

	/* ABB value is changed in below case */
	if (soc_is_exynos4412() && (exynos_result_of_asv > 3)) {
		if (exynos4x12_freq_table[new_index].frequency <= 200000)
			exynos4x12_set_abb_member(ABB_ARM, ABB_MODE_100V);
		else
			exynos4x12_set_abb_member(ABB_ARM, abb_arm);
	}
}

static int __init exynos4x12_set_param(struct exynos_dvfs_info *info)
{
	unsigned int i;

	if (soc_is_exynos4212()) {
		switch (samsung_rev()) {
		case EXYNOS4212_REV_1_0:
			exynos4x12_freq_table[L0].frequency = CPUFREQ_ENTRY_INVALID;
			break;
		default:
			exynos4x12_freq_table[L0].frequency = CPUFREQ_ENTRY_INVALID;
			exynos4x12_freq_table[L1].frequency = CPUFREQ_ENTRY_INVALID;
			break;
		}
		
	} else if (soc_is_exynos4412()) {
		switch (samsung_rev()) {
		case EXYNOS4412_REV_1_1:
			exynos4x12_freq_table[L0].frequency = CPUFREQ_ENTRY_INVALID;
			exynos4x12_freq_table[L1].frequency = CPUFREQ_ENTRY_INVALID;
			break;
		case EXYNOS4412_REV_2_0:
			pr_info("1600MHZ support\n");
			break;
		default:
			exynos4x12_freq_table[L0].frequency = CPUFREQ_ENTRY_INVALID;
			exynos4x12_freq_table[L1].frequency = CPUFREQ_ENTRY_INVALID;
			exynos4x12_freq_table[L2].frequency = CPUFREQ_ENTRY_INVALID;
			exynos4x12_freq_table[L3].frequency = CPUFREQ_ENTRY_INVALID;
			break;
		}
	}

	pr_debug("DVFS : VDD_ARM Voltage table set with %d Group\n",
				exynos_result_of_asv);

	if (exynos_result_of_asv == 0xff) {
		for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++)
			info->full_volt_table[i] = asv_voltage_s[i];
	} else {
		if (soc_is_exynos4212()) {
			for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++)
				info->full_volt_table[i] =
					asv_voltage_4212[i][exynos_result_of_asv];
		} else if (soc_is_exynos4412()) {
			if (samsung_rev() >= EXYNOS4412_REV_2_0) {
				for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++)
					info->full_volt_table[i] =
						asv_voltage_4412_rev2[i][exynos_result_of_asv] + arm_bias;
			} else {
				for (i = 0 ; i < CPUFREQ_LEVEL_END ; i++)
					info->full_volt_table[i] =
						asv_voltage_4412[i][exynos_result_of_asv] + arm_bias;
			}
		} else {
			pr_err("%s: Can't find SoC type \n", __func__);
		}
	}
	
	return 0;
}

static void __init exynos4x12_cpufreq_div_init(void)
{
	unsigned int tmp, i;

	for (i = L0; i <  CPUFREQ_LEVEL_END; i++) {

		exynos4x12_clkdiv_table[i].index = i;

		tmp = __raw_readl(EXYNOS4_CLKDIV_CPU);

		tmp &= ~(EXYNOS4_CLKDIV_CPU0_CORE_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM0_MASK |
			EXYNOS4_CLKDIV_CPU0_COREM1_MASK |
			EXYNOS4_CLKDIV_CPU0_PERIPH_MASK |
			EXYNOS4_CLKDIV_CPU0_ATB_MASK |
			EXYNOS4_CLKDIV_CPU0_PCLKDBG_MASK |
			EXYNOS4_CLKDIV_CPU0_APLL_MASK |
			EXYNOS4_CLKDIV_CPU0_CORE2_MASK);

		if (soc_is_exynos4212()) {
			tmp |= ((clkdiv_cpu0_4212[i][0] << EXYNOS4_CLKDIV_CPU0_CORE_SHIFT) |
				(clkdiv_cpu0_4212[i][1] << EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT) |
				(clkdiv_cpu0_4212[i][2] << EXYNOS4_CLKDIV_CPU0_COREM1_SHIFT) |
				(clkdiv_cpu0_4212[i][3] << EXYNOS4_CLKDIV_CPU0_PERIPH_SHIFT) |
				(clkdiv_cpu0_4212[i][4] << EXYNOS4_CLKDIV_CPU0_ATB_SHIFT) |
				(clkdiv_cpu0_4212[i][5] << EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT) |
				(clkdiv_cpu0_4212[i][6] << EXYNOS4_CLKDIV_CPU0_APLL_SHIFT) |
				(clkdiv_cpu0_4212[i][7] << EXYNOS4_CLKDIV_CPU0_CORE2_SHIFT));
		} else {
			tmp |= ((clkdiv_cpu0_4412[i][0] << EXYNOS4_CLKDIV_CPU0_CORE_SHIFT) |
				(clkdiv_cpu0_4412[i][1] << EXYNOS4_CLKDIV_CPU0_COREM0_SHIFT) |
				(clkdiv_cpu0_4412[i][2] << EXYNOS4_CLKDIV_CPU0_COREM1_SHIFT) |
				(clkdiv_cpu0_4412[i][3] << EXYNOS4_CLKDIV_CPU0_PERIPH_SHIFT) |
				(clkdiv_cpu0_4412[i][4] << EXYNOS4_CLKDIV_CPU0_ATB_SHIFT) |
				(clkdiv_cpu0_4412[i][5] << EXYNOS4_CLKDIV_CPU0_PCLKDBG_SHIFT) |
				(clkdiv_cpu0_4412[i][6] << EXYNOS4_CLKDIV_CPU0_APLL_SHIFT) |
				(clkdiv_cpu0_4412[i][7] << EXYNOS4_CLKDIV_CPU0_CORE2_SHIFT));
		}

		exynos4x12_clkdiv_table[i].clkdiv = tmp;

		tmp = __raw_readl(EXYNOS4_CLKDIV_CPU1);

		if (soc_is_exynos4212()) {
			tmp &= ~(EXYNOS4_CLKDIV_CPU1_COPY_MASK |
				EXYNOS4_CLKDIV_CPU1_HPM_MASK);
			tmp |= ((clkdiv_cpu1_4212[i][0] << EXYNOS4_CLKDIV_CPU1_COPY_SHIFT) |
				(clkdiv_cpu1_4212[i][1] << EXYNOS4_CLKDIV_CPU1_HPM_SHIFT));
		} else {
			tmp &= ~(EXYNOS4_CLKDIV_CPU1_COPY_MASK |
				EXYNOS4_CLKDIV_CPU1_HPM_MASK |
				EXYNOS4_CLKDIV_CPU1_CORES_MASK);
			tmp |= ((clkdiv_cpu1_4412[i][0] << EXYNOS4_CLKDIV_CPU1_COPY_SHIFT) |
				(clkdiv_cpu1_4412[i][1] << EXYNOS4_CLKDIV_CPU1_HPM_SHIFT) |
				(clkdiv_cpu1_4412[i][2] << EXYNOS4_CLKDIV_CPU1_CORES_SHIFT));
		}
		exynos4x12_clkdiv_table[i].clkdiv1 = tmp;
	}
}

int __init exynos4x12_cpufreq_init(struct exynos_dvfs_info *info)
{
	struct clk *cpu_clk = NULL;
	struct clk *mpll_clk = NULL;
	unsigned long rate;
	int ret;
	int i;

	info->full_volt_table = kzalloc(sizeof(*info->full_volt_table) * CPUFREQ_LEVEL_END, 
								GFP_KERNEL);
	if (IS_ERR_OR_NULL(info->full_volt_table)) {
		pr_err("failed to kzalloc full_volt_table\n");
		return PTR_ERR(info->full_volt_table);
	}

	info->volt_table_num = CPUFREQ_LEVEL_END;
#ifdef CONFIG_REGULATOR_MAX77686
	if (info->gpio_dvfs) {	
		info->update_dvs_voltage = max77686_update_dvs_voltage;
		/*max77686 support 8 level voltage setting only*/
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

	info->used_freq_table = kzalloc(sizeof(*info->used_freq_table) * \
					ARRAY_SIZE(exynos4x12_freq_table), 
					GFP_KERNEL);
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
	info->pm_lock_freq = 800*1000;
	for (i=0; i<ARRAY_SIZE(exynos4x12_freq_table); i++) {
		/*800MHZ*/
		if (exynos4x12_freq_table[i].frequency == 800*1000) {
			info->pll_safe_idx = exynos4x12_freq_table[i].index;
			break;
		}
	}

	info->full_freq_table = exynos4x12_freq_table;
	info->full_freq_num = ARRAY_SIZE(exynos4x12_freq_table);
	
	if (soc_is_exynos4212())
		cpufreq_profile_index[0] = exynos4212_profile_high;
	else if (soc_is_exynos4412() && samsung_rev() >= EXYNOS4412_REV_2_0)
		cpufreq_profile_index[0] = exynos4412_v2_profile_high;

	info->freq_profile_index = cpufreq_profile_index;
	info->pfm_id = INIT_CPU_PFM;
	info->set_freq = exynos4x12_set_frequency;
	info->need_apll_change = exynos4x12_pms_change;

	ret = exynos4x12_set_param(info);
	if (ret < 0) {
		pr_err("exynos4x12_set_param error!\n");
		goto err_clk1;
	}

	exynos4x12_cpufreq_div_init();

	return 0;

err_clk1:
	clk_put(cpu_clk);
err_clk0:
	kfree(info->used_freq_table);
err_free1:	
	kfree(info->used_volt_table);
err_free0:
	kfree(info->full_volt_table);
	return ret;
}
