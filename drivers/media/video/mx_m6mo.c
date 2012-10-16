
/*
 * Driver for M9 M6MO (8M camera) from Meizu Inc.
 * 
 * 1/4" 8Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * Copyright (C) 2010, WenBin Wu<wenbinwu@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/m6mo_platform.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/slab.h>
#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/firmware.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include "mx_m6mo.h"
#include <linux/videodev2_meizu.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio-common.h>
#include <asm/mach-types.h>


static void m6mo_enable_root_irq(struct v4l2_subdev *sd);
static void m6mo_enable_irq(struct v4l2_subdev *sd);
static int m6mo_init(struct v4l2_subdev *sd, u32 val);
static int m6mo_cancel_caf(struct v4l2_subdev *sd);
static int m6mo_s_power(struct v4l2_subdev *sd, int on);
static int m6mo_set_power_clock(struct v4l2_subdev *sd, bool enable);


/***************** factory test mode functions ************/

static int m6mo_is_factory_test_mode(void)
{
	return mx_is_factory_test_mode(MX_FACTORY_TEST_CAMERA);
}

static void m6mo_init_factory_test_mode(void)
{
	pr_info("%s\n", __func__);
	
	mx_set_factory_test_led(1);
}

static void m6mo_factory_test_success(void)
{
	int onoff = 0;

	pr_info("%s()", __func__);

	/*we don't return in factory test mode*/
	while (1) {
		mx_set_factory_test_led(onoff);
		msleep(200);
		onoff = !onoff;
	}
}

static void m6mo_factory_test_fail(void)
{
	pr_info("%s()", __func__);
	
	mx_set_factory_test_led(0);
}

static void m6mo_set_firmware_status(struct m6mo_state *state, enum firmware_status status)
{
	switch (status) {
	case FIRMWARE_NONE:
		break;
	case FIRMWARE_REQUESTING:
		if (m6mo_is_factory_test_mode())
			m6mo_init_factory_test_mode();
		break;
	case FIRMWARE_LOADED_OK:
		if (m6mo_is_factory_test_mode()) {
			wake_lock(&state->wake_lock);   /*don't sleep*/
			m6mo_factory_test_success();
		}
		break;
	case FIRMWARE_LOADED_FAIL:
		if (m6mo_is_factory_test_mode())
			m6mo_factory_test_fail();
		break;
	default:
		return;
	}
	state->fw_status = status;
}

/*
  * get firmware version from file
*/
static int m6mo_get_new_firmware_version(struct m6mo_state *state, 
	const struct firmware *fw)
{
	return ((fw->data[0x0016fffc] << 8) | fw->data[0x0016fffd]);
}

static inline struct m6mo_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct m6mo_state, sd);
}

int m6mo_i2c_read_category(struct i2c_client *client, struct m6mo_command *cmd)
{
	struct i2c_msg msg[2];
	int i,offset;	
	int err = 0;
	int retry = 0;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct m6mo_state *state = to_state(sd);

	if (!client->adapter)
		return -ENODEV;
	if(cmd->data_size>4)
		return -EINVAL;
again:
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = cmd->content_size;
	msg[0].buf = cmd->outbuf;

	cmd->outbuf[0] = cmd->content_size;/*content size*/
	cmd->outbuf[1] = cmd->cmd_type;/*command type*/
	cmd->outbuf[2] = (cmd->address >> 8) & 0xff;/* address: 15-7 */
	cmd->outbuf[3] = (cmd->address >> 0) & 0xff;
	cmd->outbuf[4] = cmd->data_size;/*N : 7-0bit, read size*/

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = cmd->data_size + 1;
	msg[1].buf = cmd->inbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err >= 0 && cmd->inbuf[0] == cmd->data_size + 1)
	{
		cmd->data = 0;
		for(i=0; i<cmd->data_size; i++)
		{
			
			offset = (cmd->data_size-i-1)*8;
			cmd->data += cmd->inbuf[i+1]<<offset;
		}
		if (state->debug_reg)
			pr_info("%s: 0x%04x =0x%x\n", __func__, cmd->address, cmd->data);
		return err;	/* Returns here on success */
	}

	/*if firmware loading, return immediately*/
	if (state->fw_status != FIRMWARE_LOADED_OK) 
		return err;
	
	/* abnormal case: retry 2 times */
	if (retry < 10) {
		dev_err(&client->dev, "%s: address: 0x%04x, err = %d, cmd->inbuf[0] = 0x%02x, cmd->data_size = 0x%02x\n", 
			__func__, cmd->address, err, cmd->inbuf[0], cmd->data_size);
		retry++;
		msleep(100);
		goto again;
	}
	return -1;

}
int m6mo_i2c_read_category_8bit(struct v4l2_subdev *sd, u16 addr, u32 *value)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_READ_PARAMETER;
	cmd.content_size = 5;
	cmd.data_size = 1;

	if (m6mo_i2c_read_category(client, &cmd)<0)
	{
		return -1;
	}
	*value = cmd.data;
	return 0;
}
int m6mo_i2c_read_category_16bit(struct v4l2_subdev *sd, u16 addr, u32 *value)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_READ_PARAMETER;
	cmd.content_size = 5;
	cmd.data_size = 2;

	if (m6mo_i2c_read_category(client, &cmd)<0)
	{
		return -1;
	}
	*value = cmd.data;
	return 0;
}
int m6mo_i2c_read_category_32bit(struct v4l2_subdev *sd, u16 addr, u32 *value)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_READ_PARAMETER;
	cmd.content_size = 5;
	cmd.data_size = 4;

	if (m6mo_i2c_read_category(client, &cmd)<0)
	{
		return -1;
	}
	*value = cmd.data;
	return 0;
}

int m6mo_i2c_write_category(struct i2c_client *client, struct m6mo_command *cmd)
{
	struct i2c_msg msg[2];
	int i,offset;		
	int err = 0;
	int retry = 0;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct m6mo_state *state = to_state(sd);

	if (!client->adapter)
		return -ENODEV;
	if(cmd->data_size>4)
		return -EINVAL;
again:
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = cmd->content_size;
	msg[0].buf = cmd->outbuf;

	cmd->outbuf[0] = cmd->content_size;/*content size*/
	cmd->outbuf[1] = cmd->cmd_type;/*command type*/
	cmd->outbuf[2] = (cmd->address >> 8) & 0xff;/* address: 15-7 */
	cmd->outbuf[3] = (cmd->address >> 0) & 0xff;
	for(i=0; i<cmd->data_size; i++)
	{
		
		offset = (cmd->data_size-i-1)*8;
		cmd->outbuf[i+4] = (cmd->data >> offset) & 0xff;/*N : 7-0bit, read size*/
	}

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
	{
		if (state->debug_reg)
			pr_info("%s: 0x%04x=0x%x\n", __func__, cmd->address, cmd->data);
		return err;	/* Returns here on success */
	}

	/*if firmware loading, return immediately*/
	if (state->fw_status != FIRMWARE_LOADED_OK) 
		return err;
	
	if (retry < 10) {  /* abnormal case: retry 10 times */
		dev_err(&client->dev, "%s: address: 0x%04x, err = %d\n", __func__, cmd->address, err);
		retry++;
		msleep(100);
		goto again;
	}
	return -1;

}
int m6mo_i2c_write_category_8bit(struct v4l2_subdev *sd, u16 addr, u32 data)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_WRITE_PARAMETER;
	cmd.content_size = 4+1;
	cmd.data_size = 1;
	cmd.data = data;
	if (m6mo_i2c_write_category(client, &cmd)<0)
	{
		return -1;
	}
	return 0;
}
int m6mo_i2c_write_category_16bit(struct v4l2_subdev *sd, u16 addr, u32 data)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_WRITE_PARAMETER;
	cmd.content_size = 4+2;
	cmd.data_size = 2;
	cmd.data = data;
	if (m6mo_i2c_write_category(client, &cmd)<0)
	{
		return -1;
	}
	return 0;
}
int m6mo_i2c_write_category_32bit(struct v4l2_subdev *sd, u16 addr, u32 data)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_WRITE_PARAMETER;
	cmd.content_size = 4+4;
	cmd.data_size = 4;
	cmd.data = data;
	if (m6mo_i2c_write_category(client, &cmd)<0)
	{
		return -1;
	}
	return 0;
}
int m6mo_i2c_read_memory(struct i2c_client *client, struct m6mo_command *cmd)
{
	struct i2c_msg msg[2];
	int i,offset;
	int err = 0;
	int retry = 0;
	int count=0;
	
	if (!client->adapter)
		return -ENODEV;
	if(cmd->data_size>4)
		return -EINVAL;
again:
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = cmd->content_size;
	msg[0].buf = cmd->outbuf;

	cmd->outbuf[0] = 0;/*content size*/
	cmd->outbuf[1] = cmd->cmd_type;/*command type*/
	cmd->outbuf[2] = (cmd->address >> 24) & 0xff;/* address: 31-28 */
	cmd->outbuf[3] = (cmd->address >> 16) & 0xff;
	cmd->outbuf[4] = (cmd->address >> 8) & 0xff;
	cmd->outbuf[5] = (cmd->address >> 0) & 0xff;
	cmd->outbuf[6] = (cmd->data_size >> 8) & 0xff;/*N : 15-8bit, read size*/
	cmd->outbuf[7] = (cmd->data_size >> 0) & 0xff;;/*N : 7-0bit, read size*/

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = cmd->data_size + 3;
	msg[1].buf = cmd->inbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err >= 0)
	{
		count = (cmd->inbuf[1] << 8) + cmd->inbuf[2];
		if(count == cmd->data_size)
		{
			cmd->data = 0;
			for(i=3; i<cmd->data_size; i++)
			{	
				offset = (cmd->data_size-i-1)*8;
				cmd->data += (cmd->inbuf[i+3]<<offset);
			}
			return err;	/* Returns here on success */
		}
	}
	/* abnormal case: retry 5 times */
	if (retry < 10) {
		dev_err(&client->dev, "%s: address: 0x%04x\n", __func__, cmd->address);
		retry++;
		msleep(100);
		goto again;
	}
	return -1;


}
int m6mo_i2c_read_memory_8bit(struct v4l2_subdev *sd, u32 addr)
{

	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_READ_8BIT_MEMORY;
	cmd.content_size = 8;
	cmd.data_size = 1;

	if (m6mo_i2c_read_memory(client, &cmd)<0)
	{
		return -1;
	}
	return cmd.data;
}

int m6mo_i2c_read_memory_16bit(struct v4l2_subdev *sd, u32 addr)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_READ_16BIT_MEMORY;
	cmd.content_size = 8;
	cmd.data_size = 2;
	
	if(addr % 2)
		return -EINVAL;
	
	if (m6mo_i2c_read_memory(client, &cmd)<0)
	{
		return -1;
	}
	return cmd.data;
}
int m6mo_i2c_read_memory_32bit(struct v4l2_subdev *sd, u32 addr)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_READ_32BIT_MEMORY;
	cmd.content_size = 8;
	cmd.data_size = 4;
	
	if(addr % 2)
		return -EINVAL;
	
	if (m6mo_i2c_read_memory(client, &cmd)<0)
	{
		return -1;
	}
	return cmd.data;
}

int m6mo_i2c_write_memory(struct i2c_client *client, struct m6mo_command *cmd)
{
	struct i2c_msg msg[2];
	int i,offset;
	int err = 0;
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;
	if(cmd->data_size>4)
		return -EINVAL;	
again:

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = cmd->content_size+cmd->data_size;
	msg[0].buf = cmd->outbuf;

	msg[0].buf[0] = 0;/*content size*/
	msg[0].buf[1] = cmd->cmd_type;/*command type*/
	msg[0].buf[2] = (cmd->address >> 24) & 0xff;/* address: 31-28 */
	msg[0].buf[3] = (cmd->address >> 16) & 0xff;
	msg[0].buf[4] = (cmd->address >> 8) & 0xff;
	msg[0].buf[5] = (cmd->address >> 0) & 0xff;
	msg[0].buf[6] = (cmd->data_size >> 8) & 0xff;/*N : 15-8bit, read size*/
	msg[0].buf[7] = (cmd->data_size >> 0) & 0xff;;/*N : 7-0bit, read size*/
	for(i=0; i<cmd->data_size; i++)
	{
		offset = (cmd->data_size-i-1)*8;
		msg[0].buf[i+8] = (cmd->data >> offset) & 0xff;/*data*/
	}

	err = i2c_transfer(client->adapter, msg, 1);
	if (err > 0)
	{
		return 0;	/* Returns here on success */
	}
	/* abnormal case: retry 5 times */
	if (retry < 10) {
		dev_err(&client->dev, "%s: address: 0x%04x\n", __func__, cmd->address);
		retry++;
		msleep(100);
		goto again;
	}
	return -1;

}

int m6mo_i2c_batch_write_memory(struct v4l2_subdev *sd, u32 addr, const char *data, int len)
{
	struct i2c_msg msg[2];
	int err = 0;
	int retry = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	struct m6mo_state *state = to_state(sd);
	
	if (!client->adapter)
		return -ENODEV;
again:

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 8+len;
	msg[0].buf = state->buffer;

	msg[0].buf[0] = 0;/*content size*/
	msg[0].buf[1] = CMD_WRITE_8BIT_MEMORY;/*command type*/
	msg[0].buf[2] = (addr >> 24) & 0xff;/* address: 31-28 */
	msg[0].buf[3] = (addr >> 16) & 0xff;
	msg[0].buf[4] = (addr>> 8) & 0xff;
	msg[0].buf[5] = (addr >> 0) & 0xff;
	msg[0].buf[6] = (len >> 8) & 0xff;/*N : 15-8bit, read size*/
	msg[0].buf[7] = (len >> 0) & 0xff;;/*N : 7-0bit, read size*/

	memcpy(&msg[0].buf[8], data, len);/*data*/
	//printk("%s: write size=%d, data size=%d\n", __func__, msg[0].len, len);
	
	err = i2c_transfer(client->adapter, msg, 1);
	if (err > 0)
	{
		return 0;	/* Returns here on success */
	}
	/* abnormal case: retry 2 times */
	if (retry < 10) {
		dev_err(&client->dev, "%s: address: 0x%04x\n", __func__, addr);
		retry++;
		msleep(100);
		goto again;
	}
	return -1;

}
int m6mo_i2c_write_memory_8bit(struct v4l2_subdev *sd, u32 addr, u32 data)
{

	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_WRITE_8BIT_MEMORY;
	cmd.content_size = 8;
	cmd.data_size = 1;
	cmd.data = data;
	if (m6mo_i2c_write_memory(client, &cmd)<0)
	{
		return -1;
	}
	return 0;
}

