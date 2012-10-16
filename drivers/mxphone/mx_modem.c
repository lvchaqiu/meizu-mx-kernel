/*
 * mx_modem.c
 *
 * Copyright (c) 2011 WenbinWu	<wenbinwu@meizu.com>
 *
 * base on modemctl driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/semaphore.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <linux/sched.h>
//#include <linux/bootmode.h>
#include <linux/async.h>
#include <mach/modem.h>
#include <plat/gpio-cfg.h>
#include <asm/mach-types.h>
#include "mx_modem.h"

static struct modemctl *global_mc;
int modem_debug = 0;
DEFINE_SEMAPHORE(modem_lock);

#ifdef CONFIG_XMM6260_ACM_CLASS
extern int acm_init(void);
extern int smd_init(void);
extern void  acm_exit(void);
extern void smd_exit(void);
extern int acm_request_resume(void);
#endif
#ifdef CONFIG_USB_EHCI_HCD
extern int  s5p_ehci_power(int value);
#endif
static int modem_renumeration(struct modemctl *mc);
static int modem_boot_enumeration(struct modemctl *mc);
static int modem_main_enumeration(struct modemctl *mc);

#ifdef CONFIG_HAS_WAKELOCK	
static void modem_wake_lock_initial(struct modemctl *mc)
{
	wake_lock_init(&mc->modem_lock, WAKE_LOCK_SUSPEND, "modemctl");
}
static void modem_wake_lock_destroy(struct modemctl *mc)
{
	wake_lock_destroy(&mc->modem_lock);
}
static void modem_wake_lock(struct modemctl *mc)
{
	wake_lock(&mc->modem_lock);
}
static void modem_wake_lock_timeout(struct modemctl *mc,  int timeout)
{
	wake_lock_timeout(&mc->modem_lock, timeout);
}
#else
static void modem_wake_lock_initial(struct modemctl *mc){}
static void modem_wake_lock_destroy(struct modemctl *mc){}
static void modem_wake_lock(struct modemctl *mc){}
static void modem_wake_lock_timeout(struct modemctl *mc,  int timeout){}
#endif


int modem_is_on(void)
{
	struct modemctl *mc = global_mc;

	if (!mc)
		return 0;

	return gpio_get_value(mc->pdata->gpio_cp_reset);
}
EXPORT_SYMBOL_GPL(modem_is_on);

int modem_set_active_state(int val)
{
	struct modemctl *mc = global_mc;

	if (!mc)
		return 0;

	gpio_set_value(mc->pdata->gpio_active_state, val ? 1 : 0);
	modem_dbg("%s: AP>>CP:   ACTIVE_STATE:%d\n", __func__, val ? 1 : 0);

	return 0;
}
EXPORT_SYMBOL_GPL(modem_set_active_state);

int modem_set_slave_wakeup(int val)
{
	struct modemctl *mc = global_mc;

	if (!mc)
		return 0;

	gpio_set_value(mc->pdata->gpio_ipc_slave_wakeup, val ? 1 : 0);
	modem_dbg("%s: AP>>CP:   SLAV_WUP:%d,%d\n", __func__, val ? 1 : 0,	gpio_get_value(mc->pdata->gpio_ipc_slave_wakeup));
	return 0;
}
EXPORT_SYMBOL_GPL(modem_set_slave_wakeup);

int modem_is_host_wakeup(void)
{
	struct modemctl *mc = global_mc;

	if (!mc)
		return 0;

	return ((gpio_get_value(mc->pdata->gpio_ipc_host_wakeup)) ==HOST_WUP_LEVEL)? 1 : 0;
}
EXPORT_SYMBOL_GPL(modem_is_host_wakeup);

int modem_prepare_resume(int ms_time)
{
	int val;
	struct completion done;
	struct modemctl *mc = global_mc;

	if (!mc)
		return -EFAULT;

	if( mc->cp_flag & MODEM_EVENT_MASK || !mc->boot_done || !mc->enum_done)
		return MC_HOST_HALT;
	
	val = gpio_get_value(mc->pdata->gpio_ipc_slave_wakeup);
	if (val) {
		gpio_set_value(mc->pdata->gpio_ipc_slave_wakeup, 0);
		modem_dbg("SLAV_WUP:reset\n");
	}
	val = gpio_get_value(mc->pdata->gpio_ipc_host_wakeup);
	if (val == HOST_WUP_LEVEL) {
		modem_dbg("HOST_WUP:high!\n");
		return MC_HOST_HIGH;
	}

	init_completion(&done);
	mc->l2_done = &done;
	gpio_set_value(mc->pdata->gpio_ipc_slave_wakeup, 1);
	modem_dbg( "AP>>CP:  SLAV_WUP:1,%d\n",
		gpio_get_value(mc->pdata->gpio_ipc_slave_wakeup));

	if (!wait_for_completion_timeout(&done, ms_time)) {
		val = gpio_get_value(mc->pdata->gpio_ipc_host_wakeup);
		if (val == HOST_WUP_LEVEL) {
			modem_err( "maybe complete late.. %d\n", ms_time);
			mc->l2_done = NULL;
			return MC_SUCCESS;
		}
		modem_err("Modem wakeup timeout %d\n", ms_time);
		gpio_set_value(mc->pdata->gpio_ipc_slave_wakeup, 0);
		modem_err("AP>>CP:  SLAV_WUP:0,%d\n",
			gpio_get_value(mc->pdata->gpio_ipc_slave_wakeup));
		mc->l2_done = NULL;
		return MC_HOST_TIMEOUT;
	}
	return MC_SUCCESS;
}
EXPORT_SYMBOL_GPL(modem_prepare_resume);
static int modem_cfg(struct modemctl *mc)
{
	static int xmm6260_initialed=0;
	int err = 0;

	if(xmm6260_initialed)
		return 0;
	/*TODO: check uart init func AP FLM BOOT RX -- */
	err = gpio_request(mc->pdata->gpio_pmu_reset, "CP_PMU_RST");
	if (err) {
		modem_err("fail to request gpio %s\n", "CP_PMU_RST");
	} else {
		gpio_direction_output(mc->pdata->gpio_pmu_reset, 1);
		s3c_gpio_setpull(mc->pdata->gpio_pmu_reset, S3C_GPIO_PULL_NONE);
	}	
	err = gpio_request(mc->pdata->gpio_phone_on, "PHONE_ON");
	if (err) {
		modem_err("fail to request gpio %s\n", "PHONE_ON");
	} else {
		gpio_direction_output(mc->pdata->gpio_phone_on, 0);
		s3c_gpio_setpull(mc->pdata->gpio_phone_on, S3C_GPIO_PULL_NONE);
	}
	err = gpio_request(mc->pdata->gpio_cp_reset, "CP_RST");
	if (err) {
		modem_err("fail to request gpio %s\n", "CP_RST");
	} else {
		gpio_direction_output(mc->pdata->gpio_cp_reset, 1);
		s3c_gpio_setpull(mc->pdata->gpio_cp_reset, S3C_GPIO_PULL_NONE);
	}
	
	err = gpio_request(mc->pdata->gpio_ipc_slave_wakeup, "IPC_SLAVE_WAKEUP");
	if (err) {
		modem_err("fail to request gpio %s\n",
			"IPC_SLAVE_WAKEUP");
	} else {
		gpio_direction_output(mc->pdata->gpio_ipc_slave_wakeup, 0);
		s3c_gpio_setpull(mc->pdata->gpio_ipc_slave_wakeup, S3C_GPIO_PULL_NONE);
	}

	err = gpio_request(mc->pdata->gpio_ipc_host_wakeup, "IPC_HOST_WAKEUP");
	if (err) {
		modem_err("fail to request gpio %s\n", "IPC_HOST_WAKEUP");
	} else {
		gpio_direction_output(mc->pdata->gpio_ipc_host_wakeup, 0);
		s3c_gpio_cfgpin(mc->pdata->gpio_ipc_host_wakeup, S3C_GPIO_INPUT);
		s3c_gpio_setpull(mc->pdata->gpio_ipc_host_wakeup, S3C_GPIO_PULL_DOWN);
	}
	err = gpio_request(mc->pdata->gpio_suspend_request, "IPC_SUSPEND_REQUEST");
	if (err) {
		modem_err( "fail to request gpio %s\n", "IPC_SUSPEND_REQUEST");
	} else {
		gpio_direction_output(mc->pdata->gpio_suspend_request, 0);
		s3c_gpio_cfgpin(mc->pdata->gpio_suspend_request, S3C_GPIO_INPUT);
		s3c_gpio_setpull(mc->pdata->gpio_suspend_request, S3C_GPIO_PULL_DOWN);
	}	

	err = gpio_request(mc->pdata->gpio_cp_reset_int, "IPC_CP_ABNORMAL_RESET");
	if (err) {
		modem_err(KERN_ERR "fail to request gpio %s\n", "IPC_CP_ABNORMAL_RESET");
	} else {
		s3c_gpio_cfgpin(mc->pdata->gpio_cp_reset_int, S3C_GPIO_INPUT);
		s3c_gpio_setpull(mc->pdata->gpio_cp_reset_int, S3C_GPIO_PULL_UP);
	}
	if(machine_is_m030())
	{
		err = gpio_request(mc->pdata->gpio_sim_detect_int, "GPIO_SIM_DETECT_INT");
		if (err) {
			modem_err("fail to request gpio %s\n", "GPIO_SIM_DETECT_INT");
		} else {
			s3c_gpio_cfgpin(mc->pdata->gpio_sim_detect_int, S3C_GPIO_INPUT);
			s3c_gpio_setpull(mc->pdata->gpio_sim_detect_int, S3C_GPIO_PULL_NONE);
		}
	}
	err = gpio_request(mc->pdata->gpio_active_state, "ACTIVE_STATE");
	if (err) {
		modem_err("fail to request gpio %s\n", "ACTIVE_STATE");
	} else {
		gpio_direction_output(mc->pdata->gpio_active_state, 0);
		s3c_gpio_setpull(mc->pdata->gpio_active_state, S3C_GPIO_PULL_NONE);
	}
	xmm6260_initialed = 1;

	return 0;
}
static int modem_on(struct modemctl *mc)
{
	modem_dbg("%s: start\n", __func__);

	/*assert reset single*/
	gpio_set_value(mc->pdata->gpio_phone_on, 0);
	gpio_set_value(mc->pdata->gpio_pmu_reset, 0);	
	gpio_set_value(mc->pdata->gpio_cp_reset, 0);
	msleep(500);
	/*release reset single*/
	gpio_set_value(mc->pdata->gpio_cp_reset, 1);	
	mdelay(1);
	gpio_set_value(mc->pdata->gpio_pmu_reset, 1);
	mdelay(2);
	/*triger reset single*/
	gpio_set_value(mc->pdata->gpio_phone_on, 1);
	mdelay(1);
	gpio_set_value(mc->pdata->gpio_phone_on, 0);
	msleep(100);	
	modem_dbg("%s: finish\n", __func__);
	return 0;
}

