/* mx_smd.c
 *
 * SMD interface emulation
 *
 * Based on original version arch/arm/mach-msm/smd.c
 *
 * Adaptation to IFX XMM IPC
 * Copyright (C) 2011 MeiZu Techonology,INC
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/inet.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include "mx_smd.h"

extern struct acm_ch *acm_register(unsigned int cid, void (**low_notify)(struct acm_ch *),  int (*rxcb)(void*, char *,int, int, int *), void (*wrcb)(void*), void *priv );
static int smd_rx_data_cb (void*, char *rx_addr, int rx_len, int rx_flags, int* free);
void smd_low_layer_notification(struct smd_channel *ch);
static void smd_rx_work(struct work_struct *work);
static void smd_send_work(struct work_struct *work);

static inline void notify_modem_smd(struct smd_channel *ch)
{
	if(!ch->notify_low_layer)
	{
		pr_err("SMD: notify_modem_smd() missing callback\n");
		return;
	}
	ch->notify_low_layer(ch->xmmch);
}

static inline void notify_dsp_smd(struct smd_channel *ch)
{

}


#define SMD_CHANNELS (ARRAY_SIZE(smd_shared))


/* the spinlock is used to synchronize between the
 * irq handler and code that mutates the channel
 * list or fiddles with channel state
 */
DEFINE_SPINLOCK(smd_lock);
// DEFINE_SPINLOCK(smem_lock);

/* the mutex is used during open() and close()
 * operations to avoid races while creating or
 * destroying smd_channel structures
 */
static DEFINE_MUTEX(smd_creation_mutex);

static int smd_initialized;

LIST_HEAD(smd_ch_closed_list);
LIST_HEAD(smd_ch_list_modem);
LIST_HEAD(smd_ch_list_dsp);

static struct smd_alloc_elm smd_shared[] = {
	// char name [20], uint32_t cid, uint32_t ctype , uint32_t ref_count
	// MX_TTY0  (hardcoded in mx_tty.c)
	{"TTY0",       0, SMD_TYPE_APPS_MODEM | SMD_KIND_STREAM, 1},
	// MX_TTY1  (hardcoded in mx_tty.c)
	{"TTY1",            1, SMD_TYPE_APPS_MODEM | SMD_KIND_STREAM, 1},
	// MX_TTY2  (hardcoded in mx_tty.c)
	{"TTY2",         2, SMD_TYPE_APPS_MODEM | SMD_KIND_STREAM, 1},
	// MX_DATA1 (hardcoded in mx_net.c)
	{"TTY3",         3, SMD_TYPE_APPS_MODEM | SMD_KIND_STREAM, 1},
};

static unsigned char smd_ch_allocated[SMD_CHANNELS];
static int smd_alloc_channel(const char *name, uint32_t cid, uint32_t type);

static void __devinit smd_channel_probe(void)
{
	struct smd_alloc_elm *shared;
	unsigned ctype;
	unsigned type;
	unsigned n;

	pr_info("SMD: smd_channel_probe()\n");

	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(*shared) * SMD_CHANNELS);
	if (!shared) {
		pr_err("smd: cannot find allocation table\n");
		return;
	}
	for (n = 0; n < SMD_CHANNELS; n++) {
		if (smd_ch_allocated[n])
			continue;
		if (!shared[n].ref_count)
			continue;
		if (!shared[n].name[0])
			continue;
		ctype = shared[n].ctype;
		type = ctype & SMD_TYPE_MASK;

		/* DAL channels are stream but neither the modem,
		 * nor the DSP correctly indicate this.  Fixup manually.
		 */
		if (!memcmp(shared[n].name, "DAL", 3))
			ctype = (ctype & (~SMD_KIND_MASK)) | SMD_KIND_STREAM;

		type = shared[n].ctype & SMD_TYPE_MASK;
		if ((type == SMD_TYPE_APPS_MODEM) ||
		    (type == SMD_TYPE_APPS_DSP))
			if (!smd_alloc_channel(shared[n].name, shared[n].cid, ctype))
				smd_ch_allocated[n] = 1;
	}
}

/* how many bytes are available for reading */
static int smd_stream_read_avail(struct smd_channel *ch)
{
	return (ch->recv->head - ch->recv->tail) & ch->fifo_mask;
}

