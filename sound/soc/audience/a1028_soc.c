/*
 * Misc driver for audience A1028
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author:  Lvcha qiu <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/a1028_soc.h>
#include <linux/a1028_param.h>
#include <asm/mach-types.h>

//#define DEBUG
#undef DEBUG

#define	_CMD_FIFO_USED_

#define A1028_12M_SOC_FW "a1028_12m_soc_fw.bin"
#define A1028_24M_SOC_FW "a1028_24m_soc_fw.bin"

static int a1028_soc_setmode(struct a1028_soc *a1028, int mode);
static void a1028_soc_firmware_handler(const struct firmware *fw,void *context);

static int a1028_i2c_read(struct i2c_client *client,
				  int bytes, void *dest)
{
	struct i2c_msg xfer;
	int ret;

	xfer.addr = client->addr;
	xfer.flags = I2C_M_RD;
	xfer.len = bytes;
	xfer.buf = (char *)dest;

	ret = i2c_transfer(client->adapter, &xfer, 1);

	if (ret < 0)
		goto err_read;
	if (ret != 1) {
		ret = -EIO;
		goto err_read;
	}

	return 0;

err_read:
	return ret;
}

static int a1028_i2c_write(struct i2c_client *client,
				int bytes, const void *src)
{
	struct i2c_msg xfer;
	int ret;

	xfer.addr = client->addr;
	xfer.flags = 0;
	xfer.len = bytes;
	xfer.buf = (char *)src;

	ret = i2c_transfer(client->adapter, &xfer, 1);
	if (ret < 0)
		goto err_write;
	if (ret != 1) {
		ret = -EIO;
		goto err_write;
	}

	return 0;

err_write:
	return ret;
}


static void a1028_soc_coldreset(struct a1028_soc *a1028)
{
	int err = 0;
	const struct firmware *fw;
	const char *fw_name;

	if(machine_is_m030()) {
		fw_name = A1028_12M_SOC_FW;
	} else {
		fw_name = A1028_24M_SOC_FW;
	}

	err = request_firmware(&fw, fw_name,  a1028->dev);
	if (err) {
		printk(KERN_ERR "Failed to load firmware \"%s\"\n", fw_name);
		return;
	}

	a1028_soc_firmware_handler(fw,(void *)a1028);
	pr_info("%s:A1028:load firmware %s\n", dev_name(a1028->dev), fw_name);
}


static int a1028_soc_softreset(struct a1028_soc *a1028)
{
	int ret = 0;

	if (A1028_RST != a1028->status) {
		unsigned long cmd = be32_to_cpu(A1028_MSG_RESET);
		ret = a1028_i2c_write(a1028->client, sizeof(cmd), &cmd);
		if (!ret) {
			a1028->status = A1028_RST;
			msleep(20);
		}
	}
	return ret;
}

static int a1028_soc_execute_cmdmsg(struct a1028_soc *a1028,unsigned int msg)
{
	int ret = 0;
	int retries, pass = 0;
	unsigned char msgbuf[4];
	unsigned char chkbuf[4];

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

	memcpy(chkbuf, msgbuf, 4);
	retries = POLLING_RETRY_CNT;
	while (retries--) {

		ret = a1028_i2c_write(a1028->client, 4,msgbuf);
		if (ret < 0)
		{
			pr_err("%s: a1028_i2c_write error %d\n", __func__, ret);
			a1028_soc_softreset(a1028);
			return -1;
		}

		msleep(20);

		memset(msgbuf, 0, sizeof(msgbuf));
		ret = a1028_i2c_read(a1028->client, 4,msgbuf);
		if (ret < 0)
		{
			pr_err("a1028_i2c_read error, ret = %d\n", ret);
			continue;
		}

		if (msgbuf[0] == 0x80  && msgbuf[1] == chkbuf[1]) {
			pass = 1;
			break;
		} else if (msgbuf[0] == 0xff && msgbuf[1] == 0xff) {
			pr_err("%s: illegal cmd 0x%08X\n", __func__, msg);
			ret = -EINVAL;
			break;
		} else if ( msgbuf[0] == 0x00 && msgbuf[1] == 0x00 ) {
			pr_info("%s: not ready (%d retries)\n", __func__,retries);
			ret = -EBUSY;
		} else {
			pr_info("%s: cmd/ack mismatch: (%d retries left)\n",__func__,retries);
#ifdef DEBUG
			pr_info("%s: msgbuf[0] = %x\n", __func__, msgbuf[0]);
			pr_info("%s: msgbuf[1] = %x\n", __func__, msgbuf[1]);
			pr_info("%s: msgbuf[2] = %x\n", __func__, msgbuf[2]);
			pr_info("%s: msgbuf[3] = %x\n", __func__, msgbuf[3]);
#endif
			ret = -EBUSY;
		}
	}

	if (!pass) {
		pr_err("%s: failed execute cmd 0x%08X (%d)\n", __func__,msg, ret);
		a1028_soc_softreset(a1028);
	}
	return ret;
}


static int a1028_soc_sleep(struct a1028_soc *a1028)
{
	int ret = 0;

	if (A1028_SLEEP != a1028->status) {
		msleep(20); // wait sleep
		ret = a1028_soc_execute_cmdmsg(a1028,A1028_MSG_SLEEP);
		if (ret < 0) {
			pr_err("%s: suspend error\n", __func__);
		}
	}
	a1028->status = A1028_SLEEP;
	msleep(120); // wait sleep
	return ret;
}

static int a1028_soc_wakeup(struct a1028_soc *a1028)
{
	int ret = 0,retry = 3;

	if (A1028_NORMAL != a1028->status) {
		do{
			gpio_set_value(a1028->gpio_wake, 0);
			msleep(30);
			gpio_set_value(a1028->gpio_wake, 1);

			ret = a1028_soc_execute_cmdmsg(a1028,A1028_MSG_SYNC);
		 } while ((ret < 0) && --retry);

		if (!ret) {
			a1028->status = A1028_NORMAL;
		}
	}

	return ret;
}

static int a1028_build_label(struct a1028_soc *a1028,char * buf)
{
	unsigned int firstChar = be32_to_cpu(A1028_VER0);
	unsigned int nextChar = be32_to_cpu(A1028_VER1);

	unsigned char msg[4] = {0x00, 0x00, 0x00, 0x01};
	int ret,i=0;
	char * str = buf;

	if (A1028_NORMAL != a1028->status)
	{
		printk("a1028 is in %d status\n",a1028->status);
		*str = '\0';
		return -1;
	}

    // get 1st build label char
	ret = a1028_i2c_write(a1028->client, sizeof(firstChar),&firstChar);
	if (ret < 0)
	{
		printk("\n A1026_WRITE_MSG (0x%.8X) error, ret = %d\n", be32_to_cpu(firstChar),ret);
		return -1;
	}

	while(1){
		msleep(20);

		ret = a1028_i2c_read(a1028->client, 4,msg);
		if (ret < 0)
		{
		    printk("\n A1026_READ_DATA error, ret = %d\n", ret);
		    return -1;
		}

		*(str+i) = msg[3];
		if(msg[3] == 0)
		{
			if(i==0) continue;
			break;
		}

		// get next build label char
		ret = a1028_i2c_write(a1028->client, sizeof(nextChar),&nextChar);
		if (ret < 0)
		{
		    printk("\n A1026_WRITE_MSG (0x%.8X) error, ret = %d\n", be32_to_cpu(nextChar),ret);
		    return -1;
		}

		i++;
	};

	return 0;
}

unsigned int a1028_cmd = A1028_MSG_SYNC;
int a1028_send_cmd(struct a1028_soc *a1028,unsigned int cmd)
{
	int ret;
	unsigned int msg;

	if (A1028_NORMAL == a1028->status)
	{
		msg = be32_to_cpu(cmd);

		ret = a1028_i2c_write(a1028->client, 4,&msg);
		if (ret < 0)
		{
			printk("a1028_i2c_write (0x%.8X) error, ret = %d\n", cmd,ret);
			return -1;
		}
		else
		{
			printk("a1028_i2c_write (0x%.8X) OK\n", cmd);
			a1028_cmd = cmd;
		}
	}
	else
	{
		printk("a1028 is in %d mode\n",a1028->status);
		return -1;
	}
	return 0;
}

int a1028_get_cmd(struct a1028_soc *a1028,unsigned int cmd)
{
	int ret = -1;
	unsigned int msg;

	if (A1028_NORMAL == a1028->status)
	{
		ret = a1028_i2c_read(a1028->client, 4,&msg);
		if (ret < 0)
		{
			printk("a1028_i2c_read error, ret = %d\n", ret);
			return -EINVAL;
		}
		else
		{
			printk("a1028_i2c_read (0x%.8x) OK\n", be32_to_cpu(msg));
			ret = be32_to_cpu(msg);
		}
	}
	else
	{
		printk("a1028 is in %d mode\n",a1028->status);
		ret = -EPERM;
	}
	return ret;
}

static ssize_t a1028_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf);
static ssize_t a1028_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count);

#define A1028_ATTR(_name)\
{\
    .attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR},\
    .show = a1028_show_property,\
    .store = a1028_store,\
}

static struct device_attribute a1028_attrs[] = {
    A1028_ATTR(mode),
    A1028_ATTR(status),
    A1028_ATTR(cmd),
    A1028_ATTR(nr_bt),
    A1028_ATTR(reset),
    A1028_ATTR(version),
};
enum {
	A1028_NR_MODE,
	A1028_NR_STATUS,
	A1028_NR_CMD,
	A1028_NR_BT,
	A1028_NR_RESET,
	A1028_FWR_VER,
};
static ssize_t a1028_show_property(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
	int i = 0;
	ptrdiff_t off;
	struct a1028_soc *a1028 = (struct a1028_soc*)dev_get_drvdata(dev);

	if(!a1028)
	{
		pr_err("%s(): failed!!!\n", __func__);
		return -ENODEV;
	}

	off = attr - a1028_attrs;

	switch(off){
	case A1028_NR_MODE:
		i += scnprintf(buf+i, PAGE_SIZE-i, "%d\n",a1028->mode);
		break;
	case A1028_NR_STATUS:
		i += scnprintf(buf+i, PAGE_SIZE-i, "%d\n",a1028->status);
		break;
	case A1028_NR_CMD:
		i += scnprintf(buf+i, PAGE_SIZE-i, "0x%.8X\n",a1028_get_cmd(a1028,a1028_cmd));
		break;
	case A1028_NR_BT:
		i += scnprintf(buf+i, PAGE_SIZE-i, "%d\n",a1028->nr_bt);
		break;
	case A1028_FWR_VER:
		{
			char str[64];
			a1028_build_label(a1028,str);
			i += scnprintf(buf+i, PAGE_SIZE-i, "%s\n",str);
		}
		break;
	case A1028_NR_RESET:
		i += scnprintf(buf+i, PAGE_SIZE-i, "\n");
		break;
	default:
		i += scnprintf(buf+i, PAGE_SIZE-i, "Error\n");
		break;
	}
	return i;
}

static ssize_t a1028_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned int reg,value;
	int ret = 0;
	ptrdiff_t off;
	struct a1028_soc *a1028 = (struct a1028_soc*)dev_get_drvdata(dev);

	if(!a1028)
	{
		pr_err("%s(): failed!!!\n", __func__);
		return -ENODEV;
	}

	off = attr - a1028_attrs;

	switch(off){
	case A1028_NR_MODE:
		if (sscanf(buf, "%d\n", &value) == 1) {
			a1028_soc_setmode(a1028,value);
		}
		ret = count;
		break;
	case A1028_NR_STATUS:
		if (sscanf(buf, "%d\n", &value) == 1) {
			switch(value)
			{
				case A1028_NORMAL:
					a1028_soc_wakeup(a1028);
					break;

				case A1028_SLEEP:
					a1028_soc_sleep(a1028);
					break;

				case A1028_RST:
					a1028_soc_softreset(a1028);
					break;

				default:
					pr_err("A1028: wrong status %d!!\n", value);
					break;
			}
		}
		ret = count;
		break;
	case A1028_NR_CMD:
		if (sscanf(buf, "%x %x", &reg, &value) == 2) {
			a1028_send_cmd(a1028,reg);
			a1028_send_cmd(a1028,value);
		}
		else if (sscanf(buf, "%x\n", &value) == 1) {
			a1028_send_cmd(a1028,value);
		}
		else {
			pr_err("%s(): failed > 2!!!\n", __func__);
		}
		ret = count;
		break;
	case A1028_NR_BT:
		if (sscanf(buf, "%x\n", &value) == 1) {
			a1028->nr_bt = !!value;
			//if( a1028->mode == A1028_INCALL_BT )
			//	a1028_soc_setmode(a1028,A1028_INCALL_BT);
			dev_info(a1028->dev, "update nr for bt %s \n", a1028->nr_bt?"On":"off");
		}
		ret = count;
		break;
	case A1028_NR_RESET:
		if (sscanf(buf, "%x\n", &value) == 1) {
			if(value)
				a1028_soc_coldreset(a1028);
			else
				a1028_soc_softreset(a1028);

			dev_info(a1028->dev, "Firmware %s reset. \n", value?"cold":"soft");
		}
		ret = count;
		break;
	case A1028_FWR_VER:
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int a1028_create_attrs(struct device * dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(a1028_attrs); i++) {
		rc = device_create_file(dev, &a1028_attrs[i]);
		if (rc)
			goto a1028_attrs_failed;
	}
	goto succeed;

a1028_attrs_failed:
	printk(KERN_INFO "%s(): failed!!!\n", __func__);
	while (i--)
		device_remove_file(dev, &a1028_attrs[i]);
succeed:
	return rc;

}

static void a1028_destroy_atts(struct device * dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(a1028_attrs); i++)
		device_remove_file(dev, &a1028_attrs[i]);
}

static int a1028_soc_config(struct a1028_soc *a1028, enum a1028_mode mode)
{
	const u8 *a1028_param;
	u8 ack_buf[256] ={0,};
	int size;
	int ret = 0;

	pr_info("A1028: set a1028 into %s mode\n", (mode == A1028_SUSPEND?"Suspend"
		:mode == A1028_INCALL_RECEIVER?"Receiver"
		:mode == A1028_INCALL_HEADSET?"Headset"
		:mode == A1028_INCALL_SPEAKER?"Speaker"
		:mode == A1028_INCALL_BT?"Bluetooth"
		:mode == A1028_BYPASS?"Bypass":"Unknown"));

	switch (mode) {
	case A1028_SUSPEND:
		//a1028_param = suspend_mode;
		//size = sizeof(suspend_mode);
		return a1028_soc_sleep(a1028);
		break;
	case A1028_INCALL_RECEIVER:
		if(machine_is_m030())
		{
			a1028_param = buf_ct_tuning_m030;
			size = sizeof(buf_ct_tuning_m030);
		}
		else
		{
			a1028_param = buf_ct_tuning;
			size = sizeof(buf_ct_tuning);
		}
		break;
	case A1028_INCALL_HEADSET:
		a1028_param = buf_whs_tuning;
		size = sizeof(buf_whs_tuning);
		break;
	case A1028_INCALL_SPEAKER:
		a1028_param = buf_ft_tuning;
		size = sizeof(buf_ft_tuning);
		break;
	case A1028_INCALL_BT:
		if(a1028->nr_bt)
		{
			a1028_param = buf_bt_tuning;
			size = sizeof(buf_bt_tuning);
		}
		else
		{
			a1028_param = buf_bt_tuning_VPoff;
			size = sizeof(buf_bt_tuning_VPoff);
		}
		dev_info(a1028->dev, "set nr for bt %s \n", a1028->nr_bt?"On":"off");
		break;
	case A1028_BYPASS:
		a1028_param = buf_bypass_tuning;
		size = sizeof(buf_bypass_tuning);
		break;
	default:
		a1028_param = NULL;
		size = 0;
		a1028->mode = A1028_INVALID;
		dev_err(a1028->dev, "mode %d invalid\n", mode);
		break;
	}

	if (a1028->mode != A1028_INVALID) {

#ifdef _CMD_FIFO_USED_
{
	int i = 0;

	ret = a1028_soc_execute_cmdmsg(a1028, A1028_MSG_SYNC);
	if (ret < 0) {
		dev_err(a1028->dev, "failed to a1028_i2c_write at %d line\n", __LINE__);
		return ret;
	}

	memset(ack_buf, 0, sizeof(ack_buf));
	while( i<size )
	{
		ret = a1028_i2c_write(a1028->client, (size-i)<A1028_CMD_FIFO_DEPTH?(size-i):A1028_CMD_FIFO_DEPTH, a1028_param+i);
		if (ret < 0) {
		    pr_err("A1028 CMD block write error!\n");
		    goto err_config;
		}

		msleep(20);
		ret = a1028_i2c_read(a1028->client, (size-i)<A1028_CMD_FIFO_DEPTH?(size-i):A1028_CMD_FIFO_DEPTH, ack_buf+i);
		if (ret < 0) {
		    pr_err("%s: CMD ACK block read error\n", __func__);
		    goto err_config;
		}

		i+=A1028_CMD_FIFO_DEPTH;
		msleep(20);
	}

#ifdef DEBUG
	for (i = 0; i < size; i++) {
		if ( !(i & 0x3)) printk("\n");
		printk("%.2X ", ack_buf[i]);
	}
	printk("\n");
#endif

	for (i=0; i<size; i+=4) {
		if (A1028_NORMAL_ACK != ack_buf[i]) {
			dev_err(a1028->dev, "failed to check ack\n");
			goto err_config;
		}
	}
}
#else
		ret = a1028_i2c_write(a1028->client, size, a1028_param);
		if (ret) {
			dev_err(a1028->dev, "failed to a1028_i2c_write at %d line\n", __LINE__);
			goto err_config;
		}

		if (a1028->mode != A1028_SUSPEND) {
			int i;
			ret = a1028_i2c_read(a1028->client, size, ack_buf);
			if (ret) {
				dev_err(a1028->dev, "failed to a1028_i2c_read at %d line\n", __LINE__);
				goto err_config;
			}
			for (i=0; i<size; i+=4) {
				if (A1028_NORMAL_ACK != ack_buf[i]) {
					dev_err(a1028->dev, "failed to check ack\n");
					goto err_config;
				}
			}
		}
#endif
	}
	return 0;

err_config:
	a1028_soc_softreset(a1028);
	return ret;
}

static int a1028_soc_setmode(struct a1028_soc *a1028, int mode)
{
	static struct a1028_soc *a1028_pre;
	int ret = 0;
	int retry = 5;

	pr_debug("A1028: set mode %d\n", mode);

	if (!a1028_pre && !a1028)
		return -EINVAL;

	if(a1028 && a1028_pre == NULL)
		a1028_pre = a1028;

	if( mode == A1028_LASTMODE)
	{
		pr_info("A1028: set to the last setting mode.\n");
		if (A1028_NORMAL == a1028_pre->status)
			return ret;
		mode = a1028_pre->mode;
	}

	mutex_lock(&a1028_pre->a1028_mutex);
	ret = a1028_soc_wakeup(a1028_pre);
	if(ret < 0)
	{
		dev_err(a1028_pre->dev, "failed to a1028_soc_wakeup, to cold reset on the device.\n");
		a1028_soc_coldreset(a1028_pre);
		ret = a1028_soc_wakeup(a1028_pre);
	}
	if (ret) {
		dev_err(a1028_pre->dev, "failed to a1028_soc_wakeup at %d line\n", __LINE__);
		goto err_mode;
	}

	do {
		ret = a1028_soc_config(a1028_pre, mode);
		if (!ret) break;
	} while(--retry>0);

	if (ret < 0)
		dev_err(a1028_pre->dev, "failed to set mode to %d \n", mode);
	else
		a1028_pre->mode = mode;

err_mode:
	mutex_unlock(&a1028_pre->a1028_mutex);
	return ret;
}

int (*a1028_setmode)(struct a1028_soc *, int) = NULL;

static void a1028_soc_firmware_handler(const struct firmware *fw,
        void *context)
{
	struct a1028_soc *a1028 = context;
	short cmds;
	int ret;

	/* Reset A1028 chip */
	gpio_set_value(a1028->gpio_reset, 0);
	mdelay(1);
	/* Take out of reset */
	gpio_set_value(a1028->gpio_reset, 1);

	mdelay(50);	//waitting a1028 reset done

	cmds = be16_to_cpu(A1028_MSG_BOOT);
	ret = a1028_i2c_write(a1028->client, sizeof(cmds), &cmds);
	if (ret) {
		dev_err(a1028->dev, "failed to a1028_i2c_write at %d line\n", __LINE__);
		goto error_fw;
	}

	ret = a1028_i2c_read(a1028->client, sizeof(cmds), &cmds);
	if (ret) {
		dev_err(a1028->dev, "failed to a1028_i2c_read at %d line\n", __LINE__);
		goto error_fw;
	}
	if (A1028_BOOT_ACK != cmds) {
		pr_err("%s: not a boot-mode ack, cmds = 0x%x\n", __func__, cmds);
		goto error_fw;
	}

