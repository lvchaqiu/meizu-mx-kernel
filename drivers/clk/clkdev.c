/*
 * drivers/clk/clkdev.c
 *
 *  Copyright (C) 2008 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Helper for the clk API to assist looking up a struct clk.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

#if defined(CONFIG_PM_DEBUG) \
	&& defined(CONFIG_DEBUG_FS) \
	&& defined(CONFIG_ARCH_EXYNOS)
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <plat/clock.h>
#endif

static LIST_HEAD(clocks);
static DEFINE_MUTEX(clocks_mutex);

#if defined(CONFIG_PM_DEBUG) \
	&& defined(CONFIG_DEBUG_FS) \
	&& defined(CONFIG_ARCH_EXYNOS)
static DEFINE_SPINLOCK(clocks_lock);
#endif
/*
 * Find the correct struct clk for the device and connection ID.
 * We do slightly fuzzy matching here:
 *  An entry with a NULL ID is assumed to be a wildcard.
 *  If an entry has a device ID, it must match
 *  If an entry has a connection ID, it must match
 * Then we take the most specific entry - with the following
 * order of precedence: dev+con > dev only > con only.
 */
static struct clk_lookup *clk_find(const char *dev_id, const char *con_id)
{
	struct clk_lookup *p, *cl = NULL;
	int match, best = 0;

	list_for_each_entry(p, &clocks, node) {
		match = 0;
		if (p->dev_id) {
			if (!dev_id || strcmp(p->dev_id, dev_id))
				continue;
			match += 2;
		}
		if (p->con_id) {
			if (!con_id || strcmp(p->con_id, con_id))
				continue;
			match += 1;
		}

		if (match > best) {
			cl = p;
			if (match != 3)
				best = match;
			else
				break;
		}
	}
	return cl;
}

struct clk *clk_get_sys(const char *dev_id, const char *con_id)
{
	struct clk_lookup *cl;

	mutex_lock(&clocks_mutex);
	cl = clk_find(dev_id, con_id);
	if (cl && !__clk_get(cl->clk))
		cl = NULL;
	mutex_unlock(&clocks_mutex);

	return cl ? cl->clk : ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL(clk_get_sys);

struct clk *clk_get(struct device *dev, const char *con_id)
{
	const char *dev_id = dev ? dev_name(dev) : NULL;

	return clk_get_sys(dev_id, con_id);
}
EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
	__clk_put(clk);
}
EXPORT_SYMBOL(clk_put);

void clkdev_add(struct clk_lookup *cl)
{
	mutex_lock(&clocks_mutex);
	list_add_tail(&cl->node, &clocks);
	mutex_unlock(&clocks_mutex);
}
EXPORT_SYMBOL(clkdev_add);

void __init clkdev_add_table(struct clk_lookup *cl, size_t num)
{
	mutex_lock(&clocks_mutex);
	while (num--) {
		list_add_tail(&cl->node, &clocks);
		cl++;
	}
	mutex_unlock(&clocks_mutex);
}

#define MAX_DEV_ID	20
#define MAX_CON_ID	16

struct clk_lookup_alloc {
	struct clk_lookup cl;
	char	dev_id[MAX_DEV_ID];
	char	con_id[MAX_CON_ID];
};

struct clk_lookup * __init_refok
clkdev_alloc(struct clk *clk, const char *con_id, const char *dev_fmt, ...)
{
	struct clk_lookup_alloc *cla;

	cla = __clkdev_alloc(sizeof(*cla));
	if (!cla)
		return NULL;

	cla->cl.clk = clk;
	if (con_id) {
		strlcpy(cla->con_id, con_id, sizeof(cla->con_id));
		cla->cl.con_id = cla->con_id;
	}

	if (dev_fmt) {
		va_list ap;

		va_start(ap, dev_fmt);
		vscnprintf(cla->dev_id, sizeof(cla->dev_id), dev_fmt, ap);
		cla->cl.dev_id = cla->dev_id;
		va_end(ap);
	}

	return &cla->cl;
}
EXPORT_SYMBOL(clkdev_alloc);

int clk_add_alias(const char *alias, const char *alias_dev_name, char *id,
	struct device *dev)
{
	struct clk *r = clk_get(dev, id);
	struct clk_lookup *l;

	if (IS_ERR(r))
		return PTR_ERR(r);

	l = clkdev_alloc(r, alias, alias_dev_name);
	clk_put(r);
	if (!l)
		return -ENODEV;
	clkdev_add(l);
	return 0;
}
EXPORT_SYMBOL(clk_add_alias);

/*
 * clkdev_drop - remove a clock dynamically allocated
 */
void clkdev_drop(struct clk_lookup *cl)
{
	mutex_lock(&clocks_mutex);
	list_del(&cl->node);
	mutex_unlock(&clocks_mutex);
	kfree(cl);
}
EXPORT_SYMBOL(clkdev_drop);

#if defined(CONFIG_PM_DEBUG) && defined(CONFIG_DEBUG_FS) && defined(CONFIG_ARCH_EXYNOS)
/* debugfs support to trace clock tree hierarchy and attributes */
static void print_clk(struct seq_file *s, struct clk *p, int leaf)
{
	if (!p)
		return;

	print_clk(s, p->parent, 0);
	seq_printf(s, "%s", p->name);

	if (p->id >= 0)
		seq_printf(s, ".%d", p->id);

	if (leaf)
		seq_printf(s, "[%ld, %d]\n", clk_get_rate(p), p->usage);
	else
		seq_printf(s, "-->");
}

static int s3c24xx_clock_show(struct seq_file *s, void *unused)
{
	unsigned long flags;
	struct clk_lookup *p;
	
	spin_lock_irqsave(&clocks_lock, flags);

	list_for_each_entry(p, &clocks, node) {
		if (s->private || p->clk->usage){
			print_clk(s,p->clk, 1);
		}
	}

	spin_unlock_irqrestore(&clocks_lock, flags);

	return 0;
}

static int s3c24xx_clock_open(struct inode *inode, struct file *file)
{
	return single_open(file, s3c24xx_clock_show, inode->i_private);
}

static const struct file_operations s3c24xx_clock_operations = {
	.open		= s3c24xx_clock_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init clk_debugfs_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("clocks", NULL);
	if (!d)
		return -ENOMEM;

	debugfs_create_file("all", S_IFREG | S_IRUGO, d, (void *)1,
				    &s3c24xx_clock_operations);
	debugfs_create_file("using", S_IFREG | S_IRUGO, d, (void *)0,
				    &s3c24xx_clock_operations);
	return 0;
}
late_initcall(clk_debugfs_init);
#endif /* defined(CONFIG_PM_DEBUG) && defined(CONFIG_DEBUG_FS) */
