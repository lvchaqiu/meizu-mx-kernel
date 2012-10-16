/* mv_net.c
 *
 * Base on msm-rmnet.c
 *
 * Virtual Ethernet Interface for Meizu MX phone Networking
 *
 * Copyright (C) 2011 Meizu Techonolgy, Inc.
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
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/wakelock.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "mx_smd.h"

/* XXX should come from smd headers */
#define SMD_PORT_ETHER0 11
#define POLL_DELAY 1000000 /* 1 second delay interval */
#define NET_CHANNELS	1

#define MX_NET_DEFAULT_MTU 1400

struct mxnet_private
{
	smd_channel_t *ch;
	struct net_device_stats stats;
	const char *chname;
	struct wake_lock wake_lock;
#ifdef CONFIG_MSM_RMNET_DEBUG
	ktime_t last_packet;
	short active_countdown; /* Number of times left to check */
	short restart_count; /* Number of polls seems so far */
	unsigned long wakeups_xmit;
	unsigned long wakeups_rcv;
	unsigned long timeout_us;
	unsigned long awake_time_ms;
	struct delayed_work work;
#endif
};

static int count_this_packet(struct sk_buff *skb)
{
//	struct iphdr *rx_ip_hdr=(struct iphdr *)_hdr;

	pr_debug("%s: skb->protocol=0x%x\n", __func__, ntohs(skb->protocol));
	if(skb->protocol != htons(ETH_P_IP))
		return 0;

	return 1;
}

#ifdef CONFIG_MSM_RMNET_DEBUG
static int in_suspend;
static unsigned long timeout_us;
static struct workqueue_struct *mxnet_wq;

