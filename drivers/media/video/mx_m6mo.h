/*
 * Driver for M6MO from Meizu Inc.
 * 
 * 1/4" 5Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * Copyright (C) 2010, Wenbin Wu<wenbinwu@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __M6MO_H__
#define __M6MO_H__

enum image_size_m6mo {
	VGA,	/* 640*480 */
	SVGA,	/* 800*600 */
	HD720P,	/* 1280*720 */
	SXGA,	/* 1280*1024 */
};

enum m6mo_regtype{
	REG_8BIT=0,
	REG_16BIT,
	REG_32BIT,
};

struct m6mo_reg {
	unsigned char type;
	unsigned int addr;
	unsigned int val;
};

struct m6mo_win_size {
	int	width;
	int	height;
	struct m6mo_reg *regs; /* Regs to tweak */
	int size;
};
/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
struct m6mo_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;	
	struct m6mo_reg *regs;
	int size;
	int bpp;   /* Bytes per pixel */
};

#define M6MO_DRIVER_NAME		"M6MO"
#define M6MO_FIRMWARE_FILE_NAME	"isp_firmware.bin"
#define M6MO_FIRMWARE_FILE_SIZE		2016*1024// 2MB
#define M6MO_SECTION64_WRITE_SIZE	1984*1024
#define M6MO_SECTION8_WRITE_SIZE		M6MO_FIRMWARE_FILE_SIZE - M6MO_SECTION64_WRITE_SIZE
#define M6MO_SECTION64_FRAME_SIZE	64*1024//64KB
#define M6MO_SECTION32_FRAME_SIZE	32*1024//32KB
#define M6MO_SECTION8_FRAME_SIZE		8*1024//8KB
#define M6MO_SECTION64_FLASH_ADDRESS	0x10000000
#define M6MO_SECTION8_FLASH_ADDRESS		0x101f0000
#define M6MO_INTERNAL_RAM_ADDRESS		0x68000000
/*
	Firmware file format:
	1472KB	--- Main Body of Firmware
	512KB	--- Memory map area
	32KB	--- Reversed
	8KB		---	factory adjustment data1
	8KB		---	factory adjustment data2
	8KB		--- Reversed
	8KB		--- Reversed	
*/
#define M6MO_PIN_VALUE		16
#define M6MO_PIN1_ADDRESS		0x50000300
const char m6mo_pin1_data[M6MO_PIN_VALUE] ={
	0x00,0x00,0x00,0xff,
	0xff,0xff,0xff,0x1f,
	0x76,0x00,0x18,0x00,
	0x00,0x00,0xff,0xff};

#define M6MO_PIN2_ADDRESS		0x50000100
const char m6mo_pin2_data[M6MO_PIN_VALUE] ={
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00};

#define M6MO_PIN3_ADDRESS		0x50000200
const char m6mo_pin3_data[M6MO_PIN_VALUE] ={
	0x00,0x00,0x00,0xff,
	0xff,0xfe,0xff,0x3f,
	0xff,0x00,0x20,0x11,
	0x00,0x00,0xff,0xff};
/* Default resolution & pixelformat. plz ref m6mo_platform.h */
#define DEFAULT_RES		VGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	M6MO_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

#define DEFAULT_JPEG_MEM_SIZE	5*1024*1024//5M

#define DEFAULT_RAW_MEM_SIZE	20*1024*1024//10M

/*waiting event max timeout (in second)*/
#define WAIT_TIMEOUT 5

enum v4l2_camera_mode
{
	V4L2_CAMERA_PREVIEW = 0,//普通预览(单拍和连拍适用)
	V4L2_CAMERA_RECORD = 1,//录像预览(录像适用)
	V4L2_CAMERA_PANORAMA = 2,//全景预览（全景拍照适用）
	V4L2_CAMERA_SINGLE_CAPTURE = 3,//单帧拍照模式
	V4L2_CAMERA_MULTI_CAPTURE = 4,//连拍模式
	V4L2_CAMERA_PANORAMA_CAPTURE = 5,//全景拍照模式
};

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10 
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

/*M6MO I2C address:
   Read : 0x3E
   Write : 0x3F
*/
/*M6MO command format*/
/*| address |number of send|command|comand data|*/
/*command define*/
enum {
	CMD_READ_PARAMETER = 1,
	CMD_WRITE_PARAMETER,
	CMD_READ_8BIT_MEMORY,
	CMD_WRITE_8BIT_MEMORY,
	CMD_READ_16BIT_MEMORY,
	CMD_WRITE_16BIT_MEMORY,
	CMD_READ_32BIT_MEMORY,
	CMD_WRITE_32BIT_MEMORY,
};
struct m6mo_command{
	u8 cmd_type;/* read/write command type*/
	u8 content_size;//0  for memory read/write
	u32 address;	/*regitser address 32bit*/
	u32 data; /*data to be read/write*/
	u16 data_size;	/*memory read/write size, no use for category read/write*/
	unsigned char inbuf[32];/*input data buffer*/
	unsigned char outbuf[32];/*output data  buffer*/
};

