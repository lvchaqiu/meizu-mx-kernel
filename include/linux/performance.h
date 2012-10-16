/* include/linux/performance.h
 *
 * Copyright (C) 2012 ZhuHai MEIZU Technology Co., Ltd.
 *	  http://www.meizu.com
 *
 * Meizu System performance adjust interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __H_SYS_PERFORMANCE__
#define __H_SYS_PERFORMANCE__

#include <linux/sysdev.h>

/*cpu performance level*/
enum cpu_performance{
	CPU_PFM_HIGH,
	CPU_PFM_MED,
	CPU_PFM_LOW,
	CPU_PFM_NUMBER,
};

struct pfm_info {
#ifdef CONFIG_BUSFREQ_OPP
	struct device *bus_dev;
	struct device dev;
#endif
	struct sysdev_class pfm_class;
#define MAX_PFM_ID 2
	int pfm_id;
};

#define INIT_CPU_PFM CPU_PFM_MED

extern int unregister_pfm_notifier(struct notifier_block *);
extern int register_pfm_notifier(struct notifier_block *);

#endif
