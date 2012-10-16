/* linux/drivers/iommu/exynos_iommu.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#include <linux/memblock.h>
//#include <linux/export.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>

#include <plat/clock.h>

#include <mach/map.h>
#include <mach/regs-sysmmu.h>

#define SECT_ORDER 20
#define LPAGE_ORDER 16
#define SPAGE_ORDER 12

#define SECT_SIZE (1 << SECT_ORDER)
#define LPAGE_SIZE (1 << LPAGE_ORDER)
#define SPAGE_SIZE (1 << SPAGE_ORDER)

#define SECT_MASK (~(SECT_SIZE - 1))
#define LPAGE_MASK (~(LPAGE_SIZE - 1))
#define SPAGE_MASK (~(SPAGE_SIZE - 1))

#define lv1ent_fault(sent) (((*(sent) & 3) == 0) || ((*(sent) & 3) == 3))
#define lv1ent_page(sent) ((*(sent) & 3) == 1)
#define lv1ent_section(sent) ((*(sent) & 3) == 2)

#define lv2ent_fault(pent) ((*(pent) & 3) == 0)
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)

#define section_phys(sent) (*(sent) & SECT_MASK)
#define section_offs(iova) ((iova) & 0xFFFFF)
#define lpage_phys(pent) (*(pent) & LPAGE_MASK)
#define lpage_offs(iova) ((iova) & 0xFFFF)
#define spage_phys(pent) (*(pent) & SPAGE_MASK)
#define spage_offs(iova) ((iova) & 0xFFF)

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)

#define NUM_LV1ENTRIES 4096
#define NUM_LV2ENTRIES 256

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * sizeof(long))

#define SPAGES_PER_LPAGE (LPAGE_SIZE / SPAGE_SIZE)

#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

#define mk_lv1ent_sect(pa) ((pa) | 2)
#define mk_lv1ent_page(pa) ((pa) | 1)
#define mk_lv2ent_lpage(pa) ((pa) | 1)
#define mk_lv2ent_spage(pa) ((pa) | 2)

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0

static struct kmem_cache *lv2table_kmem_cache;

static unsigned long *section_entry(unsigned long *pgtable, unsigned long iova)
{
	return pgtable + lv1ent_offset(iova);
}

static unsigned long *page_entry(unsigned long *sent, unsigned long iova)
{
	return (unsigned long *)__va(lv2table_base(sent)) + lv2ent_offset(iova);
}

enum EXYNOS_SYSMMU_INTERRUPT_TYPE {
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, /* 7 */
	SYSMMU_FAULT_UNKNOWN,
	SYSMMU_FAULTS_NUM
};

typedef int (*sysmmu_fault_handler_t)(enum EXYNOS_SYSMMU_INTERRUPT_TYPE itype,
			unsigned long pgtable_base, unsigned long fault_addr);

static unsigned short fault_reg_offset[SYSMMU_FAULTS_NUM] = {
	EXYNOS_PAGE_FAULT_ADDR,
	EXYNOS_AR_FAULT_ADDR,
	EXYNOS_AW_FAULT_ADDR,
	EXYNOS_DEFAULT_SLAVE_ADDR,
	EXYNOS_AR_FAULT_ADDR,
	EXYNOS_AR_FAULT_ADDR,
	EXYNOS_AW_FAULT_ADDR,
	EXYNOS_AW_FAULT_ADDR
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNKNOWN FAULT"
};

struct exynos_iommu_domain;

struct sysmmu_drvdata {
	struct list_head node;
	struct device *dev;
	struct device *owner;
	void __iomem *sfrbase;
	struct clk *clk;
	bool active;
	rwlock_t lock;
	struct iommu_domain *domain;
	sysmmu_fault_handler_t fault_handler;
	unsigned long pgtable;
};

static LIST_HEAD(sysmmu_list);

static struct sysmmu_drvdata *get_sysmmu_data(struct device *owner,
						struct sysmmu_drvdata *start)
{
	if (start) {
		list_for_each_entry_continue(start, &sysmmu_list, node)
			if (start->owner == owner)
				return start;
	} else  {
		list_for_each_entry(start, &sysmmu_list, node)
			if (start->owner == owner)
				return start;
	}

	return NULL;
}

static struct sysmmu_drvdata *get_sysmmu_data_rollback(struct device *owner,
						struct sysmmu_drvdata *start)
{
	if (start) {
		list_for_each_entry_continue_reverse(start, &sysmmu_list, node)
			if (start->owner == owner)
				return start;
	}

