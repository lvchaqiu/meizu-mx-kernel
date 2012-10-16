/* linux/drivers/video/backlight/s6e8ab0_mipi_lcd.c
 *
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>

#include <video/mipi_display.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-dsim.h>

#include <plat/dsim.h>
#include <plat/mipi_dsi.h>

void init_lcd(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0, 0);
	mdelay(60);
	/* Exit sleep */
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		0x11, 0);
	mdelay(600);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_TURN_ON_PERIPHERAL,
		0, 0);
}

void s6e8ab0_mipi_lcd_off(struct mipi_dsim_device *dsim)
{
	mdelay(1);
}

static int s6e8ab0_mipi_lcd_bl_update_status(struct backlight_device *bd)
{
	return 0;
}

static const struct backlight_ops s6e8ab0_mipi_lcd_bl_ops = {
	.update_status = s6e8ab0_mipi_lcd_bl_update_status,
};

static int s6e8ab0_mipi_lcd_probe(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_device *dsim_drv;
	struct backlight_device *bd = NULL;
	struct backlight_properties props;

	dsim_drv = kzalloc(sizeof(struct mipi_dsim_device), GFP_KERNEL);
	if (!dsim_drv)
		return -ENOMEM;

	dsim_drv = (struct mipi_dsim_device *) dsim;

	props.max_brightness = 1;
	props.type = BACKLIGHT_PLATFORM;

	bd = backlight_device_register("pwm-backlight",
		dsim_drv->dev, dsim_drv, &s6e8ab0_mipi_lcd_bl_ops, &props);

	return 0;
}

static int s6e8ab0_mipi_lcd_suspend(struct mipi_dsim_device *dsim)
{
	s6e8ab0_mipi_lcd_off(dsim);
	return 0;
}

static int s6e8ab0_mipi_lcd_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);

	return 0;
}

static int s6e8ab0_mipi_lcd_resume(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 0;
}

struct mipi_dsim_lcd_driver s6e8ab0_mipi_lcd_driver = {
	.probe = s6e8ab0_mipi_lcd_probe,
	.suspend =  s6e8ab0_mipi_lcd_suspend,
	.displayon = s6e8ab0_mipi_lcd_displayon,
	.resume = s6e8ab0_mipi_lcd_resume,
};
