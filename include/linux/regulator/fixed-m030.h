/* include/linux/regulator/mx-fixed.h
 *
 * Fixed regulators for meizu MX
 * 
 * Copyright (c) 2010-2011 MeiZu Co., Ltd.
 *		http://www.meizu.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#ifndef __H_MX_REGULATOR_FIXED_H__
#define __H_MX_REGULATOR_FIXED_H__

#include <mach/gpio-m030.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>

static struct regulator_consumer_supply m030_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("lcd_analog", "ls040b3sx01"),
};

static struct regulator_init_data m030_fixed_voltage0_init_data = {
	.constraints = {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem={
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(m030_fixed_voltage0_supplies),
	.consumer_supplies	= m030_fixed_voltage0_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage0_config = {
	.supply_name	= "LCD_5.5V",
	.microvolts	= 5500000,
	.gpio		= M030_GPIO_LCD_EN,
	.enable_high = 1,
	.init_data	= &m030_fixed_voltage0_init_data,
};

static struct platform_device m030_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data	= &m030_fixed_voltage0_config,
	},
};

/* mipi core 1.1v power */
static struct regulator_consumer_supply m030_fixed_voltage2_supplies[] = {
	REGULATOR_SUPPLY("mipi_core", "s5p-mipi-dsim.0"),
};

