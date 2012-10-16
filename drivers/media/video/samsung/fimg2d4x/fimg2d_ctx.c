/* linux/drivers/media/video/samsung/fimg2d4x/fimg2d_ctx.c
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
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <plat/fimg2d.h>
#include "fimg2d.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

static int fimg2d_check_params(struct fimg2d_blit __user *u)
{
	int w, h;
	struct fimg2d_rect *r;
	struct fimg2d_clip *c;

	/* DST op makes no effect */
	if (u->op < 0 || u->op == BLIT_OP_DST || u->op >= BLIT_OP_END) {
		printk(KERN_ERR "%s: invalid op\n", __func__);
		return -1;
	}

	if (u->src) {
		w = u->src->width;
		h = u->src->height;
		r = u->src_rect;

		if (!r) {
			printk(KERN_ERR "%s: missing src rect\n", __func__);
			return -1;
		}

		/* 8000: max width & height */
		if (w > 8000 || h > 8000 ||
			r->x1 < 0 || r->x2 > w ||
			r->y1 < 0 || r->y2 > h ||
			r->x1 == r->x2 || r->y1 == r->y2) {
			printk(KERN_ERR "%s: invalid src rect LT(%d,%d) RB(%d,%d)\n",
					__func__, r->x1, r->y1, r->x2, r->y2);
			return -1;
		}
	}

	if (u->msk) {
		w = u->msk->width;
		h = u->msk->height;
		r = u->msk_rect;

		if (!r) {
			printk(KERN_ERR "%s: missing msk rect\n", __func__);
			return -1;
		}

		if (w > 8000 || h > 8000 ||
			r->x1 < 0 || r->x2 > w ||
			r->y1 < 0 || r->y2 > h ||
			r->x1 == r->x2 || r->y1 == r->y2) {
			printk(KERN_ERR "%s: invalid msk rect, LT(%d,%d) RB(%d,%d)\n",
					__func__, r->x1, r->y1, r->x2, r->y2);
			return -1;
		}
	}

	if (u->dst) {
		w = u->dst->width;
		h = u->dst->height;
		r = u->dst_rect;

		if (!r) {
			printk(KERN_ERR "%s: missing dst rect\n", __func__);
			return -1;
		}

		if (w > 8000 || h > 8000 ||
			r->x1 < 0 || r->x1 >= w ||
			r->y1 < 0 || r->y1 >= h ||
			r->x1 == r->x2 || r->y1 == r->y2) {
			printk(KERN_ERR "%s: invalid dst rect, LT(%d,%d) RB(%d,%d)\n",
					__func__, r->x1, r->y1, r->x2, r->y2);
			return -1;
		}

		/* out of dst_rect */
		if (u->clipping && u->clipping->enable) {
			c = u->clipping;
			if (c->x1 >= r->x2 || c->x2 <= r->x1 ||
				c->y1 >= r->y2 || c->y2 <= r->y1) {
				printk(KERN_ERR "%s: invalid clip rect, LT(%d,%d) RB(%d,%d)\n",
						__func__, c->x1, c->y1, c->x2, c->y2);
				return -1;
			}
		}
	} else {
		return -EINVAL;   /*accroding to fimg2d_blit, dst must be set, or it will cause sysmmu crash*/
	}

	return 0;
}

