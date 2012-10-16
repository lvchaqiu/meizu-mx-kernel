/* linux/arch/arm/plat-samsung/include/plat/jpeg-core.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Samsung JPEG Controller core functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_PLAT_JPEG_CORE_H
#define __ASM_PLAT_JPEG_CORE_H __FILE__

/* These functions are only for use with the core support code, such as
 * the cpu specific initialisation code
 */

/* re-define device name depending on support. */
static inline void s5p_jpeg_setname(char *name)
{
#ifdef CONFIG_VIDEO_JPEG_MX
	s5p_device_jpeg.name = name;
#endif
}

#endif /* __ASM_PLAT_ADC_CORE_H */
