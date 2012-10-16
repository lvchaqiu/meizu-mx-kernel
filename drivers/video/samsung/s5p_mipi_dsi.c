/* linux/drivers/video/samsung/s5p_mipi_dsi.c
 *
 * Samsung MIPI-DSI driver.
 *
 * InKi Dae, <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/pm_runtime.h>

#include <asm/mach-types.h>

#include <plat/fb.h>
#include <plat/mipi_dsim.h>
#include <plat/regs-mipidsim.h>
#include <mach/gpio-common.h>

#include "s5p_mipi_dsi_common.h"

#define REPEATE_CNT 50

#define master_to_driver(a)	(a->dsim_lcd_drv)
#define master_to_device(a)	(a->dsim_lcd_dev)

enum lcd_shutdown_state{
	SUSPEND_LCD,
	SHUTDOWN_LCD,		
};

static void s5p_dsim_earler_suspend(struct early_suspend *);
static void s5p_dsim_early_suspend(struct early_suspend *);
static void s5p_dsim_late_resume(struct early_suspend *);

struct mipi_dsim_ddi {
	int				bus_id;
	struct list_head		list;
	struct mipi_dsim_lcd_device	*dsim_lcd_dev;
	struct mipi_dsim_lcd_driver	*dsim_lcd_drv;
};

static LIST_HEAD(dsim_ddi_list);
static LIST_HEAD(dsim_lcd_dev_list);

static DEFINE_MUTEX(mipi_dsim_lock);

static struct s5p_platform_mipi_dsim *to_dsim_plat(struct platform_device *pdev)
{
	return (struct s5p_platform_mipi_dsim *)pdev->dev.platform_data;
}

static irqreturn_t s5p_mipi_dsi_interrupt_handler(int irq, void *dev_id)
{
	struct mipi_dsim_device *dsim = dev_id;
	unsigned int reg;

	reg = __raw_readl(dsim->reg_base + S5P_DSIM_INTSRC);
	__raw_writel(reg, dsim->reg_base + S5P_DSIM_INTSRC);
	return IRQ_HANDLED;
}

int s5p_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device *lcd_dev)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_dev) {
		printk(KERN_ERR "mipi_dsim_lcd_device is NULL.\n");
		return -EFAULT;
	}

	if (!lcd_dev->name) {
		printk(KERN_ERR "dsim_lcd_device name is NULL.\n");
		return -EFAULT;
	}

	dsim_ddi = kzalloc(sizeof(struct mipi_dsim_ddi), GFP_KERNEL);
	if (!dsim_ddi) {
		printk(KERN_ERR "failed to allocate dsim_ddi object.\n");
		return -EFAULT;
	}

	dsim_ddi->dsim_lcd_dev = lcd_dev;

	mutex_lock(&mipi_dsim_lock);
	list_add_tail(&dsim_ddi->list, &dsim_ddi_list);
	mutex_unlock(&mipi_dsim_lock);

	return 0;
}

static struct mipi_dsim_ddi
	*s5p_mipi_dsi_find_lcd_device(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi;
	struct mipi_dsim_lcd_device *lcd_dev;

	mutex_lock(&mipi_dsim_lock);

	list_for_each_entry(dsim_ddi, &dsim_ddi_list, list) {
		lcd_dev = dsim_ddi->dsim_lcd_dev;
		if (!lcd_dev)
			continue;

		if (lcd_drv->id >= 0) {
			if ((strcmp(lcd_drv->name, lcd_dev->name)) == 0 &&
					lcd_drv->id == lcd_dev->id) {
				/**
				 * bus_id would be used to identify
				 * connected bus.
				 */
				dsim_ddi->bus_id = lcd_dev->bus_id;
				mutex_unlock(&mipi_dsim_lock);

				return dsim_ddi;
			}
		} else {
			if ((strcmp(lcd_drv->name, lcd_dev->name)) == 0) {
				/**
				 * bus_id would be used to identify
				 * connected bus.
				 */
				dsim_ddi->bus_id = lcd_dev->bus_id;
				mutex_unlock(&mipi_dsim_lock);

				return dsim_ddi;
			}
		}

		kfree(dsim_ddi);
		list_del(&dsim_ddi_list);
	}

	mutex_unlock(&mipi_dsim_lock);

	return NULL;
}

