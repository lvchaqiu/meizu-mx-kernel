/*  arch/arm/mach-exynos/include/mach/gpio-mx.h
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/gpio.h>
#include <asm/mach-types.h>
#include <mach/gpio-common.h>
#include <plat/gpio-cfg.h>

#define GPIO_SETPIN_LOW 	0
#define GPIO_SETPIN_HI   	1
#define GPIO_SETPIN_NONE	2

#define GPIO_PULL_NONE		S3C_GPIO_PULL_NONE
#define GPIO_PULL_DOWN	S3C_GPIO_PULL_DOWN
#define GPIO_PULL_UP		S3C_GPIO_PULL_UP

#define GPIO_DRVSTR_LV1 	S5P_GPIO_DRVSTR_LV1
#define GPIO_DRVSTR_LV2	S5P_GPIO_DRVSTR_LV2
#define GPIO_DRVSTR_LV3	S5P_GPIO_DRVSTR_LV3
#define GPIO_DRVSTR_LV4 	S5P_GPIO_DRVSTR_LV4

#define GPIO_SLP_OUT0       ((__force s3c_gpio_pull_t)0x00)
#define GPIO_SLP_OUT1       ((__force s3c_gpio_pull_t)0x01)
#define GPIO_SLP_INPUT      ((__force s3c_gpio_pull_t)0x02)
#define GPIO_SLP_PREV       ((__force s3c_gpio_pull_t)0x03)

struct gpio_info_table{
	unsigned int pin;
	unsigned int type;
	unsigned int data;
	unsigned int pull;
	unsigned int drv;
};

struct gpio_slp_info{
	unsigned char *name;
	unsigned int pin;
	unsigned int type;
	unsigned int pull;
};

extern int s3c_gpio_slp_cfgpin(unsigned int, unsigned int);
extern int s3c_gpio_slp_setpull_updown(unsigned int, unsigned int);

static __init int 
mx_config_gpio_table(const struct gpio_info_table *table, int size)
{
	struct gpio_info_table gpio;
	unsigned int i;

	for (i = 0; i < size; i++) {
		gpio = table[i];

		/* Off part */
		if(gpio.pin <= EXYNOS4_GPIO_END) {
			if(gpio.type == S3C_GPIO_RESERVED) {
				s3c_gpio_setpull(gpio.pin, gpio.pull);
			} else if (gpio.type == S3C_GPIO_INPUT) {
				s3c_gpio_setpull(gpio.pin, gpio.pull);
				s3c_gpio_cfgpin(gpio.pin, gpio.type);
			} else if (gpio.type == S3C_GPIO_OUTPUT) {
				s3c_gpio_setpull(gpio.pin, gpio.pull);
				gpio_set_value(gpio.pin, !!gpio.data);
				s3c_gpio_cfgpin(gpio.pin, S3C_GPIO_OUTPUT);
				s5p_gpio_set_drvstr(gpio.pin,  gpio.drv);
			}
		}
	}
	return 0;
}

static inline void mx_set_sleep_pin(unsigned int pin, 
		s5p_gpio_pd_cfg_t conpdn, s5p_gpio_pd_pull_t pudpdn)
{
	s3c_gpio_slp_cfgpin(pin, conpdn);
	s3c_gpio_slp_setpull_updown(pin, pudpdn);
}