	return NULL;
}

static bool set_sysmmu_active(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU was not active previously
	   and it needs to be initialized */
	if (WARN_ON_ONCE(data->active))
		return false;
	data->active = true;
	return true;
}

static bool set_sysmmu_inactive(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU is needed to be disabled */
	if (WARN_ON_ONCE(!data->active))
		return false;
	data->active = false;
	return true;
}

static bool is_sysmmu_active(struct sysmmu_drvdata *data)
{
	return data->active;
}

static void sysmmu_block(void __iomem *sfrbase)
{
	__raw_writel(CTRL_BLOCK, sfrbase + EXYNOS_MMU_CTRL);
}

static void sysmmu_unblock(void __iomem *sfrbase)
{
	__raw_writel(CTRL_ENABLE, sfrbase + EXYNOS_MMU_CTRL);
}

static void __sysmmu_tlb_invalidate(void __iomem *sfrbase)
{
	__raw_writel(0x1, sfrbase + EXYNOS_MMU_FLUSH);
}

static void __sysmmu_set_ptbase(void __iomem *sfrbase,
				       unsigned long pgd)
{
	__raw_writel(0x0, sfrbase + EXYNOS_MMU_CFG); /* 16KB LV1 */
	__raw_writel(pgd, sfrbase + EXYNOS_PT_BASE_ADDR);

	__sysmmu_tlb_invalidate(sfrbase);
}

static void __set_fault_handler(struct sysmmu_drvdata *data,
					sysmmu_fault_handler_t handler)
{
	unsigned long flags;

	write_lock_irqsave(&data->lock, flags);
	data->fault_handler = handler;
	write_unlock_irqrestore(&data->lock, flags);
}

void exynos_sysmmu_set_fault_handler(struct device *owner,
					sysmmu_fault_handler_t handler)
{
	struct sysmmu_drvdata *data = NULL;

	while ((data = get_sysmmu_data(owner, data)))
		__set_fault_handler(data, handler);
}

static int default_fault_handler(enum EXYNOS_SYSMMU_INTERRUPT_TYPE itype,
		     unsigned long pgtable_base, unsigned long fault_addr)
{
	unsigned long *ent;

	if ((itype >= SYSMMU_FAULTS_NUM) || (itype < SYSMMU_PAGEFAULT))
		itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx(Page table base: 0x%lx)\n",
			sysmmu_fault_name[itype], fault_addr, pgtable_base);

	ent = section_entry(__va(pgtable_base), fault_addr);
	pr_err("\tLv1 entry: 0x%lx\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%lx\n", *ent);
	}

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	BUG();

	return 0;
}

static irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *data = dev_id;
	enum EXYNOS_SYSMMU_INTERRUPT_TYPE itype;
	unsigned long addr;
	int ret = -ENOSYS;

	read_lock(&data->lock);

	WARN_ON(!is_sysmmu_active(data));

	itype = (enum EXYNOS_SYSMMU_INTERRUPT_TYPE)
		__ffs(__raw_readl(data->sfrbase + EXYNOS_INT_STATUS));

	if (WARN_ON(!((itype >= 0) && (itype < SYSMMU_FAULT_UNKNOWN)))) {
		itype = SYSMMU_FAULT_UNKNOWN;
		addr = (unsigned long)-1;
	} else {
		addr = __raw_readl(data->sfrbase + fault_reg_offset[itype]);
	}

	if (data->domain)
		ret = report_iommu_fault(data->domain, data->owner, addr,
									itype);
	if ((ret == -ENOSYS) && data->fault_handler) {
		unsigned long base;
		base = __raw_readl(data->sfrbase + EXYNOS_PT_BASE_ADDR);
		ret = data->fault_handler(itype, base, addr);
	}

	if (!ret && (itype != SYSMMU_FAULT_UNKNOWN))
		__raw_writel(1 << itype, data->sfrbase + EXYNOS_INT_CLEAR);
	else
		dev_dbg(data->dev, "%s is not handled.\n",
						sysmmu_fault_name[itype]);

	sysmmu_unblock(data->sfrbase);

	read_unlock(&data->lock);

	return IRQ_HANDLED;
}