/* how many bytes we are free to write */
static int smd_stream_write_avail(struct smd_channel *ch)
{
	return ch->fifo_mask -
		((ch->send->head - ch->send->tail) & ch->fifo_mask);
}

static int smd_packet_read_avail(struct smd_channel *ch)
{
	if (ch->current_packet) {
		int n = smd_stream_read_avail(ch);
		if (n > ch->current_packet)
			n = ch->current_packet;
		return n;
	} else {
		return 0;
	}
}

static int smd_packet_write_avail(struct smd_channel *ch)
{
	int n = smd_stream_write_avail(ch);
	return n;
}

static int ch_is_open(struct smd_channel *ch)
{
	return (ch->recv->state == SMD_SS_OPENED) &&
		(ch->send->state == SMD_SS_OPENED);
}

/* provide a pointer and length to readable data in the fifo */
static unsigned ch_read_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->recv->head;
	unsigned tail = ch->recv->tail;
	*ptr = (void *) (ch->recv_data + tail);

	if (tail <= head)
		return head - tail;
	else
		return ch->fifo_size - tail;
}

/* advance the fifo read pointer after data from ch_read_buffer is consumed */
static void ch_read_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_read_avail(ch));
	ch->recv->tail = (ch->recv->tail + count) & ch->fifo_mask;
// PATCH TI - inconsistent (REVISIT)
//	ch->send->fTAIL = 1;
	ch->recv->fTAIL = 1;
}

/* basic read interface to ch_read_{buffer,done} used
 * by smd_*_read() and update_packet_state()
 * will read-and-discard if the _data pointer is null
 */
static int ch_read(struct smd_channel *ch, void *_data, int len)
{
	void *ptr;
	unsigned n;
	unsigned char *data = _data;
	int orig_len = len;

	while (len > 0) {
		n = ch_read_buffer(ch, &ptr);
		if (n == 0)
			break;

		if (n > len)
			n = len;
		if (_data)
			memcpy(data, ptr, n);

		data += n;
		len -= n;
		ch_read_done(ch, n);
	}

	return orig_len - len;
}
static int ch_read_packet_header(struct smd_channel *ch, void *_data, int len)
{
	void *ptr;
	unsigned n;
	unsigned char *data = _data;
	int orig_len = len;

	while (len > 0) {
		n = ch_read_buffer(ch, &ptr);
		if (n == 0)
			break;

		if (n > len)
			n = len;
		if (_data)
			memcpy(data, ptr, n);

		data += n;
		len -= n;
	}

	return orig_len - len;
}

static void update_stream_state(struct smd_channel *ch)
{
	/* streams have no special state requiring updating */
}

static void update_packet_state(struct smd_channel *ch)
{
	struct iphdr rx_ip_hdr;
	int r;
	
	/* can't do anything if we're in the middle of a packet */
	if (ch->current_packet != 0)
		return;

	/* don't bother unless we can get the full header */
	if (smd_stream_read_avail(ch) < sizeof(struct iphdr))
		return;

	memset(&rx_ip_hdr,0, sizeof(struct iphdr));
	r = ch_read_packet_header(ch, &rx_ip_hdr, sizeof(struct iphdr));
	BUG_ON(r != sizeof(struct iphdr));

	
	if(rx_ip_hdr.ihl != 5 && rx_ip_hdr.version != 4)
	{
		printk("%s no IP packet!\n", ch->name);
		r = ch_read(ch, 0, smd_stream_read_avail(ch));
		return;
	}
	ch->current_packet = ntohs(rx_ip_hdr.tot_len);
	switch(rx_ip_hdr.protocol)
	{
		case IPPROTO_IP:
			printk(KERN_DEBUG "%s: IP dummy packet, size=%d!\n", ch->name, ch->current_packet);
			break;
		case IPPROTO_ICMP:
			printk(KERN_DEBUG "%s: IP ICMP packet, size=%d!\n", ch->name, ch->current_packet);
			break;			
		case IPPROTO_IGMP:
			printk(KERN_DEBUG "%s: IP IGMP packet, size=%d!\n", ch->name, ch->current_packet);
			break;			
		case IPPROTO_IPIP:
			printk(KERN_DEBUG "%s: IP IP tunnel packet, size=%d!\n", ch->name, ch->current_packet);
			break;	
		case IPPROTO_TCP:
			printk(KERN_DEBUG "%s: IP TCP packet, size=%d!\n", ch->name, ch->current_packet);
			break;			
		case IPPROTO_UDP:
			printk(KERN_DEBUG "%s: IP UDP packet, size=%d!\n", ch->name, ch->current_packet);
			break;			
		break;
	}
}

