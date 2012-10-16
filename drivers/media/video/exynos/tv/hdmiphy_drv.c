/*
 * Samsung HDMI Physical interface driver
 *
 * Copyright (C) 2010-2011 Samsung Electronics Co.Ltd
 * Author: Tomasz Stanislawski <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/err.h>

#include <media/v4l2-subdev.h>

#include "hdmi.h"

MODULE_AUTHOR("Tomasz Stanislawski <t.stanislaws@samsung.com>");
MODULE_DESCRIPTION("Samsung HDMI Physical interface driver");
MODULE_LICENSE("GPL");

const u8 *hdmiphy_preset2conf(u32 preset)
{
	int i;
	for (i = 0; i < hdmiphy_conf_cnt; ++i)
		if (hdmiphy_conf[i].preset == preset)
			return hdmiphy_conf[i].data;
	return NULL;
}

static int hdmiphy_s_power(struct v4l2_subdev *sd, int on)
{
	/* to be implemented */
	return 0;
}

static int hdmiphy_s_dv_preset(struct v4l2_subdev *sd,
	struct v4l2_dv_preset *preset)
{
	int i;
	const u8 *data;
	u8 buffer[32];
	u8 recv_buffer[32];
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct device *dev = &client->dev;

	dev_dbg(dev, "s_dv_preset(preset = %d)\n", preset->preset);
	data = hdmiphy_preset2conf(preset->preset);
	if (!data) {
		dev_err(dev, "format not supported\n");
		return -EINVAL;
	}

	memset(recv_buffer, 0, 32);

	i2c_master_recv(client, recv_buffer, 32);
	for (i = 0; i < 32; i++) {
		dev_dbg(dev, "[%x]", recv_buffer[i]);
		if (!(i % 8) && i)
			dev_dbg(dev, "\n");
	}
	dev_dbg(dev, "\n");

	/* storing configuration to the device */
	memcpy(buffer, data, 32);
	ret = i2c_master_send(client, buffer, 32);
	if (ret != 32) {
		dev_err(dev, "failed to configure HDMIPHY via I2C\n");
		return -EIO;
	}

	i2c_master_recv(client, recv_buffer, 32);
	for (i = 0; i < 32; i++) {
		dev_dbg(dev, "[%x]", recv_buffer[i]);
		if (!(i % 8) && i)
			dev_dbg(dev, "\n");
	}
	dev_dbg(dev, "\n");

	return 0;
}

static int hdmiphy_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct device *dev = &client->dev;
	u8 buffer[2];
	int ret;

	dev_dbg(dev, "s_stream(%d)\n", enable);
	/* going to/from configuration from/to operation mode */
	buffer[0] = 0x1f;
	buffer[1] = enable ? 0x80 : 0x00;

	ret = i2c_master_send(client, buffer, 2);
	if (ret != 2) {
		dev_err(dev, "stream (%d) failed\n", enable);
		return -EIO;
	}
	return 0;
}

static const struct v4l2_subdev_core_ops hdmiphy_core_ops = {
	.s_power =  hdmiphy_s_power,
};

static const struct v4l2_subdev_video_ops hdmiphy_video_ops = {
	.s_dv_preset = hdmiphy_s_dv_preset,
	.s_stream =  hdmiphy_s_stream,
};

static const struct v4l2_subdev_ops hdmiphy_ops = {
	.core = &hdmiphy_core_ops,
	.video = &hdmiphy_video_ops,
};

static int __devinit hdmiphy_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	static struct v4l2_subdev sd;

	printk("hdmiphy_probe start\n");

	v4l2_i2c_subdev_init(&sd, client, &hdmiphy_ops);
	dev_info(&client->dev, "probe successful\n");
	return 0;
}

static int __devexit hdmiphy_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "remove successful\n");
	return 0;
}

static const struct i2c_device_id hdmiphy_id[] = {
	{ "hdmiphy", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, hdmiphy_id);

static struct i2c_driver hdmiphy_driver = {
	.driver = {
		.name	= "s5p-hdmiphy",
		.owner	= THIS_MODULE,
	},
	.probe		= hdmiphy_probe,
	.remove		= __devexit_p(hdmiphy_remove),
	.id_table = hdmiphy_id,
};

static int __init hdmiphy_init(void)
{
	printk("hdmiphy init start\n");
	return i2c_add_driver(&hdmiphy_driver);
}
module_init(hdmiphy_init);

static void __exit hdmiphy_exit(void)
{
	i2c_del_driver(&hdmiphy_driver);
}
module_exit(hdmiphy_exit);
