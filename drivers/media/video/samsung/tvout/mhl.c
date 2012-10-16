/* linux/drivers/media/video/samsung/tv20/mhl.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Core file for MHL driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/async.h>
#include <linux/mfd/max8997.h>
#include <linux/slab.h>

#include <plat/gpio-cfg.h>

#include <mach/regs-gpio.h>

#include <linux/mhl.h>
#include <linux/sii_9244_driver.h>

#undef SUPPORT_MHL_RCP

#ifdef SUPPORT_MHL_RCP
#endif
/* #define CHECK_I2C_WRITE */
#define USE_I2C_BUS

#define	APP_DEMO_RCP_SEND_KEY_CODE 0x41


extern void max8997_set_buck6_pwm_mode(int mode);

bool_t	vbuspowerstate = true;		// false: 0 = vbus output on; true: 1 = vbus output off;
struct i2c_client *mhl_page0;
struct i2c_client *mhl_page1;
struct i2c_client *mhl_page2;
struct i2c_client *mhl_cbus;

static void work_queue(void);
static DECLARE_WORK(mhl_work, (void *)work_queue);

/* sii9244 timer count */
uint16_t g_timercounters[TIMER_COUNT];

uint16_t g_timerelapsed;
uint16_t g_elapsedtick;
uint16_t g_timerelapsedgranularity;

uint16_t g_timerelapsed1;
uint16_t g_elapsedtick1;
uint16_t g_timerelapsedgranularity1;

static struct timer_list g_mhl_timer;

atomic_t g_mhlcablestatus;
bool work_initial;

struct mhl_platform_data *mhl_pdata = NULL;

/* I2C functions used by the driver. */
uint8_t sii_9224_i2c_readbyte(uint8_t slaveaddr, uint8_t regaddr)
{
	int  readdata = 0;

	switch (slaveaddr)
	{
		case PAGE_0_0X72:
			readdata = i2c_smbus_read_byte_data(mhl_page0, regaddr);
			break;
		case PAGE_1_0X7A:
			readdata = i2c_smbus_read_byte_data(mhl_page1, regaddr);
			break;
		case PAGE_2_0X92:
			readdata = i2c_smbus_read_byte_data(mhl_page2, regaddr);
			break;
		case PAGE_CBUS_0XC8:
			readdata = i2c_smbus_read_byte_data(mhl_cbus, regaddr);
			break;
	}
	if(readdata < 0)
		return 0;

	return readdata;
}

void sii_9224_i2c_writebyte(uint8_t slaveaddr, uint8_t regaddr, uint8_t data)
{
	switch (slaveaddr)
	{
		case PAGE_0_0X72:
			i2c_smbus_write_byte_data(mhl_page0, regaddr, data);
			break;
		case PAGE_1_0X7A:
			i2c_smbus_write_byte_data(mhl_page1, regaddr, data);
			break;
		case PAGE_2_0X92:
			i2c_smbus_write_byte_data(mhl_page2, regaddr, data);
			break;
		case PAGE_CBUS_0XC8:
			i2c_smbus_write_byte_data(mhl_cbus, regaddr, data);
			break;
	}
}

static const bool match_id(const struct i2c_device_id *id,
				const struct i2c_client *client)
{
	if (strcmp(client->name, id->name) == 0)
		return true;

	return false;
}

static int mhl_read_id(void)
{
	u8 dev_idl = 0, dev_idh = 0;

	dev_idl = sii_9224_i2c_readbyte(PAGE_0_0X72, 0x02);
	dev_idh = sii_9224_i2c_readbyte(PAGE_0_0X72, 0x03);

	printk("dev_id is: [0x%x : 0x%x]", dev_idh, dev_idl);
	if (dev_idl == 0x34 && dev_idh == 0x92) {
		dev_info(&mhl_page0->dev, "sii-9244 found"
			": dev_id is: [0x%x : 0x%x]", dev_idh, dev_idl);
		return 0;
	} else {
		dev_info(&mhl_page0->dev, "sii-9244 found"
			": dev_id is: [0x%x : 0x%x]", dev_idh, dev_idl);
		return 0;
	}

	dev_err(&mhl_page0->dev, "error device"
		": dev_id is: [0x%x : 0x%x]", dev_idh, dev_idl);
	return -EIO;
}