static int modem_off(struct modemctl *mc)
{
	modem_dbg("%s\n", __func__);

	gpio_set_value(mc->pdata->gpio_pmu_reset, 0);
	gpio_set_value(mc->pdata->gpio_cp_reset, 0);
	gpio_set_value(mc->pdata->gpio_phone_on, 0);

	modem_wake_lock_timeout(mc, 10*HZ);
	
	return 0;
}

static int modem_reset(struct modemctl *mc)
{
	modem_err("%s\n", __func__);
	/*assert reset single*/
	gpio_set_value(mc->pdata->gpio_phone_on, 0);
	gpio_set_value(mc->pdata->gpio_pmu_reset, 0);	
	gpio_set_value(mc->pdata->gpio_cp_reset, 0);
	msleep(500);
	
	/*release reset single*/
	gpio_set_value(mc->pdata->gpio_cp_reset, 1);	
	mdelay(1);
	gpio_set_value(mc->pdata->gpio_pmu_reset, 1);
	mdelay(2);
	/*triger reset single*/
	gpio_set_value(mc->pdata->gpio_phone_on, 1);
	mdelay(1);
	gpio_set_value(mc->pdata->gpio_phone_on, 0);	
	return 0;
}

void modem_crash_event(int type)
{
	if (!global_mc)
		return;

	switch (type)
	{
	case MODEM_EVENT_POWEROFF:
		global_mc->cp_flag = 0;
		wake_up_interruptible(&global_mc->read_wq);
		modem_dbg("%s: MODEM_EVENT_POWEROFF\n", __func__);
		break;
	case MODEM_EVENT_RESET:
		if(global_mc->boot_done)
		{
			global_mc->cp_flag |= MODEM_RESET_FLAG;
			wake_up_interruptible(&global_mc->read_wq);
			modem_dbg("%s: MODEM_EVENT_RESET\n", __func__);
		}
		break;
	case MODEM_EVENT_DUMP:	
		if(global_mc->enum_done)
		{		
			global_mc->cp_flag |= MODEM_DUMP_FLAG;
			wake_up_interruptible(&global_mc->read_wq);
			modem_dbg("%s: MODEM_EVENT_DUMP\n", __func__);
		}
		break;
	case MODEM_EVENT_CRASH:
		if(global_mc->enum_done)
		{
			global_mc->cp_flag |= MODEM_CRASH_FLAG;
			wake_up_interruptible(&global_mc->read_wq);
			modem_dbg("%s: MODEM_EVENT_CRASH\n", __func__);
		}
		break;
	case MODEM_EVENT_CONN:
		if(global_mc->enum_done)
		{
			global_mc->cp_flag |= MODEM_CONNECT_FLAG;
			wake_up_interruptible(&global_mc->conn_wq);
			modem_dbg("%s: MODEM_EVENT_CONN\n", __func__);
		}
		break;
	case MODEM_EVENT_DISCONN:
		if(global_mc->enum_done)
		{		
			global_mc->cp_flag |= MODEM_DISCONNECT_FLAG;
			wake_up_interruptible(&global_mc->read_wq);
			modem_dbg("%s: MODEM_EVENT_DISCONN\n", __func__);
		}
		break;
	case MODEM_EVENT_SIM:
		if(global_mc->boot_done)
		{
			global_mc->cp_flag |= MODEM_SIM_DETECT_FLAG;
			global_mc->cp_flag |= MODEM_CRASH_FLAG;
			wake_up_interruptible(&global_mc->read_wq);
			modem_dbg("%s: MODEM_EVENT_SIM\n", __func__);
		}
		break;
	}
}

