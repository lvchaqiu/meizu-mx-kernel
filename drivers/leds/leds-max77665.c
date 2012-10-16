/*
 * leds-max77665.c - LED class driver for MAX77665 LEDs.
 *
 * Copyright (C) 2011 Samsung Electronics
 * Donggeun Kim <dg77.kim@samsung.com>
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
#include <linux/mfd/max77665.h>
#include <linux/mfd/max77665-private.h>
#include <linux/platform_device.h>

#define MAX77665_LED_FLASH_SHIFT			0
#define MAX77665_LED_FLASH_CUR_MASK		0x3f
#define MAX77665_LED1_TORCH_SHIFT			0
#define MAX77665_LED2_TORCH_SHIFT			4
#define MAX77665_LED_TORCH_CUR_MASK		0x0f

#define MAX77665_LED_FLASH_MAX_BRIGHTNESS	0x3f
#define MAX77665_LED_TORCH_MAX_BRIGHTNESS	0xf
#define MAX77665_LED_NONE_MAX_BRIGHTNESS		0


#define MAX77665_FLED2_ENABLE_TORCH_SHIFT		0
#define MAX77665_FLED1_ENABLE_TORCH_SHIFT		2
#define MAX77665_FLED2_ENABLE_FLASH_SHIFT		4
#define MAX77665_FLED1_ENABLE_FLASH_SHIFT		6

#define MAX77665_FLED_ENABLE_MASK			3
#define MAX77665_FLED_BY_PIN_FLASHEN			1
#define MAX77665_FLED_BY_PIN_TORCHEN			2
#define MAX77665_FLED_VIA_I2C					3


/* MAX77665_LED_REG_IFLASH1 */
#define MAX77665_FLASH_IOUT1		0x3F

/* MAX77665_LED_REG_IFLASH2 */
#define MAX77665_FLASH_IOUT2		0x3F

/* MAX77665_LED_REG_ITORCH */
#define MAX77665_TORCH_IOUT1		0x0F
#define MAX77665_TORCH_IOUT2		0xF0

/* MAX77665_LED_REG_ITORCHTIMER */
#define MAX77665_TORCH_TMR_DUR		0x0F
#define MAX77665_DIS_TORCH_TMR		0x40
#define MAX77665_TORCH_TMR_MODE		0x80
#define MAX77665_TORCH_TMR_MODE_ONESHOT	0x00
#define MAX77665_TORCH_TMR_MDOE_MAXTIMER	0x01

/* MAX77665_LED_REG_FLASH_TIMER */
#define MAX77665_FLASH_TMR_DUR		0x0F
#define MAX77665_FLASH_TMR_MODE		0x80
#define MAX77665_FLASH_TMR_MODE_ONESHOT	0x00
#define MAX77665_FLASH_TMR_MDOE_MAXTIMER	0x01

/* MAX77665_LED_REG_FLASH_EN */
#define MAX77665_TORCH_FLED2_EN		0x03
#define MAX77665_TORCH_FLED1_EN		0x0C
#define MAX77665_FLASH_FLED2_EN		0x30
#define MAX77665_FLASH_FLED1_EN		0xC0
#define MAX77665_TORCH_OFF		0x00
#define MAX77665_TORCH_BY_FLASHPIN	0x01
#define MAX77665_TORCH_BY_TORCHPIN	0x02
#define MAX77665_TORCH_BY_I2C		0X03
#define MAX77665_FLASH_OFF		0x00
#define MAX77665_FLASH_BY_FLASHPIN	0x01
#define MAX77665_FLASH_BY_TORCHPIN	0x02
#define MAX77665_FLASH_BY_I2C		0x03

