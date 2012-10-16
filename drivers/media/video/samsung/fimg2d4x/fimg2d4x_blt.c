/* linux/drivers/media/video/samsung/fimg2d4x/fimg2d4x_blt.c
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

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <plat/sysmmu.h>
#ifdef CONFIG_PM_RUNTIME
#include <plat/devs.h>
#include <linux/pm_runtime.h>
#endif
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d4x.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

#define BLIT_TIMEOUT	msecs_to_jiffies(1000)

static inline void fimg2d4x_blit_wait(struct fimg2d_control *info, struct fimg2d_bltcmd *cmd)
{
	if (wait_event_timeout(info->wait_q, !atomic_read(&info->busy), BLIT_TIMEOUT)) {
		fimg2d_debug("blitter wake up\n");
	} else {
		printk(KERN_ERR "[%s] bitblt wait timeout\n", __func__);
		fimg2d_dump_command(cmd);

		if (!fimg2d4x_blit_done_status(info))
			info->err = true; /* device error */
	}
}

static void fimg2d4x_pre_bitblt(struct fimg2d_control *info, struct fimg2d_bltcmd *cmd)
{
#ifdef CONFIG_OUTER_CACHE
	struct mm_struct *mm = cmd->ctx->mm;
	struct fimg2d_cache *csrc, *cdst, *cmsk;
	int clip_x, clip_w, clip_h;
	int clip_size, clip_start, y;

	csrc = &cmd->src_cache;
	cdst = &cmd->dst_cache;
	cmsk = &cmd->msk_cache;

#ifdef PERF_PROFILE
	perf_start(cmd->ctx, PERF_L2CC_FLUSH);
#endif
	if (cmd->size_all >= L2_CACHE_SIZE) {
		fimg2d_debug("outercache all\n");
		outer_flush_all();
	} else {
		fimg2d_debug("outercache range\n");
		if (cmd->srcen) {
			if (cmd->src.addr.type == ADDR_USER)
				fimg2d_clean_outer_pagetable(mm, csrc->addr, csrc->size);
			if (cmd->src.addr.cacheable) {
				clip_x = point_to_offset(cmd->src_rect.x1, cmd->src.fmt);
				clip_w = width_to_bytes(cmd->src_rect.x2 - cmd->src_rect.x1, cmd->src.fmt);
				clip_h = cmd->src_rect.y2 - cmd->src_rect.y1;
				clip_size = clip_w * clip_h;
				if ((clip_size * 100 > csrc->size * 80) || (csrc->size < SZ_32K)) {
					fimg2d_dma_sync_outer(mm, csrc->addr, csrc->size, CACHE_CLEAN);
				} else {
					for (y = 0; y < clip_h; y++) {
						clip_start = csrc->addr + (cmd->src.stride * y) + clip_x;
						fimg2d_dma_sync_outer(mm, clip_start, clip_w, CACHE_CLEAN);
					}
				}
			}
		}

		if (cmd->msken) {
			if (cmd->msk.addr.type == ADDR_USER)
				fimg2d_clean_outer_pagetable(mm, cmsk->addr, cmsk->size);
			if (cmd->msk.addr.cacheable) {
				clip_x = point_to_offset(cmd->msk_rect.x1, cmd->msk.fmt);
				clip_w = width_to_bytes(cmd->msk_rect.x2 - cmd->msk_rect.x1, cmd->msk.fmt);
				clip_h = cmd->msk_rect.y2 - cmd->msk_rect.y1;
				clip_size = clip_w * clip_h;
				if ((clip_size * 100 > cmsk->size * 80) || (cmsk->size < SZ_32K)) {
					fimg2d_dma_sync_outer(mm, cmsk->addr, cmsk->size, CACHE_CLEAN);
				} else {
					for (y = 0; y < clip_h; y++) {
						clip_start = cmsk->addr + (cmd->msk.stride * y) + clip_x;
						fimg2d_dma_sync_outer(mm, clip_start, clip_w, CACHE_CLEAN);
					}
				}
			}
		}

		if (cmd->dsten) {
			if (cmd->dst.addr.type == ADDR_USER)
				fimg2d_clean_outer_pagetable(mm, cdst->addr, cdst->size);
			if (cmd->dst.addr.cacheable) {
				clip_x = point_to_offset(cmd->dst_rect.x1, cmd->dst.fmt);
				clip_w = width_to_bytes(cmd->dst_rect.x2 - cmd->dst_rect.x1, cmd->dst.fmt);
				clip_h = cmd->dst_rect.y2 - cmd->dst_rect.y1;
				clip_size = clip_w * clip_h;
				if ((clip_size * 100 > cdst->size * 80) || (cdst->size < SZ_32K)) {
					fimg2d_dma_sync_outer(mm, cdst->addr, cdst->size, CACHE_FLUSH);
				} else {
					for (y = 0; y < clip_h; y++) {
						clip_start = cdst->addr + (cmd->dst.stride * y) + clip_x;
						fimg2d_dma_sync_outer(mm, clip_start, clip_w, CACHE_FLUSH);
					}
				}
			}
		}
	}
#ifdef PERF_PROFILE
	perf_end(cmd->ctx, PERF_L2CC_FLUSH);
#endif

#endif
}