#ifdef CONFIG_XMM6260_ACM_CLASS
static irqreturn_t modem_resume_thread(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	int val = gpio_get_value(mc->pdata->gpio_ipc_host_wakeup);

	modem_dbg("CP>>AP:  HOST_WUP:%d\n", val);
	if(!mc->enum_done)
	{
		if (val == HOST_WUP_LEVEL) {
			if (mc->l2_done) {
				complete(mc->l2_done);
				mc->l2_done = NULL;
			}
			gpio_set_value(mc->pdata->gpio_ipc_slave_wakeup, 0);
			modem_dbg("AP>>CP:  SLAV_WUP:0,%d\n", gpio_get_value(mc->pdata->gpio_ipc_slave_wakeup));
			return IRQ_HANDLED;
		}
		return IRQ_HANDLED;
	}else{
		if (val != HOST_WUP_LEVEL) {
			if (mc->l2_done) {
				complete(mc->l2_done);
				mc->l2_done = NULL;
			}
			gpio_set_value(mc->pdata->gpio_ipc_slave_wakeup, 0);
			modem_dbg("AP>>CP:  SLAV_WUP:0,%d\n",
				gpio_get_value(mc->pdata->gpio_ipc_slave_wakeup));
			mc->debug_cnt = 0;
			return IRQ_HANDLED;
		}
	}


	if (val == HOST_WUP_LEVEL) {
		acm_request_resume();
		mc->debug_cnt++;
	}
	if (mc->debug_cnt > 30) {
		modem_err("Abnormal Host wakeup -- over 30times");
		mc->debug_cnt = 0;
		modem_dbg("before crash_event %s\n", __func__);
		modem_crash_event(MODEM_EVENT_DISCONN);
		modem_dbg("after crash_event %s\n", __func__);
	}

	if (!val
		&& mc->wakeup_flag == HOST_WAKEUP_WAIT_RESET) {
		mc->wakeup_flag = HOST_WAKEUP_LOW;
		modem_err( "%s: wakeup flag (%d)\n",
			__func__, mc->wakeup_flag);
	}

	return IRQ_HANDLED;
}

