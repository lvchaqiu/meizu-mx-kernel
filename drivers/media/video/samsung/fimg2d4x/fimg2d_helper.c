/* linux/drivers/media/video/samsung/fimg2d4x/fimg2d_helper.c
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

#include "fimg2d.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

#define MAX_COLOR_FORMAT	(MSK_FORMAT_END+1)

static int cf2bpp[MAX_COLOR_FORMAT] = {
	32,	/* CF_XRGB_8888 */
	32,	/* CF_ARGB_8888 */
	16,	/* CF_RGB_565 */
	16,	/* CF_XRGB_1555 */
	16,	/* CF_ARGB_1555 */
	16,	/* CF_XRGB_4444 */
	16,	/* CF_ARGB_4444 */
	24,	/* CF_RGB_888 */
	8,	/* CF_YCBCR_444 */
	8,	/* CF_YCBCR_422 */
	8,	/* CF_YCBCR_420 */
	8,	/* CF_A8 */
	8,	/* CF_L8 */
	0,	/* SRC_DST_FORMAT_END */
	1,	/* CF_MSK_1BIT */
	4,	/* CF_MSK_4BIT */
	8,	/* CF_MSK_8BIT */
	16,	/* CF_MSK_16BIT_565 */
	16,	/* CF_MSK_16BIT_1555 */
	16,	/* CF_MSK_16BIT_4444 */
	32,	/* CF_MSK_32BIT_8888 */
	0,	/* MSK_FORMAT_END */
};

int point_to_offset(int point, enum color_format cf)
{
	return (point * cf2bpp[cf]) >> 3;
}

int width_to_bytes(int width, enum color_format cf)
{
	int bpp = cf2bpp[cf];

	switch (bpp) {
	case 1:
		return (width + 7) >> 3;
	case 4:
		return (width + 1) >> 1;
	case 8:
	case 16:
	case 24:
	case 32:
		return width * bpp >> 3;
	default:
		return 0;
	}
}

void perf_print(struct fimg2d_context *ctx, int seq_no)
{
	int i;
	long time;
	struct fimg2d_perf *perf;

	for (i = 0; i < MAX_PERF_DESCS; i++) {
		perf = &ctx->perf[i];
		if (perf->valid != 0x11)
			continue;
		time = elapsed_usec(&perf->start, &perf->end);
		printk(KERN_INFO "[FIMG2D PERF %02d] ctx(0x%08x) seq(%d) %8ld   usec\n",
				i, (unsigned int)ctx, seq_no, time);
	}
	printk(KERN_INFO "[FIMG2D PERF **]\n");
}

void fimg2d_print_params(struct fimg2d_blit __user *u)
{
	struct fimg2d_image *image;
	struct fimg2d_rect *rect;

	fimg2d_debug("op: %d\n", u->op);
	fimg2d_debug("solid color: 0x%lx\n", u->solid_color);
	fimg2d_debug("g_alpha: 0x%x\n", u->g_alpha);
	fimg2d_debug("premultiplied: %d\n", u->premult);
	fimg2d_debug("dither: %d\n", u->dither);
	fimg2d_debug("rotate: %d\n", u->rotate);

	if (u->scaling) {
		fimg2d_debug("scaling mode: %d, factor: %d, percent(w:%d, h:%d) "
				"pixel(s:%d,%d d:%d,%d)\n",
				u->scaling->mode, u->scaling->factor,
				u->scaling->scale_w, u->scaling->scale_h,
				u->scaling->src_w, u->scaling->src_h,
				u->scaling->dst_w, u->scaling->dst_h);
	}

	if (u->repeat) {
		fimg2d_debug("repeat mode: %d, pad color: 0x%lx\n",
				u->repeat->mode, u->repeat->pad_color);
	}

	if (u->bluscr) {
		fimg2d_debug("bluescreen mode: %d, bs_color: 0x%lx bg_color: 0x%lx\n",
				u->bluscr->mode, u->bluscr->bs_color, u->bluscr->bg_color);
	}

	if (u->clipping) {
		fimg2d_debug("clipping mode: %d, LT(%d,%d) RB(%d,%d)\n",
				u->clipping->enable, u->clipping->x1, u->clipping->y1,
				u->clipping->x2, u->clipping->y2);
	}

	if (u->dst) {
		image = u->dst;
		fimg2d_debug("%s type: %d addr: 0x%lx size: 0x%x cacheable: %d\n",
				imagename(IDST),
				image->addr.type, image->addr.start, image->addr.size,
				image->addr.cacheable);
		fimg2d_debug("%s width: %d height: %d stride: %d order: %d format: %d\n",
				imagename(IDST),
				image->width, image->height, image->stride,
				image->order, image->fmt);
	}

	if (u->src) {
		image = u->src;
		fimg2d_debug("%s type: %d addr: 0x%lx size: 0x%x cacheable: %d\n",
				imagename(ISRC),
				image->addr.type, image->addr.start, image->addr.size,
				image->addr.cacheable);
		fimg2d_debug("%s width: %d height: %d stride: %d order: %d format: %d\n",
				imagename(ISRC),
				image->width, image->height, image->stride,
				image->order, image->fmt);
	}

	if (u->msk) {
		image = u->msk;
		fimg2d_debug("%s type: %d addr: 0x%lx size: 0x%x cacheable: %d\n",
				imagename(IMSK),
				image->addr.type, image->addr.start, image->addr.size,
				image->addr.cacheable);
		fimg2d_debug("%s width: %d height: %d stride: %d order: %d format: %d\n",
				imagename(IMSK),
				image->width, image->height, image->stride,
				image->order, image->fmt);
	}

	if (u->dst_rect) {
		rect = u->dst_rect;
		fimg2d_debug("%s rect LT(%d,%d) RB(%d,%d)\n",
				imagename(IDST),
				rect->x1, rect->y1, rect->x2, rect->y2);
	}

	if (u->src_rect) {
		rect = u->src_rect;
		fimg2d_debug("%s rect LT(%d,%d) RB(%d,%d)\n",
				imagename(ISRC),
				rect->x1, rect->y1, rect->x2, rect->y2);
	}

	if (u->msk_rect) {
		rect = u->msk_rect;
		fimg2d_debug("%s rect LT(%d,%d) RB(%d,%d)\n",
				imagename(IMSK),
				rect->x1, rect->y1, rect->x2, rect->y2);
	}
}