/* provide a pointer and length to next free space in the fifo */
static unsigned ch_write_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->send->head;
	unsigned tail = ch->send->tail;
	*ptr = (void *) (ch->send_data + head);

	if (head < tail) {
		return tail - head - 1;
	} else {
		if (tail == 0)
			return ch->fifo_size - head - 1;
		else
			return ch->fifo_size - head;
	}
}

/* advace the fifo write pointer after freespace
 * from ch_write_buffer is filled
 */
static void ch_write_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_write_avail(ch));
	ch->send->head = (ch->send->head + count) & ch->fifo_mask;
	ch->send->fHEAD = 1;
}

static void ch_set_state(struct smd_channel *ch, unsigned n)
{
	if (n == SMD_SS_OPENED) {
		ch->send->fDSR = 1;
		ch->send->fCTS = 1;
		ch->send->fCD = 1;
	} else {
		ch->send->fDSR = 0;
		ch->send->fCTS = 0;
		ch->send->fCD = 0;
	}
	ch->send->state = n;
	ch->send->fSTATE = 1;
//	ch->notify_other_cpu(ch);
}
#if 0
static void do_smd_probe(void)
{
	static int done = 0;
	if (!done) {
		schedule_work(&probe_work);
		done = 1;
	}
}
#endif
static void smd_state_change(struct smd_channel *ch,
			     unsigned last, unsigned next)
{
	ch->last_state = next;

	printk("SMD: ch %d %d -> %d\n", ch->n, last, next);

	switch (next) {
	case SMD_SS_OPENING:
		ch->recv->tail = 0;
	case SMD_SS_OPENED:
		if (ch->send->state != SMD_SS_OPENED)
			ch_set_state(ch, SMD_SS_OPENED);
		ch->notify(ch->priv, SMD_EVENT_OPEN);
		break;
	case SMD_SS_FLUSHING:
	case SMD_SS_RESET:
		/* we should force them to close? */
	default:
		ch->notify(ch->priv, SMD_EVENT_CLOSE);
	}
}

static void handle_smd_irq(struct smd_channel *ch, void (*notify)(struct smd_channel *))
{
	unsigned long flags;
	unsigned tmp;
	unsigned do_notify=0;

	if (ch_is_open(ch)) {
		spin_lock_irqsave(&ch->read_lock, flags);
		tmp = ch->recv->state;
		if (tmp != ch->last_state)
			smd_state_change(ch, ch->last_state, tmp);
		ch->update_state(ch);
		spin_unlock_irqrestore(&ch->read_lock, flags);
		ch->notify(ch->priv, SMD_EVENT_DATA);
	}
	if (do_notify)
		notify(ch);
}
static void smd_rx_work(struct work_struct *work)
{
	struct smd_channel *smd_ch = container_of(work, struct smd_channel, read_work);
	
	handle_smd_irq(smd_ch, NULL);
}

static void smd_write_complete_cb(void *priv)
{
	struct smd_channel *ch = (struct smd_channel*)priv;
	if(!smd_initialized)
		return;
	if(!ch_is_open(ch))
		return;
	queue_work(ch->write_wq, &ch->write_work);
}
void smd_kick(smd_channel_t *ch)
{
	smd_low_layer_notification(ch);
}

static int smd_is_packet(int chn, unsigned type)
{
	type &= SMD_KIND_MASK;
	if (type == SMD_KIND_PACKET)
		return 1;
	if (type == SMD_KIND_STREAM)
		return 0;

	/* older AMSS reports SMD_KIND_UNKNOWN always */
	if ((chn > 4) || (chn == 1))
		return 1;
	else
		return 0;
}

