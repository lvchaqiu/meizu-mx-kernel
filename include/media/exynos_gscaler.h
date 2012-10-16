/* include/media/exynos_gscaler.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS SoC Gscaler driver header
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS_GSCALER_H_
#define EXYNOS_GSCALER_H_

#include <media/exynos_mc.h>

enum gsc_cam_bus_type {
	GSC_ITU_601 = 1,
	GSC_MIPI_CSI2,
};

enum gsc_cam_port {
	CAM_PORT_A,
	CAM_PORT_B,
};

#define GSC_CLK_INV_PCLK	(1 << 0)
#define GSC_CLK_INV_VSYNC	(1 << 1)
#define GSC_CLK_INV_HREF	(1 << 2)
#define GSC_CLK_INV_HSYNC	(1 << 3)

struct i2c_board_info;

/**
 * struct exynos_gscaler_isp_info - image sensor information required for host
 *			      interace configuration.
 *
 * @board_info: pointer to I2C subdevice's board info
 * @clk_frequency: frequency of the clock the host interface provides to sensor
 * @bus_type: determines bus type, MIPI, ITU-R BT.601 etc.
 * @csi_data_align: MIPI-CSI interface data alignment in bits
 * @i2c_bus_num: i2c control bus id the sensor is attached to
 * @mux_id: FIMC camera interface multiplexer index (separate for MIPI and ITU)
 * @flags: flags defining bus signals polarity inversion (High by default)
 * @use_cam: a means of used by GSCALER
 */
struct exynos_gscaler_isp_info {
	struct i2c_board_info *board_info;
	unsigned long clk_frequency;
	const char *cam_srclk_name;
	const char *cam_clk_name;
	enum gsc_cam_bus_type bus_type;
	u16 csi_data_align;
	u16 i2c_bus_num;
	enum gsc_cam_port cam_port;
	u16 flags;
};

/**
 * struct exynos_platform_gscaler - camera host interface platform data
 *
 * @isp_info: properties of camera sensor required for host interface setup
 */
struct exynos_platform_gscaler {
	struct exynos_gscaler_isp_info *isp_info[MAX_CAMIF_CLIENTS];
	u32 active_cam_index;
	u32 num_clients;
	u32 cam_preview:1;
	u32 cam_camcording:1;
};

extern struct exynos_platform_gscaler exynos_gsc0_default_data;
extern struct exynos_platform_gscaler exynos_gsc1_default_data;
extern struct exynos_platform_gscaler exynos_gsc2_default_data;
extern struct exynos_platform_gscaler exynos_gsc3_default_data;

/**
 * exynos5_gsc_set_parent_clock() = Exynos5 setup function for parent clock.
 * @child: child clock used for gscaler
 * @parent: parent clock used for gscaler
 */
int __init exynos5_gsc_set_parent_clock(const char *child, const char *parent);

/**
 * exynos5_gsc_set_clock_rate() = Exynos5 setup function for clock rate.
 * @clk: name of clock used for gscaler
 * @clk_rate: clock_rate for gscaler clock
 */
int __init exynos5_gsc_set_clock_rate(const char *clk, unsigned long clk_rate);
#endif /* EXYNOS_GSCALER_H_ */