#ifdef _CMD_FIFO_USED_
{
	cmds = RETRY_CNT;
	while(cmds--)
	{
		int remaining;
		const u8 *index;

		/* Download firmware to device*/
		remaining = fw->size / A1028_CMD_FIFO_DEPTH;
		index = fw->data;

		pr_info("A1028: starting to load fw ...\n");

		for (; remaining; remaining--, index += A1028_CMD_FIFO_DEPTH) {
			ret = a1028_i2c_write(a1028->client, A1028_CMD_FIFO_DEPTH, index);
			if (ret < 0)
				break;
		}

		if (ret >= 0 && fw->size % A1028_CMD_FIFO_DEPTH)
			ret = a1028_i2c_write(a1028->client, fw->size % A1028_CMD_FIFO_DEPTH, index);

		if (ret == 0)
		{
			msleep(20); /* Delay time before issue a Sync Cmd */
			ret = a1028_soc_execute_cmdmsg(a1028,A1028_MSG_SYNC);
			if (ret < 0) {
				dev_err(a1028->dev, "failed to a1028_i2c_write at %d line\n", __LINE__);
				continue;
			}

			break;
		}

		pr_err("%s: fw load error %d (%d retries left)\n",__func__, ret, cmds);
	}

	if (ret < 0)
		pr_err("A1028: failed to load fw \n");
	else
		pr_info("A1028: fw load successfully \n");

}
#else
	ret = a1028_i2c_write(a1028->client, fw->size, fw->data);
	if (ret) {
		dev_err(a1028->dev, "failed to a1028_i2c_write at %d line\n", __LINE__);
		goto error_fw;
	}