static int smd_stream_write(smd_channel_t *ch, const void *_data, int len)
{
	void *ptr;
	const unsigned char *buf = _data;
	unsigned xfer;
	int orig_len = len;

	if (len < 0)
		return -EINVAL;

	while ((xfer = ch_write_buffer(ch, &ptr)) != 0) {
		if (!ch_is_open(ch))
		{
			break;
		}
		if (xfer > len)
			xfer = len;
		memcpy(ptr, buf, xfer);
		ch_write_done(ch, xfer);
		len -= xfer;
		buf += xfer;
		queue_work(ch->write_wq, &ch->write_work);
		if (len == 0)
			break;
	}

	return orig_len - len;
}

static int smd_packet_write(smd_channel_t *ch, const void *_data, int len)
{
	if (len < 0)
		return -EINVAL;

	if (smd_stream_write_avail(ch) < len)
		return -ENOMEM;

	return smd_stream_write(ch, _data, len);
}

static int smd_stream_read(smd_channel_t *ch, void *data, int len)
{
	int r;

	if (len < 0)
		return -EINVAL;
	r = ch_read(ch, data, len);
	if (r > 0)
		ch->notify_other_cpu(ch);

	return r;
}

static int smd_packet_read(smd_channel_t *ch, void *data, int len)
{
	unsigned long flags;
	int r;

	if (len < 0)
		return -EINVAL;

	if (len > ch->current_packet)
		len = ch->current_packet;

	r = ch_read(ch, data, len);
	if (r > 0)
		ch->notify_other_cpu(ch);

	spin_lock_irqsave(&ch->read_lock, flags);
	ch->current_packet -= r;
	update_packet_state(ch);
	spin_unlock_irqrestore(&ch->read_lock, flags);

	return r;
}

static int smd_alloc_v1(struct smd_channel *ch)
{
	struct smd_shared_v1 *shared1;
	shared1 = smem_alloc(ID_SMD_CHANNELS + ch->n, sizeof(*shared1));
	if (!shared1) {
		pr_err("smd_alloc_channel() cid %d does not exist\n", ch->n);
		return -1;
	}
	ch->send = &shared1->ch0;
	ch->recv = &shared1->ch1;
	ch->chuck = &shared1->ch2;
	ch->send_data = shared1->data0;
	ch->recv_data = shared1->data1;
	ch->chuck_data = shared1->data2;	
	ch->fifo_size = SMD_BUF_SIZE;
	return 0;
}
static int smd_alloc_channel(const char *name, uint32_t cid, uint32_t type)
{
	struct smd_channel *ch;
	char wq_name[32];
	
	ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
	if (ch == 0) {
		pr_err("smd_alloc_channel() out of memory\n");
		return -1;
	}
	ch->n = cid;

	if (smd_alloc_v1(ch)) {
		kfree(ch);
		return -1;
	}
	ch->xmmch = acm_register(cid, &ch->notify_low_layer, smd_rx_data_cb, smd_write_complete_cb, ch);
	if(!ch->xmmch){
		kfree(ch);
		return -1;
	}
	ch->fifo_mask = ch->fifo_size - 1;
	ch->type = type;

	if ((type & SMD_TYPE_MASK) == SMD_TYPE_APPS_MODEM)
		ch->notify_other_cpu = notify_modem_smd;
	else
		ch->notify_other_cpu = notify_dsp_smd;

	if (smd_is_packet(cid, type)) {
		ch->read = smd_packet_read;
		ch->write = smd_packet_write;
		ch->read_avail = smd_packet_read_avail;
		ch->write_avail = smd_packet_write_avail;
		ch->update_state = update_packet_state;
	} else {
		ch->read = smd_stream_read;
		ch->write = smd_stream_write;
		ch->read_avail = smd_stream_read_avail;
		ch->write_avail = smd_stream_write_avail;
		ch->update_state = update_stream_state;
	}
	
	spin_lock_init(&ch->read_lock);
	spin_lock_init(&ch->write_lock);
	
	if ((type & 0xff) == 0)
		memcpy(ch->name, "SMD_", 4);
	else
		memcpy(ch->name, "DSP_", 4);
	memcpy(ch->name + 4, name, 20);
	ch->name[23] = 0;

	memset(wq_name, 0, 32);
	sprintf(wq_name, "WR_%s", ch->name);
	ch->write_wq = create_singlethread_workqueue(wq_name);
	INIT_WORK(&ch->write_work, smd_send_work);

	memset(wq_name, 0, 32);
	sprintf(wq_name, "RD_%s", ch->name);
	ch->read_wq = create_singlethread_workqueue(wq_name);
	INIT_WORK(&ch->read_work, smd_rx_work);
#if 0
	ch->rx_task.func = smd_rx_tasklet;
	ch->rx_task.data = (unsigned long) ch;
	
	ch->wr_task.func = smd_send_tasklet;
	ch->wr_task.data = (unsigned long) ch;
#endif
//	INIT_WORK(&ch->rx_resume_work, smd_rx_resume_worker);
	pr_info("smd_alloc_channel() cid=%02d size=%05d '%s'\n",
		ch->n, ch->fifo_size, ch->name);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	return 0;
}