int m6mo_i2c_write_memory_16bit(struct v4l2_subdev *sd, u32 addr, u32 data)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_WRITE_16BIT_MEMORY;
	cmd.content_size = 8;
	cmd.data_size = 2;
	cmd.data = data;
	
	if(addr % 2)
		return -EINVAL;
	
	if (m6mo_i2c_write_memory(client, &cmd)<0)
	{
		return -1;
	}
	return 0;
}
int m6mo_i2c_write_memory_32bit(struct v4l2_subdev *sd, u32 addr, u32 data)
{
	struct m6mo_command cmd;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	cmd.address = addr;
	cmd.cmd_type = CMD_WRITE_32BIT_MEMORY;
	cmd.content_size = 8;
	cmd.data_size = 4;
	cmd.data = data;
	
	if(addr % 2)
		return -EINVAL;
	
	if (m6mo_i2c_write_memory(client, &cmd)<0)
	{
		return -1;
	}
	return 0;
}
static int m6mo_write_array_regs(struct v4l2_subdev *sd, const struct m6mo_reg regs[], int size)
{
	int err = -EINVAL, i;

	for (i = 0; i < size; i++) {
		switch(regs[i].type)
		{
			case REG_8BIT:
				err = m6mo_i2c_write_category_8bit(sd, regs[i].addr, regs[i].val);
				break;				
			case REG_16BIT:
				err = m6mo_i2c_write_category_16bit(sd, regs[i].addr, regs[i].val);
				break;
			case REG_32BIT:
				err = m6mo_i2c_write_category_32bit(sd, regs[i].addr, regs[i].val);
				break;
		}
	}
	return err;
}

int m6mo_get_af_touch_coordinate(int val, int *x, int *y)
{
	int row, col;

	row = val / AF_TOUCH_ROW;
	col = val - row * AF_TOUCH_ROW;
	
	if(row > AF_TOUCH_ROW-1)
	{	
		row = AF_TOUCH_ROW-1;
	}
	if(row < 0)
		row= 0;
	if(col > AF_TOUCH_COL-1)
	{
		col = AF_TOUCH_COL-1;
	}
	if(col < 0)
		col= 0;

	pr_info("%s(): row %d, col %d.\n", __func__, row, col);
	
	/*x stand for col, y stand for row*/
	*x = col * AF_TOUCH_WIDTH;
	*y = row * AF_TOUCH_HEIGHT;
	
	return 0;
}

static int m6mo_fps_to_step(struct v4l2_subdev *sd, int fps)
{
	int step;
	if(fps<=0)
		step = MON_FPS_AUTO;//auto
	if(fps > 15)
		step = MON_FPS_30;//30fps
	else if(fps>12)
		step = MON_FPS_15;//15fps
	else if(fps>10)
		step = MON_FPS_10;//10fps
	else
		step = MON_FPS_7P5;//7.5fps

	return step;
}

static int m6mo_fps_to_evp_step(struct v4l2_subdev *sd, int fps)
{
	if (fps > 15)
		return EVP_MODE_30FPS;
	else if (fps > 0)
		return EVP_MODE_15FPS;
	else 
		return EVP_MODE_AUTO;
}

static int m6mo_set_camera_mode(struct v4l2_subdev *sd, int mode)
{
	int err = 0;
	int status, count;
	struct m6mo_state *state = to_state(sd);
	int timeout_count = 10;

	if (state->camera_mode == mode) return 0;

	init_completion(&state->completion);
	switch(mode)
	{	
	case SYS_PARAMETER_MODE:
		m6mo_enable_irq(sd);
		err = m6mo_i2c_write_category_8bit(sd,  SYS_MODE, mode);	
		for (count = 0; count < timeout_count; count++) {
			status = wait_for_completion_interruptible_timeout(&state->completion, 
				WAIT_TIMEOUT * HZ);
			if (status <= 0) {
				printk( "%s while waiting for interrupt timeout\n",__func__);
				return -ETIME;
			}	
			if (state->irq_state & 0x01) break;
		}
		break;
	case SYS_MONITOR_MODE:
		m6mo_enable_irq(sd);
		err = m6mo_i2c_write_category_8bit(sd,  SYS_MODE, mode);	
		for (count = 0; count < timeout_count; count++) {
			status = wait_for_completion_interruptible_timeout(&state->completion, 
				WAIT_TIMEOUT * HZ);
			if (status <= 0) {
				printk( "%s while waiting for interrupt timeout\n",__func__);
				return -ETIME;
			}	
			if (state->irq_state & 0x01) break;
		}			
		break;
	case SYS_CAPTURE_MODE:
		if (state->userset.af_scan_mode == AF_CONTINUOUS_FOCUS) {
			err = m6mo_cancel_caf(sd);
			if (err < 0) {
				pr_err("%s():cancel continuous focus mode fail\n", __func__);
				return err;
			}
		}
			
		m6mo_enable_irq(sd);
		err = m6mo_i2c_write_category_8bit(sd,  SYS_MODE, mode);
		for (count = 0; count < timeout_count; count++) {
			status = wait_for_completion_interruptible_timeout(&state->completion, 
				WAIT_TIMEOUT * HZ);
			if (status <= 0) {
				printk( "%s while waiting for interrupt timeout\n",__func__);
				return -ETIME;
			}	
			if (state->irq_state & 0x01) {
				if (state->userset.flash_mode != M6MO_FLASH_OFF)  /*set full flash current*/
					regulator_set_current_limit(state->fled_regulator, FLASH_FULL_CURRENT, FLASH_MAX_CURRENT);
			}

			if (state->irq_state & 0x08) {
				if (state->userset.flash_mode != M6MO_FLASH_OFF)   /*recovery pre flash current*/
					regulator_set_current_limit(state->fled_regulator, FLASH_PRE_CURRENT, FLASH_MAX_CURRENT);
				break;
			}
		}
		break;
	default:
		err = -EINVAL;
		break;
	}

	if (err >= 0) state->camera_mode = mode;

	return err;
}

static int m6mo_stream_on(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m6mo_state *state = to_state(sd);	
	int err = 0;

	v4l_info(client, "%s: stream on\n", __func__);
	switch(state->mode)
	{
	case  V4L2_CAMERA_PREVIEW:
	case  V4L2_CAMERA_RECORD:
		/*change to monitor mode*/
		err = m6mo_i2c_write_category_8bit(sd, SPECIAL_MON, SPECIAL_OFF);
		err = m6mo_set_camera_mode(sd, SYS_MONITOR_MODE);
		err = m6mo_i2c_write_category_8bit(sd, CAP_MODE, CAP_MODE_NORMAL);
		break;
	case V4L2_CAMERA_PANORAMA:
		/*Enable Panorama monitor mode*/
		err = m6mo_i2c_write_category_8bit(sd, SPECIAL_MON, SPECIAL_PANORAMA);
		err = m6mo_set_camera_mode(sd, SYS_MONITOR_MODE);
		/*recovery AE and AWB Lock*/
		err = m6mo_i2c_write_category_8bit(sd, AE_LOCK, 0x00);		
		err = m6mo_i2c_write_category_8bit(sd, AWB_LOCK, 0x00);
		pr_info("%s():change camera to panorama monitor mode.\n", __func__);
		break;
	case  V4L2_CAMERA_SINGLE_CAPTURE:
	case  V4L2_CAMERA_MULTI_CAPTURE:
	case V4L2_CAMERA_PANORAMA_CAPTURE:		
		break;
	}	

	if (state->camera_mode == SYS_MONITOR_MODE &&
		state->userset.scene == SCENE_MODE_PORTRAIT)
		err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_CTL, SMILE_FACE_DETECT_OFF);

	state->stream_on = 1;
	return err;
}

static int m6mo_stream_off(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m6mo_state *state = to_state(sd);	
	int err = 0;//-EINVAL;

	v4l_info(client, "%s:  stream off\n", __func__);
	
	state->stream_on = 0;
	return err;
}

int m6mo_set_focus_position(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	struct m6mo_state *state = to_state(sd);	
	int x, y;

	m6mo_get_af_touch_coordinate(ctrl->value, &x, &y);
	
	err = m6mo_i2c_write_category_16bit(sd, AF_TOUCH_WIN_W, AF_TOUCH_WIDTH);
	err = m6mo_i2c_write_category_16bit(sd, AF_TOUCH_WIN_H, AF_TOUCH_HEIGHT);	
	
	err = m6mo_i2c_write_category_16bit(sd, AF_TOUCH_WIN_X, x);
	err = m6mo_i2c_write_category_16bit(sd, AF_TOUCH_WIN_Y, y);

	if(err<0)
		return -EINVAL;
	state->userset.focus_position = ctrl->value;
	return 0;
}

int m6mo_set_wb_preset(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int err = -EINVAL;

	if(ctrl->value != M6MO_WB_AUTO)
		err = m6mo_i2c_write_category_8bit(sd, AWB_MODE, AWB_MANUAL);	
	switch(ctrl->value)
	{
	case M6MO_WB_AUTO:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MODE, AWB_AUTO);
		break;
	case M6MO_WB_INCANDESCENT:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_INCANDESCENT);
		break;		
	case M6MO_WB_FLUORESCENT_HIGH:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_FLUORESCENT_HIGH);
		break;		
	case M6MO_WB_FLUORESCENT_LOW: 
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_FLUORESCENT_LOW);
		break;
	case M6MO_WB_SUNNY:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_DAYLIGHT);
		break;		
	case M6MO_WB_CLOUDY:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_CLOUDY);
		break;
	case M6MO_WB_SHADE:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_SHADE);
		break;
	case M6MO_WB_HORIZON:
		err = m6mo_i2c_write_category_8bit(sd, AWB_MANUAL_REG, AWB_HORIZON);
		break;
	}

	if(err<0)
		return -EINVAL;
	state->userset.manual_wb = ctrl->value;
	return 0;
}
int m6mo_set_image_brightness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;	

	switch(ctrl->value)
	{
		case M6MO_EV_MINUS_2:
			err = m6mo_i2c_write_category_8bit(sd, EV_BIAS, EV_M2);
			break;
		case M6MO_EV_MINUS_1:
			err = m6mo_i2c_write_category_8bit(sd, EV_BIAS, EV_M1);
			break;			
		case M6MO_EV_DEFAULT:
			err = m6mo_i2c_write_category_8bit(sd, EV_BIAS, EV_00);
			break;
		case M6MO_EV_PLUS_1:
			err = m6mo_i2c_write_category_8bit(sd, EV_BIAS, EV_P1);
			break;			
		case M6MO_EV_PLUS_2:
			err = m6mo_i2c_write_category_8bit(sd, EV_BIAS, EV_P2);
			break;			
	}
	return err;
}
int m6mo_set_color_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int err = -EINVAL;	

	if(ctrl->value != M6MO_IE_NONE)
		err = m6mo_i2c_write_category_8bit(sd, COLOR_EFFECT, COLOR_EFFECT_ON);
	switch(ctrl->value)
	{
	case M6MO_IE_NONE:
		err = m6mo_i2c_write_category_8bit(sd, COLOR_EFFECT, COLOR_EFFECT_OFF);
		break;		
	case M6MO_IE_BNW:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x00);
		break;
	case M6MO_IE_SEPIA:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0xd8);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x18);
		break;		
	case M6MO_IE_RED:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x6b);
		break;			
	case M6MO_IE_GREEN:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0xe0);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0xe0);
		break;				
	case M6MO_IE_BLUE:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0x40);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x00);
		break;			
	case M6MO_IE_PINK:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0x20);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x40);
		break;				
	case M6MO_IE_YELLOW:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0x80);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x00);
		break;				
	case M6MO_IE_PURPLE:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0x50);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x20);
		break;			
	case M6MO_IE_ANTIQUE:
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXB, 0xd0);
		err = m6mo_i2c_write_category_8bit(sd, ZOOM_CFIXR, 0x30);
		break;			
	}

	state->userset.effect= ctrl->value;
	return err;
}
int m6mo_set_scenemode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	struct m6mo_state *state = to_state(sd);
	
	switch(ctrl->value)
	{
	case M6MO_SCENE_NONE:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_OFF);
		break;
	case M6MO_SCENE_AUTO:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_AUTO);
		break;		
	case M6MO_SCENE_PORTRAIT:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_PORTRAIT);
		if (state->camera_mode == SYS_MONITOR_MODE)
			err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_CTL, SMILE_FACE_DETECT_OFF);
		break;		
	case M6MO_SCENE_LANDSCAPE:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_LANDSCAPE);
		break;	
	case M6MO_SCENE_SPORTS:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_SPORT);
		break;
	case M6MO_SCENE_NIGHTSHOT:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_NIGHT);
		break;		
	case SCENE_MODE_SUNSET:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_SUNSET);
		break;		
	case M6MO_SCENE_MICRO:
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_MARCO);
		break;		
	case M6MO_SCENE_CHARACTER:
		pr_info("%s():set character scene mode %d.\n", __func__, SCENE_CHARACTER);
		err = m6mo_i2c_write_category_8bit(sd, SCENE_MODE, SCENE_CHARACTER);
		break;
	}

	state->userset.scene= ctrl->value;
 
	return err;
}


