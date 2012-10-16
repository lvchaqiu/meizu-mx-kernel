/* mx_tty.c
 *
 *  Base on smd-tty.c
 *
 * Copyright (C) 2011 MeiZu Techonology, Inc.
 * Author: WenBin Wu <wenbinwu@meizu.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include "mx_smd.h"
#include "mx_tty.h"
#define MAX_SMD_TTYS 4
#define TTY_PACKET_SIZE 4096

static DEFINE_MUTEX(mx_tty_lock);

struct mx_tty_info {
	smd_channel_t *ch;
	struct tty_struct *tty;
	struct wake_lock wake_lock;
	int open_count;
	spinlock_t read_lock;
	spinlock_t write_lock;
	unsigned char buffer[TTY_PACKET_SIZE];
};

static struct mx_tty_info mx_tty[MAX_SMD_TTYS];

static const struct smd_tty_channel_desc mx_default_tty_channels[] = {
	{ .id = 0, .name = "SMD_TTY0" },
	{ .id = 1, .name = "SMD_TTY1" },
	{ .id = 2, .name = "SMD_TTY2" },
	{ .id = 3, .name = "SMD_TTY3" },
};

static const struct smd_tty_channel_desc *mx_tty_channels =
		mx_default_tty_channels;
static int mx_tty_channels_len = ARRAY_SIZE(mx_default_tty_channels);

int mx_set_channel_list(const struct smd_tty_channel_desc *channels, int len)
{
	mx_tty_channels = channels;
	mx_tty_channels_len = len;
	return 0;
}

static void mx_tty_notify(void *priv, unsigned event)
{
	unsigned char *ptr;
	int avail;
	struct mx_tty_info *info = priv;
	struct tty_struct *tty = info->tty;
	
	if (!tty)
		return;

	if (event != SMD_EVENT_DATA)
	{	
		return;
	}

	for (;;) {
		if (test_bit(TTY_THROTTLED, &tty->flags))
		{
			break;
		}
		avail = smd_read_avail(info->ch);
		if (avail == 0)
			break;

		if(avail>TTY_PACKET_SIZE)
			avail = TTY_PACKET_SIZE;
		avail = tty_prepare_flip_string(tty, &ptr, avail);

		if(avail == 0)
		{
			printk(KERN_ERR "%s: tty buffer avail size=%d\n", __func__, avail);
			break;
		}
	
		if (smd_read(info->ch, ptr, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			printk(KERN_ERR "OOPS - mx_tty_buffer mismatch?!");
		}

		wake_lock_timeout(&info->wake_lock, HZ * 0.25);
		tty_flip_buffer_push(tty);
	}

	/* XXX only when writable and necessary */
//	tty_wakeup(tty);
}

static int mx_tty_open(struct tty_struct *tty, struct file *f)
{
	int res = 0;
	int n = tty->index;
	struct mx_tty_info *info;
	const char *name = NULL;
	int i;

	for (i = 0; i < mx_tty_channels_len; i++) {
		if (mx_tty_channels[i].id == n) {
			name = mx_tty_channels[i].name;
			break;
		}
	}
	if (!name)
		return -ENODEV;

	info = mx_tty + n;

	mutex_lock(&mx_tty_lock);
	tty->driver_data = info;

	if (info->open_count++ == 0) {
		wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND, name);
		info->tty = tty;
		tty->low_latency = 1;//set to low latency tty
		if (info->ch) {
			smd_kick(info->ch);
		} else {
			res = smd_open(name, &info->ch, info, mx_tty_notify);
		}
	}
	mutex_unlock(&mx_tty_lock);
	return res;
}

static void mx_tty_close(struct tty_struct *tty, struct file *f)
{
	struct mx_tty_info *info = tty->driver_data;

	if (info == 0)
		return;

	mutex_lock(&mx_tty_lock);
	if (--info->open_count == 0) {
		info->tty = NULL;
		tty->driver_data = NULL;
		if (info->ch) {
			smd_close(info->ch);
			info->ch = NULL;
		}
		wake_lock_destroy(&info->wake_lock);
	}
	mutex_unlock(&mx_tty_lock);
}

static int mx_tty_write(struct tty_struct *tty, const unsigned char *buf, int len)
{
	struct mx_tty_info *info = tty->driver_data;
	unsigned long flags;
	int ret = 0;

	if (!info)
		return -EINVAL;
	if (!len)
		return 0;

	/* if we're writing to a packet channel we will
	** never be able to write more data than there
	** is currently space for
	*/
	spin_lock_irqsave(&info->write_lock, flags);
	ret = smd_write_avail(info->ch);
	if (len > ret)
		len = ret;
	ret = smd_write(info->ch, buf, len);
	spin_unlock_irqrestore(&info->write_lock, flags);

	return ret;
}

static int mx_tty_write_room(struct tty_struct *tty)
{
	struct mx_tty_info *info = tty->driver_data;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&info->write_lock, flags);
	if (info != NULL)
		ret = smd_write_avail(info->ch);
	spin_unlock_irqrestore(&info->write_lock, flags);
	
	return ret;
}

/*
static int mx_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct mx_tty_info *info = tty->driver_data;
	return smd_read_avail(info->ch);
}
*/

static void mx_tty_unthrottle(struct tty_struct *tty)
{
	struct mx_tty_info *info = tty->driver_data;

	if (!info)
		return;

	spin_lock_irq(&info->read_lock);
	smd_kick(info->ch);
	spin_unlock_irq(&info->read_lock);
}
static void mx_tty_throttle(struct tty_struct *tty)
{
	//struct mx_tty_info *info = tty->driver_data;
	//printk("%s:  TTY_THROTTLED Set\n", __func__);
}

static struct tty_operations mx_tty_ops = {
	.open = mx_tty_open,
	.close = mx_tty_close,
	.write = mx_tty_write,
	.write_room = mx_tty_write_room,
//	.chars_in_buffer = mx_tty_chars_in_buffer,
	.unthrottle = mx_tty_unthrottle,
	.throttle =mx_tty_throttle,
};

static struct tty_driver *mx_tty_driver;

static int __init mx_tty_init(void)
{
	int ret, i;

	mx_tty_driver = alloc_tty_driver(MAX_SMD_TTYS);
	if (mx_tty_driver == 0)
		return -ENOMEM;

	mx_tty_driver->owner = THIS_MODULE;
	mx_tty_driver->driver_name = "mx_tty_driver";
	mx_tty_driver->name = "ttyACM";
	mx_tty_driver->major = 0;
	mx_tty_driver->minor_start = 0;
	mx_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	mx_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	mx_tty_driver->init_termios = tty_std_termios;
	mx_tty_driver->init_termios.c_iflag = 0;
	mx_tty_driver->init_termios.c_oflag = 0;
	mx_tty_driver->init_termios.c_cflag = B4000000 | CS8 | CREAD;
	mx_tty_driver->init_termios.c_lflag = 0;
	mx_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS |
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(mx_tty_driver, &mx_tty_ops);

	ret = tty_register_driver(mx_tty_driver);
	if (ret) return ret;

	for (i = 0; i < mx_tty_channels_len; i++) {
		tty_register_device(mx_tty_driver, mx_tty_channels[i].id, 0);
		printk("%s: %s\n", __func__, mx_tty_channels[i].name);
	}
	for (i = 0; i < MAX_SMD_TTYS; i++) {
		spin_lock_init(&mx_tty[i].write_lock);
		spin_lock_init(&mx_tty[i].read_lock);
	}
	return 0;
}

module_init(mx_tty_init);

