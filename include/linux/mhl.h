/***********************************************************************************/
/*  Copyright (c) 2010, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
#ifndef __SII_9244_API_H__
#define __SII_9244_API_H__

#include <mach/usb-detect.h>

/* #define MHLDEBUG */

#ifdef MHLDEBUG
#define MHLPRINTK(fmt, args...) \
	printk(KERN_INFO"  [MHL] %s: " fmt, __func__ , ## args)
#else
#define MHLPRINTK(fmt, args...)
#endif

#define TX_DEBUG_PRINT(x) MHLPRINTK x

/*
* Support RCP function, need keypad driver support. Jiangshanbin
*/
#define  SUPPORT_MHL_RCP

typedef	bool	bool_t;

#define LOW                     0
#define HIGH                    1

// Generic Masks
//==============
#define _ZERO				   0x00
#define BIT0                   0x01
#define BIT1                   0x02
#define BIT2                   0x04
#define BIT3                   0x08
#define BIT4                   0x10
#define BIT5                   0x20
#define BIT6                   0x40
#define BIT7                   0x80

/* Timers - Target system uses these timers*/
#define ELAPSED_TIMER		0xFF
#define ELAPSED_TIMER1		0xFE

enum timerid {
    TIMER_FOR_MONITORING= 0,		// haltimerwait() is implemented using busy waiting
    TIMER_POLLING,		// Reserved for main polling loop
    TIMER_2,			// Available
    TIMER_SWWA_WRITE_STAT,
    TIMER_TO_DO_RSEN_CHK,
    TIMER_TO_DO_RSEN_DEGLITCH,
    TIMER_COUNT			// MUST BE LAST!!!!
};

/*
* This is the time in milliseconds we poll what we poll.
*/
#define MONITORING_PERIOD		3/*->13->50*/

#define SiI_DEVICE_ID			0xB0

#define TX_HW_RESET_PERIOD		10

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Debug Definitions
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define DISABLE 0x00
#define ENABLE  0xFF

extern bool_t	vbuspowerstate;
extern atomic_t g_mhlcablestatus;
/*------------------------------------------------------------------------------
* Array of timer values
------------------------------------------------------------------------------*/
/* lt
extern uint16_t g_timercounters[TIMER_COUNT];

extern uint16_t g_timerelapsed;
extern uint16_t g_elapsedtick;
extern uint16_t g_timerelapsedgranularity;

extern uint16_t g_timerelapsed1;
extern uint16_t g_elapsedtick1;
extern uint16_t g_timerelapsedgranularity1;
*/

void haltimerset(uint8_t index, uint16_t m_sec);
uint8_t haltimerexpired(uint8_t index);
void haltimerwait(uint16_t m_sec);
uint16_t haltimerelapsed(uint8_t index);

/*
*
* appmhltxdisableinterrupts
*
* This function or macro is invoked from MhlTx driver to secure the processor
* before entering into a critical region.
*
* Application module must provide this function.
*/
//lt
//extern void appmhltxdisableinterrupts(void);

/*
*
* appmhltxrestoreinterrupts
*
* This function or macro is invoked from MhlTx driver to secure the processor
* before entering into a critical region.
*
* Application module must provide this function.
*/
//lt
//extern void appmhltxrestoreinterrupts(void);

/*
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
*/
extern void appvbuscontrol(bool poweron);

/*
	PM check the cable state.
*/
extern void s3c_cable_check_status(int flag);
/*
	MHL check cable state.
*/
extern bool mhl_cable_status(void);

/*
	I2C client for sii9244
*/

/* lt
extern struct i2c_client *mhl_page0;
extern struct i2c_client *mhl_page1;
extern struct i2c_client *mhl_page2;
extern struct i2c_client *mhl_cbus;
*/

uint8_t sii_9224_i2c_readbyte(uint8_t slaveaddr, uint8_t regaddr);
void sii_9224_i2c_writebyte(uint8_t slaveaddr, uint8_t regaddr, uint8_t data);

struct mhl_platform_data {
	u32 mhl_wake_pin;
	u32 mhl_reset_pin;
	u32 mhl_irq_pin;
	u32 eint;
	u32 mhl_usb_irq_pin;

	struct regulator *mhl_logic_regulator;

	int (*mhl_power_on)(struct mhl_platform_data *, int);
	int (*reset)(struct mhl_platform_data *);
};

extern int mhl_usb_notifier_event(struct notifier_block *this, unsigned long event, void *ptr);

#endif  /* __SII_9244_API_H__*/
