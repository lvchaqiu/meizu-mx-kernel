/* linux/arch/arm/mach-exynos4/dev-tmu.c
  *
  * Copyright 2011 by SAMSUNG
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/platform_data/exynos4_tmu.h>
#include <asm/irq.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <plat/devs.h>
#include <plat/cpu.h>

static struct resource exynos4_tmu_resource[] = {
	[0] = {
		.start	= EXYNOS4_PA_TMU,
		.end	= EXYNOS4_PA_TMU + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_TMU,
		.end	= IRQ_TMU,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct exynos4_tmu_platform_data exynos4210_tmu_data = {
	.threshold = 75,
	.trigger_levels[0] = 5,
	.trigger_levels[1] = 20,
	.trigger_levels[2] = 30,
	.trigger_levels[3] = 40,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 1,
	.gain = 15,
	.reference_voltage = 7,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.freq_tab[0] = {
		.freq_clip_pctg = 10,
	},
	.freq_tab[1] = {
		.freq_clip_pctg = 30,
	},
	.freq_tab_count = 2,
};

static struct exynos4_tmu_platform_data exynos4x12_tmu_data = {
	.threshold = 75,
	.trigger_levels[0] = 5,
	.trigger_levels[1] = 20,
	.trigger_levels[2] = 30,
	.trigger_levels[3] = 40,
	.trigger_level0_en = 1,
	.trigger_level1_en = 1,
	.trigger_level2_en = 1,
	.trigger_level3_en = 1,
	.gain = 0x8,
	.reference_voltage = 0x10,
	.cal_type = TYPE_ONE_POINT_TRIMMING,
	.freq_tab[0] = {
		.freq_clip_pctg = 10,
	},
	.freq_tab[1] = {
		.freq_clip_pctg = 30,
	},
	.freq_tab_count = 2,
};

struct platform_device exynos4_device_tmu = {
	.name		= "exynos4-tmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos4_tmu_resource),
	.resource	= exynos4_tmu_resource,
};

void __init exynos4_tmu_set_platdata(void)
{
	if (soc_is_exynos4210()) {
		exynos4_device_tmu.name = "exynos4210-tmu";
		s3c_set_platdata(&exynos4210_tmu_data, 
			sizeof(struct exynos4_tmu_platform_data), &exynos4_device_tmu);
	} else if ( soc_is_exynos4212() || soc_is_exynos4412() ){
		exynos4_device_tmu.name = "exynos4x12-tmu";
		s3c_set_platdata(&exynos4x12_tmu_data, 
			sizeof(struct exynos4_tmu_platform_data), &exynos4_device_tmu);
	}
}