extern int exynos_sysmmu_enable(struct device *owner, unsigned long pgtable);
extern void exynos_sysmmu_disable(struct device *owner);
void fimg2d4x_bitblt(struct fimg2d_control *info)
{
	struct fimg2d_context *ctx;
	struct fimg2d_bltcmd *cmd;
	unsigned long *pgd;

	fimg2d_debug("enter blitter\n");

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(info->dev);
	fimg2d_debug("pm_runtime_get_sync\n");
#endif
	fimg2d_clk_on(info);

	while ((cmd = fimg2d_get_first_command(info))) {
		ctx = cmd->ctx;
#ifdef PERF_PROFILE
		perf_end(ctx, PERF_WORKQUE);
#endif
		if (info->err) {
			printk(KERN_ERR "[%s] device error\n", __func__);
			goto blitend;
		}

		atomic_set(&info->busy, 1);
#ifdef PERF_PROFILE
		perf_start(cmd->ctx, PERF_SFR);
#endif
		info->configure(info, cmd);
#ifdef PERF_PROFILE
		perf_end(cmd->ctx, PERF_SFR);
#endif
		if (cmd->dst.addr.type != ADDR_PHYS) {
			pgd = (unsigned long *)ctx->mm->pgd;
			exynos_sysmmu_enable(info->dev, (unsigned long)virt_to_phys(pgd));
			fimg2d_debug("sysmmu enable: pgd %p ctx %p seq_no(%u)\n",
					pgd, ctx, cmd->seq_no);
		}

		fimg2d4x_pre_bitblt(info, cmd);

#ifdef PERF_PROFILE
		perf_start(cmd->ctx, PERF_BLIT);
#endif

#ifdef CONFIG_BUSFREQ_OPP
		pr_debug("%s, dev_lock\n", __func__);
		/* lock bus frequency */
		dev_lock(info->bus_dev, info->dev, 160160);
#endif

		/* start bitblt */
		info->run(info);

		fimg2d4x_blit_wait(info, cmd);

#ifdef CONFIG_BUSFREQ_OPP
		pr_debug("%s, dev_unlock\n", __func__);
		/* unlock bus frequency */
		dev_unlock(info->bus_dev, info->dev);
#endif

#ifdef PERF_PROFILE
		perf_end(cmd->ctx, PERF_BLIT);
#endif
		if (cmd->dst.addr.type != ADDR_PHYS) {
			exynos_sysmmu_disable(info->dev);
			fimg2d_debug("sysmmu disable\n");
		}
blitend:
		spin_lock(&info->bltlock);
		fimg2d_dequeue(&cmd->node);
		kfree(cmd);
		atomic_dec(&ctx->ncmd);

		/* wake up context */
		if (!atomic_read(&ctx->ncmd)) {
			fimg2d_debug("no more blit jobs for ctx %p\n", ctx);
			wake_up(&ctx->wait_q);
		}
		spin_unlock(&info->bltlock);
	}

	atomic_set(&info->active, 0);

	fimg2d_clk_off(info);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_sync(info->dev);
	fimg2d_debug("pm_runtime_put_sync\n");
#endif

	fimg2d_debug("exit blitter\n");
}

