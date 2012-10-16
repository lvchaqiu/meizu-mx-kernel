/*
 * max77665.h - Driver for the Maxim 77665
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
 *
 * This driver is based on max8997.h
 *
 * MAX77665 has PMIC, Charger, Flash LED, Haptic devices.
 * The devices share the same I2C bus and included in
 * this mfd driver.
 */

#ifndef __LINUX_MFD_MAX77665_H
#define __LINUX_MFD_MAX77665_H

enum max77665_charger_fchgtime
{
	MAX77665_FCHGTIME_DISABLE,
	MAX77665_FCHGTIME_4H,
	MAX77665_FCHGTIME_6H,
	MAX77665_FCHGTIME_8H,
	MAX77665_FCHGTIME_10H,
	MAX77665_FCHGTIME_12H,
	MAX77665_FCHGTIME_14H,
	MAX77665_FCHGTIME_16H,
};

enum max77665_charger_chg_rstrt
{
	MAX77665_CHG_RSTRT_100MV,
	MAX77665_CHG_RSTRT_150MV,
	MAX77665_CHG_RSTRT_200MV,
	MAX77665_CHG_RSTRT_DISABLE,
};

enum max77665_charger_to_ith
{
	MAX77665_CHG_TO_ITH_100MA,
	MAX77665_CHG_TO_ITH_150MA,
	MAX77665_CHG_TO_ITH_200MA,
	MAX77665_CHG_TO_ITH_250MA,
};

enum max77665_charger_top_off_timer
{
	MAX77665_CHG_TO_TIME_0MIN,
	MAX77665_CHG_TO_TIME_10MIN,
	MAX77665_CHG_TO_TIME_20MIN,
	MAX77665_CHG_TO_TIME_30MIN,
	MAX77665_CHG_TO_TIME_40MIN,
	MAX77665_CHG_TO_TIME_50MIN,
	MAX77665_CHG_TO_TIME_60MIN,
	MAX77665_CHG_TO_TIME_70MIN,
};

enum max77665_charger_chg_cv_prm
{
	MAX77665_CHG_CV_PRM_3650MV,
	MAX77665_CHG_CV_PRM_3675MV,
	MAX77665_CHG_CV_PRM_3700MV,
	MAX77665_CHG_CV_PRM_3725MV,
	MAX77665_CHG_CV_PRM_3750MV,
	MAX77665_CHG_CV_PRM_3775MV,
	MAX77665_CHG_CV_PRM_3800MV,
	MAX77665_CHG_CV_PRM_3825MV,
	MAX77665_CHG_CV_PRM_3850MV,
	MAX77665_CHG_CV_PRM_3875MV,
	MAX77665_CHG_CV_PRM_3900MV,
	MAX77665_CHG_CV_PRM_3925MV,
	MAX77665_CHG_CV_PRM_3950MV,
	MAX77665_CHG_CV_PRM_3975MV,
	MAX77665_CHG_CV_PRM_4000MV,
	MAX77665_CHG_CV_PRM_4025MV,
	MAX77665_CHG_CV_PRM_4050MV,
	MAX77665_CHG_CV_PRM_4075MV,
	MAX77665_CHG_CV_PRM_4100MV,
	MAX77665_CHG_CV_PRM_4125MV,
	MAX77665_CHG_CV_PRM_4150MV,
	MAX77665_CHG_CV_PRM_4175MV,
	MAX77665_CHG_CV_PRM_4200MV,
	MAX77665_CHG_CV_PRM_4225MV,
	MAX77665_CHG_CV_PRM_4250MV,
	MAX77665_CHG_CV_PRM_4275MV,
	MAX77665_CHG_CV_PRM_4300MV,
	MAX77665_CHG_CV_PRM_4325MV,
	MAX77665_CHG_CV_PRM_4340MV,
	MAX77665_CHG_CV_PRM_4350MV,
	MAX77665_CHG_CV_PRM_4375MV,
	MAX77665_CHG_CV_PRM_4400MV,
};

enum max77665_haptic_motor_type {
	MAX77665_HAPTIC_ERM,
	MAX77665_HAPTIC_LRA,
};

enum max77665_haptic_pulse_mode {
	MAX77665_EXTERNAL_MODE,
	MAX77665_INTERNAL_MODE,
};