/*read category parameter command format*/
/*| address |5 |command |category | reg | N |*/
/*| address |N+1 |data |*/

/*write category parameter command format*/
/*| address |N+4 |command |category | reg | data |*/
/*  N: read/write size  */

/*read memory command format*/
/*| address |0 |command |addr3 |addr2 |addr1 |addr0 | NH | NL |*/
/*| address |0 |NH | NL | data(1-N) |*/
/*  N: read/write size  */

/*write memory command format*/
/*| address |0 |command |addr3 |addr2 |addr1 |addr0 | NH | NL | data(1-N) |*/
/*  N: read/write size  */

/*m6mo error code*/
#define ERR_COMMINICATION_BYTES	0xF0		/*The values of the number of effective communication bytes are not corresponding to the number of bytes for actual data.*/		
#define ERR_COMMAND_CODE			0xF1		/*The command code is an unexpected value.*/	
#define ERR_CATEGORY_NUMBER		0xF2		/*An invalid category was specified*/	
#define ERR_BYTES_NUMBER			0xF3		/*invalid byte number was specified*/	
#define ERR_RW_OVERFLOW			0xF4		/*Category parameter Read and Write of multiple bites from Host exceeded byte effective number.*/	
#define ERR_MODE_SWITCHED			0xFA		/*The mode is being switched*/	
#define ERR_DURING_FLASH_WRITE	0xFF		/*During Flash Writer mode, if you read exclude Category F, this value has been read.*/	


/*m6mo Category F parameter*/
#define FLASH_ADDRESS_REG					0x0f00 /*4byte, default 0x00000000*/
#define FLASH_SIZE_REG						0x0f04 /*2byte, 0=64KB*/
#define FLASH_ERASE_CMD					0x0f06 /*1byte*/
#define FLASH_WRITE_CMD					0x0f07 /*1byte*/
#define RAM_CLEAN_CMD						0x0f08 /*1byte*/
#define FLASH_CHKSUM_CMD					0x0f09 /*1byte*/
#define FLASH_CHKSUM_RESULT				0x0f0A /*2byte*/
#define CAMERA_START_ADDRESS				0x0f0B /*4byte, default 0x10000000*/
#define CAMERA_START_CMD					0x0f12 /*1byte*/
#define CAMERA_FLASH_TYPE					0x0f13 /*1byte*/
/*m6mo Category F command write*/
#define FLASH_CMD_INVALID				0
#define FLASH_CMD_START				1
/*m6mo Category F command read result*/
#define FLASH_CMD_COMPLETE			0
#define FLASH_CMD_WORKING				1