/* MAX77665_LED_REG_VOUT_CNTL */
#define MAX77665_BOOST_FLASH_MODE	0x07
#define MAX77665_BOOST_FLASH_MODE_OFF	0x00
#define MAX77665_BOOST_FLASH_MODE_FLED1	0x01
#define MAX77665_BOOST_FLASH_MODE_FLED2	0x02
#define MAX77665_BOOST_FLASH_MODE_BOTH	0x03
#define MAX77665_BOOST_FLASH_MODE_FIXED	0x04
#define MAX77665_BOOST_FLASH_FLEDNUM	0x80
#define MAX77665_BOOST_FLASH_FLEDNUM_1	0x00
#define MAX77665_BOOST_FLASH_FLEDNUM_2	0x80

/* MAX77665_LED_REG_VOUT_FLASH1 */
#define MAX77665_BOOST_VOUT_FLASH	0x7F
#define MAX77665_BOOST_VOUT_FLASH_FROM_VOLT(mV)				\
		((mV) <= 3300 ? 0x00 :					\
		((mV) <= 5500 ? (((mV) - 3300) / 25 + 0x0C) : 0x7F))

#define MAX_FLASH_CURRENT	1000	// 1000mA(0x1f)
#define MAX_TORCH_CURRENT	250	// 250mA(0x0f)   
#define MAX_FLASH_DRV_LEVEL	63	/* 15.625 + 15.625*63 mA */
#define MAX_TORCH_DRV_LEVEL	15	/* 15.625 + 15.625*15 mA */

enum max77665_flash_id
{
	MAX77665_FLASH_LED_1,
	MAX77665_FLASH_LED_2,
	MAX77665_TORCH_LED_1,
	MAX77665_TORCH_LED_2,
	MAX77665_LED_MAX,
};

enum max77665_flash_time
{
	MAX77665_FLASH_TIME_62P5MS,
	MAX77665_FLASH_TIME_125MS,
	MAX77665_FLASH_TIME_187P5MS,
	MAX77665_FLASH_TIME_250MS,
	MAX77665_FLASH_TIME_312P5MS,
	MAX77665_FLASH_TIME_375MS,
	MAX77665_FLASH_TIME_437P5MS,
	MAX77665_FLASH_TIME_500MS,
	MAX77665_FLASH_TIME_562P5MS,
	MAX77665_FLASH_TIME_625MS,
	MAX77665_FLASH_TIME_687P5MS,
	MAX77665_FLASH_TIME_750MS,
	MAX77665_FLASH_TIME_812P5MS,
	MAX77665_FLASH_TIME_875MS,
	MAX77665_FLASH_TIME_937P5MS,
	MAX77665_FLASH_TIME_1000MS,
	MAX77665_FLASH_TIME_MAX,
};

enum max77665_torch_time
{
	MAX77665_TORCH_TIME_262MS,
	MAX77665_TORCH_TIME_524MS,
	MAX77665_TORCH_TIME_786MS,
	MAX77665_TORCH_TIME_1048MS,
	MAX77665_TORCH_TIME_1572MS,
	MAX77665_TORCH_TIME_2096MS,
	MAX77665_TORCH_TIME_2620MS,
	MAX77665_TORCH_TIME_3114MS,
	MAX77665_TORCH_TIME_4193MS,
	MAX77665_TORCH_TIME_5242MS,
	MAX77665_TORCH_TIME_6291MS,
	MAX77665_TORCH_TIME_7340MS,
	MAX77665_TORCH_TIME_9437MS,
	MAX77665_TORCH_TIME_11534MS,
	MAX77665_TORCH_TIME_13631MS,
	MAX77665_TORCH_TIME_15728MS,
	MAX77665_TORCH_TIME_MAX,
};

enum max77665_timer_mode
{
	MAX77665_TIMER_MODE_ONE_SHOT,
	MAX77665_TIMER_MODE_MAX_TIMER,
};



struct max77665_led {
	struct max77665_dev *iodev;
	struct led_classdev cdev;
	bool enabled;
	int id;
	enum max77665_led_mode led_mode;
	struct mutex mutex;
};

static struct max77665_led *g_data = NULL;

static void max77665_led_clear_mode(struct max77665_led *led,
			enum max77665_led_mode mode)
{
	struct i2c_client *client = led->iodev->i2c;
	u8 val = 0, mask = 0;
	int ret;

