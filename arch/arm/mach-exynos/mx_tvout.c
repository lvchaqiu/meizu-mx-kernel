/*
 * mx_tvout.c - tyout driver helper for mx board
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/notifier.h>
#include <linux/mhl.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/err.h>

#include <asm/mach-types.h>

#include <mach/irqs.h>
#include <mach/gpio-m032.h>
#include <mach/gpio-m030.h>

#include <plat/pd.h>
#include <plat/devs.h>
#include <plat/tvout.h>

#ifdef CONFIG_MHL_DRIVER
static int m032_mhl_power_on(struct mhl_platform_data *pdata, int enable)
{
	struct regulator_bulk_data supplies[] ={
		{.supply = "vdd_ldo26",},
		{.supply = "vdd_ldo20",},
		{.supply = "MHL_1.2V",},
	};
	int num_consumers = ARRAY_SIZE(supplies);
	int ret = 0;

	ret = regulator_bulk_get(NULL, num_consumers, supplies);
	if (ret) {
		pr_err("regulator_bulk_get failed\n");
		return ret;
	}

	ret = enable ? regulator_bulk_enable(num_consumers, supplies):
			    regulator_bulk_disable(num_consumers, supplies);
	if (ret) {
		MHLPRINTK("regulator_%sable failed\n", enable ? "en" : "dis");
		return ret;
	}

	regulator_bulk_free(num_consumers, supplies);

	return 0;
}

static int mhl_reset(struct mhl_platform_data *pdata)
{
	int gpio;
	int err;

	/* mhl wake */
	gpio = pdata->mhl_wake_pin;
	err = gpio_request_one(gpio, GPIOF_OUT_INIT_HIGH, "mhl_wake");
	mdelay(10);
	gpio_free(gpio);

	/* mhl reset */
	gpio = pdata->mhl_reset_pin;
	err = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "mhl_rst");
	mdelay(5);
	gpio_set_value(gpio, 1);
	gpio_free(gpio);

	return 0;
}

static struct mhl_platform_data m032_mhl_pd = {
	.mhl_wake_pin = MHL_WAKE,
	.mhl_reset_pin = MHL_RST,
	.mhl_irq_pin  = MHL_IRQ,
	.eint  = IRQ_EINT(12),
	.mhl_usb_irq_pin  = MHL_USB_IRQ,
	.mhl_power_on = m032_mhl_power_on,
	.reset = mhl_reset,
};

static int m030_mhl_power_on(struct mhl_platform_data *pdata, int enable)
{
	struct regulator *regulator;
	int ret = 0;

	regulator = regulator_get(NULL, "mhl_1.2v");
	if (IS_ERR(regulator)) {
		pr_err("mhl_power_on regulator_get failed");
		return -1;
	}

	ret = enable? regulator_enable(regulator) :
				regulator_disable(regulator);
	if (ret < 0) {
		pr_err("mhl_power_on regulator_%sable failed\n", enable ? "en" : "dis");
		return ret;
	}

	regulator_put(regulator);

	return ret;
}

static struct mhl_platform_data m030_mhl_pd = {
	.mhl_wake_pin = M030_GPIO_MHL_WAKE,
	.mhl_reset_pin = M030_GPIO_MHL_RST,
	.mhl_irq_pin  = M030_INT10_MHL,
	.eint  = IRQ_EINT(10),
	.mhl_usb_irq_pin  = M030_INT19_USB_MHL,
	.mhl_power_on = m030_mhl_power_on,
	.reset = mhl_reset,
};

static struct i2c_board_info __initdata i2c_devs8[] = {
	[0] = {
		I2C_BOARD_INFO("mhl_cbus", (0xCC >> 1)),
		.platform_data = &m032_mhl_pd,
	},
	[1] = {I2C_BOARD_INFO("mhl_page0", (0x76 >> 1)),},
	[2] = {I2C_BOARD_INFO("mhl_page1", (0x7E >> 1)),},
	[3] = {I2C_BOARD_INFO("mhl_page2", (0x96 >> 1)),},
	
};
#endif

#if defined(CONFIG_VIDEO_TVOUT)
static struct i2c_board_info __initdata i2c_devs1[] = {
	{I2C_BOARD_INFO("s5p_ddc", (0x74 >> 1)),},
};

static int mx_tvout_enable_power( int on)
{
	struct regulator_bulk_data supplies[2] ={
		{.supply = "HDMI_1.0V",},
		{.supply = "vdd_ldo19",},
	};
	int num_consumers = ARRAY_SIZE(supplies);
	int ret = 0;

	if (machine_is_m030()) {
		supplies[0].supply = "vdd_hdmi_1.1v";
		supplies[1].supply = "vdd_hdmi_3.3v";
	}

	ret = regulator_bulk_get(NULL, num_consumers, supplies);
	if (ret) {
		pr_err("regulator_bulk_get failed\n");
		return ret;
	}

	ret = (on) ? regulator_bulk_enable(num_consumers, supplies):
		regulator_bulk_disable(num_consumers, supplies);
	if (ret) {
		pr_err("regulator_bulk_%sable failed\n", on?"en":"dis");
		return ret;
	}
	regulator_bulk_free(num_consumers, supplies);

	return 0;
}

static struct s5p_platform_tvout __initdata mx_tvout_data={
	.enable_power = mx_tvout_enable_power,
};
static struct s5p_platform_hpd hdmi_hpd_data __initdata = {};
static struct s5p_platform_cec hdmi_cec_data __initdata = {};
#endif

static int  __init mx_init_tvout(void)
{
#ifdef CONFIG_MHL_DRIVER
	if (machine_is_m030())
		i2c_devs8[0].platform_data = &m030_mhl_pd;
	i2c_register_board_info(8, i2c_devs8, ARRAY_SIZE(i2c_devs8));
#endif

#if defined(CONFIG_VIDEO_TVOUT)
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	s5p_device_tvout.dev.parent = &exynos4_device_pd[PD_TV].dev;
	s5p_tvout_set_platdata(&mx_tvout_data);
	s5p_hdmi_hpd_set_platdata(&hdmi_hpd_data);
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#endif

	return 0;
}

arch_initcall(mx_init_tvout);

MODULE_DESCRIPTION("mx tvout driver helper");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
