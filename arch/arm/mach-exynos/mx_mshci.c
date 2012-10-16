/*
 * mx_mshci.c - lcd driver helper for mx board
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  lvcha qiu   <lvcha@meizu.com>
 *
 * This program is not provided / owned by Maxim Integrated Products.
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
 *
 */

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/mshci.h>
#include <plat/cpu.h>

#ifdef CONFIG_EXYNOS4_DEV_MSHC
static struct s3c_mshci_platdata __initdata mx_mshc_pdata = {
	.cd_type		= S3C_MSHCI_CD_PERMANENT,
	.fifo_depth	= 0x80,
	.max_width	= 8,
#if defined(CONFIG_EXYNOS4_MSHC_DDR)
	.host_caps	= MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23 |
				   MMC_CAP_1_8V_DDR |MMC_CAP_UHS_DDR50,
	.host_caps2	= MMC_CAP2_PACKED_CMD,
#else
	.host_caps	= MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
#endif
};

static int  __init mx_init_mshci(void)
{
	if (soc_is_exynos4210())
		mx_mshc_pdata.fifo_depth	= 0x20;

	s3c_mshci_set_platdata(&mx_mshc_pdata);
	return 0;
}

arch_initcall(mx_init_mshci);
#endif