enum max77665_haptic_pwm_divisor {
	MAX77665_PWM_DIVISOR_32,
	MAX77665_PWM_DIVISOR_64,
	MAX77665_PWM_DIVISOR_128,
	MAX77665_PWM_DIVISOR_256,
};

/*
 * max77665_haptic_platform_data
 * @pwm_channel_id: channel number of PWM device
 *		    valid for MAX77665_EXTERNAL_MODE
 * @pwm_period: period in nano second for PWM device
 *		valid for MAX77665_EXTERNAL_MODE
 * @type: motor type
 * @mode: pulse mode
 *     MAX77665_EXTERNAL_MODE: external PWM device is used to control motor
 *     MAX77665_INTERNAL_MODE: internal pulse generator is used to control motor
 * @pwm_divisor: divisor for external PWM device
 * @internal_mode_pattern: internal mode pattern for internal mode
 *     [0 - 3]: valid pattern number
 * @pattern_cycle: the number of cycles of the waveform
 *		   for the internal mode pattern
 *     [0 - 15]: available cycles
 * @pattern_signal_period: period of the waveform for the internal mode pattern
 *     [0 - 255]: available period
 */
struct max77665_haptic_platform_data {
	int pwm_channel_id;
	int pwm_period;
	int pwm_duty;

	enum max77665_haptic_motor_type type;
	enum max77665_haptic_pulse_mode mode;
	enum max77665_haptic_pwm_divisor pwm_divisor;

	int internal_mode_pattern;
	int pattern_cycle;
	int pattern_signal_period;
};

enum max77665_led_mode {
	MAX77665_NONE,
	MAX77665_FLASH_MODE,
	MAX77665_TORCH_MODE,
	MAX77665_FLASH_PIN_CONTROL_MODE,
	MAX77665_TORCH_PIN_CONTROL_MODE,
};

/**
 *  struct max77665_led_platform_data
 *  The number of LED devices for MAX77665 is two
 *  @mode: LED mode for each LED device
 *  @brightness: initial brightness for each LED device
 *	range:
 *	[0 - 31]: MAX77665_FLASH_MODE and MAX77665_FLASH_PIN_CONTROL_MODE
 *	[0 - 15]: MAX77665_MOVIE_MODE and MAX77665_MOVIE_PIN_CONTROL_MODE
 */
struct max77665_led_platform_data {
	enum max77665_led_mode mode[2];
	u8 brightness[2];
};

/* MAX77665 regulator IDs */
enum max77665_regulator {
	MAX77665_ESAFEOUT1 = 0,
	MAX77665_ESAFEOUT2,
	MAX77665_CHARGER,
	MAX77665_FLASH_LED,
	MAX77665_TORCH_LED,
	MAX77665_REVERSE,
	MAX77665_REG_MAX,
};

/**
 *  struct max77665_regulator_data
 *  The number of LED devices for MAX77665 is two
 *  @id: enum max77665_regulators
 *  @initdata: regulator_init_data 
 */
struct max77665_regulator_data {
	int id;
	struct regulator_init_data *initdata;
};

struct max77665_platform_data {
	/* IRQ */
	int irq_base;
	bool wakeup;
	char *name;

 	/* ---- Charger control ---- */
#define CHG_CC_STEP (33)
#define CHGIN_ILIM_STEP (20)
	enum max77665_charger_fchgtime fast_charge_timer;
	enum max77665_charger_chg_rstrt charging_restart_thresold;
	enum max77665_charger_to_ith top_off_current_thresold;
	enum max77665_charger_top_off_timer top_off_timer;
	enum max77665_charger_chg_cv_prm charger_termination_voltage;

	const char *supply;
	int fast_charge_current;		/* 0mA ~ 2100mA */
	int chgin_ilim_usb;				/* 60mA ~ 2.58A */
	int chgin_ilim_ac;
	int (*usb_attach) (bool);
	int charger_pin;

	/* ---- REGULATOR --- */
	struct max77665_regulator_data *regulators;
	int num_regulators;

	/* ---- HAPTIC ---- */
	struct max77665_haptic_platform_data *haptic_pdata;

	/* ---- LED ---- */
	struct max77665_led_platform_data *led_pdata;
};

#endif	/* __LINUX_MFD_MAX77665_H */
