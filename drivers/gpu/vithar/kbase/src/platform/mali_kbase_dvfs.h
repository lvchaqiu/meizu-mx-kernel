/* drivers/gpu/vithar/kbase/src/platform/mali_kbase_dvfs.h
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_dvfs.h
 * DVFS
 */

#ifndef _KBASE_DVFS_H_
#define _KBASE_DVFS_H_


struct regulator *kbase_platform_get_regulator(void);
int kbase_platform_regulator_init(struct device *dev);
int kbase_platform_regulator_disable(struct device *dev);
int kbase_platform_regulator_enable(struct device *dev);
int kbase_platform_get_voltage(struct device *dev, int *vol);
int kbase_platform_set_voltage(struct device *dev, int vol);

#endif /* _KBASE_DVFS_H_ */
