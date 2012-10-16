#ifndef	__LIS3DH_H__
#define	__LIS3DH_H__

#include <linux/ioctl.h>

#define	LIS3DH_ACC_DEV_NAME	"lis3dh"

/************************************************/
/* 	Accelerometer defines section	 	*/
/************************************************/

/* Accelerometer Sensor Full Scale */
#define	LIS3DH_ACC_FS_MASK		0x30
#define LIS3DH_ACC_G_2G 		0x00
#define LIS3DH_ACC_G_4G 		0x10
#define LIS3DH_ACC_G_8G 		0x20
#define LIS3DH_ACC_G_16G		0x30

#define SENSITIVITY_2G     1    /*mg/LSB*/ 
#define SENSITIVITY_4G     2    /*mg/LSB*/ 
#define SENSITIVITY_8G     4    /*mg/LSB*/ 
#define SENSITIVITY_16G    12   /*mg/LSB*/ 

/* Accelerometer Sensor Operating Mode */
#define LIS3DH_ACC_ENABLE  0x01 
#define LIS3DH_ACC_DISABLE 0x00 

#define HIGH_RESOLUTION    0x08 

#define AXISDATA_REG       0x28 
#define WHOAMI_LIS3DH_ACC  0x33 /* Expected content for WAI */ 

/*CONTROL REGISTERS*/
#define WHO_AM_I     0x0F /* WhoAmI register */      
#define TEMP_CFG_REG 0x1F /* temper sensor control reg */

/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define CTRL_REG1  0x20 /* control reg 1 */ 
#define CTRL_REG2  0x21 /* control reg 2 */ 
#define CTRL_REG3  0x22 /* control reg 3 */ 
#define CTRL_REG4  0x23 /* control reg 4 */ 
#define CTRL_REG5  0x24 /* control reg 5 */ 
#define CTRL_REG6  0x25 /* control reg 6 */ 
#define STATUS_REG 0x27 

#define FIFO_CTRL_REG 0x2E /* FiFo control reg */ 

#define INT_CFG1 0x30 /* interrupt 1 config    */ 
#define INT_SRC1 0x31 /* interrupt 1 source    */ 
#define INT_THS1 0x32 /* interrupt 1 threshold */ 
#define INT_DUR1 0x33 /* interrupt 1 duration  */ 

#define TT_CFG  0x38 /* tap config    */      
#define TT_SRC  0x39 /* tap source    */      
#define TT_THS  0x3A /* tap threshold */      
#define TT_LIM  0x3B /* tap time      limit   */ 
#define TT_TLAT 0x3C /* tap time      latency */ 
#define TT_TW   0x3D /* tap time      window  */ 

#define ENABLE_HIGH_RESOLUTION	1

#define LIS3DH_ACC_PM_OFF          0x00 
#define LIS3DH_ACC_ENABLE_ALL_AXES 0x07 

#define PMODE_MASK 0x08 
#define ODR_MASK   0XF0 

#define ODR1    0x10 /* 1Hz    output data rate */ 
#define ODR10   0x20 /* 10Hz   output data rate */ 
#define ODR25   0x30 /* 25Hz   output data rate */ 
#define ODR50   0x40 /* 50Hz   output data rate */ 
#define ODR100  0x50 /* 100Hz  output data rate */ 
#define ODR200  0x60 /* 200Hz  output data rate */ 
#define ODR400  0x70 /* 400Hz  output data rate */ 
#define ODR1250 0x90 /* 1250Hz output data rate */ 

#define IA 0x40 
#define ZH 0x20 
#define ZL 0x10 
#define YH 0x08 
#define YL 0x04 
#define XH 0x02 
#define XL 0x01 

/* CTRL REG BITS*/
#define CTRL_REG3_I1_AOI1  0x40 
#define CTRL_REG6_I2_TAPEN 0x80 
#define CTRL_REG6_HLACTIVE 0x02 

#define NO_MASK             0xFF    
#define INT1_DURATION_MASK  0x7F    
#define INT1_THRESHOLD_MASK 0x7F    
#define TAP_CFG_MASK        0x3F    
#define TAP_THS_MASK        0x7F    
#define TAP_TLIM_MASK       0x7F    
#define TAP_TLAT_MASK       NO_MASK 
#define TAP_TW_MASK         NO_MASK 

/* TAP_SOURCE_REG BIT */
#define DTAP               0x20 
#define STAP               0x10 
#define SIGNTAP            0x08 
#define ZTAP               0x04 
#define YTAP               0x02 
#define XTAZ               0x01 

#define FUZZ               0    
#define FLAT               0    
#define I2C_RETRY_DELAY    2    
#define I2C_RETRIES        10   
#define I2C_AUTO_INCREMENT 0x80 

/* RESUME STATE INDICES */
#define RES_CTRL_REG1     0  
#define RES_CTRL_REG2     1  
#define RES_CTRL_REG3     2  
#define RES_CTRL_REG4     3  
#define RES_CTRL_REG5     4  
#define RES_CTRL_REG6     5  

#define RES_INT_CFG1      6  
#define RES_INT_THS1      7  
#define RES_INT_DUR1      8  

#define RES_TT_CFG        9  
#define RES_TT_THS        10 
#define RES_TT_LIM        11 
#define RES_TT_TLAT       12 
#define RES_TT_TW         13 

#define RES_TEMP_CFG_REG  14 
#define RES_REFERENCE_REG 15 
#define RES_FIFO_CTRL_REG 16 

#define RESUME_ENTRIES    17 
/* end RESUME STATE INDICES */

#define OUT_ADC3_L 0x0C 
#define OUT_ADC3_H 0x0D 

/*ioctl cmds*/
#define LIS3DH_IOCTL_BASE 'l'
#define LIS3DH_IOCTL_SET_ENABLE 	_IOW(LIS3DH_IOCTL_BASE, 0, int)
#define LIS3DH_IOCTL_GET_ENABLE 	_IOR(LIS3DH_IOCTL_BASE, 1, int)
#define LIS3DH_IOCTL_SET_DELAY	   	_IOW(LIS3DH_IOCTL_BASE, 2, int)
#define LIS3DH_IOCTL_GET_DELAY		_IOR(LIS3DH_IOCTL_BASE, 3, int)
#define LIS3DH_IOCTL_READ_ACCEL_XYZ  _IOR(LIS3DH_IOCTL_BASE, 4, int[3])
#define LIS3DH_IOCTL_GET_TEMP		_IOR(LIS3DH_IOCTL_BASE, 5, int)
#define LIS3DH_IOCTL_SELFTEST		_IOR(LIS3DH_IOCTL_BASE, 6, int)
#define LIS3DH_IOCTL_GET_SUSPEND_STATUS _IOR(LIS3DH_IOCTL_BASE, 7, int)

#ifdef	__KERNEL__
struct lis3dh_acc_platform_data {
	int poll_interval;
	int min_interval;

	u8 g_range;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};
#endif	/* __KERNEL__ */

#endif	/* __LIS3DH_H__ */
