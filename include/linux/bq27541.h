/*
 * bq27541.h - Driver for the TI BQ27541
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: Chwei   <Chwei@meizu.com>
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
 *
 * Datasheets:
 * http://www.ti.com/product/bq27541-v200
 *
 */

#ifndef __LINUX_MFD_BQ27541_H
#define __LINUX_MFD_BQ27541_H
struct bq27541_platform_data {
	int wakeup;
	char *name;	
	int low_bat_gpio;
};

extern suspend_state_t get_suspend_state(void);
#endif	/* __LINUX_MFD_BQ27541_H */