	switch (mode) {
	case MAX77665_FLASH_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_FLASH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_FLASH_SHIFT);
		break;
	case MAX77665_TORCH_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_TORCH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_TORCH_SHIFT);
		break;
	case MAX77665_FLASH_PIN_CONTROL_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_FLASH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_FLASH_SHIFT);
		break;
	case MAX77665_TORCH_PIN_CONTROL_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_TORCH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_TORCH_SHIFT);
		break;
	default:
		break;
	}

	if (mask) {
		ret = max77665_update_reg(client,
				MAX77665_LED_REG_FLASH_EN, val, mask);
		if (ret)
			dev_err(led->iodev->dev,
				"failed to update register(%d)\n", ret);
	}
}

static void max77665_led_set_mode(struct max77665_led *led,
			enum max77665_led_mode mode)
{
	int ret;
	struct i2c_client *client = led->iodev->i2c;
	u8 val = 0, mask = 0;

	/* First, clear the previous mode */
	max77665_led_clear_mode(led, led->led_mode);

	switch (mode) {
	case MAX77665_FLASH_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_FLASH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_FLASH_SHIFT);
		val = led->id ?
		      (MAX77665_FLED_VIA_I2C<<MAX77665_FLED2_ENABLE_FLASH_SHIFT) : (MAX77665_FLED_VIA_I2C<<MAX77665_FLED1_ENABLE_FLASH_SHIFT);
		led->cdev.max_brightness = MAX77665_LED_FLASH_MAX_BRIGHTNESS;
		break;
	case MAX77665_TORCH_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_TORCH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_TORCH_SHIFT);
		val = led->id ?
		      (MAX77665_FLED_VIA_I2C<<MAX77665_FLED2_ENABLE_TORCH_SHIFT) : (MAX77665_FLED_VIA_I2C<<MAX77665_FLED1_ENABLE_TORCH_SHIFT);
		led->cdev.max_brightness = MAX77665_LED_TORCH_MAX_BRIGHTNESS;
		break;
	case MAX77665_FLASH_PIN_CONTROL_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_FLASH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_FLASH_SHIFT);
		val = led->id ?
		      (MAX77665_FLED_BY_PIN_FLASHEN<<MAX77665_FLED2_ENABLE_FLASH_SHIFT) : (MAX77665_FLED_BY_PIN_FLASHEN<<MAX77665_FLED1_ENABLE_FLASH_SHIFT);
		led->cdev.max_brightness = MAX77665_LED_FLASH_MAX_BRIGHTNESS;
		break;
	case MAX77665_TORCH_PIN_CONTROL_MODE:
		mask = led->id ?
		      (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED2_ENABLE_TORCH_SHIFT) : (MAX77665_FLED_ENABLE_MASK<<MAX77665_FLED1_ENABLE_TORCH_SHIFT);
		val = led->id ?
		      (MAX77665_FLED_BY_PIN_TORCHEN<<MAX77665_FLED2_ENABLE_TORCH_SHIFT) : (MAX77665_FLED_BY_PIN_TORCHEN<<MAX77665_FLED1_ENABLE_TORCH_SHIFT);
		led->cdev.max_brightness = MAX77665_LED_TORCH_MAX_BRIGHTNESS;
		break;
	default:
		led->cdev.max_brightness = MAX77665_LED_NONE_MAX_BRIGHTNESS;
		break;
	}

	if (mask) {
		ret = max77665_update_reg(client,
				MAX77665_LED_REG_FLASH_EN, val, mask);
		if (ret)
			dev_err(led->iodev->dev,
				"failed to update register(%d)\n", ret);
	}

	led->led_mode = mode;
}

