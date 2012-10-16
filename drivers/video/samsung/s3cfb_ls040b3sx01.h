/* linux/drivers/video/samsung/s3cfb_ls040b3sx01.h
 *
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
 */
 
#ifndef S3CFB_LS040B3SX01_H
#define S3CFB_LS040B3SX01_H

/*The default value is 200us when esc_clk is 20MHZ, 
  *The value double if esc_clk is 10MHZ
  */
#define BTA_NONE 0
#define BTA_TIMEOUT 200
#define  BTA_TIMEOUT_LONG 50000	/* 50ms */

#define lcd_to_master(a)		(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

#define write_cmd(lcd, cmd0, cmd1, bta) \
	lcd_to_master_ops(lcd)->cmd_write(lcd_to_master(lcd), \
					MIPI_DSI_DCS_SHORT_WRITE_PARAM, \
					cmd0, cmd1, bta)

#define write_data(lcd, array, size, bta)	\
	lcd_to_master_ops(lcd)->cmd_write(lcd_to_master(lcd),\
					MIPI_DSI_GENERIC_LONG_WRITE, \
					(unsigned int)array, size, bta)

struct ls040b3sx01_param {
	char param[16];
	int size;
	int delay;	/* delay time ms */
};

struct ls040b3sx01_info {
	struct device			*dev;
	struct lcd_device		*ld;

	struct mipi_dsim_lcd_device *dsim_dev;
	struct lcd_platform_data	*ddi_pd;
	enum {
	    LCD_DISPLAY_SLEEP_IN=0,
	    LCD_DISPLAY_DEEP_STAND_BY,
	    LCD_DISPLAY_POWER_OFF,
	} state;
	const struct ls040b3sx01_param *init_param;
};

static const struct ls040b3sx01_param ls040b3sx01_params[] = {
	{
		.param = {0xc6, 0x05},
	}, {
		.param = {MIPI_DCS_EXIT_SLEEP_MODE, 0x0},
		.delay = 10,
	}, {
		.param = {0xb0, 0x00},
	}, {
		.param = {0x99, 0x2b,0x51},
		.size = 3,
	}, {
		.param = {0x98, 0x01, 0x05, 0x06, 0x0a, 0x18, 0x0e, 0x22, 0x23,0x24},
		.size = 10,
	}, {
		.param = {0x9b, 0x02, 0x06, 0x08, 0x0a, 0x0c, 0x01},
		.size = 7,
	}, {
		.param = {0xa2, 0x00, 0x28, 0x0c, 0x05, 0xe9, 0x87, 0x66, 0x05},
		.size = 9,
	}, {
		.param = {0xa3, 0x00, 0x28, 0x0c, 0x05, 0xe9, 0x87, 0x66, 0x05},
		.size = 9,
	}, {
		.param = {0xa4, 0x04, 0x28, 0x0c, 0x05, 0xe9, 0x87, 0x66, 0x05},
		.size = 9,
	}, {
		.param = {0xa5, 0x04, 0x28, 0x0c, 0x05, 0xe9, 0x87, 0x66, 0x05},
		.size = 9,
	}, {
		.param = {0xa6, 0x02, 0x2b, 0x11, 0x46, 0x1c, 0xa9, 0x76, 0x06},
		.size = 9,
	}, {
		.param = {0xa7, 0x02, 0x2b, 0x11, 0x46, 0x1c, 0xa9, 0x76, 0x06},
		.size = 9,
	}, {
		.param = {0xb4, 0x68},
	}, {
		.param = {0xb5, 0x36, 0x03},
		.size = 3,
	}, {
		.param = {0xb6, 0x02},
	}, {
		.param = {0xb7, 0x08, 0x44, 0x06, 0x2e, 0x00, 0x00, 0x30, 0x33},
		.size = 9,
	}, {
		.param = {0xb8, 0x1f, 0x44, 0x10, 0x2e, 0x1f, 0x00, 0x30, 0x33},
		.size = 9,
	}, {
		.param = {0xb9, 0x48, 0x11, 0x01, 0x00, 0x30},
		.size = 6,
	}, {
		.param = {0xba, 0x4f, 0x11, 0x00, 0x00, 0x30},
		.size = 6,
	}, {
		.param = {0xbb, 0x11, 0x01, 0x00, 0x30},
		.size = 5,
	}, {
		.param = {0xbc, 0x06},
	}, {
		.param = {0xbf, 0x80},
	}, {
		.param = {0xb0, 0x01},
	}, {
		.param = {0xc0, 0xc8},
	}, {
		.param = {0xc2, 0x00},
	}, {
		.param = {0xc3, 0x00},
	}, {
		.param = {0xc4, 0x10},
	}, {
		.param = {0xc5, 0x20},
	}, {
		.param = {0xc8, 0x00},
	}, {
		.param = {0xca, 0x10},
	}, {
		.param = {0xcb, 0x44},
	}, {
		.param = {0xcc, 0x10},
	}, {
		.param = {0xd4, 0x00},
	}, {
		.param = {0xdc, 0x20},
	}, {
		.param = {0x96, 0x01},
	}, {
		.param = {MIPI_DCS_SET_DISPLAY_ON, 0x0},
	}, {
		.size = -1,
	}
};
#endif /* S3CFB_LS040B3SX01_H */
