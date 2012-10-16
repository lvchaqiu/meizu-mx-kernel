/* arch/arm/mach-exynos/include/mach/mx_debug.h
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
 
typedef enum {
	MX_USB_WAKE		= 1<<0,
	MX_LOWBAT_WAKE 	= 1<<1,
	MX_KEY_POWER_WAKE	= 1<<2,
	MX_KEY_HOME_WAKE 	= 1<<3,
	MX_MODEM_WAKE 		= 1<<4,
	MX_BLUETOOTH_WAKE	= 1<<5,
	MX_MODEM_RST_WAKE	= 1<<6,
	MX_CHARG_WAKE		= 1<<7,
	MX_ALARM_WAKE		= 1<<8,
	MX_TICK_WAKE		= 1<<9,
	MX_I2S_WAKE			= 1<<10,
	MX_SYSTIMER_WAKE	= 1<<11,
	MX_WIFI_WAKE		= 1<<12,
	MX_IR_WAKE			= 1<<13,
	MX_USB_HOST_WAKE	= 1<<14,	
	MX_MINUS_KEY_WAKE	= 1<<15,	
	MX_PLUS_KEY_WAKE	= 1<<16,
	MX_JACK_WAKE = 1<<17,
	MX_UNKNOW_WAKE	= 0,
} wake_type_t;

typedef enum {
	EINT_GROUP0,
	EINT_GROUP1,
	EINT_GROUP2,
	EINT_GROUP3,
	OTHER_INT,
} mx_int_type;

extern unsigned long mx_get_wakeup_type(void);
