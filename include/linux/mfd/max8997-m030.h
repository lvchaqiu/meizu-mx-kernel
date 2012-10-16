/* include/linux/mfd/mx-max8997.h
 *
 * max8997 regulators config for meizu MX
 * 
 * Copyright (c) 2010-2011 MeiZu Co., Ltd.
 *              http://www.meizu.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __H_M030_MAX8997_CONFIG_H__
#define __H_M030_MAX8997_CONFIG_H__

#include <linux/mfd/max8997.h>

#include <mach/gpio-m030.h>

 //default:3.3v, The power up default is ON
static struct regulator_consumer_supply ldo1_supply[] = {
	/* VDD_33ON : ADC,ABB */
	REGULATOR_SUPPLY("vadc_3.3v", NULL),
};
//default:1.1v, The power up default is ON
static struct regulator_consumer_supply ldo3_supply[] = {
	/* VDD_11OFF :  MIPI, HDMI, USB_EHCI, USB_GADGET, SATA*/
	REGULATOR_SUPPLY("vdd_hdmi_1.1v", NULL),
	REGULATOR_SUPPLY("pd_io", "s3c-usbgadget"),
};

//default:1.8v, The power up default is ON
static struct regulator_consumer_supply ldo4_supply[] = {
	REGULATOR_SUPPLY("vmipi_1.8v", "s5p-mipi-dsim.0"),
};

//default:1.2v, The power up default is ON
static struct regulator_consumer_supply ldo5_supply[] = {
	REGULATOR_SUPPLY("cam_sensor_1.2v", NULL),	//8M camera, default:1.8v
};

//default:1.8v, The power up default is ON
static struct regulator_consumer_supply ldo7_supply[] = {
	REGULATOR_SUPPLY("cam_isp_1.8v", NULL),
};

//default:3.3v, The power up default is ON
static struct regulator_consumer_supply ldo8_supply[] = {
	REGULATOR_SUPPLY("pd_core", "s3c-usbgadget"),	
};

//default:1.1v, The power up default is ON
static struct regulator_consumer_supply ldo10_supply[] = {
	REGULATOR_SUPPLY("vpll_1.1v", NULL),
};

//default:2.8v, The power up default is OFF
static struct regulator_consumer_supply ldo11_supply[] = {
	REGULATOR_SUPPLY("cam_sensor_2.7v", NULL),
};

//default:1.2v, The power up default is OFF
static struct regulator_consumer_supply ldo12_supply[] = {
	REGULATOR_SUPPLY("lcd_1.8v", "ls040b3sx01"),
};

//default:2.8v, The power up default is OFF
static struct regulator_consumer_supply ldo13_supply[] = {
	REGULATOR_SUPPLY("codec_vdd_1.8v", NULL),
};

//default:1.8v, The power up default is OFF
static struct regulator_consumer_supply ldo14_supply[] = {
	REGULATOR_SUPPLY("tp_2.8v", NULL),
};

//default:2.8v, The power up default is OFF
static struct regulator_consumer_supply ldo15_supply[] = {
	REGULATOR_SUPPLY("cam_front_2.8v", NULL),
};

//default:3.3v, The power up default is OFF
static struct regulator_consumer_supply ldo16_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_3.3v", NULL),   
};

//default:2.7v, The power up default is OFF
static struct regulator_consumer_supply ldo17_supply[] = {
	REGULATOR_SUPPLY("cam_af_2.7v", NULL),
};

//default:3.3v, The power up default is OFF
static struct regulator_consumer_supply ldo18_supply[] = {
	REGULATOR_SUPPLY("gps_1.8v", NULL),
};

//default:1.2v, The power up default is ON
static struct regulator_consumer_supply ldo21_supply[] = {
	REGULATOR_SUPPLY("vddq_m1m2", NULL),
};

//default:1.25v, The power up default is ON
static struct regulator_consumer_supply buck1_supply[] = {
	REGULATOR_SUPPLY("vdd_arm", NULL),
};

//default:1.1v, The power up default is ON
static struct regulator_consumer_supply buck2_supply[] = {
	REGULATOR_SUPPLY("vdd_int", NULL),
};

//default:1.2v, The power up default is OFF
static struct regulator_consumer_supply buck3_supply[] = {
	REGULATOR_SUPPLY("vdd_g3d", NULL),
};

