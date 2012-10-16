/*  drivers/misc/mx_jacks.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
//#define DEBUG
 
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/mx_jack.h>

#include <plat/adc.h>

#define WAKE_LOCK_TIME			(2*HZ )	/* 1 sec */
#define EAR_CHECK_LOOP_CNT	(10)
#define MAX_ZONE_LIMIT			(10)
#define MX_JACK_SAMPLE_CNT	(5)
#define MX_JACK_ADC_CH			(3)

#define SEND_KEY_CHECK_TIME_MS (100)	/* 100ms */

struct mx_jack_info {
	struct s3c_adc_client *padc;
	struct mx_jack_platform_data *pdata;
	struct delayed_work buttons_work;
	struct workqueue_struct *queue;
	struct input_dev *input_dev;
	struct wake_lock det_wake_lock;
	int det_irq;
	int pressed_code[3];
	unsigned int cur_jack_type;
	int det_status;
	bool mic_bias_on;
};

/* sysfs name HeadsetObserver.java looks for to track headset state
 */
static struct switch_dev switch_jack_detection = {
	.name = "h2w",
};

#define jack_insert(pdata) ({\
	unsigned npolarity = !pdata->det_active_high; \
	unlikely(gpio_get_value(pdata->det_gpio) ^ npolarity); \
})

static inline int mx_jack_set_bias(struct mx_jack_info *hi, bool on)
{
	struct mx_jack_platform_data *pdata = hi->pdata;
	int ret = 0;

	if (hi->mic_bias_on != on) {
		pr_debug("mx_jack_set_bias %s\n", on?"on":"off");
		ret = pdata->set_micbias_state(on);
		if (unlikely(ret < 0))
			pr_err("%s: failed to set_micbias_state\n", __func__);
		else
			hi->mic_bias_on = on;
	} else {
		pr_debug("mic_bias_on = %d already\n", on);
	}

	return ret;
}

static int mx_jack_get_adc_data(struct mx_jack_info *hi)
{
	int adc_data;
	int adc_max = 0;
	int adc_min = 0xFFFF;
	int adc_total = 0;
	int adc_retry_cnt = 0;
	int i;

	for (i = 0; i < MX_JACK_SAMPLE_CNT; i++) {
		adc_data = s3c_adc_read(hi->padc, hi->pdata->adc_channel);

		if (adc_data < 0) {
			adc_retry_cnt++;
			if (adc_retry_cnt > 10)
				return adc_data;
		}

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (MX_JACK_SAMPLE_CNT- 2);
}

static void mx_jack_set_type(struct mx_jack_info *hi, int jack_type)
{
	/* this can happen during slow inserts where we think we identified
	 * the type but then we get another interrupt and do it again
	 */
	if (jack_type == hi->cur_jack_type) {
		if (jack_type != MX_HEADSET_4POLE)
			mx_jack_set_bias(hi, false);
		return;
	}

	if (jack_type == MX_HEADSET_4POLE) {
		queue_delayed_work_on(0, hi->queue, &hi->buttons_work, 0);
	} else {
		if (cancel_delayed_work(&hi->buttons_work))
			flush_workqueue(hi->queue);
		/* micbias is left enabled for 4pole and disabled otherwise */
		mx_jack_set_bias(hi, false);
	}

	/* if user inserted ear jack slowly, different jack event can occur
	 * sometimes because irq_thread is defined IRQ_ONESHOT, detach status
	 * can be ignored sometimes so in that case, driver inform detach
	 * event to user side
	 */
	switch_set_state(&switch_jack_detection, MX_JACK_NO_DEVICE);

	hi->cur_jack_type = jack_type;
	pr_debug("%s : jack_type = %d\n", __func__, jack_type);

	switch_set_state(&switch_jack_detection, jack_type);
}

static void handle_jack_not_inserted(struct mx_jack_info *hi)
{
	mx_jack_set_type(hi, MX_JACK_NO_DEVICE);
	mx_jack_set_bias(hi, false);
}

static void determine_jack_type(struct mx_jack_info *hi)
{
	struct mx_jack_platform_data *pdata = hi->pdata;
	struct mx_jack_zone *zones = pdata->zones;
	int size = pdata->num_zones;
	int count[MAX_ZONE_LIMIT] = {0};
	int adc;
	int i;

	/* set mic bias to enable adc */
	mx_jack_set_bias(hi, true);

	while (jack_insert(pdata)) {
		adc = mx_jack_get_adc_data(hi);
		pr_debug("adc = %d\n", adc);
		if (adc < 0)
			break;

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		for (i = 0; i < size; i++) {
			if (adc <= zones[i].adc_high) {
				if (++count[i] > zones[i].check_count) {
					mx_jack_set_type(hi, zones[i].jack_type);
					return;
				}
				msleep(zones[i].delay_ms);
				break;
			}
		}
	}

	/* jack removed before detection complete */
	pr_debug("%s : jack removed before detection complete\n", __func__);
	handle_jack_not_inserted(hi);
}

/* thread run whenever the headset detect state changes (either insertion
 * or removal).
 */
static irqreturn_t mx_jack_detect_irq_thread(int irq, void *dev_id)
{
	struct mx_jack_info *hi = dev_id;
	struct mx_jack_platform_data *pdata = hi->pdata;
	unsigned npolarity = !pdata->det_active_high;
	int curr_data;
	int pre_data;
	int loopcnt;
	int check_loop_cnt = EAR_CHECK_LOOP_CNT;

	hi->det_status = true;

	/* prevent suspend to allow user space to respond to switch */
	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);

	/* debounce headset jack.  don't try to determine the type of
	 * headset until the detect state is true for a while.
	 */
	pre_data = 0;
	loopcnt = 0;
	while (true) {
		curr_data = gpio_get_value(pdata->det_gpio);
		if (pre_data == curr_data)
			loopcnt++;
		else
			loopcnt = 0;

		pre_data = curr_data;

		if (loopcnt >= check_loop_cnt) {
			if (!curr_data ^ npolarity) {
				/* jack not detected. */
				pr_debug("jack not detected\n");
				handle_jack_not_inserted(hi);
				hi->det_status = false;
				return IRQ_HANDLED;
			}
			break;
		}
		msleep(20);
	}

	/* jack presence was detected the whole time, figure out which type */
	pr_debug("jack presence was detected\n");
	determine_jack_type(hi);
	hi->det_status = false;

	return IRQ_HANDLED;
}

