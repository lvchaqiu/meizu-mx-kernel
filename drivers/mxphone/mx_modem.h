/*
 * Modem control driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 *		http://www.meizu.com/
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/wakelock.h>

#ifndef __MODEM_CONTROL_H__
#define __MODEM_CONTROL_H__

#define  MC_SUCCESS       0
#define  MC_HOST_HIGH     1
#define  MC_HOST_TIMEOUT  2
#define  MC_HOST_HALT     3

#define  MODEM_POWER_MAIN_CMD    _IOWR('M', 0, int)
#define  MODEM_POWER_FLASH_CMD   _IOWR('M', 1, int)
#define  MODEM_POWER_OFF_CMD     _IOWR('M', 2, int)
#define  MODEM_POWER_ON_CMD      _IOWR('M', 3, int)
#define  MODEM_POWER_RESET_CMD   _IOWR('M', 4, int)
#define  MODEM_POWER_REENUM_CMD  _IOWR('M', 5, int)

#define  MODEM_CONNECT_FLAG     0x0001
#define  MODEM_RESET_FLAG       0x0002
#define  MODEM_CRASH_FLAG       0x0004
#define  MODEM_DUMP_FLAG        0x0008
#define  MODEM_DISCONNECT_FLAG  0x0010
#define  MODEM_SIM_DETECT_FLAG  0x0020
#define  MODEM_INIT_ON_FLAG     0x0040
#define  MODEM_EVENT_MASK       0x007E

struct modemctl {
	int irq[4];
	struct modem_platform_data *pdata;
	struct modem_ops *ops;
	struct regulator *vcc;

	struct device *dev;
	const struct attribute_group *group;

	struct delayed_work work;
	struct work_struct resume_work;
	struct work_struct cpreset_work;
	struct delayed_work sim_work;
	
	int wakeup_flag; /*flag for CP boot GPIO sync flag*/
	wait_queue_head_t		read_wq;
	wait_queue_head_t		conn_wq;
	
	int cp_flag;
	int boot_done;
	int enum_done;
	struct completion *l2_done;
#ifdef CONFIG_HAS_WAKELOCK	
	struct wake_lock modem_lock;
#endif
	int debug_cnt;
};

extern struct platform_device modemctl;

enum HOST_WAKEUP_STATE{
	HOST_WAKEUP_LOW = 1,
	HOST_WAKEUP_WAIT_RESET,
};

enum MODEM_EVENT_TYPE{
	MODEM_EVENT_POWEROFF,
	MODEM_EVENT_RESET,
	MODEM_EVENT_CRASH,
	MODEM_EVENT_DUMP,
	MODEM_EVENT_CONN,
	MODEM_EVENT_DISCONN,
	MODEM_EVENT_SIM,
};

void modem_crash_event(int type);
extern int modem_prepare_resume(int);
#endif /* __MODEM_CONTROL_H__ */