int m6mo_set_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;

	switch(ctrl->value)
	{
	case AUTO_FOCUS_OFF:
		err = m6mo_i2c_write_category_8bit(sd, AF_START_REG, AF_STOP);
		break;
	case AUTO_FOCUS_ON:
		err = m6mo_i2c_write_category_8bit(sd, AF_START_REG, AF_START);
		break;
	}

	return err;
}
int m6mo_set_focus_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	struct m6mo_state *state = to_state(sd);

	switch(ctrl->value)
	{
	case M6MO_FOCUS_AUTO:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, CENTRE_LARGE);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_FAST_SCAN);
		state->userset.af_scan_mode = AF_FAST_SCAN;
		break;
	case M6MO_FOCUS_MACRO:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, CENTRE_LARGE);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_FAST_SCAN);
		state->userset.af_scan_mode = AF_FAST_SCAN;
		break;
	case M6MO_FOCUS_MACRO_CAF:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, CENTRE_LARGE);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_CONTINUOUS_FOCUS);
		state->userset.af_scan_mode = AF_CONTINUOUS_FOCUS;
		break;
	case M6MO_FOCUS_FD:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, BY_FACE_DETECT);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_FAST_SCAN);
		state->userset.af_scan_mode = AF_FAST_SCAN;
		break;
	case M6MO_FOCUS_FD_CAF:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, BY_FACE_DETECT);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_CONTINUOUS_FOCUS);
		state->userset.af_scan_mode = AF_CONTINUOUS_FOCUS;
		break;
	case M6MO_FOCUS_TOUCH:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, BY_USER);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_FAST_SCAN);
		state->userset.af_scan_mode = AF_FAST_SCAN;
		break;
	case M6MO_FOCUS_TOUCH_CAF:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, BY_USER);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_CONTINUOUS_FOCUS);
		state->userset.af_scan_mode = AF_CONTINUOUS_FOCUS;
		break;
	case M6MO_FOCUS_AUTO_CAF:
		err = m6mo_i2c_write_category_8bit(sd, AF_WINDOW, CENTRE_LARGE);
		err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_CONTINUOUS_FOCUS);
		state->userset.af_scan_mode = AF_CONTINUOUS_FOCUS;
		break;
	}

	return err;
}
int m6mo_set_zoom_level(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	if (ctrl->value < M6MO_ZL_1)
		ctrl->value = M6MO_ZL_1;
	if(ctrl->value >= M6MO_ZL_70)
		ctrl->value = M6MO_ZL_70;

	return m6mo_i2c_write_category_8bit(sd, ZOOM_POSITOIN, ctrl->value);
}
int m6mo_set_anti_banding(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	switch(ctrl->value)
	{
	case ANTI_BANDING_AUTO:
		err = m6mo_i2c_write_category_8bit(sd, FLICKER, FLICKER_AUTO);
		break;
	case ANTI_BANDING_50HZ:
		err = m6mo_i2c_write_category_8bit(sd, FLICKER, FLICKER_50HZ);
		break;		
	case ANTI_BANDING_60HZ:
		err = m6mo_i2c_write_category_8bit(sd, FLICKER, FLICKER_60HZ);
		break;
	case ANTI_BANDING_OFF:
		err = m6mo_i2c_write_category_8bit(sd, FLICKER, FLICKER_OFF);
	default:
		break;
	}
	return err;
}

int m6mo_set_face_detect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	switch(ctrl->value)
	{
	case FACE_LOCK_ON:
		err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_CTL, FACE_DETECT_ON);
		break;
	case FACE_LOCK_OFF:
		err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_CTL, FACE_DETECT_OFF);
	default:
		break;
	}
	return err;

}
int m6mo_set_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	switch(ctrl->value)
	{
	case M6MO_ISO_AUTO:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_AUTO);
		break;
	case M6MO_ISO_50:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_50);
		break;		
	case M6MO_ISO_100:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_100);
		break;		
	case M6MO_ISO_200:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_200);
		break;		
	case M6MO_ISO_400:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_400);
		break;		
	case M6MO_ISO_800:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_800);
		break;		
	case M6MO_ISO_1600:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_1600);
		break;		
	case M6MO_ISO_3200:
		err = m6mo_i2c_write_category_8bit(sd, ISO_SEL, ISO_SEL_3200);
		break;
	default:
		break;
	}
	return err;

}
int m6mo_set_wdr(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	switch(ctrl->value)
	{
	case M6MO_WDR_OFF:
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_EN, PART_WDR_OFF);
		break;
	case M6MO_WDR_LOW:
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_EN, PART_WDR_ON);
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_LVL, PART_WDR_LOW);
		break;		
		break;
	case M6MO_WDR_MIDDLE:
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_EN, PART_WDR_ON);
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_LVL, PART_WDR_MIDDLE);
		break;		
	case M6MO_WDR_HIGH:
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_EN, PART_WDR_ON);
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_LVL, PART_WDR_HIGH);
		break;
	case M6MO_WDR_AUTO:
		pr_info("%s():set wdr mode %d.\n", __func__, PART_WDR_AUTO);
		err = m6mo_i2c_write_category_8bit(sd, PART_WDR_EN, PART_WDR_AUTO);
		break;
	default:
		break;
	}
	return err;

}

int m6mo_enable_flash_led(struct m6mo_state *state, bool enable) 
{
	if (enable) {
		struct regulator *regulator;

		/*if flash mode is not M6MO_FLASH_OFF, it means flash led has been open*/
		if (state->userset.flash_mode != M6MO_FLASH_OFF) return 0;

		pr_info("turn on flash led!\n");
			
		regulator = regulator_get(NULL, "flash_led");//gps 1.8V
		if (IS_ERR(regulator)) {
			pr_err("%s()->%d:regulator get fail !!\n", __FUNCTION__, __LINE__);
			return -1;
		}

		regulator_set_current_limit(regulator, FLASH_PRE_CURRENT, FLASH_MAX_CURRENT);
		regulator_enable(regulator);		
		state->fled_regulator = regulator;
	} else {
		pr_info("turn off flash led!\n");
		if (state->fled_regulator) {
			regulator_disable(state->fled_regulator);	
			regulator_put(state->fled_regulator);
			state->fled_regulator = NULL;
		}
	}
	return 0;
}

int m6mo_set_flash_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;
	struct m6mo_state *state = to_state(sd);
		
	if (state->userset.flash_mode == ctrl->value)	
		return 0;

	switch(ctrl->value)
	{
	case M6MO_FLASH_OFF:
		err = m6mo_i2c_write_category_8bit(sd, LED_FLASH_CONTROL, LED_FLASH_OFF);
		break;
	case M6MO_FLASH_AUTO:
		err = m6mo_i2c_write_category_8bit(sd, LED_FLASH_CONTROL, LED_FLASH_AUTO);
		break;
	case M6MO_FLASH_ON:
		err = m6mo_i2c_write_category_8bit(sd, LED_FLASH_CONTROL, LED_FLASH_ON);
		break;
	default:
		err = -EINVAL;
		return err;
	}

	if (err) return err;
	
	if (ctrl->value != M6MO_FLASH_OFF) {
		err = m6mo_enable_flash_led(state, true);
		if (err) return err;
		
		if (machine_is_m030())
			err = m6mo_i2c_write_category_8bit(sd, FLASHLED_SELECT_REG, FLASHLED_M030);
		else
			err = m6mo_i2c_write_category_8bit(sd, FLASHLED_SELECT_REG, FLASHLED_M032);
	} else {
		err = m6mo_enable_flash_led(state, false);
	}

	if (!err) state->userset.flash_mode = ctrl->value;
	
	return err;
}

int m6mo_set_contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	switch(ctrl->value)
	{
	case M6MO_CONTRAST_MINUS_4:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_M4);
		break;
	case M6MO_CONTRAST_MINUS_3:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_M3);
		break;
	case M6MO_CONTRAST_MINUS_2:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_M2);
		break;
	case M6MO_CONTRAST_MINUS_1:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_M1);
		break;
	case M6MO_CONTRAST_DEFAULT:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_OFF);
		break;
	case M6MO_CONTRAST_PLUS_1:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_P1);
		break;
	case M6MO_CONTRAST_PLUS_2:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_P2);
		break;
	case M6MO_CONTRAST_PLUS_3:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_P3);
		break;
	case M6MO_CONTRAST_PLUS_4:
		err = m6mo_i2c_write_category_8bit(sd, TONE_CTRL, TONE_P4);
		break;
	default:
		break;
	}
	
	return err;
}


int m6mo_set_rotation(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int err = -EINVAL;

	if (state->userset.rotation == ctrl->value)
		return 0;
	
	switch(ctrl->value)
	{
	case M6MO_ROTATE_0:
		err = m6mo_i2c_write_category_8bit(sd, MAIN_MIRROR, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_REVERSE, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_ROTATION, MAIN_ROTATE_OFF);
		err = m6mo_i2c_write_category_8bit(sd, PREVIEW_ROTATION, PREVIEW_ROTATE_OFF);
		err = m6mo_i2c_write_category_8bit(sd, THUMB_ROTATION, THUMB_ROTATE_OFF);
		break;
	case M6MO_ROTATE_90:
		err = m6mo_i2c_write_category_8bit(sd, MAIN_MIRROR, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_REVERSE, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_ROTATION, MAIN_ROTATE_CW90);
		err = m6mo_i2c_write_category_8bit(sd, PREVIEW_ROTATION, PREVIEW_ROTATE_CW90);
		err = m6mo_i2c_write_category_8bit(sd, THUMB_ROTATION, THUMB_ROTATE_CW90);
		break;
	case M6MO_ROTATE_180:
		err = m6mo_i2c_write_category_8bit(sd, MAIN_MIRROR, 0x01);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_REVERSE, 0x01);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_ROTATION, MAIN_ROTATE_OFF);
		err = m6mo_i2c_write_category_8bit(sd, PREVIEW_ROTATION, PREVIEW_ROTATE_OFF);
		err = m6mo_i2c_write_category_8bit(sd, THUMB_ROTATION, THUMB_ROTATE_OFF);
		break;
	case M6MO_ROTATE_270:
		err = m6mo_i2c_write_category_8bit(sd, MAIN_MIRROR, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_REVERSE, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, MAIN_ROTATION, MAIN_ROTATE_ACW90);
		err = m6mo_i2c_write_category_8bit(sd, PREVIEW_ROTATION, PREVIEW_ROTATE_ACW90);
		err = m6mo_i2c_write_category_8bit(sd, THUMB_ROTATION, THUMB_ROTATE_ACW90);
		break;
	default:
		break;
	}

	if (!err) state->userset.rotation = ctrl->value;
	
	return err;
}


static int m6mo_get_smile_face_detection(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	
	ctrl->value = state->smile_detection_result;
	
	return 0;
}


static int m6mo_wait_multi_capture(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int status;
	int index;
	struct m6mo_state *state = to_state(sd);
	
	if (ctrl->value  < 1 || ctrl->value > state->mcap_numbers) {
		pr_err("%s():ctrl->value %d is out of range[1-%d].", __func__, ctrl->value, state->mcap_numbers);
		return -EINVAL;
	}
	
	index = ctrl->value - 1;

	mutex_lock(&state->m6mo_mutex);
	if (state->mcap_status[index] != MCAP_NOT_READY) {
		ctrl->value = state->mcap_status[index];
		mutex_unlock(&state->m6mo_mutex);
		return 0;
	}
	mutex_unlock(&state->m6mo_mutex);

	/*wait the irq return*/
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s while waiting for capture command time out\n",__func__);
		return -ETIME;
	}	

	mutex_lock(&state->m6mo_mutex);
	ctrl->value = state->mcap_status[index];
	mutex_unlock(&state->m6mo_mutex);

	return 0;
}


static int m6mo_wait_multi_capture_ready(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int status;
	struct m6mo_state *state = to_state(sd);

	if (state->mcap_ready != MCAP_NOT_READY) {
		ctrl->value = state->mcap_ready;
		return 0;
	}

	pr_info("%s:%d\n", __func__, state->mcap_ready);
	
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s while waiting for capture command time out\n",__func__);
		return -ETIME;
	}	

	ctrl->value = state->mcap_ready;
	
	return 0;
}


static int m6mo_set_smile_face_detection(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	struct m6mo_state *state = to_state(sd);

	if (ctrl->value < 0 || ctrl->value > 100) {
		pr_err("wrong smile level value!please set[0-100]\n");
		return -EINVAL;
	}

	if (ctrl->value) {
		pr_info("%s():smile face detection enable.\n", __func__);
		state->smile_detection_flag = 1;
		state->smile_detection_result = 0;
		state->irq_mode = M6MO_IRQ_SMILE_DETECT;
		err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_MAX, 0x0b);
		err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_CTL, SMILE_FACE_DETECT_ON);
		err = m6mo_i2c_write_category_8bit(sd, FD_SMILE_LEVEL_THRS, ctrl->value);
	}
	else {
		pr_info("%s():smile face detection disable.\n", __func__);
		err = m6mo_i2c_write_category_8bit(sd, FD_SMILE_LEVEL_THRS, 0x00);
		err = m6mo_i2c_write_category_8bit(sd, FACE_DETECT_CTL, SMILE_FACE_DETECT_OFF);
		state->smile_detection_flag = 0;
		state->smile_detection_result = 0;
		state->irq_mode = M6MO_IRQ_NORMAL;
	}
	
	return err;
}


static int m6mo_get_face_detected_num(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int value, err;

	err = m6mo_i2c_read_category_8bit(sd, FACE_DETECT_NUM, &value);
	if (!err) ctrl->value = value;
	
	return err;
}


static int m6mo_get_selected_face_location(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err, x, y;

	err = m6mo_i2c_read_category_16bit(sd, FACE_DETECT_X_LOCATION, &x);
	err = m6mo_i2c_read_category_16bit(sd, FACE_DETECT_Y_LOCATION, &y);
	if (!err) ctrl->value = ((x & 0xffff) << 16) | (y & 0xffff);
	
	return err;
}


static int m6mo_get_selected_face_size(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err, w, h;

	err = m6mo_i2c_read_category_16bit(sd, FACE_DETECT_FRAME_WIDTH, &w);
	err = m6mo_i2c_read_category_16bit(sd, FACE_DETECT_FRAME_HEIGH, &h);
	if (!err) ctrl->value = ((w & 0xffff) << 16) | (h & 0xffff);
	
	return err;
}


static int m6mo_set_face_selection(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int value, err;
	int retry_count = 100;
	
	err =  m6mo_i2c_write_category_8bit(sd, FACE_DETECT_READ_SEL, ctrl->value);
	if (err) return err;

	while (retry_count--) {
		err = m6mo_i2c_read_category_8bit(sd, FACE_DETECT_READ_SEL, &value);
		if (!err && value == 0xff) 
			return 0;
		msleep(5);
	}

	return -1;
}



