/*
 * linux/arch/arm/mach-exynos/exynos4_ppmu.c
 * 
 *   Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *   Author:  lvcha qiu   <lvcha@meizu.com>
 *
 * Samsung EXYNOS4 PPMU (Performance Profiling Managed Unit)
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
 */
 
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/iommu.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/platform_data/exynos4_ppmu.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/ppmu.h>
#include <mach/dev-ppmu.h>

#include <plat/cpu.h>

struct ppmu_drvdata {
	struct list_head node;
	char name[32];
	rwlock_t lock;
	struct device *dev;
	int pdev_id;
	void __iomem *base;
	struct clk *clk;

	unsigned int ccnt;
	bool ccnt_irq;
	unsigned int count[4];
	event_type event[4];
	bool event_irq[4];

	enum ppmu_irq_type {
		CCNT_IRQ = 1,
		EVENT_IRQ,
	} int_type;
	unsigned int ccnt_now;
	unsigned int count_now[4];
	atomic_t irq_count;
	
	int debug;
};

#define exynos_ppmu_debug(fmt, arg...) \
do { \
	if (ppmu->debug) \
		printk(KERN_INFO fmt, ##arg); \
} while(0)

#define exynos_ppmu_writel(data, offset) \
do { \
	__raw_writel(data, ppmu->base + offset); \
} while(0)
#define exynos_ppmu_readl(offset) __raw_readl(ppmu->base + offset)
#define exynos_ppmu_en(onoff) __raw_writel(!!onoff, ppmu->base + PPMU_PMNC)
#define exynos_ppmu_reset_all() __raw_writel(3<<1, ppmu->base + PPMU_PMNC)

static struct ppmu_drvdata *ppmu_data[PPMU_END] = {NULL,};
static struct srcu_notifier_head ppmu_notifier_list[PPMU_END];

int ppmu_register_notifier(struct notifier_block *nb, ppmu_index index)
{
	int ret;

	/* 
	  * if (initial done)??
	  */
	ret = srcu_notifier_chain_register(&ppmu_notifier_list[index], nb);

	return ret;
}

struct exynos4_ppmu_data exynos4_ppmu_update(ppmu_index index)
{
	struct ppmu_drvdata *ppmu = ppmu_data[index];
	struct exynos4_ppmu_data data;
	unsigned long flag;
	int i;

	read_lock_irqsave(&ppmu->lock, flag);

	/* Stop ppmu */
	exynos_ppmu_en(false);

	data.index = index;
	data.ccnt = exynos_ppmu_readl(PPMU_CCNT) - ppmu->ccnt;
	data.count0 = exynos_ppmu_readl(PPMU_PMCNT0) - ppmu->count[0];
	data.count1 = exynos_ppmu_readl(PPMU_PMCNT1) - ppmu->count[1];
	data.count2 = exynos_ppmu_readl(PPMU_PMCNT2) - ppmu->count[2];
	if (soc_is_exynos4210()) {
		data.count3 = exynos_ppmu_readl(PPMU_PMCNT3);
	} else {
		data.count3 = exynos_ppmu_readl(PPMU_PMCNT3) << 8;
		data.count3 |= exynos_ppmu_readl(PPMU_PMCNT4);
	}

	WARN(!data.ccnt, "ppmu = %s\n", ppmu->name);

	data.load = div64_u64((data.count3 * 100), data.ccnt?:1);

	/* reset all ccnt anf count */
	exynos_ppmu_reset_all();
	if (!soc_is_exynos4210()) {
		exynos_ppmu_writel((3<<16)|0xf, PPMU_CNT_RESET);
	}

	if (ppmu->ccnt)
		exynos_ppmu_writel(ppmu->ccnt, PPMU_CCNT);

	for (i=0; i<ARRAY_SIZE(ppmu->count); i++) {
		if (ppmu->count[i]) {
			/* Init Performance Monitor Count Registers */
			exynos_ppmu_writel(ppmu->count[i],
					PPMU_PMCNT0 + (i * PPMU_PMCNT_OFFSET));
		}
	}

	/* Sart ppmu */
	exynos_ppmu_en(true);

	read_unlock_irqrestore(&ppmu->lock, flag);

	return data;
}

static ssize_t ppmu_update_show(struct device *dev,
     struct device_attribute *attr, char *buf)
{
	struct ppmu_drvdata *ppmu = dev_get_drvdata(dev);
	struct exynos4_ppmu_data data;
	int ret;

	data = exynos4_ppmu_update((ppmu_index)ppmu->pdev_id);

	ret = sprintf(buf, "ccnt = 0x%08x\n"
				     "count0 = 0x%08x\n"
				     "count1 = 0x%08x\n"
				     "count2 = 0x%08x\n"
				     "count3 = 0x%016Lx\n",
					data.ccnt, data.count0, 
					data.count1, data.count2, 
					data.count3);

	return ret;
}

static ssize_t ppmu_debug_show(struct device *dev,
     struct device_attribute *attr, char *buf)
{
	struct ppmu_drvdata *ppmu = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n",  ppmu->debug ? "enable" : "disable");
}

static ssize_t ppmu_debug_store(struct device *dev,
      struct device_attribute *attr,
      const char *buf, size_t count)
{
	struct ppmu_drvdata *ppmu = dev_get_drvdata(dev);
	int ret;
	ret = sscanf(buf, "%d", &ppmu->debug);
	return count;
}

static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR|S_IWGRP,
   	ppmu_debug_show, ppmu_debug_store);

