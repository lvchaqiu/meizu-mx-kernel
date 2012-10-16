/*
 *
 * (C) COPYRIGHT 2010-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#include <osk/mali_osk.h>

void oskp_debug_print(const char *fmt, ...)
{
	char buffer[OSK_DEBUG_MESSAGE_SIZE];
	va_list args;
	va_start(args, fmt);
	cutils_cstr_vsnprintf(buffer, OSK_DEBUG_MESSAGE_SIZE, fmt, args);
	printk(buffer);
	va_end(args);
}

