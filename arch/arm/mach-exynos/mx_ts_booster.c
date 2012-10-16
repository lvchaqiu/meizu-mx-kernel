/*
 * Touchscreen boost driver for meizu mx
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: lvcha qiu <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#undef DEBUG

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/fb.h>

#include <mach/dev.h>

#define BOOST_CPU_FREQ 600000
#define BOOST_BUS_FREQ 267160

struct mx_booster_info {
	struct input_handler handler;
	struct input_handle handle;
	struct input_device_id ids[2];

	struct workqueue_struct *wq;
	struct device *bus_dev;
	struct work_struct boost_work;
	struct delayed_work  time_work;
	atomic_t boost_lock;
};

static bool boost_on = true;
module_param_named(boost_on, boost_on, bool, 0644);

static void mx_boost_timeout(struct work_struct *work)
{
	struct mx_booster_info *info = 
			list_entry(work, struct mx_booster_info, time_work.work);
	struct input_dev *dev = info->handle.dev;

	dev_unlock(info->bus_dev, &dev->dev);
	atomic_dec(&info->boost_lock);
}

static inline int mx_boost_freq(struct mx_booster_info *info)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	struct input_dev *dev = info->handle.dev;
	int ret;

	if (!policy)
		return -ENODEV;

	/* first boost cpu freq */
	if (policy->cur < BOOST_CPU_FREQ) {
		ret = cpufreq_driver_target(policy,
				BOOST_CPU_FREQ, CPUFREQ_RELATION_L);
		cpufreq_cpu_put(policy);
		if (ret) {
			pr_err("%s:failed to cpufreq_driver_target\n", __func__);
			return ret;
		}
	}

	/* second boost bus freq */
	ret = dev_lock(info->bus_dev, &dev->dev, BOOST_BUS_FREQ);

	atomic_inc(&info->boost_lock);

	return ret;
}

static int mx_ts_boost(struct mx_booster_info *info)
{
	ktime_t delta_total, rettime_total;
	long long delta_us = 0;


	if (likely(atomic_read(&info->boost_lock))) {
		if (delayed_work_pending(&info->time_work))
			cancel_delayed_work(&info->time_work);
	} else {
		rettime_total = ktime_get();

		/* Boost cpu and bus freq */
		mx_boost_freq(info);
	
		/* Adjust lcd freq to 60HZ */
#ifdef CONFIG_FB_DYNAMIC_FREQ
		fb_notifier_call_chain(FB_EVENT_MODE_PAN, NULL);
#endif
		delta_total= ktime_sub(ktime_get(),rettime_total);
		delta_us = ktime_to_us(delta_total);
		pr_debug("mxt_input_boost time = %Lu uS\n", delta_us);
	}

	return queue_delayed_work(info->wq, &info->time_work, HZ);
}

static void mx_boost_fn(struct work_struct *work)
{
	struct mx_booster_info *info=
		list_entry(work, struct mx_booster_info, boost_work);
	mx_ts_boost(info);
}

static void mx_ts_event(struct input_handle *handle, 
	unsigned int type, unsigned int code, int value)
{
	struct mx_booster_info *info =
		list_entry(handle, struct mx_booster_info, handle);

	/* To queue work when touch device and press only */
	if (type == EV_ABS && boost_on)
		queue_work(info->wq, &info->boost_work);
}

static int mx_ts_connect(struct input_handler *handler,
				    struct input_dev *dev,
				    const struct input_device_id *id)
{
	struct mx_booster_info *info = handler->private;
	int ret;

	info->handle.dev = dev;
	info->handle.handler = handler;
	info->handle.open = 0;
	info->handle.name = "mx_ts_booster";
	ret = input_register_handle(&info->handle);
	if (ret) {
		pr_err("%s: Failed to register mx_ts_booster handle, "
			"error %d\n", __func__, ret);
		goto err_reg;
	}

	ret = input_open_device(&info->handle);
	if (ret) {
		pr_err("%s: Failed to open input device, error %d\n",
			__func__, ret);
		goto err_open;
	}

	info->wq = create_workqueue("mx_ts_booster");
	if (!info->wq) {
		pr_err("%s: Failed to create_singlethread_workqueue\n", __func__);
		goto err_creat;
	}

	info->bus_dev = dev_get("exynos-busfreq");
	INIT_DELAYED_WORK(&info->time_work, mx_boost_timeout);
	INIT_WORK(&info->boost_work, mx_boost_fn);

	return 0;

err_creat:
	input_close_device(&info->handle);
err_open:
	input_unregister_handle(&info->handle);
err_reg:
	return ret;
}

static void mx_ts_disconnect(struct input_handle *handle)
{
	struct mx_booster_info *info =
		list_entry(handle, struct mx_booster_info, handle);

	destroy_workqueue(info->wq);
	input_close_device(handle);
	input_unregister_handle(handle);
}

static int  __init mx_init_booster(void)
{
	struct mx_booster_info *info;
	int ret;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/* To attach touch device only*/
	set_bit(BTN_TOUCH, info->ids[0].keybit);
	info->ids[0].flags = INPUT_DEVICE_ID_MATCH_KEYBIT;
	info->handler.connect = mx_ts_connect;
	info->handler.disconnect = mx_ts_disconnect;
	info->handler.event = mx_ts_event;
	info->handler.name = "mx_ts_booster";
	info->handler.id_table = info->ids;
	info->handler.private = info;

	ret = input_register_handler(&info->handler);

	return ret;

}
arch_initcall(mx_init_booster);

MODULE_AUTHOR("Lvcha qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("Mx Touchscreen boost driver");
MODULE_LICENSE("GPLV2");