static void fimg2d_fixup_params(struct fimg2d_bltcmd *cmd)
{
	/* fix up scaling */
	if (cmd->scaling.mode) {
		if (cmd->scaling.factor == SCALING_PERCENTAGE) {
			if ((!cmd->scaling.scale_w && !cmd->scaling.scale_h) ||
				(cmd->scaling.scale_w == 100 && cmd->scaling.scale_h == 100)) {
				cmd->scaling.mode = NO_SCALING;
			}
		} else if (cmd->scaling.factor == SCALING_PIXELS) {
			if ((cmd->scaling.src_w == cmd->scaling.dst_w) &&
				(cmd->scaling.src_h == cmd->scaling.dst_h)) {
				cmd->scaling.mode = NO_SCALING;
			}
		}
	}

	/* fix up dst rect */
	if (cmd->dst_rect.x2 > cmd->dst.width) {
		fimg2d_debug("fixing up dst coord x2: %d --> %d\n",
				cmd->dst_rect.x2, cmd->dst.width);
		cmd->dst_rect.x2 = cmd->dst.width;
	}
	if (cmd->dst_rect.y2 > cmd->dst.height) {
		fimg2d_debug("fixing up dst coord y2: %d --> %d\n",
				cmd->dst_rect.y2, cmd->dst.height);
		cmd->dst_rect.y2 = cmd->dst.height;
	}

	/* fix up clip rect */
	if (cmd->clipping.enable) {
		/* fit to smaller dst region  as a clip rect */
		if (cmd->clipping.x1 < cmd->dst_rect.x1) {
			fimg2d_debug("fixing up cipping coord x1: %d --> %d\n",
					cmd->clipping.x1, cmd->dst_rect.x1);
			cmd->clipping.x1 = cmd->dst_rect.x1;
		}
		if (cmd->clipping.y1 < cmd->dst_rect.y1) {
			fimg2d_debug("fixing up cipping coord y1: %d --> %d\n",
					cmd->clipping.y1, cmd->dst_rect.y1);
			cmd->clipping.y1 = cmd->dst_rect.y1;
		}
		if (cmd->clipping.x2 > cmd->dst_rect.x2) {
			fimg2d_debug("fixing up cipping coord x2: %d --> %d\n",
					cmd->clipping.x2, cmd->dst_rect.x2);
			cmd->clipping.x2 = cmd->dst_rect.x2;
		}
		if (cmd->clipping.y2 > cmd->dst_rect.y2) {
			fimg2d_debug("fixing up cipping coord y2: %d --> %d\n",
					cmd->clipping.y2, cmd->dst_rect.y2);
			cmd->clipping.y2 = cmd->dst_rect.y2;
		}
	}
}

static int fimg2d_check_dma_sync(struct fimg2d_bltcmd *cmd)
{
	struct mm_struct *mm = cmd->ctx->mm;
	struct fimg2d_cache *csrc, *cdst, *cmsk;
	enum pt_status pt;

	csrc = &cmd->src_cache;
	cdst = &cmd->dst_cache;
	cmsk = &cmd->msk_cache;

	if (cmd->srcen) {
		csrc->addr = cmd->src.addr.start +
				(cmd->src.stride * cmd->src_rect.y1);
		csrc->size = cmd->src.stride *
				(cmd->src_rect.y2 - cmd->src_rect.y1);

		if (cmd->src.addr.cacheable)
			cmd->size_all += csrc->size;

		if (cmd->src.addr.type == ADDR_USER) {
			pt = fimg2d_check_pagetable(mm, csrc->addr, csrc->size);
			if (pt == PT_FAULT)
				return -1;
		}
	}

	if (cmd->msken) {
		cmsk->addr = cmd->msk.addr.start +
				(cmd->msk.stride * cmd->msk_rect.y1);
		cmsk->size = cmd->msk.stride *
				(cmd->msk_rect.y2 - cmd->msk_rect.y1);

		if (cmd->msk.addr.cacheable)
			cmd->size_all += cmsk->size;

		if (cmd->msk.addr.type == ADDR_USER) {
			pt = fimg2d_check_pagetable(mm, cmsk->addr, cmsk->size);
			if (pt == PT_FAULT)
				return -1;
		}
	}

	/* caculate horizontally clipped region */
	if (cmd->dsten) {
		if (cmd->clipping.enable) {
			cdst->addr = cmd->dst.addr.start +
					(cmd->dst.stride * cmd->clipping.y1);
			cdst->size = cmd->dst.stride *
					(cmd->clipping.y2 - cmd->clipping.y1);
		} else {
			cdst->addr = cmd->dst.addr.start +
					(cmd->dst.stride * cmd->dst_rect.y1);
			cdst->size = cmd->dst.stride *
					(cmd->dst_rect.y2 - cmd->dst_rect.y1);
		}

		if (cmd->dst.addr.cacheable)
			cmd->size_all += cdst->size;

		if (cmd->dst.addr.type == ADDR_USER) {
			pt = fimg2d_check_pagetable(mm, cdst->addr, cdst->size);
			if (pt == PT_FAULT)
				return -1;
		}
	}

	fimg2d_debug("cached size all = %d\n", cmd->size_all);

#ifdef PERF_PROFILE
	perf_start(cmd->ctx, PERF_L1CC_FLUSH);
#endif
	if (cmd->size_all >= L1_CACHE_SIZE) {
		fimg2d_debug("innercache all\n");
		flush_all_cpu_caches();
	} else {
		fimg2d_debug("innercache range\n");
		if (cmd->srcen && cmd->src.addr.cacheable)
			fimg2d_dma_sync_inner(csrc->addr, csrc->size, DMA_TO_DEVICE);

		if (cmd->msken && cmd->msk.addr.cacheable)
			fimg2d_dma_sync_inner(cmsk->addr, cmsk->size, DMA_TO_DEVICE);

		if (cmd->dsten && cmd->dst.addr.cacheable)
			fimg2d_dma_sync_inner(cdst->addr, cdst->size, DMA_BIDIRECTIONAL);
	}
#ifdef PERF_PROFILE
	perf_end(cmd->ctx, PERF_L1CC_FLUSH);
#endif

	return 0;
}