#endif
	msleep(20);	//waiting a1028 initialize done

	ret = a1028_soc_setmode(a1028, A1028_SUSPEND);
	if (!ret)
		a1028_setmode = a1028_soc_setmode;

	//ret = a1028_soc_sleep(a1028);
	//if (ret)
	//	dev_err(a1028->dev, "failed to a1028_soc_sleep at %d line\n", __LINE__);

error_fw:
	release_firmware(fw);
}

static int __devinit a1028_soc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct a1028_platform_data *pdata = client->dev.platform_data;
	struct a1028_soc *a1028;
	const char *fw_name;
	int ret;

	a1028 = kzalloc(sizeof(*a1028), GFP_KERNEL);
	if (!a1028)
		return -ENOMEM;

	a1028->dev = &client->dev;
	a1028->gpio_wake = pdata->gpio_wakeup;
	a1028->gpio_reset = pdata->gpio_reset;
	a1028->client = client;

	ret = gpio_request_one(a1028->gpio_wake, GPIOF_OUT_INIT_HIGH, "a1028_wake");
	ret = gpio_request_one(a1028->gpio_reset, GPIOF_OUT_INIT_HIGH, "a1028_rst");
	if(machine_is_m030())
	{
		a1028->crystal_regulator = regulator_get(a1028->dev, "audio_crystal");
		if(IS_ERR(a1028->crystal_regulator))
			pr_err("failed to get regulator [%s]\n", "audio_crystal");
		else
			regulator_enable(a1028->crystal_regulator);
	}
	mutex_init(&a1028->a1028_mutex);
	i2c_set_clientdata(client, a1028);

	if(machine_is_m030()) {
		fw_name = A1028_12M_SOC_FW;
	} else {
		fw_name = A1028_24M_SOC_FW;
	}
	ret = request_firmware_nowait(THIS_MODULE,
	        FW_ACTION_HOTPLUG,
	        fw_name,
	        &client->dev,
	        GFP_KERNEL | __GFP_ZERO,
	        a1028,
	        a1028_soc_firmware_handler);

	a1028_create_attrs(a1028->dev);

	pr_info("%s: A1028: load %s\n", dev_name(&client->dev), fw_name);

	return ret;
}

