/* linux/drivers/video/backlight/tc358764_mipi_lcd.c
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
	unsigned char initcode_013c[6] = {0x3c, 0x01, 0x03, 0x00, 0x02, 0x00};
	unsigned char initcode_0114[6] = {0x14, 0x01, 0x02, 0x00, 0x00, 0x00};
	unsigned char initcode_0164[6] = {0x64, 0x01, 0x05, 0x00, 0x00, 0x00};
	unsigned char initcode_0168[6] = {0x68, 0x01, 0x05, 0x00, 0x00, 0x00};
	unsigned char initcode_016c[6] = {0x6c, 0x01, 0x05, 0x00, 0x00, 0x00};
	unsigned char initcode_0170[6] = {0x70, 0x01, 0x05, 0x00, 0x00, 0x00};
	unsigned char initcode_0134[6] = {0x34, 0x01, 0x1f, 0x00, 0x00, 0x00};
	unsigned char initcode_0210[6] = {0x10, 0x02, 0x1f, 0x00, 0x00, 0x00};
	unsigned char initcode_0104[6] = {0x04, 0x01, 0x01, 0x00, 0x00, 0x00};
	unsigned char initcode_0204[6] = {0x04, 0x02, 0x01, 0x00, 0x00, 0x00};
	unsigned char initcode_0450[6] = {0x50, 0x04, 0x20, 0x01, 0xfa, 0x00};
	unsigned char initcode_0454[6] = {0x54, 0x04, 0x20, 0x00, 0x50, 0x00};
	unsigned char initcode_0458[6] = {0x58, 0x04, 0x00, 0x05, 0x30, 0x00};
	unsigned char initcode_045c[6] = {0x5c, 0x04, 0x05, 0x00, 0x0a, 0x00};
	unsigned char initcode_0460[6] = {0x60, 0x04, 0x20, 0x03, 0x0a, 0x00};
	unsigned char initcode_0464[6] = {0x64, 0x04, 0x01, 0x00, 0x00, 0x00};
	unsigned char initcode_04a0_1[6] = {0xa0, 0x04, 0x06, 0x80, 0x44, 0x00};
	unsigned char initcode_04a0_2[6] = {0xa0, 0x04, 0x06, 0x80, 0x04, 0x00};
	unsigned char initcode_0504[6] = {0x04, 0x05, 0x04, 0x00, 0x00, 0x00};
	unsigned char initcode_049c[6] = {0x9c, 0x04, 0x0d, 0x00, 0x00, 0x00};

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_013c, sizeof(initcode_013c));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0114, sizeof(initcode_0114));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0164, sizeof(initcode_0164));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0168, sizeof(initcode_0168));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_016c, sizeof(initcode_016c));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0170, sizeof(initcode_0170));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0134, sizeof(initcode_0134));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0210, sizeof(initcode_0210));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0104, sizeof(initcode_0104));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0204, sizeof(initcode_0204));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0450, sizeof(initcode_0450));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0454, sizeof(initcode_0454));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0458, sizeof(initcode_0458));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_045c, sizeof(initcode_045c));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0460, sizeof(initcode_0460));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0464, sizeof(initcode_0464));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_04a0_1, sizeof(initcode_04a0_1));
	mdelay(5);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_04a0_2, sizeof(initcode_04a0_2));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_0504, sizeof(initcode_0504));
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_GENERIC_LONG_WRITE,
		(unsigned int) initcode_049c, sizeof(initcode_049c));

	mdelay(800);
}

void tc358764_mipi_lcd_off(struct mipi_dsim_device *dsim)
{
	mdelay(1);
}

static int tc358764_mipi_lcd_bl_update_status(struct backlight_device *bd)
{
	return 0;
}

static const struct backlight_ops tc358764_mipi_lcd_bl_ops = {
	.update_status = tc358764_mipi_lcd_bl_update_status,
};

static int tc358764_mipi_lcd_probe(struct mipi_dsim_device *dsim)
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
		dsim_drv->dev, dsim_drv, &tc358764_mipi_lcd_bl_ops, &props);

	return 0;
}

static int tc358764_mipi_lcd_suspend(struct mipi_dsim_device *dsim)
{
	tc358764_mipi_lcd_off(dsim);
	return 0;
}

static int tc358764_mipi_lcd_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);

	return 0;
}

static int tc358764_mipi_lcd_resume(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 0;
}

struct mipi_dsim_lcd_driver tc358764_mipi_lcd_driver = {
	.probe = tc358764_mipi_lcd_probe,
	.suspend =  tc358764_mipi_lcd_suspend,
	.displayon = tc358764_mipi_lcd_displayon,
	.resume = tc358764_mipi_lcd_resume,
};
