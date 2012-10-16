/* drivers/gpu/vithar/kbase/src/platform/mali_kbase_dvfs.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_dvfs.c
 * DVFS
 */

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_uku.h>
#include <kbase/src/common/mali_kbase_mem.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/linux/mali_kbase_mem_linux.h>
#include <uk/mali_ukk.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <mach/map.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <mach/regs-clock.h>
#include <asm/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator=NULL;
int mali_gpu_vol = 1100000; /* 1.1V  */
#endif

int kbase_platform_regulator_init(struct device *dev)
{
#ifdef CONFIG_REGULATOR
    g3d_regulator = regulator_get(NULL, "vdd_g3d");
    if(IS_ERR(g3d_regulator))
    {
        printk("[kbase_platform_regulator_init] failed to get vithar regulator\n");
	return -1;
    }

    if(regulator_enable(g3d_regulator) != 0)
    {
        printk("[kbase_platform_regulator_init] failed to enable vithar regulator\n");
	return -1;
    }

    if(regulator_set_voltage(g3d_regulator, mali_gpu_vol, mali_gpu_vol) != 0)
    {
        printk("[kbase_platform_regulator_init] failed to set vithar operating voltage [%d]\n", mali_gpu_vol);
	return -1;
    }
#endif
    return 0;
}

int kbase_platform_regulator_disable(struct device *dev)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_regulator_disable] g3d_regulator is not initialized\n");
	return -1;
    }

    if(regulator_disable(g3d_regulator) != 0)
    {
        printk("[kbase_platform_regulator_disable] failed to disable g3d regulator\n");
	return -1;
    }
#endif
    return 0;
}

int kbase_platform_regulator_enable(struct device *dev)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_regulator_enable] g3d_regulator is not initialized\n");
	return -1;
    }

    if(regulator_enable(g3d_regulator) != 0)
    {
        printk("[kbase_platform_regulator_enable] failed to enable g3d regulator\n");
	return -1;
    }
#endif
    return 0;
}

int kbase_platform_get_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_get_voltage] g3d_regulator is not initialized\n");
	return -1;
    }

    *vol = regulator_get_voltage(g3d_regulator);
#else
    *vol = 0;
#endif
    return 0;
}

int kbase_platform_set_voltage(struct device *dev, int vol)
{
#ifdef CONFIG_REGULATOR
    if(!g3d_regulator)
    {
        printk("[kbase_platform_set_voltage] g3d_regulator is not initialized\n");
	return -1;
    }

    if(regulator_set_voltage(g3d_regulator, vol, vol) != 0)
    {
        printk("[kbase_platform_set_voltage] failed to set voltage\n");
	return -1;
    }
#endif
    return 0;
}

