/*
 * bu26507gul.c - mfd core driver for the bu26507gul
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  lvcha qiu   <lvcha@meizu.com>
 *
 * This program is not provided / owned by Maxim Integrated Products.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.c
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/bu26507.h>
#include <linux/mfd/bu26507-private.h>

/*reg list to write*/
struct bu26507_reg {
	u8 addr;
	u8 value;
};

static struct mfd_cell bu26507_devs[] = {
	{ .name = "bu26507-led", .id = 1 },
	{ .name = "bu26507-led", .id = 2 },
	{ .name = "bu26507-led", .id = 3 },
	{ .name = "bu26507-led", .id = 4 },
	{ .name = "bu26507-led", .id = 5 },
	{ .name = "bu26507-led", .id = 6 },
	{ .name = "bu26507-led", .id = 7 },
	{ .name = "bu26507-led", .id = 8 },
	{ .name = "bu26507-led", .id = 9 },
	{ .name = "bu26507-led", .id = 10 },
	{ .name = "bu26507-pwm", .id = 0 },
	{ .name = "bu26507-slope", .id = 0 },
};

static struct bu26507_reg  init_regs[] = {
	{0x7f, 0x00}, 	/* oab */
	{0x01, 0x08}, 	/* oscen */
	{0x11, 0x3f}, 	/* led1 on - led6 on */
	{0x20, 0}, 	/* pwmset, default 2*/

	{0x7f, 0x01}, 	/* Change to the led register map */
	{0x01, 0x00},	/*All leds current default 0*/
	{0x06, 0x00},
	{0x07, 0x00},
	{0x0c, 0x00},
	{0x0d, 0x00},
	{0x12, 0x00},
	{0x13, 0x00},
	{0x18, 0x00},
	{0x19, 0x00},
	{0x1e, 0x00},

	{0x7f, 0x00},	/* Change to the control register map */
	{0X21, 0x0c},   /*sync pin enable, high is led on*/
	{0x2f, 0x00}, 	/* scroll setting, useless*/
	{0x7f, 0x00}, 	/* oab */
	{0x2d, 0x04}, 	/* default pwmen enable, scroll and slope disable*/
	{0x30, 0x01}, 	/* start */
};

static int bu26507_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct bu26507_dev *bu26507 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bu26507->iolock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&bu26507->iolock);
	if (ret < 0)
		return ret;

	ret &= 0xff;
	*dest = ret;
	return 0;
}

static int bu26507_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct bu26507_dev *bu26507 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bu26507->iolock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&bu26507->iolock);
	return ret;
}


static int bu26507_init_registers(struct bu26507_dev *bu26507)
{
	int i, ret;

	for (i=0; i<ARRAY_SIZE(init_regs); i++) {
		ret = i2c_smbus_write_byte_data(bu26507->i2c, init_regs[i].addr, init_regs[i].value);
		if (ret) {
			dev_err(bu26507->dev, "failed to init reg[%d], ret = %d\n", i, ret);
			return ret;
		}
	}
	return 0;
}

static int __devinit bu26507_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct bu26507_dev *bu26507;
	struct bu26507_platform_data *pdata = i2c->dev.platform_data;
	int ret = 0;

	if (!pdata) {
		dev_err(&i2c->dev, "no platform data\n");
		return -ENODEV;
	}

	/* initialize reset pin */
	ret = gpio_request_one(pdata->reset_pin, GPIOF_OUT_INIT_HIGH, pdata->name);
	if (ret) {
		dev_err(&i2c->dev, "gpio_request_one faild, gpio = %d\n", pdata->reset_pin);
		return -ENODEV;
	}

	/* initialize sync pin, high is led on*/
	ret = gpio_request_one(pdata->sync_pin, GPIOF_OUT_INIT_HIGH, pdata->name);
	if (ret) {
		dev_err(&i2c->dev, "gpio_request_one faild, gpio = %d\n", pdata->sync_pin);
		ret = -ENODEV;
		goto err;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&i2c->dev, "i2c_check_functionality smbus byte data faild\n");
		ret = -EIO;
		goto err1;
	}

	bu26507 = devm_kzalloc(&i2c->dev,
			sizeof(struct bu26507_dev), GFP_KERNEL);
	if (!bu26507) {
		dev_err(&i2c->dev, "devm_kzalloc faild\n");
		ret = -ENOMEM;
		goto err1;
	}

	i2c_set_clientdata(i2c, bu26507);
	bu26507->dev = &i2c->dev;
	bu26507->i2c = i2c;
	bu26507->reset_pin = pdata->reset_pin;
	bu26507->sync_pin = pdata->sync_pin;
	bu26507->i2c_read = bu26507_read_reg;
	bu26507->i2c_write = bu26507_write_reg;
	mutex_init(&bu26507->iolock);

	/*default to set normal mode.*/
	ret = bu26507_init_registers(bu26507);
	if (ret) {
		dev_err(bu26507->dev, "failed to init bu26507\n");
		goto err2;
	}

	ret = mfd_add_devices(bu26507->dev, -1, bu26507_devs,
			ARRAY_SIZE(bu26507_devs), NULL, 0);
	if (ret) {
		dev_err(bu26507->dev, "mfd_add_devices failed\n");
		goto err2;
	}
	
	pr_info("%s: doned\n", __func__);
	
	return 0;
err2:
	devm_kfree(&i2c->dev,bu26507);
err1:
	gpio_free(pdata->sync_pin);
err:
	gpio_free(pdata->reset_pin);
	return ret;
}

static int __devexit bu26507_i2c_remove(struct i2c_client *i2c)
{
	struct bu26507_dev *bu26507 = i2c_get_clientdata(i2c);

	mfd_remove_devices(bu26507->dev);

	return 0;
}

static const struct i2c_device_id bu26507_i2c_id[] = {
	{ "bu26507-led", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77665_i2c_id);

#ifdef CONFIG_PM
static int bu26507_suspend(struct i2c_client *i2c, pm_message_t mesg)
{
	struct bu26507_dev *bu26507 = i2c_get_clientdata(i2c);

	/*if sync pin is low, means that led is not used in sleep mode*/
	if (!gpio_get_value(bu26507->sync_pin)) 
		gpio_set_value(bu26507->reset_pin, 0);
	
	return 0;
}

static int bu26507_resume(struct i2c_client *i2c)
{
	struct bu26507_dev *bu26507 = i2c_get_clientdata(i2c);
	
	if (!gpio_get_value(bu26507->reset_pin)) {
		gpio_set_value(bu26507->reset_pin, 1);
		return bu26507_init_registers(bu26507);
	}
	
	return 0;
}
#else
#define bu26507_suspend NULL
#define bu26507_resume NULL
#endif

static struct i2c_driver bu26507_i2c_driver = {
	.driver = {
		   .name = "bu26507-led",
		   .owner = THIS_MODULE,
	},
	.probe = bu26507_i2c_probe,
	.remove = bu26507_i2c_remove,
	.suspend = bu26507_suspend,
	.resume = bu26507_resume,
	.id_table = bu26507_i2c_id,
};

static int __init bu26507_i2c_init(void)
{
	return i2c_add_driver(&bu26507_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(bu26507_i2c_init);

static void __exit bu26507_i2c_exit(void)
{
	i2c_del_driver(&bu26507_i2c_driver);
}
module_exit(bu26507_i2c_exit);

MODULE_DESCRIPTION("bu26507 multi-function core driver");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPL");
