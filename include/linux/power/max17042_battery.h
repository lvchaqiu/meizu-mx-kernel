/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
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

#ifndef __MAX17042_BATTERY_H_
#define __MAX17042_BATTERY_H_


#define MAX17042_REG_STATUS		0x00
#define MAX17042_REG_VALRT_TH		0x01
#define MAX17042_REG_TALRT_TH		0x02
#define MAX17042_REG_SALRT_TH		0x03
#define MAX17042_REG_SOC_REP		0x06
#define MAX17042_REG_VCELL			0x09
#define MAX17042_REG_TEMPERATURE	0x08
#define MAX17042_REG_CURRENT		0x0A
#define MAX17042_REG_FULL_CAP		0x10
#define MAX17042_REG_TTE		0x11
#define MAX17042_REG_AVGVCELL		0x19
#define MAX17042_REG_CONFIG		0x1D
#define MAX17042_REG_VERSION		0x21
#define MAX17042_REG_LEARNCFG		0x28
#define MAX17042_REG_FILTERCFG	0x29
#define MAX17042_REG_MISCCFG		0x2B
#define MAX17042_REG_CGAIN		0x2E
#define MAX17042_REG_RCOMP		0x38
#define MAX17042_REG_VFOCV		0xFB
#define MAX17042_REG_SOC_VF		0xFF

#define MAX17042_LONG_DELAY		2000
#define MAX17042_SHORT_DELAY		200

#define MAX17042_BATTERY_FULL		95


struct max17042_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*low_batt_cb)(int critical);

	struct	max17042_reg_data *init;
	int init_count;
	struct	max17042_reg_data *alert_init;
	int alert_init_count;
	int alert_gpio;
	unsigned int alert_irq;

	bool enable_current_sense;
	bool enable_gauging_temperature;
};

struct max17042_reg_data {
	u8 reg_addr;
	u8 reg_data1;
	u8 reg_data2;
};

#endif /* __MAX17042_BATTERY_H_ */