static void do_check_active(struct work_struct *work)
{
	struct mxnet_private *p =
		container_of(work, struct mxnet_private, work.work);

	/*
	 * Soft timers do not wake the cpu from suspend.
	 * If we are in suspend, do_check_active is only called once at the
	 * timeout time instead of polling at POLL_DELAY interval. Otherwise the
	 * cpu will sleeps and the timer can fire much much later than POLL_DELAY
	 * casuing a skew in time calculations.
	 */
	if (in_suspend) {
		/*
		 * Assume for N packets sent durring this session, they are
		 * uniformly distributed durring the timeout window.
		 */
		int tmp = p->timeout_us * 2 -
			(p->timeout_us / (p->active_countdown + 1));
		tmp /= 1000;
		p->awake_time_ms += tmp;

		p->active_countdown = p->restart_count = 0;
		return;
	}

	/*
	 * Poll if not in suspend, since this gives more accurate tracking of
	 * rmnet sessions.
	 */
	p->restart_count++;
	if (--p->active_countdown == 0) {
		p->awake_time_ms += p->restart_count * POLL_DELAY / 1000;
		p->restart_count = 0;
	} else {
		queue_delayed_work(mxnet_wq, &p->work,
				usecs_to_jiffies(POLL_DELAY));
	}
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/*
 * If early suspend is enabled then we specify two timeout values,
 * screen on (default), and screen is off.
 */
static unsigned long timeout_suspend_us;
static struct device *rmnet0;

/* Set timeout in us when the screen is off. */
static ssize_t timeout_suspend_store(struct device *d, struct device_attribute *attr, const char *buf, size_t n)
{
	timeout_suspend_us = simple_strtoul(buf, NULL, 10);
	return n;
}

static ssize_t timeout_suspend_show(struct device *d,
				    struct device_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%lu\n", (unsigned long) timeout_suspend_us);
}

static DEVICE_ATTR(timeout_suspend, 0664, timeout_suspend_show, timeout_suspend_store);

static void mxnet_early_suspend(struct early_suspend *handler) {
	if (rmnet0) {
		struct mxnet_private *p = netdev_priv(to_net_dev(rmnet0));
		p->timeout_us = timeout_suspend_us;
	}
	in_suspend = 1;
}

static void mxnet_late_resume(struct early_suspend *handler) {
	if (rmnet0) {
		struct mxnet_private *p = netdev_priv(to_net_dev(rmnet0));
		p->timeout_us = timeout_us;
	}
	in_suspend = 0;
}

static struct early_suspend mxnet_power_suspend = {
	.suspend = mxnet_early_suspend,
	.resume = mxnet_late_resume,
};

static int __init mxnet_late_init(void)
{
	register_early_suspend(&mxnet_power_suspend);
	return 0;
}

late_initcall(mxnet_late_init);
#endif

/* Returns 1 if packet caused rmnet to wakeup, 0 otherwise. */
static int mxnet_cause_wakeup(struct mxnet_private *p) {
	int ret = 0;
	ktime_t now;
	if (p->timeout_us == 0) /* Check if disabled */
		return 0;

	/* Start timer on a wakeup packet */
	if (p->active_countdown == 0) {
		ret = 1;
		now = ktime_get_real();
		p->last_packet = now;
		if (in_suspend)
			queue_delayed_work(mxnet_wq, &p->work,
					usecs_to_jiffies(p->timeout_us));
		else
			queue_delayed_work(mxnet_wq, &p->work,
					usecs_to_jiffies(POLL_DELAY));
	}

	if (in_suspend)
		p->active_countdown++;
	else
		p->active_countdown = p->timeout_us / POLL_DELAY;

	return ret;
}

static ssize_t wakeups_xmit_show(struct device *d,
				 struct device_attribute *attr,
				 char *buf)
{
	struct mxnet_private *p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", p->wakeups_xmit);
}

DEVICE_ATTR(wakeups_xmit, 0444, wakeups_xmit_show, NULL);

static ssize_t wakeups_rcv_show(struct device *d, struct device_attribute *attr,
		char *buf)
{
	struct mxnet_private *p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", p->wakeups_rcv);
}

DEVICE_ATTR(wakeups_rcv, 0444, wakeups_rcv_show, NULL);

/* Set timeout in us. */
static ssize_t timeout_store(struct device *d, struct device_attribute *attr,
		const char *buf, size_t n)
{
#ifndef CONFIG_HAS_EARLYSUSPEND
	struct mxnet_private *p = netdev_priv(to_net_dev(d));
	p->timeout_us = timeout_us = simple_strtoul(buf, NULL, 10);
#else
/* If using early suspend/resume hooks do not write the value on store. */
	timeout_us = simple_strtoul(buf, NULL, 10);
#endif
	return n;
}

static ssize_t timeout_show(struct device *d, struct device_attribute *attr,
			    char *buf)
{
	struct mxnet_private *p = netdev_priv(to_net_dev(d));
	p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", timeout_us);
}

DEVICE_ATTR(timeout, 0664, timeout_show, timeout_store);

/* Show total radio awake time in ms */
static ssize_t awake_time_show(struct device *d, struct device_attribute *attr,
				char *buf)
{
	struct mxnet_private *p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", p->awake_time_ms);
}
DEVICE_ATTR(awake_time_ms, 0444, awake_time_show, NULL);

#endif
#if 0
/* Called in soft-irq context */
static void smd_net_data_handler(unsigned long arg)
{
	struct net_device *dev = (struct net_device *) arg;
	struct mxnet_private *p = netdev_priv(dev);
	struct sk_buff *skb;
	void *ptr = 0;
	int sz;
	int err;

	for (;;) {
		sz = smd_read_avail(p->ch);
		if(!sz)
			break;
		if (sz > MX_NET_DEFAULT_MTU) 
			sz = MX_NET_DEFAULT_MTU;
		skb = dev_alloc_skb(sz + NET_IP_ALIGN);
		if (skb == NULL) {
			pr_err("mxnet_recv() cannot allocate skb\n");
		} else {
			skb->dev = dev;
			skb_reserve(skb, NET_IP_ALIGN);
			ptr = skb_put(skb, sz);
			wake_lock_timeout(&p->wake_lock, HZ / 2);
			if (smd_read(p->ch, ptr, sz) != sz) {
				pr_err("mxnet_recv() smd lied about avail?!");
				ptr = 0;
				dev_kfree_skb_irq(skb);
			} else {
				skb->protocol = htons(ETH_P_IP);//eth_type_trans(skb, dev);
				err = netif_rx(skb);
				if (err != NET_RX_SUCCESS)
				{
					dev_err(&dev->dev, "rx error: %d\n", err);
				}else{
					/* update out statistics */
#ifdef CONFIG_MSM_RMNET_DEBUG
					p->wakeups_rcv += mxnet_cause_wakeup(p);
#endif				
					p->stats.rx_packets++;
					p->stats.rx_bytes += skb->len;
				}
			}
		}
		msleep(10);
	}
}
/* Called in soft-irq context */
#else
static void smd_net_data_handler(unsigned long arg)
{
	struct net_device *dev = (struct net_device *) arg;
	struct mxnet_private *p = netdev_priv(dev);
	struct sk_buff *skb;
	void *ptr = 0;
	int sz;

	for (;;) {
		sz = smd_cur_packet_size(p->ch);
		if (sz == 0) break;
		if (smd_read_avail(p->ch) < sz) break;
		
		if (sz > MX_NET_DEFAULT_MTU) 
		{
			ptr = 0;
			pr_err("rmnet_recv() discarding %d len\n", sz);
		}else{
			skb = dev_alloc_skb(sz);
			if (skb == NULL) {
				pr_err("mxnet_recv() cannot allocate skb\n");
			} else {
				skb->dev = dev;
				ptr = skb_put(skb, sz);
				wake_lock_timeout(&p->wake_lock, HZ / 2);
				if (smd_read(p->ch, ptr, sz) != sz) {
					pr_err("mxnet_recv() smd lied about avail?!");
					ptr = 0;
					dev_kfree_skb_irq(skb);
				} else {
					skb->protocol = htons(ETH_P_IP);//eth_type_trans(skb, dev);			
					if(count_this_packet(skb))
					{
						/* update out statistics */
#ifdef CONFIG_MSM_RMNET_DEBUG
						p->wakeups_rcv += mxnet_cause_wakeup(p);
#endif				
						p->stats.rx_packets++;
						p->stats.rx_bytes += skb->len;
					}
					skb_reset_mac_header(skb);
					netif_rx(skb);
					pr_debug("%s: mxnet_recv() size=%d", p->chname, skb->len);
				}
				continue;
			}
		}
		if (smd_read(p->ch, ptr, sz) != sz)
			pr_err("rmnet_recv() smd lied about avail?!");
	}
}
#endif
static DECLARE_TASKLET(smd_net_data_tasklet, smd_net_data_handler, 0);

static void mx_net_notify(void *_dev, unsigned event)
{
	if (event != SMD_EVENT_DATA)
	{	
		return;
	}

	smd_net_data_tasklet.data = (unsigned long) _dev;

	tasklet_schedule(&smd_net_data_tasklet);
}

static int mxnet_open(struct net_device *dev)
{
	int r;
	struct mxnet_private *p = netdev_priv(dev);

	pr_info("mxnet_open()\n");
	if (!p->ch) {
		r = smd_open(p->chname, &p->ch, dev, mx_net_notify);

		if (r < 0)
			return -ENODEV;
	}

	netif_start_queue(dev);
	return 0;
}

static int mxnet_stop(struct net_device *dev)
{
	//struct mxnet_private *p = netdev_priv(dev);
	
	pr_info("mxnet_stop()\n");
	netif_stop_queue(dev);

	return 0;
}

static int mxnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mxnet_private *p = netdev_priv(dev);
	smd_channel_t *ch = p->ch;

	if (smd_write_atomic(ch, skb->data, skb->len) != skb->len) {
		pr_err("rmnet fifo full, dropping packet\n");
	} else {
		if(count_this_packet(skb))
		{	
			p->stats.tx_packets++;
			p->stats.tx_bytes += skb->len;
#ifdef CONFIG_MSM_RMNET_DEBUG
			p->wakeups_xmit += mxnet_cause_wakeup(p);
#endif
		}
	}

	dev_kfree_skb_irq(skb);
	return 0;
}

