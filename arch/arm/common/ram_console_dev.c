/* arch/arm/common/ram_console_dev.c
 *
 * Copyright (C) 2012 Meizu Co., Ltd.
 * Author: Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/platform_data/ram_console.h>
#include <linux/cma.h>

struct ram_console_platform_data ram_console_bootinfo = {
	.bootinfo = RAM_CONSOLE_BOOT_INFO,
};

struct platform_device ram_console_dev = {
	.name = "ram_console",
	.id = -1,
	.dev = {
		.platform_data = &ram_console_bootinfo,
	},
};

static int __init ram_console_dev_init(void)
{
	int ret;

	ret = platform_device_register(&ram_console_dev);
	if (ret) {
		pr_err("unable to register ram_console platform device\n");
		return ret;
	}

	return 0;
}

arch_initcall(ram_console_dev_init);