static int m6mo_set_pan_cur_info(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int index = ctrl->value;
	int err;

	if (index < 1 || index > PAN_MAX_PICTURE) {
		pr_err("wrong pan num!\n");
		return -EINVAL;
	}

	index--;
	mutex_lock(&state->m6mo_mutex);
	state->pan_cur_index = index;
	err = state->pan_info[index].status;
	ctrl->value = state->pan_info[index].extra;
	mutex_unlock(&state->m6mo_mutex);
	
	return err;
}


static int m6mo_wait_pan_ready(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);

	if (state->pan_ready != PAN_NOT_READY) {
		ctrl->value = state->pan_ready;
	}
	else {
		int status;
		init_completion(&state->completion);
		status = wait_for_completion_interruptible_timeout(&state->completion, 
			2 * WAIT_TIMEOUT * HZ);
		if (status <= 0) {
			printk( "%s while waiting for capture command time out\n",__func__);
			return -ETIME;
		}	
		ctrl->value = state->pan_ready;
	}
	
	return 0;
}


static int m6mo_get_pan_direction(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err, value;

	err = m6mo_i2c_read_category_8bit(sd, PAN_CAP_DIRECTION, &value);
	if (!err) ctrl->value = value;
	
	return err;
}

static int m6mo_set_multi_capture(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int err = 0;

	/*the value is the frame count what we want to capture*/
	if (ctrl->value) {
		int i;
		
		pr_info("%s():multi capture start, multi capture number is %d*************\n", __func__, ctrl->value);
		state->mcap_numbers = ctrl->value;
		state->irq_mode = M6MO_IRQ_MULTI_CAP;
		state->mcap_ready = MCAP_NOT_READY;
		err = m6mo_i2c_write_category_8bit(sd, CAP_MODE, CAP_MODE_AUTO_MULTICAP);
		err = m6mo_i2c_write_category_8bit(sd, CAP_FRM_COUNT, state->mcap_numbers);
		m6mo_enable_irq(sd);
		init_completion(&state->completion);
		for (i = 0; i < state->mcap_numbers; i++) 
			state->mcap_status[i] = MCAP_NOT_READY;
		state->mcap_counter = 0;
		err = m6mo_set_camera_mode(sd, SYS_CAPTURE_MODE);
	}
	else {
		state->irq_mode = M6MO_IRQ_NORMAL;
	}

	return 0;
}


static int m6mo_get_multi_capture(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	int status;
	struct m6mo_state *state = to_state(sd);

	if (ctrl->value < 1 || ctrl->value > state->mcap_numbers) {
		pr_err("%s():ctrl->value %d is out of range[1-%d]", 
			__func__, ctrl->value, state->mcap_numbers);
		return -EINVAL;
	}

	init_completion(&state->completion);
	err = m6mo_i2c_write_category_8bit(sd, JPEG_SIZE_MAX, 0x400000);
	m6mo_enable_root_irq(sd);
	err = m6mo_i2c_write_category_8bit(sd, CAP_SEL_FRAME_MAIN, ctrl->value);
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s while waiting for capture command time out\n",__func__);
		return -ETIME;
	}	
	m6mo_enable_root_irq(sd);
	err = m6mo_i2c_write_category_8bit(sd, CAP_TRANSFER_START, CAP_TRANSFER_MAIN);
	/*wait for initial complete*/
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s while waiting for capture command time out\n",__func__);
		return -ETIME;
	}	

	/*set ctrl value to 0 means get picture success*/
	ctrl->value = 0;
	
	return 0;
}


/*exif informaion*/
static int m6mo_get_exif_exptime(struct v4l2_subdev *sd, struct v4l2_control *ctrl, int flag)
{
	if (flag) 
		return m6mo_i2c_read_category_32bit(sd, INFO_EXPTIME_DENUMINATOR, &ctrl->value);
	else 
		return m6mo_i2c_read_category_32bit(sd, INFO_EXPTIME_NUMERATOR, &ctrl->value);
}

static int m6mo_get_exif_tv(struct v4l2_subdev *sd, struct v4l2_control *ctrl, int flag)
{
	if (flag) 
		return m6mo_i2c_read_category_32bit(sd, INFO_TV_DENUMINATOR, &ctrl->value);
	else 
		return m6mo_i2c_read_category_32bit(sd, INFO_TV_NUMERATOR, &ctrl->value);
}


static int m6mo_get_exif_av(struct v4l2_subdev *sd, struct v4l2_control *ctrl, int flag)
{
	if (flag) 
		return m6mo_i2c_read_category_32bit(sd, INFO_AV_DENUMINATOR, &ctrl->value);
	else 
		return m6mo_i2c_read_category_32bit(sd, INFO_AV_NUMERATOR, &ctrl->value);
}


static int m6mo_get_exif_bv(struct v4l2_subdev *sd, struct v4l2_control *ctrl, int flag)
{
	if (flag) 
		return m6mo_i2c_read_category_32bit(sd, INFO_BV_DENUMINATOR, &ctrl->value);
	else 
		return m6mo_i2c_read_category_32bit(sd, INFO_BV_NUMERATOR, &ctrl->value);
}


static int m6mo_get_exif_ebv(struct v4l2_subdev *sd, struct v4l2_control *ctrl, int flag)
{
	if (flag) 
		return m6mo_i2c_read_category_32bit(sd, INFO_EBV_DENUMINATOR, &ctrl->value);
	else 
		return m6mo_i2c_read_category_32bit(sd, INFO_EBV_NUMERATOR, &ctrl->value);
}


static int m6mo_get_exif_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_read_category_16bit(sd, INFO_ISO, &ctrl->value);
}


static int m6mo_get_exif_flash(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_read_category_16bit(sd, INFO_FLASH, &ctrl->value);
}


static int m6mo_get_exif_sdr(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_read_category_16bit(sd, INFO_SDR, &ctrl->value);
}


static int m6mo_get_exif_qval(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_read_category_16bit(sd, INFO_QVAL, &ctrl->value);
}

static int m6mo_get_scene_ev(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_read_category_16bit(sd, REG_SCENE_EV, &ctrl->value);
}

/*
  * get auto focus result (0-operating, 1-success, 2-fail, 3-stopped at edge)
*/
static int m6mo_get_auto_focus_result(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_read_category_8bit(sd, AF_RESULT, &ctrl->value);
}

int m6mo_set_panorama_capture(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL, i;
	struct m6mo_state *state = to_state(sd);
	
	if (ctrl->value) {
		if (state->irq_mode != M6MO_IRQ_PANORAMA) {
			state->irq_mode = M6MO_IRQ_PANORAMA;
			state->pan_counter = 0;
			state->pan_cur_index = 0;
			state->pan_ready = PAN_NOT_READY;
			for (i = 0; i < PAN_MAX_PICTURE; i++) {
				state->pan_info[i].status = PAN_UNKNOWN_ERR;
				state->pan_info[i].extra = 0;
			}

			err = m6mo_i2c_write_category_8bit(sd, AE_LOCK, 0x01);		
			err = m6mo_i2c_write_category_8bit(sd, AWB_LOCK, 0x01);
			err = m6mo_i2c_write_category_8bit(sd, CAP_MODE, CAP_MODE_PARORAMA);
			err = m6mo_i2c_write_category_8bit(sd, PANO_CTRL, 0x00);
			m6mo_enable_irq(sd);
			err = m6mo_i2c_write_category_8bit(sd, PAN_CAP_READY, PAN_CAP_READY_START);
			printk("%s():start panorama capture.\n", __func__);
		}
	}
	else {
		printk("%s():stop panorama capture.\n", __func__);
		if (state->irq_mode == M6MO_IRQ_PANORAMA) {
			state->irq_mode = M6MO_IRQ_NORMAL;
		}
	}
	
	return 0;
}


static int m6mo_terminate_panorama(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;

	if (ctrl->value) 
		err = m6mo_i2c_write_category_8bit(sd, PAN_CAP_READY, PAN_CAP_READY_STOP);

	return err;
}


int m6mo_start_capture(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;
	int status;
	struct m6mo_state *state = to_state(sd);

	switch (ctrl->value) {
	case 0:
		break;
	case 1:  /*get jpeg data*/
		err = m6mo_i2c_write_category_8bit(sd, CAP_SEL_FRAME_MAIN, 0x01);
		m6mo_enable_root_irq(sd);
		init_completion(&state->completion);
		err = m6mo_i2c_write_category_8bit(sd, CAP_TRANSFER_START, CAP_TRANSFER_MAIN);
		/*wait for initial complete*/
		status = wait_for_completion_interruptible_timeout(&state->completion, 
			WAIT_TIMEOUT * HZ);
		if (status <= 0) {
			printk( "%s while waiting for capture command time out\n",__func__);
			/* FIXME cancel the configure endpoint command */
			return -ETIME;
		}
		break;
	case 2:   /*get YUV data*/
		err = m6mo_i2c_write_category_8bit(sd, CAP_SEL_FRAME_PRV, 0x01);
		m6mo_enable_root_irq(sd);
		init_completion(&state->completion);
		err = m6mo_i2c_write_category_8bit(sd, CAP_TRANSFER_START, CAP_TRANSFER_PREVIEW);
		/*wait for initial complete*/
		status = wait_for_completion_interruptible_timeout(&state->completion, 
			WAIT_TIMEOUT * HZ);
		if (status <= 0) {
			printk( "%s while waiting for capture command time out\n",__func__);
			/* FIXME cancel the configure endpoint command */
			return -ETIME;
		}	
		break;
	default:
		return err = -EINVAL;
	}
	
	return err;
}
int m6mo_get_jpeg_main_size(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	err = m6mo_i2c_read_category_32bit(sd, JPEG_IMAGE_SIZE,  &ctrl->value);
	pr_info("%s():jpeg size is %d.\n", __func__, ctrl->value);
	return err;
}
int m6mo_get_jpeg_mem_size(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);

	if (state->raw_image_flag)
		ctrl->value = DEFAULT_RAW_MEM_SIZE;
	else 
		ctrl->value = DEFAULT_JPEG_MEM_SIZE;
	
	return 0;
}


static int m6mo_set_cap_width_height(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_win_size *wsize;
	struct m6mo_state *state = to_state(sd);
	int width = (ctrl->value >> 16) & 0xffff;
	int height = ctrl->value & 0xffff;

	pr_info("%s():width %d height %d\n", __func__, width, height);

	if(width == state->cap_wsize.width && height == state->cap_wsize.height)
		return 0;

	for (wsize = m6mo_cap_sizes; wsize < m6mo_cap_sizes + N_CAP_SIZES;
		wsize++)
		if (width >= wsize->width && height >= wsize->height)
				break;
		
	if (wsize >= m6mo_cap_sizes + N_CAP_SIZES)
		wsize--;   /* Take the smallest one */

	m6mo_write_array_regs(sd, wsize->regs, wsize->size);
	state->cap_wsize = *wsize;	
	
	return 0;
}

static int m6mo_set_vflip(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int err;

	if (state->userset.reverse == ctrl->value)
		return 0;
	
	if (ctrl->value)
		err = m6mo_i2c_write_category_8bit(sd, MON_REVERSE_ISP, 0x01);
	else
		err = m6mo_i2c_write_category_8bit(sd, MON_REVERSE_ISP, 0x00);

	if (!err) state->userset.reverse = ctrl->value;

	return err;
}

static int m6mo_set_hflip(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);
	int err;

	if (state->userset.mirror == ctrl->value)
		return 0;
	
	if (ctrl->value)
		err = m6mo_i2c_write_category_8bit(sd, MON_MIRROR_ISP, 0x01);
	else
		err = m6mo_i2c_write_category_8bit(sd, MON_MIRROR_ISP, 0x00); 
	if (!err) state->userset.mirror = ctrl->value;

	return err;
}

static int m6mo_set_raw_image_index(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	if (ctrl->value == 1)
		return m6mo_i2c_write_category_8bit(sd, 0x0013, 0x01);
	else if (ctrl->value == 0)
		return m6mo_i2c_write_category_8bit(sd, 0x0013, 0x00);
	else 
		return -1;
}


static int m6mo_set_colorbar(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = -EINVAL;
	
	if (ctrl->value)
		err = m6mo_i2c_write_category_8bit(sd, COLOR_BAR_REG, ENABLE_COLOR_BAR);

	return err;
}
/*
  * set face detection direction, 0, 90, 180 or 270 angle
*/
static int m6mo_set_face_detection_direction(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return m6mo_i2c_write_category_8bit(sd, FACE_DETECT_DIRECTION, ctrl->value);
}


static int m6mo_set_raw_image_flag(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m6mo_state *state = to_state(sd);

	if (state->raw_image_flag == ctrl->value)
		return 0;
	
	if (ctrl->value)
		state->raw_image_flag = 1;
	else 
		state->raw_image_flag = 0;

	return 0;
}


static int m6mo_start_camera(struct v4l2_subdev *sd)
{
	int status;
	struct m6mo_state *state = to_state(sd);

	init_completion(&state->completion);
	/*start camera program*/
	m6mo_i2c_write_category_8bit(sd, CAMERA_START_CMD, FLASH_CMD_START);
	/*wait for initial complete*/
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s while waiting for start command time out\n",__func__);
		return -ETIME;
	}
	return 0;
}

const char * const *m6mo_ctrl_get_menu(u32 id)
{
	return v4l2_ctrl_get_menu(id);
}

static inline struct v4l2_queryctrl const *m6mo_find_qctrl(int id)
{
	return NULL;
}

static int m6mo_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	return -EINVAL;
}

