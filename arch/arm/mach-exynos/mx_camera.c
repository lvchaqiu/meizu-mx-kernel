/*
 * mx_camera.c - camera driver helper for m032 board
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

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/lcd.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/videodev2.h>

#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/gpio-m032.h>
#include <mach/gpio-m030.h>

#include <plat/fimc.h>
#include <plat/devs.h>
#include <plat/pd.h>
#include <plat/csis.h>
#include <plat/iic.h>

#include <media/m6mo_platform.h>
#include <media/ov7690_platform.h>

#ifdef CONFIG_VIDEO_FIMC
#ifdef CONFIG_VIDEO_OV7690
static int ov7690_cam_power(int enable)
{
	struct regulator_bulk_data supplies[2];
	int num_consumers = ARRAY_SIZE(supplies);
	unsigned int gpio;
	int ret;

	pr_info("%s():%d\n", __FUNCTION__, enable);

	if (machine_is_m030()) {
		supplies[0].supply = "cam_isp_1.8v";
		supplies[1].supply = "cam_front_2.8v";
		gpio = M030_GPIO_CAMERA1_PDN;
	} else {
		supplies[0].supply = "cam_1.8v";
		supplies[1].supply = "cam1_2.8v";
		gpio = FRONT_CAM_DOWN;
	}

	ret = regulator_bulk_get(NULL, num_consumers, supplies);
	if (ret) {
		pr_err("%s():regulator_bulk_get failed\n", __func__);
		return ret;
	}

	if (enable) {
		ret = regulator_bulk_enable(num_consumers, supplies);
	}
	else {
		ret = regulator_bulk_disable(num_consumers, supplies);
	}
	if (ret) {
		pr_err("%s():regulator_bulk_%sable failed\n", __func__, enable?"en":"dis");
		goto exit_regulator;
	}

	usleep_range(5000, 5000);

exit_regulator:
	regulator_bulk_free(num_consumers, supplies);
	
	return ret;
}

static int ov7690_init(struct device *dev)
{
	struct clk *srclk, *clk;
	unsigned int gpio;
	int ret = 0;
	
	/* source clk for MCLK*/
	srclk = clk_get(dev, "xusbxti");
	if (IS_ERR(srclk)) {
		dev_err(dev, "failed to get srclk source\n");
		return -EINVAL;
	}

	/* mclk */
	clk = clk_get(dev, "sclk_cam0");
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get mclk source\n");
		ret = -EINVAL;
		goto exit_clkget_cam;
	}

	if (clk_set_parent(clk, srclk)) {
		dev_err(dev, "unable to set parent.\n");
		ret = -EINVAL;
		goto exit_clkset_parent;
	}

	clk_set_rate(clk, 24000000);

	/* ov7690 power down pin should be initialized to high, 
	  * or the back camera can not get the i2c bus sometime
	  */
	if (machine_is_m030())
		gpio = M030_GPIO_CAMERA1_PDN;
	else
		gpio = FRONT_CAM_DOWN;

	ret = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "OV7690_PWDN");
	if (ret) {
		pr_err("%s():gpio_request failed\n", __func__);
		goto exit_clkset_parent;
	}

exit_clkset_parent:
	clk_put(clk);
exit_clkget_cam:
	clk_put(srclk);

	return ret;
}

static int ov7690_clock_on(struct device *dev, int enable)
{
	struct clk *fimc_clk, *clk;
	int ret = 0;

	/* be able to handle clock on/off only with this clock */
	fimc_clk = clk_get(&s3c_device_fimc0.dev, "fimc");
	if (IS_ERR(fimc_clk)) {
		dev_err(dev, "failed to get interface clock\n");
		return -EINVAL;
	}

	/* mclk */
	clk = clk_get(dev, "sclk_cam0");
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get mclk source\n");
		ret = -EINVAL;
		goto exit_clkget_cam;
	}

	if (enable) {
		clk_enable(fimc_clk);
		clk_enable(clk);
	}
	else {
		clk_disable(clk);
		clk_disable(fimc_clk);
	}

	clk_put(clk);

exit_clkget_cam:
	clk_put(fimc_clk);
	
	return ret;
}

static struct ov7690_platform_data ov7690_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,

	.init = ov7690_init,
	.power = ov7690_cam_power,
	.clock_on = ov7690_clock_on,
};

static struct i2c_board_info  ov7690_i2c_info = {
	I2C_BOARD_INFO("ov7690", 0x21),
	.platform_data = &ov7690_plat,
};

static struct s3c_platform_camera ov7690 = {
	.id		= CAMERA_PAR_A,
	.clk_name	= "sclk_cam0",
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.i2c_busnum	= 7,
	
	.info		= &ov7690_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= ov7690_cam_power,
};
#endif