//default:1.2v, The power up default is OFF
static struct regulator_consumer_supply buck4_supply[] = {
	REGULATOR_SUPPLY("cam_isp_core", NULL),
};

static struct regulator_consumer_supply buck7_supply[] = {
	REGULATOR_SUPPLY("vcc_sub", NULL),
};

static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

static struct regulator_consumer_supply led_flash_supply[] = {
	REGULATOR_SUPPLY("flash_led", NULL),
};

static struct regulator_consumer_supply led_movie_supply[] = {
	REGULATOR_SUPPLY("torch_led", NULL),
};
static struct regulator_init_data ldo1_init_data = {
	.constraints = {
		.name		= "VADC_3.3V",
		.min_uV 		= 3300000,
		.max_uV 		= 3300000,
		.always_on	= 1,
		.boot_on		= 1,		
		.apply_uV		= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo1_supply),
	.consumer_supplies = ldo1_supply,
};

static struct regulator_init_data ldo2_init_data = {
	.constraints = {
		.name		= "VALIVE_1.1V",
		.min_uV 		= 1100000,
		.max_uV 		= 1100000,
		.always_on	= 1,
		.boot_on		= 1,
		.apply_uV	 	= 1,
		.state_mem 	= {
			.enabled	= 1,
		}
	},
};

static struct regulator_init_data ldo3_init_data = {
	.constraints = {
		.name		= "VUSB_1.1V",
		.min_uV 		= 1100000,
		.max_uV 		= 1100000,
		.apply_uV		= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo3_supply),
	.consumer_supplies = ldo3_supply,
};

static struct regulator_init_data ldo4_init_data = {
	.constraints = {
		.name		= "VMIPI_1.8V",
		.min_uV 		= 1800000,
		.max_uV 		= 1800000,
		.apply_uV		= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo4_supply),
	.consumer_supplies = ldo4_supply,
};

static struct regulator_init_data ldo5_init_data = {
	.constraints = {
		.name		= "VCAMERA0_1.2V",
		.min_uV 		= 1200000,
		.max_uV 		= 1200000,
		.apply_uV		= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo5_supply),
	.consumer_supplies = ldo5_supply,
};

static struct regulator_init_data ldo6_init_data = {
	.constraints = {
		.name		= "MEM_IO_1.8V",
		.min_uV 		= 1800000,
		.max_uV 		= 1800000,
		.apply_uV		= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		}
	},
};

static struct regulator_init_data ldo7_init_data = {
	.constraints = {
		.name		= "VCAMERA0_ISP_1.8V",
		.min_uV 		= 1800000,
		.max_uV 		= 1800000,
		.apply_uV		= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo7_supply),
	.consumer_supplies = ldo7_supply,
};

static struct regulator_init_data ldo8_init_data = {
	.constraints = {
		.name		= "VUSB_3.3V",
		.min_uV 		= 3300000,
		.max_uV 		= 3300000,
		.apply_uV		= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo8_supply),
	.consumer_supplies = ldo8_supply,
};

static struct regulator_init_data ldo9_init_data = {
	.constraints = {
		.name		= "SYS_IO_2.8V",
		.min_uV 		= 2800000,
		.max_uV 		= 2800000,
		.apply_uV		= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		}
	},
};

static struct regulator_init_data ldo10_init_data = {
	.constraints = {
		.name		= "VPLL_1.1V",
		.min_uV 		= 1100000,
		.max_uV 		= 1100000,
		.always_on	= 1,
		.apply_uV		= 1,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo10_supply),
	.consumer_supplies = ldo10_supply,
};

static struct regulator_init_data ldo11_init_data = {
	.constraints = {
		.name	= "VCAMERA_SS2.7V",
		.min_uV = 2700000,
		.max_uV = 2700000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo11_supply),
	.consumer_supplies = ldo11_supply,
};

static struct regulator_init_data ldo12_init_data = {
	.constraints = {
		.name	= "VLCD_1.8V",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo12_supply),
	.consumer_supplies = ldo12_supply,
};

static struct regulator_init_data ldo13_init_data = {
	.constraints = {
		.name	= "VAUDIO_1.8V",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.boot_on =1,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo13_supply),
	.consumer_supplies = ldo13_supply,
};