static bool __sysmmu_disable(struct sysmmu_drvdata *data)
{
	unsigned long flags;
	bool disabled = false;

	write_lock_irqsave(&data->lock, flags);

	if (set_sysmmu_inactive(data)) {
		__raw_writel(CTRL_DISABLE, data->sfrbase + EXYNOS_MMU_CTRL);
		clk_disable(data->clk);
		disabled = true;
		data->pgtable = 0;
		data->domain = NULL;
	}

	write_unlock_irqrestore(&data->lock, flags);

	if (disabled)
		pm_runtime_put_sync(data->dev);

	return disabled;
}

static int __exynos_sysmmu_enable(struct device *owner, unsigned long pgtable,
						struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;
	struct sysmmu_drvdata *data = NULL;

	BUG_ON(!memblock_is_memory(pgtable));

	/* There are some devices that control more System MMUs than one such
	 * as MFC.
	 */
	while ((data = get_sysmmu_data(owner, data))) {
		ret = pm_runtime_get_sync(data->dev);
		if (ret < 0)
			break;

		write_lock_irqsave(&data->lock, flags);

		if (set_sysmmu_active(data)) {
			clk_enable(data->clk);

			data->pgtable = pgtable;

			__sysmmu_set_ptbase(data->sfrbase, pgtable);

			__raw_writel(CTRL_ENABLE,
					data->sfrbase + EXYNOS_MMU_CTRL);

			data->domain = domain;

			dev_dbg(data->dev, "Enabled.\n");
		} else {
			if (WARN_ON(pgtable != data->pgtable)) {
				set_sysmmu_inactive(data);
				ret = -EBUSY;
			}

			dev_dbg(data->dev, "Already enabled.\n");
		}

		write_unlock_irqrestore(&data->lock, flags);

		if (ret != 0)
			pm_runtime_put_sync(data->dev);

		if (ret < 0)
			break;
	}

	if (ret < 0) {
		while ((data = get_sysmmu_data_rollback(owner, data))) {
			__sysmmu_disable(data);
			dev_dbg(data->dev, "Failed to enable.\n");
		}
	} else {
		ret = 0;
	}

	return ret;
}

int exynos_sysmmu_set_ptbase(struct device *owner,  unsigned long pgtable)
{
	unsigned long flags;
	struct sysmmu_drvdata *data = NULL;
	
	BUG_ON(!memblock_is_memory(pgtable));

	/* There are some devices that control more System MMUs than one such
	 * as MFC.
	 */
	while ((data = get_sysmmu_data(owner, data))) {
		write_lock_irqsave(&data->lock, flags);
		if (is_sysmmu_active(data)) {
			data->pgtable = pgtable;
			sysmmu_block(data->sfrbase);
			__raw_writel(0x0, data->sfrbase + EXYNOS_MMU_CFG); /* 16KB LV1 */
			__raw_writel(pgtable, data->sfrbase + EXYNOS_PT_BASE_ADDR);
			__sysmmu_tlb_invalidate(data->sfrbase);
			sysmmu_unblock(data->sfrbase);
		}
		write_unlock_irqrestore(&data->lock, flags);
	}

	return 0;
}

int exynos_sysmmu_enable(struct device *owner, unsigned long pgtable)
{
	return __exynos_sysmmu_enable(owner, pgtable, NULL);
}

/*static*/ void exynos_sysmmu_disable(struct device *owner)
{
	struct sysmmu_drvdata *data = NULL;

	while ((data = get_sysmmu_data(owner, data))) {
		if (__sysmmu_disable(data))
			dev_dbg(data->dev, "Disabled.\n");
		else
			dev_dbg(data->dev,
					"Deactivation request ignorred\n");
	}
}

void exynos_sysmmu_tlb_invalidate(struct device *owner)
{
	struct sysmmu_drvdata *data = NULL;

	while ((data = get_sysmmu_data(owner, data))) {
		unsigned long flags;

		read_lock_irqsave(&data->lock, flags);

		if (is_sysmmu_active(data)) {
			sysmmu_block(data->sfrbase);
			__sysmmu_tlb_invalidate(data->sfrbase);
			sysmmu_unblock(data->sfrbase);
		} else {
			dev_dbg(data->dev,
				"Disabled. Skipping invalidating TLB.\n");
		}

		read_unlock_irqrestore(&data->lock, flags);
	}
}

