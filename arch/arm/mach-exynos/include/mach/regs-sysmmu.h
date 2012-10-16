/* linux/arch/arm/mach-exynos/include/mach/regs-sysmmu.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 - System MMU register
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_REGS_SYSMMU_H
#define __ASM_ARCH_REGS_SYSMMU_H __FILE__

#define EXYNOS_MMU_CTRL			0x000
#define EXYNOS_MMU_CFG			0x004
#define EXYNOS_MMU_STATUS		0x008
#define EXYNOS_MMU_FLUSH		0x00C
#define EXYNOS_PT_BASE_ADDR		0x014
#define EXYNOS_INT_STATUS		0x018
#define EXYNOS_INT_CLEAR			0x01C
#define EXYNOS_PAGE_FAULT_ADDR	0x024
#define EXYNOS_AW_FAULT_ADDR	0x028
#define EXYNOS_AR_FAULT_ADDR	0x02C
#define EXYNOS_DEFAULT_SLAVE_ADDR	0x030

#endif /* __ASM_ARCH_REGS_SYSMMU_H */
