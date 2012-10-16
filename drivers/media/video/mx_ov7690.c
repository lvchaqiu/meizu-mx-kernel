/*
 * A V4L2 driver for OmniVision OV7690 cameras.
 *
 * Copyright 2011 Meizu Inc.
 *
 * Based on ov7690 camera driver:
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/ov7690_platform.h>
#include <plat/gpio-cfg.h>
#include <asm/gpio.h>
#include <linux/videodev2_samsung.h>

#include <plat/devs.h>
#include <linux/clk.h>
#include <plat/clock.h>
#include <linux/pm_runtime.h>

#include <linux/videodev2_meizu.h>
#include "mx_ov7690.h"

static inline struct ov7690_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov7690_state, sd);
}

/*
 * On most platforms, we'd rather do straight i2c I/O.
 */
static int ov7690_read(struct v4l2_subdev *sd, u8 reg, u8 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 data = reg;
	struct i2c_msg msg;
	int ret;

	/*
	 * Send out the register address...
	 */
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 1;
	msg.buf = &data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		printk(KERN_ERR "Error %d on register write\n", ret);
		return ret;
	}
	/*
	 * ...then read back the result.
	 */
	msg.flags = I2C_M_RD;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0) {
		*value = data;
		ret = 0;
	}
	return ret;
}


static int ov7690_write(struct v4l2_subdev *sd, u8 reg, u8 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg;
	unsigned char data[2] = {reg, value};
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;
	if (reg == REG_12 && (value & REG12_RESET))
		msleep(5);  /* Wait for reset to run */
	return ret;
}


#if 0
static int print_all_register(struct v4l2_subdev *sd)
{
	int i, ret;
	u8 val;

	for (i = 0; i < 0xe2; i++) {
		ret = ov7690_read(sd, i, &val);
		if (ret < 0) return ret;
		printk("reg \t0x%02x: \t0x%02x.\n", i, val);
	}
	return 0;
}
#endif
/*
 * Write a list of register settings; ff/ff stops the process.
 */
static int ov7690_write_array(struct v4l2_subdev *sd, const struct ov7690_reg vals[])
{
	int ret, i = 0;
	
	while (vals[i].addr != 0xff || vals[i].value != 0xff) {
		ret = ov7690_write(sd, vals[i].addr, vals[i].value);
		if (ret < 0) {
			pr_err("%s()->%d:write 0x%02x reg value 0x%02x error(errno = %d)!\n",
				__FUNCTION__, __LINE__, vals[i].addr, vals[i].value, ret);
			return ret;
		}
		i++;
	}
	
	return 0;
}


static int ov7690_init(struct v4l2_subdev *sd, u32 val)
{
	return ov7690_write_array(sd, ov_init_regs);
}

#if 0
static int ov7690_detect(struct v4l2_subdev *sd)
{
	unsigned char pidh, pidl;
	int ret;
	
	ret = ov7690_read(sd, REG_PIDH, &pidh);
	if (ret < 0)
		return ret;
	
	ret = ov7690_read(sd, REG_PIDL, &pidl);
	if (ret < 0)
		return ret;

	pr_debug("%s():product id is %02x%02x", __FUNCTION__, pidh, pidl);	

	return 0;
}
#endif


static int ov7690_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			     enum v4l2_mbus_pixelcode *code)
{
#if 0
	struct ov7690_format_struct *ofmt;

	TRACE_FUNC();
	if (index >= N_OV7690_FMTS)
		return -EINVAL;

	ofmt = ov7690_formats + index;
	fmt->flags = 0;
	memcpy(fmt->description, ofmt->desc, sizeof(*ofmt->desc));
	fmt->pixelformat = ofmt->pixelformat;
#endif
	return 0;
}

/*
 * Store a set of start/stop values into the camera.
 */
