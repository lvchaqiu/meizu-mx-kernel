/*
 * mx_fb.c - lcd driver helper for mx board
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
 */

#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/lcd.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/gpio-m032.h>
#include <mach/gpio-m030.h>

#include <plat/mipi_dsim.h>
#include <plat/devs.h>
#include <plat/pd.h>

 #include <../../../drivers/video/samsung/s3cfb.h>

#if defined(CONFIG_FB_S5P_MIPI_DSIM)
/* fb helper*/
static struct s3cfb_lcd mx_mipi_lcd = {
	.width = 640,
	.height = 960,
	.p_width = 56,
	.p_height = 84,
	.bpp = 24,

	.freq = 60,

	.timing = {
		.h_fp = 0x12,
		.h_bp = 0x4,
		.h_sw = 0x2,
		.v_fp = 0x6,
		.v_fpe = 0x1,
		.v_bp = 0x4,
		.v_bpe = 0x1,
		.v_sw = 0x2,
		.cmd_allow_len = 0xf,
	},

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 0,
		.inv_vsync = 0,
		.inv_vden = 0,
	},

};

static void mx_fb_cfg_gpio(struct platform_device *pdev)
{
	u32 reg;

	/* Set FIMD0 bypass */
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg |= (1<<1);
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
}

static struct s3c_platform_fb __initdata mx_fb_pd = {
	.hw_ver		= 0x70,
	.nr_wins	= 5,
	.default_win	= CONFIG_FB_S5P_DEFAULT_WINDOW,
	.swap		= FB_SWAP_HWORD | FB_SWAP_WORD,
	.lcd[0]		= &mx_mipi_lcd,
	.cfg_gpio	= mx_fb_cfg_gpio,
};

/* mipi dsi helper */
static struct mipi_dsim_config mx_dsi_config = {
	.manual_flush = false,
	.eot_disable = false,
	.auto_vertical_cnt = false,
	.hse = true,
	.hfp = false,
	.hbp = false,
	.hsa = false,
	.e_interface = DSIM_VIDEO,
	.e_virtual_ch =DSIM_VIRTUAL_CH_0,
	.e_pixel_format = DSIM_24BPP_888,
	.e_burst_mode = DSIM_BURST_SYNC_EVENT,	/*for exynos4x12*/
	.e_no_data_lane = DSIM_DATA_LANE_2,
	.e_byte_clk = DSIM_PLL_OUT_DIV8,

	.p = 3,
	.m = 125,
	.s = 1,

	.pll_stable_time = 500,
	.esc_clk = 30 * 1000000,	/* escape clk : 10MHz */
	.stop_holding_cnt = 0x07ff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */
	.e_lane_swap = DSIM_NO_CHANGE,
};

static int mx_mipi_power(struct platform_device *pdev, unsigned int enable)
{
	int ret;
	struct regulator_bulk_data supplies[2];
	int num_consumers = ARRAY_SIZE(supplies);

	if (machine_is_m030()) {
		supplies[0].supply = "mipi_core";
		supplies[1].supply = "vmipi_1.8v";
	} else {
		supplies[0].supply = "vdd_ldo8";
		supplies[1].supply = "vdd_ldo10";
	}

	ret = regulator_bulk_get(&pdev->dev, num_consumers, supplies);
	if (ret) {
		dev_err(&pdev->dev, "regulator_bulk_get failed\n");
		return ret;
	}
	ret = (enable) ?
		regulator_bulk_enable(num_consumers, supplies):
		regulator_bulk_disable(num_consumers, supplies);

	if (ret) {
		dev_err(&pdev->dev, "regulator_bulk_%sable failed\n", enable?"en":"dis");
		return ret;
	}
	regulator_bulk_free(num_consumers, supplies);

	return 0;
}

