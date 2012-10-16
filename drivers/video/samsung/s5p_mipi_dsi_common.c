/* linux/drivers/video/samsung/s5p_mipi_dsi_common.c
 *
 * Samsung MIPI-DSI common driver.
 *
 * InKi Dae, <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <video/mipi_display.h>

#include <plat/regs-mipidsim.h>

#include <mach/map.h>
#include <plat/mipi_dsim.h>

#include "s3cfb.h"
#include "s5p_mipi_dsi_lowlevel.h"

#define MHZ			(1000 * 1000)
#define FIN_HZ		(24 * MHZ)

#define DFIN_PLL_MIN_HZ		(6 * MHZ)
#define DFIN_PLL_MAX_HZ		(12 * MHZ)

#define DFVCO_MIN_HZ		(500 * MHZ)
#define DFVCO_MAX_HZ		(1000 * MHZ)

#define TRY_GET_FIFO_TIMEOUT	(5000 * 2)

#if defined(CONFIG_ARCH_EXYNOS4)
#define DSIM_ESCCLK_ON		(0x1)
#define DSIM_ESCCLK_OFF		(0x0)
#else
#define DSIM_ESCCLK_ON		(0x0)
#define DSIM_ESCCLK_OFF		(0x1)
#endif

/* MIPI-DSIM status types. */
enum {
	DSIM_STATE_INIT,	/* should be initialized. */
	DSIM_STATE_STOP,	/* CPU and LCDC are LP mode. */
	DSIM_STATE_HSCLKEN,	/* HS clock was enabled. */
	DSIM_STATE_ULPS,
};

/* define DSI lane types. */
enum {
	DSIM_LANE_CLOCK = (1 << 0),
	DSIM_LANE_DATA0 = (1 << 1),
	DSIM_LANE_DATA1 = (1 << 2),
	DSIM_LANE_DATA2 = (1 << 3),
	DSIM_LANE_DATA3 = (1 << 4),
};

static unsigned int dpll_table[15] = {
	100, 120, 170, 220, 270,
	320, 390, 450, 510, 560,
	640, 690, 770, 870, 950 };

static void s5p_mipi_dsi_long_data_wr(struct mipi_dsim_device *dsim,
		unsigned int data0, unsigned int data1)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < data1; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((data1 - data_cnt) < 4) {
			if ((data1 - data_cnt) == 3) {
				payload = *(u8 *)(data0 + data_cnt) |
				    (*(u8 *)(data0 + (data_cnt + 1))) << 8 |
					(*(u8 *)(data0 + (data_cnt + 2))) << 16;
			dev_dbg(dsim->dev, "count = 3 payload = %x, %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)));
			} else if ((data1 - data_cnt) == 2) {
				payload = *(u8 *)(data0 + data_cnt) |
					(*(u8 *)(data0 + (data_cnt + 1))) << 8;
			dev_dbg(dsim->dev,
				"count = 2 payload = %x, %x %x\n", payload,
				*(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)));
			} else if ((data1 - data_cnt) == 1) {
				payload = *(u8 *)(data0 + data_cnt);
			}

			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		/* send 4bytes per one time. */
		} else {
			payload = *(u8 *)(data0 + data_cnt) |
				(*(u8 *)(data0 + (data_cnt + 1))) << 8 |
				(*(u8 *)(data0 + (data_cnt + 2))) << 16 |
				(*(u8 *)(data0 + (data_cnt + 3))) << 24;

			dev_dbg(dsim->dev,
				"count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)),
				*(u8 *)(data0 + (data_cnt + 3)));

			s5p_mipi_dsi_wr_tx_data(dsim, payload);
		}
	}
}