static int ov7690_set_hw(struct v4l2_subdev *sd, int hstart, int hstop,
		int vstart, int vstop)
{
#if 0
	int ret;
	u8 val;
	
	ret = ov7690_read(sd, REG_16, &default_win_regs);
	if (ret) return ret;

	/*set hsize lsb to 0*/
	if (val & 0x40) {
		ret = ov7690_write(sd, REG_HSIZE, val & (~0x40));
		if (ret) return ret;
	}
#endif
	return 0;
}



static int ov7690_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct ov7690_format_struct **ret_fmt,
		struct ov7690_win_size **ret_wsize)
{
	int index;
	struct ov7690_win_size *wsize;
//	struct v4l2_pix_format *pix = &fmt->fmt.pix;

	/*check the format whether it is supported */
	for (index = 0; index < N_OV7690_FMTS; index++)
		if (ov7690_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_OV7690_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = ov7690_formats[0].mbus_code;
	}
	if (ret_fmt != NULL)
		*ret_fmt = ov7690_formats + index;
	/*
	 * Fields: the OV devices claim to be progressive
	 * so we set it to none fourcely.
	 */
	fmt->field = V4L2_FIELD_NONE;  
	/*
	 * Round requested image size down to the nearest
	 * we support, but not below the smallest.
	 */
	for (wsize = ov7690_win_sizes; wsize < ov7690_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)
			break;  /*to get the smallest window size*/
	if (wsize >= ov7690_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	/*
	 * Note the size we'll actually handle.
	 */
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	//pix->bytesperline = pix->width*ov7690_formats[index].bpp;
	//pix->sizeimage = pix->height*pix->bytesperline;

	return 0;
}


/*
  * this function is to check the aqure v4l2_pix_format of user, and set the 
  * appropriate format to fmt parameter fmt, and later the user may call s_fmt to 
  * set this format.
 */
static int ov7690_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{	
	int ret;
	ret = ov7690_try_fmt_internal(sd, fmt, NULL, NULL);
	if (ret) {
		pr_err("%s()->%d:error!\n", __FUNCTION__, __LINE__);	
		return ret;
	}
	
	pr_info("%s():pix: width %d, height %d\n",
		__FUNCTION__, fmt->width, fmt->height);

	return 0;
}


static int ov7690_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int ret;
	struct ov7690_format_struct *ovfmt;
	struct ov7690_win_size *wsize;
	struct ov7690_state *state = to_state(sd);
//	unsigned char com7;

	pr_info("%s():pix: width %d, height %d\n",
		__FUNCTION__, fmt->width, fmt->height);

	ret = ov7690_try_fmt_internal(sd, fmt, &ovfmt, &wsize);
	if (ret)
		return ret;
	/*
	 * Now write the rest of the array.  Also store start/stops
	 */
	if (ovfmt->regs)
		ret = ov7690_write_array(sd, ovfmt->regs);
	if (ret) return ret;
	
	ov7690_set_hw(sd, wsize->hstart, 0, wsize->vstart,
			0);
	ret = 0;
	if (wsize->regs)
		ret = ov7690_write_array(sd, wsize->regs);
	state->fmt = ovfmt;

//	if (ret == 0)
//		ret = ov7690_write(sd, REG_CLKRC, info->clkrc);

	return 0;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 */
static int ov7690_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct ov7690_state *state = to_state(sd);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;
	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = OV7690_FRAME_RATE;
	if ((!(state->clkrc & CLK_EXT)) && (state->clkrc & CLK_SCALE))
		cp->timeperframe.denominator /= ((state->clkrc & CLK_SCALE) + 1);

	pr_info("%s():numberator %d, denominator %d.\n", __FUNCTION__, cp->timeperframe.numerator, 
		cp->timeperframe.denominator);

	return 0;
}