static struct net_device_stats *mxnet_get_stats(struct net_device *dev)
{
	struct mxnet_private *p = netdev_priv(dev);
	return &p->stats;
}

static void mxnet_set_multicast_list(struct net_device *dev)
{
}

static void mxnet_tx_timeout(struct net_device *dev)
{
	pr_info("mxnet_tx_timeout()\n");
}

static struct net_device_ops mxnet_ops = {
	.ndo_open = mxnet_open,
	.ndo_stop = mxnet_stop,
	.ndo_start_xmit = mxnet_xmit,
	.ndo_get_stats = mxnet_get_stats,
	.ndo_set_multicast_list = mxnet_set_multicast_list,
	.ndo_tx_timeout = mxnet_tx_timeout,
};

static void __init mxnet_setup(struct net_device *dev)
{

	dev->watchdog_timeo = 20*HZ;
	dev->features		= 0;
	dev->netdev_ops		= &mxnet_ops,
	dev->type		= ARPHRD_NONE;
	dev->flags		= IFF_POINTOPOINT | IFF_NOARP;
	dev->mtu		= MX_NET_DEFAULT_MTU;
	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->tx_queue_len	= 1000;

	dev->destructor		= free_netdev;

#if 0
	dev->netdev_ops = &mxnet_ops;

	dev->watchdog_timeo = 20; /* ??? */

	ether_setup(dev);

	//dev->change_mtu = 0; /* ??? */

	random_ether_addr(dev->dev_addr);
#endif	
}