static int __devexit a1028_soc_remove(struct i2c_client *client)
{
	struct a1028_soc *a1028 = i2c_get_clientdata(client);

	mutex_destroy(&a1028->a1028_mutex);
	a1028_destroy_atts(a1028->dev);
	i2c_set_clientdata(client, NULL);
	gpio_free(a1028->gpio_reset);
	gpio_free(a1028->gpio_wake);
	kfree(a1028);
	return 0;
}

static const struct i2c_device_id a1028_soc_id[] = {
	{ "a1028_soc", 0 },
	{ }
};

static struct i2c_driver a1028_i2c_driver = {
	.probe = a1028_soc_probe,
	.remove = a1028_soc_remove,
	.driver = {
		.name = "a1028_soc",
	},
	.id_table = a1028_soc_id,
};

static int __init a1028_soc_init(void)
{
	return i2c_add_driver(&a1028_i2c_driver);
}
subsys_initcall(a1028_soc_init);

static void __exit a1028_soc_exit(void)
{
	i2c_del_driver(&a1028_i2c_driver);
}
module_exit(a1028_soc_exit);

MODULE_DESCRIPTION("A1028 voice processor driver");
MODULE_AUTHOR("lvcha qiu <lvcha@meizu.com>");
MODULE_LICENSE("GPLV2");