static int ov7690_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct v4l2_fract *tpf = &cp->timeperframe;
	struct ov7690_state *state = to_state(sd);
	int div;

	pr_info("%s():numberator %d, denominator %d.\n", __FUNCTION__, cp->timeperframe.numerator, 
		cp->timeperframe.denominator);

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (tpf->numerator == 0 || tpf->denominator == 0)
		div = 1;  /*set to max rate*/
	else
		div = (tpf->numerator * OV7690_FRAME_RATE) / tpf->denominator;
	if (div == 0)
		div = 1;
	else if (div > CLK_SCALE)
		div = CLK_SCALE;
	
	state->clkrc = div - 1;
	tpf->numerator = 1;
	tpf->denominator = OV7690_FRAME_RATE / div;

	pr_info("%s():clkrc is set to %d.\n", __FUNCTION__, state->clkrc);

//	return ov7690_write(sd, REG_CLKRC, state->clkrc);
	return 0;
}


static int ov7690_s_brightness(struct v4l2_subdev *sd, int value)
{
	u8 v;
	int ret;
	struct ov7690_state *state = to_state(sd);

	if (value == state->userset.brightness)
		return 0;

	if (value < OV7690_BRIGHTNESS_MINUS4 || value > OV7690_BRIGHTNESS_PLUS4)
		return -EINVAL;
	
	/*set SDE enable*/
	ret = ov7690_read(sd, REG_81, &v);
	if (ret) return ret;

	if (!(v & REG81_SDE)) {
		ret = ov7690_write(sd, REG_81, v | REG81_SDE);
		if (ret) return ret;
	}

	ret = ov7690_read(sd, REG_D2, &v);
	if (ret) return ret;

	if (!(v & REGD2_CONT_EN)) {
		ret = ov7690_write(sd, REG_D2, v | REGD2_CONT_EN);
		if (ret) return ret;
	}

	ret = ov7690_write_array(sd, ov7690_brightness_map[value]);
	if (ret) return ret;
	
	state->userset.brightness = value;

	return 0;
}


static int ov7690_s_hflip(struct v4l2_subdev *sd, int value)
{
	u8 v;
	int ret;
	struct ov7690_state *state = to_state(sd);

	ret = ov7690_read(sd, REG_0C, &v);
	if (ret) return ret;
	
	if (value) {
		if (v & REG0C_HMIRROR) return 0;
		else v |= REG0C_HMIRROR;
	}
	else 
	{
		if (!(v & REG0C_HMIRROR)) return 0;
		v &= ~REG0C_HMIRROR;
	}
	
	ret = ov7690_write(sd, REG_0C, v);
	if (ret) return ret;

	state->userset.hflip = value;
	
	return 0;
}


static int ov7690_s_vflip(struct v4l2_subdev *sd, int value)
{
	u8 v;
	int ret;
	struct ov7690_state *state = to_state(sd);
	
	ret = ov7690_read(sd, REG_0C, &v);
	if (value) {
		if (v & REG0C_VFLIP) return 0;
		else v |= REG0C_VFLIP;
	}
	else
	{
		if (!(v & REG0C_VFLIP)) return 0;
		else v &= ~REG0C_VFLIP;
	}
	
	ret =  ov7690_write(sd, REG_0C, v);
	if (ret) return ret;

	state->userset.vflip = value;
	
	return 0;
}

/*
 * GAIN is split between REG_GAIN and REG_VREF[7:6].  If one believes
 * the data sheet, the VREF parts should be the most significant, but
 * experience shows otherwise.  There seems to be little value in
 * messing with the VREF bits, so we leave them alone.
 */
static int ov7690_g_gain(struct v4l2_subdev *sd, int *value)
{
	int ret;
	u8 v;

	ret = ov7690_read(sd, REG_GAIN, &v);
	if (ret) return ret;
	
	*value = v;
	
	return 0;
}

static int ov7690_s_gain(struct v4l2_subdev *sd, int value)
{
	int ret;
	u8 v;

	/*turn off AGC*/
	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;

	if (v & REG13_AGC) {
		ret = ov7690_write(sd, REG_13, v & ~REG13_AGC);
		if (ret) return ret;
	}

	return ov7690_write(sd, REG_GAIN, value & 0xff);
}