/*m6mo Category 0 parameter*/
#define CUSTOMER_CODE					0x0000/*customer code*/
#define PROJECT_CODE					0x0001/*project code*/
#define FIRMWARE_VERSION				0x0002/*2byte, firmware version*/
#define HARDWARE_VERSION				0x0004/*2byte,hardware version*/
#define PARAMETER_VERSION				0x0006/*2byte,parameter version*/
#define AWB_VERSION					0x0008/*2byte,AWB version*/
#define SYS_MODE					0x000b
enum system_mod{
	SYS_INITIALIZE_MODE=0,/*read status only*/
	SYS_PARAMETER_MODE,
	SYS_MONITOR_MODE,
	SYS_CAPTURE_MODE,
};
#define SYSTEM_STATUS					0x000c/*read only*/
enum system_status{
	SYS_INITIALIZE_STATUS=0,
	SYS_PARAMETER_STATUS,
	SYS_MONITOR_STATUS,
	SYS_AUTO_FOCUS_STATUS,
	SYS_FACE_DETECT_STATUS,
	SYS_MULTI_CAPTURE_STATUS,	
	SYS_SINGLE_CAPTURE_STATUS,
	SYS_PREVIEW_STATUS,
};
#define ESD_INT							0x000e/*data output stop frem sensor*/
enum esd_int{
	ESD_NO_INT =0,
	ESD_INT_OCCUR,
};
#define INT_ENABLE						0x0010/*interrupt enable*/
#define INT_ROOR_ENABLE				0x0012/*root interrupt enable*/
#define FRAMESYNC_COUNT				0x0014/*available for multi capture*/
#define VER_FIRMWARE_MINOR			0x0015  /*firmware minor version*/
#define INT_FACTOR						0x001c/*interrupt status*/
enum int_mask{
	INT_MASK_SOUND = 0x80,
	INT_MASK_LENS_INIT = 0x40,
	INT_MASK_FD = 0x20,
	INT_MASK_FRAMESYNC = 0x10,
	INT_MASK_CAPTURE = 0x08,
	INT_MASK_ZOOM = 0x04,
	INT_MASK_AF = 0x02,
	INT_MASK_MODE = 0x01,
};
/*m6mo Category 1 parameter*/
#define OUT_SELECT						0x0100
enum output_select{
	YUV_IF=0,
	HDMI_IF=1,
	MIPI_IF=2,
};
#define MON_SIZE						0x0101
enum mon_size{
	MON_QVGA=0x09,//320x240
	MON_VGA=0x17,//640x480
	MON_QCIF=0x05,//176x144
	MON_CIF=0x0e,//352x288
	MON_854X640=0x1d,  //854x640, pixel to pixel
	MON_SVGA=0x1f,//800x600
	MON_960X720=0x20, //960x720
	MON_720P=0x21,//1280x720
	MON_1080P=0x28,//1920x1080
};
#define MON_FPS							0x0102
enum mon_fps{
	MON_FPS_AUTO=1,
	MON_FPS_30,
	MON_FPS_15,
	MON_FPS_12,
	MON_FPS_10,
	MON_FPS_7P5,
};
#define FSHD_EN							0x0107
enum fshd_enable{
	FSHD_OFF=0,
	FSHD_ON,
	FSHD_MANUAL,/*for debug*/	
};
#define EFFECT_MODE					0x010b
enum effect_mode{
	EFFECT_OFF=0,
	EFFECT_WATERCOLOR,
};
#define SEND_UNIT						0x0118/*2byte*/
#define SUPPRE_CAP						0x011c/*color & luminance capture noise reduction*/
enum suppre_cap{
	SUPPRE_CAP_AUTO=0,
	SUPPRE_CAP_MANUAL,
};
#define SUPPRE_MON						0x011d/*color & luminance monitor noise reduction*/
enum suppre_mon{
	SUPPRE_MON_OFF=0,
	SUPPRE_MON_AUTO,
};
#define MON_ROTATION_ISP				0x012d/**/
enum mon_rotation_isp{
	MON_ROTATION_ISP_CW90=1,
	MON_ROTATION_ISP_ACW90=2,
};
#define CONT_YUV_OUTPUT				0x0131/*continuous output*/
enum cont_yuv_output{
	CONT_YUV_OUTPUT_DISABLE=0,
	CONT_YUV_OUTPUT_ENABLE,		
};
#define LHBLK							0x0132/*2byte, h blanking width*/
#define LVBLK							0x0134/*2byte, v blanking width*/
#define LHBLKPOST						0x0136/*2byte, post h blanking width*/
#define LVBLKPOST						0x0138/*2byte, post v blanking width*/
#define IASTER_CLK_FOR_SENSOR			0x013a/*4byte, default 27MHZ*/

/*m6mo Category 2 parameter*/
#define ZOOM_POSITOIN					0x0201
  #define 	MIN_ZOOM_POS 		0x01
  #define 	MIN_ZOOM_STEP		0x02

#define ZOOM_CUR_POSITOIN				0x0202
#define ZOOM_STEP						0x0203
#define ZOOM_CFIXB						0x0209
#define ZOOM_CFIXR						0x020a
#define COLOR_EFFECT					0x020b
enum{
	COLOR_EFFECT_ON=0,
	COLOR_EFFECT_OFF,
	COLOR_EFFECT_FILM,	
};
#define CHROMA_LVL						0x020f/*chroma level setting : 0x01-0x0a*/
#define TONE_CTRL						0x0225/*tone control level setting*/
enum tone_ctrl{
	TONE_OFF=0x00,
	TONE_M0=0x01,
	TONE_M1=0x02,
	TONE_M2=0x03,
	TONE_M3=0x04,
	TONE_M4=0x05,
	TONE_P1=0x0c,
	TONE_P2=0x0b,
	TONE_P3=0x0a,
	TONE_P4=0x09,
};
#define SCENE_MODE						0x0237/*scene mode setting*/
enum scene_mode{
	SCENE_OFF=0,
	SCENE_AUTO,
	SCENE_PORTRAIT,
	SCENE_LANDSCAPE,
	SCENE_SPORT,
	SCENE_NIGHT,
	SCENE_SUNSET,
	SCENE_MARCO,
	SCENE_CHARACTER = 0x0b,
};
#define SPECIAL_MON						0x0238/*panorama mode setting*/
enum special_mon{
	SPECIAL_OFF=0,
	SPECIAL_VAHS,
	SPECIAL_PANORAMA,
};
#define MON_REVERSE_ISP				0x0252/*ISP reverse ON/OFF*/
#define MON_MIRROR_ISP					0x0253/*ISP mirror ON/OFF*/

