/*
 * SAMSUNG S5P USB HOST EHCI Controller
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Jingoo Han <jg1.han@samsung.com>
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/async.h>

#include <plat/cpu.h>
#include <plat/ehci.h>
#include <plat/usb-phy.h>

#include <mach/regs-pmu.h>
#include <mach/regs-usb-host.h>
#include <mach/board_rev.h>
#include <mach/modem.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif

#define CP_PORT		 2  /* HSIC0 in S5PC210 */
#define RETRY_CNT_LIMIT 30  /* Max 300ms wait for cp resume*/

struct s5p_ehci_hcd {
	struct device *dev;
	struct usb_hcd *hcd;
	struct clk *clk;
	int power_on;
	int phy_on;
};
struct device *s5p_dev;
static int init_resume = 0;
#ifdef CONFIG_XMM6260_MODEM
extern int xmm6260_set_active_state(int val);
#endif

#ifdef CONFIG_HAS_WAKELOCK
struct wake_lock 	s5p_pm_lock;
static inline void s5p_wake_lock_init(void)
{
	wake_lock_init(&s5p_pm_lock, WAKE_LOCK_SUSPEND, "s5p-ehci");
}

static inline void s5p_wake_lock_destroy(void)
{
	wake_lock_destroy(&s5p_pm_lock);
}
static inline void s5p_wake_lock(void)
{
	wake_lock(&s5p_pm_lock);
}

static inline void s5p_wake_unlock(void)
{
	wake_unlock(&s5p_pm_lock);
}
#else
#define s5p_wake_lock_init(void) do { } while (0)
#define s5p_wake_lock_destroy(void) do { } while (0)
#define s5p_wake_lock(void) do { } while (0)
#define s5p_wake_unlock(void) do { } while (0)
#endif

#ifdef CONFIG_USB_EXYNOS_SWITCH
int s5p_ehci_port_power_off(struct platform_device *pdev)
{
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);

	(void) ehci_hub_control(hcd,
			ClearPortFeature,
			USB_PORT_FEAT_POWER,
			1, NULL, 0);
	/* Flush those writes */
	ehci_readl(ehci, &ehci->regs->command);
	return 0;
}
EXPORT_SYMBOL_GPL(s5p_ehci_port_power_off);

int s5p_ehci_port_power_on(struct platform_device *pdev)
{
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);

	(void) ehci_hub_control(hcd,
			SetPortFeature,
			USB_PORT_FEAT_POWER,
			1, NULL, 0);
	/* Flush those writes */
	ehci_readl(ehci, &ehci->regs->command);
	return 0;
}
EXPORT_SYMBOL_GPL(s5p_ehci_port_power_on);
#endif
static int s5p_ehci_configurate(struct usb_hcd *hcd)
{
	/* DMA burst Enable */
	writel(readl(INSNREG00(hcd->regs)) | ENA_DMA_INCR,
			INSNREG00(hcd->regs));
	return 0;
}
static int s5p_ehci_phy_on(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	int ret = 0;
	
	if(!s5p_ehci->phy_on) {
		if (pdata && pdata->phy_init)
			ret = pdata->phy_init(pdev, S5P_USB_PHY_HOST);
		s5p_ehci->phy_on = 1;
	}
	return 0;
}
static int s5p_ehci_phy_off(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	int ret = 0;
	
	if(s5p_ehci->phy_on) {
		if (pdata && pdata->phy_exit)
			ret = pdata->phy_exit(pdev, S5P_USB_PHY_HOST);
#ifdef CONFIG_XMM6260_MODEM
		modem_set_active_state(0);
#endif
		s5p_ehci->phy_on = 0;
	}	
	return 0;
}
#ifdef CONFIG_PM
static int s5p_ehci_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	unsigned long flags;
	int rc = 0;

	if (time_before(jiffies, ehci->next_statechange))
		msleep(10);

	/* Root hub was already suspended. Disable irq emission and
	 * mark HW unaccessible, bail out if RH has been resumed. Use
	 * the spinlock to properly synchronize with possible pending
	 * RH suspend or resume activity.
	 *
	 * This is still racy as hcd->state is manipulated outside of
	 * any locks =P But that will be a different fix.
	 */

	spin_lock_irqsave(&ehci->lock, flags);
	if (hcd->state != HC_STATE_SUSPENDED && hcd->state != HC_STATE_HALT) {
		rc = -EINVAL;
		goto fail;
	}
	ehci_writel(ehci, 0, &ehci->regs->intr_enable);
	(void)ehci_readl(ehci, &ehci->regs->intr_enable);

	clear_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	spin_unlock_irqrestore(&ehci->lock, flags);
	
	s5p_ehci_phy_off(dev);
	if (pdata->phy_power)
		pdata->phy_power(pdev, S5P_USB_PHY_HOST, 0);
	init_resume = 1;
	return 0;
