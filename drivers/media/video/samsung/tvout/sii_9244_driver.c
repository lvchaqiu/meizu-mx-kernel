/***********************************************************************************/
/*  Copyright (c) 2002-2010, Silicon Image, Inc.  All rights reserved.             */
/*  No part of this work may be reproduced, modified, distributed, transmitted,    */
/*  transcribed, or translated into any language or computer format, in any form   */
/*  or by any means without written permission of: Silicon Image, Inc.,            */
/*  1060 East Arques Avenue, Sunnyvale, California 94085                           */
/***********************************************************************************/
#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
//#include	"hal_mcu.h"
#include <linux/mhl.h>
#include <linux/sii_9244_driver.h>
//#include	"sii_9244_api.h"

///////////////////////////////////////////////////////////////////////////////
//
// Store global config info here. This is shared by the driver.
//
//
//
// structure to hold operating information of MhlTx component
//
mhlTx_config_t	mhlTxConfig={0};

///////////////////////////////////////////////////////////////////////////////
#define SILICON_IMAGE_ADOPTER_ID 322
#define TRANSCODER_DEVICE_ID 0x9244
//
// Software power states are a little bit different than the hardware states but
// a close resemblance exists.
//
// D3 matches well with hardware state. In this state we receive RGND interrupts
// to initiate wake up pulse and device discovery
//
// Chip wakes up in D2 mode and interrupts MCU for RGND. Firmware changes the 9244
// into D0 mode and sets its own operation mode as POWER_STATE_D0_NO_MHL because
// MHL connection has not yet completed.
//
// For all practical reasons, firmware knows only two states of hardware - D0 and D3.
//
// We move from POWER_STATE_D0_NO_MHL to POWER_STATE_D0_MHL only when MHL connection
// is established.
/*
//
//                             S T A T E     T R A N S I T I O N S
//
//
//                    POWER_STATE_D3                      POWER_STATE_D0_NO_MHL
//                   /--------------\                        /------------\
//                  /                \                      /     D0       \
//                 /                  \                \   /                \
//                /   DDDDDD  333333   \     RGND       \ /   NN  N  OOO     \
//                |   D     D     33   |-----------------|    N N N O   O     |
//                |   D     D  3333    |      IRQ       /|    N  NN  OOO      |
//                \   D     D      33  /               /  \                  /
//                 \  DDDDDD  333333  /                    \   CONNECTION   /
//                  \                /\                     /\             /
//                   \--------------/  \  TIMEOUT/         /  -------------
//                         /|\          \-------/---------/        ||
//                        / | \            500ms\                  ||
//                          |                     \                ||
//                          |  RSEN_LOW                            || MHL_EST
//                           \ (STATUS)                            ||  (IRQ)
//                            \                                    ||
//                             \      /------------\              //
//                              \    /              \            //
//                               \  /                \          //
//                                \/                  \ /      //
//                                 |    CONNECTED     |/======//
//                                 |                  |\======/
//                                 \   (OPERATIONAL)  / \
//                                  \                /
//                                   \              /
//                                    \-----------/
//                                   POWER_STATE_D0_MHL
//
//
//
*/
#define	POWER_STATE_D3		3
#define	POWER_STATE_D0_NO_MHL	2
#define	POWER_STATE_D0_MHL	0
#define	POWER_STATE_FIRST_INIT	0xFF

//
// To remember the current power state.
//
static	uint8_t	fwPowerState = POWER_STATE_FIRST_INIT;

//
// This flag is set to true as soon as a INT1 RSEN CHANGE interrupt arrives and
// a deglitch timer is started.
//
// We will not get any further interrupt so the RSEN LOW status needs to be polled
// until this timer expires.
//
static	bool_t	deglitchingRsenNow = false;

//
// To serialize the RCP commands posted to the CBUS engine, this flag
// is maintained by the function SiiMhlTxDrvSendCbusCommand()
//
static	bool_t	mscCmdInProgress;	// false when it is okay to send a new command
//
// Preserve Downstream HPD status
//
static	uint8_t	dsHpdStatus = 0;

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadBytePage0 ()
//
// PURPOSE		:	Read the value from a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be read.
//
// RETURNS		:	The value read from the Page0 register.
//
uint8_t ReadBytePage0 (uint8_t Offset)
{
	return sii_9224_i2c_readbyte(PAGE_0_0X72, Offset);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteBytePage0 ()
//
// PURPOSE		:	Write a value to a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be written.
//					Data	-	the value to be written.
//
void WriteBytePage0 (uint8_t Offset, uint8_t Data)
{
	sii_9224_i2c_writebyte(PAGE_0_0X72, Offset, Data);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWritePage0 ()
//
// PURPOSE		:	Set or clear individual bits in a Page0 register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be modified.
//					Mask	-	"1" for each Page0 register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
void ReadModifyWritePage0(uint8_t Offset, uint8_t Mask, uint8_t Data)
{
	uint8_t Temp;

	Temp = ReadBytePage0(Offset);		// Read the current value of the register.
	Temp &= ~Mask;					// Clear the bits that are set in Mask.
	Temp |= (Data & Mask);			// OR in new value. Apply Mask to Value for safety.
	WriteBytePage0(Offset, Temp);		// Write new value back to register.
}

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadByteCBUS ()
//
// PURPOSE		:	Read the value from a CBUS register.
//
// INPUT PARAMS	:	Offset - the offset of the CBUS register to be read.
//
// RETURNS		:	The value read from the CBUS register.
//
uint8_t ReadByteCBUS (uint8_t Offset)
{
	return sii_9224_i2c_readbyte(PAGE_CBUS_0XC8, Offset);
}

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteByteCBUS ()
//
// PURPOSE		:	Write a value to a CBUS register.
//
// INPUT PARAMS	:	Offset	-	the offset of the Page0 register to be written.
//					Data	-	the value to be written.
//
void WriteByteCBUS(uint8_t Offset, uint8_t Data)
{
	sii_9224_i2c_writebyte(PAGE_CBUS_0XC8, Offset, Data);
}

//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWriteCBUS ()
//
// PURPOSE		:	Set or clear individual bits on CBUS page.
//
// INPUT PARAMS	:	Offset	-	the offset of the CBUS register to be modified.
//					Mask	-	"1" for each CBUS register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
void ReadModifyWriteCBUS(uint8_t Offset, uint8_t Mask, uint8_t Value)
{
    uint8_t Temp;

    Temp = ReadByteCBUS(Offset);
    Temp &= ~Mask;
    Temp |= (Value & Mask);
    WriteByteCBUS(Offset, Temp);
}


#define	I2C_READ_MODIFY_WRITE(saddr,offset,mask)	sii_9224_i2c_writebyte(saddr, offset, sii_9224_i2c_readbyte(saddr, offset) | (mask));
#define ReadModifyWriteByteCBUS(offset,andMask,orMask)  WriteByteCBUS(offset,(ReadByteCBUS(offset)&andMask) | orMask)

#define	SET_BIT(saddr,offset,bitnumber)		I2C_READ_MODIFY_WRITE(saddr,offset, (1<<bitnumber))
#define	CLR_BIT(saddr,offset,bitnumber)		sii_9224_i2c_writebyte(saddr, offset, sii_9224_i2c_readbyte(saddr, offset) & ~(1<<bitnumber))
//
// 90[0] = Enable / Disable MHL Discovery on MHL link
//
#define	DISABLE_DISCOVERY				CLR_BIT(PAGE_0_0X72, 0x90, 0);
#define	ENABLE_DISCOVERY				SET_BIT(PAGE_0_0X72, 0x90, 0);

#define STROBE_POWER_ON                    	CLR_BIT(PAGE_0_0X72, 0x90, 1);
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(only if necessary)
//
#define	INTR_4_DESIRED_MASK				(BIT0 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_INTR_4_INTERRUPTS		sii_9224_i2c_writebyte(PAGE_0_0X72, 0x78, INTR_4_DESIRED_MASK)
#define	MASK_INTR_4_INTERRUPTS			sii_9224_i2c_writebyte(PAGE_0_0X72, 0x78, 0x00)

//	Look for interrupts on INTR_2 (Register 0x72)
//		7 = bcap done			(ignore)
//		6 = parity error		(ignore)
//		5 = ENC_EN changed		(ignore)
//		4 = no premable			(ignore)
//		3 = ACR CTS changed		(ignore)
//		2 = ACR Pkt Ovrwrt		(ignore)
//		1 = TCLK_STBL changed	(interested)
//		0 = Vsync				(ignore)
#define	INTR_2_DESIRED_MASK				(BIT1)
#define	UNMASK_INTR_2_INTERRUPTS		sii_9224_i2c_writebyte(PAGE_0_0X72, 0x76, INTR_2_DESIRED_MASK)
#define	MASK_INTR_2_INTERRUPTS			sii_9224_i2c_writebyte(PAGE_0_0X72, 0x76, 0x00)

//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)

#define	INTR_1_DESIRED_MASK				(BIT5 | BIT6)
#define	UNMASK_INTR_1_INTERRUPTS		sii_9224_i2c_writebyte(PAGE_0_0X72, 0x75, INTR_1_DESIRED_MASK)
#define	MASK_INTR_1_INTERRUPTS			sii_9224_i2c_writebyte(PAGE_0_0X72, 0x75, 0x00)

//	Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd				(reserved)
#define	INTR_CBUS1_DESIRED_MASK			(BIT2 | BIT3 | BIT4 | BIT5 | BIT6)
#define	UNMASK_CBUS1_INTERRUPTS			WriteByteCBUS(0x09, INTR_CBUS1_DESIRED_MASK)
#define	MASK_CBUS1_INTERRUPTS			WriteByteCBUS(0x09, 0x00)

//	Look for interrupts on CBUS:CBUS_INTR_STATUS2 [0xC8:0x1E]
//		7 = RSVD			(reserved)
//		6 = RSVD			(reserved)
//		5 = RSVD			(reserved)
//		4 = RSVD			(reserved)
//		3 = WRT_STAT_RECD	(interested)
//		2 = SET_INT_RECD	(interested)
//		1 = RSVD			(reserved)
//		0 = WRT_BURST_RECD (interested)
#define	INTR_CBUS2_DESIRED_MASK			(BIT0 | BIT2 | BIT3)
#define	UNMASK_CBUS2_INTERRUPTS			WriteByteCBUS(0x1F, INTR_CBUS2_DESIRED_MASK)
#define	MASK_CBUS2_INTERRUPTS			WriteByteCBUS(0x1F, 0x00)



#define	APPLY_PLL_RECOVERY

static bool mhl_processed = false;
static bool dock_inserted = false;

////////////////////////////////////////////////////////////////////
//
// L O C A L    F U N C T I O N S
//
///////////////////////////////////////////////////////////////////////////
//
// CbusReset
//
///////////////////////////////////////////////////////////////////////////

static void CbusReset(void)
{

	uint8_t	idx;
	SET_BIT(PAGE_0_0X72, 0x05, 3);
	haltimerwait(2);
	CLR_BIT(PAGE_0_0X72, 0x05, 3);

	mscCmdInProgress = false;

	// Adjust interrupt mask everytime reset is performed.
	UNMASK_CBUS1_INTERRUPTS;
	UNMASK_CBUS2_INTERRUPTS;

	for(idx=0; idx < 4; idx++)
       {
		// Enable WRITE_STAT interrupt for writes to all 4 MSC Status registers.
		WriteByteCBUS(0xE0 + idx, 0xFF);

		// Enable SET_INT interrupt for writes to all 4 MSC Interrupt registers.
		WriteByteCBUS(0xF0 + idx, 0xFF);
	}
}

///////////////////////////////////////////////////////////////////////////
// InitCBusRegs
//
///////////////////////////////////////////////////////////////////////////
static void InitCBusRegs( void )
{
	uint8_t	regval;

	TX_DEBUG_PRINT(("[MHL]: InitCBusRegs\n"));
	// Increase DDC translation layer timer
	WriteByteCBUS(0x07, 0x32);          // new default is for MHL mode
	WriteByteCBUS(0x40, 0x03); 			// CBUS Drive Strength
	WriteByteCBUS(0x42, 0x06); 			// CBUS DDC interface ignore segment pointer
	WriteByteCBUS(0x36, 0x0C);

	WriteByteCBUS(0x3D, 0xFD);
	WriteByteCBUS(0x1C, 0x01);
	WriteByteCBUS(0x1D, 0x0F);          // MSC_RETRY_FAIL_LIM

	WriteByteCBUS(0x44, 0x02);

	// Setup our devcap
	WriteByteCBUS(0x80, MHL_DEV_ACTIVE);
	WriteByteCBUS(0x81, MHL_VERSION);
	WriteByteCBUS(0x82, (MHL_DEV_CAT_SOURCE));
	WriteByteCBUS(0x83, (uint8_t)(SILICON_IMAGE_ADOPTER_ID >>   8));
	WriteByteCBUS(0x84, (uint8_t)(SILICON_IMAGE_ADOPTER_ID & 0xFF));
	WriteByteCBUS(0x85, MHL_DEV_VID_LINK_SUPPRGB444);
	WriteByteCBUS(0x86, MHL_DEV_AUD_LINK_2CH);
	WriteByteCBUS(0x87, 0);										// not for source
	WriteByteCBUS(0x88, MHL_LOGICAL_DEVICE_MAP);
	WriteByteCBUS(0x89, 0);										// not for source
	WriteByteCBUS(0x8A, (MHL_FEATURE_RCP_SUPPORT | MHL_FEATURE_RAP_SUPPORT |MHL_FEATURE_SP_SUPPORT));
	WriteByteCBUS(0x8B, (uint8_t)(TRANSCODER_DEVICE_ID>>   8));
	WriteByteCBUS(0x8C, (uint8_t)(TRANSCODER_DEVICE_ID& 0xFF));										// reserved
	WriteByteCBUS(0x8D, MHL_SCRATCHPAD_SIZE);
	WriteByteCBUS(0x8E, MHL_INT_AND_STATUS_SIZE);
	WriteByteCBUS(0x8F, 0);										//reserved

	// Make bits 2,3 (initiator timeout) to 1,1 for register CBUS_LINK_CONTROL_2
	regval = ReadByteCBUS( REG_CBUS_LINK_CONTROL_2 );
	regval = (regval | 0x0C);
	WriteByteCBUS(REG_CBUS_LINK_CONTROL_2, regval);

	 // Clear legacy bit on Wolverine TX.
     	regval = ReadByteCBUS( REG_MSC_TIMEOUT_LIMIT);
    	WriteByteCBUS(REG_MSC_TIMEOUT_LIMIT, (regval & MSC_TIMEOUT_LIMIT_MSB_MASK));

	// Set NMax to 1
	WriteByteCBUS(REG_CBUS_LINK_CONTROL_1, 0x01);

	ReadModifyWriteCBUS(0x2E, BIT4 | BIT2 | BIT0, BIT4 | BIT2 | BIT0);	//v1.22
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvAcquireUpstreamHPDControl
//
// Acquire the direct control of Upstream HPD.
//
static void SiiMhlTxDrvAcquireUpstreamHPDControl(void)
{
	// set reg_hpd_out_ovr_en to first control the hpd
	SET_BIT(PAGE_0_0X72, 0x79, 4);
	TX_DEBUG_PRINT(("[MHL]: Upstream HPD Acquired.\n"));
}

///////////////////////////////////////////////////////////////////////////////
		//
// SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow
		//
// Acquire the direct control of Upstream HPD.
		//
static void SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow(void)
{
	// set reg_hpd_out_ovr_en to first control the hpd and clear reg_hpd_out_ovr_val
	ReadModifyWritePage0(0x79, BIT5 | BIT4, BIT4);	// Force upstream HPD to 0 when not in MHL mode.
	TX_DEBUG_PRINT(("[MHL]: Upstream HPD Acquired - driven low.\n"));
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvReleaseUpstreamHPDControl
//
// Release the direct control of Upstream HPD.
//
static void SiiMhlTxDrvReleaseUpstreamHPDControl(void)
{
	// Un-force HPD (it was kept low, now propagate to source
	// let HPD float by clearing reg_hpd_out_ovr_en
	CLR_BIT(PAGE_0_0X72, 0x79, 4);
	TX_DEBUG_PRINT(("[MHL]: Upstream HPD released.\n"));
}

///////////////////////////////////////////////////////////////////////////
// WriteInitialRegisterValues
//
//
///////////////////////////////////////////////////////////////////////////
static void WriteInitialRegisterValues ( void )
{
	TX_DEBUG_PRINT(("[MHL]: WriteInitialRegisterValues\n"));
	// Power Up
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x08, 0x35);	// Power Up TMDS Tx Core
	sii_9224_i2c_writebyte(PAGE_1_0X7A, 0x3D, 0x3F);	// Power up CVCC 1.2V core
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x11, 0x01);	// Enable TxPLL Clock
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x12, 0x15);	// Enable Tx Clock Path & Equalizer

	// Reset CBus to clear state
	CbusReset();

	// Analog PLL Control
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x10, 0xC1);	// bits 5:4 = 2b00 as per characterization team.
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x17, 0x03);	// PLL Calrefsel
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x1A, 0x20);	// VCO Cal
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x22, 0x8A);	// Auto EQ
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x23, 0x6A);	// Auto EQ
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x24, 0xAA);	// Auto EQ
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x25, 0xCA);	// Auto EQ
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x26, 0xEA);	// Auto EQ
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x4C, 0xA0);	// Manual zone control
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x4D, 0x00);	// PLL Mode Value

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x80, 0x34);	// Enable Rx PLL Clock Value
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x45, 0x44);	// Rx PLL BW value from I2C
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x31, 0x0A);	// Rx PLL BW ~ 4MHz
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA0, 0xD0);
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA1, 0xFC);	// Disable internal MHL driver

       // 1x mode
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA3, 0xEB);
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA6, 0x0C);

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x2B, 0x01);	// Enable HDCP Compliance safety

	//
	// CBUS & Discovery
	// CBUS discovery cycle time for each drive and float = 100us
	//
	ReadModifyWritePage0(0x90, BIT3 | BIT2, BIT2);

	// Do not perform RGND impedance detection if connected to SiI 9290
	//
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x91, 0xA5);		// Clear bit 6 (reg_skip_rgnd)

	// Changed from 66 to 77 for 94[1:0] = 11 = 5k reg_cbusmhl_pup_sel
	// and bits 5:4 = 11 rgnd_vth_ctl
	//
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x94, 0x77);			// 1.8V CBUS VTH & GND threshold

	//set bit 2 and 3, which is Initiator Timeout
	WriteByteCBUS(0x31, ReadByteCBUS(0x31) | 0x0c);

	// Establish if connected to 9290 or any other legacy product
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA5, 0xA0);	// bits 4:2. rgnd_res_ctl = 3'b000.

	TX_DEBUG_PRINT(("[MHL]: MHL 1.0 Compliant Clock\n"));

	// RGND & single discovery attempt (RGND blocking) , Force USB ID switch to open