/*m6mo Category 3 parameter*/
#define AE_LOCK							0x0300
#define AE_MODE							0x0301
#define AE_TARGET						0x0302
#define AE_SPEED						0x0303
#define ISO_SEL							0x0305
enum iso_sel{
	ISO_SEL_AUTO=0,
	ISO_SEL_50,
	ISO_SEL_100,
	ISO_SEL_200,
	ISO_SEL_400,
	ISO_SEL_800,
	ISO_SEL_1600,
	ISO_SEL_3200,
};
#define FLICKER							0x0306
enum flicker{
	FLICKER_AUTO=0,
	FLICKER_50HZ=1,
	FLICKER_60HZ=2,
	FLICKER_OFF=3,
};
#define FLICKER_AUTO_STATUS			0x0307
#define FLICKER_SELECT					0x0308
#define EV_BIAS							0x0309
enum ev_bias{
	EV_M2=0x0a,
	EV_M1=0x14,
	EV_00=0x1e,
	EV_P1=0x28,
	EV_P2=0x32,
};
#define EVP_MODE_MON					0x030a
enum evp_mode_mon {
	EVP_MODE_AUTO = 0, 
	EVP_MODE_30FPS,
	EVP_MODE_15FPS,
};
#define NOW_GAIN						0x030e/*2byte*/
#define NOW_EXPOSURE					0x0310/*2byte*/

#define REG_SCENE_EV					0x031e /*2byte*/

/*m6mo Category 6 parameter*/
#define AWB_LOCK						0x0600
#define AWB_MODE						0x0602
enum awb_mode{
	AWB_AUTO=0x01,
	AWB_MANUAL=0x02,
	AWB_TRACKING=0x03,
};
#define AWB_MANUAL_REG						0x0603
enum awb_manual{ 
	AWB_PROHIBITION=0,
	AWB_INCANDESCENT,
	AWB_FLUORESCENT_HIGH,
	AWB_FLUORESCENT_LOW,
	AWB_DAYLIGHT,
	AWB_CLOUDY,
	AWB_SHADE,
	AWB_HORIZON,
};
#define AWB_SPEED						0x0604/*0x01-0x64: 1%*/
#define AWB_RANGE						0x0605/*0: indoor, 1:outdoor*/
#define AWB_DETECT_TEMP				0x0606/*2byte*/
#define AWB_DETECT_COLOR_X			0x0608/*2byte*/
#define AWB_DETECT_COLOR_Y			0x060a/*2byte*/

/*m6mo Category 7 parameter, read only*/
#define INFO_EXPTIME_NUMERATOR		0x0700/*4byte, exposure time*/
#define INFO_EXPTIME_DENUMINATOR		0x0704/*4byte, exposure time*/
#define INFO_TV_NUMERATOR				0x0708/*4byte, shutter speed value*/
#define INFO_TV_DENUMINATOR			0x0708/*4byte, shutter speed value*/
#define INFO_AV_NUMERATOR				0x070c/*4byte, aperture value*/
#define INFO_AV_DENUMINATOR			0x0714/*4byte, aperture value*/
#define INFO_BV_NUMERATOR				0x0718/*4byte, brightness value*/
#define INFO_BV_DENUMINATOR			0x071c/*4byte, brightness value*/
#define INFO_EBV_NUMERATOR			0x0720/*4byte, exposure bias value*/
#define INFO_EBV_DENUMINATOR			0x0724/*4byte, exposure bias value*/
#define INFO_ISO							0x0728/*2byte, ISO speed value*/
#define INFO_FLASH						0x072a/*2byte, FLASH*/
#define INFO_SDR						0x072c/*2byte, subject distance range*/
#define INFO_QVAL						0x072e/*2byte, QVALUE*/

/*m6mo Category 8 parameter, for panorama*/
#define PAN_CAP_READY				0x0834
enum pan_cap_action {
	PAN_CAP_NOT_READY = 0x00,
	PAN_CAP_READY_START = 0x01,
	PAN_CAP_READY_STOP = 0x02,
};

#define PAN_CAP_DIRECTION				0x0835
#define PANO_OFFSETX_H					0x0836
#define PANO_OFFSETX_L					0x0837
#define PANO_OFFSETY_H					0x0838
#define PANO_OFFSETY_L					0x0839
#define PAN_CAP_INDEX					0x083a
#define PAN_ERROR_NO					0x083b
#define PANO_CTRL						0x083c						

/*m6mo Category 9 parameter*/
#define FACE_DETECT_CTL				0x0900/**/
enum face_detect{
	FACE_DETECT_ON=0x11,
	FACE_DETECT_OFF=0x10,	
};
#define FACE_DETECT_MAX				0x0902
#define FD_INT_ENABLE					0x0907/*face detect INT enable*/
#define FACE_DETECT_NUM				0x090a/*number of detected*/
#define FACE_DETECT_READ_SEL			0x090b
#define FACE_DETECT_X_LOCATION 		0x090e
#define FACE_DETECT_Y_LOCATION 		0x0910
#define FACE_DETECT_FRAME_WIDTH		0x0912
#define FACE_DETECT_FRAME_HEIGH 		0x0914
#define FACE_DETECT_SMILE_INFO		0x0916
#define FD_SMILE_LEVEL_THRS			0x092a
#define FD_SMILE_DETECT_NUMBER		0x092b
#define FACE_DETECT_DIRECTION 			0x094f