int s5p_mipi_dsi_wr_data(struct mipi_dsim_device *dsim, unsigned int data_id,
	unsigned int data0, unsigned int data1, unsigned int bta_timeout)
{
	unsigned int timeout = TRY_GET_FIFO_TIMEOUT;
	int ret = 0;

	if (dsim->state == DSIM_STATE_ULPS) {
		dev_err(dsim->dev, "state is ULPS.\n");
		return -EINVAL;
	}

	/* only if transfer mode is LPDT, wait SFR becomes empty. */
	if (dsim->state == DSIM_STATE_STOP) {
		while (!(s5p_mipi_dsi_get_fifo_state(dsim) &
				SFR_HEADER_EMPTY)) {
			if ((timeout--) > 0)
				mdelay(1);
			else {
				dev_err(dsim->dev,
					"SRF header fifo is not empty.\n");
				return -EINVAL;
			}
		}
	}

	switch (data_id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data0, data1);
		break;

	/* general command */
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data0, data1);
		break;


	/* packet types for video data */
	case MIPI_DSI_V_SYNC_START:
	case MIPI_DSI_V_SYNC_END:
	case MIPI_DSI_H_SYNC_START:
	case MIPI_DSI_H_SYNC_END:
	case MIPI_DSI_END_OF_TRANSMISSION:
		return 0;

	/* short and response packet types for command */
	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		s5p_mipi_dsi_clear_all_interrupt(dsim);
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data0, data1);
		/* process response func should be implemented. */
		return 0;

	/* long packet type and null packet */
	case MIPI_DSI_NULL_PACKET:
	case MIPI_DSI_BLANKING_PACKET:
		return 0;

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
		do {
		unsigned int size, data_cnt = 0, payload = 0;

		size = data1 * 4;

		/* if data count is less then 4, then send 3bytes data.  */
		if (data1 < 4) {
			payload = *(u8 *)(data0) |
				*(u8 *)(data0 + 1) << 8 |
				*(u8 *)(data0 + 2) << 16;

			s5p_mipi_dsi_wr_tx_data(dsim, payload);

			dev_dbg(dsim->dev, "count = %d payload = %x,%x %x %x\n",
				data1, payload,
				*(u8 *)(data0 + data_cnt),
				*(u8 *)(data0 + (data_cnt + 1)),
				*(u8 *)(data0 + (data_cnt + 2)));
		/* in case that data count is more then 4 */
		} else
			s5p_mipi_dsi_long_data_wr(dsim, data0, data1);

		/* put data into header fifo */
		s5p_mipi_dsi_wr_tx_header(dsim, data_id, data1 & 0xff,
			(data1 & 0xff00) >> 8);
		} while(0);
		break;
	
	/* packet typo for video data */
	case MIPI_DSI_PACKED_PIXEL_STREAM_16:
	case MIPI_DSI_PACKED_PIXEL_STREAM_18:
	case MIPI_DSI_PIXEL_STREAM_3BYTE_18:
	case MIPI_DSI_PACKED_PIXEL_STREAM_24:
		break;
	default:
		dev_warn(dsim->dev,
			"data id %x is not supported current DSI spec.\n",
			data_id);

		ret = -EINVAL;
	}

	/* process response func should be implemented. */
	if (bta_timeout) {
		s5p_mipi_dsi_force_bta(dsim);
		ret = s5p_mipi_dsi_wait_state(dsim, (1 << 25)/*DSIM_BUS_TURN_OVER*/, bta_timeout);
	}
	if (ret)
		pr_warn("\n%s failed: data_id = 0x%x, data0 = 0x%x, data1 = 0x%x\n",
				__func__, data_id, data0, data1);
	return ret;
}

static int s5p_mipi_dsi_pll_on(struct mipi_dsim_device *dsim, unsigned int enable)
{
	int sw_timeout;

	if (enable) {
		sw_timeout = 1000;

		s5p_mipi_dsi_clear_interrupt(dsim);
		s5p_mipi_dsi_enable_pll(dsim, 1);
		while (1) {
			sw_timeout--;
			if (s5p_mipi_dsi_is_pll_stable(dsim))
				return 0;
			if (sw_timeout == 0)
				return -EINVAL;
		}
	} else
		s5p_mipi_dsi_enable_pll(dsim, 0);

	return 0;
}

