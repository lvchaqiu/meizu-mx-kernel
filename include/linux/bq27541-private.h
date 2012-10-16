/*
 * bq27541-private.h - Driver for the TI BQ27541
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author: Chwei   <Chwei@meizu.com>
 *
 * This program is not provided / owned by Maxim Integrated Products.
 *
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
 *
 * Datasheets:
 * http://www.ti.com/product/bq27541-v200
 *
 */
#ifndef __LINUX_MFD_BQ27541_PRI_H__
#define __LINUX_MFD_BQ27541_PRI_H__

#define	IIC_ADDR	(0x55)
#define	BQ27541_ID 	0x0541

#define AVERAGE_DISCHARGE_CURRENT_MA       -250	/* The average discharge current value. USER CONFIG: AtRate setting (mA)*/

enum bq27541_data_cmd_reg {
	 bq27541CMD_CNTL_LSB  		= 0x00,
	 bq27541CMD_CNTL_MSB  	= 0x01,
	 bq27541CMD_AR_LSB    		= 0x02,
	 bq27541CMD_AR_MSB    		= 0x03,
	 bq27541CMD_ARTTE_LSB 	= 0x04,
	 bq27541CMD_ARTTE_MSB 	= 0x05,
	 bq27541CMD_TEMP_LSB  	= 0x06,
	 bq27541CMD_TEMP_MSB  	= 0x07,
	 bq27541CMD_VOLT_LSB  		= 0x08,
	 bq27541CMD_VOLT_MSB  	= 0x09,
	 bq27541CMD_FLAGS_LSB 	= 0x0A,
	 bq27541CMD_FLAGS_MSB 	= 0x0B,
	 bq27541CMD_NAC_LSB   		= 0x0C,
	 bq27541CMD_NAC_MSB   		= 0x0D,
	 bq27541CMD_FAC_LSB   		= 0x0E,
	 bq27541CMD_FAC_MSB   		= 0x0F,
	 bq27541CMD_RM_LSB    		= 0x10,
	 bq27541CMD_RM_MSB    		= 0x11,
	 bq27541CMD_FCC_LSB   		= 0x12,
	 bq27541CMD_FCC_MSB   		= 0x13,
	 bq27541CMD_AI_LSB    		= 0x14,
	 bq27541CMD_AI_MSB    		= 0x15,
	 bq27541CMD_TTE_LSB   		= 0x16,
	 bq27541CMD_TTE_MSB   		= 0x17,
	 bq27541CMD_TTF_LSB   		= 0x18,
	 bq27541CMD_TTF_MSB   		= 0x19,
	 bq27541CMD_SI_LSB    		= 0x1A,
	 bq27541CMD_SI_MSB    		= 0x1B,
	 bq27541CMD_STTE_LSB  		= 0x1C,
	 bq27541CMD_STTE_MSB  	= 0x1D,
	 bq27541CMD_MLI_LSB   		= 0x1E,
	 bq27541CMD_MLI_MSB   		= 0x1F,
	 bq27541CMD_MLTTE_LSB 	= 0x20,
	 bq27541CMD_MLTTE_MSB 	= 0x21,
	 bq27541CMD_AE_LSB    		= 0x22,
	 bq27541CMD_AE_MSB    		= 0x23,
	 bq27541CMD_AP_LSB    		= 0x24,
	 bq27541CMD_AP_MSB    		= 0x25,
	 bq27541CMD_TTECP_LSB 	= 0x26,
	 bq27541CMD_TTECP_MSB 	= 0x27,
	 bq27541CMD_RSVD_LSB  		= 0x28,
	 bq27541CMD_RSVD_MSB  	= 0x29,
	 bq27541CMD_CC_LSB    		= 0x2A,
	 bq27541CMD_CC_MSB    		= 0x2B,
	 bq27541CMD_SOC_LSB   		= 0x2C,
	 bq27541CMD_SOC_MSB   		= 0x2D,
	 bq27541CMD_DCAP_LSB  	= 0x3C,
	 bq27541CMD_DCAP_MSB  	= 0x3D,
	 bq27541CMD_DFCLS     		= 0x3E,
	 bq27541CMD_DFBLK     		= 0x3F,
	 bq27541CMD_ADF       		= 0x40,
	 bq27541CMD_ACKSDFD   		= 0x54,
	 bq27541CMD_DFDCKS    		= 0x60,
	 bq27541CMD_DFDCNTL   		= 0x61,
	 bq27541CMD_DNAMELEN  	= 0x62,
	 bq27541CMD_DNAME     		= 0x63,
	 BQ27541_DATA_CMD_REG_END,
};