static int ov7690_g_autogain(struct v4l2_subdev *sd, int *value)
{
	int ret;
	u8 v;

	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;
	
	*value = (v & REG13_AGC)?1:0;
	
	return 0;
}


static int ov7690_s_autogain(struct v4l2_subdev *sd, int value)
{
	int ret;
	u8 v;

	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;
	
	if (value) {
		if (v & REG13_AGC) return 0;
		v |= REG13_AGC;
	}
	else
	{
		if (!(v & REG13_AGC)) return 0;
		v &= ~REG13_AGC;
	}
	
	return ov7690_write(sd, REG_13, v);
}

/*
 * Exposure is spread all over the place: top 6 bits in AECHH, middle
 * 8 in AECH, and two stashed in COM1 just for the hell of it.
 */
static int ov7690_g_exp(struct v4l2_subdev *sd, int *value)
{
	int ret;
	u8 aech, aecl;

	ret = ov7690_read(sd, REG_AECL, &aecl) +
		ov7690_read(sd, REG_AECH, &aech);
	if (ret) return ret;
	
	*value = (aech << 8) | aecl;
	
	return ret;
}


static int ov7690_s_exp(struct v4l2_subdev *sd, int value)
{
	int ret;
	u8 v, aech, aecl;

	/* Have to turn off AEC as well */
	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;

	if (v & REG13_AEC) {
		ret = ov7690_write(sd, REG_13, v & ~REG13_AEC);
		if (ret) return ret;
	}

	aech = (value >> 8) & 0xff;
	aecl = value & 0xff;	
	ret = ov7690_write(sd, REG_AECH, aech) +
		ov7690_write(sd, REG_AECL, aecl);

	return ret;
}

/*
 * Tweak autoexposure.
 */
static int ov7690_g_autoexp(struct v4l2_subdev *sd, int *value)
{
	int ret;
	u8 v;

	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;

	*value = (v & REG13_AEC)?1:0;
	
	return 0;
}


static int ov7690_s_autoexp(struct v4l2_subdev *sd, int value)
{
	int ret;
	u8 v;

	if (value != 0 && value != 1) return -1;
	
	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;

	if (value) {
		if (v & REG13_AEC) return 0;
		else v |= REG13_AEC;
	}
	else {
		if (!(v & REG13_AEC)) return 0;
		else v &= ~REG13_AEC;
	}
	
	return ov7690_write(sd, REG_13, v);
}


static int ov7690_s_wb_preset(struct v4l2_subdev *sd, int value)
{
	struct ov7690_state *state = to_state(sd);
	u8 v;
	int ret = -EINVAL;

	if (value == state->userset.manual_wb)
		return 0;

	if (value < OV7690_WB_AUTO || value > OV7690_WB_CLOUDY)
		return -EINVAL;

	ret = ov7690_read(sd, REG_13, &v);
	if (ret) return ret;

	if (value == OV7690_WB_AUTO) {
		if (v & REG13_AWB) {
			ret = ov7690_write(sd, REG_13, v & (~REG13_AWB));	
			if (ret) return ret;
		}
	}
	else {
		if (!(v & REG13_AWB)) {
			ret = ov7690_write(sd, REG_13, v | REG13_AWB);	
			if (ret) return ret;
		}
	}

	ret = ov7690_write_array(sd, ov7690_wb_map[value]);
	if (ret) return ret;

	state->userset.manual_wb = value;
	
	return 0;
}