static irqreturn_t modem_cpreset_irq(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	int val = gpio_get_value(mc->pdata->gpio_cp_reset_int);

	modem_dbg("CP_RESET_INT:%d\n",  val);
		
	modem_wake_lock_timeout(mc, HZ*30);
	modem_crash_event(MODEM_EVENT_RESET);

	return IRQ_HANDLED;
}
static void modem_sim_detect_worker(struct work_struct *work)
{
	modem_crash_event(MODEM_EVENT_SIM);
}
static irqreturn_t modem_sim_detect_irq(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	int val = gpio_get_value(mc->pdata->gpio_sim_detect_int);

	modem_dbg("CP_SIM_DETECTED: SIM %s\n",  val? "removed": "insert");
	modem_wake_lock_timeout(mc, HZ*30);
	if(mc->boot_done) {
		modem_off(mc);
		if (!delayed_work_pending(&mc->sim_work))
			schedule_delayed_work(&mc->sim_work, msecs_to_jiffies(1500));//removed
	}
	mdelay(200);

	return IRQ_HANDLED;
}
#endif
static void _free_all(struct modemctl *mc)
{
	if (mc) {
		if (mc->ops)
			mc->ops = NULL;
#ifdef CONFIG_XMM6260_ACM_CLASS
			if (mc->irq[0]) free_irq(mc->irq[0], mc);
			if (mc->irq[1]) free_irq(mc->irq[1], mc);
			if (mc->irq[2]) free_irq(mc->irq[1], mc);
#endif
		kfree(mc);
	}
}
int modem_open (struct inode *inode, struct file *file)
{
	return 0;
}
int modem_close (struct inode *inode, struct file *file)
{
	return 0;
}
long modem_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	if(!global_mc)
		return -1;
	switch(cmd)
	{
		case MODEM_POWER_MAIN_CMD:
			modem_main_enumeration(global_mc);
			break;
		case MODEM_POWER_FLASH_CMD:
			modem_boot_enumeration(global_mc);
			break;
		case MODEM_POWER_REENUM_CMD:
			modem_renumeration(global_mc);
			break;
		case MODEM_POWER_OFF_CMD:
			modem_off(global_mc);
			break;
		case MODEM_POWER_ON_CMD:
			modem_on(global_mc);
			break;
		case MODEM_POWER_RESET_CMD:
			modem_reset(global_mc);
			break;		
	}
	return 0;
}