static void fimg2d4x_configure(struct fimg2d_control *info, struct fimg2d_bltcmd *cmd)
{
	enum image_sel srcsel, dstsel;

	fimg2d_debug("ctx %p seq_no(%u)\n", cmd->ctx, cmd->seq_no);

	/* TODO: batch blit */
	fimg2d4x_reset(info);

	/* src and dst select */
	srcsel = dstsel = IMG_MEMORY;

	switch (cmd->op) {
	case BLIT_OP_SOLID_FILL:
		srcsel = dstsel = IMG_FGCOLOR;
		fimg2d4x_set_color_fill(info, cmd->solid_color);
		break;
	case BLIT_OP_CLR:
		srcsel = dstsel = IMG_FGCOLOR;
		fimg2d4x_set_color_fill(info, 0);
		break;
	case BLIT_OP_DST:
		srcsel = IMG_FGCOLOR;
		break;
	default:
		if (cmd->op == BLIT_OP_SRC)
			dstsel = IMG_FGCOLOR;
		if (!cmd->srcen) {
			srcsel = IMG_FGCOLOR;
			fimg2d4x_set_fgcolor(info, cmd->solid_color);
		}
		fimg2d4x_enable_alpha(info, cmd->g_alpha);
		fimg2d4x_set_alpha_composite(info, cmd->op, cmd->g_alpha);
		if (cmd->premult == NON_PREMULTIPLIED)
			fimg2d4x_set_premultiplied(info);
		break;
	}

	fimg2d4x_set_src_type(info, srcsel);
	fimg2d4x_set_dst_type(info, dstsel);

	/* src */
	if (cmd->srcen) {
		fimg2d4x_set_src_image(info, &cmd->src);
		fimg2d4x_set_src_rect(info, &cmd->src_rect);
		fimg2d4x_set_src_repeat(info, &cmd->repeat);
		if (cmd->scaling.mode != NO_SCALING)
			fimg2d4x_set_src_scaling(info, &cmd->scaling);
	}

	/* mask */
	if (cmd->msken) {
		fimg2d4x_enable_msk(info);
		fimg2d4x_set_msk_image(info, &cmd->msk);
		fimg2d4x_set_msk_rect(info, &cmd->msk_rect);
		fimg2d4x_set_msk_repeat(info, &cmd->repeat);
		if (cmd->scaling.mode != NO_SCALING)
			fimg2d4x_set_msk_scaling(info, &cmd->scaling);
	}

	/* dst */
	if (cmd->dsten) {
		fimg2d4x_set_dst_image(info, &cmd->dst);
		fimg2d4x_set_dst_rect(info, &cmd->dst_rect);
	}

	/* bluescreen */
	if (cmd->bluscr.mode != OPAQUE)
		fimg2d4x_set_bluescreen(info, &cmd->bluscr);

	/* rotation */
	if (cmd->rotate != ORIGIN)
		fimg2d4x_set_rotation(info, cmd->rotate);

	/* clipping */
	if (cmd->clipping.enable)
		fimg2d4x_enable_clipping(info, &cmd->clipping);

	/* dithering */
	if (cmd->dither)
		fimg2d4x_enable_dithering(info);
}

static void fimg2d4x_run(struct fimg2d_control *info)
{
	fimg2d_debug("start bitblt\n");
	fimg2d4x_enable_irq(info);
	fimg2d4x_clear_irq(info);
	fimg2d4x_start_blit(info);
}

static void fimg2d4x_stop(struct fimg2d_control *info)
{
	if (fimg2d4x_is_blit_done(info)) {
		fimg2d_debug("bitblt done\n");
		fimg2d4x_disable_irq(info);
		fimg2d4x_clear_irq(info);
		atomic_set(&info->busy, 0);
		wake_up(&info->wait_q);
	}
}

static void fimg2d4x_dump(struct fimg2d_control *info)
{
	fimg2d4x_dump_regs(info);
}

int fimg2d_register_ops(struct fimg2d_control *info)
{
	info->blit = fimg2d4x_bitblt;
	info->configure = fimg2d4x_configure;
	info->run = fimg2d4x_run;
	info->dump = fimg2d4x_dump;
	info->stop = fimg2d4x_stop;

	return 0;
}
