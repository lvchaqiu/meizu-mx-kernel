/* linux/arch/arm/plat-s5p/dev-dsim0.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * InKi Dae <inki.dae@samsung.com>
 *
 * device definitions for Samsung SoC MIPI-DSIM.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/fb.h>

#include <mach/map.h>
#include <asm/irq.h>

#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>

#include <plat/regs-mipidsim.h>
#include <plat/mipi_dsim.h>

static struct resource s5p_mipi_dsim0_resource[] = {
	[0] = {
		.start = S5P_PA_DSIM0,
		.end   = S5P_PA_DSIM0 + SZ_64K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_MIPIDSI0,
		.end   = IRQ_MIPIDSI0,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_mipi_dsim0 = {
	.name		= "s5p-mipi-dsim",
	.id			= 0,
	.num_resources= ARRAY_SIZE(s5p_mipi_dsim0_resource),
	.resource		= s5p_mipi_dsim0_resource,
};

static struct resource s5p_mipi_dsim1_resource[] = {
	[0] = {
		.start = S5P_PA_DSIM1,
		.end   = S5P_PA_DSIM1 + SZ_64K - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_MIPIDSI1,
		.end   = IRQ_MIPIDSI1,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_mipi_dsim1 = {
	.name		= "s5p-mipi-dsim",
	.id			= 1,
	.num_resources= ARRAY_SIZE(s5p_mipi_dsim1_resource),
	.resource		= s5p_mipi_dsim1_resource,
};

void __init s5p_dsim_set_platdata(struct s5p_platform_mipi_dsim *pd, int id)
{
	struct s5p_platform_mipi_dsim *npd;

	if (!pd) {
		printk(KERN_ERR "%s: no platform data\n", __func__);
		return;
	}

	npd = s3c_set_platdata(pd, sizeof(struct s5p_platform_mipi_dsim),
			       id ? &s5p_device_mipi_dsim1 : &s5p_device_mipi_dsim0);
}