static int exynos_sysmmu_probe(struct platform_device *pdev)
{
	struct resource *res, *ioarea;
	int ret;
	int irq;
	struct device *dev, *owner;
	void *sfr;
	struct sysmmu_drvdata *data;
	char *emsg;

	dev = &pdev->dev;
	owner = pdev->dev.archdata.iommu;

	if (owner == NULL) {
		pr_debug("%s: No System MMU is assigned for %s.%d.\n", __func__,
				pdev->name, pdev->id);
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		emsg = "Not enough memory";
		ret = -ENOMEM;
		goto err_alloc;
	}

	data->owner = owner;

	ret = dev_set_drvdata(dev, data);
	if (ret) {
		emsg = "Unable to set driver data.";
		goto err_init;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		emsg = "Failed probing system MMU: failed to get resource.";
		goto err_init;
	}

	ioarea = request_mem_region(res->start, resource_size(res),
								dev_name(dev));
	if (ioarea == NULL) {
		emsg = "failed to request memory region.";
		ret = -ENOMEM;
		goto err_init;
	}

	sfr = ioremap(res->start, resource_size(res));
	if (!sfr) {
		emsg = "failed to call ioremap().";
		ret = -ENOENT;
		goto err_ioremap;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		emsg = "failed to get irq resource.";
		ret = irq;
		goto err_irq;
	}

	ret = request_irq(irq, exynos_sysmmu_irq, 0, dev_name(dev), data);
	if (ret) {
		emsg = "failed to request irq.";
		goto err_irq;
	}

	data->clk = clk_get(dev, "clk_sysmmu");
	if (IS_ERR(data->clk)) {
		emsg = "failed to get clock descriptor";
		ret = PTR_ERR(data->clk);
		goto err_clk;
	}

	data->dev = dev;
	data->sfrbase = sfr;
	rwlock_init(&data->lock);
	INIT_LIST_HEAD(&data->node);

	__set_fault_handler(data, &default_fault_handler);
	list_add(&data->node, &sysmmu_list);

	if (dev->parent)
		pm_runtime_enable(dev);

	pr_debug("%s: System MMU for %s.%d Initialized.\n", __func__,
							pdev->name, pdev->id);

	return 0;
err_clk:
	free_irq(irq, data);
err_irq:
	iounmap(sfr);
err_ioremap:
	release_resource(ioarea);
	kfree(ioarea);
err_init:
	kfree(data);
err_alloc:
	pr_err("%s: %s.%d Failed: %s\n", __func__, pdev->name, pdev->id, emsg);
	return ret;
}

 #if 0
static int exynos_sysmmu_runtime_suspend(struct device *dev)
 {
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_sysmmu_info *sysmmu = platform_get_drvdata(pdev);

	if (sysmmu->domain)
		s5p_disable_iommu(sysmmu);

	return 0;
 }

static int exynos_sysmmu_runtime_resume(struct device *dev)
 {
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_sysmmu_info *sysmmu = platform_get_drvdata(pdev);

	if (sysmmu->domain)
		s5p_enable_iommu(sysmmu);

	return 0;
 }
#endif

static int exynos_pm_resume(struct device *dev)
{
	struct sysmmu_drvdata *data;

	data = dev_get_drvdata(dev);

	if (is_sysmmu_active(data)) {
		__sysmmu_set_ptbase(data->sfrbase, data->pgtable);

		__raw_writel(CTRL_ENABLE, data->sfrbase + EXYNOS_MMU_CTRL);
	}

	return 0;
}

static const struct dev_pm_ops exynos_pm_ops = {
//	.runtime_suspend = exynos_sysmmu_runtime_suspend,
//	.runtime_resume  = exynos_sysmmu_runtime_resume,
	.resume = &exynos_pm_resume,
};

static struct platform_driver exynos_sysmmu_driver = {
	.probe		= exynos_sysmmu_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "exynos-sysmmu",
		.pm		= &exynos_pm_ops,
	}
};

/* We does not consider super section mapping (16MB) */
struct iommu_client {
	struct list_head node;
	struct device *dev;
	int refcnt;
};

struct exynos_iommu_domain {
	struct list_head clients; /* list of iommu_client */
	unsigned long *pgtable; /* lv1 page table, 16KB */
	short *lv2entcnt; /* free lv2 entry counter for each section */
	spinlock_t lock; /* lock for this structure */
	spinlock_t pgtablelock; /* lock for modifying page table @ pgtable */
};

static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}

