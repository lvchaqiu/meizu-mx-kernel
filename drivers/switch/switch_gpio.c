/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-adc.h>
#include <plat/adc.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <plat/adc.h>
#include <linux/earlysuspend.h>
#include <asm/mach-types.h>

#define HAVE_MIC_EARPHONE   1
#define NO_MIC_EARPHONE     2

#define MIC_DETECT_THRESHOLD_ADC	200

#define MIC_DETECT_TIMES			200
#define MIC_DETECT_THRESHOLD		(int)(MIC_DETECT_TIMES*0.2)  // 200*20% 

#define MIC_DETECT_NOPRESSED			80
#define MIC_DETECT_PRESSED			30

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	struct delayed_work del_work;
	struct s3c_adc_client	*client;
	int active_level;
	int irq;
	unsigned int adc_channel;
#ifdef CONFIG_EARLYSUSPEND
	struct early_suspend early_suspends;
#endif	
};

struct gpio_switch_data *switch_data;
struct input_dev *mic_input_dev;
struct timer_list detect_mic_adc_timer;
struct work_struct mic_adc_work;
static int is_adc_timer_on;
static volatile int is_key_report_on = false;
static unsigned char is_mic_key_pressed;
#ifdef CONFIG_SND_SOC_MX_WM8958
extern void set_ext_mic_bias(bool bOnOff);
extern bool get_ext_mic_bias(void);
#else
void set_ext_mic_bias(bool bOnOff)
{
}
bool get_ext_mic_bias(void)
{
	return 0;
}
#endif
static int mx_mic_read_adc(struct gpio_switch_data *switch_data)
{
	int ret = s3c_adc_read(switch_data->client,  switch_data->adc_channel);	
	return ret;
}

static inline void mic_report_key(int presse_status)
{
	static int last_mic_key_status;
	if (last_mic_key_status != presse_status ) {
		last_mic_key_status = presse_status;
		input_report_key(mic_input_dev, KEY_FORWARDMAIL, !!presse_status);
		input_sync(mic_input_dev);
		pr_debug("%s:pressed = %d\n",  __func__,last_mic_key_status);
	}
	is_mic_key_pressed = presse_status;
}

static void mic_adc_work_func(struct work_struct *work)
{
	if (!get_ext_mic_bias() || !switch_data)
		return;
	if (gpio_get_value(switch_data->gpio) == switch_data->active_level) {
		/*mic key is pressed*/
		if (mx_mic_read_adc(switch_data) <= MIC_DETECT_PRESSED) {
			msleep(10);
			if (mx_mic_read_adc(switch_data) <= MIC_DETECT_PRESSED) {
				msleep(10);
				if (mx_mic_read_adc(switch_data) <= MIC_DETECT_PRESSED) {
					msleep(10);
					if(is_key_report_on)
						mic_report_key(1);
				}
			}
		} else {
			msleep(10);
			if (mx_mic_read_adc(switch_data) > MIC_DETECT_NOPRESSED) {
				if (is_mic_key_pressed)
					mic_report_key(0);
			}
		}
	}
}

static void mic_adc_detect_timer_func(unsigned long data)
{
	schedule_work(&mic_adc_work);
	mod_timer(&detect_mic_adc_timer, jiffies + msecs_to_jiffies(40));
	return;
}

static inline int detect_earphone_type(void)
{
	int have_mic_cnt = 0;
	int no_mic_cnt = 0;
	int loop_cnt;
	int ret = NO_MIC_EARPHONE;

	if (!get_ext_mic_bias()) {
		set_ext_mic_bias(1);
		mdelay(100);
	}
		
	/*detect 200 times*/
	for (loop_cnt = 0; loop_cnt < MIC_DETECT_TIMES; loop_cnt++) 
	{
		mdelay(5);
		if (get_ext_mic_bias() &&
			mx_mic_read_adc(switch_data) > MIC_DETECT_THRESHOLD_ADC) 
		{
			++ have_mic_cnt;
			no_mic_cnt = 0;
			if (have_mic_cnt > MIC_DETECT_THRESHOLD)
			{
				ret = HAVE_MIC_EARPHONE;
				break;
			}
		} 
		else
		{
			no_mic_cnt++;
			have_mic_cnt = 0;
			if (no_mic_cnt > MIC_DETECT_THRESHOLD)
			{
				ret = NO_MIC_EARPHONE;
				break;
			}
		}
	}
	pr_info("%s, %s:%d\n",  __func__,(ret == HAVE_MIC_EARPHONE)?"have mic":"no mic",loop_cnt);
	
	return ret;
}