static void timertickhandler(unsigned long __data)
{
	uint8_t i;
	static u32 count;
	for (i = 0; i < TIMER_COUNT; i++) {
		if (g_timercounters[i] > 0)
			g_timercounters[i]--;
	}

	g_elapsedtick++;
	if (g_elapsedtick == g_timerelapsedgranularity) {
		g_timerelapsed++;
		g_elapsedtick = 0;
	}

	g_elapsedtick1++;
	if (g_elapsedtick1 == g_timerelapsedgranularity1) {
		g_timerelapsed1++;
		g_elapsedtick1 = 0;
	}

	count++;

	if (!(count % 3))
		schedule_work(&mhl_work);


	g_mhl_timer.expires = jiffies + 4;
	add_timer(&g_mhl_timer);
}

// Function: haltimerinit
// Description:
void haltimerinit(void)
{
	uint8_t i;

	/*initializer timer counters in array*/
	for (i = 0; i < TIMER_COUNT; i++)
		g_timercounters[i] = 0;

	g_timerelapsed = 0;
	g_timerelapsed1 = 0;
	g_elapsedtick = 0;
	g_elapsedtick1 = 0;
	g_timerelapsedgranularity = 0;
	g_timerelapsedgranularity1 = 0;
}

// Function: haltimerset
// Description:
void haltimerset(uint8_t index, uint16_t m_sec)
{
    switch (index)
    {
	case ELAPSED_TIMER:
		g_timerelapsedgranularity = m_sec;
		g_timerelapsed = 0;
		g_elapsedtick = 0;
		break;

	case ELAPSED_TIMER1:
		g_timerelapsedgranularity1 = m_sec;
		g_timerelapsed1 = 0;
		g_elapsedtick1 = 0;
		break;
	default:
		g_timercounters[index] = m_sec;
		break;
    }
}

// Function:    haltimerwait
// Description: Waits for the specified number of milliseconds, using timer 0.
void haltimerwait(uint16_t ms)
{
	mdelay(ms);
}

// Function:    haltimerexpired
// Description: Returns > 0 if specified timer has expired.
uint8_t haltimerexpired(uint8_t timer)
{
	if (timer < TIMER_COUNT)
		return (g_timercounters[timer] == 0);

	return 0;
}

// Function:    haltimerelapsed
// Description: Returns current timer tick.  Rollover depends on the
             // granularity specified in the SetTimer() call.
uint16_t haltimerelapsed(uint8_t index)
{
	uint16_t elapsedTime;

	if (index == ELAPSED_TIMER)
		elapsedTime = g_timerelapsed;
	else
		elapsedTime = g_timerelapsed1;

	return elapsedTime;
}

#ifdef SUPPORT_MHL_RCP
static struct input_dev *kpdevice;
#endif

void rcp_keysend(unsigned int keycode)
{
#ifdef SUPPORT_MHL_RCP
	if(kpdevice == NULL)
		kpdevice = g_keypad_input;

	input_report_key(kpdevice, keycode, 1);
	input_sync(kpdevice);
	input_report_key(kpdevice, keycode, 0);
	input_sync(kpdevice);
#endif
}

