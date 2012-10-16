/*
 * max77665-private.h - Voltage regulator driver for the Maxim 77665
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  lvcha qiu   <lvcha@meizu.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_MAX77665_PRIV_H
#define __LINUX_MFD_MAX77665_PRIV_H

#include <linux/i2c.h>

#define MAX77665_NUM_IRQ_MUIC_REGS	3
#define MAX77665_REG_INVALID		(0xff)

/* Slave addr = 0xCC: PMIC, Charger, Flash LED */
enum max77665_pmic_reg {
	MAX77665_LED_REG_IFLASH1			= 0x00,
	MAX77665_LED_REG_IFLASH2			= 0x01,
	MAX77665_LED_REG_ITORCH			= 0x02,
	MAX77665_LED_REG_ITORCH_TIMER		= 0x03,
	MAX77665_LED_REG_FLASH_TIMER		= 0x04,
	MAX77665_LED_REG_FLASH_EN			= 0x05,
	MAX77665_LED_REG_MAX_FLASH1		= 0x06,
	MAX77665_LED_REG_MAX_FLASH2		= 0x07,
	MAX77665_LED_REG_MAX_FLASH3		= 0x08,
	MAX77665_LED_REG_MAX_FLASH4		= 0x09,
	MAX77665_LED_REG_VOUT_CNTL		= 0x0A,
	MAX77665_LED_REG_VOUT_FLASH1		= 0x0B,
	MAX77665_LED_REG_VOUT_FLASH2		= 0x0C,
	MAX77665_LED_REG_FLASH_INT		= 0x0E,
	MAX77665_LED_REG_FLASH_INT_MASK	= 0x0F,
	MAX77665_LED_REG_FLASH_INT_STATUS= 0x10,

	MAX77665_PMIC_REG_PMIC_ID1		= 0x20,
	MAX77665_PMIC_REG_PMIC_ID2		= 0x21,
	MAX77665_PMIC_REG_INTSRC			= 0x22,
	MAX77665_PMIC_REG_INTSRC_MASK	= 0x23,
	MAX77665_PMIC_REG_TOPSYS_INT		= 0x24,
	MAX77665_PMIC_REG_TOPSYS_INT_MASK= 0x26,
	MAX77665_PMIC_REG_TOPSYS_STAT	= 0x28,
	MAX77665_PMIC_REG_MAINCTRL1		= 0x2A,
	MAX77665_PMIC_REG_LSCNFG			= 0x2B,

	MAX77665_CHG_REG_CHG_INT			= 0xB0,
	MAX77665_CHG_REG_CHG_INT_MASK	= 0xB1,
	MAX77665_CHG_REG_CHG_INT_OK		= 0xB2,
	MAX77665_CHG_REG_CHG_DETAILS_00	= 0xB3,
	MAX77665_CHG_REG_CHG_DETAILS_01	= 0xB4,
	MAX77665_CHG_REG_CHG_DETAILS_02	= 0xB5,
	MAX77665_CHG_REG_CHG_DETAILS_03	= 0xB6,
	MAX77665_CHG_REG_CHG_CNFG_00		= 0xB7,
	MAX77665_CHG_REG_CHG_CNFG_01		= 0xB8,
	MAX77665_CHG_REG_CHG_CNFG_02		= 0xB9,
	MAX77665_CHG_REG_CHG_CNFG_03		= 0xBA,
	MAX77665_CHG_REG_CHG_CNFG_04		= 0xBB,
	MAX77665_CHG_REG_CHG_CNFG_05		= 0xBC,
	MAX77665_CHG_REG_CHG_CNFG_06		= 0xBD,
	MAX77665_CHG_REG_CHG_CNFG_07		= 0xBE,
	MAX77665_CHG_REG_CHG_CNFG_08		= 0xBF,
	MAX77665_CHG_REG_CHG_CNFG_09		= 0xC0,
	MAX77665_CHG_REG_CHG_CNFG_10		= 0xC1,
	MAX77665_CHG_REG_CHG_CNFG_11		= 0xC2,
	MAX77665_CHG_REG_CHG_CNFG_12		= 0xC3,
	MAX77665_CHG_REG_CHG_CNFG_13		= 0xC4,
	MAX77665_CHG_REG_CHG_CNFG_14		= 0xC5,
	MAX77665_CHG_REG_SAFEOUT_CTRL	= 0xC6,

	MAX77665_PMIC_REG_END,
};

/* Slave addr = 0x90: Haptic */
enum max77665_haptic_reg {
	MAX77665_HAPTIC_REG_STATUS		= 0x00,
	MAX77665_HAPTIC_REG_CONFIG1		= 0x01,
	MAX77665_HAPTIC_REG_CONFIG2		= 0x02,
	MAX77665_HAPTIC_REG_CONFIG_CHNL	= 0x03,
	MAX77665_HAPTIC_REG_CONFG_CYC1	= 0x04,
	MAX77665_HAPTIC_REG_CONFG_CYC2	= 0x05,
	MAX77665_HAPTIC_REG_CONFIG_PER1	= 0x06,
	MAX77665_HAPTIC_REG_CONFIG_PER2	= 0x07,
	MAX77665_HAPTIC_REG_CONFIG_PER3	= 0x08,
	MAX77665_HAPTIC_REG_CONFIG_PER4	= 0x09,
	MAX77665_HAPTIC_REG_CONFIG_DUTY1	= 0x0A,
	MAX77665_HAPTIC_REG_CONFIG_DUTY2	= 0x0B,
	MAX77665_HAPTIC_REG_CONFIG_PWM1	= 0x0C,
	MAX77665_HAPTIC_REG_CONFIG_PWM2	= 0x0D,
	MAX77665_HAPTIC_REG_CONFIG_PWM3	= 0x0E,
	MAX77665_HAPTIC_REG_CONFIG_PWM4	= 0x0F,
	MAX77665_HAPTIC_REG_REV			= 0x10,

