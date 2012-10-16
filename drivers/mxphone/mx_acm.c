/*
 * mx_acm.c
 *
 * Copyright (c) 2011 WenbinWu	<wenbinwu@meizu.com>
 *
 * USB Abstract Control Model driver for USB modems and ISDN adapters
 *
 * base on cdc-acm driver
 *
 * ChangeLog:
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#undef DEBUG
#undef VERBOSE_DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <linux/list.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/ctype.h>
#include <mach/modem.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/input.h>
#include <mach/usb-detect.h>

#include "mx_modem.h"
#include "mx_acm.h"
#include "mx_smd.h"

#define ACM_CLOSE_TIMEOUT	15	/* seconds to let writes drain */

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.01"
#define DRIVER_AUTHOR "WenBin Wu"
#define DRIVER_DESC " Abstract Control Model driver for Intel USB HSIC modems"

static struct usb_driver acm_usb_driver;
//static struct tty_driver *acm_tty_driver;
static struct acm *acm_table[ACM_INTERFACES];
static struct acm_ch *acm_channel[ACM_INTERFACES];
struct acm_share *share_acm;

#define ACM_READY(acm)	(acm && acm->dev)
#define ACM_NO_MAIN	1
#define VERBOSE_DEBUG

#ifdef VERBOSE_DEBUG
#define verbose	1
#else
#define verbose	0
#endif


#ifdef CONFIG_HAS_WAKELOCK
enum {
	ACM_WLOCK_RUNTIME,
	ACM_WLOCK_DORMANCY,
} ACM_WLOCK_TYPE;

#define ACM_DEFAULT_WAKE_TIME (0.5*HZ)
#define ACM_SUSPEND_UNLOCK_DELAY (0.5*HZ)

static inline void acm_wake_lock_init(struct acm_share *acm)
{
	wake_lock_init(&acm->pm_lock, WAKE_LOCK_SUSPEND, "cdc-acm");
	wake_lock_init(&acm->dormancy_lock, WAKE_LOCK_SUSPEND, "acm-dormancy");
	acm->wake_time = ACM_DEFAULT_WAKE_TIME;
	acm->delay_time = ACM_SUSPEND_UNLOCK_DELAY;
}

static inline void acm_wake_lock_destroy(struct acm_share *acm)
{
	wake_lock_destroy(&acm->pm_lock);
	wake_lock_destroy(&acm->dormancy_lock);
}

static inline void _wake_lock(struct acm_share *acm, int type)
{
	switch (type) {
	case ACM_WLOCK_DORMANCY:
		wake_lock(&acm->dormancy_lock);
		break;
	case ACM_WLOCK_RUNTIME:
	default:
		wake_lock(&acm->pm_lock);
		break;
	}
}

static inline void _wake_unlock(struct acm_share *acm, int type)
{
	switch (type) {
	case ACM_WLOCK_DORMANCY:
		wake_unlock(&acm->dormancy_lock);
		break;
	case ACM_WLOCK_RUNTIME:
	default:
		wake_unlock(&acm->pm_lock);
		break;
	}
}

static inline void _wake_lock_timeout(struct acm_share *acm, int type)
{
	switch (type) {
	case ACM_WLOCK_DORMANCY:
		wake_lock_timeout(&acm->dormancy_lock, acm->wake_time);
		break;
	case ACM_WLOCK_RUNTIME:
	default:
		wake_lock_timeout(&acm->pm_lock, acm->delay_time);
	}
}

static inline void _wake_lock_settime(struct acm_share *acm, long time)
{
	if (acm)
		acm->delay_time = time;
}

static inline long _wake_lock_gettime(struct acm_share *acm)
{
	return acm ? acm->delay_time : ACM_SUSPEND_UNLOCK_DELAY;
}
#else
#define _wake_lock_init(acm) do { } while (0)
#define _wake_lock_destroy(acm) do { } while (0)
#define _wake_lock(acm, type) do { } while (0)
#define _wake_unlock(acm, type) do { } while (0)
#define _wake_lock_timeout(acm, type) do { } while (0)
#define _wake_lock_settime(acm, time) do { } while (0)
#define _wake_lock_gettime(acm) (0)
#define acm_wake_lock_init(acm) do { } while (0)
#define acm_wake_lock_destroy(acm) do { } while (0)
#endif

#define wake_lock_pm(acm)	_wake_lock(acm, ACM_WLOCK_RUNTIME)
#define wake_lock_data(acm)	_wake_lock(acm, ACM_WLOCK_DORMANCY)
#define wake_unlock_pm(acm)	_wake_unlock(acm, ACM_WLOCK_RUNTIME)
#define wake_unlock_data(acm)	_wake_unlock(acm, ACM_WLOCK_DORMANCY)
#define wake_lock_timeout_pm(acm) _wake_lock_timeout(acm, ACM_WLOCK_RUNTIME)
#define wake_lock_timeout_data(acm) _wake_lock_timeout(acm, ACM_WLOCK_DORMANCY)
#define wake_lock_pm_get_timeout(acm) _wake_lock_gettime(acm)
#define wake_lock_pm_set_timeout(acm, time) _wake_lock_settime(acm, , time)

static char* format_hex_string(const unsigned char *buf, int count) 
{
	/*define max count of chars to be print*/
#define MAXCHARS 1024
	/* CHARS_PER_LINE */
#define CPL 16
	const static char hexchar_table[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	/*a char=2hex+a space+a printable char+(MAXCHARS+CPL-1)/CPL '\n'+'\0'*/
	static char line[4 * MAXCHARS + (MAXCHARS + CPL - 1)/CPL + 1];
	int actcount = (count < MAXCHARS) ? count : MAXCHARS;
	int index = 0;
	int i, r;

	r = actcount % CPL;
	for (i = 0; i < actcount; i++) {
		index = i/CPL*CPL*4 + i/CPL + i%CPL*3;
		line[index + 0] = hexchar_table[buf[i] >> 4]; 
		line[index + 1] = hexchar_table[buf[i] & 0x0f]; 
		line[index + 2] = ' ';

		if (r && (i >= actcount-r))
			index = i/CPL*CPL*4 + i/CPL + r*3 + i%CPL;
		else
			index = i/CPL*CPL*4 + i/CPL + CPL*3 + i%CPL;

		line[index] = isprint(buf[i]) ?  buf[i]: '.' ;
		
		if (i % CPL == CPL - 1) 
			line[++index] = '\n';
	}
	
	line[++index] = 0;

	return line;
}

/**
 * acm_get_device - [GENERIC] Get acm for selected access
 * @param acm		acm device structure
 * @param new_state	the state which is requested
 *
 * Get the device and lock it for exclusive access
 */
static int acm_get_device(struct acm_share *acm, int new_state)
{
	DECLARE_WAITQUEUE(wait, current);

	/*
	 * Grab the lock and see if the device is available
	 */
	while (1) {
		spin_lock(&acm->acm_lock);
		if (acm->state == ACM_READY) {
			acm->state = new_state;
			spin_unlock(&acm->acm_lock);
			break;
		}
		if (new_state == ACM_SUSPEND) {
			spin_unlock(&acm->acm_lock);
			return -EAGAIN;
		}
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&acm->wq, &wait);
		spin_unlock(&acm->acm_lock);
		schedule();
		remove_wait_queue(&acm->wq, &wait);
	}

	return 0;
}