int fimg2d_add_command(struct fimg2d_control *info, struct fimg2d_context *ctx,
			struct fimg2d_blit __user *u)
{
	struct fimg2d_bltcmd *cmd;

#ifdef CONFIG_VIDEO_FIMG2D_DEBUG
	fimg2d_print_params(u);
#endif

	if (info->err) {
		printk(KERN_ERR "[%s] device error, do sw fallback\n", __func__);
		return -EFAULT;
	}

	if (fimg2d_check_params(u))
		return -EINVAL;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		printk(KERN_ERR "[%s] failed to create bitblt command\n", __func__);
		return -ENOMEM;
	}

	cmd->ctx = ctx;
	cmd->seq_no = u->seq_no;

	cmd->op = u->op;
	cmd->premult = u->premult;
	cmd->g_alpha = u->g_alpha;
	cmd->dither = u->dither;
	cmd->rotate = u->rotate;
	cmd->solid_color = u->solid_color;

	if (u->scaling && u->scaling->mode) {
		if (copy_from_user(&cmd->scaling, u->scaling, sizeof(cmd->scaling)))
			goto err_user;
	}

	if (u->repeat && u->repeat->mode) {
		if (copy_from_user(&cmd->repeat, u->repeat, sizeof(cmd->repeat)))
			goto err_user;
	}

	if (u->bluscr && u->bluscr->mode) {
		if (copy_from_user(&cmd->bluscr, u->bluscr, sizeof(cmd->bluscr)))
			goto err_user;
	}

	if (u->clipping && u->clipping->enable) {
		if (copy_from_user(&cmd->clipping, u->clipping, sizeof(cmd->clipping)))
			goto err_user;
	}

	if (u->src) {
		cmd->srcen = true;
		if (copy_from_user(&cmd->src, u->src, sizeof(cmd->src)))
			goto err_user;
	}

	if (u->dst) {
		cmd->dsten = true;
		if (copy_from_user(&cmd->dst, u->dst, sizeof(cmd->dst)))
			goto err_user;
	}

	if (u->msk) {
		cmd->msken = true;
		if (copy_from_user(&cmd->msk, u->msk, sizeof(cmd->msk)))
			goto err_user;
	}

	if (u->src_rect) {
		if (copy_from_user(&cmd->src_rect, u->src_rect, sizeof(cmd->src_rect)))
			goto err_user;
	}

	if (u->dst_rect) {
		if (copy_from_user(&cmd->dst_rect, u->dst_rect, sizeof(cmd->dst_rect)))
			goto err_user;
	}

	if (u->msk_rect) {
		if (copy_from_user(&cmd->msk_rect, u->msk_rect, sizeof(cmd->msk_rect)))
			goto err_user;
	}

	fimg2d_fixup_params(cmd);

	if (fimg2d_check_dma_sync(cmd))
		goto err_user;

	/* add command node and increase ncmd */
	spin_lock(&info->bltlock);
	if (atomic_read(&info->suspended)) {
		fimg2d_debug("fimg2d suspended, do sw fallback\n");
		spin_unlock(&info->bltlock);
		goto err_user;
	}
	atomic_inc(&ctx->ncmd);
	fimg2d_enqueue(&cmd->node, &info->cmd_q);
	fimg2d_debug("ctx %p pgd %p ncmd(%d) seq_no(%u)\n",
			cmd->ctx,
			(unsigned long *)cmd->ctx->mm->pgd,
			atomic_read(&ctx->ncmd), cmd->seq_no);
	spin_unlock(&info->bltlock);

	return 0;

err_user:
	kfree(cmd);
	return -EFAULT;
}

void fimg2d_add_context(struct fimg2d_control *info, struct fimg2d_context *ctx)
{
	atomic_set(&ctx->ncmd, 0);
	init_waitqueue_head(&ctx->wait_q);

	atomic_inc(&info->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&info->nctx));
}

void fimg2d_del_context(struct fimg2d_control *info, struct fimg2d_context *ctx)
{
	atomic_dec(&info->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&info->nctx));
}
