/*
 * mx_idle_leds.c - Moniter cpu running state for mx by leds
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  lvcha qiu   <lvcha@meizu.com>
 *
 * This program is not provided / owned by Maxim Integrated Products.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/leds.h>

DEFINE_LED_TRIGGER(idle_led_trigger[CONFIG_NR_CPUS]);

static int mx_leds_idle_notifier(struct notifier_block *nb, unsigned long val,
                                void *data)
{
	int cpuid = smp_processor_id();

	switch (val) {
	case IDLE_START:
		led_trigger_event(idle_led_trigger[cpuid], LED_OFF);
		break;
	case IDLE_END:
		led_trigger_event(idle_led_trigger[cpuid], LED_FULL);
		break;
	}

	return 0;
}

static struct notifier_block mx_leds_idle_nb = {
	.notifier_call = mx_leds_idle_notifier,
};

static int __init mx_cpuidle_leds(void)
{
	static char trig_name[CONFIG_NR_CPUS][16];
	int i;

	for (i = 0; i<num_possible_cpus(); i++) {
		snprintf(trig_name[i], sizeof(trig_name[i]), "cpu%d-idle", i);
		led_trigger_register_simple(trig_name[i], &idle_led_trigger[i]);
	}

	idle_notifier_register(&mx_leds_idle_nb);

	return 0;
}

device_initcall(mx_cpuidle_leds);

MODULE_DESCRIPTION("Mx cpu idle moniter");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
