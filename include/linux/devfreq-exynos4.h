/* include/linux/devfreq-exynos4.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * EXYNOS4 - Memory/Bus clock frequency scaling support in DEVFREQ framework
 *	This version supports EXYNOS4210 only. This changes bus frequencies
 *	and vddint voltages. Exynos4412/4212 should be able to be supported
 *	with minor modifications.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __EXYNOS4_BUS_H
#define __EXYNOS4_BUS_H __FILE__

#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
#include <linux/devfreq.h>
struct exynos4_busfreq_platdata {
	struct devfreq_simple_ondemand_data threshold;
	unsigned int polling_ms; /* 0 to use default(50) */
};
#endif

extern void __init exynos_busfreq_set_platdata(void);

#if defined(CONFIG_MX_DVS_GPIO)
#include <linux/regulator/consumer.h>
extern int max8997_update_dvs_voltage(struct regulator_dev *rdev,
				const int * volt_table,const unsigned int);
extern int max77686_update_dvs_voltage(struct regulator_dev *rdev, 
				const int *volt_table, const unsigned int size);
#endif

#endif /* __EXYNOS4_BUS_H */