static int m6mo_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	m6mo_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, m6mo_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int m6mo_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int m6mo_init_fmt(struct v4l2_subdev *sd,  struct m6mo_format_struct *fmt, struct m6mo_win_size *ret_wsize, int mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m6mo_state *state = to_state(sd);	
	int err = 0;

	v4l_info(client, "%s: initial format for mode %d\n", __func__, mode);

	switch(mode) {
	case  V4L2_CAMERA_PREVIEW:
	case  V4L2_CAMERA_RECORD:
		if ((ret_wsize->height != state->wsize.height) || 
			(ret_wsize->width != state->wsize.width)) {
			err = m6mo_set_camera_mode(sd, SYS_PARAMETER_MODE);
			err = m6mo_write_array_regs(sd, ret_wsize->regs, ret_wsize->size);
			state->wsize = *ret_wsize;	
		}
		break;
	case V4L2_CAMERA_PANORAMA:
		/*before enter panorama mode, it must be set parameter mode*/
		err = m6mo_set_camera_mode(sd, SYS_PARAMETER_MODE);
		break;
	case  V4L2_CAMERA_SINGLE_CAPTURE:
		/*change to capture mode*/
		if (state->raw_image_flag) {
			err = m6mo_i2c_write_category_8bit(sd, YUVOUT_MAIN, MAIN_RAW10_UNPACK);
			fmt->pixelformat = MAIN_RAW10_UNPACK;
			state->cap_fmt = *fmt;
		}
		else {
			if (fmt->pixelformat != state->cap_fmt.pixelformat) {
				err = m6mo_write_array_regs(sd, fmt->regs, fmt->size);
				state->cap_fmt = *fmt;
			}
		}
		err = m6mo_set_camera_mode(sd, SYS_CAPTURE_MODE);
		break;
	case V4L2_CAMERA_MULTI_CAPTURE:
	case V4L2_CAMERA_PANORAMA_CAPTURE:
		if (fmt->pixelformat != state->cap_fmt.pixelformat) {
			err = m6mo_write_array_regs(sd, fmt->regs, fmt->size);
			state->cap_fmt = *fmt;
		}
		break;
	}

	state->mode = mode;	
	
	return err;
}

static int m6mo_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct m6mo_format_struct *ret_fmt,
		struct m6mo_win_size *ret_wsize, int mode)
{
	int index;
	struct m6mo_win_size *wsize;
//	struct v4l2_pix_format *pix = &fmt->fmt.pix;

	for (index = 0; index < N_M6MO_FMTS; index++)
		if (m6mo_formats[index].mbus_code == fmt->code)
			break;
	if (index >= N_M6MO_FMTS) {
		/* default to first format */
		index = 0;
		fmt->code = m6mo_formats[0].mbus_code;
	}

	if (ret_fmt != NULL)
		*ret_fmt =*( m6mo_formats + index);

	fmt->field = V4L2_FIELD_NONE;
	/*
	 * Round requested image size down to the nearest
	 * we support, but not below the smallest.
	 */
	if(mode < V4L2_CAMERA_SINGLE_CAPTURE){	//preview
		for (wsize = m6mo_win_sizes + N_WIN_SIZES - 1; wsize >= m6mo_win_sizes;
			wsize--)
			if (fmt->width <= wsize->width && fmt->height <= wsize->height)	
				break;

		if (wsize < m6mo_win_sizes) {
			pr_err("m6mo has no mached preview picture size, the max preview size: width %d, height %d.\n",
				m6mo_win_sizes[0].width, m6mo_win_sizes[0].height);
			return -EINVAL;
		}
		
		if (wsize >= m6mo_win_sizes + N_WIN_SIZES)
			wsize--;   /* Take the last one */
		if (ret_wsize != NULL)
			*ret_wsize = *wsize;
		/*
		 * Note the size we'll actually handle.
		 */
		fmt->width = wsize->width;
		fmt->height = wsize->height;
		//pix->bytesperline = pix->width*m6mo_formats[index].bpp;
		//pix->sizeimage = pix->height*pix->bytesperline;
	}else{
		for (wsize = m6mo_cap_sizes; wsize < m6mo_cap_sizes + N_CAP_SIZES;
		     wsize++)
			if (fmt->width >= wsize->width && fmt->height >= wsize->height)
				break;
		if (wsize >= m6mo_cap_sizes + N_CAP_SIZES)
			wsize--;   /* Take the smallest one */
		if (ret_wsize != NULL)
			*ret_wsize = *wsize;
		/*
		 * Note the size we'll actually handle.
		 */
		fmt->width = wsize->width;
		fmt->height = wsize->height;
		//pix->bytesperline = pix->width*m6mo_formats[index].bpp;
		//pix->sizeimage = pix->height*pix->bytesperline;
	}
	return 0;
}
static int m6mo_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int err = 0;
	int mode = fmt->reserved[0];
	struct m6mo_format_struct ret_fmt = {0,};
	struct m6mo_win_size ret_wsize = {0,};

	printk("%s: >> width=%d, heigh=%d for mode =%d\n", __func__, fmt->width, fmt->height, mode);	
	m6mo_try_fmt_internal(sd, fmt, &ret_fmt, &ret_wsize, mode);
	printk("%s: << width=%d, heigh=%d for mode =%d\n", __func__, fmt->width, fmt->height, mode);
	m6mo_init_fmt(sd, &ret_fmt, &ret_wsize, mode);

	return err;
}
static int m6mo_enum_framesizes(struct v4l2_subdev *sd, 
					struct v4l2_frmsizeenum *fsize)
{
	struct m6mo_state *state = to_state(sd);

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	/* this is camera sensor's width, height.
	 * originally this should be filled each file format
	 */

	if (state->mode < V4L2_CAMERA_SINGLE_CAPTURE) {
		fsize->discrete.width = state->wsize.width;
		fsize->discrete.height = state->wsize.height;	
	}
	else {
		fsize->discrete.width = state->cap_wsize.width;
		fsize->discrete.height = state->cap_wsize.height;	
	}

	return 0;
}

static int m6mo_enum_frameintervals(struct v4l2_subdev *sd, struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int m6mo_enum_fmt(struct v4l2_subdev *sd, unsigned int index, enum v4l2_mbus_pixelcode *code)
{
	int err = 0;

	return err;
}

static int m6mo_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int err = 0;

	return err;
}

static int m6mo_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct m6mo_state *state = to_state(sd);
	
	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;
	param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	param->parm.capture.capturemode = state->mode;
	param->parm.capture.timeperframe.numerator = 1;
	param->parm.capture.timeperframe.denominator = state->fps;

	return 0;
}

static int m6mo_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct m6mo_state *state = to_state(sd);
	int fps = param->parm.capture.timeperframe.denominator;
	int value;
	int err = -EINVAL;
	int evp_step;

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return err;	
	
	if(state->fps == fps)
		return 0;
	
	state->fps = fps;
	value = m6mo_fps_to_step(sd, fps);
	evp_step = m6mo_fps_to_evp_step(sd, fps);
	pr_info("set fps step %d, evp step %d.\n", value, evp_step);
	/*set parameter mode before set preview frame rate*/
	m6mo_set_camera_mode(sd, SYS_PARAMETER_MODE);

	m6mo_i2c_write_category_8bit(sd, MON_FPS, MON_FPS_AUTO);
	m6mo_i2c_write_category_8bit(sd, EVP_MODE_MON, evp_step);
	
	return 0;
}

