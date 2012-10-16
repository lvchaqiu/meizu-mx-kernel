/*
 * Copyright (C) 2010 Samsung Electronics Co., Ltd.
 *
 * Copyright (C) 2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Modified for Crespo on August, 2010 By Samsung Electronics Co.
 * This is modified operate according to each status.
 *
 */

/* Control bluetooth power for Crespo platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/serial_core.h>

#include <mach/gpio.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <mach/mx-rfkill.h>
#include <mach/gpio-common.h>

#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
#include <linux/slab.h>

#ifndef GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW		0
#define GPIO_LEVEL_HIGH		1
#endif

#define BT_LED_DELAY (12 * 1000)

#define BT_RXD EXYNOS4_GPA0(0)
#define BT_TXD EXYNOS4_GPA0(1)
#define BT_CTS EXYNOS4_GPA0(2)
#define BT_RTS EXYNOS4_GPA0(3)

static volatile int bt_is_running = 0;

static int gpio_bt_power;//BT_POWER
static int gpio_wlan_power;//WL_POWER
static int gpio_bt_reset;//BT_RESET
static int gpio_wlan_reset;//WL_RESET	
static int gpio_bt_wake;//BT_WAKE
static int gpio_bt_host_wake;//BT_HOST_WAKE
static int gpio_bt_host_wake_irq;//BT_HOST_WAKE IRQ

static const char bt_name[] = "bcm4329_bt";
static int debug_mode = 0;
static struct wake_lock rfkill_wake_lock;

struct bt_rfkill_info {
	int bt_enable;
	int bt_wake;
	int bt_test_mode;
	struct mutex bt_lock;
	struct device *dev;
	struct delayed_work test_work;
	struct delayed_work led_delay_on_work;
	struct workqueue_struct *monitor_wqueue;

	struct rfkill *bt_rfk;
};

static struct {
	struct hrtimer bt_lpm_timer;
	ktime_t bt_lpm_delay;
} bt_lpm;

int check_bt_running(void) {
	return bt_is_running;
}
EXPORT_SYMBOL(check_bt_running);

void bt_uart_rts_ctrl(int flag)
{
	if(!gpio_get_value(gpio_bt_reset))
		return ;
	if(flag) {
		// BT RTS Set to HIGH
		s3c_gpio_cfgpin(BT_RTS, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(BT_RTS, S3C_GPIO_PULL_NONE);
		gpio_set_value(BT_RTS, 1);
	}
	else {
		// BT RTS Set to LOW
		s3c_gpio_cfgpin(BT_RTS, S3C_GPIO_OUTPUT);
		gpio_set_value(BT_RTS, 0);

		s3c_gpio_cfgpin(BT_RTS, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(BT_RTS, S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(bt_uart_rts_ctrl);

//////////////////low power control//////////////////////////////

static enum hrtimer_restart bt_enter_lpm(struct hrtimer *timer)
{
	bt_is_running = 0;
	gpio_set_value(gpio_bt_wake, 0);	
	return HRTIMER_NORESTART;
}

void bt_uart_wake_peer(struct uart_port *port)
{
	bt_is_running = 1;
	if (!bt_lpm.bt_lpm_timer.function)
		return;

	hrtimer_try_to_cancel(&bt_lpm.bt_lpm_timer);
	gpio_set_value(gpio_bt_wake, 1);	
	hrtimer_start(&bt_lpm.bt_lpm_timer, bt_lpm.bt_lpm_delay, HRTIMER_MODE_REL);
}

static int bt_lpm_init(void)
{

	s3c_gpio_cfgpin(gpio_bt_wake, S3C_GPIO_OUTPUT);
	gpio_set_value(BT_RTS, 0);

	/*init hr timer*/
	hrtimer_init(&bt_lpm.bt_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.bt_lpm_delay = ktime_set(1, 0);	/* 1 sec */
	bt_lpm.bt_lpm_timer.function = bt_enter_lpm;

	return 0;
}

/////////////////rfkill interface/////////////////////////////////

