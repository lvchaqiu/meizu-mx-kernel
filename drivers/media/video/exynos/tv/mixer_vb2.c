/* linux/drivers/media/video/exynos/tv/mixer_vb2.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Videobuf2 allocator operations file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/platform_device.h>
#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif defined(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif

#include "mixer.h"

#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
void *mxr_cma_init(struct mxr_device *mdev)
{
	return vb2_cma_phys_init(mdev->dev, NULL, 0, false);
}

void mxr_cma_resume(void *alloc_ctx){}
void mxr_cma_suspend(void *alloc_ctx){}
void mxr_cma_set_cacheable(void *alloc_ctx, bool cacheable){}

int mxr_cma_cache_flush(struct vb2_buffer *vb, u32 plane_no)
{
	return 0;
}

const struct mxr_vb2 mxr_vb2_cma = {
	.ops		= &vb2_cma_phys_memops,
	.init		= mxr_cma_init,
	.cleanup	= vb2_cma_phys_cleanup,
	.plane_addr	= vb2_cma_phys_plane_paddr,
	.resume		= mxr_cma_resume,
	.suspend	= mxr_cma_suspend,
	.cache_flush	= mxr_cma_cache_flush,
	.set_cacheable	= mxr_cma_set_cacheable,
};
#elif defined(CONFIG_VIDEOBUF2_ION)
void *mxr_ion_init(struct mxr_device *mdev)
{
	struct vb2_ion vb2_ion;
	struct vb2_drv vb2_drv = {0, };
	char ion_name[16] = {0,};

	vb2_ion.dev = mdev->dev;
	memcpy(ion_name, "mxr", sizeof(ion_name));
	vb2_ion.name = ion_name;
	vb2_ion.contig = false;
	vb2_ion.cacheable = false;
	vb2_ion.align = SZ_4K;

	vb2_drv.use_mmu = true;

	return vb2_ion_init(&vb2_ion, &vb2_drv);
}

const struct mxr_vb2 mxr_vb2_ion = {
	.ops		= &vb2_ion_memops,
	.init		= mxr_ion_init,
	.cleanup	= vb2_ion_cleanup,
	.plane_addr	= vb2_ion_plane_dvaddr,
	.resume		= vb2_ion_resume,
	.suspend	= vb2_ion_suspend,
	.cache_flush	= vb2_ion_cache_flush,
	.set_cacheable	= vb2_ion_set_cacheable,
	.set_sharable	= vb2_ion_set_sharable,
};
#endif