static void do_nothing_notify(void *priv, unsigned flags)
{
}

struct smd_channel *smd_get_channel(const char *name)
{
	struct smd_channel *ch;

	mutex_lock(&smd_creation_mutex);
	list_for_each_entry(ch, &smd_ch_closed_list, ch_list) {
		if (!strcmp(name, ch->name)) {
			list_del(&ch->ch_list);
			mutex_unlock(&smd_creation_mutex);
			return ch;
		}
	}
	mutex_unlock(&smd_creation_mutex);

	return NULL;
}
struct smd_channel *smd_get_channel_opened(const unsigned int cid)
{
	struct smd_channel *ch;

	mutex_lock(&smd_creation_mutex);
	list_for_each_entry(ch, &smd_ch_list_modem, ch_list) {
		if (cid == ch->n) {
			mutex_unlock(&smd_creation_mutex);
			return ch;
		}
	}
	list_for_each_entry(ch, &smd_ch_list_dsp, ch_list) {
		if (cid == ch->n) {
			mutex_unlock(&smd_creation_mutex);
			return ch;
		}
	}
	mutex_unlock(&smd_creation_mutex);

	return NULL;
}
int smd_open(const char *name, smd_channel_t **_ch,
	     void *priv, void (*notify)(void *, unsigned))
{
	struct smd_channel *ch;
	unsigned long flags;
		
	if (smd_initialized == 0) {
		pr_info("smd_open() before smd_init()\n");
		return -ENODEV;
	}
	printk("smd_open()+, name:%s================\n", name);
	ch = smd_get_channel(name);
	if (!ch)
		return -ENODEV;

	if (notify == 0)
		notify = do_nothing_notify;

	ch->notify = notify;
	ch->current_packet = 0;
	ch->last_state = SMD_SS_CLOSED;
	ch->priv = priv;

	*_ch = ch;

	
	mutex_lock(&smd_creation_mutex);	
	if ((ch->type & SMD_TYPE_MASK) == SMD_TYPE_APPS_MODEM)
		list_add(&ch->ch_list, &smd_ch_list_modem);
	else
		list_add(&ch->ch_list, &smd_ch_list_dsp);
	mutex_unlock(&smd_creation_mutex);
	
	spin_lock_irqsave(&smd_lock, flags);
	/* If the remote side is CLOSING, we need to get it to
	 * move to OPENING (which we'll do by moving from CLOSED to
	 * OPENING) and then get it to move from OPENING to
	 * OPENED (by doing the same state change ourselves).
	 *
	 * Otherwise, it should be OPENING and we can move directly
	 * to OPENED so that it will follow.
	 */
	if (ch->recv->state == SMD_SS_CLOSING) {
		ch->send->head = 0;
		ch_set_state(ch, SMD_SS_OPENING);
	} else {
		ch_set_state(ch, SMD_SS_OPENED);
	}
	if (ch->send->fSTATE) {
		ch->send->fSTATE = 0;
		if (ch->send->state != ch->recv->state) {
			pr_info("SMD: SMD recv changed state "
				"from %d to %d\n", ch->recv->state,
					ch->send->state);
			ch->recv->state = ch->send->state;
			ch->recv->fSTATE = 1;
		}
	}
	spin_unlock_irqrestore(&smd_lock, flags);
	if(ch->xmmch->open(ch->xmmch))
		return -ENODEV;
	return 0;
}

