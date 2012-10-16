/*
 * leds-bu26507.c - LED class driver for bu26507 LEDs.
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  jerry mo <jerrymo@meizu.com>
 *		 lvcha qiu  <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
 
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/mfd/bu26507.h>
#include <linux/mfd/bu26507-private.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#define BU26507_MAX_CURRENT 15	/*led current level, from 0~15*/
#define BU26507_MAX_PWM 63		/*led pwm level, from 0~63*/
#define BU26507_MAX_LED 10
#define BU26507_DEFAULT_CURRENT 1
#define BU26507_DEFAULT_PWM 2

/*default we set the slope cycle to 3 second, if slope is disable, this setting doesn't work*/
#define LED_REG_VAL(x) (x | SLOPE_CYCLE_3)
/*whether slope is enable or not, pwm is enabled all the time*/
#define SLOPE_ENABLE_VAL (SLOPE_EN | SLOPE_QUARTER | PWN_EN)
#define SLOPE_DISABLE_VAL (SLOPE_NONE | PWN_EN)

/*led private data*/
struct bu26507_led {
	struct bu26507_dev *iodev;
	struct led_classdev cdev;
	struct mutex mutex;
	int id;
	int pd_id;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

/*all led register address*/
static enum bu26507_reg_data led_addr_array[BU26507_MAX_LED] = {
	REG_LED0,
	REG_LED1,
	REG_LED2,
	REG_LED3,
	REG_LED4,
	REG_LED5,
	REG_LED6,
	REG_LED7,
	REG_LED8,
	REG_LED9
};

static int bu26507_set_led_pwm(struct bu26507_led *led, int pwm)
{
	struct bu26507_dev *bu26507 = led->iodev;
	int ret;

	/*check pwm value*/
	if (pwm < 0 || pwm > BU26507_MAX_PWM) {
		pr_err("%s():invalid value %d\n", __func__, pwm);
		return -1;
	}

	ret = bu26507->i2c_write(bu26507->i2c, REG_LED_PWM, (u8)pwm);

	/*if pwm is zero, set sync pin to low*/
	gpio_set_value(bu26507->sync_pin, !!pwm);
	
	return ret;
}

static int bu26507_set_led_current(struct bu26507_led *led, int cur)
{
	int ret = 0;
	struct bu26507_dev *bu26507 = led->iodev;

	/*check led_current value, here*/
	if (cur < 0 || cur > BU26507_MAX_CURRENT) {
		pr_err("%s():invalid brightness %d\n", __func__, cur);
		return -1;
	}

	/* change to led registers map */
	ret = bu26507->i2c_write(bu26507->i2c, REG_REGISTER_MAP, RMCG_AB);
	if (ret < 0) {
		pr_err("%s: change to led registers map fail!\n", __func__);
		return ret;
	}
	usleep_range(50, 60);

	ret = bu26507->i2c_write(bu26507->i2c, led_addr_array[led->id], LED_REG_VAL(cur));
	if (ret < 0)   /* don't return but try to recover the register map instead */
		pr_err("%s: change led setting fail!\n", __func__); 
	else
		usleep_range(50, 60);
	
	/* recovery back to control registers map and light on the leds */
	ret = bu26507->i2c_write(bu26507->i2c, REG_REGISTER_MAP, RMCG_CONTROL);
	if (ret < 0)
		pr_err("%s: recovery registers map fail!\n", __func__);
	else 
		usleep_range(50, 60);

	return ret;
}

static int bu26507_set_led_slope(struct bu26507_led *led, int enable)
{
	int ret;
	struct bu26507_dev *bu26507 = led->iodev;

	/*stop all leds first*/
	ret = bu26507->i2c_write(bu26507->i2c, REG_MATRIX_CONTROL, MATRIX_LED_STOP);
	if (ret < 0) {
		pr_err("%s: stop all leds fail!\n", __func__);
		return ret;
	}

	if (enable)
		ret = bu26507->i2c_write(bu26507->i2c, REG_SETTING, SLOPE_ENABLE_VAL);
	else 
		ret = bu26507->i2c_write(bu26507->i2c, REG_SETTING, SLOPE_DISABLE_VAL);

	if (ret < 0)
		pr_err("%s: setting REG_SETTING fail!\n", __func__);

	/*start all leds again*/
	ret = bu26507->i2c_write(bu26507->i2c, REG_MATRIX_CONTROL, MATRIX_LED_START);
	if (ret < 0)
		pr_err("%s: start all leds fail!\n", __func__);
	
	return ret;
}

static void bu26507_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct bu26507_led *led =
			container_of(led_cdev, struct bu26507_led, cdev);

