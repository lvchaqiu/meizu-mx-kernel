/*
 * charge-detect.c
 *
 * Copyright (c) 2011 WenbinWu	<wenbinwu@meizu.com>
 *
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/sched.h>
#include <linux/regulator/machine.h>
#include <mach/usb-detect.h>
#include <mach/gpio-m032.h>
#include <plat/gpio-cfg.h>

#define USB_VBUS_INSERT_LEVEL 1
#define USB_HOST_INSERT_LEVEL 0
#define USB_DOCK_INSERT_LEVEL 0

struct usb_detect_info {
	int usb_vbus_gpio;
	int usb_host_gpio;
	int usb_dock_gpio;

	int vbus_irq;
	int host_irq;
	int dock_irq;

	struct delayed_work usb_work;
	struct regulator *reverse;
#ifdef CONFIG_HAS_WAKELOCK	
	struct wake_lock usb_detect_lock;
#endif
};

enum mx_usb_det_type {
	TYPE_USB_M030,
	TYPE_USB_M032,
};

static struct usb_detect_info *g_ud_info = NULL;
static struct blocking_notifier_head usb_notifier_list = BLOCKING_NOTIFIER_INIT(usb_notifier_list);

#define HOST_SELECT			1
#define DEVICES_SELECT		0

int mx_usb_host_select(int on)
{
	gpio_set_value(USB_SELECT, on);
	pr_debug("%s: USB_SELECT=%d\n", __func__, gpio_get_value(USB_SELECT));
	return 0;
}

static irqreturn_t usb_detect_irq_handler(int irq, void *dev_id)
{
	struct usb_detect_info *ud_info = (struct usb_detect_info *)dev_id;

	wake_lock_timeout(&ud_info->usb_detect_lock, 3*HZ);

	if (!delayed_work_pending(&ud_info->usb_work))
		cancel_delayed_work(&ud_info->usb_work);

	schedule_delayed_work(&ud_info->usb_work, msecs_to_jiffies(500));
	return IRQ_HANDLED;
}

static void m030_usb_detect_work(struct work_struct *work)
{
	struct usb_detect_info *ud_info = container_of(work, struct usb_detect_info, usb_work.work);
	int val;
	int vbus;
	int usbid;
	static int last_vbus = 0;
	static int last_usbid = 0;

	vbus = gpio_get_value(ud_info->usb_vbus_gpio) == USB_VBUS_INSERT_LEVEL;
	usbid = gpio_get_value(ud_info->usb_host_gpio) == USB_HOST_INSERT_LEVEL;

	pr_debug("vbus(%d), usbid(%d)\n", vbus, usbid);
	pr_debug("last_vbus(%d), last_usbid(%d)\n", last_vbus, last_usbid);

	if(usbid != last_usbid) {
		last_usbid = usbid;
		val = usbid ? USB_HOST_INSERT : USB_HOST_REMOVE;
		pr_debug("=======usb id %d\n", val);
		blocking_notifier_call_chain(&usb_notifier_list, val, NULL);
	}

	if(vbus != last_vbus) {
		last_vbus = vbus;
		val = vbus ? USB_VBUS_INSERT : USB_VBUS_REMOVE;
		blocking_notifier_call_chain(&usb_notifier_list, val, NULL);
	}
}

static void mx_usb_detect_work(struct work_struct *work)
{
	struct usb_detect_info *ud_info = container_of(work, struct usb_detect_info, usb_work.work);
	int val;
	int vbus;
	int usbid;
	int dock;
	static int last_vbus = 0;
	static int last_usbid = 0;
	static int last_dock = 0;

	vbus = gpio_get_value(ud_info->usb_vbus_gpio) == USB_VBUS_INSERT_LEVEL;
	dock = gpio_get_value(ud_info->usb_dock_gpio) == USB_DOCK_INSERT_LEVEL;

	pr_debug("vbus(%d), dock(%d)\n", vbus, dock);
	pr_debug("last_vbus(%d), last_dock(%d)\n", last_vbus, last_dock);

	if(dock != last_dock) {
		last_dock = dock;
		val = dock ? USB_DOCK_INSERT : USB_DOCK_REMOVE;
		blocking_notifier_call_chain(&usb_notifier_list, val, NULL);
		if(dock) {
			pr_debug("dock insert\n");
			free_irq(ud_info->host_irq, ud_info);
			mx_usb_host_select(HOST_SELECT);
			s3c_gpio_cfgpin(ud_info->usb_host_gpio, S3C_GPIO_OUTPUT);
			s3c_gpio_setpull(ud_info->usb_host_gpio, S3C_GPIO_PULL_NONE);
			gpio_set_value(ud_info->usb_host_gpio, 0);
		} else {
			int error;
			s3c_gpio_setpull(ud_info->usb_host_gpio, S3C_GPIO_PULL_UP);
			error = request_threaded_irq(ud_info->host_irq, NULL,
						usb_detect_irq_handler,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						"host_irq", ud_info);
			if(error) {
				pr_debug("request usb_host irq error\n");
				last_dock = true;
				schedule_delayed_work(&ud_info->usb_work, msecs_to_jiffies(500));
			} else {
				mx_usb_host_select(DEVICES_SELECT);
			}
		}
	}

	if(!dock) {
		usbid = gpio_get_value(ud_info->usb_host_gpio) == USB_HOST_INSERT_LEVEL;
		pr_debug("usbid(%d), last_usbid(%d)\n", usbid, last_usbid);
		if(vbus && usbid) {
			pr_debug("vbus in, dismiss the usbid in.\n");
		} else {
			if(usbid != last_usbid) {
				last_usbid = usbid;
				val = usbid ? USB_HOST_INSERT : USB_HOST_REMOVE;
				blocking_notifier_call_chain(&usb_notifier_list, val, NULL);
				if(usbid) {
					if(!regulator_is_enabled(ud_info->reverse))
						regulator_enable(ud_info->reverse);
				} else{
					if(regulator_is_enabled(ud_info->reverse))
						regulator_disable(ud_info->reverse);
				}
			}
		}
	}

	if(vbus != last_vbus) {
		last_vbus = vbus;
		val = vbus ? USB_VBUS_INSERT : USB_VBUS_REMOVE;
		blocking_notifier_call_chain(&usb_notifier_list, val, NULL);
	}
}

static int __devinit usb_detect_probe(struct platform_device *pdev)
{
	struct usb_detect_platform_data *pdata =pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	enum mx_usb_det_type type = platform_get_device_id(pdev)->driver_data;
	struct usb_detect_info *info;
	int ret;

	if(pdata == NULL) {
		dev_err(dev, "Failed to get platform data\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(struct usb_detect_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Failed to allocate device\n");
		return -ENOMEM;
	}

	g_ud_info = info;
	info->usb_vbus_gpio = pdata->usb_vbus_gpio;
	info->usb_host_gpio = pdata->usb_host_gpio;
	info->usb_dock_gpio = pdata->usb_dock_gpio;

	wake_lock_init(&info->usb_detect_lock, WAKE_LOCK_SUSPEND, "usb-detect");

	if(type == TYPE_USB_M030) {
		INIT_DELAYED_WORK(&info->usb_work, m030_usb_detect_work);
	} else {
		INIT_DELAYED_WORK(&info->usb_work, mx_usb_detect_work);
	}

	info->reverse = regulator_get(dev, "reverse");
	if (IS_ERR(info->reverse))
		dev_err(dev, "Failed to get reverse regulator\n");

	info->vbus_irq = gpio_to_irq(info->usb_vbus_gpio);
	if (info->vbus_irq < 0) {
		pr_err("failed to gpio_to_irq for vbus_irq\n");
		ret = -EINVAL;
		goto err_gpio_vbus;
	}
	ret = request_threaded_irq(info->vbus_irq, NULL,
				usb_detect_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"vbus_irq", info);
	if (ret) {
		dev_err(dev, "Active Failed to allocate usb_vbus interrupt\n");
		goto err_vbus_irq;
	}

	info->host_irq = gpio_to_irq(info->usb_host_gpio);
	if (info->host_irq < 0) {
		pr_err("failed to gpio_to_irq for host_irq\n");
		ret = -EINVAL;
		goto err_gpio_host;
	}
	ret = request_threaded_irq(info->host_irq, NULL,
					usb_detect_irq_handler,
					IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					"host_irq", info);
	if (ret) {
		dev_err(dev, "Active Failed to allocate usb_host interrupt\n");
		goto err_host_irq;
	}

	if (info->usb_dock_gpio > 0) {
		info->dock_irq = gpio_to_irq(info->usb_dock_gpio);
		if (info->dock_irq < 0) {
			pr_err("failed to gpio_to_irq for dock_irq\n");
			ret = -EINVAL;
			goto err_gpio_dock;
		}
		ret = request_threaded_irq(info->dock_irq, NULL,
						usb_detect_irq_handler,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						"dock_irq", info);
		if (ret) {
			dev_err(dev, "Active Failed to allocate dock_detect interrupt\n");
			goto err_dock_irq;
		}
	}

	platform_set_drvdata(pdev, info);

	schedule_delayed_work(&info->usb_work, msecs_to_jiffies(500));

	pr_debug("%s done!\n", __func__);

	return 0;

err_dock_irq:
err_gpio_dock:
	free_irq(info->host_irq, info);
err_host_irq:
err_gpio_host:
	free_irq(info->vbus_irq, info);
err_vbus_irq:
err_gpio_vbus:
	regulator_put(info->reverse);
	wake_lock_destroy(&info->usb_detect_lock);
	return ret;
}

static int __devexit usb_detect_remove(struct platform_device *pdev)
{
	struct usb_detect_info *info = platform_get_drvdata(pdev);

	wake_lock_destroy(&info->usb_detect_lock);
	cancel_delayed_work(&info->usb_work);
	free_irq(info->vbus_irq, info);
	free_irq(info->host_irq, info);
	if (info->dock_irq > 0)
		free_irq(info->dock_irq, info);

	if (!IS_ERR(info->reverse)) {
		if(regulator_is_enabled(info->reverse))
			regulator_disable(info->reverse);
		regulator_put(info->reverse);
	}

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void usb_detect_shutdown(struct platform_device *pdev)
{
	struct usb_detect_info *info = platform_get_drvdata(pdev);
	
	if (!IS_ERR(info->reverse)) {
		if(regulator_is_enabled(info->reverse))
			regulator_disable(info->reverse);
		regulator_put(info->reverse);
	}
}

int register_mx_usb_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&usb_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(register_mx_usb_notifier);

int unregister_mx_usb_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&usb_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_mx_usb_notifier);

int mx_is_usb_vbus_insert(void)
{
	if(g_ud_info)
		return (gpio_get_value(g_ud_info->usb_vbus_gpio) == USB_VBUS_INSERT_LEVEL) ? 1 : 0;		
	else
		return 0;
}
EXPORT_SYMBOL_GPL(mx_is_usb_vbus_insert);

int mx_is_usb_host_insert(void)
{
	if(g_ud_info)
		return (gpio_get_value(g_ud_info->usb_host_gpio) == USB_HOST_INSERT_LEVEL) ? 1 : 0;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(mx_is_usb_host_insert);

int mx_is_usb_dock_insert(void)
{
	if(g_ud_info)
		return (gpio_get_value(g_ud_info->usb_dock_gpio) == USB_DOCK_INSERT_LEVEL) ? 1 : 0;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(mx_is_usb_host_insert);

static struct platform_device_id mx_usb_detect_ids[] = {
	{ "m030_usb_det", TYPE_USB_M030 },
	{ "m032_usb_det", TYPE_USB_M032 },
	{ }
};

static struct platform_driver usb_detect_driver = {
	.probe		= usb_detect_probe,
	.remove		= __devexit_p(usb_detect_remove),
	.shutdown	= usb_detect_shutdown,
	.driver		= {
		.name	= "usb_detect",
		.owner	= THIS_MODULE,
	},
	.id_table = mx_usb_detect_ids,
};

static int __init usb_detect_init(void)
{
	platform_driver_register(&usb_detect_driver);
	return 0; 
}

static void __exit usb_detect_exit(void)
{
	platform_driver_unregister(&usb_detect_driver);
}

late_initcall(usb_detect_init);
module_exit(usb_detect_exit);