static struct regulator_init_data m030_fixed_voltage2_init_data = {
	.constraints = {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem={
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(m030_fixed_voltage2_supplies),
	.consumer_supplies	= m030_fixed_voltage2_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage2_config = {
	.supply_name	= "MIPI_1.1V",
	.microvolts	= 1100000,
	.gpio		= M030_GPIO_MIPI_EN,
	.enable_high = 1,
	.init_data	= &m030_fixed_voltage2_init_data,
};

static struct platform_device m030_fixed_voltage2 = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data	= &m030_fixed_voltage2_config,
	},
};

/* HSIC core 1.2v power */
static struct regulator_consumer_supply m030_fixed_voltage3_supplies[] = {
	REGULATOR_SUPPLY("pd_hsic", "s5p-ehci"),
};

static struct regulator_init_data m030_fixed_voltage3_init_data = {
	.constraints = {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem={
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(m030_fixed_voltage3_supplies),
	.consumer_supplies	= m030_fixed_voltage3_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage3_config = {
	.supply_name	= "HSIC_1.2V",
	.microvolts	= 1200000,
	.gpio		= M030_GPIO_HSIC_EN,
	.enable_high = 1,
	.init_data	= &m030_fixed_voltage3_init_data,
};

static struct platform_device m030_fixed_voltage3 = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev		= {
		.platform_data	= &m030_fixed_voltage3_config,
	},
};

/* usb host phy 3.3v power */
static struct regulator_consumer_supply m030_fixed_voltage4_supplies[] = {
	REGULATOR_SUPPLY("pd_core", "s5p-ehci"),
};

static struct regulator_init_data m030_fixed_voltage4_init_data = {
	.constraints = {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem={
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(m030_fixed_voltage4_supplies),
	.consumer_supplies	= m030_fixed_voltage4_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage4_config = {
	.supply_name	= "USB_3.3V",
	.microvolts	= 3300000,
	.gpio		= M030_GPIO_USB_PHY_EN,
	.enable_high = 1,
	.init_data	= &m030_fixed_voltage4_init_data,
};

static struct platform_device m030_fixed_voltage4 = {
	.name		= "reg-fixed-voltage",
	.id		= 4,
	.dev		= {
		.platform_data	= &m030_fixed_voltage4_config,
	},
};

/* usb host core 1.1v power */
static struct regulator_consumer_supply m030_fixed_voltage5_supplies[] = {
	REGULATOR_SUPPLY("pd_io", "s5p-ehci"),
};

static struct regulator_init_data m030_fixed_voltage5_init_data = {
	.constraints = {
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem={
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(m030_fixed_voltage5_supplies),
	.consumer_supplies	= m030_fixed_voltage5_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage5_config = {
	.supply_name	= "USB_1.1V",
	.microvolts	= 1100000,
	.gpio		= M030_GPIO_USB_CORE_EN,
	.enable_high = 1,
	.init_data	= &m030_fixed_voltage5_init_data,
};

static struct platform_device m030_fixed_voltage5 = {
	.name		= "reg-fixed-voltage",
	.id		= 5,
	.dev		= {
		.platform_data	= &m030_fixed_voltage5_config,
	},
};
/* audio crystal power */
static struct regulator_consumer_supply m030_fixed_voltage6_supplies[] = {
	REGULATOR_SUPPLY("audio_crystal", "0-001a"),
	REGULATOR_SUPPLY("audio_crystal", "0-003e"),
};

static struct regulator_init_data m030_fixed_voltage6_init_data = {
	.constraints = {
		.boot_on = 1,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem={
			.enabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(m030_fixed_voltage6_supplies),
	.consumer_supplies	= m030_fixed_voltage6_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage6_config = {
	.supply_name	= "12MHZ_CRYSTAL",
	.microvolts	= 1800000,
	.gpio		= M030_GPIO_CODEC_CRY,
	.enable_high 	= 1,
	.init_data	= &m030_fixed_voltage6_init_data,
};

static struct platform_device m030_fixed_voltage6 = {
	.name		= "reg-fixed-voltage",
	.id		= 6,
	.dev		= {
		.platform_data	= &m030_fixed_voltage6_config,
	},
};
static struct regulator_consumer_supply m030_fixed_voltage7_supplies =
	REGULATOR_SUPPLY("iNAND", "dw_mmc");

static struct regulator_init_data m030_fixed_voltage7_init_data = {
	.constraints = {
		.boot_on = true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem={
			.enabled	= true,
		}
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &m030_fixed_voltage7_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage7_config = {
	.supply_name	= "iNAND_POWER 2.8V",
	.microvolts	= 2800000,
	.gpio			= M030_GPIO_INAND_EN,
	.enable_high 	= true,
	.init_data	= &m030_fixed_voltage7_init_data,
};

static struct platform_device m030_fixed_voltage7 = {
	.name	= "reg-fixed-voltage",
	.id		= 7,
	.dev		= {
		.platform_data	= &m030_fixed_voltage7_config,
	},
};

/*wm8958 volt0*/
static struct regulator_consumer_supply wm8958_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("AVDD2", "0-001a"),
	REGULATOR_SUPPLY("CPVDD", "0-001a"),
	REGULATOR_SUPPLY("DBVDD1", "0-001a"),
	REGULATOR_SUPPLY("DBVDD2", "0-001a"),
	REGULATOR_SUPPLY("DBVDD3", "0-001a"),
};

static struct regulator_init_data wm8958_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = 1,
		.state_mem={
			.enabled = 1,
		}
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8958_fixed_voltage0_supplies),
	.consumer_supplies	= wm8958_fixed_voltage0_supplies,
};

static struct fixed_voltage_config wm8958_fixed_voltage0_config = {
	.supply_name	= "AUDIO_VDD_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm8958_fixed_voltage0_init_data,
};

static struct platform_device wm8958_fixed_voltage0 = {
	.name	= "reg-fixed-voltage",
	.id		= 8,
	.dev		= {
		.platform_data	= &wm8958_fixed_voltage0_config,
	},
};

/*wm8958 volt1*/
static struct regulator_consumer_supply wm8958_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDD1", "0-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "0-001a"),
};

static struct regulator_init_data wm8958_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = 1,
		.state_mem={
			.enabled = 1,
		}
	},
	.num_consumer_supplies	= ARRAY_SIZE(wm8958_fixed_voltage1_supplies),
	.consumer_supplies	= wm8958_fixed_voltage1_supplies,
};

static struct fixed_voltage_config wm8958_fixed_voltage1_config = {
	.supply_name	= "AUDIO_DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &wm8958_fixed_voltage1_init_data,
};

static struct platform_device wm8958_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 9,
	.dev		= {
		.platform_data	= &wm8958_fixed_voltage1_config,
	},
};
static struct regulator_consumer_supply m030_fixed_voltage8_supplies =
	REGULATOR_SUPPLY("mhl_1.2v", NULL);

static struct regulator_init_data m030_fixed_voltage8_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem={
			.enabled = 1,
		}
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &m030_fixed_voltage8_supplies,
};

static struct fixed_voltage_config m030_fixed_voltage8_config = {
	.supply_name	= "MHL_POWER 1.2V",
	.microvolts	= 1200000,
	.gpio			= M030_GPIO_MHL_EN0,
	.enable_high 	= true,
	.init_data	= &m030_fixed_voltage8_init_data,
};

static struct platform_device m030_fixed_voltage8 = {
	.name	= "reg-fixed-voltage",
	.id		= 10,
	.dev		= {
		.platform_data	= &m030_fixed_voltage8_config,
	},
};
#endif