	MAX77665_HAPTIC_REG_END,
};

enum max77665_irq_source {
	CHG_INT = 0,
	TOPSYS_INT,
	LED_INT ,

	MAX77665_IRQ_GROUP_NR,
};

enum max77665_irq {
	/* PMIC - Charger */
	MAX77665_CHG_IRQ_BYP_I,
	MAX77665_CHG_IRQ_THM_I,
	MAX77665_CHG_IRQ_BAT_I,
	MAX77665_CHG_IRQ_CHG_I,
	MAX77665_CHG_IRQ_CHGIN_I,

	/* PMIC - TOPSYS */
	MAX77665_TOPSYS_IRQ_T120C_INT,
	MAX77665_TOPSYS_IRQ_T140C_INT,
	MAX77665_TOPSYS_IRQ_LOWSYS_INT,
	
	/* PMIC - FLASH */
	MAX77665_LED_IRQ_FLED2_OPEN,
	MAX77665_LED_IRQ_FLED2_SHORT,
	MAX77665_LED_IRQ_FLED1_OPEN,
	MAX77665_LED_IRQ_FLED1_SHORT,
	MAX77665_LED_IRQ_MAX_FLASH,

	MAX77665_IRQ_NR,
};

struct max77665_dev {
	struct device *dev;
	struct i2c_client *i2c; /* 0xCC / PMIC, Charger, Flash LED */
	struct i2c_client *haptic; /* 0x90 / Haptic */
	struct mutex iolock;

	int type;

	int irq;
	int irq_base;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[MAX77665_IRQ_GROUP_NR];
	int irq_masks_cache[MAX77665_IRQ_GROUP_NR];
};

enum max77665_types {
	TYPE_MAX77665,
};

/*flash and torch led mask*/
#define MAX77665_FLASH_ENABLE_MASK		(0x05)
#define MAX77665_FLASH_ENABLE_SHIFT		(0x04)
#define MAX77665_TORCH_ENABLE_MASK		(0x0f)
#define MAX77665_TORCH_ENABLE_SHIFT 		(0x00)
#define MAX77665_FLASH1_CURRENT_MASK 	(0x3f)
#define MAX77665_FLASH1_CURRENT_SHIFT 	(0x00)
#define MAX77665_FLASH2_CURRENT_MASK 	(0x3f)
#define MAX77665_FLASH2_CURRENT_SHIFT 	(0x00)
#define MAX77665_TORCH1_CURRENT_MASK	(0x0f)
#define MAX77665_TORCH1_CURRENT_SHIFT 	(0x00)
#define MAX77665_TORCH2_CURRENT_MASK	(0x0f)
#define MAX77665_TORCH2_CURRENT_SHIFT 	(0x04)
#define MAX77665_BOOST_VOUT_FLASH_FROM_VOLT(mV) \
		((mV) <= 3300 ? 0x00 :					\
		((mV) <= 5500 ? (((mV) - 3300) / 25 + 0x0C) : 0x7F))

/*boost mode*/
enum max77665_boost_control_mode {
	MAX77665_BOOST_FLED_OFF_MODE = 0x00,
	MAX77665_BOOST_FLED1_ADAPTIVE_MODE,
	MAX77665_BOOST_FLED2_ADAPTIVE_MODE,
	MAX77665_BOOST_FLED_BOTH_ADAPTIVE_MODE,
	MAX77665_BOOST_FLED_FIXED_MODE,
	MAX77665_BOOST_FLED_BOTH_USED = 0x80,
};

/*flash max time if flash timer used*/		
enum max77665_flash_time
{
	MAX77665_FLASH_TIME_62P5MS,
	MAX77665_FLASH_TIME_125MS,
	MAX77665_FLASH_TIME_187P5MS,
	MAX77665_FLASH_TIME_250MS,
	MAX77665_FLASH_TIME_312P5MS,
	MAX77665_FLASH_TIME_375MS,
	MAX77665_FLASH_TIME_437P5MS,
	MAX77665_FLASH_TIME_500MS,
	MAX77665_FLASH_TIME_562P5MS,
	MAX77665_FLASH_TIME_625MS,
	MAX77665_FLASH_TIME_687P5MS,
	MAX77665_FLASH_TIME_750MS,
	MAX77665_FLASH_TIME_812P5MS,
	MAX77665_FLASH_TIME_875MS,
	MAX77665_FLASH_TIME_937P5MS,
	MAX77665_FLASH_TIME_1000MS,
	MAX77665_FLASH_TIME_MAX,
};

/*flash working mode*/
enum max77665_flash_timer_mode 
{
	MAX77665_FLASH_ONESHOT_MODE = 0x00,
	MAX77665_FLASH_MAX_TIMER_MODE = 0x80,
};

/*torch timer setting*/
enum max77665_torch_timer_enable_mode {
	MAX77665_TORCH_TIMER_ENABLE = 0x00,
	MAX77665_TORCH_TIMER_DISABLE = 0x40,
};

/*torch timer working mode*/
enum max77665_torch_timer_mode {
	MAX77665_TORCH_ONESHOT_MODE = 0x00,
	MAX77665_TORCH_MAX_TIMER_MODE = 0x80,
};

extern int max77665_irq_init(struct max77665_dev *max77665);
extern void max77665_irq_exit(struct max77665_dev *max77665);
extern int max77665_irq_resume(struct max77665_dev *max77665);

extern int max77665_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77665_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77665_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77665_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77665_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /*  __LINUX_MFD_MAX77665_PRIV_H */
