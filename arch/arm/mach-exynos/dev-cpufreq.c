/*
 * linux/arch/arm/mach-exynos/dev-cpufreq.c
 *
 * Copyright (C) 2012 ZhuHai MEIZU Technology Co., Ltd.
 *	http://www.meizu.com
 *
 * Platform device for Samsung's  EXYNOS4 CPU Frequency
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/exynos-cpufreq.h>

#include <mach/map.h>

#include <plat/devs.h>
#include <plat/cpu.h>

struct platform_device exynos_device_cpufreq = {
	.name		= "exynos-cpufreq",
	.id		= -1,
};

void __init exynos_cpufreq_set_platdata(struct exynos_cpufreq_platdata *pd)
{
	struct exynos_cpufreq_platdata *npd;
	
	npd = s3c_set_platdata(pd, sizeof(struct exynos_cpufreq_platdata),
			 &exynos_device_cpufreq);

	if (IS_ERR_OR_NULL(npd))
		return;

	if (soc_is_exynos4210())
		exynos_device_cpufreq.name = "exynos4210-cpufreq";
	else if (soc_is_exynos4212())
		exynos_device_cpufreq.name = "exynos4212-cpufreq";
	else if (soc_is_exynos4412())
		exynos_device_cpufreq.name = "exynos4412-cpufreq";
}