int smd_close(smd_channel_t *ch)
{
	unsigned long flags;

	pr_info("smd_close channel:%d(%p) ++ \n", ch->n, ch);

	if (!ch)
		return -1;
	cancel_work_sync(&ch->write_work);
	cancel_work_sync(&ch->read_work);
	
	spin_lock_irqsave(&smd_lock, flags);
	ch->notify = do_nothing_notify;
	list_del(&ch->ch_list);
	ch_set_state(ch, SMD_SS_CLOSED);

	ch->send->head = 0;
	ch->send->tail = 0;
	ch->recv->head = 0;
	ch->recv->tail = 0;
	spin_unlock_irqrestore(&smd_lock, flags);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	ch->xmmch->close(ch->xmmch);

	pr_info("smd_close channel:%d(%p) -- \n", ch->n, ch);

	return 0;
}

int smd_read(smd_channel_t *ch, void *data, int len)
{
	int ret;

	ret = ch->read(ch, data, len);
	//printk("%s: %s read =%d**\n", __func__, ch->name, ret);
	return ret;
}

int smd_write(smd_channel_t *ch, const void *data, int len)
{
	int ret;

	if(ch->xmmch->open(ch->xmmch))
		return -ENODEV;
	ret = ch->write(ch, data, len);
	//printk("%s: %s write =%d##\n", __func__, ch->name, ret);
	return ret;
}

static int smd_send_read_dbl_seg (struct smd_channel *ch,
						struct smd_seg_desc ds[2])
{
	unsigned head = ch->send->head;
	unsigned tail = ch->send->tail;

	if (tail <= head) {
		ds[0].buff = (char *) (ch->send_data + tail);
		ds[0].len = (head - tail);
		ds[1].buff = NULL;
		ds[1].len = 0;
	} else {
		ds[0].buff = (char *) (ch->send_data + tail);
		ds[0].len = (ch->fifo_size - tail);
		ds[1].buff = (char *) (ch->send_data);
		ds[1].len = head;
	}
	return (ds[0].len + ds[1].len);
}
static int smd_send_pending(struct smd_channel *smd_ch)
{
	int len;

	len = ((smd_ch->send->head - smd_ch->send->tail) & smd_ch->fifo_mask);

	if (ch_is_open(smd_ch))
		return len;
	else {
		if (len)
			pr_err(
				"SMD: pending send data in non-open smd ch\n");
		return 0;
	}
}

static void smd_send_read_done (struct smd_channel *ch,
					unsigned count)
{
	BUG_ON(count > smd_send_pending(ch));
	ch->send->tail = (ch->send->tail + count) & ch->fifo_mask;
	ch->send->fTAIL = 1;
}
static inline int smd_init_dbl_seg (struct smd_seg_desc dbs[2],
					char *data1, int len1,
					char *data2, int len2)
{
	dbs[0].buff = data1;
	dbs[0].len = len1;
	dbs[1].buff = data2;
	dbs[1].len = len2;
	return (len1 + len2);
}

static inline int smd_init_sgl_seg (struct smd_seg_desc *dbs,
					char *data, int len)
{
	dbs[0].buff = data;
	dbs[0].len = len;
	return (len);
}

static int smd_copy_sgl_dbl_seg ( struct smd_seg_desc *origin_sgs,
					struct smd_seg_desc dest_dbs[2])
{
	int copied = 0, i, len;
	struct smd_seg_desc *dest;

	for (i = 0; i < 2; i++) {
		dest = &dest_dbs[i];

		if (origin_sgs->len > dest->len)
			len = dest->len;
		else
			len = origin_sgs->len;
		memcpy (dest->buff, origin_sgs->buff ,len);

		dest->buff += len;
		dest->len -= len;
		origin_sgs->buff += len;
		origin_sgs->len -= len;
		copied += len;

		if (!origin_sgs->len)
			break;
	}

	return copied;
}


static int smd_copy_dbl_dbl_seg ( struct smd_seg_desc origin_dbs[2],
					struct smd_seg_desc dest_dbs[2])
{
	int copied = 0, i, data_len;
	struct smd_seg_desc *origin;

	data_len = origin_dbs[0].len + origin_dbs[1].len;

