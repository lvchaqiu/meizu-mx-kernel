/* linux/drivers/media/video/samsung/fimg2d4x/fimg2d.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __FIMG2D_H
#define __FIMG2D_H __FILE__

#ifdef __KERNEL__

#include <linux/clk.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_BUSFREQ_OPP
#include <mach/dev.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define FIMG2D_MINOR			(240)
#define to_fimg2d_plat(d)		(to_platform_device(d)->dev.platform_data)

#ifdef CONFIG_VIDEO_FIMG2D_DEBUG
#define fimg2d_debug(fmt, arg...)	printk(KERN_INFO "[%s] " fmt, __func__, ## arg)
#else
#define fimg2d_debug(fmt, arg...)	do { } while (0)
#endif

#endif /* __KERNEL__ */

/* ioctl commands */
#define FIMG2D_IOCTL_MAGIC	'F'
#define FIMG2D_BITBLT_BLIT	_IOWR(FIMG2D_IOCTL_MAGIC, 0, struct fimg2d_blit)
#define FIMG2D_BITBLT_SYNC	_IO(FIMG2D_IOCTL_MAGIC, 1)
#define FIMG2D_BITBLT_VERSION	_IOR(FIMG2D_IOCTL_MAGIC, 2, struct fimg2d_version)

/**
 * DO NOT CHANGE THIS ORDER
 */
enum image_object {
	IMAGE_DST = 0,
	IMAGE_SRC,
	IMAGE_MSK,
	IMAGE_END,
};
#define MAX_IMAGES		IMAGE_END
#define IDST			IMAGE_DST
#define ISRC			IMAGE_SRC
#define IMSK			IMAGE_MSK

enum addr_space {
	ADDR_UNKNOWN,
	ADDR_PHYS,
	ADDR_KERN,
	ADDR_USER,
	ADDR_DEVICE,
};

/**
 * Pixel order complies with little-endian style
 *
 * DO NOT CHANGE THIS ORDER
 */
enum pixel_order {
	AX_RGB = 0,
	RGB_AX,
	AX_BGR,
	BGR_AX,
	ARGB_ORDER_END,

	P1_CRY1CBY0,
	P1_CBY1CRY0,
	P1_Y1CRY0CB,
	P1_Y1CBY0CR,
	P1_ORDER_END,

