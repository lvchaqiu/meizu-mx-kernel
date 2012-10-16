#ifndef __OV7690_H__
#define __OV7690_H__

#define OV7690_DEV_NAME "ov7690"

#define TRACE_FUNC() printk(KERN_INFO "*******%s():%d*******\n", __FUNCTION__, __LINE__);

/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define VGA_WIDTH		640
#define VGA_HEIGHT		480
#define QVGA_WIDTH		320
#define QVGA_HEIGHT	240
#define CIF_WIDTH		352
#define CIF_HEIGHT		288
#define QCIF_WIDTH		176
#define QCIF_HEIGHT		144

/*
 * Our nominal (default) frame rate.
 */
#define OV7690_FRAME_RATE 30

#define OV7690_I2C_ADDR 0x21

/* Registers */
#define REG_GAIN	0x00	/* Gain lower 8 bits (rest in vref) */
#define REG_BLUE	0x01	/* blue gain */
#define REG_RED		0x02	/* red gain */
#define REG_GREEN	0x03	/* green gain */
#define REG_YAVG	0x04	/* Pieces of GAIN, VSTART, VSTOP */
#define REG_BAVE	0x05	/* U/B Average level */
#define REG_RAVE	0x06	/* Y/Gb Average level */
#define REG_GAVE	0x07	/* Y/Gb Average level */

#define REG_PID		0x0a	/* Product ID MSB */
#define REG_VER		0x0b	/* Product ID LSB */

#define REG_0C				0x0c	/* Control 3 */
#define REG0C_VFLIP			0x80	  /* vertical flip*/
#define REG0C_HMIRROR		0x40	  /* horizontal mirror */
#define REG0C_BRSWAP		0x20	  /* Byte swap */
#define REG0C_YUSWAP		0x10	  /* YU/YV swap */
#define REG0C_REVERSE_DATA	0x08	  /* Enable clock pin state*/
#define REG0C_CLOCKOUT		0x04	  /* Enable clock pin state*/
#define REG0C_DATAOUT		0x02	  /* Enable data output */
#define REG0C_CBAR			0x01	  /* color bar enable*/

#define REG_0D	0x0d	/* Control 4 */
#define REG_0E	0x0e	/* sleep mode, data range */
#define REG0E_SLEEP_MODE 		 0x08
#define REG0E_NOFULL_DATA_RANGE  0x04

#define REG_AECH	0x0f	/* most bits of AEC value  */
#define REG_AECL	0x10	/* least bits of AEC value */

#define REG_CLKRC	0x11	/* Clocl control */
#define CLK_EXT	  	0x40	  /* Use external clock directly */
#define CLK_SCALE	0x3f	  /* Mask for internal clock scale */

#define REG_12			0x12
#define REG12_RESET	  	0x80	  /* Register reset */
#define REG12_CCIR656	0x20
#define REG12_RGB565	0x04	  /* RGB565 */
#define REG12_FMT_MASK	0x03
#define REG12_YUV		0x00	  /* YUV */
#define REG12_RGB		0x02	  /* bits 0 and 2 - RGB format */
#define REG12_BAYER	  	0x01	  /* Bayer format */
#define REG12_PBAYER	  	0x03	  /* "Processed bayer" */

#define REG_13			0x13
#define REG13_FASTAEC	0x80	  /* Enable fast AGC/AEC */
#define REG13_AECSTEP	0x40	  /* Unlimited AEC step size */
#define REG13_BFILT	  	0x20	  /* Band filter enable */
#define REG13_AECBV	  	0x10	  /* enable AEC below banding value*/
#define REG13_SLE	  	0x08	  /* sub-line level exposure on/off*/
#define REG13_AGC	  	0x04	  /* Auto gain enable */
#define REG13_AWB	  	0x02	  /* White balance enable */
#define REG13_AEC	  	0x01	  /* Auto exposure enable */

#define REG_14		0x14	/* Control 9  - gain ceiling */
#define REG_15		0x15	/* Control 10 */
#define REG15_AFR	0x80	  /* enable auto frame rate control */

#define REG_16  		0x16	  /* Horiz sensor size bit[0] */
#define REG_HSTART	0x17	/* Horiz start high bits */
#define REG_HSIZE	0x18	/* Horiz sensor size bits 2X*/
#define REG_VSTART	0x19	/* Vert start high bits */
#define REG_VSIZE	0x1a	/* Vert sensor size 2X*/
#define REG_PSHFT	0x1b	/* Pixel delay after HREF */

#define REG_PIDH	0x0a	/*product ID high */
#define REG_PIDL	0x0b	/*product ID low */

