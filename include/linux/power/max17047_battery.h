/*
 *  max17047_battery.h
 *
 *  based on max17047_fuelgauge.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17047_BATTERY_H_
#define __MAX17047_BATTERY_H_

#define RECAL_SOC_FOR_MAXIM

#define MAX17047_REG_STATUS		0x00
#define MAX17047_REG_VALRT_TH		0x01
#define MAX17047_REG_TALRT_TH		0x02
#define MAX17047_REG_SALRT_TH		0x03
#define MAX17047_REG_TEMPERATURE	0x08
#define MAX17047_REG_VCELL		0x09
#define MAX17047_REG_AVGVCELL		0x19
#define MAX17047_REG_CONFIG		0x1D
#define MAX17047_REG_VERSION		0x21
#define MAX17047_REG_LEARNCFG		0x28
#define MAX17047_REG_FILTERCFG		0x29
#define MAX17047_REG_MISCCFG		0x2B
#define MAX17047_REG_CGAIN		0x2E
#define MAX17047_REG_RCOMP		0x38
#define MAX17047_REG_VFOCV		0xFB
#define MAX17047_REG_SOC_VF		0xFF

#define MAX17047_LONG_DELAY		2000
#define MAX17047_SHORT_DELAY		0

#define MAX17047_BATTERY_FULL		95

#define MAX17047_NEW_RCOMP		0x0065

struct max17047_reg_data {
	u8 reg_addr;
	u8 reg_data1;
	u8 reg_data2;
};

struct max17047_platform_data {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*low_batt_cb)(void);

	struct	max17047_reg_data *init;
	int init_size;
	struct	max17047_reg_data *alert_init;
	int alert_init_size;
	int alert_gpio;
	unsigned int alert_irq;
	bool enable_gauging_temperature;
#ifdef RECAL_SOC_FOR_MAXIM
	/*check need for re-calculation of soc */
	bool (*need_soc_recal)(void);
#endif
};

#endif
