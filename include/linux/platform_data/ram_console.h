/*
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _INCLUDE_LINUX_PLATFORM_DATA_RAM_CONSOLE_H_
#define _INCLUDE_LINUX_PLATFORM_DATA_RAM_CONSOLE_H_

#include <generated/utsrelease.h>

#define BOOT_TIME_LABEL "Boot time: "
#define BOOT_KMSG_LABEL "Boot kmsg:\n"
#define BOOT_INFO_LABEL "Boot info: "
#define BOOT_MACH_LABEL "Boot mach: "
#define BOOT_FROM_LABEL "Boot from: "
#define BOOT_STAT_LABEL "Boot stat:\n"
#define BOOT_PARM_LABEL "Boot parm: "

#define RAM_CONSOLE_BOOT_INFO			\
	CONFIG_DEFAULT_HOSTNAME ", "		\
	UTS_RELEASE ", "			\
	CONFIG_LOCALVERSION "\n"

struct ram_console_platform_data {
	const char *bootinfo;
};

#endif /* _INCLUDE_LINUX_PLATFORM_DATA_RAM_CONSOLE_H_ */