#define REG_AEW		0x24	/* AGC upper limit */
#define REG_AEB		0x25	/* AGC lower limit */
#define REG_VPT		0x26	/* AGC/AEC fast mode op region */

#define REG_28			0x28	/**/
#define REG28_HSYNC	  	0x40	  /* HSYNC instead of HREF */
#define REG28_HS_NEG	  	0x20	  /* HSYNC negative */
#define REG28_HREF_REV  	0x08	  /* Reverse HREF */
#define REG28_VS_LEAD	0x04	  /* VSYNC on clock leading edge */
#define REG28_VS_NEG	  	0x02	  /* VSYNC negative */

#define REG_PLL				0x29	/*PLL divider/output control*/
#define REG_38				0x38	/*stream off control*/
#define REG38_STREAM_MASK	0x30 /*stream off control mask*/

#define REG_3E			0x3e	/* Control 14 */
#define REG_3F			0x3f	/* Control 14 */
#define REG3F_PCLK_REVS	0x40	/*reverse PCLK*/

#define REG_UV_CTR0 0x5a	/*slope of UV curve*/
#define REG_UV_CTR1 0x5b	/*slope of UV curve*/
#define REG_UV_CTR2 0x5c	/*slope of UV curve*/
#define REG_UV_CTR3 0x5d	/*slope of UV curve*/

#define REG_80				0x80	/*function enable*/
#define REG80_VARIO_PIXEL	0x80	/*variopixel enable*/
#define REG80_COLOR_INTERP	0x40	/*color interpolation enable*/
#define REG80_BLACK_PIXEL	0x20	/*black pixel correction enable*/
#define REG80_WHITE_PIXEL	0x10	/*white pixel correction enable*/
#define REG80_GAMMA			0x08	/*gamma enable*/
#define REG80_AWB_GAIN		0x04	/*awb gain enable*/
#define REG80_AWB			0x02	/*awb enable*/
#define REG80_LENS			0x01	/*lens correction enable*/

#define REG_81				0x81		/*function enable*/
#define REG81_SDE			0x20	/*specical digital effects enable*/
#define REG81_UV				0x10	/*uv adjust enable*/
#define REG81_VSCALE			0x08	/*vertical scaling enable*/
#define REG81_HSCALE			0x04	/*horizontal scaling enable*/
#define REG81_UVAVG			0x02	/*uv average enable*/
#define REG81_COLOR_MATRIX	0x01	/*color matrix enable*/

#define REG_82				0x82
#define REG82_ISP_CBAR		0x10
#define REG82_OUT_FMT_MSK	0x03

#define REG_LCC0	0x85	/*lens cotrol*/
#define REG_LCC1	0x86	/*Radius of the circular section where no compensation applies*/
#define REG_LCC2	0x87	/*X coordinate of the lens correction center relative to array center*/
#define REG_LCC3	0x88	/*Y coordinate of the lens correction center relative to array center*/
#define REG_LCC4	0x89	/*R channel compensation*/
#define REG_LCC5	0x8a	/*G channel compensation*/
#define REG_LCC6	0x8b	/*B channel compensation*/

#define REG_GAM1	0xa3
#define REG_GAM2	0xa4
#define REG_GAM3	0xa5
#define REG_GAM4	0xa6
#define REG_GAM5	0xa7
#define REG_GAM6	0xa8
#define REG_GAM7	0xa9
#define REG_GAM8	0xaa
#define REG_GAM9	0xab
#define REG_GAM10	0xac
#define REG_GAM11	0xad
#define REG_GAM12	0xae
#define REG_GAM13	0xaf
#define REG_GAM14	0xb0
#define REG_GAM15	0xb1
#define REG_GAM_SLOPE	0xb2

/*
 * This matrix defines how the colors are generated, must be
 * tweaked to adjust hue and saturation.
 *
 * Order: v-red, v-green, v-blue, u-red, u-green, u-blue
 *
 * They are nine-bit signed quantities, with the sign bit
 * stored in 0xc1.  Sign for v-red is bit 0, and up from there.
 */
#define REG_CMATRIX_BASE 0xbb
#define CMATRIX_LEN             6
#define REG_CMATRIX_SIGN 0xc1

#define REG_D2			0xd2
#define REGD2_FIXY_EN	0x80
#define REGD2_NEG_EN	0x40
#define REGD2_GRAY_EN	0x20
#define REGD2_FIXV_EN	0x10
#define REGD2_FIXU_EN	0x08
#define REGD2_CONT_EN	0x04
#define REGD2_SAT_EN	0x02
#define REGD2_HUE_EN	0x01