static unsigned long s5p_mipi_dsi_change_pll(struct mipi_dsim_device *dsim,
	unsigned int pre_divider, unsigned int main_divider,
	unsigned int scaler)
{
	unsigned long dfin_pll, dfvco, dpll_out;
	unsigned int i, freq_band = 0xf;

	dfin_pll = (FIN_HZ / pre_divider);

	/******************************************************
	 *	Serial Clock(=ByteClk X 8)	FreqBand[3:0] *
	 ******************************************************
	 *	~ 99.99 MHz			0000
	 *	100 ~ 119.99 MHz		0001
	 *	120 ~ 159.99 MHz		0010
	 *	160 ~ 199.99 MHz		0011
	 *	200 ~ 239.99 MHz		0100
	 *	140 ~ 319.99 MHz		0101
	 *	320 ~ 389.99 MHz		0110
	 *	390 ~ 449.99 MHz		0111
	 *	450 ~ 509.99 MHz		1000
	 *	510 ~ 559.99 MHz		1001
	 *	560 ~ 639.99 MHz		1010
	 *	640 ~ 689.99 MHz		1011
	 *	690 ~ 769.99 MHz		1100
	 *	770 ~ 869.99 MHz		1101
	 *	870 ~ 949.99 MHz		1110
	 *	950 ~ 1000 MHz			1111
	 ******************************************************/
	if (dfin_pll < DFIN_PLL_MIN_HZ || dfin_pll > DFIN_PLL_MAX_HZ) {
		dev_warn(dsim->dev, "fin_pll range should be 6MHz ~ 12MHz\n");
		s5p_mipi_dsi_enable_afc(dsim, 0, 0);
	} else {
		if (dfin_pll < 7 * MHZ)
			s5p_mipi_dsi_enable_afc(dsim, 1, 0x1);
		else if (dfin_pll < 8 * MHZ)
			s5p_mipi_dsi_enable_afc(dsim, 1, 0x0);
		else if (dfin_pll < 9 * MHZ)
			s5p_mipi_dsi_enable_afc(dsim, 1, 0x3);
		else if (dfin_pll < 10 * MHZ)
			s5p_mipi_dsi_enable_afc(dsim, 1, 0x2);
		else if (dfin_pll < 11 * MHZ)
			s5p_mipi_dsi_enable_afc(dsim, 1, 0x5);
		else
			s5p_mipi_dsi_enable_afc(dsim, 1, 0x4);
	}

	dfvco = dfin_pll * main_divider;
	dev_dbg(dsim->dev, "dfvco = %lu, dfin_pll = %lu, main_divider = %d\n",
				dfvco, dfin_pll, main_divider);
	if (dfvco < DFVCO_MIN_HZ || dfvco > DFVCO_MAX_HZ)
		dev_warn(dsim->dev, "fvco range should be 500MHz ~ 1000MHz\n");

	dpll_out = dfvco / (1 << scaler);
	dev_dbg(dsim->dev, "dpll_out = %lu, dfvco = %lu, scaler = %d\n",
		dpll_out, dfvco, scaler);

	for (i = 0; i < ARRAY_SIZE(dpll_table); i++) {
		if (dpll_out < dpll_table[i] * MHZ) {
			freq_band = i;
			break;
		}
	}

	dev_dbg(dsim->dev, "freq_band = %d\n", freq_band);

	s5p_mipi_dsi_pll_freq(dsim, pre_divider, main_divider, scaler);

	s5p_mipi_dsi_hs_zero_ctrl(dsim, 0);
	s5p_mipi_dsi_prep_ctrl(dsim, 7);	//modified by lvcha to fix some lcd display failed

	/* Freq Band */
	s5p_mipi_dsi_pll_freq_band(dsim, freq_band);

	/* Stable time */
	s5p_mipi_dsi_pll_stable_time(dsim, dsim->dsim_config->pll_stable_time);

	/* Enable PLL */
	dev_dbg(dsim->dev, "FOUT of mipi dphy pll is %luMHz\n",
		(dpll_out / MHZ));

	return dpll_out;
}