static ssize_t modem_read (struct file *filp, char __user * buffer, size_t count, loff_t * offset)
{
	int flag = 0;
	
	if(!global_mc)
		return -EFAULT;
	
	wait_event_interruptible(global_mc->read_wq, (global_mc->cp_flag & MODEM_EVENT_MASK));
	
	flag = global_mc->cp_flag  & MODEM_EVENT_MASK;
	if(copy_to_user(buffer, &flag, sizeof(flag)))
		return -EFAULT;
	pr_debug("%s: modem event = 0x%x\n", __func__, flag);
	return 1;
}
static ssize_t modem_write (struct file *filp, const char __user *buffer, size_t count, loff_t *offset)
{
	if(!global_mc)
		return -1;
	
	modem_err("%s:%s\n", __func__, buffer);

	if(count >= 4 && !strncmp(buffer, "renum", 4))
	{
		if (down_interruptible(&modem_lock)==0)
		{	
			modem_renumeration(global_mc);
			up(&modem_lock);
		}
	}
	if(count >= 4 && !strncmp(buffer, "main", 4))
	{
		if (down_interruptible(&modem_lock)==0)
		{		
			modem_main_enumeration(global_mc);
			up(&modem_lock);
		}

	}
	if(count >= 5 && !strncmp(buffer, "flash", 5))
	{
		if (down_interruptible(&modem_lock)==0)
		{		
			modem_boot_enumeration(global_mc);
			up(&modem_lock);
		}
	}
	if(count >= 3 && !strncmp(buffer, "off", 3))
	{
		if (down_interruptible(&modem_lock) == 0) {
			modem_off(global_mc);
			modem_crash_event(MODEM_EVENT_POWEROFF);
			up(&modem_lock);
		}
	}
	if(count >= 2 && !strncmp(buffer, "on", 2))
	{
		if (down_interruptible(&modem_lock)==0)
		{	
			modem_on(global_mc);
			up(&modem_lock);
		}
	}	
	if(count >= 5 && !strncmp(buffer, "reset", 5))
	{
		if (down_interruptible(&modem_lock)==0)
		{	
			modem_reset(global_mc);
			modem_crash_event(MODEM_EVENT_RESET);
			up(&modem_lock);
		}
	}	
	if(count >= 7 && !strncmp(buffer, "debug=0", 7))
	{
		modem_debug=0;
	}
	if(count >= 7 && !strncmp(buffer, "debug=1", 7))
	{
		modem_debug=1;
	}
	return count;
}
unsigned int modem_poll (struct file *filp, struct poll_table_struct *wait)
{
	u32 mask = 0;
	
	if(!global_mc)
		return -1;
		
	if (global_mc->boot_done && (global_mc->cp_flag & MODEM_CONNECT_FLAG))
		mask = POLLIN | POLLOUT | POLLRDNORM;
	else
		poll_wait(filp, &global_mc->conn_wq, wait);

	return mask;
}
static struct file_operations modem_file_ops = {
	.owner = THIS_MODULE,
	.open = modem_open,
	.release = modem_close,
	.read = modem_read,
	.write = modem_write,
	.unlocked_ioctl = modem_ioctl,
	.poll = modem_poll,
};