/* Slave addr = 0x90: Haptic */
enum bq27541_data_flash_reg {
	 bq27541CMD_CNTL_SUB_CONTROL_STATUS 		= 0x0000, //Reports the status of DF Checksum, Hibernate, IT, etc.
	 bq27541CMD_CNTL_SUB_DEVICE_TYPE 			= 0x0001, //Reports the device type of 	= 0x0541 (indicating bq27541)
	 bq27541CMD_CNTL_SUB_FW_VERSION 			= 0x0002, //Reports the firmware version on the device type
	 bq27541CMD_CNTL_SUB_HW_VERSION 			= 0x0003, //Reports the hardware version of the device type
	 bq27541CMD_CNTL_SUB_DF_CHECKSUM 			= 0x0004, //Enables a data flash checksum to be generated and reports on a read
	 bq27541CMD_CNTL_SUB_RESET_DATA 			= 0x0005, //Returns reset data
	 bq27541CMD_CNTL_SUB_Reserved 				= 0x0006, //Not to be used
	 bq27541CMD_CNTL_SUB_PREV_MACWRITE 		= 0x0007, //Returns previous MAC command code
	 bq27541CMD_CNTL_SUB_CHEM_ID 				= 0x0008, //Reports the chemical identifier of the Impedance Track? configuration
	 bq27541CMD_CNTL_SUB_DF_VERSION 			= 0x000C, //Reports the data flash version on the device
	 bq27541CMD_CNTL_SUB_SET_FULLSLEEP 			= 0x0010, //Set the [FullSleep] bit in Control Status register to 1
	 bq27541CMD_CNTL_SUB_SET_HIBERNATE 			= 0x0011, //Forces CONTROL_STATUS [HIBERNATE] to 1
	 bq27541CMD_CNTL_SUB_CLEAR_HIBERNATE 		= 0x0012, //Forces CONTROL_STATUS [HIBERNATE] to 0
	 bq27541CMD_CNTL_SUB_SET_SHUTDOWN 			= 0x0013, //Enables the SE pin to change state
	 bq27541CMD_CNTL_SUB_CLEAR_SHUTDOWN 		= 0x0014, //Disables the SE pin from changing state
	 bq27541CMD_CNTL_SUB_SET_HDQINTEN 			= 0x0015, //Forces CONTROL_STATUS [HDQIntEn] to 1
	 bq27541CMD_CNTL_SUB_CLEAR_HDQINTEN 		= 0x0016, //Forces CONTROL_STATUS [HDQIntEn] to 0
	 bq27541CMD_CNTL_SUB_SEALED 					= 0x0020, //Places the bq27541 is SEALED access mode
	 bq27541CMD_CNTL_SUB_IT_ENABLE 				= 0x0021, //Enables the Impedance Track? algorithm
	 bq27541CMD_CNTL_SUB_CAL_MODE 				= 0x0040, //Places the bq27541 in calibration mode
	 bq27541CMD_CNTL_SUB_RESET 					= 0x0041, //Forces a full reset of the bq27541
	 BQ27541_DATA_FLASH_REG_END,
};
#endif /*  __LINUX_MFD_BQ27541_PRI_H__ */