static int s5p_mipi_dsi_set_clock(struct mipi_dsim_device *dsim,
	unsigned int byte_clk_sel, unsigned int enable)
{
	unsigned int esc_div;
	unsigned long esc_clk_error_rate;
	unsigned long hs_clk = 0, byte_clk = 0, escape_clk = 0;

	if (enable) {
		dsim->e_clk_src = byte_clk_sel;

		/* Escape mode clock and byte clock source */
		s5p_mipi_dsi_set_byte_clock_src(dsim, byte_clk_sel);

		/* DPHY, DSIM Link : D-PHY clock out */
		if (byte_clk_sel == DSIM_PLL_OUT_DIV8) {
			hs_clk = s5p_mipi_dsi_change_pll(dsim,
				dsim->dsim_config->p, dsim->dsim_config->m,
				dsim->dsim_config->s);
			if (hs_clk == 0) {
				dev_err(dsim->dev,
					"failed to get hs clock.\n");
				return -EINVAL;
			}

			byte_clk = hs_clk / 8;
			s5p_mipi_dsi_enable_pll_bypass(dsim, 0);
			s5p_mipi_dsi_pll_on(dsim, 1);
		/* DPHY : D-PHY clock out, DSIM link : external clock out */
		} else if (byte_clk_sel == DSIM_EXT_CLK_DIV8)
			dev_warn(dsim->dev,
				"this project is not support \
				external clock source for MIPI DSIM\n");
		else if (byte_clk_sel == DSIM_EXT_CLK_BYPASS)
			dev_warn(dsim->dev,
				"this project is not support \
				external clock source for MIPI DSIM\n");

		/* escape clock divider */
		esc_div = byte_clk / (dsim->dsim_config->esc_clk);
		dev_dbg(dsim->dev,
			"esc_div = %d, byte_clk = %lu, esc_clk = %lu\n",
			esc_div, byte_clk, dsim->dsim_config->esc_clk);
		if ((byte_clk / esc_div) >= (20 * MHZ) ||
				(byte_clk / esc_div) >
					dsim->dsim_config->esc_clk)
			esc_div += 1;

		escape_clk = byte_clk / esc_div;
		dev_dbg(dsim->dev,
			"escape_clk = %lu, byte_clk = %lu, esc_div = %d\n",
			escape_clk, byte_clk, esc_div);

		/* enable escape clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_ON);

		/* enable byte clk and escape clock */
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 1, esc_div);
		/* escape clock on lane */
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 1);

		dev_dbg(dsim->dev, "byte clock is %luMHz\n",
			(byte_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock that user's need is %lu\n",
			(dsim->dsim_config->esc_clk / MHZ));
		dev_dbg(dsim->dev, "escape clock divider is %x\n", esc_div);
		dev_dbg(dsim->dev, "escape clock is %luMHz\n",
			((byte_clk / esc_div) / MHZ));

		if ((byte_clk / esc_div) > escape_clk) {
			esc_clk_error_rate = escape_clk /
				(byte_clk / esc_div);
			dev_warn(dsim->dev, "error rate is %lu over.\n",
				(esc_clk_error_rate / 100));
		} else if ((byte_clk / esc_div) < (escape_clk)) {
			esc_clk_error_rate = (byte_clk / esc_div) /
				escape_clk;
			dev_warn(dsim->dev, "error rate is %lu under.\n",
				(esc_clk_error_rate / 100));
		}
	} else {
		s5p_mipi_dsi_enable_esc_clk_on_lane(dsim,
			(DSIM_LANE_CLOCK | dsim->data_lane), 0);
		s5p_mipi_dsi_set_esc_clk_prs(dsim, 0, 0);

		/* disable escape clock. */
		s5p_mipi_dsi_enable_byte_clock(dsim, DSIM_ESCCLK_OFF);

		if (byte_clk_sel == DSIM_PLL_OUT_DIV8)
			s5p_mipi_dsi_pll_on(dsim, 0);
	}

	return 0;
}

int s5p_mipi_dsi_init_dsim(struct mipi_dsim_device *dsim)
{
	dsim->state = DSIM_STATE_INIT;

	switch (dsim->dsim_config->e_no_data_lane) {
	case DSIM_DATA_LANE_1:
		dsim->data_lane = DSIM_LANE_DATA0;
		break;
	case DSIM_DATA_LANE_2:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1;
		break;
	case DSIM_DATA_LANE_3:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2;
		break;
	case DSIM_DATA_LANE_4:
		dsim->data_lane = DSIM_LANE_DATA0 | DSIM_LANE_DATA1 |
			DSIM_LANE_DATA2 | DSIM_LANE_DATA3;
		break;
	default:
		dev_info(dsim->dev, "data lane is invalid.\n");
		return -EINVAL;
	};

	s5p_mipi_dsi_standby(dsim, false);
	s5p_mipi_dsi_sw_reset(dsim);
	s5p_mipi_dsi_func_reset(dsim);

	s5p_mipi_dsi_dp_dn_swap(dsim, dsim->dsim_config->e_lane_swap);

	return 0;
}

int s5p_mipi_dsi_enable_frame_done_int(struct mipi_dsim_device *dsim,
	unsigned int enable)
{
	/* enable only frame done interrupt */
	s5p_mipi_dsi_set_interrupt_mask(dsim, INTMSK_FRAME_DONE, enable);

	return 0;
}

int s5p_mipi_dsi_set_display_mode(struct mipi_dsim_device *dsim)
{
	struct s3cfb_lcd *lcd_panel_info;
	unsigned int width = 0, height = 0;

	lcd_panel_info = (struct s3cfb_lcd *)dsim->pd->lcd_panel_info;
	width = lcd_panel_info->width;
	height = lcd_panel_info->height;

	/* in case of VIDEO MODE (RGB INTERFACE) */
	if (dsim->dsim_config->e_interface == (u32) DSIM_VIDEO) {
		if (dsim->dsim_config->auto_vertical_cnt == 0) {
				s5p_mipi_dsi_set_main_disp_vporch(dsim,
					lcd_panel_info->timing.cmd_allow_len,
					lcd_panel_info->timing.v_fp,
					lcd_panel_info->timing.v_bp);
				s5p_mipi_dsi_set_main_disp_hporch(dsim,
					lcd_panel_info->timing.h_fp,
					lcd_panel_info->timing.h_bp);
				s5p_mipi_dsi_set_main_disp_sync_area(dsim,
					lcd_panel_info->timing.v_sw,
					lcd_panel_info->timing.h_sw);
			}
	}

	s5p_mipi_dsi_set_main_disp_resol(dsim, height, width);

	s5p_mipi_dsi_display_config(dsim, dsim->dsim_config);

	return 0;
}

