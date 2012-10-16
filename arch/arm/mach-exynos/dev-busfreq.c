/*
 * linux/arch/arm/mach-exynos/dev-busfreq.c
 *
 * Copyright (C) 2012 ZhuHai MEIZU Technology Co., Ltd.
 *	http://www.meizu.com
 *
 * Platform device for Samsung's  EXYNOS4 BUS devfreq
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/devfreq-exynos4.h>

#include <mach/map.h>

#include <plat/devs.h>
#include <plat/cpu.h>


static struct exynos4_busfreq_platdata exynos4210_devfreq_bus_pdata = {
	.threshold = {
		.upthreshold = 80,
		.downdifferential = 10,
	},
	.polling_ms = 10,
};

static struct exynos4_busfreq_platdata exynos4x12_devfreq_bus_pdata = {
	.threshold = {
		.upthreshold = 80,
		.downdifferential = 10,
	},
	.polling_ms = 10,
};

struct platform_device exynos_device_busfreq = {
	.name		= "exynos4-busfreq",
	.id		= -1,
};

void __init exynos_busfreq_set_platdata(void)
{
	if (soc_is_exynos4210()) {
		exynos_device_busfreq.name = "exynos4210-busfreq";
		s3c_set_platdata(&exynos4210_devfreq_bus_pdata, 
			sizeof(struct exynos4_busfreq_platdata), &exynos_device_busfreq);
	} else if ( soc_is_exynos4212() || soc_is_exynos4412() ){
		exynos_device_busfreq.name = "exynos4x12-busfreq";
		s3c_set_platdata(&exynos4x12_devfreq_bus_pdata, 
			sizeof(struct exynos4_busfreq_platdata), &exynos_device_busfreq);
	} else if ( soc_is_exynos5250() ) {
		exynos_device_busfreq.name = "exynos5250-busfreq";
	}
}