static void gpio_switch_work(struct work_struct *work)
{
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, del_work.work);
	int state;
	
	state = gpio_get_value(data->gpio);
	msleep(10);
	if (state != gpio_get_value(data->gpio)) {
		schedule_delayed_work(&data->del_work, msecs_to_jiffies(50));
		return;
	}

	if (state == data->active_level && !get_ext_mic_bias()) {
		set_ext_mic_bias(1);
		schedule_delayed_work(&data->del_work, msecs_to_jiffies(500));
		return;
	}
	state = (state == data->active_level);
	/*ear jack plugin*/
	if (state) {
		state = detect_earphone_type();
		if (state == HAVE_MIC_EARPHONE && !is_adc_timer_on) {
			add_timer(&detect_mic_adc_timer);
			mod_timer(&detect_mic_adc_timer,
					jiffies + msecs_to_jiffies(200));
			is_adc_timer_on = 1;
		}
	} else {
		/*ear jack take out*/
		if (is_adc_timer_on) {
			del_timer_sync(&detect_mic_adc_timer);
			is_adc_timer_on = 0;
		}
		set_ext_mic_bias(0);
		if (is_mic_key_pressed)
			mic_report_key(0);
	}
	switch_set_state(&data->sdev, state);
	is_key_report_on = true;
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
		(struct gpio_switch_data *)dev_id;
	
	is_key_report_on = false;
	
	if(work_pending(&switch_data->del_work.work))
		schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(2000));
	else		
		schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(10));
	
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}
#ifdef CONFIG_EARLYSUSPEND
static void gpio_switch_early_suspend(struct early_suspend *h)
{

}

static void gpio_switch_late_resume(struct early_suspend *h)
{
	schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(200));
}
#endif

#ifdef CONFIG_PM
static int gpio_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	disable_irq(switch_data->irq);
	cancel_delayed_work_sync(&switch_data->del_work);
	if (is_adc_timer_on) {
		del_timer(&detect_mic_adc_timer);
		is_adc_timer_on = 0;
	}
	cancel_work_sync(&mic_adc_work);
	if (is_mic_key_pressed)
		mic_report_key(0);

	return 0;
}

static int gpio_switch_resume(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);
	
	enable_irq(switch_data->irq);
	return 0;
}
#else
#define gpio_switch_suspend NULL
#define gpio_switch_resume  NULL
#endif
static int __devinit gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, switch_data);
	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_gpio_print_state;
	
	switch_data->active_level = pdata->active_level;
	switch_data->adc_channel = pdata->adc_channel;
	
	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_DELAYED_WORK(&switch_data->del_work, gpio_switch_work);
	INIT_WORK(&mic_adc_work, mic_adc_work_func);

	mic_input_dev = input_allocate_device();
	mic_input_dev->name = "mx-mic-input";
	mic_input_dev->phys = "mx-mic-input/input0";
	if (input_register_device(mic_input_dev) != 0) {
		input_free_device(mic_input_dev);
		goto err_request_irq;
	}

	set_bit(EV_KEY, mic_input_dev->evbit);
	set_bit(KEY_FORWARDMAIL, mic_input_dev->keybit);

	switch_data->client = s3c_adc_register(pdev, NULL, NULL, 0);
	if (IS_ERR(switch_data->client)) {
		dev_err(&pdev->dev, "cannot register adc\n");
		ret = PTR_ERR(switch_data->client);
		goto err_request_adc;
	}

	setup_timer(&detect_mic_adc_timer, mic_adc_detect_timer_func, 0);
	
	s3c_gpio_setpull(switch_data->gpio, S3C_GPIO_PULL_UP);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}

	ret = request_threaded_irq(switch_data->irq, NULL, gpio_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, pdev->name, switch_data);
	
	if (ret < 0)
		goto err_request_irq;

#ifdef CONFIG_EARLYSUSPEND
	switch_data->early_suspends.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	switch_data->early_suspends.suspend = gpio_switch_early_suspend;
	switch_data->early_suspends.resume= gpio_switch_late_resume;
	register_early_suspend(&switch_data->early_suspends);
#endif
	schedule_delayed_work(&switch_data->del_work, msecs_to_jiffies(10));

	return 0;

err_request_irq:
	s3c_adc_release(switch_data->client);
err_request_adc:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
	switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}
static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	if (is_adc_timer_on)
		del_timer(&detect_mic_adc_timer);
	cancel_delayed_work_sync(&switch_data->del_work);
	cancel_work_sync(&mic_adc_work);

	input_unregister_device(mic_input_dev);

	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	s3c_adc_release(switch_data->client);

	return 0;
}

static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.suspend  = gpio_switch_suspend,
	.resume   = gpio_switch_resume,
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

#ifdef CONFIG_MX_SERIAL_TYPE
late_initcall(gpio_switch_init);
#else
module_init(gpio_switch_init);
#endif
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");