static int ov7690_s_contrast(struct v4l2_subdev *sd, int value)
{
	struct ov7690_state *state = to_state(sd);
	u8 v;
	int ret = -EINVAL;

	if (value == state->userset.contrast)
		return 0;

	if (value < OV7690_CONTRAST_MINUS4 || value > OV7690_CONTRAST_PLUS4)
		return -EINVAL;

	/*set UV average enable*/
	ret = ov7690_read(sd, REG_81, &v);
	if (ret) return ret;

	if (!(v & REG81_UVAVG)) {
		ret = ov7690_write(sd, REG_81, v | REG81_UVAVG);
		if (ret) return ret;
	}

	ret = ov7690_write_array(sd, ov7690_contrast_map[value]);
	if (ret) return ret;

	state->userset.contrast = value;
	
	return 0;
}


static int ov7690_s_saturation(struct v4l2_subdev *sd, int value)
{
	struct ov7690_state *state = to_state(sd);
	u8 v;
	int ret = -EINVAL;

	if (value == state->userset.saturation)
		return 0;

	if (value < OV7690_SATURATION_0 || value > OV7690_SATURATION_8)
		return -EINVAL;

	/*set SDE enable*/
	ret = ov7690_read(sd, REG_81, &v);
	if (ret) return ret;

	if (!(v & REG81_SDE)) {
		ret = ov7690_write(sd, REG_81, v | REG81_SDE);
		if (ret) return ret;
	}

	ret = ov7690_write_array(sd, ov7690_saturation_map[value]);
	if (ret) return ret;

	state->userset.saturation = value;
	
	return 0;
}


static int ov7690_s_effect(struct v4l2_subdev *sd, int value)
{
	struct ov7690_state *state = to_state(sd);
	u8 v;
	int ret = -EINVAL;

	if (value == state->userset.effect)
		return 0;

	if (value < OV7690_EFFECT_NORMAL || value > OV7690_EFFECT_NEGATIVE)
		return -EINVAL;

	/*set SDE enable*/
	ret = ov7690_read(sd, REG_81, &v);
	if (ret) return ret;

	if ((!(v & REG81_SDE)) && (value != OV7690_EFFECT_NORMAL)) {
		ret = ov7690_write(sd, REG_81, v | REG81_SDE);  /*enable SDE*/
		if (ret) return ret;
	}

	ret = ov7690_write_array(sd, ov7690_effect_map[value]);
	if (ret) return ret;

	state->userset.effect = value;
	
	return 0;
}