int s5p_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_drv) {
		printk(KERN_ERR "mipi_dsim_lcd_driver is NULL.\n");
		return -EFAULT;
	}

	if (!lcd_drv->name) {
		printk(KERN_ERR "dsim_lcd_driver name is NULL.\n");
		return -EFAULT;
	}

	dsim_ddi = s5p_mipi_dsi_find_lcd_device(lcd_drv);
	if (!dsim_ddi) {
		printk(KERN_ERR "mipi_dsim_ddi object not found.\n");
		return -EFAULT;
	}

	dsim_ddi->dsim_lcd_drv = lcd_drv;

	printk(KERN_INFO "registered panel driver(%s) to mipi-dsi driver.\n",
		lcd_drv->name);

	return 0;

}

static struct mipi_dsim_ddi
	*s5p_mipi_dsi_bind_lcd_ddi(struct mipi_dsim_device *dsim,
			const char *name)
{
	struct mipi_dsim_ddi *dsim_ddi;
	struct mipi_dsim_lcd_driver *lcd_drv;
	struct mipi_dsim_lcd_device *lcd_dev;
	int ret;

	mutex_lock(&dsim->lock);

	list_for_each_entry(dsim_ddi, &dsim_ddi_list, list) {
		lcd_drv = dsim_ddi->dsim_lcd_drv;
		lcd_dev = dsim_ddi->dsim_lcd_dev;
		if (!lcd_drv || !lcd_dev ||
			(dsim->id != dsim_ddi->bus_id))
				continue;

		dev_info(dsim->dev, "lcd_drv->id = %d, lcd_dev->id = %d\n",
				lcd_drv->id, lcd_dev->id);
		dev_info(dsim->dev, "lcd_dev->bus_id = %d, dsim->id = %d\n",
				lcd_dev->bus_id, dsim->id);

		if ((strcmp(lcd_drv->name, name) == 0)) {
			lcd_dev->master = dsim;

			lcd_dev->dev.parent = dsim->dev;
			dev_set_name(&lcd_dev->dev, "%s", lcd_drv->name);

			ret = device_register(&lcd_dev->dev);
			if (ret < 0) {
				dev_err(dsim->dev,
					"can't register %s, status %d\n",
					dev_name(&lcd_dev->dev), ret);
				mutex_unlock(&dsim->lock);

				return NULL;
			}

			dsim->dsim_lcd_dev = lcd_dev;
			dsim->dsim_lcd_drv = lcd_drv;

			mutex_unlock(&dsim->lock);

			return dsim_ddi;
		}
	}

	mutex_unlock(&dsim->lock);

	return NULL;
}

/* define MIPI-DSI Master operations. */
static struct mipi_dsim_master_ops master_ops = {
	.cmd_write			= s5p_mipi_dsi_wr_data,
	.get_dsim_frame_done	= s5p_mipi_dsi_get_frame_done_status,
	.clear_dsim_frame_done	= s5p_mipi_dsi_clear_frame_done,
};

/* update all register settings to MIPI DSI controller. */
static void s5p_mipi_update_cfg(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_init_dsim(dsim);
	s5p_mipi_dsi_init_link(dsim);
	s5p_mipi_dsi_set_hs_enable(dsim);

	/* set display timing */
	s5p_mipi_dsi_set_display_mode(dsim);

//	s5p_mipi_dsi_init_interrupt(dsim);
}

static int s5p_mipi_init_lcd(struct mipi_dsim_device *dsim)
{
#define RETRY_CNT 10	
	struct mipi_dsim_lcd_driver *dsim_lcd_drv = master_to_driver(dsim);
	struct mipi_dsim_lcd_device *dsim_lcd_dev= master_to_device(dsim);
	int i, ret = -1;

	for (i=0; i<RETRY_CNT; i++) {/*try 10 times*/

		pm_runtime_get_sync(dsim->dev);

		if (dsim_lcd_drv->resume)
			ret = dsim_lcd_drv->resume(dsim_lcd_dev);

		s5p_mipi_update_cfg(dsim);

		/* initialize mipi-dsi client(lcd panel). */
		if (dsim_lcd_drv && dsim_lcd_drv->init_lcd)
			ret = dsim_lcd_drv->init_lcd(dsim_lcd_dev);

		if(!ret)
			break;
		
		if (dsim_lcd_drv->shutdown)
			dsim_lcd_drv->shutdown(dsim_lcd_dev);
		s5p_mipi_dsi_disable_link(dsim);

		pm_runtime_put_sync(dsim->dev);

		msleep(10);
	}
	return ret;
}

