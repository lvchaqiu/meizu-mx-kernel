/* arch/arm/mach-exynos/mx_jack.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Based on mach-exynos/p4note-jack.c
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

#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mx_jack.h>
#include <linux/delay.h>

#include <asm/mach-types.h>

#include <mach/gpio.h>

extern int mx_set_mic_bias(bool bOnOff);

static int mx_set_jack_micbias(bool on)
{
	return mx_set_mic_bias(on);
}

static struct mx_jack_zone mx_jack_zones[] = {
	{
		/* adc == 0, unstable zone, default to 3pole if it stays
		 * in this range for 300ms (15ms delays, 20 samples)
		 */
		.adc_high = 0,
		.delay_ms = 15,
		.check_count = 20,
		.jack_type = MX_HEADSET_3POLE,
	},
	{
		/* 0 < adc <= 1200, unstable zone, default to 3pole if it stays
		 * in this range for 300ms (15ms delays, 20 samples)
		 */
		.adc_high = 1200,
		.delay_ms = 10,
		.check_count = 80,
		.jack_type = MX_HEADSET_3POLE,
	},
	{
		/* 1200 < adc <= 2600, unstable zone, default to 4pole if it
		 * stays in this range for 800ms (10ms delays, 80 samples)
		 */
		.adc_high = 2600,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = MX_HEADSET_4POLE,
	},
	{
		/* 2600 < adc <= 4000, 3 pole zone, default to 4pole if it
		 * stays in this range for 100ms (10ms delays, 10 samples)
		 */
		.adc_high = 4000,
		.delay_ms = 10,
		.check_count = 5,
		.jack_type = MX_HEADSET_4POLE,
	},
	{
		/* adc > 3800, unstable zone, default to 3pole if it stays
		 * in this range for two seconds (10ms delays, 200 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 200,
		.jack_type = MX_HEADSET_3POLE,
	},
};

/* To support 3-buttons earjack */
static struct mx_jack_buttons_zone mx_jack_buttons_zones[] = {
	{
		/* 0 <= adc <=10, stable zone */
		.code = KEY_FORWARDMAIL,
		.adc_low = 0,
		.adc_high = 10,
	},
	{
		/* 300 <= adc <= 350, stable zone */
		.code = KEY_BACK,
		.adc_low = 300,
		.adc_high = 350,
	},
	{
		/* 800 <= adc <= 850, stable zone */
		.code = KEY_FORWARD,
		.adc_low = 800,
		.adc_high = 850,
	},
};

static struct mx_jack_platform_data mx_jack_data = {
	.set_micbias_state = mx_set_jack_micbias,
	.zones = mx_jack_zones,
	.num_zones = ARRAY_SIZE(mx_jack_zones),
	.buttons_zones = mx_jack_buttons_zones,
	.num_buttons_zones = ARRAY_SIZE(mx_jack_buttons_zones),
	.det_gpio = EXYNOS4_GPX1(7),
	.adc_channel = 3,
};

static struct platform_device mx_device_jack = {
	.name = "mx_jack",
	.id = -1,
	.dev.platform_data = &mx_jack_data,
};

static int  __init mx_jack_init(void)
{
	if (machine_is_m030()) {
		mx_jack_data.det_gpio = EXYNOS4_GPX2(1);
		mx_jack_data.adc_channel = 1;
	}

	return platform_device_register(&mx_device_jack);
}
arch_initcall(mx_jack_init);

MODULE_DESCRIPTION("mx jack driver helper");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