int s5p_mipi_dsi_init_link(struct mipi_dsim_device *dsim)
{
	unsigned int time_out = 100;

	switch (dsim->state) {		
	case DSIM_STATE_INIT:
		//s5p_mipi_dsi_sw_reset(dsim);

		s5p_mipi_dsi_init_fifo_pointer(dsim, 0x1f);

		/* dsi configuration */
		s5p_mipi_dsi_init_config(dsim);
		s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 1);
		s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 1);

		/* set clock configuration */
		s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 1);

		/* check clock and data lane state are stop state */
		while (!(s5p_mipi_dsi_is_lane_state(dsim))) {	//???
			time_out--;
			if (time_out == 0) {
				dev_info(dsim->dev,
					"DSI Master is not stop state.\n");
				dev_info(dsim->dev,
					"Check initialization process\n");

				return -EINVAL;
			}
		}

		if (time_out != 0) {
			dev_dbg(dsim->dev,
				"DSI Master driver has been completed.\n");
			dev_dbg(dsim->dev, "DSI Master state is stop state\n");
		}

		dsim->state = DSIM_STATE_STOP;

		/* BTA sequence counters */
		s5p_mipi_dsi_set_stop_state_counter(dsim,
			dsim->dsim_config->stop_holding_cnt);

		s5p_mipi_dsi_set_bta_timeout(dsim,
			dsim->dsim_config->bta_timeout);

		s5p_mipi_dsi_set_lpdr_timeout(dsim,
			dsim->dsim_config->rx_timeout);

		return 0;
	default:
		dev_info(dsim->dev, "DSI Master is already init.\n");
		return 0;
	}

	return 0;
}
int s5p_mipi_dsi_disable_link(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_enable_lane(dsim, DSIM_LANE_CLOCK, 0);
	s5p_mipi_dsi_enable_lane(dsim, dsim->data_lane, 0);

	/* set clock configuration */
	s5p_mipi_dsi_set_clock(dsim, dsim->dsim_config->e_byte_clk, 0);

	return 0;
}
int s5p_mipi_dsi_set_hs_enable(struct mipi_dsim_device *dsim)
{
	if (dsim->state == DSIM_STATE_STOP) {
		if (dsim->e_clk_src != DSIM_EXT_CLK_BYPASS) {
			dsim->state = DSIM_STATE_HSCLKEN;

			 /* set LCDC and CPU transfer mode to HS. */
			s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
			s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);

			s5p_mipi_dsi_enable_hs_clock(dsim, 1);

			return 0;
		} else
			dev_warn(dsim->dev,
				"clock source is external bypass.\n");
	} else
		dev_warn(dsim->dev, "DSIM is not stop state.\n");

	return 0;
}

int s5p_mipi_dsi_set_data_transfer_mode(struct mipi_dsim_device *dsim,
		unsigned int mode)
{
	if (mode) {
		if (dsim->state != DSIM_STATE_HSCLKEN) {
			dev_err(dsim->dev, "HS Clock lane is not enabled.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_lcdc_transfer_mode(dsim, 0);
	} else {
		if (dsim->state == DSIM_STATE_INIT || dsim->state ==
			DSIM_STATE_ULPS) {
			dev_err(dsim->dev,
				"DSI Master is not STOP or HSDT state.\n");
			return -EINVAL;
		}

		s5p_mipi_dsi_set_cpu_transfer_mode(dsim, 0);
	}

	return 0;
}

int s5p_mipi_dsi_get_frame_done_status(struct mipi_dsim_device *dsim)
{
	return _s5p_mipi_dsi_get_frame_done_status(dsim);
}

int s5p_mipi_dsi_clear_frame_done(struct mipi_dsim_device *dsim)
{
	_s5p_mipi_dsi_clear_frame_done(dsim);

	return 0;
}

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung MIPI-DSIM common driver");
MODULE_LICENSE("GPL");