fail:
	spin_unlock_irqrestore(&ehci->lock, flags);
	return rc;
}

#ifdef CONFIG_XMM6260_ENUM_SYNC
static int s5p_ehci_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);

	dev_dbg( dev, "%s: phy resume dev->power.status=%d, hcd->state=%d\n", __func__, dev->power.is_suspended, hcd->state);
	init_resume = 0;	
	
	if (pdata->phy_power)
		pdata->phy_power(pdev, S5P_USB_PHY_HOST, 1);

	pm_runtime_resume(dev);

	if (pdata->phy_init)
		pdata->phy_init(pdev, S5P_USB_PHY_HOST);

	s5p_ehci_configurate(hcd);

	if (time_before(jiffies, ehci->next_statechange))
		msleep(10);

	/* Mark hardware accessible again as we are out of D3 state by now */
	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
	if (ehci_readl(ehci, &ehci->regs->configured_flag) == FLAG_CF) {
		int	mask = INTR_MASK;

		if (!hcd->self.root_hub->do_remote_wakeup)
			mask &= ~STS_PCD;
		ehci_writel(ehci, mask, &ehci->regs->intr_enable);
		ehci_readl(ehci, &ehci->regs->intr_enable);
		return 0;
	}

	ehci_dbg(ehci, "lost power, restarting\n");
	usb_root_hub_lost_power(hcd->self.root_hub);

	(void) ehci_halt(ehci);
	(void) ehci_reset(ehci);

	/* emptying the schedule aborts any urbs */
	spin_lock_irq(&ehci->lock);
	if (ehci->reclaim)
		end_unlink_async(ehci);
	ehci_work(ehci);
	spin_unlock_irq(&ehci->lock);

	ehci_writel(ehci, ehci->command, &ehci->regs->command);
	ehci_writel(ehci, FLAG_CF, &ehci->regs->configured_flag);
	ehci_readl(ehci, &ehci->regs->command);	/* unblock posted writes */

	/* here we "know" root ports should always stay powered */
	ehci_port_power(ehci, 1);

	hcd->state = HC_STATE_SUSPENDED;
	return 0;
}
#else
static int s5p_ehci_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;

	dev_dbg( dev, "%s: phy resume dev->power.status=%d, hcd->state=%d\n", __func__, dev->power.is_suspended, hcd->state);	
	if (pdata->phy_power)
		pdata->phy_power(pdev, S5P_USB_PHY_HOST, 1);
	hcd->state = HC_STATE_SUSPENDED;
	return 0;
}
#endif

#else
#define s5p_ehci_suspend	NULL
#define s5p_ehci_resume		NULL
#endif

#ifdef CONFIG_USB_SUSPEND
static int s5p_ehci_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
#ifdef CONFIG_USB_EXYNOS_SWITCH
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
#endif
	dev_dbg(dev, "%s\n", __func__);
	s5p_wake_unlock();
	if (pdata && pdata->phy_suspend)
		pdata->phy_suspend(pdev, S5P_USB_PHY_HOST);
