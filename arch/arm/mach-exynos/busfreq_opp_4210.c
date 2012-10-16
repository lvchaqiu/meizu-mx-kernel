/* linux/arch/arm/mach-exynos/busfreq_opp_4210.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4210 - BUS clock frequency scaling support with opp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/opp.h>
#include <linux/platform_data/exynos4_ppmu.h>

#include <asm/mach-types.h>

#include <mach/ppmu.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/regs-mem.h>
#include <mach/dev.h>
#include <mach/asv.h>
#include <mach/busfreq_exynos4.h>

#include <plat/map-s5p.h>
#include <plat/gpio-cfg.h>
#include <plat/cpu.h>

#define EXYNOS4210_DMC_MAX_THRESHOLD	42
#define PPMU_THRESHOLD			5
#define IDLE_THRESHOLD			4
#define UP_CPU_THRESHOLD		11
#define MAX_CPU_THRESHOLD		20
#define CPU_SLOPE_SIZE			7

static unsigned int asv_group_index;

enum busfreq_level_idx {
	LV_0,
	LV_1,
	LV_2,
	LV_3,
	LV_END
};

static struct busfreq_table exynos4_busfreq_table[] = {
	{LV_0, 400000, 1125000, 0, 0, 0},
	{LV_1, 267000, 1025000, 0, 0, 0},
	{LV_2, 200000, 1025000, 0, 0, 0},
	{LV_3, 133000, 1025000, 0, 0, 0},
	{0, 0, 0, 0, 0, 0},
};

#define ASV_GROUP	5
static unsigned int exynos4210_int_volt[ASV_GROUP][LV_END] = {
	{1150000, 1075000, 1050000, 1050000},
	{1125000, 1050000, 1025000, 1025000},
	{1100000, 1025000, 1000000, 1000000},
	{1100000, 1025000, 1000000, 1000000},
	{1100000, 1025000, 1000000, 1000000},
};

static unsigned int clkdiv_dmc0[LV_END][8] = {
	/*
	 * Clock divider value for following
	 * { DIVACP, DIVACP_PCLK, DIVDPHY, DIVDMC, DIVDMCD
	 *              DIVDMCP, DIVCOPY2, DIVCORE_TIMERS }
	 */

	/* DMC L0: 400MHz */
	{ 3, 2, 1, 1, 1, 1, 3, 1 },

	/* DMC L1: 266.7MHz */
	{ 4, 1, 1, 2, 1, 1, 3, 1 },
	
	/* DMC L2: 200MHz */
	{ 4, 1, 1, 3, 1, 1, 3, 1 },

	/* DMC L3: 133MHz */
	{ 5, 1, 1, 5, 1, 1, 3, 1 },
};

static unsigned int clkdiv_top[LV_END][5] = {
	/*
	 * Clock divider value for following
	 * { DIVACLK200, DIVACLK100, DIVACLK160, DIVACLK133, DIVONENAND }
	 */

	/* ACLK200 L0: 200MHz */
	{ 3, 7, 4, 5, 1 },

	/* ACLK200 L1: 200MHz */
        { 3, 7, 4, 5, 1 },

	/* ACLK200 L2: 160MHz */
	{ 4, 7, 5, 6, 1 },

	/* ACLK200 L3: 133MHz */
	{ 5, 7, 5, 7, 1 },
};

static unsigned int clkdiv_lr_bus[LV_END][2] = {
	/*
	 * Clock divider value for following
	 * { DIVGDL/R, DIVGPL/R }
	 */

	/* ACLK_GDL/R L0: 200MHz */
	{ 3, 1 },
	
	/* ACLK_GDL/R L1: 200MHz */
        { 3, 1 },

	/* ACLK_GDL/R L2: 160MHz */
	{ 4, 1 },

	/* ACLK_GDL/R L3: 133MHz */
	{ 5, 1 },
};

