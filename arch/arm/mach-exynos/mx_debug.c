/* arch/arm/mach-exynos/mx_debug.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author:  Lvcha qiu <lvcha@meizu.com>
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
 
#include <linux/err.h>
#include <linux/init.h>
#include <linux/syscore_ops.h>
#include <linux/device.h>
#include <linux/io.h>

#include <mach/regs-pmu.h>
#include <mach/regs-gpio.h>
#include <asm/mach-types.h>
#include <mach/mx_debug.h>

#include "mx_debug_show_irq.c"

static int mx_debug_suspend(void)
{
	mx_set_wakeup_type(0);
	return 0;
}

static void mx_debug_resume(void)
{
	mx_record_pending();
	mx_show_wakeup_name();
}

static struct syscore_ops mx_debug_syscore_ops = {
	.suspend	= mx_debug_suspend,
	.resume	= mx_debug_resume,
};

static __init int mx_debug_init(void)
{
	register_syscore_ops(&mx_debug_syscore_ops);
	return 0;
}

arch_initcall(mx_debug_init);

MODULE_DESCRIPTION("mx debuger driver");
MODULE_AUTHOR("Lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