/**
 * acm_release_device - [GENERIC] release device
 * @param acm		acm device structure
 *
 * Deselect, release acm lock and wake up anyone waiting on the device
 */
static void acm_release_device(struct acm_share *acm)
{
	/* Release the cdc-acm */
	spin_lock(&acm->acm_lock);
	acm->state = ACM_READY;
	wake_up(&acm->wq);
	spin_unlock(&acm->acm_lock);
}

#ifdef CONFIG_PM_RUNTIME
int acm_request_resume(void)
{
	struct acm *acm=acm_table[0];
	struct device *dev;
	int err=0;
	int spin = 10;

	if (!ACM_READY(acm))
		return -ENODEV;

	dev = &acm->dev->dev;

	if (share_acm->dpm_suspending) {
		dev_info(dev,  "%s: suspending skip host wakeup\n",
			__func__);
		wake_lock_pm(share_acm);
		return -ENODEV;
	}

	usb_mark_last_busy(acm->dev);


	if (!dev->power.is_suspended) {
		wake_lock_pm(share_acm);
		dev_dbg(dev, "%s:run time resume\n", __func__);
retry:
		switch (dev->power.runtime_status) {
		case RPM_SUSPENDED:
			err = pm_runtime_resume(dev);
			if (!err && dev->power.timer_expires == 0
				&& dev->power.request_pending == false) {
				dev_dbg(dev, "%s:run time idle\n",  __func__);
				pm_runtime_idle(dev);
			}else if(err < 0){
				modem_crash_event(MODEM_EVENT_DISCONN);	
				wake_lock_pm(share_acm);
				return -ETIMEDOUT;
			}
			break;
		case RPM_SUSPENDING:
			if (spin--) {
				dev_err(dev, "usb suspending when resum spin=%d\n", spin);
				msleep(20);
				goto retry;
			}
			return -ETIMEDOUT;
		case RPM_RESUMING:
		case RPM_ACTIVE:
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(acm_request_resume);
int acm_initiated_resume(struct acm *acm)
{
	int err;
	struct usb_device *udev;

	if (!ACM_READY(acm))
	{
		return -ENODEV;
	}
	udev = acm->dev;
	if (udev) {
		struct device *dev = &udev->dev;
		int spin = 10, spin2 = 20;
		int host_wakeup_done = 0;
		int _host_high_cnt = 0, _host_timeout_cnt = 0;
retry:
		switch (dev->power.runtime_status) {
		case RPM_SUSPENDED:
			if (share_acm->dpm_suspending || host_wakeup_done) {
				dev_info(&udev->dev,
					"DPM Suspending, spin:%d\n", spin2);
				if (spin2-- == 0) {
					dev_err(&udev->dev,
					"dpm resume timeout\n");
					return -ETIMEDOUT;
				}
				msleep(50);
				goto retry;
			}
			err = modem_prepare_resume(500);
			switch (err) {
			case MC_SUCCESS:
				host_wakeup_done = 1;
				_host_timeout_cnt = 0;
				_host_high_cnt = 0;
				goto retry; /*wait until RPM_ACTIVE states*/

			case MC_HOST_TIMEOUT:
				_host_timeout_cnt++;
				break;
			case MC_HOST_HIGH:
				_host_high_cnt++;
				if (spin2-- == 0)//try to call acm_request_resume
				{
					if(!acm_request_resume())
					{
						host_wakeup_done = 1;
						_host_timeout_cnt = 0;
						_host_high_cnt = 0;
						spin2 = 20;
						goto retry; /*wait until RPM_ACTIVE states*/
					}
					dev_err(&udev->dev,	"Modem resume fail\n");
					return -ETIMEDOUT;
				}
				break;
			case MC_HOST_HALT:
			default:
				dev_err(&udev->dev,
					"ACM initiated resume, modem halted\n");
				return -EIO;
			}
			if (spin2-- == 0) {
				dev_info(&udev->dev,
				"ACM initiated resume, RPM_SUSPEND timeout\n");
				modem_crash_event(MODEM_EVENT_DISCONN);
				return -ETIMEDOUT;
			}
			msleep(20);
			goto retry;

		case RPM_SUSPENDING:
			dev_info(&udev->dev,
				"RPM Suspending, spin:%d\n", spin);
			if (spin-- == 0) {
				dev_err(&udev->dev,
				"Modem suspending timeout\n");
				return -ETIMEDOUT;
			}
			msleep(100);
			goto retry;
		case RPM_RESUMING:
			dev_info(&udev->dev,
				"RPM Resuming, spin:%d\n", spin2);
			if (spin2-- == 0) {
				dev_err(&udev->dev,
				"Modem resume timeout\n");
				return -ETIMEDOUT;
			}
			msleep(50);
			goto retry;
		case RPM_ACTIVE:
			dev_dbg(&udev->dev,
				"RPM Active, spin:%d\n", spin2);			
			break;
		default:
			dev_info(&udev->dev,
				"RPM EIO, spin:%d\n", spin2);
			return -EIO;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(acm_initiated_resume);
void acm_runtime_start(void)
{
	struct device *dev, *ppdev;
	struct acm *acm=acm_table[0];
	
	if(share_acm->usb_auto_suspend)
		return;
	if (!ACM_READY(acm))
		return;
	dev = &acm->dev->dev;
	if (acm->dev && dev->parent) {
		ppdev = dev->parent->parent;
		/*enable runtime feature - once after boot*/
		pm_runtime_allow(ppdev); /*ehci*/
		usb_enable_autosuspend(acm->dev);
		share_acm->usb_auto_suspend = 1;
		dev_info(dev, "ACM Runtime PM Start!!\n");
	}
}
EXPORT_SYMBOL_GPL(acm_runtime_start);
void acm_runtime_stop(void)
{
	struct device *dev, *ppdev;
	struct acm *acm=acm_table[0];

	if(!share_acm->usb_auto_suspend)
		return;
	if (!ACM_READY(acm))
		return;
	dev = &acm->dev->dev;
	if (acm->dev && dev->parent) {
		ppdev = dev->parent->parent;
		/*enable runtime feature - once after boot*/
		usb_disable_autosuspend(acm->dev);
		pm_runtime_forbid(ppdev); /*ehci*/
		share_acm->usb_auto_suspend = 0;
		dev_info(dev, "ACM Runtime PM Stop!!\n");
	}
}
EXPORT_SYMBOL_GPL(acm_runtime_stop);
#else
int acm_request_resume(void){return 0;}
EXPORT_SYMBOL_GPL(acm_request_resume);
int acm_initiated_resume(struct acm *acm) {return 0; }
EXPORT_SYMBOL_GPL(acm_initiated_resume);
void acm_runtime_start(void){return;}
EXPORT_SYMBOL_GPL(acm_runtime_start);
void acm_runtime_stop(void){return;}
EXPORT_SYMBOL_GPL(acm_runtime_stop);
#endif


/*check the acm interface driver status after resume*/
static void acm_post_resume_work(struct work_struct *work)
{
#ifdef CONFIG_PM_RUNTIME
	struct acm *acm=acm_table[0];
	struct device *dev;
	
	if (!modem_is_on() || !ACM_READY(acm))
	{
		share_acm->skip_hostwakeup = 0;
		share_acm->dpm_suspending = 0;
		acm_release_device(share_acm);
		return;
	}
	dev = &acm->dev->dev;

	if (modem_is_host_wakeup() && share_acm->skip_hostwakeup) 
	{
		dev_info(dev,
			"post resume host host gpio=%d, rpm_stat=%d",
			modem_is_host_wakeup(), dev->power.runtime_status);
		share_acm->skip_hostwakeup = 0;
		share_acm->dpm_suspending = 0;
		acm_request_resume();
		acm_runtime_stop();
	}else{
		share_acm->skip_hostwakeup = 0;
		share_acm->dpm_suspending = 0;
	}

	if(mx_is_usb_host_insert()) {
		if(acm_initiated_resume(acm) == 0)
			acm_runtime_stop();
	}else{
		acm_runtime_start();
	}
	acm_release_device(share_acm);	
#endif
}

/*
 * Write buffer management.
 * All of these assume proper locks taken by the caller.
 */

static int acm_wb_alloc(struct acm *acm)
{
	int i, wbn;
	struct acm_wb *wb;

	wbn = 0;
	i = 0;
	for (;;) {
		wb = &acm->wb[wbn];
		if (!wb->use) {
			wb->use = 1;
			return wbn;
		}
		wbn = (wbn + 1) % ACM_NW;
		if (++i >= ACM_NW)
			return -1;
	}
}

static int acm_wb_is_avail(struct acm *acm)
{
	int i, n;
	unsigned long flags;

	n = ACM_NW;
	spin_lock_irqsave(&acm->write_lock, flags);
	for (i = 0; i < ACM_NW; i++)
		n -= acm->wb[i].use;
	spin_unlock_irqrestore(&acm->write_lock, flags);

	return n;
}

/*
 * Finish write. Caller must hold acm->write_lock
 */
static void acm_write_done(struct acm *acm, struct acm_wb *wb)
{
	struct acm_ch *ch;

	ch = acm_channel[acm->cid];
	wb->use = 0;
	acm->transmitting--;
	if(ch && ch ->wr_cb)
		ch ->wr_cb(ch->priv);
	usb_autopm_put_interface_async(acm->control);
}

/*
 * Poke write.
 *
 * the caller is responsible for locking
 */

static int acm_start_wb(struct acm *acm, struct acm_wb *wb)
{
	int rc;

	acm->transmitting++;

	wb->urb->transfer_buffer = wb->buf;
	wb->urb->transfer_dma = wb->dmah;
	wb->urb->transfer_buffer_length = wb->len;
	wb->urb->dev = acm->dev;

	rc = usb_submit_urb(wb->urb, GFP_ATOMIC);
	if (rc < 0) {
		dbg("usb_submit_urb(write bulk) failed: %d", rc);
		acm_write_done(acm, wb);
	}
	usb_mark_last_busy(acm->dev);
	return rc;
}

static int acm_write_start(struct acm *acm, int wbn)
{
	unsigned long flags;
	struct acm_wb *wb = &acm->wb[wbn];
	int rc;

	if(acm_initiated_resume(acm))
		return -ETIMEDOUT;
	spin_lock_irqsave(&acm->write_lock, flags);
	if (!acm->dev) {
		wb->use = 0;
		spin_unlock_irqrestore(&acm->write_lock, flags);
		return -ENODEV;
	}

	dbg("%s susp_count: %d", __func__, acm->susp_count);
	usb_autopm_get_interface_async(acm->control);
	rc = acm_start_wb(acm, wb);
	spin_unlock_irqrestore(&acm->write_lock, flags);
	wake_lock_timeout_data(share_acm);
	return rc;

}
/*
 * attributes exported through sysfs
 */
static ssize_t show_caps
(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct acm *acm = usb_get_intfdata(intf);

	return sprintf(buf, "%d", acm->ctrl_caps);
}
static DEVICE_ATTR(bmCapabilities, S_IRUGO, show_caps, NULL);

static ssize_t store_atdebug(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct acm *acm = usb_get_intfdata(intf);
	long unsigned int atdebug;
	char atdebug_str[33];
	int ret = 0;

	ret = sscanf(buf, "%s", atdebug_str);
	if (ret != 1) {
		dev_err(dev, "Invalid!!Input string for atdebug_str\n");
		return -EINVAL;
	}
	ret = strict_strtoul(atdebug_str, 0, &atdebug);
	if (ret == 0)
		acm->atdebug = atdebug;

	return count;
}

static ssize_t show_atdebug
(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct acm *acm = usb_get_intfdata(intf);

	return sprintf(buf, "%d\n", acm->atdebug);
}
static DEVICE_ATTR(atdebug, S_IWUSR | S_IRUGO, show_atdebug, store_atdebug);

/* data interface returns incoming bytes, or we got unthrottled */
static void acm_read_bulk(struct urb *urb)
{
	struct acm_rb *buf;
	struct acm_ru *rcv = urb->context;
	struct acm *acm = rcv->instance;
	int status = urb->status;

	dev_dbg(&acm->data->dev,"Entering acm_read_bulk with status %d\n", status);

	if (!ACM_READY(acm)) {
		dev_dbg(&acm->data->dev, "Aborting, acm not ready");
		return;
	}
	usb_mark_last_busy(acm->dev);
	wake_lock_timeout_data(share_acm);
	if (status)
		dev_dbg(&acm->data->dev, "bulk rx status %d\n", status);

	buf = rcv->buffer;
	buf->size = urb->actual_length;

	if (likely(status == 0)) {
		spin_lock(&acm->read_lock);
		acm->processing++;
		list_add_tail(&rcv->list, &acm->spare_read_urbs);
		list_add_tail(&buf->list, &acm->filled_read_bufs);
		dev_dbg(&acm->data->dev,"add %p to filled_read_bufs \n", buf);
		spin_unlock(&acm->read_lock);
	} else {
		/* we drop the buffer due to an error */
		spin_lock(&acm->read_lock);
		list_add_tail(&rcv->list, &acm->spare_read_urbs);
		list_add(&buf->list, &acm->spare_read_bufs);
		spin_unlock(&acm->read_lock);
		/* nevertheless the tasklet must be kicked unconditionally
		so the queue cannot dry up */
	}
	if (likely(!acm->susp_count) && likely(share_acm->usb_connected)
			&& likely(acm->urb_task.func))
		tasklet_schedule(&acm->urb_task);
}

static void acm_rx_tasklet(unsigned long _acm)
{
	struct acm *acm = (void *)_acm;
	struct acm_ch *ch;
	struct acm_rb *buf;
	//struct tty_struct *tty;
	struct acm_ru *rcv;
	unsigned long flags;
	//unsigned char throttled;
	int free;
	int ret;
	
	dev_dbg(&acm->data->dev,"Entering acm_rx_tasklet\n");
	if (!ACM_READY(acm)) {
		printk("acm_rx_tasklet: ACM not ready\n");
		return;
	}
	ch = acm_channel[acm->cid];
	
next_buffer:
	spin_lock_irqsave(&acm->read_lock, flags);
	if (list_empty(&acm->filled_read_bufs)) {
		spin_unlock_irqrestore(&acm->read_lock, flags);
		goto urbs;
	}
	buf = list_entry(acm->filled_read_bufs.next,
			 struct acm_rb, list);
	list_del(&buf->list);
	spin_unlock_irqrestore(&acm->read_lock, flags);

	dev_dbg(&acm->data->dev,"acm_rx_tasklet: procesing buf 0x%p, size = %d\n", buf, buf->size);

	if (buf->size && ch->rx_cb) {
		if(acm->atdebug) {
			int max;

			dev_info(&acm->data->dev, "Receive Buffer:\n");
			max = acm->atdebug;
			if (buf->size < max)
				max = buf->size;
			printk("%s\n", format_hex_string(buf->base, max));
		}
		
		ret = ch->rx_cb(ch->priv, buf->base, buf->size, 0, &free);
		if (ret <=0) {
			spin_lock_irqsave(&acm->read_lock, flags);
			list_add(&buf->list, &acm->filled_read_bufs);
			spin_unlock_irqrestore(&acm->read_lock, flags);
			
			usb_mark_last_busy(acm->dev);
			dev_dbg(&acm->data->dev,"acm_rx_tasklet: rx_cb process fail insert buffer to head\n");
			goto urbs;
		}
	}

	spin_lock_irqsave(&acm->read_lock, flags);
	list_add(&buf->list, &acm->spare_read_bufs);
	spin_unlock_irqrestore(&acm->read_lock, flags);
	usb_mark_last_busy(acm->dev);
	goto next_buffer;

urbs:
	dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill urb\n");
	spin_lock_irqsave(&acm->read_lock, flags);
	while (!list_empty(&acm->spare_read_bufs)) {
		
		if (list_empty(&acm->spare_read_urbs)) {
			acm->processing = 0;
			spin_unlock_irqrestore(&acm->read_lock, flags);
			usb_mark_last_busy(acm->dev);
			return;
		}
		rcv = list_entry(acm->spare_read_urbs.next,
				 struct acm_ru, list);
		list_del(&rcv->list);

		buf = list_entry(acm->spare_read_bufs.next,
				 struct acm_rb, list);
		list_del(&buf->list);
		spin_unlock_irqrestore(&acm->read_lock, flags);
		
		rcv->buffer = buf;

		usb_fill_bulk_urb(rcv->urb, acm->dev,
					  acm->rx_endpoint,
					  buf->base,
					  acm->readsize,
					  acm_read_bulk, rcv);
		rcv->urb->transfer_dma = buf->dma;
		rcv->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill read buffer %p, acm->susp_count=%d\n", rcv->buffer, acm->susp_count);
		/* This shouldn't kill the driver as unsuccessful URBs are
		   returned to the free-urbs-pool and resubmited ASAP */
		spin_lock_irqsave(&acm->read_lock, flags);
  		if (acm->susp_count ||
				usb_submit_urb(rcv->urb, GFP_ATOMIC) < 0) {
			list_add(&buf->list, &acm->spare_read_bufs);
			list_add(&rcv->list, &acm->spare_read_urbs);
			acm->processing = 0;
			spin_unlock_irqrestore(&acm->read_lock, flags);
			dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill back urb and exit\n");
			usb_mark_last_busy(acm->dev);
			return;
		} 
	}
	acm->processing = 0;
	spin_unlock_irqrestore(&acm->read_lock, flags);
	usb_mark_last_busy(acm->dev);
	dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill urb exit\n");	
}
static void acm_kick(struct acm_ch *ch)
{
	struct acm *acm = acm_table[ch->cid];
	
	if (!ACM_READY(acm))
		return;
	if (likely(!acm->susp_count) && likely(acm->urb_task.func))
		tasklet_schedule(&acm->urb_task);
}

/* data interface wrote those outgoing bytes */
static void acm_write_bulk(struct urb *urb)
{
	struct acm_wb *wb = urb->context;
	struct acm *acm = wb->instance;
	unsigned long flags;

	if (verbose || urb->status
			|| (urb->actual_length != urb->transfer_buffer_length))
		dev_dbg(&acm->data->dev, "%s: tx %d/%d bytes -- > %d\n", __func__,
			urb->actual_length,
			urb->transfer_buffer_length,
			urb->status);

	spin_lock_irqsave(&acm->write_lock, flags);
	acm_write_done(acm, wb);
	spin_unlock_irqrestore(&acm->write_lock, flags);
	if (ACM_READY(acm))
		schedule_work(&acm->work);
	else
		wake_up_interruptible(&acm->drain_wait);
	usb_mark_last_busy(acm->dev);
}

static void acm_softint(struct work_struct *work)
{
	struct acm *acm = container_of(work, struct acm, work);

	dev_dbg(&acm->data->dev, "tx work\n");
	if (!ACM_READY(acm))
		return;
}


static void stop_data_traffic(struct acm *acm)
{
	int i;
	
	dev_dbg(&acm->data->dev, "Entering stop_data_traffic\n");

	local_irq_disable();
	tasklet_kill(&acm->urb_task);
	acm->urb_task.func = NULL;
	acm->urb_task.data = 0;
	local_irq_enable();

	for (i = 0; i < ACM_NW; i++)
		usb_kill_urb(acm->wb[i].urb);
	for (i = 0; i < acm->rx_buflimit; i++)
		usb_kill_urb(acm->ru[i].urb);
}

static int acm_open(struct acm_ch *ch)
{
	return 0;
}

static void acm_close(struct acm_ch *ch)
{

}

static int acm_write(struct acm_ch *ch,
					const unsigned char *buf, int count)
{
	struct acm *acm = acm_table[ch->cid];
	int stat;
	unsigned long flags;
	int wbn;
	struct acm_wb *wb;
	
	if (!count)
		return 0;
	dev_dbg(&acm->data->dev, "Entering acm_write to write %d bytes,\n", count);
	wake_lock_pm(share_acm);
	acm_get_device(share_acm , ACM_WRITE);
	if (!ACM_READY(acm))
	{
		acm_release_device(share_acm);
		return -EINVAL;
	}
	spin_lock_irqsave(&acm->write_lock, flags);
	wbn = acm_wb_alloc(acm);
	if (wbn < 0) {
		spin_unlock_irqrestore(&acm->write_lock, flags);
		acm_release_device(share_acm);
		return 0;
	}
	wb = &acm->wb[wbn];

	count = (count > acm->writesize) ? acm->writesize : count;
	memcpy(wb->buf, buf, count);
	if(acm->atdebug) {
		int max;

		dev_info(&acm->data->dev, "Send Buffer:\n");
		max = acm->atdebug;
		if (count < max)
			max = count;
		printk("%s\n", format_hex_string(buf, max));
	}
	wb->len = count;
	spin_unlock_irqrestore(&acm->write_lock, flags);

	stat = acm_write_start(acm, wbn);
	if (stat < 0)
	{
		acm_release_device(share_acm);
		return stat;
	}
	acm_release_device(share_acm);
	return count;

}

/*
 * USB probe and disconnect routines.
 */

/* Little helpers: write/read buffers free */
static void acm_write_buffers_free(struct acm *acm)
{
	int i;
	struct acm_wb *wb;
	struct usb_device *usb_dev = interface_to_usbdev(acm->control);

	for (wb = &acm->wb[0], i = 0; i < ACM_NW; i++, wb++)
		usb_free_coherent(usb_dev, acm->writesize, wb->buf, wb->dmah);
}

static void acm_read_buffers_free(struct acm *acm)
{
	struct usb_device *usb_dev = interface_to_usbdev(acm->control);
	int i, n = acm->rx_buflimit;

	for (i = 0; i < n; i++)
		usb_free_coherent(usb_dev, acm->readsize,
				  acm->rb[i].base, acm->rb[i].dma);
}

/* Little helper: write buffers allocate */
static int acm_write_buffers_alloc(struct acm *acm)
{
	int i;
	struct acm_wb *wb;

	for (wb = &acm->wb[0], i = 0; i < ACM_NW; i++, wb++) {
		wb->buf = usb_alloc_coherent(acm->dev, acm->writesize, GFP_KERNEL,
		    &wb->dmah);
		if (!wb->buf) {
			while (i != 0) {
				--i;
				--wb;
				usb_free_coherent(acm->dev, acm->writesize,
				    wb->buf, wb->dmah);
			}
			return -ENOMEM;
		}
	}
	return 0;
}

static int acm_probe(struct usb_interface *intf,
		     const struct usb_device_id *id)
{
	struct usb_cdc_union_desc *union_header = NULL;
	struct usb_cdc_call_mgmt_descriptor *cmd = NULL;
	unsigned char *buffer = intf->altsetting->extra;
	int buflen = intf->altsetting->extralen;
	struct usb_interface *control_interface;
	struct usb_interface *data_interface;
	struct usb_endpoint_descriptor *epctrl = NULL;
	struct usb_endpoint_descriptor *epread = NULL;
	struct usb_endpoint_descriptor *epwrite = NULL;
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct usb_device *root_usbdev= to_usb_device(intf->dev.parent->parent);
	struct acm *acm;
	int minor;
	int ctrlsize, readsize;
	u8 ac_management_function = 0;
	unsigned long quirks;
	int num_rx_buf;
	int i;
	int combined_interfaces = 0;
	const struct usb_host_interface *data_desc;
	const struct usb_host_interface *ctrl_desc;
	int dev_id;
	unsigned long flags;

	/* normal quirks */
	quirks = (unsigned long)id->driver_info;
	num_rx_buf = (quirks == SINGLE_RX_URB) ? 1 : ACM_NR;
	
	/* normal probing*/
	if (!buffer) {
		dev_err(&intf->dev, "Weird descriptor references\n");
		return -EINVAL;
	}
	if (!buflen) {
		if (intf->cur_altsetting->endpoint &&
				intf->cur_altsetting->endpoint->extralen &&
				intf->cur_altsetting->endpoint->extra) {
			dev_dbg(&intf->dev,
				"Seeking extra descriptors on endpoint\n");
			buflen = intf->cur_altsetting->endpoint->extralen;
			buffer = intf->cur_altsetting->endpoint->extra;
		} else {
			dev_err(&intf->dev,
				"Zero length descriptor references\n");
			return -EINVAL;
		}
	}

	while (buflen > 0) {
		if (buffer[1] != USB_DT_CS_INTERFACE) {
			dev_err(&intf->dev, "skipping garbage\n");
			goto next_desc;
		}
		switch (buffer[2]) {
		case USB_CDC_UNION_TYPE: /* we've found it */
			if (union_header) {
				dev_err(&intf->dev, "More than one "
					"union descriptor, skipping ...\n");
				goto next_desc;
			}
			union_header = (struct usb_cdc_union_desc *)buffer;
			dev_dbg(&intf->dev, "USB_CDC_UNION_TYPE bLength=0x%x\n", union_header->bLength);
			break;
		case USB_CDC_HEADER_TYPE: /* maybe check version */
			dev_dbg(&intf->dev, "USB_CDC_HEADER_TYPE\n");
			break; /* for now we ignore it */
		case USB_CDC_ACM_TYPE:
			ac_management_function = buffer[3];
			dev_dbg(&intf->dev, "USB_CDC_ACM_TYPE Capabilities=0x%x\n", ac_management_function);
			break;
		case USB_CDC_CALL_MANAGEMENT_TYPE:
			cmd = (struct usb_cdc_call_mgmt_descriptor *)buffer;
			dev_dbg(&intf->dev, "USB_CDC_CALL_MANAGEMENT_TYPE Capabilities=0x%x\n", cmd->bmCapabilities);
			break;
		default:
			/* there are LOTS more CDC descriptors that
			 * could legitimately be found here.
			 */
			dev_dbg(&intf->dev, "Ignoring descriptor: "
					"type %02x, length %d\n",
					buffer[2], buffer[0]);
			break;
		}
next_desc:
		buflen -= buffer[0];
		buffer += buffer[0];
	}
	if(!union_header)
		return -EINVAL;
	data_interface= usb_ifnum_to_if(usb_dev, union_header->bSlaveInterface0);
	control_interface = usb_ifnum_to_if(usb_dev, union_header->bMasterInterface0);
	if (!data_interface)
		return -ENODEV;
	if (!control_interface)
		return -ENODEV;
	data_desc = data_interface->altsetting;
	ctrl_desc = control_interface->altsetting;
	/* To detect usb device order probed */
	dev_id = intf->altsetting->desc.bInterfaceNumber / 2;
	dev_dbg(&root_usbdev->dev,  "%s: probe dev_id=%d, num_altsetting=%d\n", __func__, dev_id, intf->num_altsetting);
	epctrl = &ctrl_desc->endpoint[0].desc;
	/* Endpoints */
	if (usb_pipein(data_desc->endpoint[0].desc.bEndpointAddress)) {
		epread = &data_desc->endpoint[0].desc;
		epwrite = &data_desc->endpoint[1].desc;
		dev_dbg(&data_interface->dev,"%s: usb_pipein = 0\n", __func__);
	} else {
		epread = &data_desc->endpoint[1].desc;
		epwrite = &data_desc->endpoint[0].desc;
		dev_dbg(&data_interface->dev,"%s: usb_pipein = 1\n", __func__);
	}
	// LSI 
	dev_dbg(&data_interface->dev,"epread number : 0x%x, max read=%d ", epread->bEndpointAddress, epread->wMaxPacketSize);
	dev_dbg(&data_interface->dev,"eprwrite number : 0x%x, max write=%d\n", epwrite->bEndpointAddress, epwrite->wMaxPacketSize);
	
	for (minor = 0; minor < ACM_INTERFACES && acm_table[minor]; minor++);
	
	if (minor == ACM_INTERFACES) {
		dev_err(&intf->dev, "no more free acm devices\n");
		return -ENODEV;
	}

	acm = kzalloc(sizeof(struct acm), GFP_KERNEL);
	if (acm == NULL) {
		dev_dbg(&intf->dev, "out of memory (acm kzalloc)\n");
		goto alloc_fail;
	}

	ctrlsize = le16_to_cpu(epctrl->wMaxPacketSize);
	readsize = le16_to_cpu(epread->wMaxPacketSize) *
				(quirks == SINGLE_RX_URB ? 1 : 4);
	acm->combined_interfaces = combined_interfaces;
	acm->writesize = le16_to_cpu(epwrite->wMaxPacketSize*8) ;
	acm->control = control_interface;
	acm->data = data_interface;
	acm->minor = minor;
	acm->cid = minor;
	acm->dev = usb_dev;
	acm->ctrl_caps = ac_management_function;
	acm->atdebug = 0;

	if (quirks & NO_CAP_LINE)
		acm->ctrl_caps &= ~USB_CDC_CAP_LINE;
	acm->ctrlsize = ctrlsize;
	acm->readsize = readsize;
	acm->rx_buflimit = num_rx_buf;
	acm->urb_task.func = acm_rx_tasklet;
	acm->urb_task.data = (unsigned long) acm;
	INIT_WORK(&acm->work, acm_softint);
	init_waitqueue_head(&acm->drain_wait);
	spin_lock_init(&acm->throttle_lock);
	spin_lock_init(&acm->write_lock);
	spin_lock_init(&acm->read_lock);

	mutex_init(&acm->mutex);
	acm->rx_endpoint = usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress);
	acm->tx_endpoint = usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress);	

	if (acm_write_buffers_alloc(acm) < 0) {
		dev_dbg(&intf->dev, "out of memory (write buffer alloc)\n");
		goto alloc_fail4;
	}

	for (i = 0; i < num_rx_buf; i++) {
		struct acm_ru *rcv = &(acm->ru[i]);

		rcv->urb = usb_alloc_urb(0, GFP_KERNEL);
		if (rcv->urb == NULL) {
			dev_dbg(&intf->dev,
				"out of memory (read urbs usb_alloc_urb)\n");
			goto alloc_fail6;
		}

		rcv->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		rcv->instance = acm;
	}
	for (i = 0; i < num_rx_buf; i++) {
		struct acm_rb *rb = &(acm->rb[i]);

		rb->base = usb_alloc_coherent(acm->dev, readsize,
				GFP_KERNEL, &rb->dma);
		if (!rb->base) {
			dev_dbg(&intf->dev,
				"out of memory (read bufs usb_alloc_coherent)\n");
			goto alloc_fail7;
		}
	}
	for (i = 0; i < ACM_NW; i++) {
		struct acm_wb *snd = &(acm->wb[i]);

		snd->urb = usb_alloc_urb(0, GFP_KERNEL);
		if (snd->urb == NULL) {
			dev_dbg(&intf->dev,
				"out of memory (write urbs usb_alloc_urb)");
			goto alloc_fail8;
		}

		usb_fill_bulk_urb(snd->urb, usb_dev,acm->tx_endpoint,
			NULL, acm->writesize, acm_write_bulk, snd);
		snd->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		snd->instance = acm;
	}

	usb_set_intfdata(intf, acm);

	i = device_create_file(&intf->dev, &dev_attr_bmCapabilities);
	if (i < 0)
		goto alloc_fail8;
	
	i = device_create_file(&intf->dev, &dev_attr_atdebug);
	if (i < 0)
		goto alloc_fail8;

	usb_driver_claim_interface(&acm_usb_driver, data_interface, acm);
	usb_set_intfdata(control_interface, acm);

	acm->control->needs_remote_wakeup = 0;
#ifdef CONFIG_PM
	pm_runtime_set_autosuspend_delay(&usb_dev->dev, 200);/* 200ms */
	pm_runtime_set_autosuspend_delay(&root_usbdev->dev, 200);/* 200ms */
	usb_enable_autosuspend(acm->dev);
#endif
	acm_table[minor] = acm;
	
	spin_lock_irqsave(&acm->read_lock, flags);
	INIT_LIST_HEAD(&acm->spare_read_urbs);
	INIT_LIST_HEAD(&acm->spare_read_bufs);
	INIT_LIST_HEAD(&acm->filled_read_bufs);
	
	for (i = 0; i < acm->rx_buflimit; i++)
		list_add(&(acm->ru[i].list), &acm->spare_read_urbs);
	for (i = 0; i < acm->rx_buflimit; i++)
		list_add(&(acm->rb[i].list), &acm->spare_read_bufs);
	spin_unlock_irqrestore(&acm->read_lock, flags);
	
	tasklet_schedule(&acm->urb_task);
	
	if(minor==ACM_INTERFACES-1)
	{
		share_acm->usb_connected = 1;
		acm_get_device(share_acm, ACM_CONNECT);
		if(!mx_is_usb_host_insert())
		{
			acm_runtime_start();
		}else{
			acm_runtime_stop();
		}
		wake_lock_pm(share_acm);
		acm_release_device(share_acm);	
		modem_crash_event(MODEM_EVENT_CONN);
	}
	
	dev_info(&intf->dev,"%s: ACM %d connect!\n",__func__, minor);
	return 0;
alloc_fail8:
	for (i = 0; i < ACM_NW; i++)
		usb_free_urb(acm->wb[i].urb);
alloc_fail7:
	acm_read_buffers_free(acm);
alloc_fail6:
	for (i = 0; i < num_rx_buf; i++)
		usb_free_urb(acm->ru[i].urb);
	acm_write_buffers_free(acm);
alloc_fail4:
	kfree(acm);
alloc_fail:
	return -ENOMEM;
}

static void acm_disconnect(struct usb_interface *intf)
{
	struct acm *acm = usb_get_intfdata(intf);
	int minor=0;
	int i;
	
	/* sibling interface is already cleaning up */
	if (!acm)
		return;
	minor = acm->minor;
	printk("%s: ACM %d disconnect start!\n",__func__, minor);	
	acm_get_device(share_acm, ACM_CONNECT);
	acm_table[minor] = NULL;	
	share_acm->usb_connected = 0;
	share_acm->resume_debug = 0;
	share_acm->dpm_suspending = 0;
	share_acm->usb_auto_suspend = 0;
	share_acm->skip_hostwakeup = 0;	
	acm_release_device(share_acm);	
	
	stop_data_traffic(acm);
	device_remove_file(&acm->control->dev, &dev_attr_bmCapabilities);
	device_remove_file(&acm->control->dev, &dev_attr_atdebug);
	acm->dev = NULL;
	usb_set_intfdata(acm->control, NULL);
	usb_set_intfdata(acm->data, NULL);

	acm_write_buffers_free(acm);
	acm_read_buffers_free(acm);
	
	for (i = 0; i < ACM_NW; i++)
		usb_free_urb(acm->wb[i].urb);
	for (i = 0; i < acm->rx_buflimit; i++)
		usb_free_urb(acm->ru[i].urb);
	
	if (!acm->combined_interfaces)
		usb_driver_release_interface(&acm_usb_driver, intf == acm->control ?
					acm->data : acm->control);
	kfree(acm);
	modem_crash_event(MODEM_EVENT_DISCONN);	
	wake_lock_pm(share_acm);
	printk("%s: ACM %d disconnect end!\n",__func__, minor);
}

static int acm_is_ready(struct acm_ch *ch)
{
	struct acm *acm = acm_table[ch->cid];

	return ACM_READY(acm);
}
static int acm_get_write_frame(struct acm_ch *ch)
{
	struct acm *acm;
	int size = 0;
	
	acm_get_device(share_acm, ACM_WRITE);
	acm = acm_table[ch->cid];
	if(ACM_READY(acm))
	{
		size = acm->writesize ? acm->writesize : 512;
		acm_release_device(share_acm);
		return size;
	}
	acm_release_device(share_acm);
	
	return 512;
}
static int acm_get_write_room(struct acm_ch *ch)
{
	struct acm *acm;
	int size = 0;
	
	acm_get_device(share_acm, ACM_WRITE);
	acm = acm_table[ch->cid];
	if (!ACM_READY(acm))
	{
		acm_release_device(share_acm);
		return -EINVAL;
	}
	size = acm_wb_is_avail(acm);
	size = size?acm->writesize : 0;
	acm_release_device(share_acm);
	
	return size;
}
#ifdef CONFIG_PM
static int acm_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct acm *acm = usb_get_intfdata(intf);
	int cnt;

	
	dev_dbg(&intf->dev, "%s: interface number%d\n", __func__,intf->altsetting->desc.bInterfaceNumber);
	if (message.event & PM_EVENT_AUTO) {
		int b;

		spin_lock_irq(&acm->read_lock);
		spin_lock(&acm->write_lock);
		b = acm->processing + acm->transmitting;
		spin_unlock(&acm->write_lock);
		spin_unlock_irq(&acm->read_lock);
		if (b)
			return -EBUSY;
	}
	wake_lock_timeout_pm(share_acm);	
	spin_lock_irq(&acm->read_lock);
	spin_lock(&acm->write_lock);
	cnt = acm->susp_count++;
	spin_unlock(&acm->write_lock);
	spin_unlock_irq(&acm->read_lock);

	if (cnt)
		return 0;
	/*
	we treat opened interfaces differently,
	we must guard against open
	*/
	mutex_lock(&acm->mutex);
	stop_data_traffic(acm);
	mutex_unlock(&acm->mutex);
	return 0;
}

static int acm_resume(struct usb_interface *intf)
{
	struct acm *acm = usb_get_intfdata(intf);
	int rv = 0;
	int cnt;

	dev_dbg(&intf->dev, "%s\n", __func__);

	wake_lock_pm(share_acm);
	spin_lock_irq(&acm->read_lock);
	acm->susp_count -= 1;
	cnt = acm->susp_count;
	spin_unlock_irq(&acm->read_lock);

	if (cnt)
		return 0;

	mutex_lock(&acm->mutex);
	/*
	 * delayed error checking because we must
	 * do the write path at all cost
	 */
	if (rv < 0)
		goto err_out;
	
	acm->urb_task.data = (unsigned long) acm;
	acm->urb_task.func = acm_rx_tasklet;
	tasklet_schedule(&acm->urb_task);

err_out:
	mutex_unlock(&acm->mutex);
	return rv;
}

static int acm_reset_resume(struct usb_interface *intf)
{
	struct acm *acm = usb_get_intfdata(intf);

	dev_dbg(&intf->dev, "%s\n", __func__);
	mutex_lock(&acm->mutex);

	mutex_unlock(&acm->mutex);
	return acm_resume(intf);
}

#endif /* CONFIG_PM */

#define INTEL_BOOTROM_ACM_INFO(x) \
		USB_DEVICE_AND_INTERFACE_INFO(0x058b, x, \
		USB_CLASS_COMM, USB_CDC_SUBCLASS_ACM, \
		USB_CDC_PROTO_NONE)
		
#define INTEL_PSI_ACM_INFO(x) \
		USB_DEVICE_AND_INTERFACE_INFO(0x058b, x, \
		USB_CLASS_COMM, USB_CDC_SUBCLASS_ACM, \
		USB_CDC_PROTO_NONE)
		
#define INTEL_MAIN_ACM_INFO(x) \
		USB_DEVICE_AND_INTERFACE_INFO(0x01519, x, \
		USB_CLASS_COMM, USB_CDC_SUBCLASS_ACM, \
		USB_CDC_ACM_PROTO_AT_V25TER)		
/*
 * USB driver structure.
 */

static const struct usb_device_id acm_ids[] = {
#if 0
	/* quirky and broken devices */
	{ INTEL_BOOTROM_ACM_INFO(0x0041),
	  .driver_info = ACM_NO_MAIN,
	},/*intel acm bootrom hsic device*/
	{ INTEL_PSI_ACM_INFO(0x0015), 
	  .driver_info = ACM_NO_MAIN,

	 },
#endif	 
	 /* intel ACM PSI hsic device*/
	{ INTEL_MAIN_ACM_INFO(0x0020),},/*intel acm main hsic device*/
	{ }

};

MODULE_DEVICE_TABLE(usb, acm_ids);

static struct usb_driver acm_usb_driver = {
	.name =		"cdc_acm",
	.probe =	acm_probe,
	.disconnect =	acm_disconnect,
#ifdef CONFIG_PM
	.suspend =	acm_suspend,
	.resume =	acm_resume,
	.reset_resume =	acm_reset_resume,
#endif
	.id_table =	acm_ids,
#ifdef CONFIG_PM
	.supports_autosuspend = 1,
#endif
};

/*
 * Init / exit.
 */
static int acm_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		pr_debug("%s: PM_SUSPEND_PREPARE\n", __func__);
		if(acm_get_device(share_acm, ACM_SUSPEND))
			return NOTIFY_BAD;
		share_acm->dpm_suspending = 1;
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		if (share_acm->usb_connected)
		{
			if (modem_is_host_wakeup()) {
				wake_lock_pm(share_acm);
				share_acm->skip_hostwakeup = 1;
			}
			queue_work(share_acm->post_resume_wq, &share_acm->post_resume_work);
		}else{
			share_acm->dpm_suspending = 0;
			acm_release_device(share_acm);	
		}
		pr_debug("%s: PM_POST_SUSPEND\n", __func__);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block acm_pm_notifier = {
	.notifier_call = acm_notifier_event,
};
static int acm_usb_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int rtn = NOTIFY_DONE;

	if(share_acm->usb_connected && !share_acm->dpm_suspending) {
		switch (event) {
		case USB_HOST_INSERT:
		case USB_DOCK_INSERT:
			acm_get_device(share_acm, ACM_RESUME);	
			if(!acm_initiated_resume(acm_table[0]))
				acm_runtime_stop();
			acm_release_device(share_acm);	
			rtn = NOTIFY_OK;
			break;
		case USB_HOST_REMOVE:
		case USB_DOCK_REMOVE:
			acm_get_device(share_acm, ACM_RESUME);	
			acm_runtime_start();
			acm_release_device(share_acm);	
			rtn = NOTIFY_OK;
			break;
		default:
			break;
		}
	}
	return rtn;
}
static struct notifier_block acm_usb_notifier = {
	.notifier_call = acm_usb_notifier_event,
};
struct acm_ch *acm_register(unsigned int cid, void (**low_notify)(struct acm_ch *),  int (*rxcb)(void*, char *,int, int, int *), void(*wrcb)(void*), void *priv )
{
	if(cid < ACM_INTERFACES)
	{
		*low_notify = acm_kick;
		acm_channel[cid]->rx_cb = rxcb;
		acm_channel[cid]->wr_cb = wrcb;
		acm_channel[cid]->cid = cid;
		acm_channel[cid]->priv = priv;
		acm_channel[cid]->open = acm_open;
		acm_channel[cid]->close = acm_close;
		acm_channel[cid]->write = acm_write;
		acm_channel[cid]->is_ready = acm_is_ready;
		acm_channel[cid]->get_write_frame = acm_get_write_frame;
		acm_channel[cid]->get_write_room = acm_get_write_room;
		return acm_channel[cid];
	}
	return NULL;
}
int __init acm_init(void)
{
	int i;
	int retval;

	for(i=0; i<ACM_INTERFACES;i++)
	{
		acm_channel[i] = kzalloc(sizeof(struct acm_ch), GFP_KERNEL);
		acm_table[i] = NULL;
	}
	share_acm = kzalloc(sizeof(struct acm_share), GFP_KERNEL);
	
	acm_wake_lock_init(share_acm);
	share_acm->resume_debug = 0;
	share_acm->dpm_suspending = 0;
	share_acm->skip_hostwakeup = 0;
	share_acm->usb_connected = 0;
	share_acm->usb_auto_suspend = 0;
	share_acm->state = ACM_READY;
	init_waitqueue_head(&share_acm->wq);
	for(i=0; i<ACM_INTERFACES; i++)
	{	
		share_acm->ch_state[i] = ACM_READY;
		init_waitqueue_head(&share_acm->ch_wq[i]);
		spin_lock_init(&share_acm->ch_lock[i]);
	}
	spin_lock_init(&share_acm->acm_lock);
	share_acm->post_resume_wq = create_singlethread_workqueue("acm_post_resume");
	INIT_WORK(&share_acm->post_resume_work, acm_post_resume_work);
	
	retval = usb_register(&acm_usb_driver);
	if (retval) {
		acm_wake_lock_destroy(share_acm);
		return retval;
	}
	
	register_pm_notifier(&acm_pm_notifier);
	register_mx_usb_notifier(&acm_usb_notifier);
	
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");

	return 0;
}

void __exit acm_exit(void)
{
	int i;
	
	usb_deregister(&acm_usb_driver);
	unregister_mx_usb_notifier(&acm_usb_notifier);
	unregister_pm_notifier(&acm_pm_notifier);
	for(i=0; i<ACM_INTERFACES;i++)
	{
		kfree(acm_channel[i]);
	}
	wake_unlock_pm(share_acm);
	acm_wake_lock_destroy(share_acm);	
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


