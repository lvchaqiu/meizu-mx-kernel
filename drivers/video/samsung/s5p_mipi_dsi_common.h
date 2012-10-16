/* linux/drivers/video/samsung/s5p_mipi_dsi_common.h
 *
 * Header file for Samsung SoC MIPI-DSI common driver.
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 * InKi Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5P_MIPI_DSI_COMMON_H
#define _S5P_MIPI_DSI_COMMON_H

int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim, unsigned int data_id,
	unsigned int data0, unsigned int data1, unsigned int bta_timeout);
int s5p_mipi_dsi_init_dsim(struct mipi_dsim_device *dsim);
int s5p_mipi_dsi_set_display_mode(struct mipi_dsim_device *dsim);
int s5p_mipi_dsi_init_link(struct mipi_dsim_device *dsim);
int s5p_mipi_dsi_disable_link(struct mipi_dsim_device *dsim);
int s5p_mipi_dsi_set_hs_enable(struct mipi_dsim_device *dsim);
int s5p_mipi_dsi_set_data_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int mode);
int s5p_mipi_dsi_enable_frame_done_int(struct mipi_dsim_device *dsim,
				unsigned int enable);
int s5p_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim);
int s5p_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim);

extern struct fb_info *registered_fb[FB_MAX] __read_mostly;

#endif /* _S5P_MIPI_DSI_COMMON_H */