/*
  * QOSAC_GDL
  * [0] Imgae Block: "1" Full Resource, "0" Tidemark
  * [1] TV Block: "1" Full Resource, "0" Tidemark
  * [2] G3D Block: "1" Full Resource, "0" Tidemark
  * [3] MFC_L Block: "1" Full Resource, "0" Tidemark
  *
  * QOSSAC_GDR
  * [0] CAM Block: "1" Full Resource, "0" Tidemark
  * [1] LCD0 Block: "1" Full Resource, "0" Tidemark
  * [2] LCD1 Block: "1" Full Resource, "0" Tidemark
  * [3] FSYS Block: "1" Full Resource, "0" Tidemark
  * [4] MFC_R Block: "1" Full Resource, "0" Tidemark
  * [5] MAUDIO Block: "1" Full Resource, "0" Tidemark
  */
static unsigned int exynos4_qos_value[LV_END][4] = {
	{0x00, 0x00, 0x00, 0x00},
	{0x04, 0x02, 0x04, 0x02},
	{0x04, 0x02, 0x04, 0x02},
	{0x02, 0x02, 0x02, 0x02},
};
static void exynos4210_target(int index)
{
	unsigned int tmp;

	/* Change Divider - DMC0 */
	tmp = exynos4_busfreq_table[index].clk_dmc0div;

	__raw_writel(tmp, EXYNOS4_CLKDIV_DMC0);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STAT_DMC0);
	} while (tmp & 0x11111111);

	/* Change Divider - TOP */
	tmp = exynos4_busfreq_table[index].clk_topdiv;

	__raw_writel(tmp, EXYNOS4_CLKDIV_TOP);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STAT_TOP);
	} while (tmp & 0x11111);

	/* Change Divider - LEFTBUS */
	tmp = __raw_readl(EXYNOS4_CLKDIV_LEFTBUS);

	tmp &= ~(EXYNOS4_CLKDIV_BUS_GDLR_MASK | EXYNOS4_CLKDIV_BUS_GPLR_MASK);

	tmp |= ((clkdiv_lr_bus[index][0] << EXYNOS4_CLKDIV_BUS_GDLR_SHIFT) |
		(clkdiv_lr_bus[index][1] << EXYNOS4_CLKDIV_BUS_GPLR_SHIFT));

	__raw_writel(tmp, EXYNOS4_CLKDIV_LEFTBUS);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STAT_LEFTBUS);
	} while (tmp & 0x11);

	/* Change Divider - RIGHTBUS */
	tmp = __raw_readl(EXYNOS4_CLKDIV_RIGHTBUS);

	tmp &= ~(EXYNOS4_CLKDIV_BUS_GDLR_MASK | EXYNOS4_CLKDIV_BUS_GPLR_MASK);

	tmp |= ((clkdiv_lr_bus[index][0] << EXYNOS4_CLKDIV_BUS_GDLR_SHIFT) |
		(clkdiv_lr_bus[index][1] << EXYNOS4_CLKDIV_BUS_GPLR_SHIFT));

	__raw_writel(tmp, EXYNOS4_CLKDIV_RIGHTBUS);

	do {
		tmp = __raw_readl(EXYNOS4_CLKDIV_STAT_RIGHTBUS);
	} while (tmp & 0x11);

}
static void exynos4210_set_qos(unsigned int index)
{
	__raw_writel(exynos4_qos_value[index][0], S5P_VA_GDL + 0x400);
	__raw_writel(exynos4_qos_value[index][1], S5P_VA_GDL + 0x404);
	__raw_writel(exynos4_qos_value[index][2], S5P_VA_GDR + 0x400);
	__raw_writel(exynos4_qos_value[index][3], S5P_VA_GDR + 0x404);
}
static struct opp *exynos4210_monitor(struct busfreq_data *data)
{
	unsigned int i;
	struct opp *opp = data->curr_opp;
	unsigned int cpu_load_average = 0;
	unsigned int dmc0_load_average = 0;
	unsigned int dmc1_load_average = 0;
	unsigned int dmc_load_average;
	unsigned long cpufreq = 0;
	unsigned long lockfreq, lockupfreq;
	unsigned long dmcfreq;
	unsigned long newfreq;
	unsigned long currfreq = opp_get_freq(data->curr_opp) / 1000;
	unsigned long maxfreq = opp_get_freq(data->max_opp) / 1000;
	unsigned long cpu_load;
	unsigned long dmc0_load;
	unsigned long dmc1_load;
	unsigned long dmc_load;
	int cpu_load_slope;