static int bt_set_power(void *data, enum rfkill_user_states state)
{
	int ret = 0;
	int bt_state = gpio_get_value(gpio_bt_reset);

	switch (state) {

	case RFKILL_USER_STATE_UNBLOCKED:
		
		pr_info("[BT] Device Powering ON\n");

		if(bt_state == GPIO_LEVEL_HIGH)
			pr_info("[BT] Device Powering already ON!!!\n");
		else {
			gpio_set_value(gpio_bt_power, GPIO_LEVEL_HIGH);
			gpio_set_value(gpio_wlan_power, GPIO_LEVEL_HIGH);
			gpio_set_value(gpio_bt_reset, GPIO_LEVEL_HIGH);
			gpio_set_value(gpio_bt_wake, GPIO_LEVEL_HIGH);

			ret = enable_irq_wake(gpio_bt_host_wake_irq);
			if (ret < 0)
				pr_err("[BT] set wakeup src failed\n");

			enable_irq(gpio_bt_host_wake_irq);

			/*
			 * at least 150 msec  delay,  after bt rst
			 * (bcm4329 powerup sequence)
			 */
			msleep(150);
		}

		break;

	case RFKILL_USER_STATE_SOFT_BLOCKED:
		
		pr_info("[BT] Device Powering OFF\n");
		
		if(bt_state == GPIO_LEVEL_LOW)
			pr_info("[BT] Device Powering already OFF!!!\n");
		else {
			bt_is_running = 0;

			/* Set irq */
			ret = disable_irq_wake(gpio_bt_host_wake_irq);
			if (ret < 0)
				pr_err("[BT] unset wakeup src failed\n");

			disable_irq(gpio_bt_host_wake_irq);

			/* Unlock wake lock */
			wake_unlock(&rfkill_wake_lock);

			gpio_set_value(gpio_bt_wake, GPIO_LEVEL_LOW);

			gpio_set_value(gpio_bt_reset, GPIO_LEVEL_LOW);

			/* Check WL_RESET */
			if (gpio_get_value(gpio_wlan_reset) == GPIO_LEVEL_LOW) {
				/* Set WL_POWER and BT_POWER low */
				gpio_set_value(gpio_bt_power, GPIO_LEVEL_LOW);
				gpio_set_value(gpio_wlan_power, GPIO_LEVEL_LOW);
			}
		}
		break;

	default:
		pr_err("[BT] Bad bluetooth rfkill state %d\n", state);
	}

	return 0;
}

irqreturn_t bt_host_wake_irq_handler(int irq, void *dev_id)
{
	if(debug_mode)
		pr_info("------[BT] bt_host_wake_irq_handler start\n");

	if (gpio_get_value(gpio_bt_host_wake)) {
		bt_is_running = 1;
		wake_lock(&rfkill_wake_lock);
	}
	else
		wake_lock_timeout(&rfkill_wake_lock, HZ);

	return IRQ_HANDLED;
}

static int bt_rfkill_set_block(void *data, bool blocked)
{
	unsigned int ret = 0;

	ret = bt_set_power(data, blocked ?
			RFKILL_USER_STATE_SOFT_BLOCKED :
			RFKILL_USER_STATE_UNBLOCKED);

	return ret;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_rfkill_set_block,
};

/////////////////sysfs interface/////////////////////////////////
static ssize_t bt_name_show(struct device *dev,
    					struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  bt_name);
}

static ssize_t bt_enable_show(struct device *dev,
     					struct device_attribute *attr, char *buf)
{
	struct bt_rfkill_info *bt_info = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",  bt_info->bt_enable);
}

static ssize_t bt_enable_store(struct device *dev,
      					struct device_attribute *attr,
      const char *buf, size_t count)
{
	unsigned long enable = simple_strtoul(buf, NULL, 10);
	struct bt_rfkill_info *bt_info = dev_get_drvdata(dev);
	if (enable == 1 || enable == 0){
		mutex_lock(&bt_info->bt_lock);
		bt_info->bt_enable = enable;
		mutex_unlock(&bt_info->bt_lock);		
		bt_set_power(bt_info, enable ? RFKILL_USER_STATE_UNBLOCKED: RFKILL_USER_STATE_SOFT_BLOCKED );
	} else
		dev_warn(dev, "[BT] input error\n");
	return count;
}

static ssize_t bt_wake_show(struct device *dev,
     						struct device_attribute *attr, char *buf)
{
	struct bt_rfkill_info *bt_info = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",  bt_info->bt_wake);
}

static ssize_t bt_wake_store(struct device *dev,
      						struct device_attribute *attr,      const char *buf, size_t count)
{
	unsigned long wake = simple_strtoul(buf, NULL, 10);
	struct bt_rfkill_info *bt_info = dev_get_drvdata(dev);