static const char *ch_name[NET_CHANNELS] = {
	"SMD_DATA0",
};

static int __init mxnet_init(void)
{
	int ret;
	struct device *d;
	struct net_device *dev;
	struct mxnet_private *p;
	unsigned n;

#ifdef CONFIG_MSM_RMNET_DEBUG
	timeout_us = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	timeout_suspend_us = 0;
#endif
#endif

#ifdef CONFIG_MSM_RMNET_DEBUG
	mxnet_wq = create_workqueue("rmnet");
#endif

	for (n = 0; n < NET_CHANNELS; n++) {
		dev = alloc_netdev(sizeof(struct mxnet_private),
				   "rmnet%d", mxnet_setup);

		if (!dev)
			return -ENOMEM;

		d = &(dev->dev);
		p = netdev_priv(dev);
		p->chname = ch_name[n];
		wake_lock_init(&p->wake_lock, WAKE_LOCK_SUSPEND, ch_name[n]);
#ifdef CONFIG_MSM_RMNET_DEBUG
		p->timeout_us = timeout_us;
		p->awake_time_ms = p->wakeups_xmit = p->wakeups_rcv = 0;
		p->active_countdown = p->restart_count = 0;
		INIT_DELAYED_WORK_DEFERRABLE(&p->work, do_check_active);
#endif

		ret = register_netdev(dev);
		if (ret) {
			wake_lock_destroy(&p->wake_lock);
			free_netdev(dev);
			return ret;
		}

#ifdef CONFIG_MSM_RMNET_DEBUG
		if (device_create_file(d, &dev_attr_timeout))
			continue;
		if (device_create_file(d, &dev_attr_wakeups_xmit))
			continue;
		if (device_create_file(d, &dev_attr_wakeups_rcv))
			continue;
		if (device_create_file(d, &dev_attr_awake_time_ms))
			continue;
#ifdef CONFIG_HAS_EARLYSUSPEND
		if (device_create_file(d, &dev_attr_timeout_suspend))
			continue;

		/* Only care about rmnet0 for suspend/resume tiemout hooks. */
		if (n == 0)
			rmnet0 = d;
#endif
#endif
		printk("%s: initial %s\n", __func__, p->chname);
	}
	return 0;
}

module_init(mxnet_init);

