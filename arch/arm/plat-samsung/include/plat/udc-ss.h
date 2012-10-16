/* arch/arm/plat-samsung/include/plat/udc-ss.h
 *
 * Copyright (c) 2011 Samsung Electronics Co. Ltd
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * EXYNOS SuperSpeed USB 3.0 Device Controller platform information.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/**
 * struct exynos_ss_udc_plat - platform data for EXYNOS USB 3.0 UDC
 */
struct exynos_ss_udc_plat {
	int (*phy_init)(struct platform_device *pdev, int type);
	int (*phy_exit)(struct platform_device *pdev, int type);
};

struct exynos_xhci_plat {
	int (*phy_init)(struct platform_device *pdev, int type);
	int (*phy_exit)(struct platform_device *pdev, int type);
};

extern void exynos_ss_udc_set_platdata(struct exynos_ss_udc_plat *pd);
extern void exynos_xhci_set_platdata(struct exynos_xhci_plat *pd);