#ifdef CONFIG_USB_EXYNOS_SWITCH
	if (samsung_board_rev_is_0_0()) {
		ehci_hub_control(hcd,
			ClearPortFeature,
			USB_PORT_FEAT_POWER,
			1, NULL, 0);
		/* Flush those writes */
		ehci_readl(ehci, &ehci->regs->command);

		msleep(20);
	}
#endif
	return 0;
}

static int s5p_ehci_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int rc = 0;

	dev_dbg(dev, "%s\n", __func__);
	if (dev->power.is_suspended)
		return 0;
	s5p_wake_lock();
	/* platform device isn't suspended */
	if(init_resume){
		s5p_ehci_phy_on(dev);
	}else{
		if (pdata && pdata->phy_resume)
			rc = pdata->phy_resume(pdev, S5P_USB_PHY_HOST);
	}
	if (rc || init_resume) {
		s5p_ehci_configurate(hcd);

		if (time_before(jiffies, ehci->next_statechange))
			msleep(10);

		/* Mark hardware accessible again as we are out of D3 state by now */
		set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

		ehci_dbg(ehci, "lost power, restarting\n");
		usb_root_hub_lost_power(hcd->self.root_hub);

		(void) ehci_halt(ehci);
		(void) ehci_reset(ehci);

		/* emptying the schedule aborts any urbs */
		spin_lock_irq(&ehci->lock);
		if (ehci->reclaim)
			end_unlink_async(ehci);
		ehci_work(ehci);
		spin_unlock_irq(&ehci->lock);

		ehci_writel(ehci, ehci->command, &ehci->regs->command);
		ehci_writel(ehci, FLAG_CF, &ehci->regs->configured_flag);
		ehci_readl(ehci, &ehci->regs->command);	/* unblock posted writes */

		/* here we "know" root ports should always stay powered */
		ehci_port_power(ehci, 1);

		hcd->state = HC_STATE_SUSPENDED;
#ifdef CONFIG_USB_EXYNOS_SWITCH
	} else {
		if (samsung_board_rev_is_0_0()) {
			ehci_hub_control(ehci_to_hcd(ehci),
					SetPortFeature,
					USB_PORT_FEAT_POWER,
					1, NULL, 0);
			/* Flush those writes */
			ehci_readl(ehci, &ehci->regs->command);
			msleep(20);
		}
#endif
	}
	init_resume = 0;
	return 0;
}
#else
#define s5p_ehci_runtime_suspend	NULL
#define s5p_ehci_runtime_resume		NULL
#endif
static int s5p_wait_for_cp_resume(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	u32 __iomem	*portsc ;
	u32 val32, retry_cnt = 0;

	portsc = &ehci->regs->port_status[CP_PORT-1];
#ifdef CONFIG_XMM6260_MODEM
	modem_set_active_state(1); /* CP USB Power On */
#endif
	do {
		msleep(10);
		val32 = ehci_readl(ehci, portsc);
	} while (++retry_cnt < RETRY_CNT_LIMIT && !(val32 & PORT_CONNECT));
	printk("%s: retry_cnt = %d\n", __func__, retry_cnt);
	if(retry_cnt<RETRY_CNT_LIMIT)
		return 0;
	return -ETIMEDOUT;
}

static const struct hc_driver s5p_ehci_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "S5P EHCI Host Controller",
	.hcd_priv_size		= sizeof(struct ehci_hcd),

	.irq			= ehci_irq,
	.flags			= HCD_MEMORY | HCD_USB2,

	.reset			= ehci_init,
	.start			= ehci_run,
	.stop			= ehci_stop,
	.shutdown		= ehci_shutdown,

	.get_frame_number	= ehci_get_frame,

	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,
	.endpoint_reset		= ehci_endpoint_reset,

	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.bus_suspend		= ehci_bus_suspend,
	.bus_resume		= ehci_bus_resume,

	.relinquish_port	= ehci_relinquish_port,
	.port_handed_over	= ehci_port_handed_over,
	.wait_for_device	= s5p_wait_for_cp_resume,
	.clear_tt_buffer_complete	= ehci_clear_tt_buffer_complete,
};