static int m6mo_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m6mo_state *state = to_state(sd);
	struct m6mo_userset userset = state->userset;
	int err = 0;
	
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_WHITE_BALANCE:		
		ctrl->value =userset.manual_wb;
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = userset.effect;
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		ctrl->value =  userset.brightness;
		break;		
	case V4L2_CID_CONTRAST:
		ctrl->value = userset.contrast;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value =  userset.saturation;
		break;
	case V4L2_CID_SHARPNESS:
		ctrl->value =  userset.sharpness;
		break;
	case V4L2_CID_ZOOM_ABSOLUTE:
		ctrl->value =  userset.zoom;
		break;
	case V4L2_CID_CAMERA_FOCUS_WINDOW:
		ctrl->value =  userset.focus_position;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		err = m6mo_get_jpeg_main_size(sd, ctrl);
		break;
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		err = m6mo_get_jpeg_mem_size(sd, ctrl);
		break;	
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
	case V4L2_CID_CAMERA_VT_MODE:
	case V4L2_CID_CAMERA_VGA_BLUR:
	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAMERA_SENSOR_MODE:
	case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK:
	case V4L2_CID_CAMERA_SET_GAMMA:
	case V4L2_CID_CAMERA_BATCH_REFLECTION:

	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:		
		ctrl->value =  0;
		break;
	case V4L2_CID_CAMERA_PANORAMA_CAPTURE_PICTURE_NUM:
		ctrl->value = state->pan_counter + 1;
		break;
	case V4L2_CID_CAMERA_PANORAMA_CAPTURE_READY:
		err = m6mo_wait_pan_ready(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_PANORAMA_DIRECTION:
		err = m6mo_get_pan_direction(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SMILE_FACE_DETECTION:
		err = m6mo_get_smile_face_detection(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_FACE_DET_NUM:
		err = m6mo_get_face_detected_num(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SELECTED_FACE_LOCATION:
		err = m6mo_get_selected_face_location(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SELECTED_FACE_SIZE:
		err = m6mo_get_selected_face_size(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_MULTI_CAPTURE_READY:
		err = m6mo_wait_multi_capture_ready(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_EXIF_EXPTIME_NUMERATOR:
		err = m6mo_get_exif_exptime(sd, ctrl, 0);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_EXPTIME_NUMERATOR, ctrl->value==%u\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_EXPTIME_DENUMINATOR:
		err = m6mo_get_exif_exptime(sd, ctrl, 1);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_EXPTIME_DENUMINATOR, ctrl->value==%u\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_TV_NUMERATOR:
		err = m6mo_get_exif_tv(sd, ctrl, 0);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_TV_NUMERATOR, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_TV_DENUMINATOR:
		err = m6mo_get_exif_tv(sd, ctrl, 1);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_TV_DENUMINATOR, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_AV_NUMERATOR:
		err = m6mo_get_exif_av(sd, ctrl, 0);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_AV_NUMERATOR, ctrl->value==%u\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_AV_DENUMINATOR:
		err = m6mo_get_exif_av(sd, ctrl, 1);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_AV_DENUMINATOR, ctrl->value==%u\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_BV_NUMERATOR:
		err = m6mo_get_exif_bv(sd, ctrl, 0);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_BV_NUMERATOR, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_BV_DENUMINATOR:
		err = m6mo_get_exif_bv(sd, ctrl, 1);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_BV_DENUMINATOR, ctrl->value==%u\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_EBV_NUMERATOR:
		err = m6mo_get_exif_ebv(sd, ctrl, 0);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_EBV_NUMERATOR, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_EBV_DENUMINATOR:
		err = m6mo_get_exif_ebv(sd, ctrl, 1);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_EBV_DENUMINATOR, ctrl->value==%u\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_ISOV:
		err = m6mo_get_exif_iso(sd, ctrl);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_ISOV, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_FLASHV:
		err = m6mo_get_exif_flash(sd, ctrl);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_FLASHV, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_SDRV:
		err = m6mo_get_exif_sdr(sd, ctrl);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_SDRV, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_EXIF_QV:
		err = m6mo_get_exif_qval(sd, ctrl);
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EXIF_QV, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAM_FW_MINOR_VER:
		ctrl->value = state->firmware_version;
		dev_info(&client->dev, "%s: V4L2_CID_CAM_FW_MINOR_VER, ctrl->value==%d\n", __func__,ctrl->value);
		break;
	case V4L2_CID_CAMERA_SCENE_EV:
		err = m6mo_get_scene_ev(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		err = m6mo_get_auto_focus_result(sd, ctrl); 
		break;
	default:
		dev_err(&client->dev, "%s: no such control, ctrl->id=%x, ctrl->value==%d\n", __func__,ctrl->id,ctrl->value);
		err = -EINVAL;
		break;
	}
	
	return err;
}

static int m6mo_s_stream(struct v4l2_subdev *sd, int enable)
{
	if(enable)
	{
		m6mo_stream_on(sd);
	}else{
		m6mo_stream_off(sd);
	}
	return 0;
}

static int m6mo_s_gpio(struct v4l2_subdev *sd, u32 val)
{

	return 0;
}
static int m6mo_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}
static long m6mo_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	return 0;
}

static int m6mo_cancel_caf(struct v4l2_subdev *sd)
{
	struct m6mo_state *state = to_state(sd);
	int err, status;

	pr_info("%s()\n", __func__);
	
	init_completion(&state->completion);
	/*stop caf mode capture later*/
	err = m6mo_i2c_write_category_8bit(sd, CAF_STOP_MODE, 0x01);
	err = m6mo_i2c_write_category_8bit(sd, AF_START_REG, AF_STOP);
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s():line = %d,  while waiting for capture command time out\n",
			__func__, __LINE__);
		return -ETIME;
	}
	if (state->irq_state != 0x02) {
		pr_err("%s():stop focus fail!irq state = %d\n", __func__, state->irq_state);
		return -1;
	}
	
	/*cancel stop caf mode capture later*/
	err = m6mo_i2c_write_category_8bit(sd, CAF_STOP_MODE, 0x00);
	/*set and start fine focus mode*/
	err = m6mo_i2c_write_category_8bit(sd, AF_SCAN_MODE, AF_FINE_CONTINUOUS_FOCUS);
	err = m6mo_i2c_write_category_8bit(sd, AF_START_REG, AF_START);
	status = wait_for_completion_interruptible_timeout(&state->completion, 
		WAIT_TIMEOUT * HZ);
	if (status <= 0) {
		printk( "%s():line = %d,  while waiting for capture command time out\n",
			__func__, __LINE__);
		return -ETIME;
	}
	if (state->irq_state != 0x02) {
		pr_err("%s():start fine focus fail!irq state = %d\n", __func__, state->irq_state);
		return -1;
	}

	/*read af result*/
	err = m6mo_i2c_read_category_8bit(sd, AF_RESULT, &status);
	if (status != 1) {
		pr_err("%s():fine focus fail, result = %d\n", __func__, status);
		return -1;
	}

	return 0;
}


static int m6mo_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_CAPTURE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_CAPTURE, ctrl->value==%d\n", 	__func__,ctrl->value);
		err =m6mo_start_capture(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		dev_info(&client->dev, "%s: V4L2_CID_WHITE_BALANCE_PRESET, ctrl->value==%d\n", 	__func__,ctrl->value);
		err =m6mo_set_wb_preset(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_EFFECT:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_color_effect(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_BRIGHTNESS, ctrl->value==%d\n", __func__,ctrl->value);		
		err = m6mo_set_image_brightness(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SCENE_MODE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_SCENE_MODE, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_scenemode(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_FOCUS_MODE://focus mode
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_FOCUS_MODE, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_focus_mode(sd,ctrl);
		break;
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_SET_AUTO_FOCUS, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_focus(sd,ctrl);
		break;
	case V4L2_CID_CAMERA_FOCUS_WINDOW:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_FOCUS_WINDOW, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_focus_position(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_ANTI_BANDING:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_ANTI_BANDING, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_anti_banding(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_ZOOM:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_ZOOM, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_zoom_level(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_face_detect(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_FACE_DET_SELECT:
		err = m6mo_set_face_selection(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_ISO:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_ISO, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_iso(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_WDR:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_WDR, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_wdr(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_FLASH_MODE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_FLASH_MODE, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_flash_mode(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_CONTRAST, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_contrast(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SATURATION:
		
	case V4L2_CID_CAMERA_SHARPNESS:

		break;		
	case V4L2_CID_CAMERA_CAF_START_STOP:
		break;
	case V4L2_CID_CAMERA_ROTATION:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_ROTATION, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_rotation(sd, ctrl);
		break;
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAMERA_CHECK_DATALINE:	
	case V4L2_CID_CAMERA_VGA_BLUR:
	//case V4L2_CID_CAMERA_RETURN_FOCUS:
	case V4L2_CID_CAMERA_SET_GAMMA:
	case V4L2_CID_CAMERA_BATCH_REFLECTION:
		err = 0;
		break;
	case V4L2_CID_CAMERA_PANORAMA_CAPTURE:
		err = m6mo_set_panorama_capture(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_PANORAMA_CAPTURE_STATUS:
		err = m6mo_set_pan_cur_info(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_PANORAMA_CAPTURE_PICTURE_NUM:
		err = m6mo_set_pan_cur_info(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_TERMINATE_PANORAMA_CAPTURE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_TERMINATE_PANORAMA_CAPTURE, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_terminate_panorama(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_SMILE_FACE_DETECTION:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_SMILE_FACE_DETECTION, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_smile_face_detection(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_MULTI_CAPTURE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_MULTI_CAPTURE, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_set_multi_capture(sd, ctrl);	
		break;
	case V4L2_CID_CAMERA_WAIT_MULTI_CAPTURE:
		err = m6mo_wait_multi_capture(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_MULTI_CAPTURE_PICTURE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_MULTI_CAPTURE_PICTURE, ctrl->value==%d\n", __func__,ctrl->value);
		err = m6mo_get_multi_capture(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_CAP_WIDTH_HEIGHT:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_CAP_WIDTH_HEIGHT, ctrl->value==%d\n", 	__func__,ctrl->value);
		err = m6mo_set_cap_width_height(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_ISP_REVERSE:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_ISP_REVERSE, ctrl->value==%d\n", 	__func__,ctrl->value);
		err = m6mo_set_vflip(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_ISP_MIRROR:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_ISP_MIRROR, ctrl->value==%d\n", 	__func__,ctrl->value);
		err = m6mo_set_hflip(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_RAW_IMAGE_FLAG:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_RAW_IMAGE_FLAG, ctrl->value==%d\n", 	__func__,ctrl->value);
		err = m6mo_set_raw_image_flag(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_RAW_IMAGE_INDEX:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_RAW_IMAGE_INDEX, ctrl->value==%d\n", 	__func__,ctrl->value);
		err = m6mo_set_raw_image_index(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_COLORBAR:
		dev_info(&client->dev, "%s: V4L2_CID_CAMERA_COLORBAR, ctrl->value==%d\n", 	__func__,ctrl->value);
		err = m6mo_set_colorbar(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_FACE_DETECTION_DIRECTION:
		err = m6mo_set_face_detection_direction(sd, ctrl);
		break;
	default:
		dev_err(&client->dev, "%s: no such control, ctrl->id=%x, ctrl->value==%d\n", __func__,ctrl->id,ctrl->value);
		break;
	}

	return err;
}
static void m6mo_enable_root_irq(struct v4l2_subdev *sd)
{
	m6mo_i2c_write_category_8bit(sd, INT_ROOR_ENABLE, 0x01);
}
static void m6mo_enable_irq(struct v4l2_subdev *sd)
{
	m6mo_i2c_write_category_8bit(sd, INT_ENABLE, 0xff);
	m6mo_i2c_write_category_8bit(sd, INT_ROOR_ENABLE, 0x01);
}

#if 0
static void m6mo_disable_irq(struct v4l2_subdev *sd)
{
	m6mo_i2c_write_category_8bit(sd, INT_ENABLE, 0x01);//only enable system initial int
}

static int m6mo_set_pin(struct v4l2_subdev *sd)
{
	if (m6mo_i2c_batch_write_memory(sd, M6MO_PIN1_ADDRESS, m6mo_pin1_data, M6MO_PIN_VALUE)<0)
	{
		printk("%s: set M6MO_PIN1 fail\n", __func__);
		return -1;
	}

	if (m6mo_i2c_batch_write_memory(sd, M6MO_PIN2_ADDRESS, m6mo_pin2_data, M6MO_PIN_VALUE)<0)
	{
		printk("%s: set M6MO_PIN2 fail\n", __func__);
		return -1;
	}

	if (m6mo_i2c_batch_write_memory(sd, M6MO_PIN3_ADDRESS, m6mo_pin3_data, M6MO_PIN_VALUE)<0)
	{
		printk("%s: set M6MO_PIN3 fail\n", __func__);	
		return -1;
	}
	return 0;
}
#endif

static int m6mo_get_checksum(struct v4l2_subdev *sd, unsigned short * sum)
{
	u32 value;
	u32 chsum;
	int	loop, ret, acc;
	unsigned long	chk_addr, chk_size, set_size;
	unsigned short ret_sum = 0;

	chk_addr = M6MO_SECTION64_FLASH_ADDRESS;
	chk_size = M6MO_FIRMWARE_FILE_SIZE;
	ret_sum  = 0;
	acc = 0x02;	// 16bit unit

	while( chk_size >0 ){
		if( chk_size >= M6MO_SECTION8_FRAME_SIZE ){
			set_size = M6MO_SECTION8_FRAME_SIZE;
		}
		else{
			set_size = chk_size;
		}
		// Set the start address
		ret = m6mo_i2c_write_category_32bit( sd,  FLASH_ADDRESS_REG, chk_addr );
		if (ret) return -ENODEV;
		// Set the size for checksum
		ret = m6mo_i2c_write_category_16bit( sd, FLASH_SIZE_REG, set_size );
		if (ret) return -ENODEV;
		// Start to get the checksum
		ret = m6mo_i2c_write_category_8bit( sd, FLASH_CHKSUM_CMD, acc );
		if (ret) return -ENODEV;
		// Wait for getting the checksum
		for( loop = 0 ; loop < 200 ; loop++ ){
			msleep(10);
			ret = m6mo_i2c_read_category_8bit(sd,  FLASH_CHKSUM_CMD, &value );
			if( ret >=0 && value==0 ){
				ret = m6mo_i2c_read_category_16bit(sd,  FLASH_CHKSUM_RESULT, &chsum );
				if(ret>=0)
				{
					ret_sum += chsum;
				}else{
					printk("get checksum fail\n");
					return -1;
				}
				break;
			}
			else {
				return -ENODEV;
			}
		}
		if( loop >= 200 ){
			return	-1;
		}
		// Next iteration
		chk_addr += set_size;
		chk_size -= set_size;
	}
	*sum = ret_sum;
	
	return ret_sum;
}
static int m6mo_download_section_firmware(struct v4l2_subdev *sd, int flashaddress, const char *data, int size, int interval)
{
	u32 value;
	int offset, i;
	int err;
	int count=0;
	int retry_count;
	
	for(offset=0; offset<size; offset+=interval)
	{	
		//set erase address
		printk("%s: set flash erase address:0x%x\n", __func__, flashaddress+offset);	
		err =m6mo_i2c_write_category_32bit(sd, FLASH_ADDRESS_REG, flashaddress+offset);
		if(err<0)
		{
			printk("%s: set FLASH_ADDRESS_REG fail\n", __func__);	
			return -1;
		}
		//send erase command
		err =m6mo_i2c_write_category_8bit(sd, FLASH_ERASE_CMD, 0x01);
		if(err<0)
		{
			printk("%s: set FLASH_ERASE_CMD fail\n", __func__);	
			return -1;
		}
		//wait for erase finished
		//printk("%s: erase wait...\n", __func__);	
		retry_count = 200; //wait max 10s
		while(retry_count--)  //abount 300ms
		{
			err = m6mo_i2c_read_category_8bit(sd, FLASH_ERASE_CMD, &value);
			if(err<0)
			{
				printk("%s: get FLASH_ERASE_CMD fail\n", __func__);	
				return -1;
			}		
			if(value == 0x00)
				break;
			msleep(50);
		}

		if (retry_count <= 0) {
			printk("%s: get FLASH_ERASE_CMD fail, retry_count %d\n", __func__, retry_count);	
			return -1;
		}

		//printk("%s: set program bytes\n", __func__);
		//set program bytes
		err = m6mo_i2c_write_category_16bit(sd, FLASH_SIZE_REG, (interval==M6MO_SECTION64_FRAME_SIZE)?0:interval);
		if(err<0)
		{
			printk("%s: set FLASH_SIZE_REG fail\n", __func__);	
			return -1;
		}
		
		//clear RAM
		//printk("%s: clear internal RAM\n", __func__);
		err = m6mo_i2c_write_category_8bit(sd, RAM_CLEAN_CMD, 0x01);
		if(err<0)
		{
			printk("%s:  clear RAM fail\n", __func__);	
			return -1;
		}			
		//wait for clear finished
		retry_count = 1000;
		while(retry_count--)
		{
			err = m6mo_i2c_read_category_8bit(sd, RAM_CLEAN_CMD, &value);
			if(value == 0x00)
				break;
			msleep(10);
		}
		if (retry_count <= 0) {
			printk("%s: get FLASH_ERASE_CMD fail, retry_count %d\n", __func__, retry_count);	
			return -1;
		}
		
		//send program firmware
		//printk("%s: download firmware, size=0x%x\n", __func__, interval);
		printk("%s: begin write block data, block size=0x%x\n", __func__, interval);
		for(i=0; i<interval; i+=M6MO_SECTION8_FRAME_SIZE)
		{
			err = m6mo_i2c_batch_write_memory(sd, M6MO_INTERNAL_RAM_ADDRESS+i, 
				data+offset+i, M6MO_SECTION8_FRAME_SIZE);
			if(err<0)
			{
				printk("%s:  send program firmware fail\n", __func__);	
				return -1;
			}		
		}
		printk("%s: end write block data, block size=0x%x\n", __func__, interval);
		//program
		//printk("%s: send program command\n", __func__);	
		err = m6mo_i2c_write_category_8bit(sd, FLASH_WRITE_CMD, 0x01);
		if(err<0)
		{
			printk("%s:  write FLASH_WRITE_CMD fail\n", __func__);	
			return -1;
		}			
		//wait for program finished
		//printk("%s: program wait...\n", __func__);	
		retry_count = 200;
		while(retry_count--)
		{
			err = m6mo_i2c_read_category_8bit(sd, FLASH_WRITE_CMD, &value);
			if(err<0)
			{
				printk("%s:  read FLASH_WRITE_CMD fail\n", __func__);	
				return -1;
			}			
			if(value == 0x00)
				break;
			msleep(50);
		}

		if (retry_count <= 0) {
			printk("%s: get FLASH_ERASE_CMD fail, retry_count %d\n", __func__, retry_count);	
			return -1;
		}
		count++;

		printk("%s:  write count %d##########\n", __func__, count);	
	}

	return 0;
}
static int m6mo_download_firmware(struct v4l2_subdev *sd, const char *data, int size)
{
	int err;
	u16 chsum = 0;
	
	err = m6mo_i2c_write_category_8bit(sd, CAMERA_FLASH_TYPE, 0x00);
	if (err<0)
	{
		printk("%s: set CAMERA_FLASH_TYPE fail\n", __func__);	
		return -1;
	}

	err = m6mo_download_section_firmware(sd, M6MO_SECTION64_FLASH_ADDRESS, \
		data, M6MO_SECTION64_WRITE_SIZE, M6MO_SECTION64_FRAME_SIZE);
	if (err < 0) {
		printk("%s: download section 1 fail\n", __func__);	
		return -1;
	}
	err = m6mo_download_section_firmware(sd, M6MO_SECTION8_FLASH_ADDRESS, \
		data+M6MO_SECTION64_WRITE_SIZE, M6MO_SECTION8_WRITE_SIZE, M6MO_SECTION8_FRAME_SIZE);
	if (err < 0) {
		printk("%s: download section 2 fail\n", __func__);	
		return -1;
	}

	if(m6mo_get_checksum(sd, &chsum) == 0)
	{
		printk("%s: get checksum success, checksum = 0x%04x\n", __func__, chsum);
	}
	else {
		printk("%s: get checksum fail, checksum = 0x%04x\n", __func__, chsum);
		return -1;
	}
	
	printk("%s: download firmware finished!\n", __func__);	
	
	return 0;
}
struct m6mo_reg_bank{
	u8 category;
	u8 range;
};
const struct m6mo_reg_bank m6mo_regs[]={
		{0x0, 0x1f},
		{0x1, 0x3f},
		{0x2, 0x55},
		{0x3, 0x11},
		{0x6, 0x0b},
		{0x7, 0x2f},
		{0x9, 0x2b},
		{0xa, 0x4f},
		{0xb, 0x3a},
		{0xc, 0x18},	
//		{0xf, 0x12},
}; 
static ssize_t store_register(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct v4l2_subdev *sd = dev_get_drvdata(d);
	u32 value;
	u32 reg;
	int type;
	int err;

	err = m6mo_s_power(sd, 1);
	if (err) {
		pr_err("%s():power fail", __func__);
		return err;
	}
	
	if(sscanf(buf, "W%d %x=%x", &type, &reg, &value) == 3)
	{
		switch(type)
		{
		case 8:	
			m6mo_i2c_write_category_8bit(sd, (u16)reg, value);
			break;
		case 16:
			m6mo_i2c_write_category_16bit(sd, (u16)reg, value);
			break;			
		case 32:
			m6mo_i2c_write_category_32bit(sd, (u16)reg, value);
			break;			
		default:
			printk("Invalid type!\n");
			break;			
		}
	}else{
		printk("Invalid format: echo > \"W8/16/32 0x001c=0x0000\" > register\n");
	}

	m6mo_s_power(sd, 0);
	return count;
}
static ssize_t show_register(struct device *d,
		struct device_attribute *attr, char *buf)
{
	struct v4l2_subdev *sd = dev_get_drvdata(d);
	u32 value;
	int err, i, j;
	u16 reg1, reg;

	err = m6mo_s_power(sd, 1);
	if (err) {
		pr_err("%s():power fail", __func__);
		return err;
	}
	
	for(i=0; i<ARRAY_SIZE(m6mo_regs); i++)
	{
		reg1 = m6mo_regs[i].category<<8;
		for(j=0; j<m6mo_regs[i].range; j++)
		{
			reg = reg1 + j;
			err = m6mo_i2c_read_category_8bit(sd, reg, &value);
			if(err>=0)
				printk("REG: %04x=%02x\n", reg, value);
		}
	}
	
	m6mo_s_power(sd, 0);
	
	return 0;
}
static ssize_t store_erase(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	u32 value;
	struct v4l2_subdev *sd = dev_get_drvdata(d);

	err = m6mo_s_power(sd, 1);
	if (err) {
		pr_err("%s():power fail", __func__);
		return err;
	}
	
	err = m6mo_i2c_write_category_8bit(sd, CAMERA_FLASH_TYPE, 0x00);
	if (err<0)
	{
		printk("%s: set CAMERA_FLASH_TYPE fail\n", __func__);	
		goto exit;
	}
	//set erase address
	printk("%s: set flash erase address:0x%x\n", __func__, M6MO_SECTION64_FLASH_ADDRESS);	
	err =m6mo_i2c_write_category_32bit(sd, FLASH_ADDRESS_REG, M6MO_SECTION64_FLASH_ADDRESS);
	if(err<0)
	{
		printk("%s: set FLASH_ADDRESS_REG fail\n", __func__);	
		goto exit;
	}
	//send chip erase command
	err =m6mo_i2c_write_category_8bit(sd, FLASH_ERASE_CMD, 0x02);
	if(err<0)
	{
		printk("%s: set FLASH_ERASE_CMD fail\n", __func__);	
		goto exit;
	}
	//wait for erase finished
	printk("%s: erase wait...\n", __func__);	
	while(1)
	{
		err = m6mo_i2c_read_category_8bit(sd, FLASH_ERASE_CMD, &value);
		if(err<0)
		{
			printk("%s: get FLASH_ERASE_CMD fail\n", __func__);	
			goto exit;
		}		
		if(value == 0x00)
			break;
		msleep(50);
	}
	printk("%s: chip erase OK!\n", __func__);

exit:
	m6mo_s_power(sd, 0);
	return count;
}
static ssize_t show_erase(struct device *d,
		struct device_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t store_download_firmware(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct v4l2_subdev *sd = dev_get_drvdata(d);
	struct m6mo_state *state = to_state(sd);
	const struct firmware *fw = NULL;
	enum firmware_status fw_status = state->fw_status;

	if (state->power) {
		pr_err("camera has power on, please power off first\n");
		return count;
	}

	ret = m6mo_set_power_clock(sd, true);
	if (ret) {
		pr_err("%s():power fail", __func__);
		return count;
	}

	wake_lock(&state->wake_lock);

	m6mo_set_firmware_status(state, FIRMWARE_REQUESTING);
	
	ret = request_firmware(&fw, M6MO_FIRMWARE_FILE_NAME, d);
	if (ret) {
		printk(KERN_ERR "%s() Upload failed. (file not found?)\n", __func__);
		goto exit;
	}
	printk(KERN_INFO "%s() firmware read %Zu bytes.\n",
		__func__, fw->size);

	if (fw->size != M6MO_FIRMWARE_FILE_SIZE) {
		printk(KERN_ERR "m6mo: firmware incorrect size\n");
		goto exit;
	}	

	state->firmware_version = m6mo_get_new_firmware_version(state, fw);
	
	ret = m6mo_download_firmware(sd, fw->data, fw->size);
	if (ret) {
		printk(KERN_ERR "m6mo: download firmware fail\n");
		fw_status = FIRMWARE_LOADED_FAIL;
		goto exit;
	}		

	fw_status = FIRMWARE_LOADED_OK;

exit:
	wake_unlock(&state->wake_lock);
	if (state->power) m6mo_set_power_clock(sd, false);
	m6mo_set_firmware_status(state, fw_status);
	return count;
}

static ssize_t show_download_firmware(struct device *d,
		struct device_attribute *attr, char *buf)
{
	struct v4l2_subdev *sd = dev_get_drvdata(d);
	struct m6mo_state *state = to_state(sd);	
	
	if (state->fw_status == FIRMWARE_LOADED_OK)
		return sprintf(buf, "FIRMWARE_VERSION:0x%x\n", state->firmware_version);
	else
		return sprintf(buf, "FIRMWARE_VERSION:0x%x\n", 0);  /* means incorrect */
}

static ssize_t show_firmware_status(struct device *d,
		struct device_attribute *attr, char *buf)
{
	struct v4l2_subdev *sd = dev_get_drvdata(d);
	struct m6mo_state *state = to_state(sd);	

	return sprintf(buf, "%d\n", state->fw_status);
}

static ssize_t store_debug_reg(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	struct v4l2_subdev *sd = dev_get_drvdata(d);
	struct m6mo_state *state = to_state(sd);

	if (sscanf(buf, "%d", &value) == 1)
		state->debug_reg = !!value;

	return count;
}
static DEVICE_ATTR(download_firmware, 0664, show_download_firmware, store_download_firmware);
static DEVICE_ATTR(erase, 0664, show_erase, store_erase);
static DEVICE_ATTR(register, 0664, show_register, store_register);
static DEVICE_ATTR(firmware_status, 0444, show_firmware_status, NULL);
static DEVICE_ATTR(debug_reg, 0220, NULL, store_debug_reg);

static struct attribute *m6mo_attributes[] = {
	&dev_attr_erase.attr,
	&dev_attr_download_firmware.attr,
	&dev_attr_register.attr,
	&dev_attr_firmware_status.attr,
	&dev_attr_debug_reg.attr,
	NULL
};

static const struct attribute_group m6mo_group = {
	.attrs = m6mo_attributes,
};

static int m6mo_load_firmware(struct v4l2_subdev *sd)
{
	int ret;
	const struct firmware *fw = NULL;
	
	ret = request_firmware(&fw, M6MO_FIRMWARE_FILE_NAME, sd->v4l2_dev->dev);
	if (ret) {
		printk(KERN_ERR "%s() Upload failed. (file %s not found?)\n", __func__, M6MO_FIRMWARE_FILE_NAME);
		return -1;
	}
	printk(KERN_INFO "%s() firmware read %Zu bytes.\n",
		__func__, fw->size);

	if (fw->size != M6MO_FIRMWARE_FILE_SIZE) {
		printk(KERN_ERR "m6mo: firmware incorrect size\n");
		return -1;
	}	
	ret = m6mo_download_firmware(sd, fw->data, fw->size);
	if (ret) {
		printk(KERN_ERR "m6mo: download firmware fail\n");
		return -1;
	}		
	return 0;

}

static int m6mo_init(struct v4l2_subdev *sd, u32 val)
{
	int err=0;
	struct m6mo_state *state = to_state(sd);	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	v4l_info(client, "%s: camera initialization start\n", __func__);
	
	state->initialized = STATE_INIT_PRIVEW;
	state->userset.exposure_bias = 5 / 2;
	state->userset.ae_lock = 0;
	state->userset.awb_lock = 0;
	state->userset.auto_wb = 0;
	state->userset.manual_wb = 0;
	state->userset.wb_temp = 0;	
	state->userset.effect = 0;
	state->userset.brightness = EV_00;
	state->userset.contrast = 5 / 2;
	state->userset.saturation = 5 / 2;
	state->userset.sharpness = 5 / 2;
	state->userset.focus_position = 25;
	state->userset.glamour = 0;	
	state->userset.zoom = 0;
	state->userset.scene = 0;
	state->userset.fast_shutter = 0;
	state->userset.flash_mode = M6MO_FLASH_OFF;
	state->userset.rotation = M6MO_ROTATE_0;
	state->userset.reverse = 0;
	state->userset.mirror = 0;
	state->irq_state = 0;
	state->fps = 0;
	state->stream_on = 0;
	state->camera_mode = SYS_PARAMETER_MODE;
	state->mode = V4L2_CAMERA_PREVIEW;
	state->shutter = STATE_SHUTTER_OFF;
	state->raw_image_flag = 0;
	state->userset.af_scan_mode = AF_FAST_SCAN;

	memset(&state->fmt, 0, sizeof(state->fmt));
	memset(&state->wsize, 0, sizeof(state->wsize));	

	memset(&state->cap_fmt, 0, sizeof(state->cap_fmt));
	memset(&state->cap_wsize, 0, sizeof(state->cap_wsize));

	/*inital camera system*/
	err = m6mo_start_camera(sd);	
	if (err) goto exit;
	
	/*initial m6mo default regs*/
	err = m6mo_write_array_regs(sd, m6mo_default_regs, ARRAY_SIZE(m6mo_default_regs));
	if (err) goto exit;
	
	/*enable all interrept type*/
	m6mo_enable_irq(sd);
	
	v4l_info(client, "%s: camera initialization finished\n", __func__);
	state->initialized = STATE_INIT_COMMAND;

exit:
	return err;
}

static int m6mo_set_power_clock(struct v4l2_subdev *sd, bool enable)
{
	int ret = 0;
	struct m6mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (state->power == enable) return -EBUSY;
	
	if (enable) {
		ret = state->pdata->clock_on(&client->dev, 1);
		if (ret) {
			pr_err("%s:clock on failed\n", __func__);
			return ret;
		}

		ret = state->pdata->power(1);
		if (ret) {
			pr_err("%s:power failed\n", __func__);
			state->pdata->clock_on(&client->dev, 0);
			return ret;
		}
	} else {
		state->pdata->power(0);
		state->pdata->clock_on(&client->dev, 0);
	}

	if (!ret) state->power = enable;

	return ret;
}

static int m6mo_s_power(struct v4l2_subdev *sd, int on)
{
	struct m6mo_state *state = to_state(sd);
	bool enable = !!on;

	if (state->fw_status == FIRMWARE_REQUESTING) 
		return -EBUSY;

	state->initialized = STATE_UNINITIALIZED;

	return m6mo_set_power_clock(sd, enable);
}

static const struct v4l2_subdev_core_ops m6mo_core_ops = {
	.init = m6mo_init,	/* initializing API */
	.queryctrl = m6mo_queryctrl,
	.querymenu = m6mo_querymenu,
	.g_ctrl = m6mo_g_ctrl,
	.s_ctrl = m6mo_s_ctrl,
	.s_gpio = m6mo_s_gpio,
	.reset = m6mo_reset,
	.ioctl = m6mo_ioctl,
	.load_fw = m6mo_load_firmware,
	.s_power = m6mo_s_power,
};

static const struct v4l2_subdev_video_ops m6mo_video_ops = {
	.s_crystal_freq = m6mo_s_crystal_freq,
	.s_mbus_fmt = m6mo_s_fmt,
	.enum_framesizes = m6mo_enum_framesizes,
	.enum_frameintervals = m6mo_enum_frameintervals,
	.enum_mbus_fmt = m6mo_enum_fmt,
	.try_mbus_fmt = m6mo_try_fmt,
	.g_parm = m6mo_g_parm,
	.s_parm = m6mo_s_parm,
	.s_stream = m6mo_s_stream,
};

static const struct v4l2_subdev_ops m6mo_ops = {
	.core = &m6mo_core_ops,
	.video = &m6mo_video_ops,
};


static void m6mo_handle_normal_irq(struct v4l2_subdev *sd, int irq_state)
{
	struct m6mo_state *state = to_state(sd);
	
	printk("%s: handed irq, value 0x%02x\n", __func__, irq_state);
	/*just wake up the waiters*/
	complete(&state->completion);
}


static void m6mo_handle_mcap_irq(struct v4l2_subdev *sd, int irq_state)
{
	struct m6mo_state *state = to_state(sd);
	
	printk("%s: handed irq, value 0x%02x\n", __func__, irq_state);
	
	if (state->mcap_counter >= state->mcap_numbers) {
		if (irq_state & INT_MASK_CAPTURE) {
			pr_info("%s():all multi capture pictures are ready.\n", __func__);
			state->mcap_ready = MCAP_READY;
			complete(&state->completion);
		}
	}
	else if ((irq_state & INT_MASK_SOUND) && (irq_state & INT_MASK_FRAMESYNC)) {
		mutex_lock(&state->m6mo_mutex);
		state->mcap_status[state->mcap_counter] = MCAP_READY;
		state->mcap_counter++;
		pr_info("%s():%d-th multi capture picture.\n", __func__, state->mcap_counter);
		mutex_unlock(&state->m6mo_mutex);
		complete(&state->completion);
		m6mo_enable_root_irq(sd);
	}
}


static void update_pan_status(struct v4l2_subdev *sd, int index, int status)
{
	struct m6mo_state *state = to_state(sd);
	int err = -EINVAL;

	/*lock first*/
	mutex_lock(&state->m6mo_mutex);
	
	switch (status) {
	case PAN_SUCCESS:
	case PAN_FATAL_ERR:
	case PAN_UNKNOWN_ERR:
		err = 0;
		break;
	case PAN_RETRY_ERR:
		{
		int x, y;
		err = m6mo_i2c_read_category_16bit(sd, PANO_OFFSETX_H, &x);
		err = m6mo_i2c_read_category_16bit(sd, PANO_OFFSETY_H, &y);
		state->pan_info[index].extra = ((x & 0xffff) << 16) | (y & 0xffff);
		}
		break;
	case PAN_BIG_ERR:
		{
		int value;
		err = m6mo_i2c_read_category_8bit(sd, PAN_ERROR_NO, &value);
		state->pan_info[index].extra = value;
		}
		break;
	case PAN_COMPLETE:
		err = 0;
		break;
	default:
		break;
	}

	if (!err) state->pan_info[index].status = status;
	
	mutex_unlock(&state->m6mo_mutex);
}


static void m6mo_handle_pan_irq(struct v4l2_subdev *sd, int irq_state)
{
	int err;
	struct m6mo_state *state = to_state(sd);

	if (irq_state & INT_MASK_ZOOM) {
		int value;
		err = m6mo_i2c_read_category_8bit(sd, PAN_ERROR_NO,  &value);
		if (!value) {
			state->pan_ready = PAN_READY;
			printk("stitching all images success.\n");
		}
		else {
			state->pan_ready = PAN_READY_FAIL;
			printk("stitching all images fail, 0x%02x reg is 0x%02x.\n", PAN_ERROR_NO, value);
		}
		complete(&state->completion);
	}
	else if (irq_state & INT_MASK_MODE) {
		update_pan_status(sd, state->pan_counter, PAN_COMPLETE);
		state->pan_counter++;
		pr_info("Finish panorama capture!\n");		
	}
	else if (irq_state & INT_MASK_CAPTURE) {
		if (state->pan_counter >= PAN_MAX_PICTURE) {
			update_pan_status(sd, state->pan_counter, PAN_FATAL_ERR);
			pr_err("***panorama picture counter is overhead***\n");
			return;
		}
		else {
			update_pan_status(sd, state->pan_counter, PAN_SUCCESS);
			state->pan_counter++;
			pr_info("Valid %d-th image captured.\n", state->pan_counter);
		}
	}
	else if (irq_state & INT_MASK_FRAMESYNC) {
		update_pan_status(sd, state->pan_counter, PAN_RETRY_ERR);
	}
	else if (irq_state & INT_MASK_FD) {
		update_pan_status(sd, state->pan_counter, PAN_BIG_ERR);
	}
	else if (irq_state & INT_MASK_SOUND) {
		pr_err("Fatal error found in captured image!\n");
		update_pan_status(sd, state->pan_counter, PAN_FATAL_ERR);
		return;
	}
	else {
		update_pan_status(sd, state->pan_counter, PAN_UNKNOWN_ERR);
		pr_err("%s():other interrupt status in panorama capture process!0x%02x\n", 
			__func__, irq_state);
	}
}


static void m6mo_handle_smile_irq(struct v4l2_subdev *sd, int irq_state)
{
	struct m6mo_state *state = to_state(sd);
	
	printk("%s: handed irq, value 0x%02x\n", __func__, irq_state);

	if (irq_state & INT_MASK_FRAMESYNC) {
		pr_info("*********%s():smile detection********\n", __func__);
		state->smile_detection_result = 1;
		complete(&state->completion);
	}
}

static void m6mo_handle_work(struct work_struct *work)
{
	struct m6mo_state *state = container_of(work, struct m6mo_state, m6mo_work);
	struct v4l2_subdev *sd = &state->sd;
	u32 irq_state = 0;

	if(state->initialized != STATE_UNINITIALIZED)
	{
		m6mo_i2c_read_category_8bit(sd, INT_FACTOR, &irq_state);
		state->irq_state = irq_state;	

		switch (state->irq_mode) {
		case M6MO_IRQ_NORMAL:
			m6mo_handle_normal_irq(sd, irq_state);
			break;
		case M6MO_IRQ_MULTI_CAP:
			m6mo_handle_mcap_irq(sd, irq_state);
			break;
		case M6MO_IRQ_PANORAMA:
			m6mo_handle_pan_irq(sd, irq_state);
			break;
		case M6MO_IRQ_SMILE_DETECT:
			m6mo_handle_smile_irq(sd, irq_state);
			break;
		default:
			pr_err("%s():unknown irq mode!", __func__);
			break;
		}
	}
}


static irqreturn_t m6mo_handle_irq(int irq, void *handle)
{
	struct m6mo_state *state = (struct m6mo_state *)handle;

	if (!state->power) return IRQ_HANDLED;

	queue_work(state->m6mo_wq, &state->m6mo_work);
	
	return IRQ_HANDLED;
}

static int m6mo_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct v4l2_subdev *sd =i2c_get_clientdata(client);
	struct m6mo_state *state = to_state(sd);

	free_irq(state->irq, state);
	return 0;
}
static int m6mo_resume(struct i2c_client *client)
{
	struct v4l2_subdev *sd =i2c_get_clientdata(client);
	struct m6mo_state *state = to_state(sd);
	int err;
	
	err = request_irq(state->irq, m6mo_handle_irq, 
	    IRQF_TRIGGER_RISING, client->name, state);
	if (err){
		printk(KERN_ERR "Failed to request IRQ for camera INT\n");
	}
	return 0;
}

/*
  * get current firmware version from ISP
*/
static int m6mo_get_firmware_version(struct m6mo_state *state)
{
	int ret;
	u32 val;
	struct v4l2_subdev *sd = &state->sd;
	
	state->initialized = STATE_INIT_PRIVEW;
	ret = m6mo_start_camera(sd);	
	if (ret) {
		pr_err("%s:start firmware fail\n", __func__);
		return -EIO; /*other error ? like i2c*/
	}

	ret = m6mo_i2c_read_category_16bit(sd, VER_FIRMWARE_MINOR, &val);
	if(ret) {
		pr_err("%s:get firmware version fail\n", __func__);
		return -EIO;
	}
	
	return val;
}

/*
  * get firmware update decision
  * if checksum error or old version isn't equal to new version, return true
  * if some errors happen, return false, we don't need to update firmware
*/
static bool m6mo_get_update_decision(struct m6mo_state *state, 
	const struct firmware *fw)
{
	int ret, old_version;
	unsigned short chsum;
	bool decision = false;
	struct v4l2_subdev *sd = &state->sd;

	/* first to get new version */
	state->firmware_version = m6mo_get_new_firmware_version(state, fw);

	/* then to get old version */
	ret = m6mo_set_power_clock(sd, true);
	if (ret) {
		pr_err("%s():power fail\n", __func__);
		goto exit_decision;
	}
	msleep(100);
	
	/*first to do a checksum*/
	if(m6mo_get_checksum(sd, &chsum) != 0) {
		/*if get checksum fail, it means isp has no firmware or wrong firmware*/
		pr_err("%s: get checksum fail, checksum = 0x%04x!\n", __func__, chsum);
		decision = true;
		goto exit_decision;
	}
	
	/*if checksum ok, start camera and get current firmware version*/
	old_version = m6mo_get_firmware_version(state);
	if (old_version < 0) goto exit_decision;

	pr_info("firmware old version = 0x%04x, new version = 0x%04x\n",  
		old_version, state->firmware_version);

	/* if old version is not equal to new version, need to update */
	if (old_version != state->firmware_version)
		decision = true;
	
exit_decision:
	if (state->power) m6mo_set_power_clock(sd, false);	
	return decision;
}

static void m6mo_firmware_request_complete_handler(const struct firmware *fw,
						  void *context)
{
	int ret, retry_count = 3;
	struct v4l2_subdev * sd = (struct v4l2_subdev *)context;
	struct m6mo_state *state = to_state(sd);
	enum firmware_status fw_status = FIRMWARE_LOADED_OK;
	
	if (fw != NULL) {
		/* check firmware size */
		if (fw->size != M6MO_FIRMWARE_FILE_SIZE) {
			pr_err("m6mo: firmware incorrect size(%d)\n", fw->size);
			goto err_firmware_fail;
		}	

		/* get update firmware decision, If update fail, retry 3 times*/
		if (m6mo_get_update_decision(state, fw)) {
			/* retry 3 times */
			while (retry_count--) {
				ret = m6mo_set_power_clock(sd, true);
				if(ret) {
					msleep(500);
					continue;
				}
				msleep(100);
				
				wake_lock(&state->wake_lock);
				
				ret = m6mo_download_firmware(sd, fw->data, M6MO_FIRMWARE_FILE_SIZE);
				if (ret) {
					pr_err("%s: download firmware fail\n", __func__);
					wake_unlock(&state->wake_lock);
					m6mo_set_power_clock(sd, false);
					msleep(500);
					continue;
				}
				
				wake_unlock(&state->wake_lock);
				break;
			}

			if (retry_count < 0)
				fw_status = FIRMWARE_LOADED_FAIL;
			else
				fw_status = FIRMWARE_LOADED_OK;
		}
	} else {
		pr_info("failed to copy m6mo F/W during init\n");
	}

err_firmware_fail:
	if (state->power) m6mo_set_power_clock(sd, false);
	m6mo_set_firmware_status(state, fw_status);
}

static int m6mo_check_firmware(struct v4l2_subdev *sd)
{
	int ret = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m6mo_state *state = to_state(sd);

	m6mo_set_firmware_status(state, FIRMWARE_REQUESTING);
	
	ret = request_firmware_nowait(THIS_MODULE,
				      FW_ACTION_HOTPLUG,
				      M6MO_FIRMWARE_FILE_NAME,
				      &client->dev,
				      GFP_KERNEL,
				      sd,
				      m6mo_firmware_request_complete_handler);
	if (ret) {
		dev_err(&client->dev, "could not load firmware (err=%d)\n", ret);
		/* If load system firmware fail, set loaded ok as well, or the camera may not be opened */
		m6mo_set_firmware_status(state, FIRMWARE_LOADED_OK);
	}
	
	return ret;
}

/*
 * m6mo_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */

static int m6mo_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct m6mo_state *state;
	struct v4l2_subdev *sd;	
	int err;
	
	printk( " m6mo_probe():client name %s#################################\n", client->name);

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		return -ENODEV;
	}
	
	/*alloc m6mo state*/
	state = kzalloc(sizeof(struct m6mo_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	
	/*alloc memery read/write buffer*/
	state->buffer = kzalloc(M6MO_SECTION8_FRAME_SIZE+32, GFP_KERNEL);
	if (state->buffer == NULL) {
		err = -ENOMEM;
		goto err_alloc_buffer;
	}

	state->initialized = STATE_UNINITIALIZED;
	sd = &state->sd;
	state->pdata = client->dev.platform_data;
	state->irq_mode = M6MO_IRQ_NORMAL;
	state->power = 0;
	state->fled_regulator = NULL;
	state->debug_reg = false;
	m6mo_set_firmware_status(state, FIRMWARE_NONE);
	INIT_WORK(&state->m6mo_work, m6mo_handle_work);
	mutex_init(&state->m6mo_mutex);
	wake_lock_init(&state->wake_lock, WAKE_LOCK_SUSPEND, "m6mo");
	init_completion(&state->completion);
	strcpy(sd->name, M6MO_DRIVER_NAME);
	
	state->m6mo_wq = create_singlethread_workqueue("m6mo_wq");
	if(!state->m6mo_wq){
		printk("Failed to setup workqueue - m6mo_wq \n");
		err = -EINVAL;
		goto err_create_workqueue;
	}
	
	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &m6mo_ops);
	add_v4l2_i2c_subdev(sd);

	/* Enable INT Interrupt request */
	state->irq = gpio_to_irq(client->irq);
	printk("%s: request irq = %d, gpio=%d #####\n", __func__, state->irq,  client->irq);
	if (state->irq < 0) {
		err = -EINVAL;
		goto err_irq;
	}
	
	err = request_irq(state->irq, m6mo_handle_irq, 
	    IRQF_TRIGGER_RISING, client->name, state);
	if (err){
		printk(KERN_ERR "Failed to request IRQ for camera INT\n");
		goto err_irq;
	}

	err = sysfs_create_group(&client->dev.kobj, &m6mo_group);
	if (err) {
		dev_err(&client->dev, "Failed to create sysfs files\n");
		goto err_create_sys;
	}

	if ((!state->pdata->init) || 
		(!state->pdata->power) ||
		(!state->pdata->clock_on)) {
		dev_err(&client->dev, "platform data has no init, power and clock_on functions\n");
		goto err_init_pdata;
	}
	
	err = state->pdata->init(&client->dev);
	if (err) goto err_init_pdata;

	err = m6mo_check_firmware(sd);
	if (err) goto err_init_pdata;
	
	dev_info(&client->dev, "m6mo has been probed\n");
	
	return 0;

err_init_pdata:
	sysfs_remove_group(&client->dev.kobj, &m6mo_group);
err_create_sys:
	free_irq(state->irq, state);
err_irq:
	del_v4l2_i2c_subdev(sd);
	v4l2_device_unregister_subdev(sd);
	destroy_workqueue(state->m6mo_wq);	
err_create_workqueue:
	wake_lock_destroy(&state->wake_lock);
	kfree(state->buffer);
err_alloc_buffer:
	kfree(state);
	return err;
}


static int m6mo_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct m6mo_state *state = to_state(sd);

	flush_workqueue(state->m6mo_wq);
	sysfs_remove_group(&client->dev.kobj, &m6mo_group);
	free_irq(state->irq, state);
	del_v4l2_i2c_subdev(sd);
	v4l2_device_unregister_subdev(sd);
	destroy_workqueue(state->m6mo_wq);	
	kfree(state->buffer);
	kfree(state);
	
	dev_info(&client->dev, "m6mo has been removed\n");	
	
	return 0;
}

static const struct i2c_device_id m6mo_id[] = {
	{ M6MO_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, m6mo_id);


static struct i2c_driver m6mo_i2c_driver = {
	.driver = {
		.name = M6MO_DRIVER_NAME,
	}, 
	.probe = m6mo_probe,
	.remove = m6mo_remove,
	.suspend	= m6mo_suspend,
	.resume		= m6mo_resume,
	.id_table = m6mo_id,
};

static int __init m6mo_module_init(void)
{
	return i2c_add_driver(&m6mo_i2c_driver);
}

static void __exit m6mo_module_exit(void)
{
	i2c_del_driver(&m6mo_i2c_driver);
}

module_init(m6mo_module_init);
module_exit(m6mo_module_exit);


MODULE_DESCRIPTION("8M Camera With Fujitsu M6MO ISP driver");
MODULE_AUTHOR("WenBin Wu<wenbinwu@meizu.com>");
MODULE_LICENSE("GPL");