static inline void max_jack_relase_key(struct mx_jack_info *hi)
{
	struct mx_jack_platform_data *pdata = hi->pdata;
	int i;

	for (i = 0; i < pdata->num_buttons_zones; i++) {
		if (hi->pressed_code[i]) {
			pr_info("%s: earkey is released, keycode=%d\n",
					__func__, hi->pressed_code[i]);
			input_report_key(hi->input_dev, hi->pressed_code[i], 0);
			input_sync(hi->input_dev);
			hi->pressed_code[i] = 0;
		}
	}
}

/* thread run whenever the button of headset is pressed or released */
static void mx_jack_buttons_work(struct work_struct *work)
{
	struct mx_jack_info *hi =
		container_of(work, struct mx_jack_info, buttons_work.work);
	struct mx_jack_platform_data *pdata = hi->pdata;
	struct mx_jack_buttons_zone *btn_zones = pdata->buttons_zones;
	static int adc_prev = -1;
	int adc;
	int i;

	if (hi->det_status == false) {
		/* check button is pressed? */
		adc = mx_jack_get_adc_data(hi);
		if (adc_prev != adc) {
			pr_debug("%s: adc = %d\n", __func__, adc);
			adc_prev = adc;
		}

		for (i = 0; i < pdata->num_buttons_zones; i++) {
			if (adc >= btn_zones[i].adc_low &&
			    adc <= btn_zones[i].adc_high) {
				hi->pressed_code[i] = btn_zones[i].code;
				input_report_key(hi->input_dev, btn_zones[i].code, 1);
				input_sync(hi->input_dev);
				pr_debug("%s: earkey is pressed (adc:%d), keycode=%d\n",
						__func__, adc, btn_zones[i].code);
				break;
			} else {
				max_jack_relase_key(hi);
			}
		}
	} else {
		max_jack_relase_key(hi);
		pr_debug("in det_status....\n");
	}

	if (jack_insert(pdata))
		queue_delayed_work_on(0, hi->queue, &hi->buttons_work,
					msecs_to_jiffies(SEND_KEY_CHECK_TIME_MS));
}

static int __devinit mx_jack_probe(struct platform_device *pdev)
{
	struct mx_jack_info *hi;
	struct mx_jack_platform_data *pdata = pdev->dev.platform_data;
	int ret, i;

	pr_debug("%s : Registering jack driver\n", __func__);

	hi = kzalloc(sizeof(*hi), GFP_KERNEL);
	if (NULL == hi) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		ret = -ENOMEM;
		goto err_kzalloc;
	}
	hi->pdata = pdata;

	ret = gpio_request(pdata->det_gpio, "ear_jack_detect");
	if (ret) {
		pr_err("%s : gpio_request failed for %d\n",
		       __func__, pdata->det_gpio);
		goto err_gpio_request;
	}

	ret = switch_dev_register(&switch_jack_detection);
	if (ret < 0) {
		pr_err("%s : Failed to register switch device\n", __func__);
		goto err_switch_dev_register;
	}

	hi->input_dev = input_allocate_device();
	if (NULL == hi->input_dev) {
		pr_err("%s : failed to input_allocate_device\n", __func__);
		goto err_input_alloc;
	}
	
	set_bit(EV_KEY, hi->input_dev->evbit);
	for (i = 0; i < pdata->num_buttons_zones; i++)
		set_bit(pdata->buttons_zones[i].code, hi->input_dev->keybit);

	hi->input_dev->name = "mx-mic-input";
	ret = input_register_device(hi->input_dev);
	if (ret) {
		pr_err("%s : failed to input_register_device\n", __func__);
		goto err_input_register;
	}

	wake_lock_init(&hi->det_wake_lock, WAKE_LOCK_SUSPEND, "mx_jack_det");

	INIT_DELAYED_WORK(&hi->buttons_work, mx_jack_buttons_work);
	hi->queue = create_singlethread_workqueue("mx_jack_wq");
	if (hi->queue == NULL) {
		ret = -ENOMEM;
		pr_err("%s: Failed to create workqueue\n", __func__);
		goto err_create_wq_failed;
	}

	/* Register adc client */
	hi->padc = s3c_adc_register(pdev, NULL, NULL, 0);
	if (IS_ERR(hi->padc)) {
		dev_err(&pdev->dev, "cannot register adc\n");
		ret = PTR_ERR(hi->padc);
		goto err_register_adc;
	}

	hi->det_irq = gpio_to_irq(pdata->det_gpio);
	ret = request_threaded_irq(hi->det_irq, NULL,
				   mx_jack_detect_irq_thread,
				   IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				   IRQF_ONESHOT, "mx_headset_detect", hi);
	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}

	/* to handle insert/removal when we're sleeping in a call */
	ret = enable_irq_wake(hi->det_irq);
	if (ret) {
		pr_err("%s : Failed to enable_irq_wake.\n", __func__);
		goto err_enable_irq_wake;
	}

	dev_set_drvdata(&pdev->dev, hi);

	device_init_wakeup(&pdev->dev, true);

	/* Prove current earjack state */
	determine_jack_type(hi);

	return 0;