static int ov7690_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov7690_controls); i++) {
		if (ov7690_controls[i].id == qc->id) {
			memcpy(qc, &ov7690_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}


const char * const *ov7690_ctrl_get_menu(u32 id)
{
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return ov7690_querymenu_wb_preset;
	case V4L2_CID_COLORFX:
	case V4L2_CID_CAMERA_EFFECT:
		return ov7690_querymenu_effect_mode;
	case V4L2_CID_BRIGHTNESS:
		return ov7690_querymenu_brightness_mode;
	case V4L2_CID_CONTRAST:
		return ov7690_querymenu_contrast_mode;
	case V4L2_CID_SATURATION:
		return ov7690_querymenu_saturation_mode;
	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static int ov7690_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	int ret;
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	ret = ov7690_queryctrl(sd, &qctrl);
	if (ret < 0) {
		pr_err("unsupport qctrl.\n");
		return ret;
	}

	return v4l2_ctrl_query_menu(qm, &qctrl, ov7690_ctrl_get_menu(qm->id));
}

static int ov7690_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov7690_state *state = to_state(sd);
	int ret = 0;

	pr_info("%s():ctrl id %d, value %d.\n", __FUNCTION__, ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CAMERA_BRIGHTNESS:
		ctrl->value = state->userset.brightness;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = state->userset.contrast;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value = state->userset.saturation;
		break;
	case V4L2_CID_CAMERA_EFFECT:
	case V4L2_CID_COLORFX:
		ctrl->value = state->userset.effect;
		break;
/*	case V4L2_CID_HUE:
		return 0; */
	case V4L2_CID_VFLIP:
		ctrl->value = state->userset.vflip;
		break;
	case V4L2_CID_HFLIP:
		ctrl->value = state->userset.hflip;
		break;
	case V4L2_CID_GAIN:
		return ov7690_g_gain(sd, &ctrl->value);
	case V4L2_CID_AUTOGAIN:
		return ov7690_g_autogain(sd, &ctrl->value);
	case V4L2_CID_EXPOSURE:
		return ov7690_g_exp(sd, &ctrl->value);
	case V4L2_CID_EXPOSURE_AUTO:
		return ov7690_g_autoexp(sd, &ctrl->value);
	case V4L2_CID_WHITE_BALANCE_PRESET:
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = state->userset.manual_wb;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}


static int ov7690_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int ret = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CAMERA_BRIGHTNESS:
		pr_info("%s: V4L2_CID_BRIGHTNESS, ctrl->value==%d\n", 	__FUNCTION__,ctrl->value);
		ret = ov7690_s_brightness(sd, ctrl->value);
		break;
	case V4L2_CID_CONTRAST:
		pr_info("%s: V4L2_CID_CONTRAST, ctrl->value==%d\n", __FUNCTION__,ctrl->value);
		ret = ov7690_s_contrast(sd, ctrl->value);
		break;
	case V4L2_CID_SATURATION:
		pr_info("%s: V4L2_CID_SATURATION, ctrl->value==%d\n", __FUNCTION__,ctrl->value);
		ret = ov7690_s_saturation(sd, ctrl->value);
		break;
	case V4L2_CID_CAMERA_EFFECT:
	case V4L2_CID_COLORFX:
		pr_info("%s: V4L2_CID_CAMERA_EFFECT, ctrl->value==%d\n", __FUNCTION__,ctrl->value);
		ret = ov7690_s_effect(sd, ctrl->value);
		break;
/*	case V4L2_CID_HUE:
		return 0;   */
	case V4L2_CID_VFLIP:
		pr_info("%s: V4L2_CID_VFLIP, ctrl->value==%d\n", __FUNCTION__,ctrl->value);
		ret = ov7690_s_vflip(sd, ctrl->value);
		break;
	case V4L2_CID_HFLIP:
		pr_info("%s: V4L2_CID_HFLIP, ctrl->value==%d\n", __FUNCTION__,ctrl->value);
		ret = ov7690_s_hflip(sd, ctrl->value);
		break;
	case V4L2_CID_GAIN:
		return ov7690_s_gain(sd, ctrl->value);
	case V4L2_CID_AUTOGAIN:
		return ov7690_s_autogain(sd, ctrl->value);
	case V4L2_CID_EXPOSURE:
		return ov7690_s_exp(sd, ctrl->value);
	case V4L2_CID_EXPOSURE_AUTO:
		return ov7690_s_autoexp(sd, ctrl->value);
	case V4L2_CID_WHITE_BALANCE_PRESET:
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		pr_info("%s: V4L2_CID_WHITE_BALANCE_PRESET, ctrl->value==%d\n", __FUNCTION__,ctrl->value);
		ret =ov7690_s_wb_preset(sd, ctrl->value);
		break;
	}

	return ret;
}

static int ov7690_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = VGA_WIDTH;
	fsize->discrete.height = VGA_HEIGHT;

	return 0;
}

static int ov7690_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}


static int ov7690_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int ov7690_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov7690_state *state = to_state(sd);
	int ret;
	
	if (state->power == on) return 0;
	
	if (on) 
		ret = pm_runtime_get_sync(&client->dev);
	else 
		ret = pm_runtime_put_sync(&client->dev);

	if (!ret) state->power = on;

	return ret;
}

static const struct v4l2_subdev_core_ops ov7690_core_ops = {
	.init = ov7690_init,
	.reset = ov7690_reset,
	.querymenu = ov7690_querymenu,
	.queryctrl = ov7690_queryctrl,
	.g_ctrl = ov7690_g_ctrl,
	.s_ctrl = ov7690_s_ctrl,
	.s_power = ov7690_s_power,
};


