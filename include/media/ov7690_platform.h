#ifndef _OV7690_PLATFORM_H_
#define _OV7690_PLATFORM_H_

/* linux/include/media/m6mo_platform.h
 *
 * Copyright (c) 2011 Meizu Co., Ltd.
 *		http://www.meizu.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct ov7690_platform_data {
	unsigned int default_width;
	unsigned int default_height;
	unsigned int pixelformat;
	int freq;	/* MCLK in KHz */

	/* This ISP supports Parallel & CSI-2 */
	int is_mipi;
	
	int (*init)(struct device *dev);
	int (*power)(int enable);
	int (*clock_on)(struct device *dev, int enable);
};

#endif