static int max77665_flash_setup(struct max77665_led *led)
{
	int ret = 0;
	struct i2c_client *client = led->iodev->i2c;

	ret |= max77665_write_reg(client, MAX77665_LED_REG_VOUT_CNTL,
			MAX77665_BOOST_FLASH_FLEDNUM_2 | MAX77665_BOOST_FLASH_MODE_FIXED);
	ret |= max77665_write_reg(client, MAX77665_LED_REG_VOUT_FLASH1,
			MAX77665_BOOST_VOUT_FLASH_FROM_VOLT(5000));

	// Set TORCH_TMR_DUR or FLASH_TMR_DUR
	ret |= max77665_write_reg(client, MAX77665_LED_REG_FLASH_TIMER,
		(MAX77665_FLASH_TIME_812P5MS | MAX77665_FLASH_TMR_MODE));
	ret |= max77665_write_reg(client, MAX77665_LED_REG_ITORCH_TIMER,
		(MAX77665_TORCH_TIME_3114MS | MAX77665_DIS_TORCH_TMR | MAX77665_TORCH_TMR_MODE));

	return ret;
}

static void max77665_led_enable(struct max77665_led *led, bool enable)
{
	int ret = 0;
	struct i2c_client *client = led->iodev->i2c;

	if (led->enabled == enable)
		return;

	if( enable )
	{		
		ret |= max77665_write_reg(client, MAX77665_LED_REG_VOUT_CNTL,
				MAX77665_BOOST_FLASH_FLEDNUM_2 | MAX77665_BOOST_FLASH_MODE_FIXED);
		ret |= max77665_write_reg(client, MAX77665_LED_REG_VOUT_FLASH1,
				MAX77665_BOOST_VOUT_FLASH_FROM_VOLT(5000));
	}
	else
	{		
		ret |= max77665_write_reg(client, MAX77665_LED_REG_VOUT_CNTL,0);
		ret |= max77665_write_reg(client, MAX77665_LED_REG_VOUT_FLASH1,0);
	}

	mutex_lock(&led->mutex);
	if(enable)
		max77665_led_set_mode(led,led->led_mode);
	else
		max77665_led_clear_mode(led,led->led_mode);
	mutex_unlock(&led->mutex);

	if (ret)
		dev_err(led->iodev->dev,
			"failed to update register(%d)\n", ret);
	led->enabled = enable;
}

static void max77665_led_set_current(struct max77665_led *led,
				enum led_brightness value)
{
	int ret;
	struct i2c_client *client = led->iodev->i2c;
	u8 val = 0, mask = 0, reg = 0;

	switch (led->led_mode) {
	case MAX77665_FLASH_MODE:
	case MAX77665_FLASH_PIN_CONTROL_MODE:
		val = value << MAX77665_LED_FLASH_SHIFT;
		mask = MAX77665_LED_FLASH_CUR_MASK;
		reg = led->id ? MAX77665_LED_REG_IFLASH2 : MAX77665_LED_REG_IFLASH1;
		break;
	case MAX77665_TORCH_MODE:
	case MAX77665_TORCH_PIN_CONTROL_MODE:
		val = led->id ? (value<<MAX77665_LED2_TORCH_SHIFT):(value<<MAX77665_LED1_TORCH_SHIFT);
		mask = led->id ? (MAX77665_LED_TORCH_CUR_MASK<<MAX77665_LED2_TORCH_SHIFT):(MAX77665_LED_TORCH_CUR_MASK<<MAX77665_LED1_TORCH_SHIFT);
		reg = MAX77665_LED_REG_ITORCH;
		break;
	default:
		break;
	}

	if (mask) {
		ret = max77665_update_reg(client, reg, val, mask);
		if (ret)
			dev_err(led->iodev->dev,
				"failed to update register(%d)\n", ret);
	}
}

static void max77665_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct max77665_led *led =
			container_of(led_cdev, struct max77665_led, cdev);

	if (value) {
		max77665_led_set_current(led, value);
		max77665_led_enable(led, true);
	} else {
		max77665_led_set_current(led, value);
		max77665_led_enable(led, false);
	}
}

static ssize_t max77665_led_show_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct max77665_led *led =
			container_of(led_cdev, struct max77665_led, cdev);
	ssize_t ret = 0;

	mutex_lock(&led->mutex);
	ret += sprintf(buf,"%d\n",led->led_mode);
	mutex_unlock(&led->mutex);

	return ret;
}

