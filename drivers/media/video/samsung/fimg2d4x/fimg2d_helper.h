/* linux/drivers/media/video/samsung/fimg2d4x/fimg2d_helper.h
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

#ifndef __FIMG2D_HELPER_H
#define __FIMG2D_HELPER_H

#include "fimg2d.h"

static inline char *imagename(enum image_object image)
{
	switch (image) {
	case IDST:
		return "DST";
	case ISRC:
		return "SRC";
	case IMSK:
		return "MSK";
	default:
		return NULL;
	}
}

static inline long elapsed_usec(struct timeval *start, struct timeval *end)
{
	long sec, usec, time;

	sec = end->tv_sec - start->tv_sec;
	if (end->tv_usec >= start->tv_usec) {
		usec = end->tv_usec - start->tv_usec;
	} else {
		usec = end->tv_usec + 1000000 - start->tv_usec;
		sec--;
	}
	time = sec * 1000000 + usec;

	return time; /* microseconds */
}

static inline void perf_start(struct fimg2d_context *ctx,
				enum perf_desc desc)
{
	struct timeval time;
	struct fimg2d_perf *perf = &ctx->perf[desc];

	if (!(perf->valid & 0x01)) {
		do_gettimeofday(&time);
		perf->start = time;
		perf->valid = 0x01;
	}
}

static inline void perf_end(struct fimg2d_context *ctx,
				enum perf_desc desc)
{
	struct timeval time;
	struct fimg2d_perf *perf = &ctx->perf[desc];

	if (!(perf->valid & 0x10)) {
		do_gettimeofday(&time);
		perf->end = time;
		perf->valid |= 0x10;
	}
}

static inline void perf_clear(struct fimg2d_context *ctx)
{
	int i;
	for (i = 0; i < MAX_PERF_DESCS; i++)
		ctx->perf[i].valid = 0;
}

int point_to_offset(int point, enum color_format cf);
int width_to_bytes(int pixels, enum color_format cf);
void perf_print(struct fimg2d_context *ctx, int seq_no);
void fimg2d_print_params(struct fimg2d_blit __user *u);
void fimg2d_dump_command(struct fimg2d_bltcmd *cmd);

#endif /* __FIMG2D_HELPER_H */