void fimg2d_dump_command(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *image;
	struct fimg2d_cache *cache;
	struct fimg2d_rect *rect;

	printk(KERN_INFO " op: %d\n", cmd->op);
	printk(KERN_INFO " solid color: 0x%lx\n", cmd->solid_color);
	printk(KERN_INFO " g_alpha: 0x%x\n", cmd->g_alpha);
	printk(KERN_INFO " premultiplied: %d\n", cmd->premult);
	printk(KERN_INFO " dither: %d\n", cmd->dither);
	printk(KERN_INFO " rotate: %d\n", cmd->rotate);
	printk(KERN_INFO " repeat mode: %d, pad color: 0x%lx\n",
			cmd->repeat.mode, cmd->repeat.pad_color);
	printk(KERN_INFO " bluescreen mode: %d, bs_color: 0x%lx bg_color: 0x%lx\n",
			cmd->bluscr.mode, cmd->bluscr.bs_color, cmd->bluscr.bg_color);

	if (cmd->scaling.mode != NO_SCALING) {
		if (cmd->scaling.factor == SCALING_PERCENTAGE) {
			printk(KERN_INFO " scaling mode: %d, factor: %d, percent(w:%d, h:%d)\n",
					cmd->scaling.mode, cmd->scaling.factor,
					cmd->scaling.scale_w, cmd->scaling.scale_h);
		} else {
			printk(KERN_INFO " scaling mode: %d, factor: %d, pixel(s:%d,%d d:%d,%d)\n",
					cmd->scaling.mode, cmd->scaling.factor,
					cmd->scaling.src_w, cmd->scaling.src_h,
					cmd->scaling.dst_w, cmd->scaling.dst_h);
		}
	}

	if (cmd->dsten) {
		image = &cmd->dst;
		cache = &cmd->dst_cache;
		rect = &cmd->dst_rect;
		printk(KERN_INFO " %s type: %d addr: 0x%lx size: 0x%x cacheable: %d\n",
				imagename(IDST), image->addr.type,
				image->addr.start, image->addr.size, image->addr.cacheable);
		printk(KERN_INFO " %s width: %d height: %d stride: %d order: %d format: %d\n",
				imagename(IDST), image->width, image->height,
				image->stride, image->order, image->fmt);
		printk(KERN_INFO " %s rect LT(%d,%d) RB(%d,%d)\n",
				imagename(IDST), rect->x1, rect->y1, rect->x2, rect->y2);
		printk(KERN_INFO " %s cache addr: 0x%lx size: 0x%x\n",
				imagename(IDST), cache->addr, cache->size);
	}

	if (cmd->srcen) {
		image = &cmd->src;
		cache = &cmd->src_cache;
		rect = &cmd->src_rect;
		printk(KERN_INFO " %s type: %d addr: 0x%lx size: 0x%x cacheable: %d\n",
				imagename(ISRC), image->addr.type,
				image->addr.start, image->addr.size, image->addr.cacheable);
		printk(KERN_INFO " %s width: %d height: %d stride: %d order: %d format: %d\n",
				imagename(ISRC), image->width, image->height,
				image->stride, image->order, image->fmt);
		printk(KERN_INFO " %s rect LT(%d,%d) RB(%d,%d)\n",
				imagename(ISRC), rect->x1, rect->y1, rect->x2, rect->y2);
		printk(KERN_INFO " %s cache addr: 0x%lx size: 0x%x\n",
				imagename(ISRC), cache->addr, cache->size);
	}

	if (cmd->msken) {
		image = &cmd->msk;
		cache = &cmd->msk_cache;
		rect = &cmd->msk_rect;
		printk(KERN_INFO " %s type: %d addr: 0x%lx size: 0x%x cacheable: %d\n",
				imagename(IMSK), image->addr.type,
				image->addr.start, image->addr.size, image->addr.cacheable);
		printk(KERN_INFO " %s width: %d height: %d stride: %d order: %d format: %d\n",
				imagename(IMSK), image->width, image->height,
				image->stride, image->order, image->fmt);
		printk(KERN_INFO " %s rect LT(%d,%d) RB(%d,%d)\n",
				imagename(IMSK), rect->x1, rect->y1, rect->x2, rect->y2);
		printk(KERN_INFO " %s cache addr: 0x%lx size: 0x%x\n",
				imagename(IMSK), cache->addr, cache->size);
	}

	if (cmd->clipping.enable) {
		printk(KERN_INFO " clip rect LT(%d,%d) RB(%d,%d)\n",
				cmd->clipping.x1, cmd->clipping.y1,
				cmd->clipping.x2, cmd->clipping.y2);
	}

	printk(KERN_INFO " cache size all: 0x%x bytes (L1/L2 All:0x%x L1 All:0x%x)\n",
			cmd->size_all, L2_CACHE_SIZE, L1_CACHE_SIZE);
	printk(KERN_INFO " ctx: %p seq_no(%u)\n", cmd->ctx, cmd->seq_no);
}
