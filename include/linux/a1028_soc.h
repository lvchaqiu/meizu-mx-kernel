/*
 * include/linux/a1028_soc.h
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author:  Lvcha qiu <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

  #ifndef A1028_SOC_H
 #define A1028_SOC_H
 
#define A1028_MSG_BOOT		0x0001
#define A1028_BOOT_ACK		0x101

#define A1028_NORMAL_ACK	0x80

#define A1028_MSG_SYNC		0x80000000
#define A1028_MSG_SLEEP		0x80100001
#define A1028_MSG_RESET		0x80020000

#define A1028_VER0			0x80200000
#define A1028_VER1			0x80210000


#define A1028_CMD_FIFO_DEPTH	64
#define RETRY_CNT		10
#define POLLING_RETRY_CNT	3

struct a1028_platform_data {
	unsigned int gpio_wakeup;
	unsigned int gpio_reset;
	unsigned int gpio_clk;
};

enum a1028_mode {
	A1028_SUSPEND,
	A1028_INCALL_RECEIVER,
	A1028_INCALL_HEADSET,
	A1028_INCALL_SPEAKER,
	A1028_INCALL_BT,
	A1028_BYPASS,
	A1028_LASTMODE,
	A1028_INVALID,
} ;

enum a1028_status {
	A1028_NORMAL,
	A1028_SLEEP,
	A1028_RST,
} ;

struct a1028_soc {
	struct device *dev;
	struct i2c_client *client;
	struct mutex a1028_mutex;
	int gpio_wake;
	int gpio_reset;
	enum a1028_mode  mode;
	enum a1028_status  status;
	int nr_bt;

	struct regulator *crystal_regulator;
};
//extern int a1028_soc_setmode(struct a1028_soc *, int);

#endif	/* !A1028_SOC_H */
