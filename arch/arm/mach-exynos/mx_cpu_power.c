/* arch/arm/mach-exynos/mx_cpu_power.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
#include <linux/platform_device.h>
#include <linux/power/cpupower.h>

/* default table with one default cpu_power value */
static unsigned int table_default_power[1] = {
	1024
};

static struct cputopo_power default_cpu_power = {
	.max  = 1,
	.step = 1,
	.table = table_default_power,
};

/* This table sets the cpu_power scale of a cpu according to 2 inputs which are
 * the frequency and the sched_mc mode. The content of this table could be SoC
 * specific so we should add a method to overwrite this default table.
 * TODO: Study how to use DT for setting this table
 */
static unsigned int exynos4412_power[] = {
	/* Power save mode CA9 MP */ 
	8192, /*  100 MHZ */
	8192, /*  200 MHz */
	8192, /*  300 MHz */
	8192, /*  400 MHz */
	8192, /*  500 MHz */
	1024, /*  600 MHz */
	1024, /*  700 MHz */
	1024, /*  800 MHz */
	1024, /*  900 MHz */
	1024, /* 1000 MHz */
	1024, /* 1100 MHz */   
	1024, /* 1200 MHz */
	1024, /* 1300 MHz */
	1024, /* 1400 MHz */
	1024, /* 1500 MHz */
	1024, /* 1600 MHz */
	1024, /* 1700 MHz */
	1024, /* 1800 MHz */
};

static struct cputopo_power exynos4412_cpu_power = {
	.max  = ARRAY_SIZE(exynos4412_power),
	.step = 100000,
	.table = exynos4412_power,
};

static struct cputopo_power *table_config[2] = {
	&default_cpu_power,
	&exynos4412_cpu_power,
};

static struct platform_device mx_device_cpupower = {
	.name = "cpupower",
	.id = -1,
	.dev.platform_data = table_config,
};

static int  __init mx_init_cpupower(void)
{
	return platform_device_register(&mx_device_cpupower);
}

arch_initcall(mx_init_cpupower);

MODULE_DESCRIPTION("mx cpu power driver helper");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");