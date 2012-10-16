/* linux/arm/arch/plat-s5p/include/plat/mipi_dsim.h
 *
 * Platform data header for Samsung SoC MIPI-DSIM.
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * InKi Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MIPI_DSIM_H
#define _MIPI_DSIM_H

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/earlysuspend.h>

#define PANEL_NAME_SIZE		(32)

enum mipi_dsim_interface_type {
	DSIM_COMMAND,
	DSIM_VIDEO
};

enum mipi_dsim_virtual_ch_no {
	DSIM_VIRTUAL_CH_0,
	DSIM_VIRTUAL_CH_1,
	DSIM_VIRTUAL_CH_2,
	DSIM_VIRTUAL_CH_3
};

enum mipi_dsim_burst_mode_type {
	DSIM_NON_BURST_SYNC_EVENT 	= 0,
	DSIM_BURST_SYNC_EVENT 		= 1,
	DSIM_NON_BURST_SYNC_PULSE 	= 2,
	DSIM_BURST 			= 3,
	DSIM_NON_VIDEO_MODE 		= 4,
};

enum mipi_dsim_no_of_data_lane {
	DSIM_DATA_LANE_1,
	DSIM_DATA_LANE_2,
	DSIM_DATA_LANE_3,
	DSIM_DATA_LANE_4
};

enum mipi_dsim_byte_clk_src {
	DSIM_PLL_OUT_DIV8,
	DSIM_EXT_CLK_DIV8,
	DSIM_EXT_CLK_BYPASS
};

enum mipi_dsim_pixel_format {
	DSIM_CMD_3BPP,
	DSIM_CMD_8BPP,
	DSIM_CMD_12BPP,
	DSIM_CMD_16BPP,
	DSIM_VID_16BPP_565,
	DSIM_VID_18BPP_666PACKED,
	DSIM_18BPP_666LOOSELYPACKED,
	DSIM_24BPP_888
};

enum dsim_lane_change {
	DSIM_NO_CHANGE = 0,
	DSIM_DATA_LANE_CHANGE = 1,
	DSIM_CLOCK_NALE_CHANGE = 2,
	DSIM_ALL_LANE_CHANGE = 3,
};

/**
 * struct mipi_dsim_config - interface for configuring mipi-dsi controller.
 *
 * @manual_flush: manual or auto flush of MD FIFO using VSYNC pulse.
 * @eot_disable: enable or disable EoT packet in HS mode.
 * @auto_vertical_cnt: specifies auto vertical count mode.
 *	in Video mode, the vertical line transition uses line counter
 *	configured by VSA, VBP, and Vertical resolution.
 *	If this bit is set to '1', the line counter does not use VSA and VBP
 *	registers.(in command mode, this variable is ignored)
 * @hse: set horizontal sync event mode.
 *	In VSYNC pulse and Vporch area, MIPI DSI master transfers only HSYNC
 *	start packet to MIPI DSI slave at MIPI DSI spec1.1r02.
 *	this bit transfers HSYNC end packet in VSYNC pulse and Vporch area
 *	(in mommand mode, this variable is ignored)
 * @hfp: specifies HFP disable mode.
 *	if this variable is set, DSI master ignores HFP area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @hbp: specifies HBP disable mode.
 *	if this variable is set, DSI master ignores HBP area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @hsa: specifies HSA disable mode.
 *	if this variable is set, DSI master ignores HSA area in VIDEO mode.
 *	(in command mode, this variable is ignored)
 * @e_interface: specifies interface to be used.(CPU or RGB interface)
 * @e_virtual_ch: specifies virtual channel number that main or
 *	sub diaplsy uses.
 * @e_pixel_format: specifies pixel stream format for main or sub display.
 * @e_burst_mode: selects Burst mode in Video mode.
 *	in Non-burst mode, RGB data area is filled with RGB data and NULL
 *	packets, according to input bandwidth of RGB interface.
 *	In Burst mode, RGB data area is filled with RGB data only.
 * @e_no_data_lane: specifies data lane count to be used by Master.
 * @e_byte_clk: select byte clock source. (it must be DSIM_PLL_OUT_DIV8)
 *	DSIM_EXT_CLK_DIV8 and DSIM_EXT_CLK_BYPASSS are not supported.
 * @pll_stable_time: specifies the PLL Timer for stability of the ganerated
 *	clock(System clock cycle base)
 *	if the timer value goes to 0x00000000, the clock stable bit of status
 *	and interrupt register is set.
 * @esc_clk: specifies escape clock frequency for getting the escape clock
 *	prescaler value.
 * @stop_holding_cnt: specifies the interval value between transmitting
 *	read packet(or write "set_tear_on" command) and BTA request.
 *	after transmitting read packet or write "set_tear_on" command,
 *	BTA requests to D-PHY automatically. this counter value specifies
 *	the interval between them.
 * @bta_timeout: specifies the timer for BTA.
 *	this register specifies time out from BTA request to change
 *	the direction with respect to Tx escape clock.
 * @rx_timeout: specifies the timer for LP Rx mode timeout.
 *	this register specifies time out on how long RxValid deasserts,
 *	after RxLpdt asserts with respect to Tx escape clock.
 *	- RxValid specifies Rx data valid indicator.
 *	- RxLpdt specifies an indicator that D-PHY is under RxLpdt mode.
 *	- RxValid and RxLpdt specifies signal from D-PHY.
 */