err_enable_irq_wake:
	free_irq(hi->det_irq, hi);
err_request_detect_irq:
	s3c_adc_release(hi->padc);
err_register_adc:
	destroy_workqueue(hi->queue);
err_create_wq_failed:
	wake_lock_destroy(&hi->det_wake_lock);
	input_unregister_device(hi->input_dev);
err_input_register:
	input_free_device(hi->input_dev);
err_input_alloc:
	switch_dev_unregister(&switch_jack_detection);
err_switch_dev_register:
	gpio_free(pdata->det_gpio);
err_gpio_request:
	kfree(hi);
err_kzalloc:
	return ret;
}

static int __devexit mx_jack_remove(struct platform_device *pdev)
{

	struct mx_jack_info *hi = dev_get_drvdata(&pdev->dev);

	pr_debug("%s :\n", __func__);
	disable_irq_wake(hi->det_irq);
	free_irq(hi->det_irq, hi);
	destroy_workqueue(hi->queue);
	input_unregister_device(hi->input_dev);
	input_free_device(hi->input_dev);
	wake_lock_destroy(&hi->det_wake_lock);
	switch_dev_unregister(&switch_jack_detection);
	gpio_free(hi->pdata->det_gpio);
	s3c_adc_release(hi->padc);
	kfree(hi);

	return 0;
}

#ifdef CONFIG_PM
static int mx_jack_prepare(struct device *dev)
{
	struct mx_jack_info *hi = dev_get_drvdata(dev);

	pr_debug("%s\n", __func__);

	disable_irq(hi->det_irq);
	cancel_delayed_work(&hi->buttons_work);

	return 0;
}

static void mx_jack_complete(struct device *dev)
{
	struct mx_jack_info *hi = dev_get_drvdata(dev);

	pr_debug("%s\n", __func__);
	enable_irq(hi->det_irq);
}

static int mx_jack_suspend(struct device *dev)
{
	struct mx_jack_info *hi = dev_get_drvdata(dev);
	int ret;

	pr_debug("%s\n", __func__);

	ret = enable_irq_wake(hi->det_irq);

	if (hi->cur_jack_type != MX_HEADSET_4POLE)
		hi->mic_bias_on = false;

	return ret;
}

static int mx_jack_resume(struct device *dev)
{
	struct mx_jack_info *hi = dev_get_drvdata(dev);
	int ret;

	pr_debug("%s\n", __func__);

	ret = disable_irq_wake(hi->det_irq);

	/* Try to detect jack type again after 1 sec */
	if (jack_insert(hi->pdata) &&
	    hi->cur_jack_type == MX_HEADSET_4POLE) {
		queue_delayed_work_on(0, hi->queue, &hi->buttons_work, 2*HZ);
		hi->mic_bias_on = true;
	}
	return 0;
}

static const struct dev_pm_ops mx_jack_dev_pm_ops = {
	.prepare= mx_jack_prepare,
	.complete = mx_jack_complete,
	.suspend	= mx_jack_suspend,
	.resume	= mx_jack_resume,
};
#endif

static struct platform_driver mx_jack_driver = {
	.probe	= mx_jack_probe,
	.remove	= __devexit_p(mx_jack_remove),
	.driver	= {
		.name = "mx_jack",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &mx_jack_dev_pm_ops,
#endif		
	},
};

static int __init mx_jack_init(void)
{
	int ret;

	ret =  platform_driver_register(&mx_jack_driver);
	if (ret)
		pr_err("%s: Failed to add mx jack driver\n", __func__);

	return ret;
}

static void __exit mx_jack_exit(void)
{
	platform_driver_unregister(&mx_jack_driver);
}

late_initcall(mx_jack_init);
module_exit(mx_jack_exit);

MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("Meizu mx Ear-Jack detection driver");
MODULE_LICENSE("GPLV2");