	P2_CRCB,
	P2_CBCR,
	P2_ORDER_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum color_format {
	CF_XRGB_8888 = 0,
	CF_ARGB_8888,
	CF_RGB_565,
	CF_XRGB_1555,
	CF_ARGB_1555,
	CF_XRGB_4444,
	CF_ARGB_4444,
	CF_RGB_888,
	CF_YCBCR_444,
	CF_YCBCR_422,
	CF_YCBCR_420,
	CF_A8,
	CF_L8,
	SRC_DST_FORMAT_END,

	CF_MSK_1BIT,
	CF_MSK_4BIT,
	CF_MSK_8BIT,
	CF_MSK_16BIT_565,
	CF_MSK_16BIT_1555,
	CF_MSK_16BIT_4444,
	CF_MSK_32BIT_8888,
	MSK_FORMAT_END,
};

enum rotation {
	ORIGIN,
	ROT_90,	/* clockwise */
	ROT_180,
	ROT_270,
	XFLIP,	/* x-axis flip */
	YFLIP,	/* y-axis flip */
};

/**
 * @NO_REPEAT: no effect
 * @REPEAT_NORMAL: repeat horizontally and vertically
 * @REPEAT_PAD: pad with pad color
 * @REPEAT_REFLECT: reflect horizontally and vertically
 * @REPEAT_CLAMP: pad with edge color of original image
 *
 * DO NOT CHANGE THIS ORDER
 */
enum repeat {
	NO_REPEAT = 0,
	REPEAT_NORMAL,	/* default setting */
	REPEAT_PAD,
	REPEAT_REFLECT, REPEAT_MIRROR = REPEAT_REFLECT,
	REPEAT_CLAMP,
};

enum scaling {
	NO_SCALING,
	SCALING_NEAREST,
	SCALING_BILINEAR,
};

/**
 * @SCALING_PERCENTAGE: percentage of width, height
 * @SCALING_PIXELS: coordinate of src, dest
 */
enum scaling_factor {
	SCALING_PERCENTAGE,
	SCALING_PIXELS,
};

/**
 * premultiplied alpha
 */
enum premultiplied {
	PREMULTIPLIED,
	NON_PREMULTIPLIED,
};

/**
 * @TRANSP: discard bluescreen color
 * @BLUSCR: replace bluescreen color with background color
 */
enum bluescreen {
	OPAQUE,
	TRANSP,
	BLUSCR,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum blit_op {
	BLIT_OP_SOLID_FILL = 0,

	BLIT_OP_CLR,
	BLIT_OP_SRC, BLIT_OP_SRC_COPY = BLIT_OP_SRC,
	BLIT_OP_DST,
	BLIT_OP_SRC_OVER,
	BLIT_OP_DST_OVER, BLIT_OP_OVER_REV = BLIT_OP_DST_OVER,
	BLIT_OP_SRC_IN,
	BLIT_OP_DST_IN, BLIT_OP_IN_REV = BLIT_OP_DST_IN,
	BLIT_OP_SRC_OUT,
	BLIT_OP_DST_OUT, BLIT_OP_OUT_REV = BLIT_OP_DST_OUT,
	BLIT_OP_SRC_ATOP,
	BLIT_OP_DST_ATOP, BLIT_OP_ATOP_REV = BLIT_OP_DST_ATOP,
	BLIT_OP_XOR,

	BLIT_OP_ADD,
	BLIT_OP_MULTIPLY,
	BLIT_OP_SCREEN,
	BLIT_OP_DARKEN,
	BLIT_OP_LIGHTEN,

	BLIT_OP_DISJ_SRC_OVER,
	BLIT_OP_DISJ_DST_OVER, BLIT_OP_SATURATE = BLIT_OP_DISJ_DST_OVER,
	BLIT_OP_DISJ_SRC_IN,
	BLIT_OP_DISJ_DST_IN, BLIT_OP_DISJ_IN_REV = BLIT_OP_DISJ_DST_IN,
	BLIT_OP_DISJ_SRC_OUT,
	BLIT_OP_DISJ_DST_OUT, BLIT_OP_DISJ_OUT_REV = BLIT_OP_DISJ_DST_OUT,
	BLIT_OP_DISJ_SRC_ATOP,
	BLIT_OP_DISJ_DST_ATOP, BLIT_OP_DISJ_ATOP_REV = BLIT_OP_DISJ_DST_ATOP,
	BLIT_OP_DISJ_XOR,

	BLIT_OP_CONJ_SRC_OVER,
	BLIT_OP_CONJ_DST_OVER, BLIT_OP_CONJ_OVER_REV = BLIT_OP_CONJ_DST_OVER,
	BLIT_OP_CONJ_SRC_IN,
	BLIT_OP_CONJ_DST_IN, BLIT_OP_CONJ_IN_REV = BLIT_OP_CONJ_DST_IN,
	BLIT_OP_CONJ_SRC_OUT,
	BLIT_OP_CONJ_DST_OUT, BLIT_OP_CONJ_OUT_REV = BLIT_OP_CONJ_DST_OUT,
	BLIT_OP_CONJ_SRC_ATOP,
	BLIT_OP_CONJ_DST_ATOP, BLIT_OP_CONJ_ATOP_REV = BLIT_OP_CONJ_DST_ATOP,
	BLIT_OP_CONJ_XOR,

	/* user select coefficient manually */
	BLIT_OP_USER_COEFF,

	BLIT_OP_USER_SRC_GA,

	/* Add new operation type here */

	/* end of blit operation */
	BLIT_OP_END,
};
#define MAX_FIMG2D_BLIT_OP (int)BLIT_OP_END

#ifdef __KERNEL__

/**
 * blit_sync - [kernel] bitblt sync mode
 * @BLIT_SYNC: sync mode
 * @BLIT_ASYNC: async mode
 *
 * Sync/async mode is capability of FIMG2D device driver
 * Sync mode is to wait for bitblt done irq,
 * async mode is not to wait for the irq.
 * User only need to pass 'pinnable' information via struct fimg2d_addr.
 * If memory allocator is pinnable, the driver can use async and batch
 * bitblt.
 */
enum blit_sync {
	BLIT_SYNC,
	BLIT_ASYNC,
};

/**
 * @addr: start address of clipped region for cache operation
 * @size: size of clipped region for cache operation
 */
struct fimg2d_cache {
	unsigned long addr;
	size_t size;
};

#endif /* __KERNEL__ */

struct fimg2d_version {
	unsigned int hw;
	unsigned int sw;
};

/**
 * @start: start address or unique id of image
 * @size: whole length of allocated image
 * @cacheable: memory is cacheable
 * @pinnable: memory is pinnable. currently not supported.
 */
struct fimg2d_addr {
	enum addr_space type;
	unsigned long start;
	size_t size;
	int cacheable;
	int pinnable;
};

struct fimg2d_rect {
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2; /* y1 + height */
};

/**
 * if factor is percentage, scale_w and scale_h are valid
 * if factor is pixels, src_w, src_h, dst_w, dst_h are valid
 */
struct fimg2d_scale {
	enum scaling mode;
	enum scaling_factor factor;

	/* percentage */
	int scale_w;
	int scale_h;

