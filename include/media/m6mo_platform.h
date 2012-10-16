/* linux/include/media/m6mo_platform.h
 *
 * Copyright (c) 2011 Meizu Co., Ltd.
 *		http://www.meizu.com/
 *
 * Driver for M6MO (8M camera) from Fujisu Electronics
 * 1/6" 8MMp CMOS Image Sensor with ISP
 * supporting MIPI CSI-2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __M6MO_PLATFORM_DATA__H__
#define __M6MO_PLATFORM_DATA__H__

struct m6mo_platform_data {
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