enum smile_face_detect {
	SMILE_FACE_DETECT_OFF = 0x00,
	SMILE_FACE_DETECT_ON = 0x01,
};

/*m6mo Category 0xa parameter*/
#define AF_START_REG						0x0a02
enum af_start{
	AF_STOP=0,
	AF_START = 1,
	AF_RELEASE = 3,
};
#define AF_RESULT						0x0a03
enum af_result{
	AF_OPERATING=0,
	AF_SUCCESS,
	AF_FAIL,
	AF_STOPPED,
};

#define AF_BUSY 							0x0a0b
#define AF_TOUCH_WIN_W				0x0a22/*2byte*/
#define AF_TOUCH_WIN_H				0x0a24/*2byte*/
#define AF_TOUCH_WIN_X					0x0a2a/*2byte*/
#define AF_TOUCH_WIN_Y					0x0a2c/*2byte*/
#define CAF_STOP_MODE					0x0a2e
#define CAF_UPDATE_AF_FRAME			0x0a2f
#define AF_WINDOW						0x0a40
enum af_window{
	CENTRE_SMALL=0,
	CENTRE_LARGE,
	FIVE_POINT,
	BY_FACE_DETECT,/*face detect af*/
	BY_USER,/*touch af*/
};
enum af_touch_info{
	AF_TOUCH_ROW=0x07,
	AF_TOUCH_COL=0x07,	
	AF_TOUCH_WIDTH=0x24,
	AF_TOUCH_HEIGHT=0x24,
	AF_TOUCH_WIDTH_MAX=0x100,
	AF_TOUCH_HEIGHT_MAX=0x100,
};

#define AF_SCAN_MODE					0x0a41
enum af_scan_mode{
	AF_FULL_SCAN=0,
	AF_FAST_SCAN=3,
	AF_CONTINUOUS_FOCUS=4,
	AF_FINE_CONTINUOUS_FOCUS=5,
};
#define AF_RANGE_MODE					0x0a42
enum af_range_mode{
	AF_NORMAL=0,
	AF_MACRO,
	AF_FULL,
};
#define AF_DETECT_RESULT				0x0a44
enum af_detect_result{
	AF_DETECT_FAIL=0,
	AF_DETECT_SUCCESS,
};
#define AF_VCM_START_CURRENT			0x0a46/*4btye*/
#define AF_VCM_SLOPE					0x0a46/*4btye*/

/*m6mo Category 0xb parameter*/
#define YUVOUT_MAIN					0x0b00
enum yvuout_main{
	MAIN_OUTPUT_YUV422 = 0,
	MAIN_OUTPUT_JPEG_422,
	MAIN_OUTPUT_JPEG_420,

	MAIN_RAW10_PACK = 5,
	MAIN_RAW8 = 6,
	MAIN_RAW10_UNPACK = 7,
	MAIN_RAW10_UNDEFINED = 8,
};
#define MAIN_IMAGE_SIZE				0x0b01
enum main_image_size{
	MAIN_SIZE_160_120=0x00,
	MAIN_SIZE_320_240=0x02,
	MAIN_SIZE_426_240=0x05,
	MAIN_SIZE_640_480=0x09,
	MAIN_SIZE_854_480=0x0c,
	MAIN_SIZE_1024_768=0x0f,
	MAIN_SIZE_1280_960=0x14,
	MAIN_SIZE_1600_1200=0x17,
	MAIN_SIZE_2048_1536=0x1b,	
	MAIN_SIZE_2560_1920=0x1f,
	MAIN_SIZE_3264_2448=0x25,	
};
#define MAIN_MIRROR						0x0b02
#define MAIN_REVERSE					0x0b03
#define MAIN_ROTATION					0x0b04
enum main_rotation{
	MAIN_ROTATE_OFF = 0,
	MAIN_ROTATE_CW90,
	MAIN_ROTATE_ACW90,
};
#define YUVOUT_PREVIEW					0x0b05
enum yvuout_preview{
	PREVIEW_OUTPUT_YUV422 = 0,
	PREVIEW_OUTPUT_RGB565,
};
#define PREVIEW_IMAGE_SIZE				0x0b06
enum preview_image_size{
	PREVIEW_SIZE_320_240=0x01,
	PREVIEW_SIZE_426_240=0x04,
	PREVIEW_SIZE_640_480=0x08,
	PREVIEW_SIZE_854_480=0x0b,
};
#define PREVIEW_MIRROR					0x0b07
#define PREVIEW_REVERSE				0x0b08
#define PREVIEW_ROTATION				0x0b09
enum preview_rotation{
	PREVIEW_ROTATE_OFF = 0,
	PREVIEW_ROTATE_CW90,
	PREVIEW_ROTATE_ACW90,
};
#define YUVOUT_THUMB					0x0b0a
enum yvuout_thumb{
	THUMB_OUTPUT_YUV422 = 0,
	THUMB_OUTPUT_JPEG_422,
	THUMB_OUTPUT_JPEG_420,
};
#define THUMB_IMAGE_SIZE				0x0b0b
enum thumb_image_size{
	THUMB_SIZE_160_120=0x02,
	THUMB_SIZE_320_240=0x04,
	THUMB_SIZE_426_240=0x07,
	THUMB_SIZE_640_480=0x0b,
	THUMB_SIZE_854_480=0x0e,
};
#define THUMB_MIRROR					0x0b0c
#define THUMB_REVERSE					0x0b0d
#define THUMB_ROTATION					0x0b0e
enum thumb_rotation{
	THUMB_ROTATE_OFF = 0,
	THUMB_ROTATE_CW90,
	THUMB_ROTATE_ACW90,
};
#define JPEG_SIZE_MAX					0x0b0f/*4byte*/
#define JPEG_SIZE_MIN					0x0b13/*4byte*/
#define JPEG_RATIO						0x0b17
#define JPEG_DUAL_RATIO				0x0b18
#define JPEG_DRI							0x0b19
#define JPEG_RETRY						0x0b1a
#define LED_FLASH_CONTROL				0x0b1f
enum led_flash_control{
	LED_FLASH_OFF=0,
	LED_FLASH_AUTO = 2,
	LED_FLASH_ON = 3,
};
#define PART_WDR_EN					0x0b38
enum part_wdr_en{
	PART_WDR_OFF=0,
	PART_WDR_ON,	
	PART_WDR_AUTO,
};
#define PART_WDR_LVL					0x0b39
enum part_wdr_lvl{
	PART_WDR_LOW=0x00,
	PART_WDR_MIDDLE=0x01,	
	PART_WDR_HIGH=0x02,
	PART_WDR_FAIL=0x0c,
};

