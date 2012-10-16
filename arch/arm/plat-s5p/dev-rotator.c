/* linux/arch/arm/plat-s5p/dev-rotator.c
 *
 * Copyright (c) 2010 Samsung Electronics
 *
 * Base S5P Rotator resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>

#include <mach/map.h>
#include <mach/irqs.h>

/* rotator interface */
static struct resource s5p_rotator_resource[] = {
	[0] = {
		.start = S5P_PA_ROTATOR,
		.end   = S5P_PA_ROTATOR + S5P_SZ_ROTATOR - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_ROTATOR,
		.end   = IRQ_ROTATOR,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_rotator = {
	.name		= "s5p-rotator",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_rotator_resource),
	.resource	= s5p_rotator_resource
};
EXPORT_SYMBOL(s5p_device_rotator);