	if (wake == 1 || wake == 0) {
		mutex_lock(&bt_info->bt_lock);
		bt_info->bt_wake = wake;
		mutex_unlock(&bt_info->bt_lock);
		gpio_set_value(gpio_bt_wake, wake ? 1:0 );  
	} else
		dev_warn(dev, "[BT] input error\n");

	return count;
}

static ssize_t debug_mode_store(struct device *dev,
      						struct device_attribute *attr,      const char *buf, size_t count)
{
	unsigned long debug = simple_strtoul(buf, NULL, 10);
	printk("bt debug mode %lu\n", debug);
	debug_mode = debug;
	return count;
}

static void test_work(struct work_struct *work)
{
	static int gpio_value = 0;
	struct bt_rfkill_info *bt_info = container_of(work, struct bt_rfkill_info, test_work.work);

	mx_set_factory_test_led(gpio_value);
	gpio_value = !gpio_value;
	queue_delayed_work(bt_info->monitor_wqueue, &bt_info->test_work, msecs_to_jiffies(250));

	return;
}

static void led_delay_on_work(struct work_struct *work)
{
	mx_set_factory_test_led(1);
	return;
}

static ssize_t bt_test_mode_show(struct device *dev,
     							struct device_attribute *attr, char *buf)
{
	struct bt_rfkill_info *bt_info = dev_get_drvdata(dev);

	if(bt_info->bt_test_mode)
		return sprintf(buf, "1\n");

	mutex_lock(&bt_info->bt_lock);		
	if(mx_is_factory_test_mode(MX_FACTORY_TEST_BT)) {
		msleep(100);
		if(mx_is_factory_test_mode(MX_FACTORY_TEST_BT)) {
			printk("in BT_TEST_MODE\n");
			bt_info->bt_test_mode = 1;            							//test mode

			bt_info->monitor_wqueue = create_freezable_workqueue("wifi_test_led_wqueue");
			INIT_DELAYED_WORK_DEFERRABLE(&bt_info->test_work, test_work);
			INIT_DELAYED_WORK_DEFERRABLE(&bt_info->led_delay_on_work, led_delay_on_work);
			queue_delayed_work(bt_info->monitor_wqueue, &bt_info->led_delay_on_work, msecs_to_jiffies(BT_LED_DELAY));
		}
	}
	mutex_unlock(&bt_info->bt_lock);

	return sprintf(buf, "%d\n",  bt_info->bt_test_mode);
	
}

static ssize_t bt_test_mode_store(struct device *dev,
      						struct device_attribute *attr,      const char *buf, size_t count)
{
	unsigned long flash = simple_strtoul(buf, NULL, 10);
	struct bt_rfkill_info *bt_info = dev_get_drvdata(dev);

	if(bt_info->bt_test_mode) {
		if(flash) {
			cancel_delayed_work_sync(&bt_info->test_work);
			queue_delayed_work(bt_info->monitor_wqueue, &bt_info->test_work, 0);
		} else {
			cancel_delayed_work_sync(&bt_info->test_work);
			mx_set_factory_test_led(0);
		}
	}
	return count;
}

static DEVICE_ATTR(name, S_IRUGO|S_IWUSR|S_IWGRP,
  		 				bt_name_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
   						bt_enable_show, bt_enable_store);
static DEVICE_ATTR(wake, S_IRUGO|S_IWUSR|S_IWGRP,
		   				bt_wake_show, bt_wake_store);
static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR|S_IWGRP,
		   				NULL, debug_mode_store);
static DEVICE_ATTR(bt_test_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		   				bt_test_mode_show, bt_test_mode_store);

static struct attribute * bcm_attributes[] = {
	&dev_attr_name.attr,
	&dev_attr_enable.attr,
	&dev_attr_wake.attr,
	&dev_attr_debug.attr,
	&dev_attr_bt_test_mode.attr,
	NULL
};

static struct attribute_group bcm_attribute_group = {
	.attrs = bcm_attributes
};

