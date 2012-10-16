/*
 * exynos4_ppmu.h - Samsung EXYNOS4 PPMU (Performance Profiling Managed Unit)
 *
 *   Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *   Author:
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

#ifndef _LINUX_EXYNOS4_PPMU_H
#define _LINUX_EXYNOS4_PPMU_H

#define CCNT_OFFSET 31
#define COUNT3_OFFSET 3
#define COUNT2_OFFSET 2
#define COUNT1_OFFSET 1
#define COUNT0_OFFSET 0

typedef enum {
	PPMU_3D,
	PPMU_ACP,
	PPMU_CAMIF,
	PPMU_CPU,
	PPMU_DMC0,
	PPMU_DMC1,
	PPMU_FSYS,
	PPMU_IMAGE,
	PPMU_LCD0,
	PPMU_LCD1,
	PPMU_MFC_L,
	PPMU_MFC_R,
	PPMU_TV, 
	PPMU_BUS_L,
	PPMU_BUS_R,
	PPMU_END,
} ppmu_index;

typedef enum {
	INVALID = 0,
	RD_BUSY,
	WR_BUSY,
	RW_BUSY,
	RD_REQ,
	WR_REQ,
	RD_DATA,
	WR_DATA,
	RW_DATA,
	RD_LATENCY = 0x12,
	WR_LATENCY = 0x16,
} event_type;

struct exynos4_ppmu_pd {
	char *name;
	unsigned int ccnt;
	bool ccnt_irq;
	unsigned int count[4];
	event_type event[4];
	bool event_irq[4];
};

struct exynos4_ppmu_data {
	ppmu_index index;
	unsigned int ccnt;
	unsigned int count0;
	unsigned int count1;
	unsigned int count2;
	unsigned long long count3;
	unsigned long load;
};

extern void exynos_ppmu_set_pd(struct exynos4_ppmu_pd *pd,
			struct platform_device *ppmu);
extern int ppmu_register_notifier(struct notifier_block *nb, ppmu_index index);
extern struct exynos4_ppmu_data exynos4_ppmu_update(ppmu_index index);

#endif /* _LINUX_EXYNOS4_PPMU_H */