	struct exynos4_ppmu_data ppmu_data;
	ppmu_data = exynos4_ppmu_update(PPMU_CPU);
	cpu_load = ppmu_data.load;
	ppmu_data = exynos4_ppmu_update(PPMU_DMC0);
	dmc0_load = div64_u64(ppmu_data.load * currfreq, maxfreq);
	ppmu_data = exynos4_ppmu_update(PPMU_DMC1);
	dmc1_load = div64_u64(ppmu_data.load * currfreq, maxfreq);

	cpu_load_slope = cpu_load -
		data->load_history[PPMU_CPU][data->index ? data->index - 1 : LOAD_HISTORY_SIZE - 1];

	data->load_history[PPMU_CPU][data->index] = cpu_load;
	data->load_history[PPMU_DMC0][data->index] = dmc0_load;
	data->load_history[PPMU_DMC1][data->index++] = dmc1_load;

	if (data->index >= LOAD_HISTORY_SIZE)
		data->index = 0;

	for (i = 0; i < LOAD_HISTORY_SIZE; i++) {
		cpu_load_average += data->load_history[PPMU_CPU][i];
		dmc0_load_average += data->load_history[PPMU_DMC0][i];
		dmc1_load_average += data->load_history[PPMU_DMC1][i];
	}

	/* Calculate average Load */
	cpu_load_average /= LOAD_HISTORY_SIZE;
	dmc0_load_average /= LOAD_HISTORY_SIZE;
	dmc1_load_average /= LOAD_HISTORY_SIZE;

	if (dmc0_load >= dmc1_load) {
		dmc_load = dmc0_load;
		dmc_load_average = dmc0_load_average;
	} else {
		dmc_load = dmc1_load;
		dmc_load_average = dmc1_load_average;
	}

	if (cpu_load >= UP_CPU_THRESHOLD) {
		cpufreq = opp_get_freq(data->max_opp);
		if (cpu_load < MAX_CPU_THRESHOLD) {
			opp = data->curr_opp;
			if (cpu_load_slope > CPU_SLOPE_SIZE) {
				cpufreq--;
				opp = opp_find_freq_floor(data->dev, &cpufreq);
			}
			cpufreq = opp_get_freq(opp);
		}
	}

	if (dmc_load >= EXYNOS4210_DMC_MAX_THRESHOLD) {
		dmcfreq = opp_get_freq(data->max_opp);
	} else if (dmc_load < IDLE_THRESHOLD) {
		if (dmc_load_average < IDLE_THRESHOLD)
			opp = step_down(data, 1);
		else
			opp = data->curr_opp;
		dmcfreq = opp_get_freq(opp);
	} else {
		if (dmc_load < dmc_load_average) {
			dmc_load = dmc_load_average;
			if (dmc_load >= EXYNOS4210_DMC_MAX_THRESHOLD)
				dmc_load = EXYNOS4210_DMC_MAX_THRESHOLD;
		}
		dmcfreq = div64_u64(maxfreq * dmc_load * 1000, EXYNOS4210_DMC_MAX_THRESHOLD);
	}

	lockfreq = dev_max_freq(data->dev);
	lockupfreq = dev_uplimit_freq(data->dev);
	newfreq = max3(lockfreq, dmcfreq, cpufreq);
	if( lockupfreq && newfreq > lockupfreq )
		newfreq = lockupfreq;
	opp = opp_find_freq_ceil(data->dev, &newfreq);
	
	pr_debug("curfreq %ld, newfreq %ld, dmc0_load %ld, dmc1_load %ld, cpu_load %ld\n",
		currfreq, newfreq, dmc0_load, dmc1_load, cpu_load);
	if (IS_ERR(opp)) {
		opp = data->max_opp;
	}
	return opp;
}

