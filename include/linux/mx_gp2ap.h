#ifndef __GP2AP020A00F_H__
#define __GP2AP020A00F_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define GP2AP_DEV_NAME "gp2ap020a00f"

/*ioctl cmds*/
#define GP2AP_IOCTL_BASE 'g'
#define GP2AP_IOCTL_SET_ALS_ENABLE	_IOW(GP2AP_IOCTL_BASE, 0, int)
#define GP2AP_IOCTL_GET_ALS_ENABLE	_IOR(GP2AP_IOCTL_BASE, 1, int)
#define GP2AP_IOCTL_SET_PS_ENABLE	_IOW(GP2AP_IOCTL_BASE, 2, int)
#define GP2AP_IOCTL_GET_PS_ENABLE	_IOR(GP2AP_IOCTL_BASE, 3, int)
#define GP2AP_IOCTL_GET_ALS_DATA	_IOR(GP2AP_IOCTL_BASE, 4, int)
#define GP2AP_IOCTL_GET_PS_DATA		_IOR(GP2AP_IOCTL_BASE, 5, int)

/* command regisgers */
#define REG_COMMAND1 0x00
#define SOFTWARE_SHUTDOWN	(0 << 7)
#define SOFTWARE_OPERATION	(1 << 7)
#define AUTO_SHUTDOWN			(0 << 6)
#define CONTINUE_OPERATION	(1 << 6)
#define OPERATING_MODE_ALL	(0 << 4)
#define OPERATING_MODE_ALS	(1 << 4)
#define OPERATING_MODE_PS	(2 << 4)
#define OPERATING_MODE_DEBUG (3 << 4)
#define PS_DETECTION_MASK (1 << 3)
#define PS_INTERRUPT_MASK (1 << 2)
#define ALS_INTERRUPT_MASK (1 << 1)
#define ALS_EXTERNAL_CALCULATION (0)
#define ALS_INTERNAL_CALCULATION (1)

#define REG_COMMAND2 0x01
#define MEASURE_CYCLE_0 (0 << 6)
#define MEASURE_CYCLE_4 (1 << 6)
#define MEASURE_CYCLE_8 (2 << 6)
#define MEASURE_CYCLE_16    (3 << 6)
#define ALS_RESOLUTION_10	(6 << 3)  /*14 bit,25ms*/
#define ALS_RESOLUTION_14	(4 << 3)  /*14 bit,25ms*/
#define ALS_RESOLUTION_16	(3 << 3)  /*16 bit,100ms*/
#define ALS_RESOLUTION_17	(2 << 3)  /*17bit,00ms*/
#define ALS_RANGE_X1	(0)    /*about 500 lux*/
#define ALS_RANGE_X2	(1)
#define ALS_RANGE_X4	(2)
#define ALS_RANGE_X8    (3)
#define ALS_RANGE_X32	(5)
#define ALS_RANGE_X64	(6)
#define ALS_RANGE_X128	(7)   /*about 46000 lux*/

#define REG_COMMAND3 0x02
#define ALC_ON	(1 << 7)
#define ALC_OFF	(0 << 7)
#define INT_TYPE_LEVEL (0 << 6)
#define INT_TYPE_PULSE (1 << 6)
#define PS_RESOLUTION_10	(6 << 3)  /*10 bit,1.56ms * 2*/
#define PS_RESOLUTION_12	(5 << 3)  /*12 bit,6.25ms * 2*/
#define PS_RESOLUTION_14	(4 << 3)  /*14 bit,25ms * 2*/
#define PS_RANGE_X1	(0)   /**/
#define PS_RANGE_X2	(1)
#define PS_RANGE_X4	(2)
#define PS_RANGE_X8	(3)
#define PS_RANGE_X16	(4)
#define PS_RANGE_X32	(5)   /*about 22000lux*/
#define PS_RANGE_X64	(6)   /*about 88000lux*/
#define PS_RANGE_X128	(7)   /*about 100000lux*/

#define REG_COMMAND4 0x03
#define INTVAL_TIME_0	(0 << 6)
#define INTVAL_TIME_4	(1 << 6)
#define INTVAL_TIME_8	(2 << 6)
#define INTVAL_TIME_16	(3 << 6)
#define LED_CURRENT_13		(0 << 4) /*13.8mA*/
#define LED_CURRENT_27		(1 << 4) /*27.5mA*/
#define LED_CURRENT_55		(2 << 4) /*55mA*/
#define LED_CURRENT_110 (3 << 4) /*110mA*/
#define INT_SETTING_ALL	(0 << 2)
#define INT_SETTING_ALS	(1 << 2)
#define INT_SETTING_PS		(2 << 2)
#define INT_SETTING_PS_DETECTION (3 << 2)
#define LED_FREQUENCY_327K (0 << 1)
#define LED_FREQUENCY_81K (1 << 1)
#define SOFTWARE_RESET (1 << 0)

/* als threshold registers */
#define REG_ATS_LLSB	0x04
#define REG_ATS_LMSB	0x05
#define REG_ATS_HLSB	0x06
#define REG_ATS_HMSB	0x07

/* ps threshold registers */
#define REG_PTS_LLSB	0x08
#define REG_PTS_LMSB	0x09
#define REG_PTS_HLSB	0x0a
#define REG_PTS_HMSB	0x0b

/* als d0 data register */
#define REG_D0_LSB	0x0c
#define REG_D0_MSB	0x0d

/* ps d1 data register */
#define REG_D1_LSB	0x0e
#define REG_D1_MSB	0x0f

/* ps d2 data registers */
#define REG_D2_LSB	0x10
#define REG_D2_MSB	0x11

/*sensor mask*/
#define ID_ALS	(1 << 0)
#define ID_PS	(1 << 1)

/*report event type*/
#define ABS_ALS ABS_MISC
#define ABS_PS	ABS_DISTANCE

#define LSB(x) ((u8)((x) & 0xff))
#define MSB(x) ((u8)((((u16)(x))>>8) & 0xff))

#define I2C_RETRY_DELAY 2
#define I2C_RETRIES	10

#define PS_FAR 1
#define PS_NEAR 0
#define is_far(status) ((status) == PS_FAR)

#define PROXIMITY_CALIB_FILE "/data/calibration/proximity_calibration"

#define PS_IRQ_MODE		0
#define PS_SHUTDOWN_MODE	1

#define ALS_DELAYTIME	100	/* ms */

/* gp2ap driver private data struct*/
#ifdef __KERNEL__
struct gp2ap_data {
	struct i2c_client *client;
	unsigned int irq;
	struct workqueue_struct *als_wq;
	struct delayed_work als_dwork;
	struct input_dev *input_dev;
	struct mutex lock;
	struct miscdevice misc_device;
	struct early_suspend early_suspend;
	int enabled_sensors;
	int sensor_id;
	atomic_t opened;
	int debug;

	int als_data[2];
	int current_range;
	int current_intval_time;
	int ps_data;
	int prev_lux;
	int prev_range;
	u16 ps_near_threshold;	/* The count when barrier is at about 3 ~ 4cm */
	u16 ps_far_threshold;	/* The count when barrier is at about 4 ~ 5cm */
	u16 ps_calib_value;	/* The count when no barrier */
	u8 ps_thresholds[4];
	int CURRENT_ALS_RANGE[3];
	int init_threshold_flag;
	int reset_threshold_flag;
	bool irq_wake_enabled;
	bool calib_value_readed;

	struct mutex ioctl_lock;
};
#endif

#endif
