/* linux/arch/arm/mach-exynos/mx_reboot.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: lvcha qiu <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Revision History
 *
 * Inital code : Apr 16 , 2012 : lvcha@meizu.com
 *
*/

#include <linux/delay.h>
#include <linux/pm.h>

#include <asm/io.h>
#include <asm/cacheflush.h>
#include <asm/mach-types.h>

#include <mach/system.h>
#include <mach/regs-pmu.h>
#include <mach/gpio.h>

#define REBOOT_MODE_CHARGE		0x0
#define REBOOT_MODE_WIPE		0x1
#define REBOOT_MODE_UPGRADE		0x2

static void mx_reboot_internal(const char *cmd)
{
	local_irq_disable();

	if(cmd) {
		if (!strcmp(cmd, "charge"))
			__raw_writel(REBOOT_MODE_CHARGE, S5P_INFORM4);
		else if (!strcmp(cmd, "wipe"))
			__raw_writel(REBOOT_MODE_WIPE, S5P_INFORM4);
		else if (!strcmp(cmd, "upgrade"))
			__raw_writel(REBOOT_MODE_UPGRADE, S5P_INFORM4);
	}

	flush_cache_all();
	outer_flush_all();
	arch_reset(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1) arch_reset(0, 0);

}

static void mx_disable_inand(void)
{
	__raw_writel(0x100, S5P_VA_GPIO2 + 0x40);
	__raw_writel(0, S5P_VA_GPIO2 + 0x44);
	__raw_writel(0, S5P_VA_GPIO2 + 0x48);
	__raw_writel(0, S5P_VA_GPIO2 + 0x60);
	__raw_writel(0, S5P_VA_GPIO2 + 0x64);
	__raw_writel(0, S5P_VA_GPIO2 + 0x68);
}

static void mx_power_off(void)
{
	int regs;
	int gpio;

	if (machine_is_m030())
		gpio = EXYNOS4_GPX2(5);
	else
		gpio = EXYNOS4_GPX0(3);

	mx_disable_inand();
	if (gpio_get_value(gpio)) { /* 1. Check reboot charging */
		mx_reboot_internal("charge");
	} else {	/* 2. Power off */
		regs = __raw_readl(S5P_PS_HOLD_CONTROL);
		/* dead loop to avoid sometimes auto restart*/
		while(1) {
			pr_emerg("%s: waiting for reboot\n", __func__);
			__raw_writel(regs & 0xFFFFFEFF, S5P_PS_HOLD_CONTROL);
		}
	}
}

static void mx_reboot(char str, const char *cmd)
{
	WARN(1, "%s (%c, %s)\n", __func__, str, cmd ? cmd : "(null)");
	mx_disable_inand();
	mx_reboot_internal(cmd);
}

static int __init mx_reboot_init(void)
{
	/* to support system shut down */
	pm_power_off = mx_power_off;
	arm_pm_restart = mx_reboot;

	return 0;
}

subsys_initcall(mx_reboot_init);
