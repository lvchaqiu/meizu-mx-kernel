/*
 * Register interface file for Samsung Camera Interface (FIMC-Lite) driver
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef FLITE_CORE_H_
#define FLITE_CORE_H_

/* #define DEBUG */
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/exynos_flite.h>
#include <media/v4l2-ioctl.h>
#ifdef CONFIG_ARCH_EXYNOS5
#include <media/exynos_mc.h>
#endif
#include "fimc-lite-reg.h"

#define flite_info(fmt, args...) \
	printk(KERN_INFO "[INFO]%s:%d: "fmt "\n", __func__, __LINE__, ##args)
#define flite_err(fmt, args...) \
	printk(KERN_ERR "[ERROR]%s:%d: "fmt "\n", __func__, __LINE__, ##args)
#define flite_warn(fmt, args...) \
	printk(KERN_WARNING "[WARNNING]%s:%d: "fmt "\n", __func__, __LINE__, ##args)

#ifdef DEBUG
#define flite_dbg(fmt, args...) \
	printk(KERN_DEBUG "[DEBUG]%s:%d: " fmt "\n", __func__, __LINE__, ##args)
#else
#define flite_dbg(fmt, args...)
#endif

#define FLITE_MAX_RESET_READY_TIME	20 /* 100ms */
#define FLITE_MAX_WIDTH_SIZE		8192
#define FLITE_MAX_HEIGHT_SIZE		8192
#ifdef CONFIG_ARCH_EXYNOS4
#define FLITE_MAX_MBUS_NUM		1
#endif
enum flite_input_entity {
	FLITE_INPUT_NONE,
	FLITE_INPUT_SENSOR,
	FLITE_INPUT_CSIS,
};

enum flite_output_entity {
	FLITE_OUTPUT_NONE,
	FLITE_OUTPUT_GSC,
};

enum flite_out_path {
	FLITE_ISP,
	FLITE_DMA,
};

enum flite_state {
	FLITE_ST_POWERED,
	FLITE_ST_STREAMING,
	FLITE_ST_SUSPENDED,
};

/**
  * struct flite_fmt - driver's color format data
  * @name :	format description
  * @code :	Media Bus pixel code
  * @fmt_reg :	H/W bit for setting format
  */
struct flite_fmt {
	char 				*name;
	enum v4l2_mbus_pixelcode	code;
	u32				fmt_reg;
	u32				is_yuv;
};

/**
 * struct flite_frame - source/target frame properties
 * @o_width:	buffer width as set by S_FMT
 * @o_height:	buffer height as set by S_FMT
 * @width:	image pixel width
 * @height:	image pixel weight
 * @offs_h:	image horizontal pixel offset
 * @offs_v:	image vertical pixel offset
 */

/*
		o_width
	---------------------
	|    width(cropped) |
	|    	----- 	    |
	|offs_h |   | 	    |
	|    	-----	    |
	|		    |
	---------------------
 */
struct flite_frame {
	u32 o_width;
	u32 o_height;
	u32 width;
	u32 height;
	u32 offs_h;
	u32 offs_v;
};
/**
  * struct flite_dev - top structure of FIMC-Lite device
  * @pdev :	pointer to the FIMC-Lite platform device
  * @lock :	the mutex protecting this data structure
  * @sd :	subdevice pointer of FIMC-Lite
  * @fmt :	Media bus format of FIMC-Lite
  * @regs_res :	ioremapped regs of FIMC-Lite
  * @regs :	SFR of FIMC-Lite
  */
struct flite_dev {
	struct platform_device		*pdev;
	struct exynos_platform_flite	*pdata; /* depended on isp */
	spinlock_t			slock;
	struct exynos_md		*mdev;
	struct v4l2_subdev		sd;
#if defined(CONFIG_MEDIA_CONTROLLER) && defined(CONFIG_ARCH_EXYNOS5)
	struct media_pad		pads[FLITE_PADS_NUM];
#endif
	struct v4l2_mbus_framefmt	mbus_fmt;
	struct flite_frame		source_frame;
	struct resource			*regs_res;
	void __iomem			*regs;
	int				irq;
	unsigned long			state;
	u32				out_path;
	wait_queue_head_t		irq_queue;
	u32				id;
	enum flite_input_entity		input;
	enum flite_output_entity	output;
};

/* inline function for performance-sensitive region */
static inline void flite_hw_clear_irq(struct flite_dev *dev)
{
	u32 cfg = readl(dev->regs + FLITE_REG_CISTATUS);
	cfg &= ~FLITE_REG_CISTATUS_IRQ_CAM;
	writel(cfg, dev->regs + FLITE_REG_CISTATUS);
}

static inline void flite_hw_get_int_src(struct flite_dev *dev, u32 *src)
{
	*src = readl(dev->regs + FLITE_REG_CISTATUS);
	*src &= FLITE_REG_CISTATUS_IRQ_MASK;
}

inline struct flite_fmt const *find_flite_format(struct
		v4l2_mbus_framefmt *mf);
/* fimc-reg.c */
void flite_hw_set_cam_source_size(struct flite_dev *dev);
void flite_hw_set_cam_channel(struct flite_dev *dev);
void flite_hw_set_camera_type(struct flite_dev *dev, struct s3c_platform_camera *cam);
int flite_hw_set_source_format(struct flite_dev *dev);
void flite_hw_set_output_dma(struct flite_dev *dev, bool enable);
void flite_hw_set_interrupt_source(struct flite_dev *dev, u32 source);
void flite_hw_set_config_irq(struct flite_dev *dev, struct s3c_platform_camera *cam);
void flite_hw_set_window_offset(struct flite_dev *dev);
void flite_hw_set_capture_start(struct flite_dev *dev);
void flite_hw_set_capture_stop(struct flite_dev *dev);
void flite_hw_set_last_capture_end_clear(struct flite_dev *dev);

#endif /* FLITE_CORE_H */
