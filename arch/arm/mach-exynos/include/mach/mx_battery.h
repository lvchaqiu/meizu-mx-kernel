/*
 * Copyright (C) 2010 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MACH_MX_BATTERY_H
#define __MACH_MX_BATTERY_H __FILE__

/*soc level for 3.6V */
#define MX_BATTERY_SOC_3_6	7

/**
 * struct sec_bat_plaform_data - init data for sec batter driver
 * @fuel_gauge_name: power supply name of fuel gauge
 * @charger_name: power supply name of charger
 */
struct mx_bat_platform_data {
	char *fuel_gauge_name;
	char *charger_name;
};

#endif /* __MACH_MX_BATTERY_H */