static int s5p_mipi_shutdown_lcd(struct mipi_dsim_device *dsim, enum lcd_shutdown_state state)
{
	struct platform_device *pdev =
		container_of(dsim->dev, struct platform_device, dev);
	struct mipi_dsim_lcd_driver *dsim_lcd_drv = master_to_driver(dsim);
	struct mipi_dsim_lcd_device *dsim_lcd_dev= master_to_device(dsim);

	switch(state)
	{
	case SUSPEND_LCD:
		if (dsim_lcd_drv->suspend)
			dsim_lcd_drv->suspend(dsim_lcd_dev);
		break;
	case SHUTDOWN_LCD:
		if (dsim_lcd_drv->shutdown)
			dsim_lcd_drv->shutdown(dsim_lcd_dev);

		/* dsi configuration */
		s5p_mipi_dsi_disable_link(dsim);
		
		pm_runtime_put_sync(&pdev->dev);

		break;
	}
	return 0;
}

static int s5p_mipi_setup_clk(struct platform_device *pdev)
{
#ifdef CONFIG_ARCH_EXYNOS4
	struct clk *clock_phy, *parent;
	int ret = -1;

	/* set mipiphyl4 = 800MHZ */
	clock_phy = clk_get(&pdev->dev, "sclk_mipi_phy");
	if (IS_ERR(clock_phy)) {
		panic("clk_get sclk_mipi_phy failed\n");
	}

	if (machine_is_m032() || machine_is_m031())
		parent = clk_get(NULL, "mout_mpll_user");
	else
		parent = clk_get(NULL, "mout_mpll");

	if (IS_ERR(parent)) {
		panic("clk_get mout_mpll_user failed\n");
	}

	ret = clk_set_parent(clock_phy, parent);
	if (ret) {
		panic("clk_set_parent failed\n");
	}

	ret = clk_set_rate(clock_phy, 800000000);
	if (ret) {
		panic("clk_set_rate 800MHZ  failed\n");
	} 
	ret = clk_enable(clock_phy);
	if (ret) {
		panic("clk_set_rate 800MHZ  failed\n");
	}

	clock_phy = clk_get(&pdev->dev, "sclk_mipi");
	ret = clk_set_rate(clock_phy, 100000000);
	if (ret) {
		panic("clk_set_rate 100MHZ  failed\n");
	}
	return ret;
#endif
	return 0;
}
static int s5p_mipi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mipi_dsim_device *dsim;
	struct mipi_dsim_config *dsim_config;
	struct s5p_platform_mipi_dsim *dsim_pd;
	struct mipi_dsim_ddi *dsim_ddi;
	struct mipi_dsim_lcd_driver *dsim_lcd_drv;
	int ret = -1;

	if (mx_is_factory_test_mode(MX_FACTORY_TEST_ALL))
		return 0;
	
	dsim = kzalloc(sizeof(struct mipi_dsim_device), GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -EFAULT;
	}

	dsim->pd = to_dsim_plat(pdev);
	dsim->dev = &pdev->dev;
	dsim->resume_complete = 0;

	/* get s5p_platform_dsim. */
	dsim_pd = (struct s5p_platform_mipi_dsim *)dsim->pd;
	if (IS_ERR_OR_NULL(dsim_pd)) {
		dev_err(&pdev->dev, "failed to get platform data for dsim.\n");
		return -EFAULT;
	}

	/* get mipi_dsim_config. */
	dsim_config = dsim_pd->dsim_config;
	if (IS_ERR_OR_NULL(dsim_config)) {
		dev_err(&pdev->dev, "failed to get dsim config data.\n");
		return -EFAULT;
	}

	dsim->dsim_config = dsim_config;
	dsim->master_ops = &master_ops;

	platform_set_drvdata(pdev, dsim);

	dsim->ip_clock = clk_get(&pdev->dev, "dsim0");
	if (IS_ERR(dsim->ip_clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source\n");
		goto err_clock_get;
	}

	ret = s5p_mipi_setup_clk(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup_clk\n");
		goto err_clock_set;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		ret = -EINVAL;
		goto err_platform_get;
	}

	res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	dsim->res = res;

	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -EINVAL;
		goto err_mem_region;
	}

	/*
	 * it uses frame done interrupt handler
	 * only in case of MIPI Video mode.
	 */
	if (dsim_config->e_interface == DSIM_VIDEO) {
		dsim->irq = platform_get_irq(pdev, 0);
		if (request_irq(dsim->irq, s5p_mipi_dsi_interrupt_handler,
				IRQF_SHARED, dev_name(&pdev->dev), dsim)) {
			dev_err(&pdev->dev, "request_irq failed.\n");
			goto err_request_irq;
		}
	}

	mutex_init(&dsim->lock);

	/* find lcd panel driver registered to mipi-dsi driver. */
	dsim_ddi = s5p_mipi_dsi_bind_lcd_ddi(dsim, dsim_pd->lcd_panel_name);
	if (!dsim_ddi) {
		dev_err(&pdev->dev, "mipi_dsim_ddi object not found.\n");
		goto err_bind;
	}
	
	dsim_lcd_drv = master_to_driver(dsim);
	/* initialize mipi-dsi client(lcd panel). */
	if (dsim_lcd_drv && dsim_lcd_drv->probe)
		dsim_lcd_drv->probe(dsim_ddi->dsim_lcd_dev);

	pm_runtime_enable(&pdev->dev);

	ret = s5p_mipi_init_lcd(dsim);
	if (ret)
		panic("init lcd failed\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	dsim->mipi_early_suspend.suspend = s5p_dsim_early_suspend;
	dsim->mipi_early_suspend.resume = s5p_dsim_late_resume;
	dsim->mipi_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB+10;
	register_early_suspend(&dsim->mipi_early_suspend);

	dsim->mipi_earler_suspend.suspend = s5p_dsim_earler_suspend;
	dsim->mipi_earler_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&dsim->mipi_earler_suspend);