struct mipi_dsim_config {
	unsigned char			manual_flush;
	unsigned char			eot_disable;

	unsigned char			auto_vertical_cnt;
	unsigned char			hse;
	unsigned char			hfp;
	unsigned char			hbp;
	unsigned char			hsa;

	enum mipi_dsim_interface_type	e_interface;
	enum mipi_dsim_virtual_ch_no	e_virtual_ch;
	enum mipi_dsim_pixel_format	e_pixel_format;
	enum mipi_dsim_burst_mode_type	e_burst_mode;
	enum mipi_dsim_no_of_data_lane	e_no_data_lane;
	enum mipi_dsim_byte_clk_src	e_byte_clk;

	/*
	 * ===========================================
	 * |    P    |    M    |    S    |    MHz    |
	 * -------------------------------------------
	 * |    3    |   100   |    3    |    100    |
	 * |    3    |   100   |    2    |    200    |
	 * |    3    |    63   |    1    |    252    |
	 * |    4    |   100   |    1    |    300    |
	 * |    4    |   110   |    1    |    330    |
	 * |   12    |   350   |    1    |    350    |
	 * |    3    |   100   |    1    |    400    |
	 * |    4    |   150   |    1    |    450    |
	 * |    3    |   118   |    1    |    472    |
	 * |   12    |   250   |    0    |    500    |
	 * |    4    |   100   |    0    |    600    |
	 * |    3    |    81   |    0    |    648    |
	 * |    3    |    88   |    0    |    704    |
	 * |    3    |    90   |    0    |    720    |
	 * |    3    |   100   |    0    |    800    |
	 * |   12    |   425   |    0    |    850    |
	 * |    4    |   150   |    0    |    900    |
	 * |   12    |   475   |    0    |    950    |
	 * |    6    |   250   |    0    |   1000    |
	 * -------------------------------------------
	 */
	unsigned char			p;
	unsigned short			m;
	unsigned char			s;

	unsigned int			pll_stable_time;
	unsigned long			esc_clk;

	unsigned short			stop_holding_cnt;
	unsigned char			bta_timeout;
	unsigned short			rx_timeout;
	enum dsim_lane_change	e_lane_swap;
};

/**
 * struct mipi_dsim_device - global interface for mipi-dsi driver.
 *
 * @dev: driver model representation of the device.
 * @id: unique device id.
 * @clock: pointer to MIPI-DSI clock of clock framework.
 * @irq: interrupt number to MIPI-DSI controller.
 * @reg_base: base address to memory mapped SRF of MIPI-DSI controller.
 *	(virtual address)
 * @lock: the mutex protecting this data structure.
 * @dsim_info: infomation for configuring mipi-dsi controller.
 * @master_ops: callbacks to mipi-dsi operations.
 * @dsim_lcd_dev: pointer to activated ddi device.
 *	(it would be registered by mipi-dsi driver.)
 * @dsim_lcd_drv: pointer to activated_ddi driver.
 *	(it would be registered by mipi-dsi driver.)
 * @lcd_info: pointer to mipi_lcd_info structure.
 * @state: specifies status of MIPI-DSI controller.
 *	the status could be RESET, INIT, STOP, HSCLKEN and ULPS.
 * @resume_complete: indicates whether resume operation is completed or not.
 * @data_lane: specifiec enabled data lane number.
 *	this variable would be set by driver according to e_no_data_lane
 *	automatically.
 * @e_clk_src: select byte clock source.
 * @pd: pointer to MIPI-DSI driver platform data.
 */
struct mipi_dsim_device {
	struct device			*dev;
	int				id;
	struct resource			*res;
	struct clk			*ip_clock;
	struct clk			*phy_clock;
	unsigned int			irq;
	void __iomem			*reg_base;
	struct mutex			lock;

	struct mipi_dsim_config		*dsim_config;
	struct mipi_dsim_master_ops	*master_ops;
	struct mipi_dsim_lcd_device	*dsim_lcd_dev;
	struct mipi_dsim_lcd_driver	*dsim_lcd_drv;

	unsigned int			state;
	unsigned int			resume_complete;
	unsigned int			data_lane;
	enum mipi_dsim_byte_clk_src	e_clk_src;

	struct s5p_platform_mipi_dsim	*pd;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend mipi_early_suspend;
	struct early_suspend mipi_earler_suspend;
#endif
};