static struct s5p_platform_mipi_dsim __initdata mx_dsi_pd = {
	.lcd_panel_name	= "ls040b3sx01",
	.phy_enable	= s5p_dsim_phy_enable,
	.mipi_power	= mx_mipi_power,
	.dsim_config	= &mx_dsi_config,
	.lcd_panel_info	= &mx_mipi_lcd,

	/*
	 * the stable time of needing to write data on SFR
	 * when the mipi mode becomes LP mode.
	 */
	.delay_for_stabilization = 10,
};
#endif

#if defined(CONFIG_FB_MX_MIPI_LCD)
/* mipi lcd helper */
static int ls040b3sx01_lcd_reset(struct lcd_device *ld)
{
	int ret;
	unsigned int gpio;

	if (machine_is_m030())
		gpio = M030_GPIO_LCD_RST;
	else
		gpio = M032_LCD_RST;

	ret = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "LCD_RST");
	if (ret) {
		dev_err(&ld->dev, "gpio_request failed\n");
		return ret;
	}
	usleep_range(10000, 10000);
	gpio_set_value(gpio, 1);
	gpio_free(gpio);
	usleep_range(5000, 5000);	//wait min 5ms
	return 0;
}

static int ls040b3sx01_lcd_power (struct lcd_device *ld, int enable)
{
	int ret;
	struct regulator_bulk_data supplies[2];
	int num_consumers = ARRAY_SIZE(supplies);

	if (machine_is_m030()) {
		supplies[0].supply = "lcd_1.8v";
		supplies[1].supply = "lcd_analog";
	} else {
		supplies[0].supply = "vdd_ldo13";
		supplies[1].supply = "LCD_5.5V";
	}

	ret = regulator_bulk_get(&ld->dev, num_consumers, supplies);
	if (ret) {
		dev_err(&ld->dev, "regulator_bulk_get failed\n");
		return ret;
	}
	ret = (enable) ?
		regulator_bulk_enable(num_consumers, supplies):
		regulator_bulk_disable(num_consumers, supplies);

	if (ret) {
		dev_err(&ld->dev, "regulator_bulk_%sable failed\n", enable?"en":"dis");
		return ret;
	}
	regulator_bulk_free(num_consumers, supplies);

	usleep_range(10000, 10000);	//waiting for power stable

	return 0;
}

/* ls040b3sx01 panel. */
static struct lcd_platform_data ls040b3sx01_pd = {
	.reset			= ls040b3sx01_lcd_reset,
	.power_on		= ls040b3sx01_lcd_power,
	.power_off_delay	= 120,
	.power_on_delay	= 1,
};

static struct mipi_dsim_lcd_device mx_mipi_lcd_device = {
	.name		= "ls040b3sx01",
	.id		= -1,
	.bus_id		= 0,
	.platform_data	= (void *)&ls040b3sx01_pd,
};
#endif

static int  __init mx_init_fb(void)
{
	if (machine_is_m030()) {
		mx_dsi_config.m = 120;
		mx_dsi_config.esc_clk = 10 * 1000000;
	}

#if defined(CONFIG_FB_S5P)
	s3cfb_set_platdata(&mx_fb_pd);
#ifndef CONFIG_PM_GENERIC_DOMAINS
	s3c_device_fb.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif

#if defined(CONFIG_FB_S5P_MIPI_DSIM)
	s5p_dsim_set_platdata(&mx_dsi_pd, 0);
#ifndef CONFIG_PM_GENERIC_DOMAINS
	s5p_device_mipi_dsim0.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif

#if defined(CONFIG_FB_MX_MIPI_LCD)
	if (machine_is_m030())
		mx_mipi_lcd_device.irq = gpio_to_irq(M030_GPIO_LCD_TE);
	else
		mx_mipi_lcd_device.irq = gpio_to_irq(M032_LCD_TE);
	
	s5p_mipi_dsi_register_lcd_device(&mx_mipi_lcd_device);
#endif

	return 0;
}

arch_initcall(mx_init_fb);

MODULE_DESCRIPTION("mx fb and lcd driver helper");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