#endif

	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim_config->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");

	return 0;

err_bind:
	pm_runtime_put(&pdev->dev);

	if (dsim->dsim_config->e_interface == DSIM_VIDEO)
		free_irq(dsim->irq, dsim);

err_request_irq:
	release_resource(dsim->res);
	kfree(dsim->res);

	iounmap((void __iomem *) dsim->reg_base);

err_mem_region:
err_platform_get:
	clk_disable(dsim->ip_clock);
err_clock_set:
	clk_put(dsim->ip_clock);
err_clock_get:
	kfree(dsim);

	return ret;

}

#if defined(CONFIG_PM_RUNTIME)
static int s5p_mipi_runtime_suspend(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);

	/* enable MIPI-DSI PHY. */
	if (dsim->pd->phy_enable)
		dsim->pd->phy_enable(pdev, false);

	clk_disable(dsim->ip_clock);
	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(pdev, false);
	return 0;
}

static int s5p_mipi_runtime_resume(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_get_drvdata(dev);
	struct platform_device *pdev = to_platform_device(dev);
	
	clk_enable(dsim->ip_clock);

	if (dsim->pd->mipi_power)
		dsim->pd->mipi_power(pdev, true);

	/* enable MIPI-DSI PHY. */
	if (dsim->pd->phy_enable)
		dsim->pd->phy_enable(pdev, true);
	return 0;
}
#else
#define s5p_mipi_runtime_suspend NULL
#define s5p_mipi_runtime_resume NULL
#endif

static const struct dev_pm_ops s5p_mipi_pm_ops = {
	.runtime_suspend = s5p_mipi_runtime_suspend,
	.runtime_resume = s5p_mipi_runtime_resume,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void s5p_dsim_earler_suspend(struct early_suspend *h)
{
	struct mipi_dsim_device *dsim =
		container_of(h, struct mipi_dsim_device, mipi_earler_suspend);
	
	s5p_mipi_shutdown_lcd(dsim, SUSPEND_LCD);
}

static void s5p_dsim_early_suspend(struct early_suspend *h)
{
	struct mipi_dsim_device *dsim =
		container_of(h, struct mipi_dsim_device, mipi_early_suspend);

	s5p_mipi_shutdown_lcd(dsim, SHUTDOWN_LCD);
}

static void s5p_dsim_late_resume(struct early_suspend *h)
{
	struct mipi_dsim_device *dsim =
		container_of(h, struct mipi_dsim_device, mipi_early_suspend);
	int ret;

	ret = s5p_mipi_init_lcd(dsim);
	if (ret)
		panic("resume lcd failed\n");	
}
#endif

static struct platform_driver s5p_mipi_driver = {
	.probe = s5p_mipi_probe,
	.driver = {
		.name = "s5p-mipi-dsim",
		.owner = THIS_MODULE,
		.pm = &s5p_mipi_pm_ops,
	},
};

static int s5p_mipi_register(void)
{
	return platform_driver_register(&s5p_mipi_driver);
}

static void s5p_mipi_unregister(void)
{
	platform_driver_unregister(&s5p_mipi_driver);
}

module_init(s5p_mipi_register);
module_exit(s5p_mipi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSI driver");
MODULE_LICENSE("GPLV2");