#ifdef CONFIG_VIDEO_M6MO
int m6mo_mipi_cam_power(int enable)
{
	struct regulator_bulk_data supplies[5];
	int num_consumers = ARRAY_SIZE(supplies);
	unsigned int gpio, front_gpio;
	int ret;

	pr_info("%s():%d\n", __FUNCTION__, enable);

	if (machine_is_m030()) {
		supplies[0].supply = "cam_isp_1.8v";
		supplies[1].supply = "cam_isp_core";
		supplies[2].supply = "cam_sensor_2.7v";
		supplies[3].supply = "cam_sensor_1.2v";
		supplies[4].supply = "cam_af_2.7v";
		gpio = M030_GPIO_CAMERA0_RST;
		front_gpio = M030_GPIO_CAMERA1_PDN;
	} else {
		supplies[0].supply = "cam_1.8v";
		supplies[1].supply = "cam0_isp_1.2v";
		supplies[2].supply = "cam0_sensor_1.2v";
		supplies[3].supply = "cam0_sensor_2.7v";
		supplies[4].supply = "cam0_af_2.7v";
		gpio = BACK_CAM_RST;
		front_gpio = FRONT_CAM_DOWN;
	}
	
	ret = regulator_bulk_get(NULL, num_consumers, supplies);
	if (ret) {
		pr_err("%s():regulator_bulk_get failed\n", __func__);
		return ret;
	}

	if (enable) {
		gpio_set_value(front_gpio, 1);
		ret = regulator_bulk_enable(num_consumers, supplies);
		gpio_set_value(gpio, 1);
	}
	else {
		gpio_set_value(gpio, 0);
		ret = regulator_bulk_disable(num_consumers, supplies);
		gpio_set_value(front_gpio, 0);
	}
	if (ret) {
		pr_err("%s():regulator_bulk_%sable failed\n", __func__, enable?"en":"dis");
		goto exit_regulator;
	}

	usleep_range(5000, 5000);

exit_regulator:
	regulator_bulk_free(num_consumers, supplies);
	
	return ret;
}

static int m6mo_init(struct device *dev)
{
	struct clk *srclk, *clk;
	unsigned int gpio;
	int ret = 0;
	
	/* source clk for MCLK*/
	srclk = clk_get(dev, "xusbxti");
	if (IS_ERR(srclk)) {
		dev_err(dev, "failed to get srclk source\n");
		return -EINVAL;
	}

	/* mclk */
	clk = clk_get(dev, "sclk_cam0");
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get mclk source\n");
		ret = -EINVAL;
		goto exit_clkget_cam;
	}

	if (clk_set_parent(clk, srclk)) {
		dev_err(dev, "unable to set parent.\n");
		ret = -EINVAL;
		goto exit_clkset_parent;
	}

	clk_set_rate(clk, 24000000);

	if (machine_is_m030())
		gpio = M030_GPIO_CAMERA0_RST;
	else
		gpio = BACK_CAM_RST;

	/*m6mo reset pin shoud be initialized to low*/
	ret = gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "M6MO_RESET");
	if (ret) {
		pr_err("%s():gpio_request failed\n", __func__);
		goto exit_clkset_parent;
	}

exit_clkset_parent:
	clk_put(clk);
exit_clkget_cam:
	clk_put(srclk);

	return ret;
}

static int m6mo_clock_on(struct device *dev, int enable)
{
	struct clk *fimc_clk, *clk;
	int ret = 0;

	/* be able to handle clock on/off only with this clock */
	fimc_clk = clk_get(&s3c_device_fimc0.dev, "fimc");
	if (IS_ERR(fimc_clk)) {
		dev_err(dev, "failed to get interface clock\n");
		return -EINVAL;
	}

	/* mclk */
	clk = clk_get(dev, "sclk_cam0");
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get mclk source\n");
		ret = -EINVAL;
		goto exit_clkget_cam;
	}

	if (enable) {
		clk_enable(fimc_clk);
		clk_enable(clk);
	}
	else {
		clk_disable(clk);
		clk_disable(fimc_clk);
	}

	clk_put(clk);

exit_clkget_cam:
	clk_put(fimc_clk);
	
	return ret;
}

static struct m6mo_platform_data m6mo_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,

	.init = m6mo_init,
	.power = m6mo_mipi_cam_power,
	.clock_on = m6mo_clock_on,
};

static struct i2c_board_info m6mo_i2c_info = {
	I2C_BOARD_INFO("M6MO", 0x1f),//write 0x3e, read 0x3f,
	.platform_data = &m6mo_plat,
	.irq = CAMERA_ISP_IRQ,
};

static struct s3c_platform_camera m6mo = {
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
	.i2c_busnum	= 7,

	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.info		= &m6mo_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	/* default resol for preview kind of thing */
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= m6mo_mipi_cam_power,
};
#endif

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
	.camera		= {
#ifdef CONFIG_VIDEO_M6MO
		&m6mo,
#endif
#ifdef CONFIG_VIDEO_OV7690
		&ov7690,
#endif
	},
	.hw_ver = 0x51,
};
#endif /* CONFIG_VIDEO_FIMC */

/* I2C7 */
static struct i2c_board_info __initdata i2c_devs7[] = {
#ifdef CONFIG_VIDEO_M6MO
	[0] = {
		I2C_BOARD_INFO("M6MO", 0x1f),
		.platform_data = &m6mo_plat,
		.irq = CAMERA_ISP_IRQ,
	},
#endif
#ifdef CONFIG_VIDEO_OV7690
	[1] = {
		I2C_BOARD_INFO("ov7690", 0x21),
		.platform_data = &ov7690_plat,
	},
#endif
};

static int  __init mx_init_camera(void)
{
#ifdef CONFIG_VIDEO_M6MO
	if (machine_is_m030())
		i2c_devs7[0].irq = M030_GPIO_CAMERA0_EINT;
#endif

	s3c_i2c7_set_platdata(NULL);
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));
	
#ifdef CONFIG_VIDEO_FIMC
	s3c_fimc0_set_platdata(&fimc_plat);
	s3c_fimc1_set_platdata(NULL);
	s3c_fimc2_set_platdata(NULL);
	s3c_fimc3_set_platdata(NULL);
#ifndef CONFIG_PM_GENERIC_DOMAINS
	s3c_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif	

#ifdef CONFIG_VIDEO_FIMC_MIPI
	s3c_csis0_set_platdata(NULL);
#ifndef CONFIG_PM_GENERIC_DOMAINS
	s3c_device_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif

	return 0;
}

arch_initcall(mx_init_camera);

MODULE_DESCRIPTION("mx fimc and camera driver helper");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
