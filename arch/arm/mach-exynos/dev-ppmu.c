/*
 * linux/arch/arm/mach-exynos/dev-ppmu.c
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
 
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/platform_data/exynos4_ppmu.h>

#include <mach/map.h>
#include <mach/irqs.h>
#include <mach/dev-ppmu.h>

#include <plat/devs.h>
#include <plat/s5p-clock.h>

#define EXYNOS_PA_PPMU(ipbase) EXYNOS4_PA_PPMU_##ipbase

#define PPMU_RESOURCE(ipname, base, irq) \
static struct resource ppmu_resource_##ipname[] =\
{\
	{\
		.start	= EXYNOS_PA_PPMU(base),\
		.end	= EXYNOS_PA_PPMU(base) + SZ_64K - 1,\
		.flags	= IORESOURCE_MEM,\
	}, {\
		.start	= IRQ_PPMU_##irq,\
		.end	= IRQ_PPMU_##irq,\
		.flags	= IORESOURCE_IRQ,\
	},\
}

#define PPMU_PLATFORM_DEVICE(ipname, devid) \
struct platform_device PPMU_PLATDEV(ipname) =\
{\
	.name		= PPMU_DEVNAME_BASE,\
	.id		= devid,\
	.num_resources	= ARRAY_SIZE(ppmu_resource_##ipname),\
	.resource	= ppmu_resource_##ipname,\
}

PPMU_RESOURCE(3d, 3D, 3D);
PPMU_RESOURCE(acp, ACP, ACP0_M0);
PPMU_RESOURCE(camif,	CAMIF, CAMIF_M0);
PPMU_RESOURCE(cpu, CPU, CORE_D_S0);
PPMU_RESOURCE(dmc_l, DMC0, CORE_D_M0);
PPMU_RESOURCE(dmc_r, DMC1, CORE_D_M1);
PPMU_RESOURCE(fsys, FSYS, FILE_D_M0);
PPMU_RESOURCE(image, IMAGE,	IMAGE_M0);
PPMU_RESOURCE(lcd0, LCD0, LCD0);
PPMU_RESOURCE(lcd1, LCD1, LCD1);
PPMU_RESOURCE(mfc_l, MFC_L,	MFC_M0);
PPMU_RESOURCE(mfc_r, MFC_R,	MFC_M1);
PPMU_RESOURCE(tv, TV, TV_M0);
PPMU_RESOURCE(bus_left,	BUS_L,	D_LEFT_M0);
PPMU_RESOURCE(bus_right,	BUS_R,	D_RIGHT_M0);

PPMU_PLATFORM_DEVICE(3d, PPMU_3D);
PPMU_PLATFORM_DEVICE(acp, PPMU_ACP);
PPMU_PLATFORM_DEVICE(camif, PPMU_CAMIF);
PPMU_PLATFORM_DEVICE(cpu, PPMU_CPU);
PPMU_PLATFORM_DEVICE(dmc_l, PPMU_DMC0);
PPMU_PLATFORM_DEVICE(dmc_r, PPMU_DMC1);
PPMU_PLATFORM_DEVICE(fsys, PPMU_FSYS);
PPMU_PLATFORM_DEVICE(image, PPMU_IMAGE);
PPMU_PLATFORM_DEVICE(lcd0, PPMU_LCD0);
PPMU_PLATFORM_DEVICE(lcd1, PPMU_LCD1);
PPMU_PLATFORM_DEVICE(mfc_l, PPMU_MFC_L);
PPMU_PLATFORM_DEVICE(mfc_r, PPMU_MFC_R);
PPMU_PLATFORM_DEVICE(tv, PPMU_TV);
PPMU_PLATFORM_DEVICE(bus_left, PPMU_BUS_L);
PPMU_PLATFORM_DEVICE(bus_right, PPMU_BUS_R);

void __init exynos_ppmu_set_pd(struct exynos4_ppmu_pd *pd,
			struct platform_device *ppmu)
{
	struct exynos4_ppmu_pd *npd;

	npd = s3c_set_platdata(pd, sizeof(struct exynos4_ppmu_pd), ppmu);
	pr_debug("name:%s: npd = 0x%p\n", ppmu->name, npd);
}
