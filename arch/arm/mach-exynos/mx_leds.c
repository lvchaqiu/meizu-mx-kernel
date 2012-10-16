/* arch/arm/mach-exynos/mx_leds.c
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
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/err.h>

#include <mach/map.h>
#include <mach/gpio-m032.h>

static struct gpio_led __initdata m032_gpio_leds[] = {
	[0] = {
		.name = "gpio-led0",
		.default_trigger = "mmc0",
		.gpio = LED_ID1,
		.active_low = true,
		.retain_state_suspended = false,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	[1] = {
		.name = "gpio-led1",
		.default_trigger = "cpu3-idle",
		.gpio = LED_ID2,
		.active_low = true,
		.retain_state_suspended = true,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	[2] = {
		.name = "gpio-led2",
		.default_trigger = "cpu0-idle",
		.gpio = LED_ID3,
		.active_low = true,
		.retain_state_suspended = false,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	[3] = {
		.name = "gpio-led3",
		.default_trigger = "cpu1-idle",
		.gpio = LED_ID4,
		.active_low = true,
		.retain_state_suspended = false,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	[4] = {
		.name = "gpio-led4",
		.default_trigger = "cpu2-idle",
		.gpio = LED_ID5,
		.active_low = true,
		.retain_state_suspended = false,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	[5] = {
		.name = "gpio-led5",
		.default_trigger = "didle",
		.gpio = LED_ID6,
		.active_low = true,
		.retain_state_suspended = false,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
};

static struct gpio_led_platform_data __initdata mx_gpio_led = {
	.num_leds = ARRAY_SIZE(m032_gpio_leds),
	.leds = m032_gpio_leds,
};

static int  __init mx_init_led(void)
{
	struct platform_device *ret = NULL;

	ret = gpio_led_register_device(-1, &mx_gpio_led);

	return PTR_ERR(ret);
}

arch_initcall(mx_init_led);

MODULE_DESCRIPTION("mx led driver helper");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
