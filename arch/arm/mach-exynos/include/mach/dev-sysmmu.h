/* linux/arch/arm/mach-exynos/include/mach/dev-sysmmu.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - System MMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ARM_MACH_EXYNOS_SYSMMU_H_
#define _ARM_MACH_EXYNOS_SYSMMU_H_

#define SYSMMU_DEVNAME_BASE "exynos-sysmmu"

#ifdef CONFIG_EXYNOS_IOMMU
#include <linux/device.h>

#define SYSMMU_PLATDEV(ipname) exynos_device_sysmmu_##ipname

#ifdef CONFIG_EXYNOS_DEV_PD
#define ASSIGN_SYSMMU_POWERDOMAIN(ipname, powerdomain) \
		SYSMMU_PLATDEV(ipname).dev.parent = powerdomain
#else
#define ASSIGN_SYSTEM_POWERDOMAIN(ipname, powerdomain) do { } while (0)
#endif

extern struct platform_device SYSMMU_PLATDEV(sss);
extern struct platform_device SYSMMU_PLATDEV(jpeg);
extern struct platform_device SYSMMU_PLATDEV(fimd1);
extern struct platform_device SYSMMU_PLATDEV(pcie);
extern struct platform_device SYSMMU_PLATDEV(2d);
extern struct platform_device SYSMMU_PLATDEV(rot);
extern struct platform_device SYSMMU_PLATDEV(mdma);
extern struct platform_device SYSMMU_PLATDEV(tv);
extern struct platform_device SYSMMU_PLATDEV(mfc_l);
extern struct platform_device SYSMMU_PLATDEV(mfc_r);
extern struct platform_device SYSMMU_PLATDEV(is_isp);
extern struct platform_device SYSMMU_PLATDEV(is_drc);
extern struct platform_device SYSMMU_PLATDEV(is_fd);
extern struct platform_device SYSMMU_PLATDEV(is_cpu);

#ifdef CONFIG_ARCH_EXYNOS4
extern struct platform_device SYSMMU_PLATDEV(fimc0);
extern struct platform_device SYSMMU_PLATDEV(fimc1);
extern struct platform_device SYSMMU_PLATDEV(fimc2);
extern struct platform_device SYSMMU_PLATDEV(fimc3);
extern struct platform_device SYSMMU_PLATDEV(g2d_acp);
extern struct platform_device SYSMMU_PLATDEV(fimd0);
#endif

#ifdef CONFIG_ARCH_EXYNOS5
extern struct platform_device SYSMMU_PLATDEV(gsc0);
extern struct platform_device SYSMMU_PLATDEV(gsc1);
extern struct platform_device SYSMMU_PLATDEV(gsc2);
extern struct platform_device SYSMMU_PLATDEV(gsc3);

extern struct platform_device SYSMMU_PLATDEV(is_sclrc);
extern struct platform_device SYSMMU_PLATDEV(is_sclrp);
extern struct platform_device SYSMMU_PLATDEV(is_odc);
extern struct platform_device SYSMMU_PLATDEV(is_dis0);
extern struct platform_device SYSMMU_PLATDEV(is_dis1);
extern struct platform_device SYSMMU_PLATDEV(is_3dnr);
#endif

static inline void sysmmu_set_owner(struct device *sysmmu, struct device *owner)
{
	sysmmu->archdata.iommu = owner;
}
int __init s5p_create_iommu_mapping(struct device *client, dma_addr_t base,
                                   unsigned int size, int order);

#else /* !CONFIG_EXYNOS_IOMMU */
#define sysmmu_set_owner(sysmmu, owner) do { } while (0)
#define ASSIGN_SYSMMU_POWERDOMAIN(ipname, powerdomain) do { } while (0)
#endif

#define SYSMMU_CLOCK_NAME(ipname, id) SYSMMU_DEVNAME_BASE "." #id

#endif /* _ARM_MACH_EXYNOS_SYSMMU_H_ */