#define ARM_INT_CORRECTION 267000
#define CPU_FREQ_THRESHOLD  800000
static int exynos4210_busfreq_cpufreq_transition(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct cpufreq_freqs *freqs = (struct cpufreq_freqs *)data;
	struct busfreq_data *bus_data = container_of(nb, struct busfreq_data,
			exynos_cpufreq_notifier);

	switch (event) {
	case CPUFREQ_PRECHANGE:
		if (freqs->new >= CPU_FREQ_THRESHOLD && 
			freqs->old < CPU_FREQ_THRESHOLD)
			dev_lock(bus_data->dev, bus_data->dev, ARM_INT_CORRECTION);
		break;
	case CPUFREQ_POSTCHANGE:
		if (freqs->old >= CPU_FREQ_THRESHOLD && 
			freqs->new < CPU_FREQ_THRESHOLD)
			dev_unlock(bus_data->dev, bus_data->dev);
		break;
	}

	return NOTIFY_DONE;
}

static void exynos4210_set_bus_volt(void)
{
	unsigned int asv_group;
	unsigned int i;

	asv_group = exynos_result_of_asv & 0xF;
	asv_group_index = asv_group;

	pr_info("EXYNOS4210 ASV: VDD_INT Voltage table set with %d Group\n", asv_group);

	for (i = 0 ; i < LV_END ; i++) {

		switch (asv_group) {
		case 0:
			exynos4_busfreq_table[i].volt =
				exynos4210_int_volt[0][i];
			break;
		case 1:
		case 2:
			exynos4_busfreq_table[i].volt =
				exynos4210_int_volt[1][i];
			break;
		case 3:
		case 4:
			exynos4_busfreq_table[i].volt =
				exynos4210_int_volt[2][i];
			break;
		case 5:
		case 6:
			exynos4_busfreq_table[i].volt =
				exynos4210_int_volt[3][i];
			break;
		case 7:
			exynos4_busfreq_table[i].volt =
				exynos4210_int_volt[4][i];
			break;
		}
	}

	return;
}

static unsigned int exynos4210_get_int_volt(unsigned long index)
{
	return exynos4210_int_volt[asv_group_index][index];
}

static unsigned int exynos4210_get_table_index(struct opp *opp)
{
	unsigned int index;

	for (index = LV_0; index < LV_END; index++)
		if (opp_get_freq(opp) == exynos4_busfreq_table[index].mem_clk)
			break;
	return index;
}