static struct miscdevice modem_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "modemctl",
	.fops = &modem_file_ops
};
#ifdef CONFIG_XMM6260_ACM_CLASS
static int modem_renumeration(struct modemctl *mc)
{
	wake_up_interruptible(&mc->read_wq);
	modem_wake_lock(mc);
	mc->enum_done = 0;
	mc->cp_flag =0;
	s5p_ehci_power(0);//remove modem devices
	s5p_ehci_power(1);//renumerate main program
	mc->enum_done = 1;	
	modem_wake_lock_timeout(mc, 5*HZ);
	return 0;	
}
static int modem_boot_enumeration(struct modemctl *mc)
{
	wake_up_interruptible(&mc->read_wq);
	modem_wake_lock(mc);
	mc->boot_done =0;
	mc->enum_done = 0;
	mc->cp_flag =0;
	s5p_ehci_power(0);//remove modem devices
	msleep(50);
	s5p_ehci_power(1);//enumerate bootrom flash program
	modem_on(mc);//power on modem
	mc->boot_done =1;
	mc->enum_done = 1;	
	modem_wake_lock_timeout(mc, 5*HZ);
	return 0;	
}
static int modem_main_enumeration(struct modemctl *mc)
{
	struct completion done;

	wake_up_interruptible(&mc->read_wq);
	modem_wake_lock(mc);
	mc->boot_done =0;
	mc->enum_done = 0;
	mc->cp_flag =0;
	modem_off(mc);//power off modem
	s5p_ehci_power(0);//power off hsic and remove modem devices
	modem_set_active_state(0);
	modem_on(mc);//power on modem
	init_completion(&done);
	mc->l2_done = &done;	
	wait_for_completion_timeout(&done, 20*HZ);//wait modem power on
	mc->l2_done = NULL;
	s5p_ehci_power(1);//power on hsic
	mc->boot_done =1;
	mc->enum_done = 1;
	modem_wake_lock_timeout(mc, 5*HZ);
	return 0;
}

#else
static int modem_renumeration(struct modemctl *mc){return 0;}
static int modem_boot_enumeration(struct modemctl *mc){return 0;}
static int modem_main_enumeration(struct modemctl *mc){return 0;}
#endif

#ifdef CONFIG_XMM6260_ACM_CLASS
static void __devinit async_modem_probe(void *data, async_cookie_t cookie)
{
	struct modemctl *mc = (struct modemctl *)data;
	pr_info("%s\n", __func__);
	mc->cp_flag = MODEM_RESET_FLAG|MODEM_INIT_ON_FLAG;
}

