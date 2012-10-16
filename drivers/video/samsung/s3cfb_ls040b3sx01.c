/* linux/drivers/video/samsung/s3cfb_ls040b3sx01.c
 *
 *
 * Driver for meizu m9w mipi dsim lcd. D54E6PA8963
 * 
 * Copyright (C) 2010 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: 	lvcha qiu	<lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 
 *
 * Revision History
 *
 * Inital code : Oct 20 , 2010 : lvcha@meizu.com
 *
 * Revision 1.1  May 11  2012  lvcha
 * - modified initialized parameter
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#include <video/mipi_display.h>
#include <plat/mipi_dsim.h>
#include "s3cfb_ls040b3sx01.h"

static int ls040b3sx01_panel_init(struct ls040b3sx01_info *lcd)
{
	int i = 0, ret = 0;

	do {
		ret = lcd->init_param[i].size ?
				write_data(lcd, lcd->init_param[i].param,
						lcd->init_param[i].size, BTA_TIMEOUT) :
				write_cmd(lcd, lcd->init_param[i].param[0],
						lcd->init_param[i].param[1], BTA_TIMEOUT);

		if (lcd->init_param[i].delay)
			msleep(lcd->init_param[i].delay);
		i++;
	} while(!ret && lcd->init_param[i].size >= 0);

	return ret?-EAGAIN:0;
}

static int ls040b3sx01_lcd_init(struct mipi_dsim_lcd_device *mipi_dev)
{
	struct ls040b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	return ls040b3sx01_panel_init(lcd);
}

static int ls040b3sx01_remove(struct mipi_dsim_lcd_device *mipi_dev)
{
	struct ls040b3sx01_info *lcd = NULL;
	struct lcd_platform_data	*ddi_pd;

	lcd = (struct ls040b3sx01_info *)dev_get_drvdata(&mipi_dev->dev);
	ddi_pd = lcd->ddi_pd;
	if (ddi_pd->power_on)
		ddi_pd->power_on(lcd->ld, false);

	kfree(lcd);

	return dev_set_drvdata(&mipi_dev->dev,NULL);
}

static int ls040b3sx01_set_power(struct lcd_device *ld, int power)
{
	return 0;
}

static int ls040b3sx01_get_power(struct lcd_device *ld)
{
	return 0;
}

static struct lcd_ops ls040b3sx01_lcd_ops = {
	.set_power = ls040b3sx01_set_power,
	.get_power = ls040b3sx01_get_power,
};

static int ls040b3sx01_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct ls040b3sx01_info *lcd = NULL;
	struct lcd_platform_data *ddi_pd;

	lcd = kzalloc(sizeof(struct ls040b3sx01_info), GFP_KERNEL);
	if (!lcd) {
		dev_err(&dsim_dev->dev, "failed to allocate ls040b3sx01 structure.\n");
		return -ENOMEM;
	}

	lcd->dsim_dev = dsim_dev;
	lcd->ddi_pd = ddi_pd = (struct lcd_platform_data *)dsim_dev->platform_data;
	lcd->init_param = ls040b3sx01_params;

	if (IS_ERR_OR_NULL(lcd->ddi_pd))
		pr_err("%s: ddi_pd is NULL\n", __func__);

	lcd->dev = &dsim_dev->dev;

	dev_set_drvdata(&dsim_dev->dev, lcd);

	lcd->ld = lcd_device_register("ls040b3sx01", lcd->dev, lcd,
								&ls040b3sx01_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		goto err_dev_register;
	}

	/* lcd power on */
	if (ddi_pd->power_on)
		ddi_pd->power_on(lcd->ld, true);

	/* lcd reset */
	if (ddi_pd->reset)
		ddi_pd->reset(lcd->ld);

	lcd->state = LCD_DISPLAY_SLEEP_IN;

	pr_info("ls040b3sx01_probe finish\n");
	return 0;

err_dev_register:
	kfree(lcd);
	return -1;
}

static int ls040b3sx01_reset(struct mipi_dsim_lcd_device *mipi_dev)
{
	struct ls040b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	struct lcd_platform_data	*ddi_pd = lcd->ddi_pd;

	/* lcd reset */
	if (ddi_pd->reset)
		ddi_pd->reset(lcd->ld);

	return 0;
}

static void ls040b3sx01_shutdown(struct mipi_dsim_lcd_device *mipi_dev)
{
	struct ls040b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	struct lcd_platform_data	*ddi_pd = lcd->ddi_pd;
	int ret;

	lcd->state = LCD_DISPLAY_POWER_OFF;

	/* deep-standby */
	ret = write_cmd(lcd, 0x70, 0x1, BTA_NONE);	//do not need to do bta

	if (ddi_pd->power_off_delay)
		msleep(ddi_pd->power_off_delay);	/*120ms*/

	/* lcd power off */
	if (ddi_pd->power_on)
		ddi_pd->power_on(lcd->ld, false);

	return;
}

static int ls040b3sx01_suspend(struct mipi_dsim_lcd_device *mipi_dev)
{
	struct ls040b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	int ret;

	/* display off */
	ret = write_cmd(lcd, MIPI_DCS_SET_DISPLAY_OFF, 0, BTA_TIMEOUT_LONG);
	if (ret)
		return ret;

	/* sleep in */
	ret = write_cmd(lcd, MIPI_DCS_ENTER_SLEEP_MODE, 0, BTA_TIMEOUT_LONG);
	if (ret)
		return ret;

	msleep(120);

	lcd->state = LCD_DISPLAY_SLEEP_IN;

	return 0;
}

static int ls040b3sx01_resume(struct mipi_dsim_lcd_device *mipi_dev)
{
	struct ls040b3sx01_info *lcd = dev_get_drvdata(&mipi_dev->dev);
	struct lcd_platform_data	*ddi_pd = lcd->ddi_pd;
	
	pr_debug("%s: lcd->state = %d\n", __func__, lcd->state);

	switch(lcd->state) {
	case LCD_DISPLAY_POWER_OFF:
		/* lcd power on */
		if (ddi_pd->power_on)
			ddi_pd->power_on(lcd->ld, true);
		/* lcd reset */
		if (lcd->ddi_pd->reset)
			ddi_pd->reset(lcd->ld);
		break;

	case LCD_DISPLAY_DEEP_STAND_BY:
		/* lcd reset */
		if (lcd->ddi_pd->reset)
			lcd->ddi_pd->reset(lcd->ld);
		break;

	default:
		break;
	}

	return 0;
}

static struct mipi_dsim_lcd_driver ls040b3sx01_mipi_driver = {
	.name	= "ls040b3sx01",
	.id		= -1,
	.probe	= ls040b3sx01_probe,
	.init_lcd	= ls040b3sx01_lcd_init,
	.suspend	= ls040b3sx01_suspend,
	.resume	= ls040b3sx01_resume,
	.shutdown= ls040b3sx01_shutdown,
	.remove	= ls040b3sx01_remove,
	.reset_lcd= ls040b3sx01_reset,
};

static int __init ls040b3sx01_init(void)
{
	return s5p_mipi_dsi_register_lcd_driver(&ls040b3sx01_mipi_driver);
}

subsys_initcall(ls040b3sx01_init);

MODULE_AUTHOR("Lvcha qiu <lvcha@meizu.com>");
MODULE_DESCRIPTION("MIPI-DSI based ls040b3sx01 LCD Panel Driver");
MODULE_LICENSE("GPLV2");
