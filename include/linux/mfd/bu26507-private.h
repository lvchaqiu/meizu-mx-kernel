/*
 * bu26507-private.h - Voltage regulator driver for the bu26507 led
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
 */

#ifndef __LINUX_MFD_BU26507_PRIV_H
#define __LINUX_MFD_BU26507_PRIV_H

#include <linux/i2c.h>

/* Slave addr = 0x74: device registers */
enum bu26507_led_reg {
	REG_SOFTWARE_RESET = 0x00,
	REG_OSC_CONTROL = 0x01,
	REG_LED_ENABLE = 0x11,
	REG_LED_PWM = 0x20,
	REG_SYNC_CONTROL = 0x21,
	REG_SETTING = 0x2d,
	REG_SCROLL_SETTING = 0x2f,
	REG_MATRIX_CONTROL = 0x30,
	REG_REGISTER_MAP = 0x7f,
};

enum bu26507_reg_map {
	RMCG_CONTROL = 0x00,
	RMCG_AB,
};

/* pattern register data */
enum bu26507_reg_data {
	REG_LED0 = 0x07,   /*DA11, D808*/
	REG_LED1 = 0x01,   /*DA00, D802*/
	REG_LED2 = 0x0d,   /*DA22, D804*/
	REG_LED3 = 0x06,   /*DA10, D803*/
	REG_LED4 = 0x0c,   /*DA21, D809*/
	REG_LED5 = 0x13,   /*DA33, D805*/
	REG_LED6 = 0x12,   /*DA32, D810*/
	REG_LED7 = 0x1e,   /*DA54, D807*/
	REG_LED8 = 0x18,   /*DA43, D811*/
	REG_LED9 = 0x19,   /*DA44, D806*/
	REG_LED10 = 0x0B,   /*DA20, D812*/
	REG_LED11 = 0x10,   /*DA30, D813*/
	REG_LED12 = 0x15,   /*DA40, D814*/
};

enum bu26507_type {
	bu26507_LED,
	bu26507_PWM,
	bu26507_SLOPE,
};

/*REG_SETTING setting values*/
enum bu26507_setting_type {
	SCROLL_EN = 1 << 0,
	SLOPE_EN = 1 << 1,
	PWN_EN = 1 << 2,
	SLOPE_QUARTER = 0 << 3,   	/*1/4 slope*/
	SLOPE_NONE = 1 << 3,	/*none slope*/
	SLOPE_HALF = 2 << 3,	/*1/2 slope*/
};

/*led matrix control, start or stop*/
enum bu26507_matrix_control {
	MATRIX_LED_STOP = 0x00,
	MATRIX_LED_START,
};

/*slope cycle time values, 0~3 second*/
enum bu26507_slope_cycle_time {
	SLOPE_CYCLE_0 = 0 << 6,
	SLOPE_CYCLE_1 = 1 << 6,
	SLOPE_CYCLE_2 = 2 << 6,
	SLOPE_CYCLE_3 = 3 << 6,
};

struct bu26507_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex iolock;
	int reset_pin;
	int sync_pin;
	int (*i2c_read)(struct i2c_client *, u8, u8 *);
	int (*i2c_write)(struct i2c_client *, u8, u8);
};

#endif /*  __LINUX_MFD_BU26507_PRIV_H */