static void __devinit async_acm_smd_probe(void *data, async_cookie_t cookie)
{
	pr_info("%s\n", __func__);
	acm_init();
	smd_init();
}
#endif
static int __devinit modem_probe(struct platform_device *pdev)
{
	struct modem_platform_data *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct modemctl *mc;
#ifdef CONFIG_XMM6260_ACM_CLASS	
	int irq = 0;
#endif
	int error;

	if (!pdata) {
		dev_err(dev, "No platform data\n");
		return -EINVAL;
	}

	mc = kzalloc(sizeof(struct modemctl), GFP_KERNEL);
	if (!mc) {
		dev_err(dev, "Failed to allocate device\n");
		return -ENOMEM;
	}

	mc->pdata = pdata;
	mc->ops = &pdata->ops;
	mc->dev = dev;
	mc->boot_done =0;
	mc->enum_done = 0;
	mc->cp_flag =0;
	dev_set_drvdata(mc->dev, mc);

	modem_wake_lock_initial(mc);
	init_waitqueue_head(&mc->read_wq);
	init_waitqueue_head(&mc->conn_wq);
	modem_cfg(mc);
	
#ifdef CONFIG_XMM6260_ACM_CLASS
	irq = gpio_to_irq(pdata->gpio_ipc_host_wakeup);

	error = request_threaded_irq(irq, NULL, modem_resume_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"IPC_HOST_WAKEUP", mc);

	if (error) {
		dev_err(dev, "Resume thread Failed to allocate an interrupt(%d)\n", irq);
		goto fail;
	}
	mc->irq[0] = irq;
	
	irq = gpio_to_irq(pdata->gpio_cp_reset_int);
	error = request_threaded_irq(irq, NULL, modem_cpreset_irq,
			IRQF_TRIGGER_FALLING,
			"CP_RESET_INT", mc);
	if (error) {
		dev_err(dev, "Failed to allocate an interrupt(%d)\n", irq);
		goto fail;
	}
	mc->irq[1] = irq;
	INIT_DELAYED_WORK(&mc->sim_work, modem_sim_detect_worker);
	if(machine_is_m030())
	{
		irq = gpio_to_irq(pdata->gpio_sim_detect_int);
		error = request_threaded_irq(irq, NULL, modem_sim_detect_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"CP_SIM_DETECT_INT", mc);
		if (error) {
			dev_err(dev, "Failed to allocate an interrupt(%d)\n", irq);
			goto fail;
		}
		mc->irq[2] = irq;
	}
#endif
	mc->debug_cnt = 0;

	device_init_wakeup(&pdev->dev, pdata->wakeup);
	platform_set_drvdata(pdev, mc);
	global_mc = mc;

	error = misc_register(&modem_miscdev);
	if(error)
	{
		dev_err(dev, "Failed to register modem control device\n");
		goto fail;
	}

#ifdef CONFIG_XMM6260_ACM_CLASS
	async_schedule(async_acm_smd_probe, mc);
	async_schedule(async_modem_probe, mc);
#endif

	return 0;

fail:
	modem_wake_lock_destroy(mc);
	_free_all(mc);
	return error;
}

static int __devexit modem_remove(struct platform_device *pdev)
{
	struct modemctl *mc = platform_get_drvdata(pdev);

	flush_work(&mc->work.work);
	flush_work(&mc->cpreset_work);
	platform_set_drvdata(pdev, NULL);
	modem_wake_lock_destroy(mc);

	misc_deregister(&modem_miscdev);
	_free_all(mc);
	return 0;
}
#ifdef CONFIG_PM
static int modem_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct modemctl *mc = platform_get_drvdata(pdev);

	if( mc->cp_flag & MODEM_EVENT_MASK)
	{
		modem_wake_lock(mc);
		modem_wake_lock_timeout(mc, HZ*30);
		return -1;
	}
	if (device_may_wakeup(dev) && modem_is_on())
	{
#ifdef CONFIG_XMM6260_ACM_CLASS
		if(mc->irq[0]) enable_irq_wake(mc->irq[0]);
		if(mc->irq[1]) enable_irq_wake(mc->irq[1]);
		if(mc->irq[2]) enable_irq_wake(mc->irq[2]);
#endif
	}
	return 0;
}

static int modem_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
#ifdef CONFIG_XMM6260_ACM_CLASS	
	struct modemctl *mc = platform_get_drvdata(pdev);
#endif
	if (device_may_wakeup(dev) && modem_is_on())
	{
#ifdef CONFIG_XMM6260_ACM_CLASS		
		if(mc->irq[0]) disable_irq_wake(mc->irq[0]);
		if(mc->irq[1]) disable_irq_wake(mc->irq[1]);
		if(mc->irq[2]) disable_irq_wake(mc->irq[2]);
#endif
	}
	return 0;
}

static const struct dev_pm_ops modem_pm_ops = {
	.suspend	= modem_suspend,
	.resume		= modem_resume,
};
#endif

static struct platform_driver modem_driver = {
	.probe		= modem_probe,
	.remove		= __devexit_p(modem_remove),
	.driver		= {
		.name	= "xmm_modem",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &modem_pm_ops,
#endif
	},
};

static int __init modem_init(void)
{
	platform_driver_register(&modem_driver);

	return 0; 
}

static void __exit modem_exit(void)
{
#ifdef CONFIG_XMM6260_ACM_CLASS
	smd_exit();
	acm_exit();
#endif
	platform_driver_unregister(&modem_driver);
}
module_init(modem_init);
module_exit(modem_exit);
