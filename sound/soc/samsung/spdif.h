/* sound/soc/samsung/spdif.h
 *
 * ALSA SoC Audio Layer - Samsung S/PDIF Controller driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SAMSUNG_SPDIF_H
#define __SND_SOC_SAMSUNG_SPDIF_H	__FILE__

#define SND_SOC_SPDIF_INT_MCLK		0
#define SND_SOC_SPDIF_EXT_MCLK		1

enum codec_type {
	SPDIF_CODEC_PCM = 0,
	SPDIF_CODEC_AC3,
	SPDIF_CODEC_DTS,
	SPDIF_CODEC_MP3,
	SPDIF_CODEC_UNKNOWN,
};

enum bits_per_sample {
	SPDIF_BIT_16,
	SPDIF_BIT_20,
	SPDIF_BIT_24,
	SPDIF_BIT_UNKNOWN,
};

enum audio_clk_freq {
	SPDIF_FREQ_256,
	SPDIF_FREQ_384,
	SPDIF_FREQ_512,
	SPDIF_FREQ_768,
	SPDIF_FREQ_UNKNOWN,
};

enum endian_format {
	SPDIF_BIG_ENDIAN,
	SPDIF_4BYTES_SWAP,
	SPDIF_3BYTES_SWAP,
	SPDIF_2BYTES_SWAP,
};

enum filo_level_threshold {
	SPDIF_FIFO_LV_0,
	SPDIF_FIFO_LV_1,
	SPDIF_FIFO_LV_4,
	SPDIF_FIFO_LV_6,
	SPDIF_FIFO_LV_10,
	SPDIF_FIFO_LV_12,
	SPDIF_FIFO_LV_14,
	SPDIF_FIFO_LV_15,
};

enum transer_mode {
	SPDIF_TRANS_DMA,
	SPDIF_TRANS_INT
};

enum compress_type {
	SPDIF_COMP_NULL = 0,
	SPDIF_COMP_AC3 = 1,
	SPDIF_COMP_MP1 = 4,
	SPDIF_COMP_MP3 = 5,
};

enum sample_freq {
	SPDIF_SR_44100 = 0,
	SPDIF_SR_48000 = 2,
	SPDIF_SR_32000 = 3,
	SPDIF_SR_96000 = 10,
};

enum category_code {
	SPDIF_CC_CDP = 1,
	SPDIF_CC_DAT = 3,
	SPDIF_CC_DDC = 0x43,
	SPDIF_CC_MINI = 0x49,
};

enum audio_sample_word {
	SPDIF_ASW_LINEAR_PCM,
	SPDIF_ASW_NON_LINEAR_PCM,
};

struct spdif_con {
	unsigned int	stream_type		:1;	/*stream type*/
	unsigned int	data_size 		:2;	/*data size*/
	unsigned int	main_aclk		:2;	/*main audio clock frequency*/
	unsigned int	soft_reset		:1;	/*software reset*/
	unsigned int	int_strmend_en	:1;	/*stream end interrupt enable*/
	unsigned int	int_strmend_st	:1;	/*stream end interrupt enable*/
	unsigned int	int_bufempty_en	:1;	/*buffer empty interrupt enable*/
	unsigned int	int_bufempty_st	:1;	/*buffer empty interrupt enable*/
	unsigned int	int_userdata_en	:1;	/*userdata interrupt enable*/
	unsigned int	int_userdata_st	:1;	/*userdata interrupt enable*/
	unsigned int	user_dat_attach	:1;	/*user data attach*/
	unsigned int	endian_format	:2;	/*endian format*/
	unsigned int	int_fifo_en		:1;	/*fifo level interrupt enable*/
	unsigned int	int_fifo_st		:1;	/*fifo level interrupt enable*/
	unsigned int	fifo_trans_mode	:2;	/*fifo transer mode*/
	unsigned int	fifo_thresh		:3;	/*fifo level threshold*/
	unsigned int	fifo_level0		:2;	/*fifo level*/
	unsigned int	fifo_level1		:3;	/*fifo level*/
	unsigned int	reserved		:5;	/*fifo level*/
};

struct spdif_bstat {
	unsigned int	compressed_type	:5;	/*compressed data type*/
	unsigned int	reserved		:2;	/*compressed data type*/
	unsigned int	error_flag		:1;
	unsigned int	data_dep_info	:5;
	unsigned int	bitstream_num	:3;
	unsigned int	burst_data_len	:16;
};

struct spdif_cstat {
	unsigned int	chan_stat_blk	:1;
	unsigned int	audio_samp_word	:1;
	unsigned int	copyright		:1;
	unsigned int	emphasis		:3;
	unsigned int	chan_stat_mode	:2;
	unsigned int	category_code	:8;
	unsigned int	source_num		:4;
	unsigned int	chan_num		:4;
	unsigned int	sample_rate		:4;
	unsigned int	clk_accuracy	:2;
	unsigned int	reserved		:2;
};
/* Registers */
#define CLKCON				0x00
#define CON				0x04
#define BSTAS				0x08
#define CSTAS				0x0C
#define DATA_OUTBUF			0x10
#define DCNT				0x14
#define BSTAS_S				0x18
#define DCNT_S				0x1C

#define CLKCTL_MASK			0x7
#define CLKCTL_MCLK_EXT			(0x1 << 2)
#define CLKCTL_PWR_ON			(0x1 << 0)

#define CON_MASK			0x3ffffff
#define CON_FIFO_TH_SHIFT		19
#define CON_FIFO_TH_MASK		(0x7 << 19)
#define CON_USERDATA_23RDBIT		(0x1 << 12)

#define CON_SW_RESET			(0x1 << 5)

#define CON_MCLKDIV_MASK		(0x3 << 3)
#define CON_MCLKDIV_256FS		(0x0 << 3)
#define CON_MCLKDIV_384FS		(0x1 << 3)
#define CON_MCLKDIV_512FS		(0x2 << 3)

#define CON_PCM_MASK			(0x3 << 1)
#define CON_PCM_16BIT			(0x0 << 1)
#define CON_PCM_20BIT			(0x1 << 1)
#define CON_PCM_24BIT			(0x2 << 1)

#define CON_PCM_DATA			(0x1 << 0)
#define CON_STREAM_DATA		(0x0 << 0)

#define CSTAS_MASK			0x3fffffff
#define CSTAS_SAMP_FREQ_MASK		(0xF << 24)
#define CSTAS_SAMP_FREQ_44		(0x0 << 24)
#define CSTAS_SAMP_FREQ_48		(0x2 << 24)
#define CSTAS_SAMP_FREQ_32		(0x3 << 24)
#define CSTAS_SAMP_FREQ_96		(0xA << 24)

#define CSTAS_CATEGORY_MASK		(0xFF << 8)
#define CSTAS_CATEGORY_CODE_CDP		(0x01 << 8)

#define CSTAS_NO_COPYRIGHT		(0x1 << 2)

#endif	/* __SND_SOC_SAMSUNG_SPDIF_H */