#define FLASHLED_SELECT_REG 			0x0b3d    /* mx flash led select reg */
enum flashled_select {
	FLASHLED_M030,
	FLASHLED_M032,   /*default value is m032*/
};

/*m6mo Category 0xc parameter*/
#define CAP_MODE						0x0c00
enum cap_mode{
	CAP_MODE_NORMAL=0x00,
	CAP_MODE_AUTO_MULTICAP=0x01,
	CAP_MODE_ANTI_HANDSHAKING=0x0d,
	CAP_MODE_PARORAMA=0x0e,
};
#define CAP_FRM_INTERVAL				0x0c01
enum cap_frm_interval{
	CAP_FRAMERATE_5FPS=0x01,
	CAP_FRAMERATE_3P33FPS=0x02,
	CAP_FRAMERATE_2P5FPS=0x03,
	CAP_FRAMERATE_2FPS=0x04,
	CAP_FRAMERATE_1P66FPS=0x05,
	CAP_FRAMERATE_1P42FPS=0x06,
	CAP_FRAMERATE_1P25FPS=0x07,
	CAP_FRAMERATE_1P11FPS=0x08,
	CAP_FRAMERATE_1P1FPS=0x09,
};
#define CAP_FRM_COUNT					0x0c02
#define CAP_SEL_FRAME_MAIN			0x0c06
#define CAP_SEL_FRAME_PRV				0x0c07
#define CAP_SEL_FRAME_THM				0x0c08
#define CAP_TRANSFER_START			0x0c09

enum cap_transfer_start{
	CAP_TRANSFER_OFF=0x00,
	CAP_TRANSFER_MAIN=0x01,
	CAP_TRANSFER_PREVIEW=0x02,
	CAP_TRANSFER_THUMB=0x03,
	CAP_TRANSFER_MAIN1B=0x04,
	CAP_TRANSFER_MAIN2B=0x05,
	CAP_TRANSFER_MAIN3B=0x06,
	CAP_TRANSFER_MAIN4B=0x07,
};
#define START_SUPPRE					0x0c0c
#define JPEG_IMAGE_SIZE					0x0c0d/*4byte*/
#define THM_JPEG_SIZE					0x0c11/*4byte*/
#define ACT_CAP_MODE					0x0c15
#define MULTI_ERR_FLAG					0x0c18

#define COLOR_BAR_REG 					0x0d1b
#define ENABLE_COLOR_BAR				0x05

enum multi_err_flag{
	MULTI_CAP_SUCCESS=0,
	MULTI_CAP_ERROR=1,
};

/* Camera functional setting values configured by user concept */
struct m6mo_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int brightness;
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
	unsigned int scene;
	unsigned int focus_position;
	unsigned int zoom;
	unsigned int fast_shutter;
	unsigned int wdr;	
	unsigned int iso;
	unsigned int flash_mode;
	unsigned int rotation;
	unsigned int reverse;
	unsigned int mirror;
	unsigned int af_scan_mode;
};
enum state_type{
	STATE_CAPTURE_OFF=0,
	STATE_CAPTURE_ON,
};
enum shutter_type{
	STATE_SHUTTER_OFF=0,
	STATE_SHUTTER_ON,
};
enum init_type{
	STATE_UNINITIALIZED=0,
	STATE_INIT_PRIVEW,
	STATE_INIT_COMMAND,
};

