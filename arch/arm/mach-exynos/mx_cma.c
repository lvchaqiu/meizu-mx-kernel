/*
 * mx_cma.c - cma initial code for mx board
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  lvcha qiu   <lvcha@meizu.com>
 *
 * This program is not provided / owned by Maxim Integrated Products.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#include <linux/memblock.h>
#include <linux/cma.h>

#include <asm/mach-types.h>

#if defined(CONFIG_S5P_MEM_CMA)
static void __init mx_cma_region_reserve(
			struct cma_region *regions_normal,
			struct cma_region *regions_secure)
{
	struct cma_region *reg;
	phys_addr_t paddr_last = 0xFFFFFFFF;

	for (reg = regions_normal; reg->size != 0; reg++) {
		phys_addr_t paddr;

		if (!IS_ALIGNED(reg->size, PAGE_SIZE)) {
			pr_err("S5P/CMA: size of '%s' is NOT page-aligned\n",
								reg->name);
			reg->size = PAGE_ALIGN(reg->size);
		}

		if (reg->reserved) {
			pr_err("S5P/CMA: '%s' alread reserved\n", reg->name);
			continue;
		}

		if (reg->alignment) {
			if ((reg->alignment & ~PAGE_MASK) ||
				(reg->alignment & ~reg->alignment)) {
				pr_err("S5P/CMA: Failed to reserve '%s': "
						"incorrect alignment 0x%08x.\n",
						reg->name, reg->alignment);
				continue;
			}
		} else {
			reg->alignment = PAGE_SIZE;
		}

		if (reg->start) {
			if (!memblock_is_region_reserved(reg->start, reg->size)
			    && (memblock_reserve(reg->start, reg->size) == 0))
				reg->reserved = 1;
			else
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
								reg->name);
			continue;
		}

		paddr = memblock_find_in_range(0, MEMBLOCK_ALLOC_ACCESSIBLE,
						reg->size, reg->alignment);
		if (paddr != MEMBLOCK_ERROR) {
			if (memblock_reserve(paddr, reg->size)) {
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
								reg->name);
				continue;
			}
			reg->start = paddr;
			reg->reserved = 1;
			pr_info("name = %s, paddr = 0x%x, size = %d\n", reg->name, paddr, reg->size);
		} else {
			pr_err("S5P/CMA: No free space in memory for '%s'\n",
								reg->name);
		}

		if (cma_early_region_register(reg)) {
			pr_err("S5P/CMA: Failed to register '%s'\n",
								reg->name);
			memblock_free(reg->start, reg->size);
		} else {
			paddr_last = min(paddr, paddr_last);
		}
	}

	if (regions_secure && regions_secure->size) {
		size_t size_secure = 0;
		size_t align_secure, size_region2, aug_size, order_region2;

		for (reg = regions_secure; reg->size != 0; reg++)
			size_secure += reg->size;

		reg--;

		/* Entire secure regions will be merged into 2
		 * consecutive regions. */
		align_secure = 1 <<
			(get_order((size_secure + 1) / 2) + PAGE_SHIFT);
		/* Calculation of a subregion size */
		size_region2 = size_secure - align_secure;
		order_region2 = get_order(size_region2) + PAGE_SHIFT;
		if (order_region2 < 20)
			order_region2 = 20; /* 1MB */
		order_region2 -= 3; /* divide by 8 */
		size_region2 = ALIGN(size_region2, 1 << order_region2);

		aug_size = align_secure + size_region2 - size_secure;
		if (aug_size > 0)
			reg->size += aug_size;

		size_secure = ALIGN(size_secure, align_secure);

		if (paddr_last >= memblock.current_limit) {
			paddr_last = memblock_find_in_range(0,
					MEMBLOCK_ALLOC_ACCESSIBLE,
					size_secure, reg->alignment);
		} else {
			paddr_last -= size_secure;
			paddr_last = round_down(paddr_last, align_secure);
		}

		if (paddr_last) {
			while (memblock_reserve(paddr_last, size_secure))
				paddr_last -= align_secure;

			do {
				reg->start = paddr_last;
				reg->reserved = 1;
				paddr_last += reg->size;

				if (cma_early_region_register(reg)) {
					memblock_free(reg->start, reg->size);
					pr_err("S5P/CMA: "
					"Failed to register secure region "
					"'%s'\n", reg->name);
				} else {
					size_secure -= reg->size;
				}
			} while (reg-- != regions_secure);

			if (size_secure > 0)
				memblock_free(paddr_last, size_secure);
		} else {
			pr_err("S5P/CMA: Failed to reserve secure regions\n");
		}
	}
}

static struct cma_region mx_regions[] = {
	{
		.name = "jpeg",
		.size = 1024 * SZ_1K,
		.start = 0
	}, {
		.name = "fimc1",
		.size = 32768 * SZ_1K,
		.start = 0
	}, {
		.name = "fimc0",
		.size = 25600 * SZ_1K,
		.start = 0
	},  {
		.name = "fimc3",
		.size = 4 * SZ_1K,
		.start = 0
	}, {
		.name = "mfc0",
		.size = 49152 * SZ_1K,
		{ .alignment = 1 << 17 },
	}, {
		.name = "mfc1",
		.size = 24576 * SZ_1K,
		{ .alignment = 1 << 17 },
	}, {
		.name = "fimd",
		.size = 9600 * SZ_1K,
		.start = 0
	}, {
		.name = "ram_console",
		.size = 128 * SZ_1K,
		.start = 0,
	}, {
		.size = 0
	},
};

void __init mx_reserve_mem(void)
{
	static const char map[] __initconst =
		"ram_console=ram_console;"
		"android_pmem.0=pmem;android_pmem.1=pmem_gpu1;"
		"s3cfb.0/fimd=fimd;exynos4-fb.0/fimd=fimd;"
		"s3c-fimc.0=fimc0;s3c-fimc.1=fimc1;s3c-fimc.2=fimc2;s3c-fimc.3=fimc3;"
		"exynos4210-fimc.0=fimc0;exynos4210-fimc.1=fimc1;exynos4210-fimc.2=fimc2;exynos4210-fimc.3=fimc3;"
		"s3c-mfc/A=mfc0,mfc-secure;"
		"s3c-mfc/B=mfc1,mfc-normal;"
		"s3c-mfc/AB=mfc;"
		"samsung-rp=srp;"
		"s5p-jpeg=jpeg;"
		"jpeg_v1=jpeg;"
		"jpeg_v2=jpeg;"
		"exynos4-fimc-is/f=fimc_is;"
		"s5p-mixer=tv;"
		"s5p-fimg2d=fimg2d;"
		"ion-exynos=ion,fimd,fimc0,fimc1,fimc2,fimc3,fw,b1,b2;"
		"s5p-smem/mfc=mfc0,mfc-secure;"
		"s5p-smem/fimc=fimc3;"
		"s5p-smem/mfc-shm=mfc1,mfc-normal;";

	cma_set_defaults(NULL, map);

	mx_cma_region_reserve(mx_regions, NULL);
}
#endif