#define REG_YBRIGHT	0xd3
#define REG_YGAIN		0xd4
#define REG_YOFFSET		0xd5
#define REG_HUE_COS	0xd6
#define REG_HUE_SIN		0xd7
#define REG_SAT_U		0xd8
#define REG_SAT_V		0xd9
#define REG_UREG		0xda
#define REG_VREG		0xdb

#define REG_D3			0xd3
#define REG_D4			0xd4
#define REG_D5			0xd5
#define REG_D6			0xd6
#define REG_D7			0xd7
#define REG_D8			0xd8
#define REG_D9			0xd9
#define REG_DA			0xda
#define REG_DB			0xdb

#define REG_EXHCL		0x2a
#define REG_EXHCM		0x2b

#define REG_DC			0xdc

#define DEFAULT_FMT V4L2_PIX_FMT_YUYV

struct ov7690_reg {
	u8 addr;
	u8 value;
};


/* Camera functional setting values configured by user concept */
struct ov7690_userset {
	int exposure_bias;	/* V4L2_CID_EXPOSURE */
	int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	int effect;	/* Color FX (AKA Color tone) */
	int contrast;	/* V4L2_CID_CONTRAST */
	int saturation;	/* V4L2_CID_SATURATION */
	int brightness;
	int sharpness;	/* V4L2_CID_SHARPNESS */
	int hue;
	int vflip;
	int hflip;
};

static struct ov7690_reg ov7690_fmt_yuv422[] = {
    	{0xff, 0xff},
};


/*
 * Store information about the video data format.  The color matrix
 * is deeply tied into the format, so keep the relevant values here.
 * The magic matrix nubmers come from OmniVision.
 */
static struct ov7690_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;		
	struct ov7690_reg *regs;
	int cmatrix[CMATRIX_LEN];
	int bpp;   /* Bytes per pixel */
} ov7690_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mbus_code = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_SRGB,		
		.regs 		= ov7690_fmt_yuv422,
		.cmatrix	= { 128, -128, 0, -34, -94, 128 },
		.bpp		= 2,
	},
};

#define N_OV7690_FMTS ARRAY_SIZE(ov7690_formats)

/*
 * Information we maintain about a known sensor.
 */