static ssize_t max77665_led_store_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct max77665_led *led =
			container_of(led_cdev, struct max77665_led, cdev);
	int mode;

	if(sscanf(buf,"%d",&mode) !=1)
		return -EINVAL;

	mutex_lock(&led->mutex);
	max77665_led_set_mode(led, mode);
	mutex_unlock(&led->mutex);

	return size;
}

static DEVICE_ATTR(mode, 0642, max77665_led_show_mode, max77665_led_store_mode);

static int __devinit max77665_led_probe(struct platform_device *pdev)
{
	struct max77665_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77665_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct max77665_led *led;
	int ret = 0;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "no platform data\n");
		return -ENODEV;
	}

	led = kzalloc(sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		ret = -ENOMEM;
		goto err_mem;
	}

	led->id = pdev->id;

	led->cdev.name = "flash_led";
	led->cdev.brightness_set = max77665_led_brightness_set;
	led->cdev.flags |= LED_CORE_SUSPENDRESUME;
	led->cdev.brightness = 0;
	led->iodev = iodev;

	/* initialize mode and brightness according to platform_data */
	if (pdata->led_pdata) {
		u8 mode = 0, brightness = 0;

		mode = pdata->led_pdata->mode[led->id];
		brightness = pdata->led_pdata->brightness[led->id];

		max77665_led_set_mode(led, pdata->led_pdata->mode[led->id]);

		if (brightness > led->cdev.max_brightness)
			brightness = led->cdev.max_brightness;
		max77665_led_set_current(led, brightness);
		led->cdev.brightness = brightness;
	} else {
		max77665_led_set_mode(led, MAX77665_NONE);
		max77665_led_set_current(led, 0);
	}

	max77665_flash_setup(led);
	
	mutex_init(&led->mutex);

	platform_set_drvdata(pdev, led);

	ret = led_classdev_register(&pdev->dev, &led->cdev);
	if (ret < 0)
		goto err_led;

	ret = device_create_file(led->cdev.dev, &dev_attr_mode);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"failed to create file: %d\n", ret);
		goto err_file;
	}

	g_data = led;

	dev_info(&pdev->dev,"--\n");
	return 0;

err_file:
	led_classdev_unregister(&led->cdev);
err_led:
	kfree(led);
err_mem:
	return ret;
}

static int __devexit max77665_led_remove(struct platform_device *pdev)
{
	struct max77665_led *led = platform_get_drvdata(pdev);

	device_remove_file(led->cdev.dev, &dev_attr_mode);
	led_classdev_unregister(&led->cdev);
	kfree(led);

	g_data = NULL;

	return 0;
}

static struct platform_driver max77665_led_driver = {
	.driver = {
		.name  = "max77665-led",
		.owner = THIS_MODULE,
	},
	.probe  = max77665_led_probe,
	.remove = __devexit_p(max77665_led_remove),
};

static int __init max77665_led_init(void)
{
	return platform_driver_register(&max77665_led_driver);
}
module_init(max77665_led_init);

static void __exit max77665_led_exit(void)
{
	platform_driver_unregister(&max77665_led_driver);
}
module_exit(max77665_led_exit);



/**********************************************/
/**      This interface is called by camera driver         **/
/**********************************************/


int flash_led_set_mode(int mode)
{
	if(!g_data)
		return -ENODEV;

	max77665_led_set_mode(g_data, mode);
	
	return 0;         
}
EXPORT_SYMBOL(flash_led_set_mode);

int flash_led_set_current(int cur)
{
	if(!g_data)
		return -ENODEV;

	max77665_led_set_current(g_data, cur);
	
	if (cur) max77665_led_enable(g_data, true);
	else max77665_led_enable(g_data, false);

	return 0;
}
EXPORT_SYMBOL(flash_led_set_current);

MODULE_AUTHOR("Donggeun Kim <dg77.kim@samsung.com>");
MODULE_DESCRIPTION("MAX77665 LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77665-led");
