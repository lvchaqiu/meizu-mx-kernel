/* linux/drivers/media/video/samsung/rotator/rotator_v2xx.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * http://www.samsung.com/
 *
 * Header file of the driver for Samsung Image Rotator
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef	_S5P_ROTATOR_V2XX_H_
#define	_S5P_ROTATOR_V2XX_H_

#define	ROTATOR_IOCTL_MAGIC 'R'

#define	ROTATOR_MINOR	230
#define	ROTATOR_TIMEOUT	100

#define	ROTATOR_NAME	"s5p-rotator"
#define	ROT_CLK_NAME	"rotator"

#define	ROTATOR_EXEC	_IO(ROTATOR_IOCTL_MAGIC, 0)

enum rot_status	{
	ROT_IDLE,
	ROT_RUN,
	ROT_READY_SLEEP,
	ROT_SLEEP,
};

struct rot_ctrl	{
	char			name[16];
	atomic_t		in_use;
	char			clk_name[16];
	struct clk		*clock;
	struct device		*dev;
	void __iomem		*regs;
	int			irq_num;
	struct mutex		lock;
	wait_queue_head_t	wq;
	enum rot_status		status;
};

enum rot_format	{
	ROT_YUV420_3P =	0,
	ROT_YUV420_2P =	1,
	ROT_YUV422_1P =	3,
	ROT_RGB565 = 4,
	ROT_RGB888 = 6
};

enum rot_degree	{
	ROT_0,
	ROT_90,
	ROT_180,
	ROT_270,
} ;

enum rot_flip {
	ROT_FLIP_NONE = 0,
	ROT_VFLIP = 2,
	ROT_HFLIP = 3
} ;

struct rot_rect	{
	u32   left;
	u32   top;
	u32   width;
	u32   height;
};

struct rot_param {
	dma_addr_t	src_base[3];	/* src image base address */
	dma_addr_t	dst_base[3];	/* dst image base address */

	struct rot_rect	src_full;	/* src image full rect */
	struct rot_rect	src_crop;	/* src image crop rect */
	struct rot_rect	dst_full;	/* dst image full rect */
	struct rot_rect	dst_crop;	/* dst image crop rect */

	enum rot_format	fmt;		/* color space of image	*/
	enum rot_degree	degree;		/* degree */
	enum rot_flip flip;		/* flip	*/
};
#endif /* _S5P_ROTATOR_V2XX_H_	*/

