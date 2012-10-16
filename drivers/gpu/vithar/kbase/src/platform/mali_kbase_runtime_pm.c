/* drivers/gpu/vithar/kbase/src/platform/mali_kbase_runtime_pm.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 runtime pm driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_runtime_pm.c
 * Runtime PM
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
#include <linux/timer.h>
#include <kbase/src/platform/mali_kbase_platform.h>
#include <linux/pm_runtime.h>

#if 0
/* 100 ticks means 100ms */
#define RUNTIME_PM_RUNTIME_DELAY_TIME 100 

struct timer_list runtime_pm_timer;

static void kbase_device_runtime_callback(unsigned long data)
{
    printk("kbase_device_runtime_callback, data=%ld\n", data);
}

void kbase_device_runtime_init_timer(void)
{
    init_timer(&runtime_pm_timer);

    runtime_pm_timer.expires = jiffies + RUNTIME_PM_RUNTIME_DELAY_TIME;
    runtime_pm_timer.data = 0;
    runtime_pm_timer.function = kbase_device_runtime_callback;

    add_timer(&runtime_pm_timer);
}

void kbase_device_runtime_restart_timer(void)
{
    mod_timer(&runtime_pm_timer, jiffies + RUNTIME_PM_RUNTIME_DELAY_TIME);
}

void kbase_device_runtime_stop_timer(void)
{
    if(del_timer_sync(&runtime_pm_timer) == 0)
    {
	printk("runtime_pm_timer is already inactive\n");
    }
}
#endif

/** Suspend callback from the OS.
 *
 * This is called by Linux runtime PM when the device should suspend.
 *
 * @param dev	The device to suspend
 *
 * @return A standard Linux error code
 */
int kbase_device_runtime_suspend(struct device *dev)
{
    if(kbase_platform_clock_off(dev))
	panic("failed to turn off sclk_g3d\n");
    if(kbase_platform_power_off(dev))
	panic("failed to turn off g3d power\n");

    return 0;
}

/** Resume callback from the OS.
 *
 * This is called by Linux runtime PM when the device should resume from suspension.
 *
 * @param dev	The device to resume
 *
 * @return A standard Linux error code
 */
int kbase_device_runtime_resume(struct device *dev)
{
    if(kbase_platform_clock_on(dev))
	panic("failed to turn on sclk_g3d\n");
    if(kbase_platform_power_on(dev))
	panic("failed to turn on g3d power\n");

    return 0;
}

/** Enable runtiem pm
 *
 * @param dev	The device to enable rpm
 *
 * @return A standard Linux error code
 */
void kbase_device_runtime_enable(struct device *dev)
{
    pm_runtime_enable(dev);
}

/** Disable runtime pm
 *
 * @param dev	The device to enable rpm
 *
 * @return A standard Linux error code
 */
void kbase_device_runtime_disable(struct device *dev)
{
    pm_runtime_disable(dev);
}

/** Initialize runtiem pm fields in given device 
 *
 * @param dev	The device to initialize
 *
 * @return A standard Linux error code
 */
void kbase_device_runtime_init(struct device *dev)
{
    pm_runtime_enable(dev);
}