	for (i = 0; i < 2; i++) {
		origin = &origin_dbs[i];
		copied += smd_copy_sgl_dbl_seg (origin, dest_dbs);
	}

	if (copied != data_len)
		pr_err("SMD: write_seg error in: %d written: %d \n",
			data_len, copied);

	return copied;
}

static int smd_clip_dbl_seg ( struct smd_seg_desc data_dbs[2],
					int len)
{
	data_dbs[0].len = min (data_dbs[0].len, len);
	data_dbs[1].len = min (data_dbs[1].len, len - data_dbs[0].len);

	return (data_dbs[0].len + data_dbs[1].len);
}
static void smd_send_work(struct work_struct *work)
{
	struct smd_channel *ch = container_of(work, struct smd_channel, write_work);
	struct acm_ch *xmmch;
	struct smd_seg_desc data[2], dest[2];
	int len, maxsize;
	char *chunk;
	
	xmmch = ch->xmmch;

	for(;;)
	{
		if(xmmch->is_ready(xmmch) && ch_is_open(ch))
		{
			maxsize =  xmmch->get_write_frame(xmmch);
			chunk = ch->chuck_data;
			if (ch->fifo_size < maxsize) 
				maxsize = ch->fifo_size;

			smd_init_dbl_seg(data, NULL, 0, NULL, 0);

			len = smd_send_read_dbl_seg(ch, data);
			if(len<=0)
			{
				break;
			}
			if (len > maxsize) 
				len = maxsize;
			len = smd_clip_dbl_seg(data, len);
			smd_init_dbl_seg(dest, chunk, maxsize, NULL, 0);
			smd_copy_dbl_dbl_seg(data, dest);
			if(xmmch->get_write_room(xmmch) <= 0)
			{
				break;
			}
			len = xmmch->write(xmmch, chunk, len);
			if(len<=0)
			{
				break;
			}
			smd_send_read_done (ch, len);
		}else{
			len = smd_send_pending(ch);
			if(len>0)
				smd_send_read_done (ch, len);//modem not ready drop write data
			break;
		}
	}
}
int smd_write_atomic(smd_channel_t *ch, const void *data, int len)
{
	unsigned long flags;
	int res;
	spin_lock_irqsave(&ch->write_lock, flags);
	res = ch->write(ch, data, len);
	spin_unlock_irqrestore(&ch->write_lock, flags);

	//printk("%s: %s write =%d##\n", __func__, ch->name, res);	
	return res;
}

int smd_read_avail(smd_channel_t *ch)
{
	return ch->read_avail(ch);
}

int smd_write_avail(smd_channel_t *ch)
{
	return ch->write_avail(ch);
}

int smd_wait_until_readable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_wait_until_writable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_cur_packet_size(smd_channel_t *ch)
{
	return ch->current_packet;
}


/* ------------------------------------------------------------------------- */

static struct smd_shared_v1 *smem_base[SMD_CHANNELS];

void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}

void *smem_item(unsigned id, unsigned *size)
{
	void *ptr = 0;

	if (id == ID_CH_ALLOC_TBL) {
		ptr = (void *) smd_shared;
		*size = sizeof(smd_shared);
	}

	if ((id >= ID_SMD_CHANNELS) &&
	    (id < ID_SMD_CHANNELS + SMD_CHANNELS)) {
		ptr = (void *) smem_base[id - ID_SMD_CHANNELS];
		*size = sizeof(struct smd_shared_v1);
	}

	return ptr;
}

void *smem_find(unsigned id, unsigned size_in)
{
	unsigned size;
	void *ptr;

	ptr = smem_item(id, &size);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		pr_err("smem_find(%d, %d): wrong size %d\n",
		       id, size_in, size);
		return 0;
	}

	return ptr;
}

void smd_low_layer_notification(struct smd_channel *ch)
{
	queue_work(ch->read_wq, &ch->read_work);
}
static int smd_recv_write_dbl_seg (struct smd_channel *smd_ch,
						struct smd_seg_desc ds[2])
{
	unsigned head = smd_ch->recv->head;
	unsigned tail = smd_ch->recv->tail;