static int exynos_iommu_domain_init(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtable = (unsigned long *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->pgtable)
		goto err_pgtable;

	priv->lv2entcnt = (short *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 1);
	if (!priv->lv2entcnt)
		goto err_counter;

	pgtable_flush(priv->pgtable, priv->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->pgtablelock);
	INIT_LIST_HEAD(&priv->clients);

	domain->priv = priv;
	return 0;

err_counter:
	free_pages((unsigned long)priv->pgtable, 2);
err_pgtable:
	kfree(priv);
	return -ENOMEM;
}

static void exynos_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct list_head *pos, *n;
	unsigned long flags;
	int i;

	WARN_ON(!list_empty(&priv->clients));

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_safe(pos, n, &priv->clients) {
		struct iommu_client *client;

		client = list_entry(pos, struct iommu_client, node);
		exynos_sysmmu_disable(client->dev);
		kfree(client);
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	for (i = 0; i < NUM_LV1ENTRIES; i++)
		if (lv1ent_page(priv->pgtable + i))
			kmem_cache_free(lv2table_kmem_cache,
						__va(lv2table_base(priv->pgtable + i)));

	free_pages((unsigned long)priv->pgtable, 2);
	free_pages((unsigned long)priv->lv2entcnt, 1);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int exynos_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	int ret;
	struct exynos_iommu_domain *priv = domain->priv;
	struct iommu_client *client = NULL;
	struct list_head *pos;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each(pos, &priv->clients) {
		struct iommu_client *cur;

		cur = list_entry(pos, struct iommu_client, node);
		if (cur->dev == dev) {
			client = cur;
			break;
		}
	}

	if (client != NULL) {
		dev_err(dev, "%s: IOMMU with pgtable 0x%lx already attached\n",
					__func__, __pa(priv->pgtable));
		client->refcnt++;
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (client != NULL)
		return 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	INIT_LIST_HEAD(&client->node);
	client->dev = dev;
	client->refcnt = 1;

	ret = __exynos_sysmmu_enable(dev, __pa(priv->pgtable), domain);
	if (ret) {
		kfree(client);
		return ret;
	}

	spin_lock_irqsave(&priv->lock, flags);
	list_add_tail(&client->node, &priv->clients);
	spin_unlock_irqrestore(&priv->lock, flags);

	dev_info(dev, "%s: Attached new IOMMU with pgtable 0x%lx\n", __func__,
						__pa(priv->pgtable));
	return 0;
}

static void exynos_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct iommu_client *client = NULL;
	struct list_head *pos;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each(pos, &priv->clients) {
		struct iommu_client *cur;

		cur = list_entry(pos, struct iommu_client, node);
		if (cur->dev == dev) {
			cur->refcnt--;
			client = cur;
			break;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (WARN_ON(client == NULL))
		return;

	if (client->refcnt > 0) {
		dev_dbg(dev, "%s: Detaching IOMMU with pgtable 0x%lx delayed\n",
					__func__, __pa(priv->pgtable));
		return;
	}

	BUG_ON(client->refcnt != 0);

	list_del(&client->node);
	exynos_sysmmu_disable(client->dev);
	kfree(client);
	dev_dbg(dev, "%s: Detached IOMMU with pgtable 0x%lx\n", __func__,
						__pa(priv->pgtable));
}

static unsigned long *alloc_lv2entry(unsigned long *sent, unsigned long iova,
					short *pgcounter)
{
	if (lv1ent_fault(sent)) {
		unsigned long *pent;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return NULL;

		*sent = mk_lv1ent_page(__pa(pent));
		*pgcounter = NUM_LV2ENTRIES;
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		pgtable_flush(sent, sent + 1);
	}

	return page_entry(sent, iova);
}

static int lv1set_section(unsigned long *sent, phys_addr_t paddr, int nent,
							short *pgcounter)
{
	int i;

	for (i = 0; i < nent; i++) {
		if (lv1ent_section(sent))
			goto error;

		if (lv1ent_page(sent)) {
			if (*pgcounter != NUM_LV2ENTRIES)
				goto error;

			kmem_cache_free(lv2table_kmem_cache, page_entry(sent, 0));

			*pgcounter = 0;
		}

		*sent = mk_lv1ent_sect(paddr);

		paddr += SECT_SIZE;
		sent++;
		pgcounter++;
	}

	pgtable_flush(sent - nent, sent);

	return 0;
error:
	if (i > 0)
		memset(sent - i, 0, i * sizeof(*sent));

	return -EADDRINUSE;
}

static int lv2set_page(unsigned long *pent, phys_addr_t paddr, int nent,
							short *pgcounter)
{
	int i;

	if (pent == NULL)
		return -ENOMEM;

	if (nent < SPAGES_PER_LPAGE) {
		for (i = 0; i < nent; i++) {
			if (!lv2ent_fault(pent))
				goto error;

			*pent = mk_lv2ent_spage(paddr);

			paddr += SPAGE_SIZE;
			pent++;
		}
	} else {
		for (i = 0; i < nent; i += SPAGES_PER_LPAGE) {
			int j;
			for (j = 0; j < SPAGES_PER_LPAGE; j++) {
				if (!lv2ent_fault(pent)) {
					i += j;
					goto error;
				}

				*pent = mk_lv2ent_lpage(paddr);
				pent++;
			}
			paddr += LPAGE_SIZE;
		}
	}

	pgtable_flush(pent - nent, pent);

	*pgcounter -= nent;
	if (*pgcounter < 0)
		pr_err("%s: pgcounter < 0: pgcounter = %d, nent = %d\n",
				__func__, *pgcounter, nent);
	return 0;

error:
	memset(pent - i, 0, i * sizeof(*pent));

	return -EADDRINUSE;
}

static int exynos_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long *entry;
	unsigned long flags;
	int ret = -ENOMEM;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (size >= SECT_SIZE) {
		ret = lv1set_section(entry, paddr, size >> SECT_ORDER,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		unsigned long *pent;

		pent = alloc_lv2entry(entry, iova,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

		ret = lv2set_page(pent, paddr, size >> SPAGE_ORDER,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	}
	if (ret) {
		pr_err("%s: Failed to map iova 0x%lx/0x%x bytes\n",
							__func__, iova, size);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return ret;
}

static size_t exynos_iommu_unmap(struct iommu_domain *domain,
					       unsigned long iova, size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct iommu_client *client;
	unsigned long flags;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	while (size != 0) {
		int i, nent, order;
		unsigned long *pent, *sent;

		sent = section_entry(priv->pgtable, iova);

		order = min(__ffs(iova), __fls(size));

		if (!lv1ent_fault(sent) && (order < SECT_ORDER)) {
			pent = page_entry(sent, iova);

			BUG_ON((order < LPAGE_ORDER) && lv2ent_large(pent));

			nent = 1 << (order - SPAGE_ORDER);
			memset(pent, 0, nent * sizeof(*pent));
			pgtable_flush(pent, pent + nent);

			priv->lv2entcnt[lv1ent_offset(iova)] += (short)nent;
			iova += 1 << order;
		} else {
			nent = 1 << (order - SECT_ORDER);

			for (i = 0; i < nent; i++) {
				if (lv1ent_section(sent)) {
					*sent = 0;
				} else if (lv1ent_page(sent)) {
					pent = page_entry(sent, 0);
					memset(pent, 0,
						NUM_LV2ENTRIES * sizeof(*pent));
					pgtable_flush(pent,
							pent + NUM_LV2ENTRIES);
					priv->lv2entcnt[lv1ent_offset(iova)]
							= NUM_LV2ENTRIES;
				}
				iova += SECT_SIZE;
				sent++;
			}
			pgtable_flush(sent - nent, sent);
		}

		size -= 1 << order;
	}

	list_for_each_entry(client, &priv->clients, node) {
		exynos_sysmmu_tlb_invalidate(client->dev);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return size;
}

static phys_addr_t exynos_iommu_iova_to_phys(struct iommu_domain *domain,
					  unsigned long iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long *entry;
	unsigned long flags;
	phys_addr_t phys = -1;

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return phys;
}

static int exynos_iommu_domain_has_cap(struct iommu_domain *domain,
				    unsigned long cap)
{
	return 0;
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.domain_has_cap = &exynos_iommu_domain_has_cap,
	.pgsize_bitmap = 0xFFFFF000,
};

static int __init exynos_iommu_init(void)
{
	int ret;

	lv2table_kmem_cache = kmem_cache_create("exynos-iommu-lv2table",
               LV2TABLE_SIZE, LV2TABLE_SIZE, 0, NULL);
	if (!lv2table_kmem_cache) {
	       pr_err("%s: failed to create kmem cache\n", __func__);
	       return -ENOMEM;
	}

	ret = platform_driver_register(&exynos_sysmmu_driver);

	if (ret == 0)
		bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);

	return ret;
}
arch_initcall(exynos_iommu_init);