static struct regulator_init_data ldo14_init_data = {
	.constraints = {
		.name	= "VTOUCH_2.8V",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.boot_on =1,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo14_supply),
	.consumer_supplies = ldo14_supply,
};

static struct regulator_init_data ldo15_init_data = {
	.constraints = {
		.name	= "VCAMERA1_2.8V",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo15_supply),
	.consumer_supplies = ldo15_supply,
};

static struct regulator_init_data ldo16_init_data = {
	.constraints = {
		.name	= "VHDMI_3.3V",
		.min_uV = 3300000,
		.max_uV = 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo16_supply),
	.consumer_supplies = ldo16_supply,
};

static struct regulator_init_data ldo17_init_data = {
	.constraints = {
		.name	= "VCAMERA_AF2.7V",
		.min_uV = 2700000,
		.max_uV = 2700000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo17_supply),
	.consumer_supplies = ldo17_supply,
};

static struct regulator_init_data ldo18_init_data = {
	.constraints = {
		.name	= "VGPS_1.8V",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo18_supply),
	.consumer_supplies = ldo18_supply,
};

static struct regulator_init_data ldo21_init_data = {
	.constraints = {
		.name	= "VDDQ_M1M2_1.2V",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.boot_on	= 1,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = ARRAY_SIZE(ldo21_supply),
	.consumer_supplies = ldo21_supply,
};

static struct regulator_init_data buck1_init_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		= 650000,
		.max_uV		= 2225000,
		.always_on	= 1,
		.boot_on	= 1,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(buck1_supply),
	.consumer_supplies	= buck1_supply,
};

static struct regulator_init_data buck2_init_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		= 650000,
		.max_uV		= 2225000,
		.always_on	= 1,
		.boot_on	= 1,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_supply),
	.consumer_supplies	= buck2_supply,
};

static struct regulator_init_data buck3_init_data = {
	.constraints	= {
		.name		= "G3D_1.1V",
		.min_uV		= 950000, 
		.max_uV		= 1250000,
		.apply_uV		= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck3_supply),
	.consumer_supplies	= buck3_supply,
};