struct ov7690_state {
	struct v4l2_subdev sd;
	struct ov7690_format_struct *fmt;  /* Current format */
	struct v4l2_pix_format pix;
	int freq;
	int is_mipi;
	int fps;
	u8 clkrc;			/* Clock divider value */
	struct ov7690_userset userset;
	struct ov7690_platform_data *pdata;
	int power;
};

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct ov7690_win_size {
	int	width;
	int	height;
	unsigned char com7_bit;
	int	hstart;		/* Start/stop values for the camera.  Note */
	int	hsize;		/* that they do not always make complete */
	int	vstart;		/* sense to humans, but evidently the sensor */
	int	vsize;		/* will do the right thing... */
	struct ov7690_reg *regs; /* Regs to tweak */
/* h/vref stuff */
} ov7690_win_sizes[] = {
	/* VGA */
	{
		.width	= VGA_WIDTH,
		.height	= VGA_HEIGHT,
		.hstart	= 0x69,   /*These values from sharp*/
		.hsize	= 640,
		.vstart	= 0x11,
		.vsize	= 480,
		.regs 	= NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(ov7690_win_sizes))

#if 0
static struct ov7690_reg default_win_regs[] = {
	{0x17, 0x69},  /*===Resolution===*/
	{0x18, 0xa4},
	{0x19, 0x0c}, 
	{0x1a, 0xf6}, 
	{0xc8, 0x02}, 
	{0xc9, 0x80},  /*ISP input hsize (640)*/
	{0xca, 0x01},
	{0xcb, 0xe0},  /*ISP input vsize (480)*/
	{0xcc, 0x02},
	{0xcd, 0x80}, /*ISP output hsize (640)*/
	{0xce, 0x01},
	{0xcf, 0xe0},  /*ISP output vsize (480)*/
};
#endif


/*WLCS100903CV23qfps_30-15fps.ccf setting from sharp*/
#if 0
static struct ov7690_reg sharp_init_regs[] = {
	{0x12, 0x80},   /*reset*/ 
	{0x48, 0x42}, 
	{0x41, 0x43}, 
	{0x81, 0xff}, 
	{0x21, 0x44}, 
	{0x16, 0x03}, 
	{0x39, 0x80}, 
	{0x12, 0x00},
	{0x82, 0x03}, 
	{0xd0, 0x48}, 
	{0x80, 0x7f}, 
	{0x3e, 0x30}, 
	{0x22, 0x00}, 
	{0x17, 0x69},  /*===Resolution===*/
	{0x18, 0xa4},
	{0x19, 0x0c}, 
	{0x1a, 0xf6}, 
	{0xc8, 0x02}, 
	{0xc9, 0x80},  /*ISP input hsize (640)*/
	{0xca, 0x01},
	{0xcb, 0xe0},  /*ISP input vsize (480)*/
	{0xcc, 0x02},
	{0xcd, 0x80}, /*ISP output hsize (640)*/
	{0xce, 0x01},
	{0xcf, 0xe0},  /*ISP output vsize (480)*/
	{0x85, 0x90},  /*===Lens Correction==*/
	{0x86, 0x00},
	{0x87, 0x00}, 
	{0x88, 0x10}, 
	{0x89, 0x30}, 
	{0x8a, 0x29}, 
	{0x8b, 0x26}, 
	{0xbb, 0x80},  /*====Color Matrix====*/
	{0xbc, 0x62},
	{0xbd, 0x1e}, 
	{0xbe, 0x26}, 
	{0xbf, 0x7b}, 
	{0xc0, 0xac}, 
	{0xc1, 0x1e}, 
	{0xb7, 0x05},  /*===Edge + Denoise====*/
	{0xb8, 0x05}, 
	{0xb9, 0x02}, 
	{0xba, 0x20}, 
	{0x5a, 0x14},  /*===UVAdjust====*/
	{0x5b, 0xa2},
	{0x5c, 0x70}, 
	{0x5d, 0x20}, 
	{0x24, 0x88},  /*====AEC/AGC target====*/
	{0x25, 0x78},
	{0x26, 0xb4}, 
	{0xa3, 0x17},  /*====Gamma====*/
	{0xa4, 0x20},  
	{0xa5, 0x34}, 
	{0xa6, 0x57}, 
	{0xa7, 0x68}, 
	{0xa8, 0x77}, 
	{0xa9, 0x85}, 
	{0xaa, 0x91}, 
	{0xab, 0x9c}, 
	{0xac, 0xa7},
	{0xad, 0xb9}, 
	{0xae, 0xc8}, 
	{0xaf, 0xdd}, 
	{0xb0, 0xea}, 
	{0xb1, 0xf1}, 
	{0xb2, 0x06}, 
	{0x8c, 0x5d},    /*auto while balance, 0x8c~0xa2*/
	{0x8d, 0x11}, 
	{0x8e, 0x12}, 
	{0x8f, 0x11}, 
	{0x90, 0x50}, 
	{0x91, 0x22}, 
	{0x92, 0xd1}, 
	{0x93, 0xa7}, 
	{0x94, 0x23},
	{0x95, 0x3b}, 
	{0x96, 0xff}, 
	{0x97, 0x00}, 
	{0x98, 0x4a}, 
	{0x99, 0x46}, 
	{0x9a, 0x3d}, 
	{0x9b, 0x3a}, 
	{0x9c, 0xf0}, 
	{0x9d, 0xf0}, 
	{0x9e, 0xf0}, 
	{0x9f, 0xff}, 
	{0xa0, 0x56}, 
	{0xa1, 0x55}, 
	{0xa2, 0x13}, 
	{0x14, 0x20},  /*==General Control==*/
	{0x13, 0xf7}, 
	{0x28, 0x02}, 
	{0x0c, 0x56},  /*Mirror*/
//	{0x15, 0x90},  /*auto frame, Max reduction to 1/2 frame rate*/
	{0x11, 0x01},  /*the max Framerate 15 fps*/
	{0x50, 0x26},   
	{0x51, 0x1f}, 
	{0x21, 0x9f},
	{0x20, 0xc0},

	/*the follows are added by sharp in 24/8/2011*/
	{0xb4, 0x26},
	{0xb6, 0x04},
	{0xd2, 0x04},
	{0xd4, 0x20},
	{0xd5, 0x20},
	{0xd3, 0x10},
	{0xd8, 0x60},
	{0xd9, 0x60},
	{0xb5, 0x06},
	{0x24, 0x78},
	{0x25, 0x68},
	{0x26, 0xb3},

	{0xff, 0xff},	/* END MARKER */		
};
#endif

static struct ov7690_reg ov_init_regs[] = {
	{0x12, 0x80},
	{0x0c, 0x56},

	{0x48, 0x42},
	{0x41, 0x43}, 
	{0x4c, 0x73},

	{0x81, 0xef},
	{0x21, 0x44},	
	{0x16, 0x03},
	{0x39, 0x80},
	{0x1e, 0xb1},
	//===Format===;;
	{0x12, 0x00},
	{0x82, 0x03},
	{0xd0, 0x48},
	{0x80, 0x7f},
	{0x3e, 0x30},
	{0x22, 0x00},
	//===Resolution===;;
	{0x17, 0x69},
	{0x18, 0xa4},
	{0x19, 0x0c},
	{0x1a, 0xf6},

	{0xc8, 0x02},  
	{0xc9, 0x80}, //ISP input hsize (640)
	{0xca, 0x01},  
	{0xcb, 0xe0}, //ISP input vsize (480)

	{0xcc, 0x02},  
	{0xcd, 0x80}, //ISP output hsize (640)
	{0xce, 0x01},  
	{0xcf, 0xe0}, //ISP output vsize (480)

	//===Lens Correction==;;
	{0x80, 0x7f},
	{0x85, 0x10},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
	{0x89, 0x35},
	{0x8a, 0x30},
	{0x8b, 0x30},
	//====Color Matrix====;;
	{0xbb, 0x80},
	{0xbc, 0x62},
	{0xbd, 0x1E},
	{0xbe, 0x26},
	{0xbf, 0x7B},
	{0xc0, 0xAC},
	{0xc1, 0x1E},
	//===Edge + Denoise====;;
	{0xb7, 0x05},  //0c
	{0xb8, 0x05},
	{0xb9, 0x02},
	{0xba, 0x20},

	//===UVAdjust====;;
	{0x5A, 0x14},
	{0x5B, 0xa2},
	{0x5C, 0x70},
	{0x5d, 0x20},
	//====AEC/AGC target====;;
	{0x24, 0x88},
	{0x25, 0x78},
	{0x26, 0xb4},
	//====Gamma====;;
	{0xa3, 0x17},
	{0xa4, 0x20},
	{0xa5, 0x34},
	{0xa6, 0x57},
	{0xa7, 0x68},
	{0xa8, 0x77},
	{0xa9, 0x85},
	{0xaa, 0x91},
	{0xab, 0x9c},
	{0xac, 0xa7},
	{0xad, 0xb9},
	{0xae, 0xc8},
	{0xaf, 0xdd},
	{0xb0, 0xea},
	{0xb1, 0xf1},
	{0xb2, 0x06},
	//===AWB===;;
	//==Advance==;;
	{0x8c, 0x5d},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x11},
	{0x90, 0x50},
	{0x91, 0x22},
	{0x92, 0xd1},
	{0x93, 0xa7},
	{0x94, 0x23},
	{0x24, 0x88},
	{0x25, 0x78},
	{0x26, 0xc4},
	{0x95, 0x3b},
	{0x96, 0xff},
	{0x97, 0x00},
	{0x98, 0x4a},
	{0x99, 0x46},
	{0x9a, 0x3d},
	{0x9b, 0x3a},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
	{0xa0, 0x56},
	{0xa1, 0x55},
	{0xa2, 0x13},
	//==General Control==;;
	{0x50, 0x4d},
	{0x51, 0x3f},
	{0x21, 0x57},	//Steps
	{0x20, 0x00},

	{0x14, 0x29},
	{0x13, 0xf7},
	{0x11, 0x01},
	{0x68, 0xb0},
	{0x42, 0x0d}, 
	{0xd2, 0x07},
	
	{0xff, 0xff},	/* END MARKER */		
};

