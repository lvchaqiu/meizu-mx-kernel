/* linux/arch/arm/mach-exynos/include/mach/gpio-m30x.h
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author: lvcha qiu <lvcha@meizu.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 
 *
 * Revision History
 *
 * Inital code : Apr 16 , 2012 : lvcha@meizu.com
 *
*/

#ifndef _MX30X_GPIO_H
#define _MX30X_GPIO_H

typedef enum {
	/* M30X EINT */
	POWER_KEY = EXYNOS4_GPX0(0),
	TOUCH_IRQ,
	HOME_KEY,
	USB_IRQ,
	PMIC0_IRQ,
	PMIC1_IRQ,
	IR_IRQ,
	CODEC_IRQ,

	ACC_IRQ1 = EXYNOS4_GPX1(0),
	DOCK_IRQ,
	MHL_USB_IRQ,
	COMPASS_IRQ,
	MHL_IRQ,
	GYR_DRDY_IRQ,
	ACC_IRQ0,
	HDETEC_IRQ,
	
	VOLUMEDOWN_KEY = EXYNOS4_GPX2(0),
	BAT_LOW_IRQ,
	WL_HOST_WAKE = EXYNOS4_GPX2(3),
	BT_HOST_WAKE,
	VOLUMEUP_KEY,
	BB_SRDY_IRQ = EXYNOS4_GPX2(7),
	
	BB_RST_IRQ = EXYNOS4_GPX3(0),
	IPC_TRIG_IRQ,
	PMIC1_RST_IRQ,
	GYR_IRQ,
	FUEL_IRQ,
	IPC_WAKEUP_IRQ,
	HDMI_HPD_IRQ = EXYNOS4_GPX3(7),


	/* M30X GPIO INT */
	CAMERA_ISP_IRQ = EXYNOS4_GPF2(6),
	CHARGER_IRQ = EXYNOS4212_GPM3(0),
	CENTER_KEY =EXYNOS4212_GPM3(1),
	BACK_KEY = EXYNOS4212_GPM3(5),
} M30X_IRQ;

typedef enum {
	ADC_BAT,	/* battery voltage measure */
	ADC_THERM,	/* Thermal Sensor measure */
	ADC_ARM,	/* arm core voltage measure */
	ADC_MIC,	/* mic key measure */
} M30X_ADC;

typedef enum {
	LED_ID1 =EXYNOS4_GPY0(4) ,
	LED_ID2 = EXYNOS4_GPY2(1),
	LED_ID3,
	LED_ID4,
	LED_ID5,
	LED_ID6,
} M30X_LED;

/* wifi and bluetooth zone */
typedef enum {
	BT_RXD = EXYNOS4_GPA0(0),
	BT_TXD =EXYNOS4_GPA0(1),
	BT_CTS = EXYNOS4_GPA0(2),
	BT_RTS = EXYNOS4_GPA0(3),
	WL_RESET = EXYNOS4_GPY5(1),
	WL_WAKE = EXYNOS4_GPY5(3),
	BT_RESET = EXYNOS4_GPY5(5),
	WL_POWER = EXYNOS4_GPY6(3),
	WL_WIFICS = EXYNOS4_GPY6(4),
	BT_WAKE = EXYNOS4_GPY6(6),
	BT_POWER = EXYNOS4_GPY6(7),
} M30X_WIFI;

/* arm core, int, g3d voltage control pin */
typedef enum {
	GPIO_DVS1 = EXYNOS4_GPF1(4),
	GPIO_DVS2 = EXYNOS4_GPF0(5),
	GPIO_DVS3 = EXYNOS4_GPF3(2),
	BUCK2_SEL = EXYNOS4_GPF1(0),
	BUCK3_SEL = EXYNOS4_GPF2(4),
	BUCK4_SEL = EXYNOS4_GPF2(1),
} M30X_DVFS;

/* modem control */
typedef enum {
	IPC_TRIGIN = EXYNOS4_GPL0(0),
	MODEM_ON,
	MODEM_RST0,	/*reset pmu*/
	MODEM_RST1,	/*reset bb*/
	IPC_SLAVE_WAKE,
} M30X_MODEM;

/* version control */
typedef enum {
	VER_PIN0 = EXYNOS4_GPL2(4),
	VER_PIN1,
	VER_PIN2,
	VER_PIN3,
} M30X_VERSION;