static struct regulator_init_data buck4_init_data = {
	.constraints	= {
		.name		= "CAM_ISP_CORE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck4_supply),
	.consumer_supplies	= buck4_supply,
};

static struct regulator_init_data buck5_init_data = {
	.constraints	= {
		.name		= "VMEM_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.mode		= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
	},
};

static struct regulator_init_data buck7_init_data = {
	.constraints	= {
		.name		= "VCC_SUB_2.0V",
		.min_uV		= 2000000,
		.max_uV		= 2000000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(buck7_supply),
	.consumer_supplies	= buck7_supply,
};

static struct regulator_init_data safeout1_init_data = {
	.constraints	= {
		.name		= "safeout1 range",
		.min_uV		= 4950000,
		.max_uV		= 4950000,
		.apply_uV	= 1,
		.boot_on	= 1,		
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout1_supply),
	.consumer_supplies	= safeout1_supply,
};

static struct regulator_init_data safeout2_init_data = {
	.constraints	= {
		.name		= "safeout2 range",
		.min_uV		= 4950000,
		.max_uV		= 4950000,
		.apply_uV	= 1,
		.boot_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout2_supply),
	.consumer_supplies	= safeout2_supply,
};

static struct regulator_init_data led_flash_init_data = {
	.constraints = {
		.name	= "FLASH_CUR",
		.min_uA = 23440,
		.max_uA = 750080,
		.valid_ops_mask	= REGULATOR_CHANGE_CURRENT |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &led_flash_supply[0],
};

static struct regulator_init_data led_movie_init_data = {
	.constraints = {
		.name	= "MOVIE_CUR",
		.min_uA = 15625,
		.max_uA = 250000,
		.valid_ops_mask	= REGULATOR_CHANGE_CURRENT |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &led_movie_supply[0],
};

static struct max8997_regulator_data max8997_regulators[] = {
	{ MAX8997_LDO1,	 &ldo1_init_data, },	
	{ MAX8997_LDO2,	 &ldo2_init_data, },
	{ MAX8997_LDO3,	 &ldo3_init_data, },
	{ MAX8997_LDO4,	 &ldo4_init_data, },
	{ MAX8997_LDO5,	 &ldo5_init_data, },
	{ MAX8997_LDO6,	 &ldo6_init_data, },
	{ MAX8997_LDO7,	 &ldo7_init_data, },
	{ MAX8997_LDO8,	 &ldo8_init_data, },
	{ MAX8997_LDO9,	 &ldo9_init_data, },
	{ MAX8997_LDO10, &ldo10_init_data, },
	{ MAX8997_LDO11, &ldo11_init_data, },
	{ MAX8997_LDO12, &ldo12_init_data, },
	{ MAX8997_LDO13, &ldo13_init_data, },
	{ MAX8997_LDO14, &ldo14_init_data, },
	{ MAX8997_LDO15, &ldo15_init_data, },
	{ MAX8997_LDO16, &ldo16_init_data, },
	{ MAX8997_LDO17, &ldo17_init_data, },
	{ MAX8997_LDO18, &ldo18_init_data, },
	{ MAX8997_LDO21, &ldo21_init_data, },
	{ MAX8997_BUCK1, &buck1_init_data, },	/*arm core*/
	{ MAX8997_BUCK2, &buck2_init_data, },	/*int*/
	{ MAX8997_BUCK3, &buck3_init_data, },	/*G3D*/
	{ MAX8997_BUCK4, &buck4_init_data, },	/*CAMERA ISP*/
	{ MAX8997_BUCK5, &buck5_init_data, },	/*MEM*/
	{ MAX8997_BUCK7, &buck7_init_data, },	/*SYS 2.0V*/
	{ MAX8997_ESAFEOUT1, &safeout1_init_data, },
	{ MAX8997_ESAFEOUT2, &safeout2_init_data, },
	{ MAX8997_FLASH_CUR, &led_flash_init_data, },
	{ MAX8997_MOVIE_CUR, &led_movie_init_data, },
};

static struct max8997_power_data max8997_charger = {
	.vbus_gpio = M030_GPIO_VBUS_GPIO,
	.adc_ch = 5,
};

static struct max8997_platform_data m030_max8997_info = {
	.num_regulators = ARRAY_SIZE(max8997_regulators),
	.regulators     = max8997_regulators,
	.irq_base	= IRQ_BOARD_START,
	.wakeup 	= 1,

	.buck1_voltage[0] = 1350000, /* 1.35V */
	.buck1_voltage[1] = 1300000, /* 1.3V */
	.buck1_voltage[2] = 1250000, /* 1.25V */
	.buck1_voltage[3] = 1200000, /* 1.2V */
	.buck1_voltage[4] = 1150000, /* 1.15V */
	.buck1_voltage[5] = 1100000, /* 1.1V */
	.buck1_voltage[6] = 1000000, /* 1.0V */
	.buck1_voltage[7] = 950000, /* 0.95V */

	.buck2_voltage[0] = 1100000, /* 1.1V */
	.buck2_voltage[1] = 1100000, /* 1.1V */
	.buck2_voltage[2] = 1100000, /* 1.1V */
	.buck2_voltage[3] = 1100000, /* 1.1V */
	.buck2_voltage[4] = 1100000, /* 1.1V */
	.buck2_voltage[5] = 1100000, /* 1.1V */
	.buck2_voltage[6] = 1100000, /* 1.1V */
	.buck2_voltage[7] = 1100000, /* 1.1V */

	.buck5_voltage[0] = 1225000, /* 1.2V */
	.buck5_voltage[1] = 1225000, /* 1.2V */
	.buck5_voltage[2] = 1225000, /* 1.2V */
	.buck5_voltage[3] = 1225000, /* 1.2V */
	.buck5_voltage[4] = 1225000, /* 1.2V */
	.buck5_voltage[5] = 1225000, /* 1.2V */
	.buck5_voltage[6] = 1225000, /* 1.2V */
	.buck5_voltage[7] = 1225000, /* 1.2V */

	.buck125_gpios[0] = M030_GPIO_BUCK125_EN_A,
	.buck125_gpios[1] = M030_GPIO_BUCK125_EN_B,
	.buck125_gpios[2] = M030_GPIO_BUCK125_EN_C,
	
	.buck1_gpiodvs	  = false,
	.buck2_gpiodvs	  = false,
	.buck5_gpiodvs	  = false,
	.power = &max8997_charger,
};
#endif