/*
 *White Balance (presets)
 */
static const struct ov7690_reg ov7690_wb_auto[] = {
	{0x8c, 0x5c},   
	{0x8d, 0x11},   
	{0x8e, 0x12},   
	{0x8f, 0x19},   
	{0x90, 0x50},   
	{0x91, 0x20},   
	{0x92, 0x96},   
	{0x93, 0x80},   
	{0x94, 0x13},
	{0x95, 0x1b},   
	{0x96, 0xff},   
	{0x97, 0x00},   
	{0x98, 0x3d},   
	{0x99, 0x36},   
	{0x9a, 0x51},   
	{0x9b, 0x43},   
	{0x9c, 0xf0},   
	{0x9d, 0xf0},   
	{0x9e, 0xf0},   
	{0x9f, 0xff},   
	{0xa0, 0x68},   
	{0xa1, 0x62},   
	{0xa2, 0x0e},   

	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_wb_alight[] = {
	{0x8c, 0x5c},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x19},
	{0x90, 0x50},
	{0x91, 0x20},
	{0x92, 0x85},
	{0x93, 0x87},
	{0x94, 0x0e},
	{0x95, 0x06},
	{0x96, 0xff},
	{0x97, 0x00},
	{0x98, 0x51},
	{0x99, 0x2b},
	{0x9a, 0x3f},
	{0x9b, 0x3f},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
	{0xa0, 0xa7},
	{0xa1, 0x4f},
	{0xa2, 0x0d},
	{0x01, 0x5e},
	{0x02, 0x4a},
	{0x03, 0x40},

	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_wb_cwflight[] = {
	{0x8c, 0x5c},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x19},
	{0x90, 0x50},
	{0x91, 0x20},
	{0x92, 0x85},
	{0x93, 0x87},
	{0x94, 0x0e},
	{0x95, 0x06},
	{0x96, 0xff},
	{0x97, 0x00},
	{0x98, 0x51},
	{0x99, 0x2b},
	{0x9a, 0x3f},
	{0x9b, 0x3f},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
	{0xa0, 0xa7},
	{0xa1, 0x4f},
	{0xa2, 0x0d},
	{0x01, 0x5c},
	{0x02, 0x54},
	{0x03, 0x40},
	
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_wb_daylight[] = {
	{0x8c, 0x5c},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x19},
	{0x90, 0x50},
	{0x91, 0x20},
	{0x92, 0x85},
	{0x93, 0x87},
	{0x94, 0x0e},
	{0x95, 0x06},
	{0x96, 0xff},
	{0x97, 0x00},
	{0x98, 0x51},
	{0x99, 0x2b},
	{0x9a, 0x3f},
	{0x9b, 0x3f},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
	{0xa0, 0xa7},
	{0xa1, 0x4f},
	{0xa2, 0x0d},
	{0x01, 0x47},
	{0x02, 0x5e},
	{0x03, 0x40},
	
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_wb_cloudy[] = {
	{0x8c, 0x5c},
	{0x8d, 0x11},
	{0x8e, 0x12},
	{0x8f, 0x19},
	{0x90, 0x50},
	{0x91, 0x20},
	{0x92, 0x85},
	{0x93, 0x87},
	{0x94, 0x0e},
	{0x95, 0x06},
	{0x96, 0xff},
	{0x97, 0x00},
	{0x98, 0x51},
	{0x99, 0x2b},
	{0x9a, 0x3f},
	{0x9b, 0x3f},
	{0x9c, 0xf0},
	{0x9d, 0xf0},
	{0x9e, 0xf0},
	{0x9f, 0xff},
	{0xa0, 0xa7},
	{0xa1, 0x4f},
	{0xa2, 0x0d},
	{0x01, 0x61},
	{0x02, 0x5a},
	{0x03, 0x40},
	
	{0xff, 0xff},
};

static const struct ov7690_reg *ov7690_wb_map[] = {
	ov7690_wb_auto,
	ov7690_wb_alight,
	ov7690_wb_cwflight,
	ov7690_wb_daylight,
	ov7690_wb_cloudy,
};

enum ov7690_wb_mode {	
	OV7690_WB_AUTO,
	OV7690_WB_ALIGHT,
	OV7690_WB_DWFLIGHT,
	OV7690_WB_DAYLIGHT,
	OV7690_WB_CLOUDY,
};

static const struct ov7690_reg ov7690_brightness_minus4[] = {
	{REG_D3, 0x40},
	{REG_DC, 0x08},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_minus3[] = {
	{REG_D3, 0x30},
	{REG_DC, 0x08},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_minus2[] = {
	{REG_D3, 0x20},
	{REG_DC, 0x08},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_minus1[] = {
	{REG_D3, 0x10},
	{REG_DC, 0x08},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_default[] = {
	{REG_D3, 0x00},
	{REG_DC, 0x00},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_plus1[] = {
	{REG_D3, 0x10},
	{REG_DC, 0x00},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_plus2[] = {
	{REG_D3, 0x20},
	{REG_DC, 0x00},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_plus3[] = {
	{REG_D3, 0x30},
	{REG_DC, 0x00},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_brightness_plus4[] = {
	{REG_D3, 0x40},
	{REG_DC, 0x00},
	{0xff, 0xff},
};

static const struct ov7690_reg *ov7690_brightness_map[] = {
	ov7690_brightness_minus4,
	ov7690_brightness_minus3,
	ov7690_brightness_minus2,
	ov7690_brightness_minus1,
	ov7690_brightness_default,
	ov7690_brightness_plus1,
	ov7690_brightness_plus2,
	ov7690_brightness_plus3,
	ov7690_brightness_plus4,
};

enum ov7690_brightness_mode {	
	OV7690_BRIGHTNESS_MINUS4,
	OV7690_BRIGHTNESS_MINUS3,
	OV7690_BRIGHTNESS_MINUS2,
	OV7690_BRIGHTNESS_MINUS1,
	OV7690_BRIGHTNESS_DEFAULT,
	OV7690_BRIGHTNESS_PLUS1,
	OV7690_BRIGHTNESS_PLUS2,
	OV7690_BRIGHTNESS_PLUS3,
	OV7690_BRIGHTNESS_PLUS4,
};

static const struct ov7690_reg ov7690_contrast_minus4[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x10},
	{REG_D3, 0xd0},
	{REG_DC, 0x04},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_minus3[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x14},
	{REG_D3, 0x80},
	{REG_DC, 0x04},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_minus2[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x18},
	{REG_D3, 0x48},
	{REG_DC, 0x04},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_minus1[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x1c},
	{REG_D3, 0x20},
	{REG_DC, 0x04},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_default[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x20},
	{REG_D3, 0x00},
	{REG_DC, 0x00},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_plus1[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x24},
	{REG_D3, 0x00},
	{REG_DC, 0x00},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_plus2[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x28},
	{REG_D3, 0x00},
	{REG_DC, 0x00},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_plus3[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x2c},
	{REG_D3, 0x00},
	{REG_DC, 0x00},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_contrast_plus4[] = {
	{REG_D5, 0x20},
	{REG_D4, 0x30},
	{REG_D3, 0x00},
	{REG_DC, 0x00},
	{REG_D2, 0x04},
	{0xff, 0xff},
};

static const struct ov7690_reg *ov7690_contrast_map[] = {
	ov7690_contrast_minus4,
	ov7690_contrast_minus3,
	ov7690_contrast_minus2,
	ov7690_contrast_minus1,
	ov7690_contrast_default,
	ov7690_contrast_plus1,
	ov7690_contrast_plus2,
	ov7690_contrast_plus3,
	ov7690_contrast_plus4,
};

enum ov7690_contrast_mode {
	OV7690_CONTRAST_MINUS4,
	OV7690_CONTRAST_MINUS3,
	OV7690_CONTRAST_MINUS2,
	OV7690_CONTRAST_MINUS1,
	OV7690_CONTRAST_DEFAULT,
	OV7690_CONTRAST_PLUS1,
	OV7690_CONTRAST_PLUS2,
	OV7690_CONTRAST_PLUS3,
	OV7690_CONTRAST_PLUS4,
};

static const struct ov7690_reg ov7690_saturation_0[] = {
	{REG_D8, 0x00},
	{REG_D9, 0x00},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0
};

static const struct ov7690_reg ov7690_saturation_1[] = {
	{REG_D8, 0x10},
	{REG_D9, 0x10},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0.25
};

static const struct ov7690_reg ov7690_saturation_2[] = {
	{REG_D8, 0x20},
	{REG_D9, 0x20},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0.5
};

static const struct ov7690_reg ov7690_saturation_3[] = {
	{REG_D8, 0x30},
	{REG_D9, 0x30},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0.75
};

static const struct ov7690_reg ov7690_saturation_4[] = {
	{REG_D8, 0x40},
	{REG_D9, 0x40},
	{REG_D2, 0x02},
	{0xff, 0xff},  // 1
};

static const struct ov7690_reg ov7690_saturation_5[] = {
	{REG_D8, 0x50},
	{REG_D9, 0x50},
	{REG_D2, 0x02},
	{0xff, 0xff},  // 1.25
};

static const struct ov7690_reg ov7690_saturation_6[] = {
	{REG_D8, 0x60},
	{REG_D9, 0x60},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0.75
};

static const struct ov7690_reg ov7690_saturation_7[] = {
	{REG_D8, 0x70},
	{REG_D9, 0x70},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0.75
};

static const struct ov7690_reg ov7690_saturation_8[] = {
	{REG_D8, 0x80},
	{REG_D9, 0x80},
	{REG_D2, 0x02},
	{0xff, 0xff},  //0.75
};

static const struct ov7690_reg *ov7690_saturation_map[] = {
	ov7690_saturation_0,
	ov7690_saturation_1,
	ov7690_saturation_2,
	ov7690_saturation_3,
	ov7690_saturation_4,
	ov7690_saturation_5,
	ov7690_saturation_6,
	ov7690_saturation_7,
	ov7690_saturation_8,
};

enum ov7690_saturation_mode {
	OV7690_SATURATION_0,
	OV7690_SATURATION_1,
	OV7690_SATURATION_2,
	OV7690_SATURATION_3,
	OV7690_SATURATION_4,
	OV7690_SATURATION_5,
	OV7690_SATURATION_6,
	OV7690_SATURATION_7,
	OV7690_SATURATION_8,
};

static const struct ov7690_reg ov7690_effect_normal[] = {
	{REG_28, 0x00},
	{REG_DA, 0x80},
	{REG_DB, 0x80},
	{0xff, 0xff}, 
};

static const struct ov7690_reg ov7690_effect_bw[] = {
	{REG_28, 0x00},
	{REG_D2, 0x18},
	{REG_DA, 0x80},
	{REG_DB, 0x80},
	{0xff, 0xff}, 
};

static const struct ov7690_reg ov7690_effect_bluish[] = {
	{REG_28, 0x00},
	{REG_D2, 0x18},
	{REG_DA, 0xa0},
	{REG_DB, 0x40},
	{0xff, 0xff}, 
};

static const struct ov7690_reg ov7690_effect_sepia[] = {
	{REG_28, 0x00},
	{REG_D2, 0x18},
	{REG_DA, 0x40},
	{REG_DB, 0xa0},
	{0xff, 0xff}, 
};


static const struct ov7690_reg ov7690_effect_reddish[] = {
	{REG_28, 0x00},
	{REG_D2, 0x18},
	{REG_DA, 0x80},
	{REG_DB, 0xc0},
	{0xff, 0xff}, 
};

static const struct ov7690_reg ov7690_effect_greenish[] = {
	{REG_28, 0x00},
	{REG_D2, 0x18},
	{REG_DA, 0x60},
	{REG_DB, 0x60},
	{0xff, 0xff},
};

static const struct ov7690_reg ov7690_effect_negative[] = {
	{REG_28, 0x80},
	{REG_D2, 0x40},
	{0xff, 0xff},
};


static const struct ov7690_reg *ov7690_effect_map[] = {
	ov7690_effect_normal,
	ov7690_effect_bw,
	ov7690_effect_bluish,
	ov7690_effect_sepia,
	ov7690_effect_reddish,
	ov7690_effect_greenish,
	ov7690_effect_negative,
};

enum ov7690_effect_mode {
	OV7690_EFFECT_NORMAL,
	OV7690_EFFECT_BW,
	OV7690_EFFECT_BLUISH,
	OV7690_EFFECT_SEPIA,
	OV7690_EFFECT_REDDISH,
	OV7690_EFFECT_GREENISH,
	OV7690_EFFECT_NEGATIVE,
};

static const char *ov7690_querymenu_wb_preset[] = {
	"WB auto", "WB alight", "WB cwflight", "WB daylight",  "WB cloudy", NULL
};

static const char *ov7690_querymenu_effect_mode[] = {
	"Effect normal", "Effect B/W", "Effect bluish",
	"Effect sepia", "Effect reddish", "greenish", "negative", NULL
};

static const char *ov7690_querymenu_brightness_mode[] = {
	"-4EV", "-3EV", "-2EV", "-1EV", "0", "+1EV", "+2EV", "+3EV", "+4EV", NULL
};

static const char *ov7690_querymenu_contrast_mode[] = {
	"-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", NULL
};

static const char *ov7690_querymenu_saturation_mode[] = {
	"UV_0x", "UV_0.25x", "UV_0.5x", "UV_0.75x", "UV_1x", "UV_1.25x", "UV_1.5x", 
	"UV_1.75x", "UV_2.0x", NULL
};


static struct v4l2_queryctrl ov7690_controls[] = {
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov7690_wb_map)-1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = 65535,
		.step = 1,
		.default_value = 496,
	},
	{
		.id = V4L2_CID_EXPOSURE_AUTO,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Exposure auto",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_GAIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Gain cotrol",
		.minimum = 0,
		.maximum = 255,
		.step = 1,
		.default_value = 128,
	},
	{
		.id = V4L2_CID_AUTOGAIN,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto gain contrl",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov7690_effect_map) - 1,
		.step = 1,
		.default_value = OV7690_EFFECT_NORMAL,
	},
	{
		.id = V4L2_CID_VFLIP,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "vertically flip",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_HFLIP,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Horizontally mirror",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Contrast",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov7690_contrast_map)-1,
		.step = 1,
		.default_value = OV7690_CONTRAST_DEFAULT,//0
	},
	{
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Saturation",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov7690_saturation_map)-1,
		.step = 1,
		.default_value = OV7690_SATURATION_4,//0
	},
	{
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Brightness",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov7690_brightness_map)-1,
		.step = 1,
		.default_value = OV7690_BRIGHTNESS_DEFAULT,//0
	},
};

#endif   