	switch (led->pd_id) {
	case bu26507_LED:
		bu26507_set_led_current(led, value);
		break;
	case bu26507_PWM:
		bu26507_set_led_pwm(led, value);
		break;
	case bu26507_SLOPE:
		bu26507_set_led_slope(led, !!value);
		break;
	default:
		break;
	}
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void bu26507_early_suspend(struct early_suspend *h)
{
	struct bu26507_led *led =
			container_of(h, struct bu26507_led, early_suspend);

	switch (led->pd_id) {
	case bu26507_PWM:  /*we just handle pwm, or it may be cause some error*/
		if (led->cdev.brightness) bu26507_set_led_pwm(led, 0);
		break;
	default:
		break;
	}
}

static void bu26507_late_resume(struct early_suspend *h)
{
	struct bu26507_led *led =
			container_of(h, struct bu26507_led, early_suspend);

	switch (led->pd_id) {
	case bu26507_PWM:
		if (led->cdev.brightness) bu26507_set_led_pwm(led, led->cdev.brightness);
		break;
	default:
		break;
	}
}
#endif

static int __devinit bu26507_led_probe(struct platform_device *pdev)
{
	struct bu26507_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct bu26507_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct bu26507_led *led;
	char name[20];
	int ret;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		ret = -ENODEV;
	}

	led = kzalloc(sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		ret = -ENOMEM;
		goto err_mem;
	}

	led->pd_id = platform_get_device_id(pdev)->driver_data;
	led->id = pdev->id;
	
	pr_debug("%s():pd_id = %d, id = %d\n", __func__, led->pd_id, led->id);

	led->cdev.brightness_set = bu26507_led_brightness_set;
	led->cdev.brightness = 0;
	led->iodev = iodev;

	if (bu26507_LED == led->pd_id) {
		snprintf(name, sizeof(name), "%s%d", pdev->name, pdev->id);
		led->cdev.name = name;
		if (pdata->led_on[led->id]) {
			bu26507_set_led_current(led, BU26507_DEFAULT_CURRENT);
			led->cdev.brightness = BU26507_DEFAULT_CURRENT;
		} else {
			led->cdev.brightness = 0;
		}
	} else if (bu26507_PWM) {
		led->cdev.name = pdev->name;
		bu26507_set_led_pwm(led, BU26507_DEFAULT_PWM);
		led->cdev.brightness = BU26507_DEFAULT_PWM;
	} else {
		led->cdev.name = pdev->name;
		led->cdev.brightness = 0;
	}

	mutex_init(&led->mutex);
	platform_set_drvdata(pdev, led);

	ret = led_classdev_register(&pdev->dev, &led->cdev);
	if (ret < 0)
		goto err_register_led;

#ifdef CONFIG_HAS_EARLYSUSPEND
	led->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	led->early_suspend.suspend = bu26507_early_suspend;
	led->early_suspend.resume = bu26507_late_resume;
	register_early_suspend(&led->early_suspend);
#endif

	return 0;

err_register_led:
	kfree(led);
err_mem:
	return ret;
}

static int __devexit bu26507_led_remove(struct platform_device *pdev)
{
	struct bu26507_led *led = platform_get_drvdata(pdev);

	unregister_early_suspend(&led->early_suspend);
	led_classdev_unregister(&led->cdev);
	kfree(led);

	return 0;
}

const struct platform_device_id bu26507_id[] = {
	{
		.name		= "bu26507-led",
		.driver_data	= bu26507_LED,
	}, {
		.name		= "bu26507-pwm",
		.driver_data	= bu26507_PWM,
	}, {
		.name		= "bu26507-slope",
		.driver_data	= bu26507_SLOPE,
	},
	{},
};

static struct platform_driver bu26507_led_driver = {
	.driver = {
		.name  = "bu26507-led",
		.owner = THIS_MODULE,
	},
	.probe  = bu26507_led_probe,
	.remove = __devexit_p(bu26507_led_remove),
	.id_table = bu26507_id,
};

static int __init bu26507_led_init(void)
{
	return platform_driver_register(&bu26507_led_driver);
}
module_init(bu26507_led_init);

static void __exit bu26507_led_exit(void)
{
	platform_driver_unregister(&bu26507_led_driver);
}
module_exit(bu26507_led_exit);

MODULE_AUTHOR("Jerry mo <jerrymo@meizu.com>, Lvcha qiu <lvcha qiu@meizu.com>");
MODULE_DESCRIPTION("bu26507 LED driver");
MODULE_LICENSE("GPLV2");
MODULE_ALIAS("platform:bu26507-led");