#define PAN_MAX_PICTURE 100
#define MULTI_CAP_MAX_PICTURE 9

enum pan_status {
	PAN_SUCCESS = 0,
	PAN_RETRY_ERR,
	PAN_BIG_ERR,
	PAN_FATAL_ERR,	
	PAN_UNKNOWN_ERR,
	PAN_COMPLETE,
};

/*
  *panorama status
*/
struct pan_info {
	enum pan_status status;
	unsigned int extra;
};

/*M6MO irq mode*/
enum m6mo_irq_mode {
	M6MO_IRQ_NORMAL = 0,
	M6MO_IRQ_MULTI_CAP,
	M6MO_IRQ_PANORAMA,
	M6MO_IRQ_SMILE_DETECT,
};

enum firmware_status {
	FIRMWARE_NONE,
	FIRMWARE_REQUESTING,
	FIRMWARE_LOADED_OK,
	FIRMWARE_LOADED_FAIL,
};

enum multi_cap_ready_status {
	MCAP_READY,
	MCAP_READY_FAIL,	
	MCAP_NOT_READY,
};

enum pan_cap_ready_status {
	PAN_READY,
	PAN_READY_FAIL,
	PAN_NOT_READY,
};

struct m6mo_state {
	struct m6mo_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct m6mo_userset userset;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;

	int state;
	int mode;
	int shutter;
	int camera_mode;
	
	int initialized;
	int stream_on;

	int irq;
	int irq_state;
	struct completion   completion;
	struct work_struct      m6mo_work;
	struct workqueue_struct *m6mo_wq;
	
	struct m6mo_format_struct fmt;
	struct m6mo_win_size wsize;

	struct m6mo_format_struct cap_fmt;
	struct m6mo_win_size cap_wsize;
	
	unsigned char *buffer;//for firmware buffer
	
	/*panorama parameters*/
	unsigned int pan_counter;  /*panorama picture counter*/
	struct pan_info pan_info[PAN_MAX_PICTURE + 1];
	int pan_cur_index;   /*the current panorama picture index*/
	enum pan_cap_ready_status pan_ready;

	/*face detection params*/
	int smile_detection_flag;
	int smile_detection_result;

	/*multi capture params*/
	int mcap_numbers;
	int mcap_counter;
	enum multi_cap_ready_status mcap_status[MULTI_CAP_MAX_PICTURE + 1];
	enum multi_cap_ready_status mcap_ready;

	struct mutex m6mo_mutex;
	int irq_mode;

	int raw_image_flag;
	int firmware_version;
	struct wake_lock wake_lock;
	enum firmware_status fw_status;
	bool power;
	struct regulator *fled_regulator;
	bool debug_reg;
};

struct m6mo_reg m6mo_default_regs[]={
	{REG_8BIT, OUT_SELECT, MIPI_IF},
	//{REG_8BIT, MON_REVERSE_ISP, 1},
	//{REG_8BIT, MON_MIRROR_ISP, 1},
	{REG_8BIT, FSHD_EN, FSHD_ON},
	{REG_8BIT, JPEG_RATIO, 0x62},
	{REG_32BIT, IASTER_CLK_FOR_SENSOR, 0x06ddd000},//24MHZ
};
struct m6mo_reg m6mo_vga[]={
	{REG_8BIT, MON_SIZE, MON_VGA},
};
struct m6mo_reg m6mo_854x640[]={
	{REG_8BIT, MON_SIZE, MON_854X640},
};
struct m6mo_reg m6mo_svga[]={
	{REG_8BIT, MON_SIZE, MON_SVGA},
};
struct m6mo_reg m6mo_960x720[]={
	{REG_8BIT, MON_SIZE, MON_960X720},
};
struct m6mo_reg m6mo_720p[]={
	{REG_8BIT, MON_SIZE, MON_720P},
};
struct m6mo_reg m6mo_1080p[]={
	{REG_8BIT, MON_SIZE, MON_1080P},
};
static struct m6mo_win_size m6mo_win_sizes[] = {
	/* 1080P */
	{
		.width		= 1920,
		.height		= 1088,
		.regs 		= m6mo_1080p,
		.size			= ARRAY_SIZE(m6mo_1080p),
	},
	/* 720P */
	{
		.width		= 1280,
		.height		= 720,
		.regs 		= m6mo_720p,
		.size			= ARRAY_SIZE(m6mo_720p),
	},
	/* 960 * 720*/
	{
		.width		= 960,
		.height		= 720,
		.regs 		= m6mo_960x720,
		.size			= ARRAY_SIZE(m6mo_960x720),
	},
	/* 854 * 640, this resolution is pixel to pixel to our LCD */
	{
		.width		= 854,
		.height		= 640,
		.regs 		= m6mo_854x640,
		.size			= ARRAY_SIZE(m6mo_854x640),
	},	
	/* VGA */
	{
		.width		= 640,
		.height		= 480,
		.regs 		= m6mo_vga,
		.size			= ARRAY_SIZE(m6mo_vga),
	},	
};