static int __devinit bt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct bt_rfkill_info *bt_info;

	if(machine_is_m030()) {
		gpio_bt_power		= EXYNOS4_GPF2(3);
		gpio_wlan_power		= EXYNOS4_GPF1(5);
		gpio_bt_reset		= EXYNOS4_GPF2(1);	
		gpio_wlan_reset		= EXYNOS4_GPF1(0);
		gpio_bt_wake		= EXYNOS4_GPF3(1);
		gpio_bt_host_wake	= EXYNOS4_GPX1(1);
	} else {
		gpio_bt_power		= EXYNOS4_GPY6(7);
		gpio_wlan_power		= EXYNOS4_GPY6(3);
		gpio_bt_reset		= EXYNOS4_GPY5(5);	
		gpio_wlan_reset		= EXYNOS4_GPY5(1);
		gpio_bt_wake		= EXYNOS4_GPY6(6);
		gpio_bt_host_wake	= EXYNOS4_GPX2(4);
	}
	/* BT Host Wake IRQ */
	gpio_bt_host_wake_irq = gpio_to_irq(gpio_bt_host_wake);
	
	bt_info = kzalloc(sizeof(struct bt_rfkill_info), GFP_KERNEL);
	if(!bt_info) {
		ret = -ENOMEM;
		pr_debug("[BT]  sysfs_create_group failed\n");
		goto err_req_bt_mem;
	}
	bt_info->dev = &pdev->dev;
	platform_set_drvdata(pdev, bt_info);
	
	/* Initialize wake locks */
	wake_lock_init(&rfkill_wake_lock, WAKE_LOCK_SUSPEND, "bt_host_wake");


	/* BT Host Wake IRQ */
	ret = request_irq(gpio_bt_host_wake_irq, bt_host_wake_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"bt_host_wake_irq_handler", bt_info);

	if (ret < 0) {
		pr_err("[BT] Request_irq failed\n");
		goto err_req_irq;
	}

	disable_irq(gpio_bt_host_wake_irq);

	/* init rfkill */
	bt_info->bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bt_rfkill_ops, bt_info);

	if (!bt_info->bt_rfk) {
		pr_err("[BT] bt_rfk : rfkill_alloc is failed\n");
		ret = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_init_sw_state(bt_info->bt_rfk, 0);	

	ret = rfkill_register(bt_info->bt_rfk);
	if (ret) {
		pr_debug("********ERROR IN REGISTERING THE RFKILL********\n");
		goto err_register;
	}

	rfkill_set_sw_state(bt_info->bt_rfk, 1);
	
	/* init low power state*/
	ret = bt_lpm_init();
	if (ret < 0) {
		pr_debug("[BT]  set low power failed\n");
		goto err_register;
	}

	bt_info->bt_test_mode =0;     //bt   in normal mode
	bt_info->bt_enable = 0;
	bt_info->bt_wake = 0;
	mutex_init(&bt_info->bt_lock);	

	/* create sysfs attributes */
	ret = sysfs_create_group(&pdev->dev.kobj, &bcm_attribute_group);
	if (ret < 0) {
		pr_debug("[BT]  sysfs_create_group failed\n");
		goto err_register;
	}

	device_init_wakeup(&pdev->dev, 1);

	/* set init power state*/
	bt_set_power(NULL, RFKILL_USER_STATE_SOFT_BLOCKED);

	pr_info("[BT] driver loaded!\n");
	return ret;

err_register:
	rfkill_destroy(bt_info->bt_rfk);

err_rfkill_alloc:
	free_irq(gpio_bt_host_wake_irq, NULL);

err_req_irq:
	wake_lock_destroy(&rfkill_wake_lock);
	kfree(bt_info);

err_req_bt_mem:
	return ret;
}

static int __devexit bt_remove(struct platform_device *pdev)
{

	struct bt_rfkill_info *bt_info = platform_get_drvdata(pdev);
	
	sysfs_remove_group(&pdev->dev.kobj, &bcm_attribute_group);
	free_irq(gpio_bt_host_wake_irq, NULL);
	rfkill_unregister(bt_info->bt_rfk);
	bt_set_power(NULL, RFKILL_USER_STATE_SOFT_BLOCKED);
	if(bt_info->bt_test_mode) {
		destroy_workqueue(bt_info->monitor_wqueue);
	}
	return 0;	
}

static struct platform_driver bt_rfkill_driver = {
	.probe = bt_probe,
	.remove = __devexit_p(bt_remove),
	.driver = {
		.name = "bt_ctr",
		.owner = THIS_MODULE,
	},
};

static int __init bt_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&bt_rfkill_driver);

	return rc;
}

device_initcall(bt_init);
