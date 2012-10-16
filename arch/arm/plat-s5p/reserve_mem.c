/* linux/arch/arm/plat-s5p/reserve_mem.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Reserve mem helper functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <asm/setup.h>
#include <linux/io.h>
#include <mach/memory.h>
#include <plat/media.h>
#include <mach/media.h>

extern struct s5p_media_device media_devs[];
extern int nr_media_devs;

static dma_addr_t media_base[NR_BANKS];

static struct s5p_media_device *s5p_get_media_device(int dev_id, int bank)
{
	struct s5p_media_device *mdev = NULL;
	int i = 0, found = 0;

	if (dev_id < 0)
		return NULL;

	while (!found && (i < nr_media_devs)) {
		mdev = &media_devs[i];
		if (mdev->id == dev_id && mdev->bank == bank)
			found = 1;
		else
			i++;
	}

	if (!found)
		mdev = NULL;

	return mdev;
}

dma_addr_t s5p_get_media_memory_bank(int dev_id, int bank)
{
	struct s5p_media_device *mdev;

	mdev = s5p_get_media_device(dev_id, bank);
	if (!mdev) {
		printk(KERN_ERR "invalid media device\n");
		return 0;
	}

	if (!mdev->paddr) {
		printk(KERN_ERR "no memory for %s\n", mdev->name);
		return 0;
	}

	return mdev->paddr;
}
EXPORT_SYMBOL(s5p_get_media_memory_bank);

size_t s5p_get_media_memsize_bank(int dev_id, int bank)
{
	struct s5p_media_device *mdev;

	mdev = s5p_get_media_device(dev_id, bank);
	if (!mdev) {
		printk(KERN_ERR "invalid media device\n");
		return 0;
	}

	return mdev->memsize;
}
EXPORT_SYMBOL(s5p_get_media_memsize_bank);

dma_addr_t s5p_get_media_membase_bank(int bank)
{
	if (bank > meminfo.nr_banks) {
		printk(KERN_ERR "invalid bank.\n");
		return -EINVAL;
	}

	return media_base[bank];
}
EXPORT_SYMBOL(s5p_get_media_membase_bank);

void s5p_reserve_mem(size_t boundary)
{
	struct s5p_media_device *mdev;
	u64 start, end;
	int i, ret;

	for (i = 0; i < meminfo.nr_banks; i++)
		media_base[i] = meminfo.bank[i].start + meminfo.bank[i].size;

	for (i = 0; i < nr_media_devs; i++) {
		mdev = &media_devs[i];
		if (mdev->memsize <= 0)
			continue;

		if (mdev->bank > meminfo.nr_banks) {
			pr_err("mdev %s: mdev->bank(%d), max_bank(%d)\n",
				mdev->name, mdev->bank, meminfo.nr_banks);
			return;
		}

		if (!mdev->paddr) {
			start = meminfo.bank[mdev->bank].start;
			end = start + meminfo.bank[mdev->bank].size;

			if (boundary && (boundary < end - start))
				start = end - boundary;

			mdev->paddr = memblock_find_in_range(start, end,
						mdev->memsize, PAGE_SIZE);
		}

		ret = memblock_remove(mdev->paddr, mdev->memsize);
		if (ret < 0)
			pr_err("memblock_reserve(%x, %x) failed\n",
				mdev->paddr, mdev->memsize);

		if (media_base[mdev->bank] > mdev->paddr)
			media_base[mdev->bank] = mdev->paddr;

		printk(KERN_INFO "s5p: %lu bytes system memory reserved "
			"for %s at 0x%08x, %d-bank base(0x%08x)\n",
			(unsigned long) mdev->memsize, mdev->name, mdev->paddr,
			mdev->bank, media_base[mdev->bank]);
	}
}