static DEVICE_ATTR(update, S_IRUGO,
   	ppmu_update_show, NULL);

static struct attribute * ppmu_attributes[] = {
	&dev_attr_debug.attr,
	&dev_attr_update.attr, 
	NULL
};

static struct attribute_group ppmu_attribute_group = {
	.attrs = ppmu_attributes
};

static irqreturn_t exynos_ppmu_irq_thread(int irq, void *dev_id)
{
	struct ppmu_drvdata *ppmu = dev_id;
	int ret;
	ret = srcu_notifier_call_chain(&ppmu_notifier_list[ppmu->pdev_id],
				ppmu->int_type, ppmu);
	exynos_ppmu_debug("%s: ret = %d\n", __func__, ret);
	return IRQ_HANDLED;
}

static int inline exynos_irq_ccnt(struct ppmu_drvdata *ppmu)
{
	int i;

	ppmu->int_type = CCNT_IRQ;
	ppmu->ccnt_now = ~ppmu->ccnt;

	/* Init Cycle Count Register */
	exynos_ppmu_writel(ppmu->ccnt, PPMU_CCNT);

	for (i=0;i<ARRAY_SIZE(ppmu->count_now);i++) {
		unsigned int count_now;

		count_now = exynos_ppmu_readl(PPMU_PMCNT0 + (i * PPMU_PMCNT_OFFSET));
		ppmu->count_now[i] = count_now - ppmu->count[i];

		/* Init Performance Monitor Count Registers */
		exynos_ppmu_writel(ppmu->count[i],
					PPMU_PMCNT0 + (i * PPMU_PMCNT_OFFSET));

		if (ppmu->count_now[i] && ppmu->event_irq[i])
			exynos_ppmu_debug("CCNT_IRQ: count[%d] = %u\n", i, ppmu->count_now[i]);
	}

	return 0;
}