	/* pixels */
	int src_w, src_h;
	int dst_w, dst_h;
};

/**
 * coordinate from start address(0,0) of image
 */
struct fimg2d_clip {
	bool enable;
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2; /* y1 + height */
};

struct fimg2d_repeat {
	enum repeat mode;
	unsigned long pad_color;
};

/**
 * @bg_color: bg_color is valid only if bluescreen mode is BLUSCR.
 */
struct fimg2d_bluscr {
	enum bluescreen mode;
	unsigned long bs_color;
	unsigned long bg_color;
};

/**
 * @plane2: address info for CbCr in YCbCr 2plane mode
 */
struct fimg2d_image {
	struct fimg2d_addr addr;
	struct fimg2d_addr plane2;
	int width;
	int height;
	int stride;
	enum pixel_order order;
	enum color_format fmt;
};

/**
 * @op: bitblt operation mode
 * @premult: premultiplied or non_premultiplied mode
 * @g_alpha: global(constant) alpha. 0xff is opaque, 0x0 is transparnet
 * @dither: dithering
 * @rotate: rotation degree in clockwise
 * @scaling: common scaling info for source and mask image.
 * @repeat: repeat type (tile mode)
 * @bluscr: blue screen and transparent mode
 * @clipping: clipping region must be the same to or smaller than dest rect
 * @solid_color: valid when op is solid fill or src is null
 * @src: source image info. set null when use solid_color
 * @msk: mask image info. set null when does not use mask
 * @dst: dest image info. must be set
 * @src_rect: source region to read. set null when use solid_color
 * @dst_rect: dest region to write. must be set
 * @msk_rect: mask region to read. set null when does not use mask
 * @seq_no: debugging info. set sequence number or pid
 */
struct fimg2d_blit {
	enum blit_op op;

	enum premultiplied premult;
	unsigned char g_alpha;
	bool dither;
	enum rotation rotate;
	struct fimg2d_scale *scaling;
	struct fimg2d_repeat *repeat;
	struct fimg2d_bluscr *bluscr;
	struct fimg2d_clip *clipping;

	unsigned long solid_color;
	struct fimg2d_image *src;
	struct fimg2d_image *dst;
	struct fimg2d_image *msk;

	struct fimg2d_rect *src_rect;
	struct fimg2d_rect *dst_rect;
	struct fimg2d_rect *msk_rect;

	unsigned int seq_no;
};

#ifdef __KERNEL__

/* enables definition to estimate performance */
#undef PERF_PROFILE

enum perf_desc {
	PERF_L1CC_FLUSH,
	PERF_WORKQUE,
	PERF_L2CC_FLUSH,
	PERF_SFR,
	PERF_BLIT,
	PERF_KERN,	/* a whole driver */
	PERF_END
};
#define MAX_PERF_DESCS		PERF_END

struct fimg2d_perf {
	int valid;
	struct timeval start;
	struct timeval end;
};

/**
 * @pgd: base address of arm mmu pagetable
 * @ncmd: request count in blit command queue
 * @wait_q: conext wait queue head
*/
struct fimg2d_context {
	struct mm_struct *mm;
	atomic_t ncmd;
	wait_queue_head_t wait_q;
	struct fimg2d_perf perf[MAX_PERF_DESCS];
};

/**
 * @seq_no: used for debugging
 * @node: list head of blit command queue
 */
struct fimg2d_bltcmd {
	enum blit_op op;

	enum premultiplied premult;
	unsigned char g_alpha;
	bool dither;
	enum rotation rotate;
	struct fimg2d_scale scaling;
	struct fimg2d_repeat repeat;
	struct fimg2d_bluscr bluscr;
	struct fimg2d_clip clipping;

	bool srcen;
	bool dsten;
	bool msken;

	unsigned long solid_color;
	struct fimg2d_image src;
	struct fimg2d_image dst;
	struct fimg2d_image msk;

	struct fimg2d_rect src_rect;
	struct fimg2d_rect dst_rect;
	struct fimg2d_rect msk_rect;

	size_t size_all;
	struct fimg2d_cache src_cache;
	struct fimg2d_cache dst_cache;
	struct fimg2d_cache msk_cache;

	unsigned int seq_no;
	struct fimg2d_context *ctx;
	struct list_head node;
};

/**
 * @suspended: in suspend mode
 * @clkon: power status for runtime pm
 * @mem: resource platform device
 * @regs: base address of hardware
 * @dev: pointer to device struct
 * @err: true if hardware is timed out while blitting
 * @irq: irq number
 * @nctx: context count
 * @busy: 1 if hardware is running
 * @bltlock: spinlock for blit
 * @wait_q: blit wait queue head
 * @cmd_q: blit command queue
 * @workqueue: workqueue_struct for kfimg2dd
*/
struct fimg2d_control {
	atomic_t suspended;
	atomic_t clkon;
	struct clk *clock;
	struct device *dev;
	struct resource *mem;
	void __iomem *regs;

	bool err;
	int irq;
	atomic_t nctx;
	atomic_t busy;
	atomic_t active;
	spinlock_t bltlock;
	wait_queue_head_t wait_q;
	struct list_head cmd_q;
	struct workqueue_struct *work_q;

	void (*blit)(struct fimg2d_control *info);
	void (*configure)(struct fimg2d_control *info, struct fimg2d_bltcmd *cmd);
	void (*run)(struct fimg2d_control *info);
	void (*stop)(struct fimg2d_control *info);
	void (*dump)(struct fimg2d_control *info);
	void (*finalize)(struct fimg2d_control *info);
#ifdef CONFIG_BUSFREQ_OPP
	struct device *bus_dev;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif	
};

int fimg2d_register_ops(struct fimg2d_control *info);

#endif /* __KERNEL__ */

#endif /* __FIMG2D_H__ */
