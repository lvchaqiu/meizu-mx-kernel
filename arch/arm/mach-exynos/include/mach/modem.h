/* linux/arch/arm/mach-s5pv310/include/mach/modem.h
 *
 * Copyright (c) 2011 Meizu Technology Co., Ltd.
 *		http://www.meizu.com/
 *
 *
 * Based on arch/arm/mach-s5p6442/include/mach/io.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MODEM_H
#define __MODEM_H

#define HOST_WUP_LEVEL 0

struct modem_ops {
	void (*modem_on)(void);
	void (*modem_off)(void);
	void (*modem_reset)(void);
	void (*modem_cfg)(void);
};

struct modem_platform_data {
	const char *name;
	unsigned gpio_phone_on;
	unsigned gpio_phone_active;
	unsigned gpio_pda_active;
	unsigned gpio_pmu_reset;
	unsigned gpio_cp_reset;
	unsigned gpio_usim_boot;
	unsigned gpio_flm_sel;
	unsigned gpio_cp_req_reset;	/*HSIC*/
	unsigned gpio_ipc_slave_wakeup;
	unsigned gpio_ipc_host_wakeup;
	unsigned gpio_suspend_request;
	unsigned gpio_active_state;
	unsigned gpio_cp_dump_int;
	unsigned gpio_cp_reset_int;
	unsigned gpio_sim_detect_int;
	int wakeup;
	struct modem_ops ops;
};

extern int modem_is_on(void);
extern int modem_is_host_wakeup(void);
extern int modem_set_active_state(int val);
extern int modem_set_slave_wakeup(int val);

extern int modem_debug;

#define modem_dbg( format, arg...)		\
		do{\
			if(modem_debug)\
				printk(KERN_INFO"%s"format, "Modem: ", ## arg);\
		 }while (0)

#define modem_err( format, arg...)		\
		printk(KERN_ERR"%s"format , "Modem Error: ",## arg)

#endif//__MODEM_H