int exynos4210_init(struct device *dev, struct busfreq_data *data)
{
	unsigned int i;
	unsigned int tmp;
	unsigned long maxfreq = ULONG_MAX;
	unsigned long minfreq = 0;
	unsigned long freq;
	struct clk *sclk_dmc;
	int ret;

	tmp = __raw_readl(EXYNOS4_CLKDIV_DMC0);

	for (i = 0; i <  LV_END; i++) {
		tmp &= ~(EXYNOS4_CLKDIV_DMC0_ACP_MASK |
			EXYNOS4_CLKDIV_DMC0_ACPPCLK_MASK |
			EXYNOS4_CLKDIV_DMC0_DPHY_MASK |
			EXYNOS4_CLKDIV_DMC0_DMC_MASK |
			EXYNOS4_CLKDIV_DMC0_DMCD_MASK |
			EXYNOS4_CLKDIV_DMC0_DMCP_MASK |
			EXYNOS4_CLKDIV_DMC0_COPY2_MASK |
			EXYNOS4_CLKDIV_DMC0_CORETI_MASK);

		tmp |= ((clkdiv_dmc0[i][0] << EXYNOS4_CLKDIV_DMC0_ACP_SHIFT) |
			(clkdiv_dmc0[i][1] << EXYNOS4_CLKDIV_DMC0_ACPPCLK_SHIFT) |
			(clkdiv_dmc0[i][2] << EXYNOS4_CLKDIV_DMC0_DPHY_SHIFT) |
			(clkdiv_dmc0[i][3] << EXYNOS4_CLKDIV_DMC0_DMC_SHIFT) |
			(clkdiv_dmc0[i][4] << EXYNOS4_CLKDIV_DMC0_DMCD_SHIFT) |
			(clkdiv_dmc0[i][5] << EXYNOS4_CLKDIV_DMC0_DMCP_SHIFT) |
			(clkdiv_dmc0[i][6] << EXYNOS4_CLKDIV_DMC0_COPY2_SHIFT) |
			(clkdiv_dmc0[i][7] << EXYNOS4_CLKDIV_DMC0_CORETI_SHIFT));

		exynos4_busfreq_table[i].clk_dmc0div = tmp;
	}

	tmp = __raw_readl(EXYNOS4_CLKDIV_TOP);

	for (i = 0; i <  LV_END; i++) {
		tmp &= ~(EXYNOS4_CLKDIV_TOP_ACLK200_MASK |
			EXYNOS4_CLKDIV_TOP_ACLK100_MASK |
			EXYNOS4_CLKDIV_TOP_ACLK160_MASK |
			EXYNOS4_CLKDIV_TOP_ACLK133_MASK |
			EXYNOS4_CLKDIV_TOP_ONENAND_MASK);

		tmp |= ((clkdiv_top[i][0] << EXYNOS4_CLKDIV_TOP_ACLK200_SHIFT) |
			(clkdiv_top[i][1] << EXYNOS4_CLKDIV_TOP_ACLK100_SHIFT) |
			(clkdiv_top[i][2] << EXYNOS4_CLKDIV_TOP_ACLK160_SHIFT) |
			(clkdiv_top[i][3] << EXYNOS4_CLKDIV_TOP_ACLK133_SHIFT) |
			(clkdiv_top[i][4] << EXYNOS4_CLKDIV_TOP_ONENAND_SHIFT));

		exynos4_busfreq_table[i].clk_topdiv = tmp;
	}

	exynos4210_set_bus_volt();

	for (i = 0; i < LV_END; i++) {
		ret = opp_add(dev, exynos4_busfreq_table[i].mem_clk,
				exynos4_busfreq_table[i].volt);
		if (ret) {
			dev_err(dev, "Fail to add opp entries.\n");
			return ret;
		}
	}
	data->table = exynos4_busfreq_table;
	data->table_size = LV_END;

	/* Find max frequency */
	data->max_opp = opp_find_freq_floor(dev, &maxfreq);
	data->min_opp = opp_find_freq_ceil(dev, &minfreq);

	sclk_dmc = clk_get(NULL, "sclk_dmc");

	if (IS_ERR(sclk_dmc)) {
		pr_err("Failed to get sclk_dmc.!\n");
		data->curr_opp = data->max_opp;
	} else {
		freq = clk_get_rate(sclk_dmc) / 1000;
		clk_put(sclk_dmc);
		data->curr_opp = opp_find_freq_ceil(dev, &freq);
		if(IS_ERR(data->curr_opp))
			data->curr_opp = data->max_opp;
	}

	data->vdd_int = regulator_get(NULL, "vdd_int");
	if (IS_ERR(data->vdd_int)) {
		pr_err("failed to get resource %s\n", "vdd_int");
		return -ENODEV;
	}

	data->exynos_cpufreq_notifier.notifier_call =
				exynos4210_busfreq_cpufreq_transition;

	if (cpufreq_register_notifier(&data->exynos_cpufreq_notifier,
				CPUFREQ_TRANSITION_NOTIFIER)) {
		pr_err("Failed to setup cpufreq notifier\n");
		goto err_cpufreq;
	}
	
	/*init functions*/	
	data->target = exynos4210_target;
	data->get_int_volt = exynos4210_get_int_volt;
	data->get_table_index = exynos4210_get_table_index;
	data->monitor = exynos4210_monitor;
	data->set_qos = exynos4210_set_qos;
	//data->busfreq_prepare = exynos4x12_prepare;
	//data->busfreq_post = exynos4x12_post;
	//data->busfreq_suspend = exynos4x12_suspend;
	//data->busfreq_resume = exynos4x12_resume;

	return 0;

err_cpufreq:
	regulator_put(data->vdd_int);

	return -ENODEV;
}