int  s5p_ehci_power(int value)
{
	struct usb_hcd *hcd;
	int power_on = value;
	int retval;
	struct platform_device *pdev;
	struct s5p_ehci_platdata *pdata;
	struct s5p_ehci_hcd *s5p_ehci;
	int irq;

	if(!s5p_dev)
		return -1;

	init_resume = 0;	
	pdev = to_platform_device(s5p_dev);
	s5p_ehci = platform_get_drvdata(pdev);
	hcd = s5p_ehci->hcd;
	pdata = pdev->dev.platform_data;
	s5p_wake_lock();
	device_lock(s5p_dev);
	if (power_on == 0 && s5p_ehci->power_on) {
		printk("%s: EHCI turns off\n", __func__);
		pm_runtime_forbid(&pdev->dev);
		usb_remove_hcd(hcd);
		s5p_ehci_phy_off(s5p_dev);
		s5p_ehci->power_on = 0;
		msleep(5);
		s5p_wake_unlock();
	} else if (power_on == 1) {
		printk(KERN_DEBUG "%s: EHCI turns on\n", __func__);
		if (s5p_ehci->power_on) {
			pm_runtime_forbid(&pdev->dev);
			usb_remove_hcd(hcd);
			s5p_ehci_phy_off(s5p_dev);
			msleep(5);
		}
		s5p_ehci_phy_on(s5p_dev);
		s5p_ehci_configurate(hcd);

		irq = platform_get_irq(pdev, 0);
		retval = usb_add_hcd(hcd, irq,
				IRQF_DISABLED | IRQF_SHARED);
		if (retval < 0) {
			dev_err(s5p_dev, "Power On Fail\n");
			goto exit;
		}
		s5p_ehci->power_on = 1;
#ifdef CONFIG_XMM6260_MODEM
		modem_set_active_state(1);
#endif
	}
exit:
	device_unlock(s5p_dev);

	return 0;
}
EXPORT_SYMBOL(s5p_ehci_power);
static ssize_t show_ehci_power(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);

	return sprintf(buf, "EHCI Power %s\n", (s5p_ehci->power_on) ? "on" : "off");
}

static ssize_t store_ehci_power(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int power_on;

	if (sscanf(buf, "%d", &power_on) != 1)
		return -EINVAL;

	s5p_ehci_power(power_on);

	return count;
}
static DEVICE_ATTR(ehci_power, 0664, show_ehci_power, store_ehci_power);