// apprcpdemo
// This function is supposed to provide a demo code to elicit how to call RCP
// API function.
static void apprcpdemo(uint8_t event, uint8_t eventparameter)
{
	uint8_t		rcpkeycode;

	/*MHLPRINTK("App: got event = %02x, eventparameter = %02x\n",
		(int)event, (int)eventparameter);*/

	switch (event) {
	case MHL_TX_EVENT_DISCONNECTION:
		MHLPRINTK("App: Got event = MHL_TX_EVENT_DISCONNECTION\n");
		break;

	case MHL_TX_EVENT_CONNECTION:
		MHLPRINTK("App: Got event = MHL_TX_EVENT_CONNECTION\n");
		break;

	case MHL_TX_EVENT_RCP_READY:
#if 0 /* removed demo RCP key send in here. by oscar 20101215 */
	/* Demo RCP key code PLAY*/
	rcpKeyCode = APP_DEMO_RCP_SEND_KEY_CODE;

	MHLPRINTK("App: Got event = MHL_TX_EVENT_RCP_READY... \
		Sending RCP (%02X)\n", (int) rcpKeyCode);

	if ((0 == (MHL_FEATURE_RCP_SUPPORT & eventParameter)))
		MHLPRINTK("App: Peer does NOT support RCP\n");

	if ((0 == (MHL_FEATURE_RAP_SUPPORT & eventParameter)))
		MHLPRINTK("App: Peer does NOT support RAP\n");

	if ((0 == (MHL_FEATURE_SP_SUPPORT & eventParameter)))
		MHLPRINTK("App: Peer does NOT support WRITE_BURST\n");

	/*
	 * If RCP engine is ready, send one code
	*/
	if (SiiMhlTxRcpSend(rcpKeyCode))
		MHLPRINTK("App: SiiMhlTxRcpSend (%02X)\n", (int) rcpKeyCode);
	else
		MHLPRINTK("App: SiiMhlTxRcpSend (%02X) Returned Failure.\n",
			(int) rcpKeyCode);
#endif
		break;
	case MHL_TX_EVENT_RCP_RECEIVED:
		/*
		* Check if we got an RCP. Application can perform the operation
		* and send RCPK or RCPE. For now, we send the RCPK
		*/
		rcpkeycode = eventparameter;
		MHLPRINTK("App1: Received an RCP key code = %02X\n",
			(int)rcpkeycode);

		/* Added RCP key printf and interface with UI.
			by oscar 20101217*/
		switch (rcpkeycode) {
		case MHL_RCP_CMD_SELECT:
			rcp_keysend(33);
			TX_DEBUG_PRINT(("\nSelect received\n\n"));
			break;
		case MHL_RCP_CMD_UP:
			rcp_keysend(25);
			TX_DEBUG_PRINT(("\nUp received\n\n"));
			break;
		case MHL_RCP_CMD_DOWN:
			rcp_keysend(41);
			TX_DEBUG_PRINT(("\nDown received\n\n"));
			break;
		case MHL_RCP_CMD_LEFT:
			rcp_keysend(57);
			TX_DEBUG_PRINT(("\nLeft received\n\n"));
			break;
		case MHL_RCP_CMD_RIGHT:
			rcp_keysend(49);
			TX_DEBUG_PRINT(("\nRight received\n\n"));
			break;
		case MHL_RCP_CMD_ROOT_MENU:
			TX_DEBUG_PRINT(("\nRoot Menu received\n\n"));
			break;
		case MHL_RCP_CMD_EXIT:
			rcp_keysend(34);
			TX_DEBUG_PRINT(("\nExit received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_0:
			TX_DEBUG_PRINT(("\nNumber 0 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_1:
			TX_DEBUG_PRINT(("\nNumber 1 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_2:
			TX_DEBUG_PRINT(("\nNumber 2 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_3:
			TX_DEBUG_PRINT(("\nNumber 3 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_4:
			TX_DEBUG_PRINT(("\nNumber 4 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_5:
			TX_DEBUG_PRINT(("\nNumber 5 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_6:
			TX_DEBUG_PRINT(("\nNumber 6 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_7:
			TX_DEBUG_PRINT(("\nNumber 7 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_8:
			TX_DEBUG_PRINT(("\nNumber 8 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_9:
			TX_DEBUG_PRINT(("\nNumber 9 received\n\n"));
			break;
		case MHL_RCP_CMD_DOT:
			TX_DEBUG_PRINT(("\nDot received\n\n"));
			break;
		case MHL_RCP_CMD_ENTER:
			rcp_keysend(33);
			TX_DEBUG_PRINT(("\nEnter received\n\n"));
			break;
		case MHL_RCP_CMD_CLEAR:
			TX_DEBUG_PRINT(("\nClear received\n\n"));
			break;
		case MHL_RCP_CMD_SOUND_SELECT:
			TX_DEBUG_PRINT(("\nSound Select received\n\n"));
			break;
		case MHL_RCP_CMD_PLAY:
			TX_DEBUG_PRINT(("\nPlay received\n\n"));
			break;
		case MHL_RCP_CMD_PAUSE:
			TX_DEBUG_PRINT(("\nPause received\n\n"));
			break;
		case MHL_RCP_CMD_STOP:
			TX_DEBUG_PRINT(("\nStop received\n\n"));
			break;
		case MHL_RCP_CMD_FAST_FWD:
			TX_DEBUG_PRINT(("\nFastfwd received\n\n"));
			break;
		case MHL_RCP_CMD_REWIND:
			TX_DEBUG_PRINT(("\nRewind received\n\n"));
			break;
		case MHL_RCP_CMD_EJECT:
			TX_DEBUG_PRINT(("\nEject received\n\n"));
			break;
		case MHL_RCP_CMD_FWD:
			TX_DEBUG_PRINT(("\nForward received\n\n"));
			break;
		case MHL_RCP_CMD_BKWD:
			TX_DEBUG_PRINT(("\nBackward received\n\n"));
			break;
		case MHL_RCP_CMD_PLAY_FUNC:
			TX_DEBUG_PRINT(("\nPlay Function received\n\n"));
			break;
		case MHL_RCP_CMD_PAUSE_PLAY_FUNC:
			TX_DEBUG_PRINT(("\nPause_Play Function received\n\n"));
			break;
		case MHL_RCP_CMD_STOP_FUNC:
			TX_DEBUG_PRINT(("\nStop Function received\n\n"));
			break;
		case MHL_RCP_CMD_F1:
			TX_DEBUG_PRINT(("\nF1 received\n\n"));
			break;
		case MHL_RCP_CMD_F2:
			TX_DEBUG_PRINT(("\nF2 received\n\n"));
			break;
		case MHL_RCP_CMD_F3:
			TX_DEBUG_PRINT(("\nF3 received\n\n"));
			break;
		case MHL_RCP_CMD_F4:
			TX_DEBUG_PRINT(("\nF4 received\n\n"));
			break;
		case MHL_RCP_CMD_F5:
			TX_DEBUG_PRINT(("\nF5 received\n\n"));
			break;
		default:
			break;
		}

		siimhltxrcpksend(rcpkeycode);
		break;

	case MHL_TX_EVENT_RCPK_RECEIVED:
		// MHLPRINTK("App: Received an RCPK = %02X\n", (int)eventParameter);
		break;

	case MHL_TX_EVENT_RCPE_RECEIVED:
		// MHLPRINTK("App: Received an RCPE = %02X\n", (int)eventParameter);
		break;

	default:
		break;
	}
}

/*--------------------------------------------------------------------------
*
* appvbuscontrol
*
* This function or macro is invoked from MhlTx driver to ask application to
* control the VBUS power. If powerOn is sent as non-zero, one should assume
* peer does not need power so quickly remove VBUS power.
*
* if value of "powerOn" is 0, then application must turn the VBUS power on
* within 50ms of this call to meet MHL specs timing.
*
* Application module must provide this function.
*-------------------------------------------------------------------------*/
void appvbuscontrol(bool_t poweron)
{
	return ;
}

static void work_queue(void)
{
	bool_t interruptdriven;
	uint8_t pollintervalms;
	uint8_t event;
	uint8_t eventparameter;

	/*Initialize host microcontroller and the timer*/
	/*halinitcpu();*/

	if (!work_initial) {
		haltimerinit();
		haltimerset(TIMER_POLLING, MONITORING_PERIOD);

		printk(KERN_INFO"\n============================================\n");
		printk(KERN_INFO"Copyright 2010 Silicon Image\n");
		printk(KERN_INFO"SiI-9244 Starter Kit Firmware Version 1.00.87\n");
		printk(KERN_INFO"============================================\n");

		/*
		* Initialize the registers as required. Setup firmware vars.
		*/
		SiiMhlTxInitialize(interruptdriven = false,
				pollintervalms = MONITORING_PERIOD);

		work_initial = true;

		g_mhl_timer.expires = jiffies + 1;
		add_timer(&g_mhl_timer);
	}

	/* Event loop */
	SiiMhlGetEvents(&event, &eventparameter);

	if (MHL_TX_EVENT_NONE != event)
		apprcpdemo(event, eventparameter);
}

unsigned int int_count;
static irqreturn_t
s3c_mhl_interrupt(int irq, void *dev_id)
{
	MHLPRINTK("%d  Interrupt occure\n", irq);
	MHLPRINTK("XEINT10  Interrupt occure\n");

	if (work_initial) {
		int_count += 10;
		if (int_count > 20) {
			int_count = 20;
		}
	} else {
		MHLPRINTK("%d  Interrupt occure\n", irq);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static struct i2c_device_id mhl_idtable[] = {
	{"mhl_page0", 0},
	{"mhl_page1", 0},
	{"mhl_page2", 0},
	{"mhl_cbus", 0},
};

bool mhl_cable_status()
{
	if (work_initial) {
		if (atomic_read(&g_mhlcablestatus) == 0)
			return false;
		else
			return true;
	} else
		return false;
}
EXPORT_SYMBOL(mhl_cable_status);

static struct notifier_block mhl_usb_notifier = {
	.notifier_call = mhl_usb_notifier_event,
};

static int real_mhl_probe(struct i2c_client *client)
{
	bool_t interruptdriven;
	uint8_t pollintervalms;
	int ret;

	work_initial = false;
	/* get platform_data */
	mhl_pdata = client->dev.platform_data;
	if (mhl_pdata == NULL) {
		MHLPRINTK("get platform_data failed");
		ret = -ENODEV;
		goto exit;
	}

	/* mhl power on */
	if (NULL != mhl_pdata->mhl_power_on) {
		ret = mhl_pdata->mhl_power_on(mhl_pdata, true);
		if (ret <0) {
			MHLPRINTK("mhl_power_on failed");
			goto exit;
		}
	}

	/* mhl reset */
	if (NULL != mhl_pdata->reset) {
		ret = mhl_pdata->reset(mhl_pdata);
		if (ret <0) {
			MHLPRINTK("mhl_reset failed");
			goto exit;
		}
	}

	/* mhl read id */
	if (mhl_read_id() != 0) {
		MHLPRINTK("mhl_read_id failed");
		ret = -ENXIO;
		goto exit;
	}

	haltimerinit();
	haltimerset(TIMER_POLLING, MONITORING_PERIOD);

	printk(KERN_INFO"============================================\n");
	printk(KERN_INFO"Copyright 2010 Silicon Image\n");
	printk(KERN_INFO"SiI-9244 Linux Driver V1.22\n");
	printk(KERN_INFO"============================================\n");

	/*
	 * Initialize the registers as required.
	 * Setup firmware vars.
	*/
	SiiMhlTxInitialize(interruptdriven = false,
			pollintervalms = MONITORING_PERIOD);

	work_initial = true;
	atomic_set(&g_mhlcablestatus, 0);

	/* sii9244 interrupt initial&setup */
	ret = gpio_request(mhl_pdata->mhl_irq_pin, NULL);
	if (ret) {
		MHLPRINTK("gpio_request failed");
		goto exit;
	}
	s3c_gpio_cfgpin(mhl_pdata->mhl_irq_pin, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(mhl_pdata->mhl_irq_pin, S3C_GPIO_PULL_UP);
	ret = request_irq(mhl_pdata->eint, s3c_mhl_interrupt, IRQF_TRIGGER_FALLING, "s3c MHL", NULL);
	if(ret) {
		MHLPRINTK("request_irq failed");
		goto exit;
	}

	/* sii9244 timer & workqueue initial */
	init_timer(&g_mhl_timer);
	g_mhl_timer.function = timertickhandler;
	g_mhl_timer.expires = jiffies + 5 * HZ;
	add_timer(&g_mhl_timer);

	register_mx_usb_notifier(&mhl_usb_notifier);
exit:
	return ret;
}

/* i2c client ftn. */
static int mhl_probe(struct i2c_client *client,
			const struct i2c_device_id *dev_id)
{
	int ret = 0;

	MHLPRINTK("probe start");

//	if (NULL == client->dev.platform_data) {
//		dev_err(&client->dev, "platform data is NULL. exiting.\n");
//		ret = -ENODEV;
//	}

	// max8997_set_buck6_pwm_mode(0);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "mhl sii9244 probe: check_functionality failed.\n");
		ret = -ENODEV;
	}

	if (match_id(&mhl_idtable[0], client))
		mhl_page0 = client;
	else if (match_id(&mhl_idtable[1], client))
		mhl_page1 = client;
	else if (match_id(&mhl_idtable[2], client))
		mhl_page2 = client;
	else if (match_id(&mhl_idtable[3], client))
		mhl_cbus = client;
	else {
		dev_err(&client->adapter->dev,
			"invalid i2c adapter: can not found dev_id matched\n");
		ret = -EIO;
	}

	dev_info(&client->adapter->dev, "attached %s "
		"into i2c adapter successfully\n", dev_id->name);

	if (mhl_page0 != NULL && mhl_page1 != NULL
		&& mhl_page2 != NULL && mhl_cbus != NULL) {
		real_mhl_probe(mhl_cbus);
	}

	return ret;
}

static int mhl_remove(struct i2c_client *client)
{
	dev_info(&client->adapter->dev, "detached s5p_mhl "
		"from i2c adapter successfully\n");

	if (mhl_page0 == client)
		mhl_page0 = NULL;
	else if (mhl_page1 == client)
		mhl_page1 = NULL;
	else if (mhl_page2 == client)
		mhl_page2 = NULL;
	else if (mhl_cbus == client)
		mhl_cbus = NULL;

	if (mhl_page0 == NULL && mhl_page1 == NULL
		&& mhl_page2 == NULL && mhl_cbus == NULL) {
		mhl_pdata->reset(mhl_pdata);
	}

	mhl_pdata->mhl_power_on(mhl_pdata, false);

	mhl_pdata = NULL;
	return 0;
}

static int mhl_suspend(struct i2c_client *cl, pm_message_t mesg)
{
	pr_debug("%s \n", __func__);
	return 0;
}

static int mhl_resume(struct i2c_client *cl)
{
	pr_debug("%s \n", __func__);
	return 0;
}

MODULE_DEVICE_TABLE(i2c, mhl_idtable);

static struct i2c_driver mhl_driver = {
	.driver = {
		.name = "s5p_mhl",
	},
	.id_table	= mhl_idtable,
	.probe		= mhl_probe,
	.remove		= mhl_remove,

	.suspend	= mhl_suspend,
	.resume		= mhl_resume,
};

static int __init mhl_init(void)
{
	return i2c_add_driver(&mhl_driver);
}

static void __exit mhl_exit(void)
{
	i2c_del_driver(&mhl_driver);
}

MODULE_AUTHOR("Jiang Shanbin <sb.jiang@samsung.com>");
MODULE_DESCRIPTION("Driver for SiI-9244 devices");
MODULE_LICENSE("GPL");

module_init(mhl_init);
module_exit(mhl_exit);