	if (head < tail) {
		ds[0].buff = (char *) (smd_ch->recv_data + head);
		ds[0].len = (tail - head - 1);
		ds[1].buff = NULL;
		ds[1].len = 0;
	} else {
		ds[0].buff = (char *) (smd_ch->recv_data + head);
		ds[1].buff = (char *) (smd_ch->recv_data);
		if (tail == 0) {
			ds[0].len = (smd_ch->fifo_size - head - 1);
			ds[1].len = 0;
		} else {
			ds[0].len = (smd_ch->fifo_size - head);
			ds[1].len = (tail - 1);
		}
	}
	return (ds[0].len + ds[1].len);
}
static int smd_recv_free(struct smd_channel *smd_ch)
{
	int len;
	len = (smd_ch->fifo_size - 1) -
		((smd_ch->recv->head - smd_ch->recv->tail)
						& smd_ch->fifo_mask);
	return len;
}
static void smd_recv_write_done (struct smd_channel *smd_ch,
								unsigned count)
{
	BUG_ON(count > smd_recv_free(smd_ch));
	smd_ch->recv->head = (smd_ch->recv->head + count) & smd_ch->fifo_mask;
	smd_ch->recv->fHEAD = 1;
}

static int smd_rx_data_cb (void *priv, char *rx_addr, int rx_len,
					int rx_flags, int* free)
{
	struct smd_channel *ch = priv;
	struct smd_seg_desc frame[2], pdu[2];
	int len, n;
	char *data;
	
	if(!smd_initialized)
		return rx_len;
	if(!ch_is_open(ch))
		return rx_len;
	
	data = rx_addr;
	len = rx_len;
	n = smd_recv_write_dbl_seg(ch, pdu);
	if (len > n) {
		pr_err("SMD: channel %d RX overflow,"
			"len:%d, dropping %d bytes\n", ch->n, len, (len - n));
		smd_low_layer_notification(ch);

		return len;
	}

	smd_init_sgl_seg(&frame[0], data, len);
	smd_copy_sgl_dbl_seg(&frame[0], pdu);
	smd_recv_write_done (ch, len);
	
	smd_low_layer_notification(ch);

	return len;
}

static int __devinit smd_core_init(void)
{
	int id, status = 0;
	struct smd_shared_v1 *addr, *ch;
	unsigned long virt_addr;

	for (id = 0; id < SMD_CHANNELS; id++) {
		virt_addr = (unsigned long)kzalloc(sizeof(struct smd_shared_v1), GFP_KERNEL);
		addr = ch = (struct smd_shared_v1 *)virt_addr;
		smem_base[id] = ch;
		ch->ch0.state  = ch->ch1.state = SMD_SS_CLOSED;
		ch->ch0.fDSR   = ch->ch1.fDSR  = 0;
		ch->ch0.fCTS   = ch->ch1.fCTS  = 0;
		ch->ch0.fCD    = ch->ch1.fCD  = 0;
		ch->ch0.fRI    = ch->ch1.fRI   = 0;
		ch->ch0.fHEAD  = ch->ch1.fHEAD = 0;
		ch->ch0.fTAIL  = ch->ch1.fTAIL = 0;
		ch->ch0.fSTATE = ch->ch1.fSTATE = 0;
		ch->ch0.tail   = ch->ch1.tail  = 0;
		ch->ch0.head   = ch->ch1.head  = 0;
	}
	return status;
}

int __init smd_init(void)
{
	if (smd_core_init()) {
		pr_err("smd_init() failed\n");
		return -1;
	}

	smd_channel_probe();
	smd_initialized = 1;

	return 0;
}

void __exit smd_exit(void)
{
	int id = 0;
	struct smd_channel *ch;
	pr_err("SMD: smd_exit() - Can't be removed! yet \n");

	mutex_lock(&smd_creation_mutex);
	list_for_each_entry(ch, &smd_ch_list_modem, ch_list) {
		kfree(ch);
	}
	list_for_each_entry(ch, &smd_ch_list_dsp, ch_list) {
		kfree(ch);
	}
	list_for_each_entry(ch, &smd_ch_closed_list, ch_list) {
		kfree(ch);
	}
	mutex_unlock(&smd_creation_mutex);
	for (id = 0; id < SMD_CHANNELS; id++)
	{
		kfree(smem_base[0]);
	}
	smd_initialized = 0;
}