#define N_WIN_SIZES (ARRAY_SIZE(m6mo_win_sizes))

struct m6mo_reg m6mo_cap_qvga[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_320_240},
};
struct m6mo_reg m6mo_cap_vga[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_640_480},
};
struct m6mo_reg m6mo_cap_1m[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_1280_960},
};
struct m6mo_reg m6mo_cap_2m[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_1600_1200},
};
struct m6mo_reg m6mo_cap_3m[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_2048_1536},
};
struct m6mo_reg m6mo_cap_5m[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_2560_1920},
};
struct m6mo_reg m6mo_cap_8m[]={
	{REG_8BIT, MAIN_IMAGE_SIZE, MAIN_SIZE_3264_2448},
};
static struct m6mo_win_size m6mo_cap_sizes[] = {
#if 1
	/* 8M */
	{
		.width		= 3264,
		.height		= 2448,
		.regs 		= m6mo_cap_8m,
		.size			= ARRAY_SIZE(m6mo_cap_8m),
	},
#endif	
	/* 5M */
	{
		.width		= 2560,
		.height		= 1920,
		.regs 		= m6mo_cap_5m,
		.size			= ARRAY_SIZE(m6mo_cap_5m),
	},
	/* 3M */
	{
		.width		= 2048,
		.height		= 1536,
		.regs 		= m6mo_cap_3m,
		.size			= ARRAY_SIZE(m6mo_cap_3m),
	},	
	/* 2M */
	{
		.width		= 1600,
		.height		= 1200,
		.regs 		= m6mo_cap_2m,
		.size			= ARRAY_SIZE(m6mo_cap_2m),
	},
	/* 1M */
	{
		.width		= 1280,
		.height		= 960,
		.regs 		= m6mo_cap_1m,
		.size			= ARRAY_SIZE(m6mo_cap_1m),
	},
	/* VGA */
	{
		.width		= 640,
		.height		= 480,
		.regs 		= m6mo_cap_vga,
		.size			= ARRAY_SIZE(m6mo_cap_vga),
	},
#if 0	
	/* QVGA */
	{
		.width		= 320,
		.height		= 240,
		.regs 		= m6mo_cap_qvga,
		.size			= ARRAY_SIZE(m6mo_cap_qvga),
	},
#endif	
};

#define N_CAP_SIZES (ARRAY_SIZE(m6mo_cap_sizes))

#if 0
static struct m6mo_win_size m6mo_jpeg_sizes[] = {
	/* 5M jpeg*/
	{
		.width		= 3264,
		.height		= 2448,
		.regs 		= m6mo_cap_8m,
		.size			= ARRAY_SIZE(m6mo_cap_8m),
	},
	/* 4M jpeg*/
	{
		.width		= 3264,
		.height		= 2448,
		.regs 		= m6mo_cap_8m,
		.size			= ARRAY_SIZE(m6mo_cap_8m),
	},
	/* 1M jpeg*/
	{
		.width		= 3264,
		.height		= 2448,
		.regs 		= m6mo_cap_8m,
		.size			= ARRAY_SIZE(m6mo_cap_8m),
	},	
};
#endif

struct m6mo_reg m6mo_fmt_yuv422[]={
	{REG_8BIT, YUVOUT_MAIN, MAIN_OUTPUT_YUV422},
};
struct m6mo_reg m6mo_fmt_jpeg[]={
	{REG_8BIT, YUVOUT_MAIN, MAIN_OUTPUT_JPEG_422},
};
/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct m6mo_format_struct m6mo_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mbus_code = V4L2_MBUS_FMT_VYUY8_2X8,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.regs 		= m6mo_fmt_yuv422,
		.size			= ARRAY_SIZE(m6mo_fmt_yuv422),
		.bpp		= 2,
	},
	{
		.desc		= "JPEG encoded data",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
		.mbus_code = V4L2_MBUS_FMT_JPEG_1X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.regs		= m6mo_fmt_jpeg,
		.size			= ARRAY_SIZE(m6mo_fmt_jpeg),
		.bpp		= 0,
	},
};
#define N_M6MO_FMTS ARRAY_SIZE(m6mo_formats)

/*These setting by Fujstu*/
#define FLASH_PRE_CURRENT 300000   /*300000 uA*/
#define FLASH_FULL_CURRENT 500000   /*500000 uA*/
#define FLASH_MAX_CURRENT 1000000  

#endif /* __M6MO_H__ */