#if (CI2CA_LOW_MHL == DISABLE)
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x95, 0x71);
#else
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x95, 0x75);
	ReadModifyWritePage0(0x91, BIT3, BIT3);
	ReadModifyWritePage0(0x96, BIT5, 0x00);	// Enable CI2CA as an open-drain output (to control external USB switch)
#endif

	// Use only 1K for MHL impedance. Set BIT5 for No-open-drain.
	// Default is good.
	//
	// Use 1k and 2k commented.
//	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, 0x22);

	// Use VBUS path of discovery state machine
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x97, 0x00);


	//
	// For MHL compliance we need the following settings for register 93 and 94
	// Bug 20686
	//
	// To allow RGND engine to operate correctly.
	//
	// When moving the chip from D2 to D0 (power up, init regs) the values should be
	// 94[1:0] = 11  reg_cbusmhl_pup_sel[1:0] should be set for 5k
	// 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be set for 10k
	// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	//
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x92, 0x86);
		// change from CC to 8C to match 10K
        // 0b11 is 5K, 0b10 is 10K, 0b01 is 20k and 0b00 is off
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x93, 0x8C);				// Disable CBUS pull-up during RGND measurement

	//Jiangshanbin HPD BIT6 for push pull
 	ReadModifyWritePage0(0x79, BIT6, 0x00);

	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

	haltimerwait(25);
	ReadModifyWritePage0(0x95, BIT6, 0x00);	// Release USB ID switch

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x90, 0x27);	// Enable CBUS discovery

	InitCBusRegs();

	// Enable Auto soft reset on SCDT = 0
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x05, 0x04);

	// HDMI Transcode mode enable
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x0D, 0x1C);

    	UNMASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_2_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;
}

///////////////////////////////////////////////////////////////////////////
//
// ForceUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ForceUsbIdSwitchOpen ( void )
{
	DISABLE_DISCOVERY		// Disable CBUS discovery
	ReadModifyWritePage0(0x95, BIT6, BIT6);	// Force USB ID switch to open

	WriteBytePage0(0x92, 0x86);

	// Force HPD to 0 when not in MHL mode.
	SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();
}

///////////////////////////////////////////////////////////////////////////
//
// ReleaseUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ReleaseUsbIdSwitchOpen ( void )
{
	haltimerwait(50); // per spec

	// Release USB ID switch
	ReadModifyWritePage0(0x95, BIT6, 0x00);

	ENABLE_DISCOVERY;
}

////////////////////////////////////////////////////////////////////
// SwitchToD0
// This function performs s/w as well as h/w state transitions.
//
// Chip comes up in D2. Firmware must first bring it to full operation
// mode in D0.
////////////////////////////////////////////////////////////////////
static void SwitchToD0( void )
{
	TX_DEBUG_PRINT(("[MHL]: [%d]: Switch To Full power mode (D0)\n",
							(int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)) );

	//
	// WriteInitialRegisterValues switches the chip to full power mode.
	//
	WriteInitialRegisterValues();

	// Force Power State to ON
    	STROBE_POWER_ON

	fwPowerState = POWER_STATE_D0_NO_MHL;
}

////////////////////////////////////////////////////////////////////
// SwitchToD3
//
// This function performs s/w as well as h/w state transitions.
//
////////////////////////////////////////////////////////////////////
static void SwitchToD3( void )
{
	//if(POWER_STATE_D3 != fwPowerState)	//by oscar 20110125 for USB OTG
	{
		TX_DEBUG_PRINT(("[MHL]: Switch To D3\n"));

		ForceUsbIdSwitchOpen();

                //
		// To allow RGND engine to operate correctly.
		// So when moving the chip from D0 MHL connected to D3 the values should be
		// 94[1:0] = 00  reg_cbusmhl_pup_sel[1:0] should be set for open
		// 93[7:6] = 00  reg_cbusdisc_pup_sel[1:0] should be set for open
		// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
                //
		// Disable CBUS pull-up during RGND measurement
		ReadModifyWritePage0(0x93, BIT7 | BIT6 | BIT5 | BIT4, 0);

		ReadModifyWritePage0(0x94, BIT1 | BIT0, 0);

		// 1.8V CBUS VTH & GND threshold

		ReleaseUsbIdSwitchOpen();

		// Force HPD to 0 when not in MHL mode.
		SiiMhlTxDrvAcquireUpstreamHPDControlDriveLow();

		// Change TMDS termination to high impedance on disconnection
		// Bits 1:0 set to 11
		sii_9224_i2c_writebyte(PAGE_2_0X92, 0x01, 0x03);

		//
		// Change state to D3 by clearing bit 0 of 3D (SW_TPI, Page 1) register
		// ReadModifyWriteIndexedRegister(INDEXED_PAGE_1, 0x3D, BIT0, 0x00);
		//
		CLR_BIT(PAGE_1_0X7A, 0x3D, 0);

		fwPowerState = POWER_STATE_D3;
	}

#if (VBUS_POWER_CHK == ENABLE)		// Turn VBUS power off when switch to D3(cable out)
	if( vbuspowerstate == false )
	{
		appvbuscontrol( vbuspowerstate = true );
	}
#endif
}

///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessConnection
//
///////////////////////////////////////////////////////////////////////////
static void MhlTxDrvProcessConnection ( void )
{
	bool_t	mhlConnected = true;

	// double check RGND impedance for USB_ID deglitching.	//by oscar 20110412
	if(0x02 != (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x99) & 0x03) )
	{
		TX_DEBUG_PRINT (("[MHL]: MHL_EST interrupt but not MHL impedance\n"));
		if(dock_inserted) {
			msleep(5);
			TX_DEBUG_PRINT(("mhl wakeup now\n"));
		}
		SwitchToD0();
		SwitchToD3();
		return;
	}
	TX_DEBUG_PRINT (("[MHL]: MHL Cable Connected. CBUS:0x0A = %02X\n", (int) ReadByteCBUS(0x0a)));
	pr_info("mhl cable connected!\n");
	atomic_set(&g_mhlcablestatus, 1);
	if( POWER_STATE_D0_MHL == fwPowerState )
	{
		return;
	}

	//
	// Discovery over-ride: reg_disc_ovride
	//
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA0, 0x10);

	fwPowerState = POWER_STATE_D0_MHL;

	//
	// Increase DDC translation layer timer (uint8_t mode)
	// Setting DDC Byte Mode
	//
	WriteByteCBUS(0x07, 0x32);  // CBUS DDC byte handshake mode
	// Doing it this way causes problems with playstation: ReadModifyWriteByteCBUS(0x07, BIT2,0);

	// Enable segment pointer safety
	SET_BIT(PAGE_CBUS_0XC8, 0x44, 1);

        // upstream HPD status should not be allowed to rise until HPD from downstream is detected.

	//by oscar 20110125 for USB OTG
	// Change TMDS termination to 50 ohm termination (default)
	// Bits 1:0 set to 00
	sii_9224_i2c_writebyte(PAGE_2_0X92, 0x01, 0x00);

        // TMDS should not be enabled until RSEN is high, and HPD and PATH_EN are received

	// Keep the discovery enabled. Need RGND interrupt
	ENABLE_DISCOVERY;

	// Wait T_SRC_RXSENSE_CHK ms to allow connection/disconnection to be stable (MHL 1.0 specs)
	TX_DEBUG_PRINT (("[MHL]: [%d]: Wait T_SRC_RXSENSE_CHK (%d ms) before checking RSEN\n",
							(int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD),
							(int) T_SRC_RXSENSE_CHK) );

	//
	// Ignore RSEN interrupt for T_SRC_RXSENSE_CHK duration.
	// Get the timer started
	//
	haltimerset(TIMER_TO_DO_RSEN_CHK, T_SRC_RXSENSE_CHK);

	// Notify upper layer of cable connection
	SiiMhlTxNotifyConnection(mhlConnected = true);
}

///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessDisconnection
//
///////////////////////////////////////////////////////////////////////////
static void MhlTxDrvProcessDisconnection ( void )
{
	bool_t	mhlConnected = false;

	TX_DEBUG_PRINT (("[MHL]: [%d]: MhlTxDrvProcessDisconnection\n", (int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));

	pr_info("mhl cable disconnected!\n");
	atomic_set(&g_mhlcablestatus, 0);
	// clear all interrupts
//	sii_9224_i2c_writebyte(PAGE_0_0X72, (0x74), sii_9224_i2c_readbyte(PAGE_0_0X72, (0x74)));

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0xA0, 0xD0);

	//
	// Reset CBus to clear register contents
	// This may need some key reinitializations
	//
//	CbusReset();

	// Disable TMDS
	SiiMhlTxDrvTmdsControl( false );

	if( POWER_STATE_D0_MHL == fwPowerState )
	{
		// Notify upper layer of cable connection
		SiiMhlTxNotifyConnection(mhlConnected = false);
	}

	// Now put chip in sleep mode
	SwitchToD3();
}