static const struct v4l2_subdev_video_ops ov7690_video_ops = {
	.enum_mbus_fmt = ov7690_enum_fmt,
	.try_mbus_fmt = ov7690_try_fmt,
	.s_mbus_fmt = ov7690_s_fmt,
	.g_parm = ov7690_g_parm,
	.s_parm = ov7690_s_parm,
	.enum_framesizes = ov7690_enum_framesizes,
	.s_stream = ov7690_s_stream,
};

static const struct v4l2_subdev_ops ov7690_ops = {
	.core = &ov7690_core_ops,
	.video = &ov7690_video_ops,
};

/* ----------------------------------------------------------------------- */

static int ov7690_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct ov7690_state *state; 
	int ret = 0;

	TRACE_FUNC();

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		return -ENODEV;
	}

	state = kzalloc(sizeof(struct ov7690_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	
	sd = &state->sd;
	state->pdata = client->dev.platform_data;
	state->power = 0;

	/*register v4l2_subdev, in fact, sd and client set the private data each other.*/
	v4l2_i2c_subdev_init(sd, client, &ov7690_ops);
	add_v4l2_i2c_subdev(sd);

	pm_runtime_enable(&client->dev);

	if ((!state->pdata->init) || 
		(!state->pdata->power) ||
		(!state->pdata->clock_on)) {
		dev_err(&client->dev, "platform data has no init, power and clock_on functions\n");
		goto err_init_pdata;
	}
	
	ret = state->pdata->init(&client->dev);
	if (ret) goto err_init_pdata;

	state->fmt = &ov7690_formats[0];
	state->clkrc = 1;	/* 15fps */
	state->fps = 15;
	state->userset.exposure_bias = 128;
	state->userset.manual_wb = OV7690_WB_AUTO;
	state->userset.contrast = OV7690_CONTRAST_DEFAULT;
	state->userset.saturation = OV7690_SATURATION_4;
	state->userset.brightness = OV7690_BRIGHTNESS_DEFAULT;
	state->userset.sharpness = 0;
	state->userset.hue = 128;
	state->userset.effect = OV7690_EFFECT_NORMAL;
	state->userset.hflip = 0;
	state->userset.vflip = 0;
	
	v4l_info(client, "chip found @ 0x%02x (%s)\n",
			client->addr << 1, client->adapter->name);

	return 0;

err_init_pdata:
	kfree(state);
	return ret;
}


static int ov7690_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	del_v4l2_i2c_subdev(sd);
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	
	return 0;
}

static int ov7690_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov7690_state *state = to_state(sd);

	state->pdata->power(0);
	state->pdata->clock_on(&client->dev, 0);

	return 0;
}

static int ov7690_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov7690_state *state = to_state(sd);
	int ret;

	state->pdata->clock_on(&client->dev, 1);

	ret = state->pdata->power(1);
	if (ret) {
		state->pdata->clock_on(&client->dev, 0);
		return ret;
	}
	
	return 0;
}

static const struct dev_pm_ops ov7690_pm_ops = {
	.runtime_suspend = ov7690_runtime_suspend,
	.runtime_resume = ov7690_runtime_resume,
};

static const struct i2c_device_id ov7690_id[] = {
	{OV7690_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ov7690_id);

static struct i2c_driver ov7690_i2c_driver = {
	.driver = {
		.name = "ov7690",
#if (defined(CONFIG_EXYNOS_DEV_PD) && defined(CONFIG_PM_RUNTIME))
		.pm = &ov7690_pm_ops,
#endif
	},	
	.probe = ov7690_probe,
	.remove = ov7690_remove,
	.id_table = ov7690_id,
};

static int __init ov7690_module_init(void)
{
	return i2c_add_driver(&ov7690_i2c_driver);
}

static void __exit ov7690_module_exit(void)
{
	i2c_del_driver(&ov7690_i2c_driver);
}

module_init(ov7690_module_init);
module_exit(ov7690_module_exit);


