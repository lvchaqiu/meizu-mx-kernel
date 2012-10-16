/* linux/arch/arm/plat-s5p/include/plat/sysmmu.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * Samsung System MMU driver for S5P platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM__PLAT_SYSMMU_H
#define __ASM__PLAT_SYSMMU_H __FILE__

struct device;

 /**
 * enum s5p_sysmmu_ip - integrated peripherals identifiers
 * @S5P_SYSMMU_MDMA:   MDMA
 * @S5P_SYSMMU_SSS:    SSS
 * @S5P_SYSMMU_FIMC0:  FIMC0
 * @S5P_SYSMMU_FIMC1:  FIMC1
 * @S5P_SYSMMU_FIMC2:  FIMC2
 * @S5P_SYSMMU_FIMC3:  FIMC3
 * @S5P_SYSMMU_JPEG:   JPEG
 * @S5P_SYSMMU_FIMD0:  FIMD0
 * @S5P_SYSMMU_FIMD1:  FIMD1
 * @S5P_SYSMMU_PCIe:   PCIe
 * @S5P_SYSMMU_G2D:    G2D
 * @S5P_SYSMMU_ROTATOR:        ROTATOR
 * @S5P_SYSMMU_MDMA2:  MDMA2
 * @S5P_SYSMMU_TV:     TV
 * @S5P_SYSMMU_MFC_L:  MFC_L
 * @S5P_SYSMMU_MFC_R:  MFC_R
  */
enum s5p_sysmmu_ip {
       S5P_SYSMMU_MDMA,
       S5P_SYSMMU_SSS,
       S5P_SYSMMU_FIMC0,
       S5P_SYSMMU_FIMC1,
       S5P_SYSMMU_FIMC2,
       S5P_SYSMMU_FIMC3,
       S5P_SYSMMU_JPEG,
       S5P_SYSMMU_FIMD0,
       S5P_SYSMMU_FIMD1,
       S5P_SYSMMU_PCIe,
       S5P_SYSMMU_G2D,
       S5P_SYSMMU_ROTATOR,
       S5P_SYSMMU_MDMA2,
       S5P_SYSMMU_TV,
       S5P_SYSMMU_MFC_L,
       S5P_SYSMMU_MFC_R,
       S5P_SYSMMU_TOTAL_IP_NUM,
};

#endif /* __ASM_PLAT_SYSMMU_H */