static inline int create_ehci_sys_file(struct ehci_hcd *ehci)
{
	return device_create_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static inline void remove_ehci_sys_file(struct ehci_hcd *ehci)
{
	device_remove_file(ehci_to_hcd(ehci)->self.controller,
			&dev_attr_ehci_power);
}

static int __devinit s5p_ehci_probe(struct platform_device *pdev)
{
	struct s5p_ehci_platdata *pdata;
	struct s5p_ehci_hcd *s5p_ehci;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	struct resource *res;
	int irq;
	int err;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data defined\n");
		return -EINVAL;
	}
	
#ifdef CONFIG_XMM6260_MODEM
	if (pdata->phy_power)
		pdata->phy_power(pdev, S5P_USB_PHY_HOST, 1);
#endif
	s5p_ehci = kzalloc(sizeof(struct s5p_ehci_hcd), GFP_KERNEL);
	if (!s5p_ehci)
		return -ENOMEM;

	s5p_ehci->dev = &pdev->dev;

	hcd = usb_create_hcd(&s5p_ehci_hc_driver, &pdev->dev,
					dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "Unable to create HCD\n");
		err = -ENOMEM;
		goto fail_hcd;
	}

	s5p_ehci->hcd = hcd;
	s5p_ehci->clk = clk_get(&pdev->dev, "usbhost");

	if (IS_ERR(s5p_ehci->clk)) {
		dev_err(&pdev->dev, "Failed to get usbhost clock\n");
		err = PTR_ERR(s5p_ehci->clk);
		goto fail_clk;
	}

	err = clk_enable(s5p_ehci->clk);
	if (err)
		goto fail_clken;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get I/O memory\n");
		err = -ENXIO;
		goto fail_io;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = ioremap(res->start, resource_size(res));
	if (!hcd->regs) {
		dev_err(&pdev->dev, "Failed to remap I/O memory\n");
		err = -ENOMEM;
		goto fail_io;
	}

	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		err = -ENODEV;
		goto fail;
	}

	if (pdata->phy_init)
		pdata->phy_init(pdev, S5P_USB_PHY_HOST);
	s5p_ehci->phy_on = 1;

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs +
		HC_LENGTH(ehci, readl(&ehci->caps->hc_capbase));

	s5p_ehci_configurate(hcd);

	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = readl(&ehci->caps->hcs_params);

	err = usb_add_hcd(hcd, irq, IRQF_DISABLED | IRQF_SHARED);
	if (err) {
		dev_err(&pdev->dev, "Failed to add USB HCD\n");
		goto fail;
	}

	platform_set_drvdata(pdev, s5p_ehci);
	s5p_wake_lock_init();
	s5p_wake_lock();
	create_ehci_sys_file(ehci);
	s5p_ehci->power_on = 1;

#ifdef CONFIG_USB_EXYNOS_SWITCH
	if (samsung_board_rev_is_0_0())
		ehci_hub_control(ehci_to_hcd(ehci),
				ClearPortFeature,
				USB_PORT_FEAT_POWER,
				1, NULL, 0);
#endif
	s5p_dev = &pdev->dev;
#ifdef CONFIG_USB_SUSPEND
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_forbid(&pdev->dev);
#endif
	return 0;

fail:
	iounmap(hcd->regs);
fail_io:
	clk_disable(s5p_ehci->clk);
fail_clken:
	clk_put(s5p_ehci->clk);
fail_clk:
	usb_put_hcd(hcd);
fail_hcd:
	kfree(s5p_ehci);
	return err;
}

static int __devexit s5p_ehci_remove(struct platform_device *pdev)
{
	struct s5p_ehci_platdata *pdata = pdev->dev.platform_data;
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;

#ifdef CONFIG_USB_SUSPEND
	pm_runtime_disable(&pdev->dev);
#endif
	s5p_ehci->power_on = 0;
	remove_ehci_sys_file(hcd_to_ehci(hcd));
	usb_remove_hcd(hcd);

	if (pdata && pdata->phy_exit)
		pdata->phy_exit(pdev, S5P_USB_PHY_HOST);

	iounmap(hcd->regs);

	clk_disable(s5p_ehci->clk);
	clk_put(s5p_ehci->clk);

	usb_put_hcd(hcd);
	kfree(s5p_ehci);
	s5p_wake_lock_destroy();
	return 0;
}

static void s5p_ehci_shutdown(struct platform_device *pdev)
{
	struct s5p_ehci_hcd *s5p_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = s5p_ehci->hcd;

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static const struct dev_pm_ops s5p_ehci_pm_ops = {
	.suspend		= s5p_ehci_suspend,
	.resume			= s5p_ehci_resume,
	.runtime_suspend	= s5p_ehci_runtime_suspend,
	.runtime_resume		= s5p_ehci_runtime_resume,
};

static struct platform_driver s5p_ehci_driver = {
	.probe		= s5p_ehci_probe,
	.remove		= __devexit_p(s5p_ehci_remove),
	.shutdown	= s5p_ehci_shutdown,
	.driver = {
		.name	= "s5p-ehci",
		.owner	= THIS_MODULE,
		.pm = &s5p_ehci_pm_ops,
	}
};

MODULE_ALIAS("platform:s5p-ehci");
