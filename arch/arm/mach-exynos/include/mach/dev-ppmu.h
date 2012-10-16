/*
 * linux/arch/arm/mach-exynos/include/mach/dev-ppmu.h
 * 
 *   Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *   Author:
 *
 * Samsung EXYNOS4 PPMU (Performance Profiling Managed Unit)
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
 
#ifndef _ARM_MACH_EXYNOS_PPMU_H_
#define _ARM_MACH_EXYNOS_PPMU_H_

#define PPMU_DEVNAME_BASE "exynos4-ppmu"

#include <linux/device.h>

#define PPMU_PLATDEV(ipname) exynos_device_ppmu_##ipname

#ifdef CONFIG_EXYNOS_DEV_PD
#define ASSIGN_PPMU_POWERDOMAIN(ipname, powerdomain) \
		PPMU_PLATDEV(ipname).dev.parent = powerdomain
#else
#define ASSIGN_SYSTEM_POWERDOMAIN(ipname, powerdomain) do { } while (0)
#endif

extern struct platform_device PPMU_PLATDEV(3d);
extern struct platform_device PPMU_PLATDEV(acp);
extern struct platform_device PPMU_PLATDEV(camif);
extern struct platform_device PPMU_PLATDEV(cpu);
extern struct platform_device PPMU_PLATDEV(dmc_l);
extern struct platform_device PPMU_PLATDEV(dmc_r);
extern struct platform_device PPMU_PLATDEV(fsys);
extern struct platform_device PPMU_PLATDEV(image);
extern struct platform_device PPMU_PLATDEV(lcd0);
extern struct platform_device PPMU_PLATDEV(lcd1);
extern struct platform_device PPMU_PLATDEV(mfc_l);
extern struct platform_device PPMU_PLATDEV(mfc_r);
extern struct platform_device PPMU_PLATDEV(tv);
extern struct platform_device PPMU_PLATDEV(bus_left);
extern struct platform_device PPMU_PLATDEV(bus_right);

#define PPMU_CLOCK_NAME(ipname, id) PPMU_DEVNAME_BASE "." #id

#endif /* _ARM_MACH_EXYNOS_PPMU_H_ */