//#define CbusWakeUpPulse_GPIO
/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   CbusWakeUpPulseGenerator ()
//
// PURPOSE      :   Generate Cbus Wake up pulse sequence using GPIO or I2C method.
//
// INPUT PARAMS :   None
//
// OUTPUT PARAMS:   None
//
// GLOBALS USED :   None
//
// RETURNS      :   None
//
static void CbusWakeUpPulseGenerator( void )
{
#ifdef CbusWakeUpPulse_GPIO
	//
        // GPIO method
	//
        TX_DEBUG_PRINT(("[MHL]: CbusWakeUpPulseGenerator: GPIO mode\n"));
    	 // put reg_cbus_dbgmode = 1
	 sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) | 0x08));

	// Start the pulse
    	 pinMHLTxCbusWakeUpPulse = 1;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 0;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 1;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 0;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_2);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 1;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 0;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 1;
        haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// no adjustmebt for code path in GPIO mode

        pinMHLTxCbusWakeUpPulse = 0;
        haltimerwait(T_SRC_WAKE_TO_DISCOVER);

        // put reg_cbus_dbgmode = 0
        sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) & 0xF7));

        sii_9224_i2c_writebyte(PAGE_0_0X72, 0x90, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x90) & 0xFE));
        sii_9224_i2c_writebyte(PAGE_0_0X72, 0x90, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x90) | 0x01));

#else
	//
	// I2C method
	//
	TX_DEBUG_PRINT(("[MHL]: CbusWakeUpPulseGenerator: I2C mode\n"));
	// Start the pulse
	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) | 0xC0));
    	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) & 0x3F));
    	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) | 0xC0));
    	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) & 0x3F));
    	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_2);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) | 0xC0));
    	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) & 0x3F));
    	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) | 0xC0));
	haltimerwait(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x96, (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x96) & 0x3F));

	haltimerwait(T_SRC_WAKE_TO_DISCOVER);
#endif

	//
	// Toggle MHL discovery bit
	//
//	DISABLE_DISCOVERY;
//	ENABLE_DISCOVERY;

}


///////////////////////////////////////////////////////////////////////////
// ProcessRgnd
//
// H/W has detected impedance change and interrupted.
// We look for appropriate impedance range to call it MHL and enable the
// hardware MHL discovery logic. If not, disable MHL discovery to allow
// USB to work appropriately.
//
// In current chip a firmware driven slow wake up pulses are sent to the
// sink to wake that and setup ourselves for full D0 operation.
///////////////////////////////////////////////////////////////////////////
static void	ProcessRgnd( void )
{
	uint8_t		reg99RGNDRange;
	//
	// Impedance detection has completed - process interrupt
	//
	reg99RGNDRange = sii_9224_i2c_readbyte(PAGE_0_0X72, 0x99) & 0x03;
	TX_DEBUG_PRINT(("[MHL]: RGND Reg 99 = %02X\n", (int)reg99RGNDRange));

	//
	// Reg 0x99
	// 00, 01 or 11 means USB.
	// 10 means 1K impedance (MHL)
	//
	// If 1K, then only proceed with wake up pulses
	if(0x02 == reg99RGNDRange)
	{
		// Switch to full power mode.	// oscar 20110211 for UTG ID=GND endless loop
		SwitchToD0();

		// Select CBUS drive float.
		SET_BIT(PAGE_0_0X72, 0x95, 5);

		//The sequence of events during MHL discovery is as follows:
		//	(i) SiI9244 blocks on RGND interrupt (Page0:0x74[6]).
		//	(ii) System firmware turns off its own VBUS if present.
		//	(iii) System firmware waits for about 200ms (spec: TVBUS_CBUS_STABLE, 100 - 1000ms), then checks for the presence of
		//		VBUS from the Sink.
		//	(iv) If VBUS is present then system firmware proceed to drive wake pulses to the Sink as described in previous
		//		section.
		//	(v) If VBUS is absent the system firmware turns on its own VBUS, wait for an additional 200ms (spec:
		//		TVBUS_OUT_TO_STABLE, 100 - 1000ms), and then proceed to drive wake pulses to the Sink as described in above.

		// AP need to check VBUS power present or absent in here 	// by oscar 20110527

#if (VBUS_POWER_CHK == ENABLE)			// Turn on VBUS output.
		appvbuscontrol( vbuspowerstate = false );
#endif

		TX_DEBUG_PRINT(("[MHL]: Waiting T_SRC_VBUS_CBUS_TO_STABLE (%d ms)\n", (int)T_SRC_VBUS_CBUS_TO_STABLE));
		haltimerwait(T_SRC_VBUS_CBUS_TO_STABLE);

		//
		// Send slow wake up pulse using GPIO or I2C
		//
		CbusWakeUpPulseGenerator();

		haltimerset( ELAPSED_TIMER1, T_SRC_DISCOVER_TO_MHL_EST );	//xding
	}
	else
	{
		TX_DEBUG_PRINT(("[MHL]: USB impedance. Set for USB Established.\n"));

		CLR_BIT(PAGE_0_0X72, 0x95, 5);
		mhl_processed = true;
        }
}

////////////////////////////////////////////////////////////////////
// Int4Isr
//
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(interested during D0)
////////////////////////////////////////////////////////////////////
static void Int4Isr( void )
{
	uint8_t reg74;

	reg74 = sii_9224_i2c_readbyte(PAGE_0_0X72, (0x74));	// read status

	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if(0xFF == reg74)
	{
		return;
	}

#if 0
	if(reg74)
	{
		TX_DEBUG_PRINT(("[MHL]: >Got INTR_4. [reg74 = %02X]\n", (int)reg74));
	}
#endif

	// process MHL_EST interrupt
	if(reg74 & BIT2) // MHL_EST_INT
	{
		haltimerset( ELAPSED_TIMER1, 0 );	//xding
		MhlTxDrvProcessConnection();
	}

	// process USB_EST interrupt
	else if(reg74 & BIT3) // MHL_DISC_FAIL_INT
	{
		MhlTxDrvProcessDisconnection();
//		return;
	}

	if((POWER_STATE_D3 == fwPowerState) && (reg74 & BIT6))
	{
		// process RGND interrupt

		// Switch to full power mode.
		//SwitchToD0();

		//
		// If a sink is connected but not powered on, this interrupt can keep coming
		// Determine when to go back to sleep. Say after 1 second of this state.
		//
		// Check RGND register and send wake up pulse to the peer
		//
		ProcessRgnd();
	}

	// CBUS Lockout interrupt?
	if (reg74 & BIT4)
	{
		TX_DEBUG_PRINT(("[MHL]: CBus Lockout\n"));

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();
	}
	sii_9224_i2c_writebyte(PAGE_0_0X72, (0x74), reg74);	// clear all interrupts

}