/* Camera0 and Camera1 zone */
#define CAMERA_ISP_EN		EXYNOS4_GPB(4)		/* camer power12 enable pin ??*/
#define FRONT_CAM_EN		EXYNOS4_GPY3(1) 	/* front camser power 2.8v enable pin */
#define FRONT_CAM_DOWN	EXYNOS4212_GPJ1(4)
#define BACK_CAM_RST		EXYNOS4_GPF1(2)
#define BACK_CAM_DOWN		EXYNOS4_GPF1(7)

/*codec zone*/
#define CODEC_CLK_EN		EXYNOS4_GPF2(3)
#define NOISER_WAKE		EXYNOS4_GPF1(1)
#define NOISER_RST		EXYNOS4_GPF0(6)
#define PCM_SELECT		EXYNOS4_GPF0(7)
#define CODEC_LDO1_EN	EXYNOS4_GPF1(3)
#define CODEC_LDO2_EN	EXYNOS4_GPF3(5)

/* hdmi power 1.0v enable pin */
#define HDMI_P10EN		EXYNOS4212_GPM4(4)
#define MHL_WAKE			EXYNOS4_GPF1(5)
#define MHL_RST			EXYNOS4_GPF3(4)
#define MHL12_EN			EXYNOS4212_GPM4(1)


#define GPIO_HOST_ACTIVE        EXYNOS4_GPL0(0)
#define GPIO_MODEM_ON           EXYNOS4_GPL0(1)
#define GPIO_MODEM_RST_FULL     EXYNOS4_GPL0(2)
#define GPIO_MODEM_RST          EXYNOS4_GPL0(3)
#define GPIO_SLAVE_WAKEUP       EXYNOS4_GPL0(4)

#define M032_INT_USB 		EXYNOS4_GPX0(3)
#define M032_INT_USB_MHL 	EXYNOS4_GPX1(2)
#define GPIO_MODEM_DUMP_INT     EXYNOS4_GPX2(7)
#define GPIO_MODEM_RESET_INT    EXYNOS4_GPX3(0)
#define GPIO_REQUEST_SUSPEND    EXYNOS4_GPX3(1)
#define GPIO_HOST_WAKEUP        EXYNOS4_GPX3(5)



/* touch pannel reset pin */
#define TOUCH_RESET		EXYNOS4_GPC1(1)

/* spdif out enable pin */
#define SPDIF_OUT			EXYNOS4_GPC1(2)

/* iNAND power enable pin */
#define INAND_EN			EXYNOS4_GPK0(2)

/* led zone */
#define LED_SYNC			EXYNOS4_GPF2(5)
#define LED_RST			EXYNOS4_GPF2(0)

/* All sensor power enable pin */
#define SENSOR_EN		EXYNOS4_GPY5(7)

/* turn off pmic1 pin */
#define PMIC1_OFF		EXYNOS4_GPY6(1)

/* DOCK output pin */
#define DOCK_OUTPUT		EXYNOS4_GPY6(5)

/* lcd reset pin */
#define M032_LCD_RST		EXYNOS4_GPF0(0)
#define M032_LCD_TE			EXYNOS4_GPF0(1)
#define LCD_ON		EXYNOS4212_GPM4(3)

/* HSIC zone */
#define GPIO_HSIC_EN		EXYNOS4_GPB(5)

/* Sub pmic zone */
#define TORCH_EN			EXYNOS4212_GPM4(2)

#define USB_SELECT		EXYNOS4212_GPM3(7)

/* gpio i2c define zone */
#define SCL_FUEL0			EXYNOS4_GPY4(0)
#define SDA_FUEL0		EXYNOS4_GPY4(1)
#define SCL_IR			EXYNOS4_GPY4(6)
#define SDA_IR			EXYNOS4_GPY4(7)
#define SCL_MHL			EXYNOS4_GPF3(0)
#define SDA_MHL			EXYNOS4_GPF2(2)
#define SCL_LED			EXYNOS4_GPF3(1)
#define SDA_LED			EXYNOS4_GPF3(3)
#define SCL_GY			EXYNOS4212_GPM4(7)
#define SDA_GY			EXYNOS4212_GPM4(6)
#define SCL_GS			EXYNOS4212_GPM3(3)
#define SDA_GS			EXYNOS4212_GPM3(2)
#define SCL_CP			EXYNOS4212_GPM2(4)
#define SDA_CP			EXYNOS4212_GPM2(3)

/*factory mode define zone*/
#define M032_GPIO_FACTORY_MODE EXYNOS4_GPY2(2)
#define M032_GPIO_TEST_LED EXYNOS4_GPY0(4)

#endif	/* _MX30X_GPIO_H */