static int inline exynos_irq_count(struct ppmu_drvdata *ppmu)
{
	int i;

	ppmu->int_type = EVENT_IRQ;
	ppmu->ccnt_now = exynos_ppmu_readl(PPMU_CCNT);
	ppmu->ccnt_now -= ppmu->ccnt;

	for (i=0; i<ARRAY_SIZE(ppmu->count); i++) {
		if (ppmu->count[i]) {
			ppmu->count_now[i] = ~ppmu->count[i];
			/* Init Performance Monitor Count Registers */
			exynos_ppmu_writel(ppmu->count[i],
					PPMU_PMCNT0 + (i * PPMU_PMCNT_OFFSET));
		}
	}

	return 0;
}
static irqreturn_t exynos_ppmu_irq(int irq, void *dev_id)
{
	struct ppmu_drvdata *ppmu = dev_id;
	int flag;

	read_lock(&ppmu->lock);

	/* Stop ppmu */
	exynos_ppmu_en(false);

	ppmu->int_type = 0;

	do {
		flag = exynos_ppmu_readl(PPMU_FLAG);
		exynos_ppmu_writel(flag, PPMU_FLAG);

		if (flag & 1<<CCNT_OFFSET) {/* ccnt int */
			exynos_irq_ccnt(ppmu);
		} else if (flag & 0xf) {	/* event int */
			exynos_irq_count(ppmu);
		}
	} while(flag);

	/* Start ppmu */
	exynos_ppmu_en(true);

	read_unlock(&ppmu->lock);

	return ppmu->int_type ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static inline int exynos_ppmu_init(struct ppmu_drvdata *ppmu)
{
	int i;
	unsigned int ppmu_cnt_en = 0;
	unsigned int ppmu_int_en = 0;

	/* Stop ppmu */
	exynos_ppmu_en(false);

	/* Init Cycle Count Register */
	exynos_ppmu_writel(ppmu->ccnt, PPMU_CCNT);

	/* Enable cycle counter? */
	ppmu_cnt_en |= 1 << CCNT_OFFSET;

	/* Enable ccnt irq? */
	ppmu_int_en |= ppmu->ccnt_irq ? (1 << CCNT_OFFSET) : 0;

	for (i=0; i<ARRAY_SIZE(ppmu->count); i++) {
		/* Init Performance Monitor Count Registers */
		exynos_ppmu_writel(ppmu->count[i],
				PPMU_PMCNT0 + (i * PPMU_PMCNT_OFFSET));

		/* Event Selection Register */
		if (ppmu->event[i]) {
			exynos_ppmu_writel(ppmu->event[i]-1, 
					PPMU_BEVT0SEL + (i * PPMU_BEVTSEL_OFFSET));
		}

		/* Enable Counter? */
		ppmu_cnt_en |= ppmu->event[i] ? (1 << i) : 0;

		/* Enable event irq? */
		ppmu_int_en |= ppmu->event_irq[i] ? (1 << i) : 0;
	}

	/* Enable Performance Monitor Control Register */
	if (ppmu_cnt_en) {
		exynos_ppmu_writel(ppmu_cnt_en, PPMU_CNTENS);
		exynos_ppmu_writel(~ppmu_cnt_en, PPMU_CNTENC);
	}

	/* overflow interrupt enable */
	if (ppmu_int_en) {
		exynos_ppmu_writel(ppmu_int_en, PPMU_INTENS);
		exynos_ppmu_writel(~ppmu_int_en, PPMU_INTENC);
	}

	if (soc_is_exynos4210()) {
		for (i = 0; i < NUMBER_OF_COUNTER; i++) {
			exynos_ppmu_writel(0x0, DEVT0_ID + (i * DEVT_ID_OFFSET));
			exynos_ppmu_writel(0x0, DEVT0_IDMSK + (i * DEVT_ID_OFFSET));
		}
	}

	/* Start ppmu */
	exynos_ppmu_en(true);

	return 0;
}

static int exynos_ppmu_probe(struct platform_device *pdev)
{
	struct ppmu_drvdata *ppmu;
	struct exynos4_ppmu_pd *pd = pdev->dev.platform_data;
	struct resource *res, *ioarea;
	void *sfr;
	char *emsg;
	int irq, i;
	int ret;

	ppmu = kzalloc(sizeof(*ppmu), GFP_KERNEL);
	if (!ppmu) {
		emsg = "Not enough memory";
		ret = -ENOMEM;
		goto err_alloc;
	}

	ppmu->dev = &pdev->dev;

	ret = dev_set_drvdata(ppmu->dev, ppmu);
	if (ret) {
		emsg = "Unable to set driver data.";
		goto err_init;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		emsg = "Failed probing ppmu: failed to get resource.";
		goto err_init;
	}

	ioarea = request_mem_region(res->start, resource_size(res),
								dev_name(ppmu->dev));
	if (ioarea == NULL) {
		emsg = "failed to request memory region.";
		ret = -ENOMEM;
		goto err_mem;
	}

	sfr = ioremap(res->start, resource_size(res));
	if (!sfr) {
		emsg = "failed to call ioremap().";
		ret = -ENOENT;
		goto err_ioremap;
	}
	ppmu->base = sfr;
		
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		emsg = "failed to get irq resource.";
		ret = irq;
		goto err_irq;
	}

	ret = request_threaded_irq(irq, exynos_ppmu_irq,
					exynos_ppmu_irq_thread,
					0, dev_name(ppmu->dev), ppmu);
	if (ret) {
		emsg = "failed to request irq.";
		goto err_irq;
	}
	
	ppmu->clk = clk_get(ppmu->dev, "clk_ppmu");
	if (IS_ERR(ppmu->clk)) {
		emsg = "failed to get clock descriptor";
		ret = PTR_ERR(ppmu->clk);
		goto err_clk;
	}
	clk_enable(ppmu->clk);

	rwlock_init(&ppmu->lock);
	srcu_init_notifier_head(&ppmu_notifier_list[pdev->id]);

	ppmu->ccnt = pd->ccnt;
	ppmu->ccnt_irq = pd->ccnt_irq;
	ppmu->pdev_id = pdev->id;
	ppmu_data[pdev->id] = ppmu;
	ppmu->debug = false;
	strncpy(ppmu->name, pd->name, sizeof(ppmu->name));

	for (i=0; i<ARRAY_SIZE(ppmu->count); i++) {
		ppmu->count[i] = pd->count[i];
		ppmu->event[i] = pd->event[i];
		ppmu->event_irq[i] = pd->event_irq[i];
	}

	ret = sysfs_create_group(&ppmu->dev->kobj, &ppmu_attribute_group);

	ret = exynos_ppmu_init(ppmu);

	pr_debug("%s: PPMU for %s.%d Initialized.\n", __func__,
				pdev->name, pdev->id);
	return 0;

err_clk:
	free_irq(irq, ppmu);
err_irq:
	iounmap(sfr);
err_ioremap:
	release_resource(ioarea);
	kfree(ioarea);
err_mem:
	dev_set_drvdata(ppmu->dev, NULL);
err_init:
	kfree(ppmu);
err_alloc:
	pr_err("%s: %s.%d Failed: %s\n", __func__, pdev->name, pdev->id, emsg);
	return ret;
}

#ifdef CONFIG_PM
static int exynos_ppmu_suspend(struct device *dev)
{
	struct ppmu_drvdata *ppmu = dev_get_drvdata(dev);

	/* Stop ppmu*/
	exynos_ppmu_en(false);
	exynos_ppmu_reset_all();

	return 0;
}

static int exynos_ppmu_resume(struct device *dev)
{
	struct ppmu_drvdata *ppmu = dev_get_drvdata(dev);
	int ret;

	ret = exynos_ppmu_init(ppmu);

	return 0;
}
#else
#define exynos_ppmu_suspend NULL
#define exynos_ppmu_resume NULL
#endif

static const struct dev_pm_ops exynos_ppmu_pm = {
	.suspend = exynos_ppmu_suspend,
	.resume = exynos_ppmu_resume,
};

static struct platform_driver exynos_ppmu_driver = {
	.probe		= exynos_ppmu_probe,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "exynos4-ppmu",
		.pm	= &exynos_ppmu_pm,
	}
};

static int __init init_ppmu_pdevice(void)
{
	return platform_driver_register(&exynos_ppmu_driver);
}

late_initcall(init_ppmu_pdevice);

MODULE_DESCRIPTION("Exynos performance profiling managed unit driver");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