//////////////////////////////////////////////////////////////////////////////
//
// DeglitchRsenLow
//
// This function looks at the RSEN signal if it is low.
//
// The disconnection will be performed only if we were in fully MHL connected
// state for more than 400ms AND a 150ms deglitch from last interrupt on RSEN
// has expired.
//
// If MHL connection was never established but RSEN was low, we unconditionally
// and instantly process disconnection.
//
static void DeglitchRsenLow( void )
{
	TX_DEBUG_PRINT(("[MHL]: DeglitchRsenLow RSEN <72:09[2]> = %02X\n", (int) (sii_9224_i2c_readbyte(PAGE_0_0X72, 0x09)) ));

	if((sii_9224_i2c_readbyte(PAGE_0_0X72, 0x09) & BIT2) == 0x00)
	{
		TX_DEBUG_PRINT(("[MHL]: [%d]: RSEN is Low.\n", (int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));
                //
		// If no MHL cable is connected or RSEN deglitch timer has started,
		// we may not receive interrupts for RSEN.
		// Monitor the status of RSEN here.
                //
	        //
		// First check means we have not received any interrupts and just started
		// but RSEN is low. Case of "nothing" connected on MHL receptacle
	        //
		if((POWER_STATE_D0_MHL == fwPowerState)    && haltimerexpired(TIMER_TO_DO_RSEN_DEGLITCH) )
		{
			// Second condition means we were fully operational, then a RSEN LOW interrupt
			// occured and a DEGLITCH_TIMER per MHL specs started and completed.
			// We can disconnect now.
	                //
			TX_DEBUG_PRINT(("[MHL]: Disconnection due to RSEN Low\n"));

			deglitchingRsenNow = false;

			// FP1226: Toggle MHL discovery to level the voltage to deterministic vale.
			DISABLE_DISCOVERY;
			ENABLE_DISCOVERY;
			//
			// We got here coz cable was never connected
			//
            		dsHpdStatus &= ~BIT6;  //cable disconnect implies downstream HPD low
            		WriteByteCBUS(0x0D, dsHpdStatus);
            		SiiMhlTxNotifyDsHpdChange( 0 );
			MhlTxDrvProcessDisconnection();
		}
	}
	else
	{
              //
		// Deglitch here:
		// RSEN is not low anymore. Reset the flag.
		// This flag will be now set on next interrupt.
		//
		// Stay connected
              //
		deglitchingRsenNow = false;
	}
}

////////////////////////////////////////////////////////////////////
// Int1RsenIsr
//
// This interrupt is used only to decide if the MHL is disconnected
// The disconnection is determined by looking at RSEN LOW and applying
// all MHL compliant disconnect timings and deglitch logic.
//
//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)
////////////////////////////////////////////////////////////////////
void	Int1RsenIsr( void )
{
	uint8_t	reg71 = sii_9224_i2c_readbyte(PAGE_0_0X72, 0x71);
	uint8_t	rsen  = sii_9224_i2c_readbyte(PAGE_0_0X72, 0x09) & BIT2;
	// Look at RSEN interrupt.
	// If RSEN interrupt is lost, check if we should deglitch using the RSEN status only.
	if( (reg71 & BIT5) ||
		((false == deglitchingRsenNow) && (rsen == 0x00)) )
	{
		TX_DEBUG_PRINT (("[MHL]: Got INTR_1: reg71 = %02X, rsen = %02X\n", (int) reg71, (int) rsen));
		//
		// RSEN becomes LOW in SYS_STAT register 0x72:0x09[2]
		// SYS_STAT	==> bit 7 = VLOW, 6:4 = MSEL, 3 = TSEL, 2 = RSEN, 1 = HPD, 0 = TCLK STABLE
		//
		// Start countdown timer for deglitch
		// Allow RSEN to stay low this much before reacting
		//
		if(rsen == 0x00)
		{
			TX_DEBUG_PRINT (("[MHL]: Int1RsenIsr: Start T_SRC_RSEN_DEGLITCH (%d ms) before disconnection\n",
									 (int)(T_SRC_RSEN_DEGLITCH) ) );
	//
			// We got this interrupt due to cable removal
			// Start deglitch timer
	//
			haltimerset(TIMER_TO_DO_RSEN_DEGLITCH, T_SRC_RSEN_DEGLITCH);

			deglitchingRsenNow = true;
		}
		else if( deglitchingRsenNow )
		{
			TX_DEBUG_PRINT(("[MHL]: [%d]: Ignore now, RSEN is high. This was a glitch.\n", (int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));
	//
			// Ignore now, this was a glitch
	//
			deglitchingRsenNow = false;
		}
		// Clear MDI_RSEN interrupt
		sii_9224_i2c_writebyte(PAGE_0_0X72, 0x71, BIT5);
	}
	else if( deglitchingRsenNow )
	{
		TX_DEBUG_PRINT(("[MHL]: [%d]: Ignore now coz (reg71 & BIT5) has been cleared. This was a glitch.\n", (int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));
	//
		// Ignore now, this was a glitch
	//
		deglitchingRsenNow = false;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// ApplyDdcAbortSafety
//
static void ApplyDdcAbortSafety( void )
{
	uint8_t	bTemp, bPost;

/*	TX_DEBUG_PRINT(("[MHL]: [%d]: Do we need DDC Abort Safety\n",
								(int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));*/

	WriteByteCBUS(0x29, 0xFF);  // clear the ddc abort counter
	bTemp = ReadByteCBUS(0x29);  // get the counter
	haltimerwait(3);
	bPost = ReadByteCBUS(0x29);  // get another value of the counter

    	TX_DEBUG_PRINT(("[MHL]: bTemp: 0x%X bPost: 0x%X\n",(int)bTemp,(int)bPost));

	if (bPost > (bTemp + 50))
	{
		TX_DEBUG_PRINT(("[MHL]: Applying DDC Abort Safety(SWWA 18958)\n"));

        	CbusReset();

		InitCBusRegs();

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();

		MhlTxDrvProcessDisconnection();
	}

}

///////////////////////////////////////////////////////////////////////////
//
// CBusProcessErrors
//
//
///////////////////////////////////////////////////////////////////////////
static uint8_t CBusProcessErrors( uint8_t intStatus )
{
    uint8_t result          = 0;
    uint8_t mscAbortReason  = 0;
    uint8_t ddcAbortReason  = 0;

    /* At this point, we only need to look at the abort interrupts. */

    intStatus &=  (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT);

    if ( intStatus )
    {
//      result = ERROR_CBUS_ABORT;		// No Retry will help

        /* If transfer abort or MSC abort, clear the abort reason register. */
	if( intStatus & BIT_DDC_ABORT )
	{
		result = ddcAbortReason = ReadByteCBUS( REG_DDC_ABORT_REASON );
	 	TX_DEBUG_PRINT( ("[MHL]: CBUS:: DDC ABORT happened, reason:: %02X\n", (int)(ddcAbortReason)));
	}

        if ( intStatus & BIT_MSC_XFR_ABORT )
        {
            result = mscAbortReason = ReadByteCBUS( REG_PRI_XFR_ABORT_REASON );

            TX_DEBUG_PRINT( ("[MHL]: CBUS:: MSC Transfer ABORTED. Clearing 0x0D\n"));
            WriteByteCBUS( REG_PRI_XFR_ABORT_REASON, 0xFF );
        }
        if ( intStatus & BIT_MSC_ABORT )
        {
            TX_DEBUG_PRINT( ("[MHL]: CBUS:: MSC Peer sent an ABORT. Clearing 0x0E\n"));
            WriteByteCBUS( REG_CBUS_PRI_FWR_ABORT_REASON, 0xFF );
        }

        // Now display the abort reason.

        if ( mscAbortReason != 0 )
        {
            TX_DEBUG_PRINT( ("[MHL]: CBUS:: Reason for ABORT is ....0x%02X\n", (int)mscAbortReason ));

            if ( mscAbortReason & CBUSABORT_BIT_REQ_MAXFAIL)
            {
                TX_DEBUG_PRINT( ("[MHL]: CBUS:: Requestor MAXFAIL - retry threshold exceeded\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_PROTOCOL_ERROR)
            {
                TX_DEBUG_PRINT( ("[MHL]: CBUS:: Protocol Error\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_REQ_TIMEOUT)
            {
                TX_DEBUG_PRINT( ("[MHL]: CBUS:: Requestor translation layer timeout\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_PEER_ABORTED)
            {
                TX_DEBUG_PRINT( ("[MHL]: CBUS:: Peer sent an abort\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_UNDEFINED_OPCODE)
            {
                TX_DEBUG_PRINT( ("[MHL]: CBUS:: Undefined opcode\n"));
            }
        }
    }
    return( result );
}

///////////////////////////////////////////////////////////////////////////
//
// MhlCbusIsr
//
// Only when MHL connection has been established. This is where we have the
// first looks on the CBUS incoming commands or returned data bytes for the
// previous outgoing command.
//
// It simply stores the event and allows application to pick up the event
// and respond at leisure.
//
// Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
///////////////////////////////////////////////////////////////////////////
static void MhlCbusIsr( void )
{
	uint8_t	cbusInt;
	uint8_t    gotData[4];	// Max four status and int registers.
	uint8_t	i;
	uint8_t	reg71 = sii_9224_i2c_readbyte(PAGE_0_0X72, 0x71);

	//
	// Main CBUS interrupts on CBUS_INTR_STATUS
	//
	cbusInt = ReadByteCBUS(0x08);

	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if(cbusInt == 0xFF)
	{
		return;
	}

	cbusInt &= (~(BIT1|BIT0));	 //don't check Reserved bits // by oscar added 20101207
	if( cbusInt )
	{
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x08, cbusInt);

	    	TX_DEBUG_PRINT(("[MHL]: Clear CBUS INTR_1: %02X\n", (int) cbusInt));
	}

	// Look for DDC_ABORT
	if (cbusInt & BIT2)
	{
		ApplyDdcAbortSafety();
	}
	// MSC_MSG (RCP/RAP)
	if((cbusInt & BIT3))
	{
    	    	uint8_t mscMsg[2];
	    	TX_DEBUG_PRINT(("[MHL]: MSC_MSG Received\n"));
		//
		// Two bytes arrive at registers 0x18 and 0x19
		//
        	mscMsg[0] = ReadByteCBUS( 0x18 );
        	mscMsg[1] = ReadByteCBUS( 0x19 );

	    	TX_DEBUG_PRINT(("[MHL]: MSC MSG: %02X %02X\n", (int)mscMsg[0], (int)mscMsg[1] ));
		SiiMhlTxGotMhlMscMsg( mscMsg[0], mscMsg[1] );
	}
	if((cbusInt & BIT5) || (cbusInt & BIT6))	// MSC_REQ_ABORT or MSC_RESP_ABORT
	{
		gotData[0] = CBusProcessErrors(cbusInt);
		// Ignore CBUS error and release CBUS status for new command. // by oscar 20101215
		mscCmdInProgress = false;
	}
	// MSC_REQ_DONE received.
	if(cbusInt & BIT4)
	{
	    	TX_DEBUG_PRINT(("[MHL]: MSC_REQ_DONE\n"));

		mscCmdInProgress = false;
        	// only do this after cBusInt interrupts are cleared above
		SiiMhlTxMscCommandDone( ReadByteCBUS( 0x16 ) );
	}

    	if (BIT7 & cbusInt)
    	{
#define CBUS_LINK_STATUS_2 0x38
	    	TX_DEBUG_PRINT(("[MHL]: Clearing CBUS_link_hard_err_count\n"));
        	// reset the CBUS_link_hard_err_count field
        	WriteByteCBUS(CBUS_LINK_STATUS_2,(uint8_t)(ReadByteCBUS(CBUS_LINK_STATUS_2) & 0xF0));
    	}
	//
	// Now look for interrupts on register 0x1E. CBUS_MSC_INT2
	// 7:4 = Reserved
	//   3 = msc_mr_write_state = We got a WRITE_STAT
	//   2 = msc_mr_set_int. We got a SET_INT
	//   1 = reserved
	//   0 = msc_mr_write_burst. We received WRITE_BURST
	//
	cbusInt = ReadByteCBUS(0x1E);
	if( cbusInt )
	{
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x1E, cbusInt);

	    	TX_DEBUG_PRINT(("[MHL]: Clear CBUS INTR_2: %02X\n",(int) cbusInt));
	}
    	if ( BIT0 & cbusInt)
    	{
        	// WRITE_BURST complete
        	SiiMhlTxMscWriteBurstDone( cbusInt );
	}
	if (cbusInt & BIT2)
	{
    		uint8_t intr[4];
    		uint8_t address;

	    	TX_DEBUG_PRINT(("[MHL]: MHL INTR Received\n"));
   		for(i = 0,address=0xA0; i < 4; ++i,++address)
		{
			// Clear all, recording as we go
           		 intr[i] = ReadByteCBUS( address );
			WriteByteCBUS( address, intr[i]);
		}
		// We are interested only in first two bytes.
		SiiMhlTxGotMhlIntr( intr[0], intr[1] );

	}
	if ((cbusInt & BIT3)||haltimerexpired(TIMER_SWWA_WRITE_STAT))
	{
    		uint8_t status[4];
    		uint8_t address;

        	for (i = 0,address=0xB0; i < 4;++i,++address)
		{
			// Clear all, recording as we go
            		status[i] = ReadByteCBUS( address );
			WriteByteCBUS( address , 0xFF /* future status[i]*/ );
		}
		SiiMhlTxGotMhlStatus( status[0], status[1] );
        	haltimerset(TIMER_SWWA_WRITE_STAT, T_SWWA_WRITE_STAT);
	}
	if(reg71)
	{
	    	//TX_DEBUG_PRINT(("[MHL]: INTR_1 @72:71 = %02X\n", (int) reg71));
		// Clear MDI_HPD interrupt
		sii_9224_i2c_writebyte(PAGE_0_0X72, 0x71, reg71);  /*INTR_1_DESIRED_MASK*/
	}
	//
	// Check if a SET_HPD came from the downstream device.
	//
	cbusInt = ReadByteCBUS(0x0D);

	// CBUS_HPD status bit
	if( BIT6 & (dsHpdStatus ^ cbusInt))
	{
		// Remember
       	dsHpdStatus = cbusInt;

        	TX_DEBUG_PRINT(("[MHL]: Downstream HPD changed to: %02X\n", (int) cbusInt));
		// Inform upper layer of change in Downstream HPD
       	SiiMhlTxNotifyDsHpdChange( BIT6 & cbusInt );
	}
}

#ifdef	APPLY_PLL_RECOVERY
///////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyPllRecovery
//
// PURPOSE:		This function helps recover PLL.
//
///////////////////////////////////////////////////////////////////////////
static void ApplyPllRecovery ( void )
{
	// Disable TMDS
	CLR_BIT(PAGE_0_0X72, 0x80, 4);

	// Enable TMDS
	SET_BIT(PAGE_0_0X72, 0x80, 4);

	// followed by a 10ms settle time
	haltimerwait(10);

	// MHL FIFO Reset here
	SET_BIT(PAGE_0_0X72, 0x05, 4);

	CLR_BIT(PAGE_0_0X72, 0x05, 4);

	TX_DEBUG_PRINT(("[MHL]: Applied PLL Recovery\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   SiiMhlTxDrvRecovery ()
//
// PURPOSE      :   Check SCDT interrupt and PSTABLE interrupt
//
//
// DESCRIPTION :  If SCDT interrupt happened and current status
// is HIGH, irrespective of the last status (assuming we can miss an interrupt)
// go ahead and apply PLL recovery.
//
// When a PSTABLE interrupt happens, it is an indication of a possible
// FIFO overflow condition. Apply a recovery method.
//
//////////////////////////////////////////////////////////////////////////////
static void SiiMhlTxDrvRecovery( void )
{
	//
	// Detect Rising Edge of SCDT
        //
	// Check if SCDT interrupt came
	if((sii_9224_i2c_readbyte(PAGE_0_0X72, (0x74)) & BIT0))
       {
       	TX_DEBUG_PRINT(("[MHL]: SCDT Interrupt\n"));
	        //
		// Clear this interrupt and then check SCDT.
		// if the interrupt came irrespective of what SCDT was earlier
		// and if SCDT is still high, apply workaround.
	        //
		// This approach implicitly takes care of one lost interrupt.
		//
		SET_BIT(PAGE_0_0X72, (0x74), 0);


		// Read status, if it went HIGH
		if (((sii_9224_i2c_readbyte(PAGE_0_0X72, 0x81)) & BIT1) >> 1)
	        {
			// Toggle TMDS and reset MHL FIFO.
			ApplyPllRecovery();
		}
        }
        //
	// Check PSTABLE interrupt...reset FIFO if so.
        //
	if((sii_9224_i2c_readbyte(PAGE_0_0X72, (0x72)) & BIT1))
	{

		TX_DEBUG_PRINT(("[MHL]: PSTABLE Interrupt\n"));

		// Toggle TMDS and reset MHL FIFO.
		ApplyPllRecovery();

		// clear PSTABLE interrupt. Do not clear this before resetting the FIFO.
		SET_BIT(PAGE_0_0X72, (0x72), 1);

	}
}
#endif // APPLY_PLL_RECOVERY

////////////////////////////////////////////////////////////////////
//
// E X T E R N A L L Y    E X P O S E D   A P I    F U N C T I O N S
//
////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxChipInitialize
//
// Chip specific initialization.
// This function is for SiI 9244 Initialization: HW Reset, Interrupt enable.
//
//
//////////////////////////////////////////////////////////////////////////////

bool_t SiiMhlTxChipInitialize ( void )
{
	// Toggle TX reset pin
	// It will be done in I2C driver proble, Jiangshanbin
	//pinTxHwReset = LOW;
	//haltimerwait(TX_HW_RESET_PERIOD);
	//pinTxHwReset = HIGH;

	//
	// Setup our own timer for now. 50ms.
	//
	haltimerset( ELAPSED_TIMER, MONITORING_PERIOD );

	TX_DEBUG_PRINT( ("[MHL]: SiiMhlTxChipInitialize: 92%02X\n", (int)sii_9224_i2c_readbyte(PAGE_0_0X72, 0x02)) );

	// setup device registers. Ensure RGND interrupt would happen.
	WriteInitialRegisterValues();

	sii_9224_i2c_writebyte(PAGE_0_0X72, 0x71, INTR_1_DESIRED_MASK); // [dave] clear HPD & RSEN interrupts

	// Setup interrupt masks for all those we are interested.
	UNMASK_INTR_4_INTERRUPTS;
	UNMASK_INTR_1_INTERRUPTS;

	// CBUS interrupts are unmasked after performing the reset.
	// UNMASK_CBUS1_INTERRUPTS;
	// UNMASK_CBUS2_INTERRUPTS;

	SwitchToD3();

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxDeviceIsr
//
// This function must be called from a master interrupt handler or any polling
// loop in the host software if during initialization call the parameter
// interruptDriven was set to true. SiiMhlGetEvents will not look at these
// events assuming firmware is operating in interrupt driven mode. MhlTx component
// performs a check of all its internal status registers to see if a hardware event
// such as connection or disconnection has happened or an RCP message has been
// received from the connected device. Due to the interruptDriven being true,
// MhlTx code will ensure concurrency by asking the host software and hardware to
// disable interrupts and restore when completed. Device interrupts are cleared by
// the MhlTx component before returning back to the caller. Any handling of
// programmable interrupt controller logic if present in the host will have to
// be done by the caller after this function returns back.

// This function has no parameters and returns nothing.
//
// This is the master interrupt handler for 9244. It calls sub handlers
// of interest. Still couple of status would be required to be picked up
// in the monitoring routine (Sii9244TimerIsr)
//
// To react in least amount of time hook up this ISR to processor's
// interrupt mechanism.
//
// Just in case environment does not provide this, set a flag so we
// call this from our monitor (Sii9244TimerIsr) in periodic fashion.
//
// Device Interrupts we would look at
//		RGND		= to wake up from D3
//		MHL_EST 	= connection establishment
//		CBUS_LOCKOUT= Service USB switch
//		RSEN_LOW	= Disconnection deglitcher
//		CBUS 		= responder to peer messages
//					  Especially for DCAP etc time based events
//
void SiiMhlTxDeviceIsr( void )
{

	//
	// Look at discovery interrupts if not yet connected.
	//
	if( POWER_STATE_D0_MHL != fwPowerState )
	{
		//
		// Check important RGND, MHL_EST, CBUS_LOCKOUT and SCDT interrupts
		// During D3 we only get RGND but same ISR can work for both states
		//
        	//if (0==pinMHLTxInt)
        	{
			Int4Isr();
		}
	}
	else if( POWER_STATE_D0_MHL == fwPowerState )
	{
		//
		// Check RSEN LOW interrupt and apply deglitch timer for transition
		// from connected to disconnected state.
		//
		if(haltimerexpired( TIMER_TO_DO_RSEN_CHK ))
		{
			//
			// If no MHL cable is connected, we may not receive interrupt for RSEN at all
			// as nothing would change. Poll the status of RSEN here.
			//
			// Also interrupt may come only once who would have started deglitch timer.
			// The following function will look for expiration of that before disconnection.
			//
			if(deglitchingRsenNow)
			{
				TX_DEBUG_PRINT(("[MHL]: [%d]: deglitchingRsenNow.\n", (int) (haltimerelapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));
				DeglitchRsenLow();
			}
			else
			{
				Int1RsenIsr();
			}
		}

		if( POWER_STATE_D0_MHL != fwPowerState )		// Check if chip enter D3 mode in DeglitchRsenLow() function
			return;

#ifdef	APPLY_PLL_RECOVERY
		//
		// Trigger a PLL recovery if SCDT is high or FIFO overflow has happened.
		//
		SiiMhlTxDrvRecovery();

#endif	//	APPLY_PLL_RECOVERY
		//
		// Check for any peer messages for DCAP_CHG etc
		// Dispatch to have the CBUS module working only once connected.
		//
		MhlCbusIsr();
	}

}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvTmdsControl
//
// Control the TMDS output. MhlTx uses this to support RAP content on and off.
//
void	SiiMhlTxDrvTmdsControl( bool_t enable )
{
	if( enable )
	{
		SET_BIT(PAGE_0_0X72, 0x80, 4);
	    	TX_DEBUG_PRINT(("[MHL]: MHL Output Enabled\n"));
        	SiiMhlTxDrvReleaseUpstreamHPDControl();  // this triggers an EDID read
	}
	else
	{
		CLR_BIT(PAGE_0_0X72, 0x80, 4);
	    	TX_DEBUG_PRINT(("[MHL]: MHL Ouput Disabled\n"));
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvNotifyEdidChange
//
// MhlTx may need to inform upstream device of an EDID change. This can be
// achieved by toggling the HDMI HPD signal or by simply calling EDID read
// function.
//
void	SiiMhlTxDrvNotifyEdidChange ( void )
{
    	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxDrvNotifyEdidChange\n"));
	//
	// Prepare to toggle HPD to upstream
	//
    	SiiMhlTxDrvAcquireUpstreamHPDControl();

	// reg_hpd_out_ovr_val = LOW to force the HPD low
	CLR_BIT(PAGE_0_0X72, 0x79, 5);

	// wait a bit
	haltimerwait(110);

    	// force upstream HPD back to high by reg_hpd_out_ovr_val = HIGH
	SET_BIT(PAGE_0_0X72, 0x79, 5);
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvSendCbusCommand
//
// Write the specified Sideband Channel command to the CBUS.
// Command can be a MSC_MSG command (RCP/RAP/RCPK/RCPE/RAPK), or another command
// such as READ_DEVCAP, SET_INT, WRITE_STAT, etc.
//
// Parameters:
//              pReq    - Pointer to a cbus_req_t structure containing the
//                        command to write
// Returns:     true    - successful write
//              false   - write failed
//
bool_t SiiMhlTxDrvSendCbusCommand ( cbus_req_t *pReq  )
{
    bool_t  success = true;

    uint8_t i, startbit;

    //
    // If not connected, return with error
//
    if( (POWER_STATE_D0_MHL != fwPowerState ) || (mscCmdInProgress))
    {
	    TX_DEBUG_PRINT(("[MHL]: Error: fwPowerState: %02X, or CBUS(0x0A):%02X mscCmdInProgress = %d\n",
				(int) fwPowerState,
				(int) ReadByteCBUS(0x0a),
				(int) mscCmdInProgress));

   	    return false;
    }
    // Now we are getting busy
    mscCmdInProgress = true;

    TX_DEBUG_PRINT(("[MHL]: Sending MSC command %02X, %02X, %02X, %02X\n",
			(int)pReq->command,
			(int)(pReq->offsetData),
		 	(int)pReq->payload_u.msgData[0],
		 	(int)pReq->payload_u.msgData[1]));

    /****************************************************************************************/
    /* Setup for the command - write appropriate registers and determine the correct        */
    /*                         start bit.                                                   */
    /****************************************************************************************/

    // Set the offset and outgoing data byte right away
	WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD    & 0xFF), pReq->offsetData); 	// set offset
	WriteByteCBUS( (REG_CBUS_PRI_WR_DATA_1ST & 0xFF), pReq->payload_u.msgData[0]);

    startbit = 0x00;
    switch ( pReq->command )
    {
	case MHL_SET_INT:	// Set one interrupt register = 0x60
		startbit = MSC_START_BIT_WRITE_REG;
		break;

       case MHL_WRITE_STAT:	// Write one status register = 0x60 | 0x80
          	startbit = MSC_START_BIT_WRITE_REG;
            	break;

       case MHL_READ_DEVCAP:	// Read one device capability register = 0x61
            	startbit = MSC_START_BIT_READ_REG;
            	break;

 	case MHL_GET_STATE:			// 0x62 -
	case MHL_GET_VENDOR_ID:		// 0x63 - for vendor id
	case MHL_SET_HPD:			// 0x64	- Set Hot Plug Detect in follower
	case MHL_CLR_HPD:			// 0x65	- Clear Hot Plug Detect in follower
	case MHL_GET_SC1_ERRORCODE:		// 0x69	- Get channel 1 command error code
	case MHL_GET_DDC_ERRORCODE:		// 0x6A	- Get DDC channel command error code.
	case MHL_GET_MSC_ERRORCODE:		// 0x6B	- Get MSC command error code.
	case MHL_GET_SC3_ERRORCODE:		// 0x6D	- Get channel 3 command error code.
		WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command );
            	startbit = MSC_START_BIT_MSC_CMD;
            	break;

       case MHL_MSC_MSG:
		WriteByteCBUS( (REG_CBUS_PRI_WR_DATA_2ND & 0xFF), pReq->payload_u.msgData[1] );
		WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command );
            	startbit = MSC_START_BIT_VS_CMD;
            	break;

       case MHL_WRITE_BURST:
            	ReadModifyWriteCBUS((REG_MSC_WRITE_BURST_LEN & 0xFF),0x0F,pReq->length -1);

            	// Now copy all bytes from array to local scratchpad
            	if (NULL == pReq->payload_u.pdatabytes)
            {
                	TX_DEBUG_PRINT(("[MHL]: Put pointer to WRITE_BURST data in req.pdatabytes!!!\n\n"));
            }
            else
            {
            		uint8_t *pData = pReq->payload_u.pdatabytes;
                	TX_DEBUG_PRINT(("[MHL]: Writing data into scratchpad\n\n"));
            		for ( i = 0; i < pReq->length; i++ )
            		{
                    		WriteByteCBUS( (REG_CBUS_SCRATCHPAD_0 & 0xFF) + i, *pData++ );
              	}
            }
            startbit = MSC_START_BIT_WRITE_BURST;
            break;

       default:
            	success = false;
            	break;
    }

    /****************************************************************************************/
    /* Trigger the CBUS command transfer using the determined start bit.                    */
    /****************************************************************************************/

    if ( success )
    {
        WriteByteCBUS( REG_CBUS_PRI_START & 0xFF, startbit );
    }
    else
    {
        TX_DEBUG_PRINT(("[MHL]: SiiMhlTxDrvSendCbusCommand failed\n\n"));
    }

    return( success );
}
bool_t SiiMhlTxDrvCBusBusy(void)
{
    return mscCmdInProgress ? true :false;
}

void SiiMhlTxDrvGetScratchPad(uint8_t startReg,uint8_t *pData,uint8_t length)
{
    int i;
    uint8_t regOffset;

    for (regOffset= 0xC0 + startReg,i = 0; i < length;++i,++regOffset)
    {
        *pData++ = ReadByteCBUS( regOffset );
    }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/*
queue implementation
*/
#define NUM_CBUS_EVENT_QUEUE_EVENTS 5
typedef struct _CBusQueue_t
{
    uint8_t head;   // queue empty condition head == tail
    uint8_t tail;
    cbus_req_t queue[NUM_CBUS_EVENT_QUEUE_EVENTS];
}CBusQueue_t,*PCBusQueue_t;


#define QUEUE_SIZE(x) (sizeof(x.queue)/sizeof(x.queue[0]))
#define MAX_QUEUE_DEPTH(x) (QUEUE_SIZE(x) -1)
#define QUEUE_DEPTH(x) ((x.head <= x.tail)?(x.tail-x.head):(QUEUE_SIZE(x)-x.head+x.tail))
#define QUEUE_FULL(x) (QUEUE_DEPTH(x) >= MAX_QUEUE_DEPTH(x))

#define ADVANCE_QUEUE_HEAD(x) { x.head = (x.head < MAX_QUEUE_DEPTH(x))?(x.head+1):0; }
#define ADVANCE_QUEUE_TAIL(x) { x.tail = (x.tail < MAX_QUEUE_DEPTH(x))?(x.tail+1):0; }

#define RETREAT_QUEUE_HEAD(x) { x.head = (x.head > 0)?(x.head-1):MAX_QUEUE_DEPTH(x); }


// Because the Linux driver can be opened multiple times it can't
// depend on one time structure initialization done by the compiler.
//CBusQueue_t CBusQueue={0,0,{0}};
CBusQueue_t CBusQueue;

cbus_req_t *GetNextCBusTransactionImpl(void)
{
    if (0==QUEUE_DEPTH(CBusQueue))
    {
        return NULL;
    }
    else
    {
    	cbus_req_t *retVal;
       retVal = &CBusQueue.queue[CBusQueue.head];
       ADVANCE_QUEUE_HEAD(CBusQueue)
       return retVal;
    }
}
cbus_req_t *GetNextCBusTransactionWrapper(char *pszFunction,int iLine)
{
    TX_DEBUG_PRINT(("[MHL]:%d %s\n",iLine,pszFunction));
    return  GetNextCBusTransactionImpl();
}
#define GetNextCBusTransaction(func) GetNextCBusTransactionWrapper(#func,__LINE__)

bool_t PutNextCBusTransactionImpl(cbus_req_t *pReq)
{
    if (QUEUE_FULL(CBusQueue))
    {
        //queue is full
        return false;
    }
    // at least one slot available
    CBusQueue.queue[CBusQueue.tail] = *pReq;
    ADVANCE_QUEUE_TAIL(CBusQueue)
    return true;
}
// use this wrapper to do debugging output for the routine above.
bool_t PutNextCBusTransactionWrapper(cbus_req_t *pReq,int iLine)
{
    bool_t retVal;

    TX_DEBUG_PRINT(("[MHL]:%d PutNextCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n"
                ,iLine
                ,(int)pReq->command
                ,(int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[0]:pReq->offsetData)
                ,(int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[1]:pReq->payload_u.msgData[0])
                ,(int)QUEUE_DEPTH(CBusQueue)
                ,(int)CBusQueue.head
                ,(int)CBusQueue.tail
                ));
    retVal = PutNextCBusTransactionImpl(pReq);

    if (!retVal)
    {
        TX_DEBUG_PRINT(("[MHL]:%d PutNextCBusTransaction queue full, when adding event %d\n",iLine,(int)pReq->command));
    }
    return retVal;
}
#define PutNextCBusTransaction(req) PutNextCBusTransactionWrapper(req,__LINE__)

bool_t PutPriorityCBusTransactionImpl(cbus_req_t *pReq)
{
    if (QUEUE_FULL(CBusQueue))
    {
        //queue is full
        return false;
    }
    // at least one slot available
    RETREAT_QUEUE_HEAD(CBusQueue)
    CBusQueue.queue[CBusQueue.head] = *pReq;
    return true;
}
bool_t PutPriorityCBusTransactionWrapper(cbus_req_t *pReq,int iLine)
{
    bool_t retVal;
    TX_DEBUG_PRINT(("[MHL]:%d: PutPriorityCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n"
                ,iLine
                ,(int)pReq->command
                ,(int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[0]:pReq->offsetData)
                ,(int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[1]:pReq->payload_u.msgData[0])
                ,(int)QUEUE_DEPTH(CBusQueue)
                ,(int)CBusQueue.head
                ,(int)CBusQueue.tail
                ));
    retVal = PutPriorityCBusTransactionImpl(pReq);
    if (!retVal)
    {
        TX_DEBUG_PRINT(("[MHL]:%d: PutPriorityCBusTransaction queue full, when adding event 0x%02X\n",iLine,(int)pReq->command));
    }
    return retVal;
}
#define PutPriorityCBusTransaction(pReq) PutPriorityCBusTransactionWrapper(pReq,__LINE__)

#define IncrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount++; TX_DEBUG_PRINT(("[MHL]:%d %s cbusReferenceCount:%d\n",(int)__LINE__,#func,(int)mhlTxConfig.cbusReferenceCount)); }
#define DecrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount--; TX_DEBUG_PRINT(("[MHL]:%d %s cbusReferenceCount:%d\n",(int)__LINE__,#func,(int)mhlTxConfig.cbusReferenceCount)); }

#define SetMiscFlag(func,x) { mhlTxConfig.miscFlags |=  (x); TX_DEBUG_PRINT(("[MHL]:%d %s set %s\n",(int)__LINE__,#func,#x)); }
#define ClrMiscFlag(func,x) { mhlTxConfig.miscFlags &= ~(x); TX_DEBUG_PRINT(("[MHL]:%d %s clr %s\n",(int)__LINE__,#func,#x)); }

//
// Functions used internally.
//
static bool_t SiiMhlTxSetDCapRdy( void );
/* static bool_t SiiMhlTxClrDCapRdy( void ); */
static bool_t SiiMhlTxSetPathEn(void );
static bool_t SiiMhlTxClrPathEn( void );
static bool_t  SiiMhlTxRapkSend( void );
static void MhlTxDriveStates( void );
static void MhlTxResetStates( void );
static bool_t MhlTxSendMscMsg ( uint8_t command, uint8_t cmdData );
extern uint8_t	rcpSupportTable [];

bool_t MhlTxCBusBusy(void)
{
    return ((QUEUE_DEPTH(CBusQueue) > 0)||SiiMhlTxDrvCBusBusy() || mhlTxConfig.cbusReferenceCount)?true:false;
}
///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxTmdsEnable
//
// Implements conditions on enabling TMDS output stated in MHL spec section 7.6.1
//
//
static void SiiMhlTxTmdsEnable(void)
{
	TX_DEBUG_PRINT( ("[MHL]: SiiMhlTxTmdsEnable\n"));
    	if (MHL_RSEN & mhlTxConfig.mhlHpdRSENflags)
    	{
    		TX_DEBUG_PRINT( ("\tMHL_RSEN\n"));
        	if (MHL_HPD & mhlTxConfig.mhlHpdRSENflags)
       	{
        		TX_DEBUG_PRINT( ("\t\tMHL_HPD\n"));
            		if (MHL_STATUS_PATH_ENABLED & mhlTxConfig.status_1)
            		{
            			TX_DEBUG_PRINT(("\t\t\tMHL_STATUS_PATH_ENABLED\n"));
                		SiiMhlTxDrvTmdsControl( true );
            		}
        	}
    	}
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxSetInt
//
// Set MHL defined INTERRUPT bits in peer's register set.
//
// This function returns true if operation was successfully performed.
//
//  regToWrite      Remote interrupt register to write
//
//  mask            the bits to write to that register
//
//  priority        0:  add to head of CBusQueue
//                  1:  add to tail of CBusQueue
//
static bool_t SiiMhlTxSetInt( uint8_t regToWrite,uint8_t  mask, uint8_t priorityLevel )
{
	cbus_req_t	req;
	bool_t retVal;

	// find the offset and bit position
	// and feed
    	req.retryCount  = 2;
	req.command     = MHL_SET_INT;
	req.offsetData  = regToWrite;
	req.payload_u.msgData[0]  = mask;
    	if (0 == priorityLevel)
    	{
        	retVal = PutPriorityCBusTransaction(&req);
    	}
    	else
    	{
        	retVal = PutNextCBusTransaction(&req);
    	}
    	return retVal;
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDoWriteBurst
//
static bool_t SiiMhlTxDoWriteBurst( uint8_t startReg, uint8_t *pData,uint8_t length )
{
    if (FLAGS_WRITE_BURST_PENDING & mhlTxConfig.miscFlags)
    {
	cbus_req_t	req;
       bool_t retVal;

    	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxDoWriteBurst startReg:%d length:%d\n",(int)__LINE__,(int)startReg,(int)length) );

       req.retryCount  = 1;
	req.command     = MHL_WRITE_BURST;
    	req.length      = length;
	req.offsetData  = startReg;
    	req.payload_u.pdatabytes  = pData;

       retVal = PutPriorityCBusTransaction(&req);
       ClrMiscFlag(MhlTxDriveStates, FLAGS_WRITE_BURST_PENDING)
       return retVal;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////
// SiiMhlTxRequestWriteBurst
//
bool_t SiiMhlTxRequestWriteBurst(void)
{
    bool_t retVal = false;

    if ((FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)||MhlTxCBusBusy())
    {
        TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxRequestWriteBurst failed FLAGS_SCRATCHPAD_BUSY \n",(int)__LINE__) );
    }
    else
    {
    	 TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxRequestWriteBurst, request sent\n",(int)__LINE__) );
        retVal =  SiiMhlTxSetInt(MHL_RCHANGE_INT,MHL_INT_REQ_WRT, 1);
    }

    return retVal;
}

///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxInitialize
//
// Sets the transmitter component firmware up for operation, brings up chip
// into power on state first and then back to reduced-power mode D3 to conserve
// power until an MHL cable connection has been established. If the MHL port is
// used for USB operation, the chip and firmware continue to stay in D3 mode.
// Only a small circuit in the chip observes the impedance variations to see if
// processor should be interrupted to continue MHL discovery process or not.
//
// interruptDriven		If true, MhlTx component will not look at its status
//						registers in a polled manner from timer handler
//						(SiiMhlGetEvents). It will expect that all device
//						events will result in call to the function
//						SiiMhlTxDeviceIsr() by host's hardware or software
//						(a master interrupt handler in host software can call
//						it directly). interruptDriven == true also implies that
//						the MhlTx component shall make use of AppDisableInterrupts()
//						and AppRestoreInterrupts() for any critical section work to
//						prevent concurrency issues.
//
//						When interruptDriven == false, MhlTx component will do
//						all chip status analysis via looking at its register
//						when called periodically into the function
//						SiiMhlGetEvents() described below.
//
// pollIntervalMs		This number should be higher than 0 and lower than
//						51 milliseconds for effective operation of the firmware.
//						A higher number will only imply a slower response to an
//						event on MHL side which can lead to violation of a
//						connection disconnection related timing or a slower
//						response to RCP messages.
//
//
//
//
void SiiMhlTxInitialize( bool_t interruptDriven, uint8_t pollIntervalMs )
{
	// Initialize queue of pending CBUS requests.
	CBusQueue.head = 0;
	CBusQueue.tail = 0;

	//
	// Remember mode of operation.
	//
	mhlTxConfig.interruptDriven = interruptDriven;
	mhlTxConfig.pollIntervalMs  = pollIntervalMs;

	MhlTxResetStates( );
	SiiMhlTxChipInitialize ();
}


///////////////////////////////////////////////////////////////////////////////
//
// rcpSupportTable
//
#define	MHL_MAX_RCP_KEY_CODE	(0x7F + 1)	// inclusive

uint8_t	rcpSupportTable [MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),		// 0x00 = Select
	(MHL_DEV_LD_GUI),		// 0x01 = Up
	(MHL_DEV_LD_GUI),		// 0x02 = Down
	(MHL_DEV_LD_GUI),		// 0x03 = Left
	(MHL_DEV_LD_GUI),		// 0x04 = Right
	0, 0, 0, 0,				// 05-08 Reserved
	(MHL_DEV_LD_GUI),		// 0x09 = Root Menu
	0, 0, 0,				// 0A-0C Reserved
	(MHL_DEV_LD_GUI),		// 0x0D = Select
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0E-1F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Numeric keys 0x20-0x29
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,						// 0x2A = Dot
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Enter key = 0x2B
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Clear key = 0x2C
	0, 0, 0,				// 2D-2F Reserved
	(MHL_DEV_LD_TUNER),		// 0x30 = Channel Up
	(MHL_DEV_LD_TUNER),		// 0x31 = Channel Dn
	(MHL_DEV_LD_TUNER),		// 0x32 = Previous Channel
	(MHL_DEV_LD_AUDIO),		// 0x33 = Sound Select
	0,						// 0x34 = Input Select
	0,						// 0x35 = Show Information
	0,						// 0x36 = Help
	0,						// 0x37 = Page Up
	0,						// 0x38 = Page Down
	0, 0, 0, 0, 0, 0, 0,	// 0x39-0x3F Reserved
	0,						// 0x40 = Undefined

	(MHL_DEV_LD_SPEAKER),	// 0x41 = Volume Up
	(MHL_DEV_LD_SPEAKER),	// 0x42 = Volume Down
	(MHL_DEV_LD_SPEAKER),	// 0x43 = Mute
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x44 = Play
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x45 = Stop
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x46 = Pause
	(MHL_DEV_LD_RECORD),	// 0x47 = Record
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x48 = Rewind
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x49 = Fast Forward
	(MHL_DEV_LD_MEDIA),		// 0x4A = Eject
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4B = Forward
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4C = Backward
	0, 0, 0,				// 4D-4F Reserved
	0,						// 0x50 = Angle
	0,						// 0x51 = Subpicture
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 52-5F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x60 = Play Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x61 = Pause the Play Function
	(MHL_DEV_LD_RECORD),	// 0x62 = Record Function
	(MHL_DEV_LD_RECORD),	// 0x63 = Pause the Record Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x64 = Stop Function

	(MHL_DEV_LD_SPEAKER),	// 0x65 = Mute Function
	(MHL_DEV_LD_SPEAKER),	// 0x66 = Restore Mute Function
	0, 0, 0, 0, 0, 0, 0, 0, 0, 	                        // 0x67-0x6F Undefined or reserved
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 		// 0x70-0x7F Undefined or reserved
};

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlGetEvents
//
// This is a function in MhlTx that must be called by application in a periodic
// fashion. The accuracy of frequency (adherence to the parameter pollIntervalMs)
// will determine adherence to some timings in the MHL specifications, however,
// MhlTx component keeps a tolerance of up to 50 milliseconds for most of the
// timings and deploys interrupt disabled mode of operation (applicable only to
// Sii 9244) for creating precise pulse of smaller duration such as 20 ms.
//
// This function does not return anything but it does modify the contents of the
// two pointers passed as parameter.
//
// It is advantageous for application to call this function in task context so
// that interrupt nesting or concurrency issues do not arise. In addition, by
// collecting the events in the same periodic polling mechanism prevents a call
// back from the MhlTx which can result in sending yet another MHL message.
//
// An example of this is responding back to an RCP message by another message
// such as RCPK or RCPE.
//
//
// *event		MhlTx returns a value in this field when function completes execution.
// 				If this field is 0, the next parameter is undefined.
//				The following values may be returned.
//
//

extern u32 int_count;
void SiiMhlGetEvents( uint8_t *event, uint8_t *eventParameter )
{
	//
	// If interrupts have not been routed to our ISR, manually call it here.
	//
	//if(false == mhlTxConfig.interruptDriven)
	if(int_count>0)
	{
		SiiMhlTxDeviceIsr();
		//g_mhl_inttimer.expires = jiffies + 1;
		//add_timer(&g_mhl_inttimer);
		int_count--;

	}
	MhlTxDriveStates( );

	*event = MHL_TX_EVENT_NONE;
	*eventParameter = 0;

	if( mhlTxConfig.mhlConnectionEvent )
	{
		TX_DEBUG_PRINT(("[MHL]: SiiMhlGetEvents mhlConnectionEvent\n"));

		// Consume the message
		mhlTxConfig.mhlConnectionEvent = false;

		//
		// Let app know the connection went away.
		//
		*event          = mhlTxConfig.mhlConnected;
		*eventParameter	= mhlTxConfig.mscFeatureFlag;

		// If connection has been lost, reset all state flags.
		if(MHL_TX_EVENT_DISCONNECTION == mhlTxConfig.mhlConnected)
		{
			MhlTxResetStates( );
		}
        else if (MHL_TX_EVENT_CONNECTION == mhlTxConfig.mhlConnected)
        {
            SiiMhlTxSetDCapRdy();
        }
	}
	else if( mhlTxConfig.mscMsgArrived )
	{
		TX_DEBUG_PRINT( ("[MHL]: SiiMhlGetEvents MSC MSG <%02X, %02X>\n",
							(int) ( mhlTxConfig.mscMsgSubCommand ),
							(int) ( mhlTxConfig.mscMsgData )) );

		// Consume the message
		mhlTxConfig.mscMsgArrived = false;

		//
		// Map sub-command to an event id
		//
		switch(mhlTxConfig.mscMsgSubCommand)
		{
			case	MHL_MSC_MSG_RAP:
				// RAP is fully handled here.
				//
				// Handle RAP sub-commands here itself
				//
				if( MHL_RAP_CONTENT_ON == mhlTxConfig.mscMsgData)
				{
                    			SiiMhlTxTmdsEnable();
				}
				else if( MHL_RAP_CONTENT_OFF == mhlTxConfig.mscMsgData)
				{
					SiiMhlTxDrvTmdsControl( false );
				}
				// Always RAPK to the peer
				SiiMhlTxRapkSend( );
				break;

			case	MHL_MSC_MSG_RCP:
				// If we get a RCP key that we do NOT support, send back RCPE
				// Do not notify app layer.
				if(MHL_LOGICAL_DEVICE_MAP & rcpSupportTable [mhlTxConfig.mscMsgData & 0x7F] )
				{
				*event          = MHL_TX_EVENT_RCP_RECEIVED;
				*eventParameter = mhlTxConfig.mscMsgData; // key code
				}
				else
				{
					// Save keycode to send a RCPK after RCPE.
					mhlTxConfig.mscSaveRcpKeyCode = mhlTxConfig.mscMsgData; // key code
					SiiMhlTxRcpeSend( RCPE_INEEFECTIVE_KEY_CODE );
				}
				break;

			case	MHL_MSC_MSG_RCPK:
				*event = MHL_TX_EVENT_RCPK_RECEIVED;
				*eventParameter = mhlTxConfig.mscMsgData; // key code
                		DecrementCBusReferenceCount(SiiMhlGetEvents)
                		mhlTxConfig.mscLastCommand = 0;
                		mhlTxConfig.mscMsgLastCommand = 0;

        	    		TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlGetEvents RCPK\n",(int)__LINE__) );
				break;

			case	MHL_MSC_MSG_RCPE:
				*event = MHL_TX_EVENT_RCPE_RECEIVED;
				*eventParameter = mhlTxConfig.mscMsgData; // status code
				break;

			case	MHL_MSC_MSG_RAPK:
				// Do nothing if RAPK comes, except decrement the reference counter
                		DecrementCBusReferenceCount(SiiMhlGetEvents)
                		mhlTxConfig.mscLastCommand = 0;
                		mhlTxConfig.mscMsgLastCommand = 0;
        	    		TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlGetEvents RAPK\n",(int)__LINE__) );
				break;

			default:
				// Any freak value here would continue with no event to app
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// MhlTxDriveStates
//
// This is an internal function to move the MSC engine to do the next thing
// before allowing the application to run RCP APIs.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
static void MhlTxDriveStates( void )
{

/*
// Polling POW bit at here only for old MHL dongle which used 9292 rev1.0 silicon (not used for the latest rev1.1 silicon)
#if (VBUS_POWER_CHK == ENABLE)
	// Source VBUS power check	// by oscar 20110118
	static uint8_t vbusCheckTime = 0;

	vbusCheckTime ++;
	if( vbusCheckTime == (uint8_t)(T_SRC_VBUS_POWER_CHK / 60))  // 2s period  // 60 = work_queue time
	{
		vbusCheckTime = 0;
		MHLPowerStatusCheck();
	}
#endif
*/

    // Discover timeout check	//xding
    if ((POWER_STATE_D0_MHL != fwPowerState) && haltimerelapsed( ELAPSED_TIMER1 ) )
    {
	printk("Discover timeout!\n");
	haltimerset( ELAPSED_TIMER1, 0 );
	MhlTxDrvProcessDisconnection();
    }

    // process queued CBus transactions
    if (QUEUE_DEPTH(CBusQueue) > 0)
            		{
        if (!SiiMhlTxDrvCBusBusy())
        {
        	int reQueueRequest = 0;
        	cbus_req_t *pReq = GetNextCBusTransaction(MhlTxDriveStates);
            	// coordinate write burst requests and grants.
            	if (MHL_SET_INT == pReq->command)
            	{
                if (MHL_RCHANGE_INT == pReq->offsetData)
                {
                    if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)
                    {
                        if (MHL_INT_REQ_WRT == pReq->payload_u.msgData[0])
                        {
                            reQueueRequest= 1;
            		}
                        else if (MHL_INT_GRT_WRT == pReq->payload_u.msgData[0])
        		{
                            reQueueRequest= 0;
                        }
                    }
                    else
	{
                        if (MHL_INT_REQ_WRT == pReq->payload_u.msgData[0])
                        {
                            IncrementCBusReferenceCount(MhlTxDriveStates)
                            SetMiscFlag(MhlTxDriveStates, FLAGS_SCRATCHPAD_BUSY)
                            SetMiscFlag(MhlTxDriveStates, FLAGS_WRITE_BURST_PENDING)
                        }
                        else if (MHL_INT_GRT_WRT == pReq->payload_u.msgData[0])
            		{
                            SetMiscFlag(MhlTxDriveStates, FLAGS_SCRATCHPAD_BUSY)
                        }
                    }
                }
            		}
            if (reQueueRequest)
        		{
                // send this one to the back of the line for later attempts
                if (pReq->retryCount-- > 0)
            			{
                    PutNextCBusTransaction(pReq);
                }
            			}
            			else
            			{
                if (MHL_MSC_MSG == pReq->command)
                {
                    mhlTxConfig.mscMsgLastCommand = pReq->payload_u.msgData[0];
                    mhlTxConfig.mscMsgLastData    = pReq->payload_u.msgData[1];
            			}
                else
                {
                    mhlTxConfig.mscLastOffset  = pReq->offsetData;
                    mhlTxConfig.mscLastData    = pReq->payload_u.msgData[0];
        		}
                mhlTxConfig.mscLastCommand = pReq->command;

                IncrementCBusReferenceCount(MhlTxDriveStates)
                SiiMhlTxDrvSendCbusCommand( pReq  );
            }
        }
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxMscCommandDone
//
// This function is called by the driver to inform of completion of last command.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
#define FLAG_OR_NOT(x) (FLAGS_HAVE_##x & mhlTxConfig.miscFlags)?#x:""
#define SENT_OR_NOT(x) (FLAGS_SENT_##x & mhlTxConfig.miscFlags)?#x:""

void	SiiMhlTxMscCommandDone( uint8_t data1 )
{
	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone. data1 = %02X\n",(int)__LINE__, (int) data1) );

    	DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
    	if ( MHL_READ_DEVCAP == mhlTxConfig.mscLastCommand )
	{
    		if(MHL_DEV_CATEGORY_OFFSET == mhlTxConfig.mscLastOffset)
		{
            		mhlTxConfig.miscFlags |= FLAGS_HAVE_DEV_CATEGORY;
        		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_CATEGORY\n",(int)__LINE__));

#if (VBUS_POWER_CHK == ENABLE)
		if( vbuspowerstate != (bool_t) ( data1 & MHL_DEV_CATEGORY_POW_BIT) )
		{
			vbuspowerstate = (bool_t) ( data1 & MHL_DEV_CATEGORY_POW_BIT);
			appvbuscontrol( vbuspowerstate );
		}
#endif

            		// OK to call this here, since requests always get queued and processed in the "foreground"
			SiiMhlTxReadDevcap( MHL_DEV_FEATURE_FLAG_OFFSET );
		}
    		else if(MHL_DEV_FEATURE_FLAG_OFFSET == mhlTxConfig.mscLastOffset)
		{
	            	mhlTxConfig.miscFlags |= FLAGS_HAVE_DEV_FEATURE_FLAGS;
	        	TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_FEATURE_FLAGS\n",(int)__LINE__));

			// Remember features of the peer
			mhlTxConfig.mscFeatureFlag	= data1;

			// These variables are used to remember if we issued a READ_DEVCAP
			   		//    or other MSC command
			// Since we are done, reset them.
			mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscLastOffset  = 0;

    			TX_DEBUG_PRINT( ("[MHL]:%d Peer's Feature Flag = %02X\n\n",(int)__LINE__, (int) data1) );
    		}
	}
	else if(MHL_WRITE_STAT == mhlTxConfig.mscLastCommand)
	{

	    	TX_DEBUG_PRINT( ("[MHL]: WRITE_STAT miscFlags: %02X\n\n", (int) mhlTxConfig.miscFlags) );
	        if (MHL_STATUS_REG_CONNECTED_RDY == mhlTxConfig.mscLastOffset)
        	{
	            if (MHL_STATUS_DCAP_RDY & mhlTxConfig.mscLastData)
	            {
	                	mhlTxConfig.miscFlags |= FLAGS_SENT_DCAP_RDY;
	            		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_SENT_DCAP_RDY\n",(int)__LINE__));
	            }
		 }
	        else if (MHL_STATUS_REG_LINK_MODE == mhlTxConfig.mscLastOffset)
	        {
	            if ( MHL_STATUS_PATH_ENABLED & mhlTxConfig.mscLastData)
        		{
	                 	mhlTxConfig.miscFlags |= FLAGS_SENT_PATH_EN;
	            		TX_DEBUG_PRINT(("[MHL]:%d SiiMhlTxMscCommandDone FLAGS_SENT_PATH_EN\n",(int)__LINE__));
	            }
        	}

    		mhlTxConfig.mscLastCommand = 0;
    		mhlTxConfig.mscLastOffset  = 0;
	}
	else if (MHL_MSC_MSG == mhlTxConfig.mscLastCommand)
    	{
    		if(MHL_MSC_MSG_RCPE == mhlTxConfig.mscMsgLastCommand)
		{
			//
			// RCPE is always followed by an RCPK with original key code that came.
			//
			if( siimhltxrcpksend( mhlTxConfig.mscSaveRcpKeyCode ) )
			{
		    	}
    		}
        	else
        	{
    	    		/*TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone default\n"
	    	            "\tmscLastCommand: 0x%02X \n"
	    	            "\tmscMsgLastCommand: 0x%02X mscMsgLastData: 0x%02X\n"
	                    "\tcbusReferenceCount: %d\n"
	    	            ,(int)__LINE__
	    	            ,(int)mhlTxConfig.mscLastCommand
	    	            ,(int)mhlTxConfig.mscMsgLastCommand
	    	            ,(int)mhlTxConfig.mscMsgLastData
	                    ,(int)mhlTxConfig.cbusReferenceCount
	    	            ) );*/
        	}
        	mhlTxConfig.mscLastCommand = 0;
    }
    else if (MHL_WRITE_BURST == mhlTxConfig.mscLastCommand)
    {
        TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone MHL_WRITE_BURST\n",(int)__LINE__ ) );
        mhlTxConfig.mscLastCommand = 0;
        mhlTxConfig.mscLastOffset  = 0;
        mhlTxConfig.mscLastData    = 0;

        // all CBus request are queued, so this is OK to call here
        // use priority 0 so that other queued commands don't interfere
        SiiMhlTxSetInt( MHL_RCHANGE_INT,MHL_INT_DSCR_CHG,0 );
    }
    else if (MHL_SET_INT == mhlTxConfig.mscLastCommand)
    {
        TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone MHL_SET_INT\n",(int)__LINE__ ) );
        if (MHL_RCHANGE_INT == mhlTxConfig.mscLastOffset)
        {
        	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone MHL_RCHANGE_INT\n",(int)__LINE__) );
            if (MHL_INT_DSCR_CHG == mhlTxConfig.mscLastData)
            {
                DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)  // this one is for the write burst request
                TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone MHL_INT_DSCR_CHG\n",(int)__LINE__) );
                ClrMiscFlag(SiiMhlTxMscCommandDone, FLAGS_SCRATCHPAD_BUSY)
            }
        }
			// Once the command has been sent out successfully, forget this case.
        mhlTxConfig.mscLastCommand = 0;
        mhlTxConfig.mscLastOffset  = 0;
        mhlTxConfig.mscLastData    = 0;
    }
    else
    {
    	/* TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone default\n"
    	            "\tmscLastCommand: 0x%02X mscLastOffset: 0x%02X\n"
                    "\tcbusReferenceCount: %d\n"
    	            ,(int)__LINE__
    	            ,(int)mhlTxConfig.mscLastCommand
    	            ,(int)mhlTxConfig.mscLastOffset
                    ,(int)mhlTxConfig.cbusReferenceCount
    	            ) );*/
   }
    if (!(FLAGS_RCP_READY & mhlTxConfig.miscFlags))
    {
    	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscCommandDone. have(%s %s) sent(%s %s)\n"
    	                    , (int) __LINE__
                            , FLAG_OR_NOT(DEV_CATEGORY)
                            , FLAG_OR_NOT(DEV_FEATURE_FLAGS)
                            , SENT_OR_NOT(PATH_EN)
                            , SENT_OR_NOT(DCAP_RDY)
    	));
        if (FLAGS_HAVE_DEV_CATEGORY & mhlTxConfig.miscFlags)
        {
            if (FLAGS_HAVE_DEV_FEATURE_FLAGS& mhlTxConfig.miscFlags)
            {
                if (FLAGS_SENT_PATH_EN & mhlTxConfig.miscFlags)
                {
                    if (FLAGS_SENT_DCAP_RDY & mhlTxConfig.miscFlags)
                    {
                        mhlTxConfig.miscFlags |= FLAGS_RCP_READY;
                		// Now we can entertain App commands for RCP
                		// Let app know this state
                		mhlTxConfig.mhlConnectionEvent = true;
                		mhlTxConfig.mhlConnected = MHL_TX_EVENT_RCP_READY;
                    }
                }
            }
		}
    }
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxMscWriteBurstDone
//
// This function is called by the driver to inform of completion of a write burst.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
void	SiiMhlTxMscWriteBurstDone( uint8_t data1 )
{
#define WRITE_BURST_TEST_SIZE 16
	uint8_t temp[WRITE_BURST_TEST_SIZE];
	uint8_t i;
    	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxMscWriteBurstDone(%02X) \"",(int)__LINE__,(int)data1 ) );
    	SiiMhlTxDrvGetScratchPad(0,temp,WRITE_BURST_TEST_SIZE);
    	for (i = 0; i < WRITE_BURST_TEST_SIZE ; ++i)
    	{
        	if (temp[i]>=' ')
        	{
            		TX_DEBUG_PRINT(("%02X %c ",(int)temp[i],temp[i]));
        	}
        	else
        	{
            		TX_DEBUG_PRINT(("%02X . ",(int)temp[i]));
		}
	}
    	TX_DEBUG_PRINT(("\"\n"));
}


///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlMscMsg
//
// This function is called by the driver to inform of arrival of a MHL MSC_MSG
// such as RCP, RCPK, RCPE. To quickly return back to interrupt, this function
// remembers the event (to be picked up by app later in task context).
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing of its own,
//
// No printfs.
//
// Application shall not call this function.
//
void	SiiMhlTxGotMhlMscMsg( uint8_t subCommand, uint8_t cmdData )
{
	// Remeber the event.
	mhlTxConfig.mscMsgArrived		= true;
	mhlTxConfig.mscMsgSubCommand	= subCommand;
	mhlTxConfig.mscMsgData			= cmdData;
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlIntr
//
// This function is called by the driver to inform of arrival of a MHL INTERRUPT.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
void	SiiMhlTxGotMhlIntr( uint8_t intr_0, uint8_t intr_1 )
{
	TX_DEBUG_PRINT( ("[MHL]: INTERRUPT Arrived. %02X, %02X\n", (int) intr_0, (int) intr_1) );

	//
	// Handle DCAP_CHG INTR here
	//
	if(MHL_INT_DCAP_CHG & intr_0)
	{
        	// OK to call this here, since all requests are queued
		SiiMhlTxReadDevcap( MHL_DEV_CATEGORY_OFFSET );
	}

	if( MHL_INT_DSCR_CHG & intr_0)
    	{
        	SiiMhlTxDrvGetScratchPad(0,mhlTxConfig.localScratchPad,sizeof(mhlTxConfig.localScratchPad));
        // remote WRITE_BURST is complete
        ClrMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
    	}
	if( MHL_INT_REQ_WRT  & intr_0)
    	{

        	// this is a request from the sink device.
        	if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)
        	{
            		// use priority 1 to defer sending grant until
           	 	//  local traffic is done
            		SiiMhlTxSetInt( MHL_RCHANGE_INT, MHL_INT_GRT_WRT,1);
        	}
       	else
       	{
            		SetMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
            		// OK to call this here, since all requests are queued
            		// use priority 0 to respond immediately
            		SiiMhlTxSetInt( MHL_RCHANGE_INT, MHL_INT_GRT_WRT,0);
       	}
    	}
	if( MHL_INT_GRT_WRT  & intr_0)
    	{
    		uint8_t length =sizeof(mhlTxConfig.localScratchPad);
        	TX_DEBUG_PRINT(("[MHL]:%d MHL_INT_GRT_WRT length:%d\n",(int)__LINE__,(int)length));
        	SiiMhlTxDoWriteBurst(0x40, mhlTxConfig.localScratchPad, length);
    	}

    // removed "else", since interrupts are not mutually exclusive of each other.
	if(MHL_INT_EDID_CHG & intr_1)
	{
		// force upstream source to read the EDID again.
		// Most likely by appropriate togggling of HDMI HPD
		SiiMhlTxDrvNotifyEdidChange ( );
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlStatus
//
// This function is called by the driver to inform of arrival of a MHL STATUS.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
void	SiiMhlTxGotMhlStatus( uint8_t status_0, uint8_t status_1 )
{
//	TX_DEBUG_PRINT( ("[MHL]: STATUS Arrived. %02X, %02X\n", (int) status_0, (int) status_1) );
	//
	// Handle DCAP_RDY STATUS here itself
	//
	uint8_t StatusChangeBitMask0,StatusChangeBitMask1;
    	StatusChangeBitMask0 = status_0 ^ mhlTxConfig.status_0;
    	StatusChangeBitMask1 = status_1 ^ mhlTxConfig.status_1;
	// Remember the event.   (other code checks the saved values, so save the values early, but not before the XOR operations above)
	mhlTxConfig.status_0 = status_0;
	mhlTxConfig.status_1 = status_1;

	if(MHL_STATUS_DCAP_RDY & StatusChangeBitMask0)
	{
        	TX_DEBUG_PRINT( ("[MHL]: DCAP_RDY changed\n") );
	if(MHL_STATUS_DCAP_RDY & status_0)
	{
            // OK to call this here since all requests are queued
    		SiiMhlTxReadDevcap( MHL_DEV_CATEGORY_OFFSET );
	}
	}

    	// did PATH_EN change?
	if(MHL_STATUS_PATH_ENABLED & StatusChangeBitMask1)
    	{
        	TX_DEBUG_PRINT( ("[MHL]: PATH_EN changed\n") );
        	if(MHL_STATUS_PATH_ENABLED & status_1)
        	{
            		// OK to call this here since all requests are queued
            		SiiMhlTxSetPathEn();
        	}
        	else
    		{
            		// OK to call this here since all requests are queued
            		SiiMhlTxClrPathEn();
        	}
    	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpSend
//
// This function checks if the peer device supports RCP and sends rcpKeyCode. The
// function will return a value of true if it could successfully send the RCP
// subcommand and the key code. Otherwise false.
//
// The followings are not yet utilized.
//
// (MHL_FEATURE_RAP_SUPPORT & mhlTxConfig.mscFeatureFlag))
// (MHL_FEATURE_SP_SUPPORT & mhlTxConfig.mscFeatureFlag))
//
//
bool_t SiiMhlTxRcpSend( uint8_t rcpKeyCode )
{
	bool_t retVal;
	//
	// If peer does not support do not send RCP or RCPK/RCPE commands
	//

	if((0 == (MHL_FEATURE_RCP_SUPPORT & mhlTxConfig.mscFeatureFlag))
	    ||
        !(FLAGS_RCP_READY & mhlTxConfig.miscFlags)
		)
	{
    		TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxRcpSend failed\n",(int)__LINE__) );
		retVal=false;
	}

	retVal=MhlTxSendMscMsg ( MHL_MSC_MSG_RCP, rcpKeyCode );
    	if(retVal)
    	{
    		TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxRcpSend\n",(int)__LINE__) );
        	IncrementCBusReferenceCount(SiiMhlTxRcpSend)
	}
    	return retVal;
}

///////////////////////////////////////////////////////////////////////////////
//
// siimhltxrcpksend
//
// This function sends RCPK to the peer device.
//
bool_t siimhltxrcpksend( uint8_t rcpKeyCode )
{
	return	( MhlTxSendMscMsg ( MHL_MSC_MSG_RCPK, rcpKeyCode ) );
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRapkSend
//
// This function sends RAPK to the peer device.
//
static bool_t SiiMhlTxRapkSend( void )
{
	return ( MhlTxSendMscMsg ( MHL_MSC_MSG_RAPK, 0 ) );
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpeSend
//
// The function will return a value of true if it could successfully send the RCPE
// subcommand. Otherwise false.
//
// When successful, MhlTx internally sends RCPK with original (last known)
// keycode.
//
bool_t SiiMhlTxRcpeSend( uint8_t rcpeErrorCode )
{
	return ( MhlTxSendMscMsg ( MHL_MSC_MSG_RCPE, rcpeErrorCode ) );
}

/*
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRapSend
//
// This function checks if the peer device supports RAP and sends rcpKeyCode. The
// function will return a value of true if it could successfully send the RCP
// subcommand and the key code. Otherwise false.
//

bool_t SiiMhlTxRapSend( uint8_t rapActionCode )
{
bool_t retVal;
    if (!(FLAGS_RCP_READY & mhlTxConfig.miscFlags))
    {
    	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxRapSend failed\n",(int)__LINE__) );
        retVal = false;
    }
    else
    {
    	retVal = MhlTxSendMscMsg ( MHL_MSC_MSG_RAP, rapActionCode );
        if(retVal)
        {
            IncrementCBusReferenceCount
            TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxRapSend\n",(int)__LINE__) );
        }
    }
    return retVal;
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlWriteBurst
//
// This function is called by the driver to inform of arrival of a scratchpad data.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
// Application shall not call this function.
//
void	SiiMhlTxGotMhlWriteBurst( uint8_t *spadArray )
{
}
*/
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxSetStatus
//
// Set MHL defined STATUS bits in peer's register set.
//
// register	    MHLRegister to write
//
// value        data to write to the register
//
static bool_t SiiMhlTxSetStatus( uint8_t regToWrite, uint8_t value )
{
	cbus_req_t	req;
    	bool_t retVal;

	// find the offset and bit position
	// and feed
    	req.retryCount  = 2;
	req.command     = MHL_WRITE_STAT;
	req.offsetData  = regToWrite;
	req.payload_u.msgData[0]  = value;

    	TX_DEBUG_PRINT( ("[MHL]:%d SiiMhlTxSetStatus\n",(int)__LINE__) );
    	retVal = PutNextCBusTransaction(&req);
    	return retVal;
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxSetDCapRdy
//
static bool_t SiiMhlTxSetDCapRdy( void )
{
    mhlTxConfig.connectedReady |= MHL_STATUS_DCAP_RDY;   // update local copy
    return SiiMhlTxSetStatus( MHL_STATUS_REG_CONNECTED_RDY, mhlTxConfig.connectedReady);
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxClrDCapRdy
//
#if 0
static bool_t SiiMhlTxClrDCapRdy( void )
{
    mhlTxConfig.connectedReady &= ~MHL_STATUS_DCAP_RDY;  // update local copy
    return SiiMhlTxSetStatus( MHL_STATUS_REG_CONNECTED_RDY, mhlTxConfig.connectedReady);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
//  SiiMhlTxSendLinkMode
//
static bool_t SiiMhlTxSendLinkMode(void)
{
    return SiiMhlTxSetStatus( MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxSetPathEn
//
static bool_t SiiMhlTxSetPathEn(void )
{
	TX_DEBUG_PRINT( ("[MHL]: SiiMhlTxSetPathEn\n"));
    	SiiMhlTxTmdsEnable();
    	mhlTxConfig.linkMode |= MHL_STATUS_PATH_ENABLED;     // update local copy
    	return SiiMhlTxSetStatus( MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxClrPathEn
//
static bool_t SiiMhlTxClrPathEn( void )
{
	TX_DEBUG_PRINT( ("[MHL]: SiiMhlTxClrPathEn\n"));
    	SiiMhlTxDrvTmdsControl( false );
    	mhlTxConfig.linkMode &= ~MHL_STATUS_PATH_ENABLED;    // update local copy
    	return SiiMhlTxSetStatus( MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxReadDevcap
//
// This function sends a READ DEVCAP MHL command to the peer.
// It  returns true if successful in doing so.
//
// The value of devcap should be obtained by making a call to SiiMhlGetEvents()
//
// offset		Which byte in devcap register is required to be read. 0..0x0E
//
bool_t SiiMhlTxReadDevcap( uint8_t offset )
{
	cbus_req_t	req;
	TX_DEBUG_PRINT( ("[MHL]: SiiMhlTxReadDevcap\n"));
	//
	// Send MHL_READ_DEVCAP command
	//
    	req.retryCount  = 2;
	req.command     = MHL_READ_DEVCAP;
	req.offsetData  = offset;
    	req.payload_u.msgData[0]  = 0;  // do this to avoid confusion

    	return PutNextCBusTransaction(&req);
}

///////////////////////////////////////////////////////////////////////////////
//
// MhlTxSendMscMsg
//
// This function sends a MSC_MSG command to the peer.
// It  returns true if successful in doing so.
//
// The value of devcap should be obtained by making a call to SiiMhlGetEvents()
//
// offset		Which byte in devcap register is required to be read. 0..0x0E
//
static bool_t MhlTxSendMscMsg ( uint8_t command, uint8_t cmdData )
{
	cbus_req_t	req;
	uint8_t		ccode;

	//
	// Send MSC_MSG command
	//
	// Remember last MSC_MSG command (RCPE particularly)
	//
	req.retryCount  = 2;
	req.command     = MHL_MSC_MSG;
	req.payload_u.msgData[0]  = command;
	req.payload_u.msgData[1]  = cmdData;
	ccode = PutNextCBusTransaction(&req);
	return( (bool_t) ccode );
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxNotifyConnection
//
//
void	SiiMhlTxNotifyConnection( bool_t mhlConnected )
{
	mhlTxConfig.mhlConnectionEvent = true;

	TX_DEBUG_PRINT(("[MHL]: SiiMhlTxNotifyConnection MSC_STATE_IDLE %01X\n", (int) mhlConnected ));

	if(mhlConnected)
	{
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_CONNECTION;
		mhlTxConfig.mhlHpdRSENflags |= MHL_RSEN;
		SiiMhlTxTmdsEnable();
		SiiMhlTxSendLinkMode();
	}
	else
	{
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_DISCONNECTION;
		mhlTxConfig.mhlHpdRSENflags &= ~MHL_RSEN;
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxNotifyDsHpdChange
// Driver tells about arrival of SET_HPD or CLEAR_HPD by calling this function.
//
// Turn the content off or on based on what we got.
//
void	SiiMhlTxNotifyDsHpdChange( uint8_t dsHpdStatus )
{
	if( 0 == dsHpdStatus )
	{
		TX_DEBUG_PRINT(("[MHL]: Disable TMDS\n"));
	    TX_DEBUG_PRINT(("[MHL]: DsHPD OFF\n"));
            mhlTxConfig.mhlHpdRSENflags &= ~MHL_HPD;
		SiiMhlTxDrvTmdsControl( false );
	}
	else
	{
		TX_DEBUG_PRINT(("[MHL]: Enable TMDS\n"));
	    TX_DEBUG_PRINT(("[MHL]: DsHPD ON\n"));
        mhlTxConfig.mhlHpdRSENflags |= MHL_HPD;
        SiiMhlTxTmdsEnable();
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// MhlTxResetStates
//
// Application picks up mhl connection and rcp events at periodic intervals.
// Interrupt handler feeds these variables. Reset them on disconnection.
//
static void	MhlTxResetStates( void )
{
	mhlTxConfig.mhlConnectionEvent	= false;
	mhlTxConfig.mhlConnected		= MHL_TX_EVENT_DISCONNECTION;
	mhlTxConfig.mhlHpdRSENflags    &= ~(MHL_RSEN | MHL_HPD);
	mhlTxConfig.mscMsgArrived		= false;

	mhlTxConfig.status_0			= 0;
	mhlTxConfig.status_1			= 0;
	mhlTxConfig.connectedReady		= 0;
	mhlTxConfig.linkMode			= 3; // indicate normal (24-bit) mode
	mhlTxConfig.cbusReferenceCount	= 0;
	mhlTxConfig.miscFlags			= 0;
	mhlTxConfig.mscLastCommand		= 0;
	mhlTxConfig.mscMsgLastCommand	= 0;
}

#if (VBUS_POWER_CHK == ENABLE)
///////////////////////////////////////////////////////////////////////////////
//
// Function Name: MHLPowerStatusCheck()
//
// Function Description: Check MHL device (dongle or sink) power status.
//
void MHLPowerStatusCheck (void)
{
	static uint8_t DevCatPOWValue = 0;
	uint8_t RegValue;

	if( POWER_STATE_D0_MHL == fwPowerState )
	{
		WriteByteCBUS( REG_CBUS_PRI_ADDR_CMD, MHL_DEV_CATEGORY_OFFSET );	// DevCap 0x02
		WriteByteCBUS( REG_CBUS_PRI_START, MSC_START_BIT_READ_REG ); // execute DevCap reg read command

		RegValue = ReadByteCBUS( REG_CBUS_PRI_RD_DATA_1ST );

		if( DevCatPOWValue != (RegValue & MHL_DEV_CATEGORY_POW_BIT) )
		{
			DevCatPOWValue = RegValue & MHL_DEV_CATEGORY_POW_BIT;
			TX_DEBUG_PRINT(("[MHL]: DevCapReg0x02=0x%02X, POW bit Changed...\n", (int)RegValue));

			if( vbuspowerstate != (bool_t) ( DevCatPOWValue ) )
			{
				vbuspowerstate = (bool_t) ( DevCatPOWValue );
				appvbuscontrol( vbuspowerstate );
			}
		}
	}
}
#endif

int mhl_usb_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	int rtn = NOTIFY_DONE;

	switch (event) {
	case USB_DOCK_INSERT:
		TX_DEBUG_PRINT(("##########MHL recieve usb dock insert\n"));
		dock_inserted = true;
		rtn = NOTIFY_OK;
		break;
	case USB_DOCK_REMOVE:
		TX_DEBUG_PRINT(("##########MHL recieve usb dock remove\n"));
		dock_inserted = false;
		SwitchToD0();
		SwitchToD3();
		rtn = NOTIFY_OK;
		break;
	case USB_HOST_REMOVE:
		if(mhl_processed) {
			TX_DEBUG_PRINT(("##########MHL recieve usb host remove\n"));
			mhl_processed = false;
			SwitchToD0();
			SwitchToD3();
		}
		rtn = NOTIFY_OK;
		break;
	default:
		break;
	}
	return rtn;
}