/**
 * struct s5p_platform_mipi_dsim - interface to platform data
 *	for mipi-dsi driver.
 *
 * @lcd_panel_name: specifies lcd panel name registered to mipi-dsi driver.
 *	lcd panel driver searched would be actived.
 * @dsim_config: pointer of structure for configuring mipi-dsi controller.
 * @lcd_panel_info: pointer for lcd panel specific structure.
 *	this structure specifies width, height, timing and polarity and so on.
 * @mipi_power: callback pointer for enabling or disabling mipi power.
 * @phy_enable: pointer to a callback controlling D-PHY enable/reset
 */
struct s5p_platform_mipi_dsim {
	char lcd_panel_name[PANEL_NAME_SIZE];

	struct mipi_dsim_config *dsim_config;
	void	*lcd_panel_info;
	unsigned int delay_for_stabilization;
	
	int (*mipi_power)(struct platform_device *pdev, unsigned int enable);
	int (*phy_enable)(struct platform_device *pdev, bool on);
};

/**
 * struct mipi_dsim_master_ops - callbacks to mipi-dsi operations.
 * 
 * @cmd_write: transfer command to lcd panel at LP mode.
 * @cmd_read: read command from rx register.
 * @get_dsim_frame_done: get the status that all screen data have been
 *	transferred to mipi-dsi.
 * @clear_dsim_frame_done: clear frame done status.
 * @get_fb_frame_done: get frame done status of display controller.
 * @trigger: trigger display controller.
 *	- this one would be used only in case of CPU mode.
 */

struct mipi_dsim_master_ops {
	int (*cmd_write)(struct mipi_dsim_device *dsim, unsigned int data_id,
		unsigned int data0, unsigned int data1, unsigned int bta_timeout);
	int (*cmd_read)(struct mipi_dsim_device *dsim, unsigned int data_id,
		unsigned int data0, unsigned int data1);
	int (*get_dsim_frame_done)(struct mipi_dsim_device *dsim);
	int (*clear_dsim_frame_done)(struct mipi_dsim_device *dsim);
	int (*get_fb_frame_done)(struct fb_info *info);
	void (*trigger)(struct fb_info *info);
};

/**
 * device structure for mipi-dsi based lcd panel.
 *
 * @name: name of the device to use with this device, or an
 *	alias for that name.
 * @dev: driver model representation of the device.
 * @id: id of device to be registered.
 * @bus_id: bus id for identifing connected bus
 *	and this bus id should be same as id of mipi_dsim_device.
 * @irq: irq number for signaling when framebuffer transfer of
 * 	lcd panel module is completed.
 *	this irq would be used only for MIPI-DSI based CPU mode lcd panel.
 * @master: pointer to mipi-dsi master device object.
 * @platform_data: lcd panel specific platform data.
 */
struct mipi_dsim_lcd_device {
	char			*name;
	struct device	dev;
	int			id;
	int			bus_id;
	int			irq;

	struct mipi_dsim_device *master;
	void			*platform_data;
};

/**
 * driver structure for mipi-dsi based lcd panel.
 *
 * this structure should be registered by lcd panel driver.
 * mipi-dsi driver seeks lcd panel registered through name field
 * and calls these callback functions in appropriate time.
 *
 * @name: name of the driver to use with this device, or an
 *	alias for that name.
 * @id: id of driver to be registered.
 *	this id would be used for finding device object registered.
 */
struct mipi_dsim_lcd_driver {
	char			*name;
	int			id;

	int	(*probe)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*init_lcd)(struct mipi_dsim_lcd_device *mipi_dev);
	int	(*reset_lcd)(struct mipi_dsim_lcd_device *mipi_dev);
	int	(*remove)(struct mipi_dsim_lcd_device *dsim_dev);
	void	(*shutdown)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*suspend)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*resume)(struct mipi_dsim_lcd_device *dsim_dev);
	
};

/**
 * register mipi_dsim_lcd_driver object defined by lcd panel driver
 * to mipi-dsi driver.
 */
int s5p_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver
						*lcd_drv);

/**
 * register mipi_dsim_lcd_device to mipi-dsi master.
 */
int s5p_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device
						*lcd_dev);

/**
 * enable regulators to MIPI-DSI power.
 */
int s5p_mipi_dsi_dphy_power(struct mipi_dsim_device *dsim,
					unsigned int enable);

/**
 * s5p_dsim_phy_enable - global MIPI-DSI receiver D-PHY control
 * @pdev: MIPI-DSIM platform device
 * @on: true to enable D-PHY and deassert its reset
 *	false to disable D-PHY
 */
int s5p_dsim_phy_enable(struct platform_device *pdev, bool on);

/**
  *
  */
void  s5p_dsim_set_platdata(struct s5p_platform_mipi_dsim *pd, int id);

#endif /* _MIPI_DSIM_H */
