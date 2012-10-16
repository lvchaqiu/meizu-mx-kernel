/* linux/arch/arm/mach-exynos/m032_regulator.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *		http://www.meizu.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#if defined(CONFIG_REGULATOR_FIXED_VOLTAGE)
static struct regulator_consumer_supply wm8958_fixed_voltage0_supplies[] = {
	REGULATOR_SUPPLY("AVDD2", "0-001a"),
	REGULATOR_SUPPLY("CPVDD", "0-001a"),
	REGULATOR_SUPPLY("DBVDD1", "0-001a"),
	REGULATOR_SUPPLY("DBVDD2", "0-001a"),
	REGULATOR_SUPPLY("DBVDD3", "0-001a"),
};

static struct regulator_consumer_supply wm8958_fixed_voltage1_supplies[] = {
	REGULATOR_SUPPLY("SPKVDD1", "0-001a"),
	REGULATOR_SUPPLY("SPKVDD2", "0-001a"),
};

static struct regulator_consumer_supply wm8958_fixed_voltage2_supplies =
	REGULATOR_SUPPLY("DBVDD", "0-001a");

static struct regulator_init_data wm8958_fixed_voltage0_init_data = {
	.constraints = {
		.always_on = true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(wm8958_fixed_voltage0_supplies),
	.consumer_supplies = wm8958_fixed_voltage0_supplies,
};

static struct regulator_init_data wm8958_fixed_voltage1_init_data = {
	.constraints = {
		.always_on = true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = ARRAY_SIZE(wm8958_fixed_voltage1_supplies),
	.consumer_supplies = wm8958_fixed_voltage1_supplies,
};

static struct regulator_init_data wm8958_fixed_voltage2_init_data = {
	.constraints = {
		.always_on = true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &wm8958_fixed_voltage2_supplies,
};

static struct fixed_voltage_config wm8958_fixed_voltage0_config = {
	.supply_name	= "VDD_1.8V",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &wm8958_fixed_voltage0_init_data,
};

static struct fixed_voltage_config wm8958_fixed_voltage1_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &wm8958_fixed_voltage1_init_data,
};

static struct fixed_voltage_config wm8958_fixed_voltage2_config = {
	.supply_name	= "VDD_3.3V",
	.microvolts	= 3300000,
	.gpio		= -EINVAL,
	.init_data	= &wm8958_fixed_voltage2_init_data,
};

static struct platform_device wm8958_fixed_voltage0 = {
	.name		= "reg-fixed-voltage",
	.id		= 0,
	.dev		= {
		.platform_data = &wm8958_fixed_voltage0_config,
	},
};

static struct platform_device wm8958_fixed_voltage1 = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev		= {
		.platform_data = &wm8958_fixed_voltage1_config,
	},
};

static struct platform_device wm8958_fixed_voltage2 = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev		= {
		.platform_data = &wm8958_fixed_voltage2_config,
	},
};

static struct regulator_consumer_supply m032_fixed_voltage0_supplies =
	REGULATOR_SUPPLY("LCD_5.5V", "ls040b3sx01");

static struct regulator_init_data m032_fixed_voltage0_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &m032_fixed_voltage0_supplies,
};

static struct fixed_voltage_config m032_fixed_voltage0_config = {
	.supply_name	= "VDD_5.5V",
	.microvolts	= 5500000,
	.gpio		= LCD_ON,
	.enable_high 	= true,
	.init_data	= &m032_fixed_voltage0_init_data,
};

static struct platform_device m032_fixed_voltage0 = {
	.name	= "reg-fixed-voltage",
	.id		= 3,
	.dev		= {
		.platform_data = &m032_fixed_voltage0_config,
	},
};

static struct regulator_consumer_supply m032_fixed_voltage1_supplies =
	REGULATOR_SUPPLY("spdif_en", NULL);

static struct regulator_init_data m032_fixed_voltage1_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &m032_fixed_voltage1_supplies,
};

static struct fixed_voltage_config m032_fixed_voltage1_config = {
	.supply_name	= "SPDIF 1.8V",
	.microvolts	= 1800000,
	.gpio		= SPDIF_OUT,
	.init_data	= &m032_fixed_voltage1_init_data,
};

static struct platform_device m032_fixed_voltage1 = {
	.name	= "reg-fixed-voltage",
	.id		= 4,
	.dev		= {
		.platform_data = &m032_fixed_voltage1_config,
	},
};

static struct regulator_consumer_supply m032_fixed_voltage2_supplies =
	REGULATOR_SUPPLY("sensor_power", NULL);

static struct regulator_init_data m032_fixed_voltage2_init_data = {
	.constraints = {
		.boot_on = true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &m032_fixed_voltage2_supplies,
};

static struct fixed_voltage_config m032_fixed_voltage2_config = {
	.supply_name	= "SENSOR_POWER 2.8V",
	.microvolts	= 2800000,
	.gpio		= SENSOR_EN,
	.enable_high 	= true,
	.init_data	= &m032_fixed_voltage2_init_data,
};

static struct platform_device m032_fixed_voltage2 = {
	.name	= "reg-fixed-voltage",
	.id		= 5,
	.dev		= {
		.platform_data = &m032_fixed_voltage2_config,
	},
};

static struct regulator_consumer_supply m032_fixed_voltage3_supplies =
	REGULATOR_SUPPLY("iNAND", "dw_mmc");

static struct regulator_init_data m032_fixed_voltage3_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &m032_fixed_voltage3_supplies,
};

static struct fixed_voltage_config m032_fixed_voltage3_config = {
	.supply_name	= "iNAND_POWER 2.8V",
	.microvolts	= 2800000,
	.gpio		= INAND_EN,
	.enable_high 	= true,
	.init_data	= &m032_fixed_voltage3_init_data,
};

static struct platform_device m032_fixed_voltage3 = {
	.name	= "reg-fixed-voltage",
	.id		= 6,
	.dev		= {
		.platform_data = &m032_fixed_voltage3_config,
	},
};

/* HSIC core 1.0v power */
static struct regulator_consumer_supply m032_fixed_voltage4_supplies[] ={
	REGULATOR_SUPPLY("pd_hsic", "s3c-usbgadget"),
	REGULATOR_SUPPLY("pd_hsic", "s5p-ehci"),
};

static struct regulator_init_data m032_fixed_voltage4_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 2,
	.consumer_supplies = m032_fixed_voltage4_supplies,
};

static struct fixed_voltage_config m032_fixed_voltage4_config = {
	.supply_name	= "HSIC_1.0V",
	.microvolts	= 1000000,
	.gpio		= GPIO_HSIC_EN,
	.enable_high 	= true,
	.init_data	= &m032_fixed_voltage4_init_data,
};

static struct platform_device m032_fixed_voltage4 = {
	.name	= "reg-fixed-voltage",
	.id		= 7,
	.dev		= {
		.platform_data = &m032_fixed_voltage4_config,
	},
};

static struct regulator_consumer_supply m032_hdmi_fixed_voltage_supplies =
	REGULATOR_SUPPLY("HDMI_1.0V", NULL);

static struct regulator_init_data m032_hdmi_fixed_voltage_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies= 1,
	.consumer_supplies	= &m032_hdmi_fixed_voltage_supplies,
};

static struct fixed_voltage_config m032_hdmi_fixed_voltage_config = {
	.supply_name	= "VDD_1.0V",
	.microvolts	= 1000000,
	.gpio		= HDMI_P10EN,
	.enable_high	= true,
	.init_data	= &m032_hdmi_fixed_voltage_init_data,
};

static struct platform_device m032_hdmi_fixed_voltage = {
	.name	= "reg-fixed-voltage",
	.id		= 8,
	.dev	= {
		.platform_data = &m032_hdmi_fixed_voltage_config,
	},
};

static struct regulator_consumer_supply m032_mhl_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("MHL_1.2V", NULL),
};

static struct regulator_init_data m032_mhl_fixed_voltage_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies= 1,
	.consumer_supplies	= m032_mhl_fixed_voltage_supplies,
};

static struct fixed_voltage_config m032_mhl_fixed_voltage_config = {
	.supply_name	= "VDD_1.2V",
	.microvolts	= 1200000,
	.gpio		= MHL12_EN,
	.enable_high	= true,
	.init_data	= &m032_mhl_fixed_voltage_init_data,
};

static struct platform_device m032_mhl_fixed_voltage = {
	.name	= "reg-fixed-voltage",
	.id		= 9,
	.dev		= {
		.platform_data = &m032_mhl_fixed_voltage_config,
	},
};

static struct regulator_consumer_supply m032_isp_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("cam0_isp_1.2v", NULL),
};

static struct regulator_init_data m032_isp_fixed_voltage_init_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies= 1,
	.consumer_supplies	= m032_isp_fixed_voltage_supplies,
};

static struct fixed_voltage_config m032_isp_fixed_voltage_config = {
	.supply_name	= "VDD_1.2V",
	.microvolts	= 1200000,
	.gpio		= CAMERA_ISP_EN,
	.enable_high	= true,
	.init_data	= &m032_isp_fixed_voltage_init_data,
};

static struct platform_device m032_isp_fixed_voltage = {
	.name	= "reg-fixed-voltage",
	.id		= 10,
	.dev		= {
		.platform_data = &m032_isp_fixed_voltage_config,
	},
};
#endif

#if defined(CONFIG_REGULATOR_MAX77686)
/* max77686 */
static struct regulator_consumer_supply __initdata max77686_buck1 =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply __initdata max77686_buck2[] = {
	REGULATOR_SUPPLY("vdd_arm", "exynos4212-cpufreq"),
	REGULATOR_SUPPLY("vdd_arm", "exynos4412-cpufreq"),
};

static struct regulator_consumer_supply __initdata max77686_buck3 =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply __initdata max77686_buck4 =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply __initdata max77686_buck5 =
	REGULATOR_SUPPLY("vdd_mem", NULL);

static struct regulator_consumer_supply __initdata max77686_buck6 =
	REGULATOR_SUPPLY("vdd_1.35v", NULL);

static struct regulator_consumer_supply __initdata max77686_buck7 =
	REGULATOR_SUPPLY("vdd_2.00v", NULL);

static struct regulator_consumer_supply __initdata max77686_buck8 =
	REGULATOR_SUPPLY("vdd_2.80v", NULL);

/*isp core, default 1.2v*/
static struct regulator_consumer_supply __initdata max77686_buck9 =
	REGULATOR_SUPPLY("cam0_isp", NULL);

/* alive 1.0v */
static struct regulator_consumer_supply __initdata max77686_ldo1 =
	REGULATOR_SUPPLY("vdd_ldo1", NULL);

/* memoff 1.2v */
static struct regulator_consumer_supply __initdata max77686_ldo2 =
	REGULATOR_SUPPLY("vdd_ldo2", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo3 =
	REGULATOR_SUPPLY("vdd_ldo3", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo4 =
	REGULATOR_SUPPLY("vmmc", "dw_mmc");

/*0.3M and 8M together, default 1.8v*/
static struct regulator_consumer_supply __initdata max77686_ldo5 =
	REGULATOR_SUPPLY("cam_1.8v", NULL);

/* mpll 1.0v */
static struct regulator_consumer_supply __initdata max77686_ldo6 =
	REGULATOR_SUPPLY("vdd_ldo6", NULL);

/* vpll 1.0v */
static struct regulator_consumer_supply __initdata max77686_ldo7 =
	REGULATOR_SUPPLY("vdd_ldo7", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo8 = /*mipi 1.0v */
	REGULATOR_SUPPLY("vdd_ldo8", "s5p-mipi-dsim.0");

/*gps power, 1.8v*/
static struct regulator_consumer_supply __initdata max77686_ldo9 =
	REGULATOR_SUPPLY("gps_1.8v", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo10 = /*mipi 1.8v */
	REGULATOR_SUPPLY("vdd_ldo10", "s5p-mipi-dsim.0");

static struct regulator_consumer_supply __initdata max77686_ldo11 =
	REGULATOR_SUPPLY("vdd_ldo11", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo12[] = {
	REGULATOR_SUPPLY("vdd_ldo12", "s3c-usbgadget"),
	REGULATOR_SUPPLY("vdd_ldo12", "s5p-ehci"),
};

static struct regulator_consumer_supply __initdata max77686_ldo13 = /*lcd 1.8v */
	REGULATOR_SUPPLY("vdd_ldo13", "ls040b3sx01");

static struct regulator_consumer_supply __initdata max77686_ldo14 =
	REGULATOR_SUPPLY("vdd_ldo14", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo15[] = {
	REGULATOR_SUPPLY("vdd_ldo15", "s3c-usbgadget"),
	REGULATOR_SUPPLY("vdd_ldo15", "s5p-ehci"),
};

static struct regulator_consumer_supply __initdata max77686_ldo16[] ={
	REGULATOR_SUPPLY("vdd_ldo16", "s3c-usbgadget"),
	REGULATOR_SUPPLY("vdd_ldo16", "s5p-ehci"),
};

/*8M camera sensor, default 1.2v*/
static struct regulator_consumer_supply __initdata max77686_ldo17 = 
	REGULATOR_SUPPLY("cam0_sensor_1.2v", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo18 =
	REGULATOR_SUPPLY("vdd_ldo18", NULL);

/* hdmi 1.8v */
static struct regulator_consumer_supply __initdata max77686_ldo19 =
	REGULATOR_SUPPLY("vdd_ldo19", NULL);

/* mhl 1.2v */
static struct regulator_consumer_supply __initdata max77686_ldo20 =
	REGULATOR_SUPPLY("vdd_ldo20", NULL);

/*front camera, default 2.8v*/
static struct regulator_consumer_supply __initdata max77686_ldo21 =
	REGULATOR_SUPPLY("cam1_2.8v", NULL);

/* iNAND IO 2.8v */
static struct regulator_consumer_supply __initdata max77686_ldo22 =
	REGULATOR_SUPPLY("inand_io2.8v", NULL);

/*8M camera sensor, default 2.7v*/
static struct regulator_consumer_supply __initdata max77686_ldo23 =
	REGULATOR_SUPPLY("cam0_sensor_2.7v", NULL);

/*8M camera af, default 2.7v*/
static struct regulator_consumer_supply __initdata max77686_ldo24 =
	REGULATOR_SUPPLY("cam0_af_2.7v", NULL);

static struct regulator_consumer_supply __initdata max77686_ldo25 =
	REGULATOR_SUPPLY("vdd_ldo25", NULL);

/* MHL 3.0v */
static struct regulator_consumer_supply __initdata max77686_ldo26 =
	REGULATOR_SUPPLY("vdd_ldo26", NULL);

static struct regulator_consumer_supply max77686_enp32khz =
	REGULATOR_SUPPLY("lpo", NULL);

static struct regulator_init_data __initdata max77686_buck1_data = {
	.constraints = {
		.name 	= "vdd_mif range",
		.min_uV 	= 750000,
		.max_uV 	= 1050000,
		.always_on = true,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck1,
};

static struct regulator_init_data __initdata max77686_buck2_data = {
	.constraints = {
		.name 	= "vdd_arm range",
		.min_uV 	= 600000,
		.max_uV 	= 1400000,
		.always_on = true,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 2,
	.consumer_supplies = max77686_buck2,
};

static struct regulator_init_data __initdata max77686_buck3_data = {
	.constraints = {
		.name 	= "vdd_int range",
		.min_uV 	= 600000,
		.max_uV 	= 1200000,
		.always_on = true,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck3,
};

static struct regulator_init_data __initdata max77686_buck4_data = {
	.constraints = {
		.name 	= "vdd_g3d range",
		.min_uV 	= 600000,
		.max_uV 	= 1200000,
		.boot_on 	= true,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
		.state_mem = {
			.disabled = true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck4,
};

static struct regulator_init_data __initdata max77686_buck5_data = {
	.constraints = {
		.name 	= "vdd_mem_on",
		.min_uV 	= 1200000,
		.max_uV 	= 1200000,
		.boot_on 	= true,
		.always_on = true,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck5,
};

static struct regulator_init_data __initdata max77686_buck6_data = {
	.constraints = {
		.name 	= "vdd_1.35v",
		.min_uV 	= 1350000,
		.max_uV 	= 1350000,
		.boot_on 	= true,
		.always_on = true,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck6,
};

static struct regulator_init_data __initdata max77686_buck7_data = {
	.constraints = {
		.name 	= "vdd_2.00v",
		.min_uV 	= 2000000,
		.max_uV 	= 2000000,
		.boot_on 	= true,
		.always_on = true,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck7,
};

static struct regulator_init_data __initdata max77686_buck8_data = {
	.constraints = {
		.name 	= "vdd_2.80v",
		.min_uV 	= 2800000,
		.max_uV 	= 2800000,
		.boot_on 	= true,
		.always_on = true,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_buck8,
};

static struct regulator_init_data __initdata max77686_buck9_data = {
	.constraints	= {
		.name		= "CAM0_ISP_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.boot_on 		= true,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_buck9,
};

static struct regulator_init_data __initdata max77686_ldo1_data = {
	.constraints = {
		.name = "vdd_ldo1 range",
		.min_uV = 1000000,
		.max_uV = 1000000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo1,
};

static struct regulator_init_data __initdata max77686_ldo2_data = {
	.constraints = {
		.name = "vdd_ldo2 range",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.apply_uV	= true,
		.always_on = true,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo2,
};

static struct regulator_init_data __initdata max77686_ldo3_data = {
	.constraints = {
		.name = "vdd_ldo3 range",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo3,
};

static struct regulator_init_data __initdata max77686_ldo4_data = {
	.constraints = {
		.name = "vdd_ldo4 range",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo4,
};

static struct regulator_init_data __initdata max77686_ldo5_data = {
	.constraints = {
		.name = "CAM_1.8V",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled = 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo5,
};

static struct regulator_init_data __initdata max77686_ldo6_data = {
	.constraints = {
		.name = "vdd_ldo6 range",
		.min_uV = 1000000,
		.max_uV = 1000000,
		.apply_uV	= true,
		.always_on = true,
		.state_mem = {
			.disabled = 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo6,
};

static struct regulator_init_data __initdata max77686_ldo7_data = {
	.constraints = {
		.name = "vdd_ldo7 range",
		.min_uV = 1000000,
		.max_uV = 1000000,
		.apply_uV	= true,
		.always_on = true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo7,
};

static struct regulator_init_data __initdata max77686_ldo8_data = {
	.constraints	= {
		.name	= "vdd_ldo8 range",
		.min_uV	= 1000000,
		.max_uV	= 1000000,
		.apply_uV	= true,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo8,
};

static struct regulator_init_data __initdata max77686_ldo9_data = {
	.constraints = {
		.name	= "GPS_1.8V",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo9,
};

static struct regulator_init_data __initdata max77686_ldo10_data = {
	.constraints	= {
		.name	= "vdd_ldo10 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo10,
};

static struct regulator_init_data __initdata max77686_ldo11_data = {
	.constraints	= {
		.name	= "vdd_ldo11 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo11,
};

static struct regulator_init_data __initdata max77686_ldo12_data = {
	.constraints	= {
		.name	= "vdd_ldo12 range",
		.min_uV	= 3000000,
		.max_uV	= 3000000,
		.apply_uV	= true,
		.always_on= true,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= max77686_ldo12,
};

static struct regulator_init_data __initdata max77686_ldo13_data = {
	.constraints	= {
		.name	= "vdd_ldo13 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo13,
};

static struct regulator_init_data __initdata max77686_ldo14_data = {
	.constraints	= {
		.name	= "vdd_ldo14 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo14,
};

static struct regulator_init_data __initdata max77686_ldo15_data = {
	.constraints	= {
		.name	= "vdd_ldo15 range",
		.min_uV	= 1000000,
		.max_uV	= 1000000,
		.apply_uV	= true,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= max77686_ldo15,
};

static struct regulator_init_data __initdata max77686_ldo16_data = {
	.constraints	= {
		.name	= "vdd_ldo16 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.valid_ops_mask =  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= max77686_ldo16,
};

static struct regulator_init_data __initdata max77686_ldo17_data = {
	.constraints = {
		.name		= "CAM0_SENSOR_1.2V",
		.min_uV 		= 1200000,
		.max_uV 		= 1200000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo17,
};

static struct regulator_init_data __initdata max77686_ldo18_data = {
	.constraints	= {
		.name	= "vdd_ldo18 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo18,
};

static struct regulator_init_data __initdata max77686_ldo19_data = {
	.constraints	= {
		.name	= "vdd_ldo19 range",
		.min_uV	= 1800000,
		.max_uV	= 1800000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo19,
};

static struct regulator_init_data __initdata max77686_ldo20_data = {
	.constraints	= {
		.name	= "vdd_ldo20 range",
		.min_uV	= 1200000,
		.max_uV	= 1200000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo20,
};

static struct regulator_init_data __initdata max77686_ldo21_data = {
	.constraints = {
		.name	= "CAM1_2.8V",
		.min_uV = 2800000,
		.max_uV = 2800000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo21,
};

static struct regulator_init_data __initdata max77686_ldo22_data = {
	.constraints	= {
		.name	= "vdd_ldo22 range",
		.min_uV	= 2800000,
		.max_uV	= 2800000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo22,
};

static struct regulator_init_data __initdata max77686_ldo23_data = {
	.constraints = {
		.name	= "CAM0_SENSOR_2.7V",
		.min_uV = 2700000,
		.max_uV = 2700000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo23,
};

static struct regulator_init_data __initdata max77686_ldo24_data = {
	.constraints = {
		.name	= "CAM0_AF_2.7V",
		.min_uV = 2700000,
		.max_uV = 2700000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		}
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_ldo24,
};

static struct regulator_init_data __initdata max77686_ldo25_data = {
	.constraints	= {
		.name	= "vdd_ldo25 range",
		.min_uV	= 2800000,
		.max_uV	= 2800000,
		.apply_uV	= true,
		.always_on= true,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo25,
};

static struct regulator_init_data __initdata max77686_ldo26_data = {
	.constraints	= {
		.name	= "vdd_ldo26 range",
		.min_uV	= 3000000,
		.max_uV	= 3000000,
		.apply_uV	= true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_ldo26,
};

static struct regulator_init_data max77686_enp32khz_data = {
	.constraints = {
		.name = "32KHZ_PMIC",
		.always_on = true,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &max77686_enp32khz,
};

static struct max77686_regulator_data __initdata max77686_regulators[] = {
	/* System important power supply */
	{MAX77686_BUCK1, 	&max77686_buck1_data},
	{MAX77686_BUCK2, 	&max77686_buck2_data},
	{MAX77686_BUCK3, 	&max77686_buck3_data},
	{MAX77686_BUCK4, 	&max77686_buck4_data},
	{MAX77686_BUCK5, 	&max77686_buck5_data},
	{MAX77686_BUCK6, 	&max77686_buck6_data},
	{MAX77686_BUCK7, 	&max77686_buck7_data},
	{MAX77686_BUCK8, 	&max77686_buck8_data},
	{MAX77686_LDO1,		&max77686_ldo1_data},
	{MAX77686_LDO2,		&max77686_ldo2_data},
	{MAX77686_LDO3,		&max77686_ldo3_data},
	{MAX77686_LDO4,		&max77686_ldo4_data},
	{MAX77686_LDO6,		&max77686_ldo6_data},
	{MAX77686_LDO7,		&max77686_ldo7_data},
	{MAX77686_LDO11,	&max77686_ldo11_data},
	{MAX77686_LDO14,	&max77686_ldo14_data},
	{MAX77686_LDO22,	&max77686_ldo22_data},

	/* Other perpheral power supply  */
	{MAX77686_BUCK9, &max77686_buck9_data},
	{MAX77686_LDO5,	 &max77686_ldo5_data},
	{MAX77686_LDO8,	 &max77686_ldo8_data},
	{MAX77686_LDO9,	 &max77686_ldo9_data},
	{MAX77686_LDO10, &max77686_ldo10_data},
	{MAX77686_LDO12, &max77686_ldo12_data},
	{MAX77686_LDO13, &max77686_ldo13_data},
	{MAX77686_LDO15, &max77686_ldo15_data},
	{MAX77686_LDO16, &max77686_ldo16_data},
	{MAX77686_LDO17, &max77686_ldo17_data},
	{MAX77686_LDO18, &max77686_ldo18_data},
	{MAX77686_LDO19, &max77686_ldo19_data},
	{MAX77686_LDO20, &max77686_ldo20_data},
	{MAX77686_LDO21, &max77686_ldo21_data},
	{MAX77686_LDO23, &max77686_ldo23_data},
	{MAX77686_LDO24, &max77686_ldo24_data},
	{MAX77686_LDO25, &max77686_ldo25_data},
	{MAX77686_LDO26, &max77686_ldo26_data},
	{MAX77686_P32KH, &max77686_enp32khz_data,}
};

struct max77686_opmode_data max77686_opmode_private_data[MAX77686_REG_MAX] = {
	[MAX77686_LDO2] 		= {MAX77686_LDO2, MAX77686_OPMODE_STANDBY},	/*MOFF*/
	[MAX77686_LDO3] 		= {MAX77686_LDO3, MAX77686_OPMODE_NORMAL},	/*DVDD1.8V*/
	[MAX77686_LDO4] 		= {MAX77686_LDO4, MAX77686_OPMODE_LP},			/*MMC IO*/
	[MAX77686_LDO6] 		= {MAX77686_LDO6, MAX77686_OPMODE_STANDBY},	/*MPLL*/
	[MAX77686_LDO7] 		= {MAX77686_LDO7, MAX77686_OPMODE_STANDBY},	/*APLL*/
	[MAX77686_LDO8] 		= {MAX77686_LDO8, MAX77686_OPMODE_STANDBY},	/*MIPI 1.0V*/
	[MAX77686_LDO10]	= {MAX77686_LDO10, MAX77686_OPMODE_STANDBY},	/*MIPI 1.8V*/
	[MAX77686_LDO11]	= {MAX77686_LDO11, MAX77686_OPMODE_LP},		/*ABB1*/
	[MAX77686_LDO12]	= {MAX77686_LDO12, MAX77686_OPMODE_LP},		/*OTG 3.0V*/
	[MAX77686_LDO14]	= {MAX77686_LDO14, MAX77686_OPMODE_LP}, 		/*ABB2*/
	[MAX77686_LDO15] 	= {MAX77686_LDO15, MAX77686_OPMODE_STANDBY},	/*OTG*/
	[MAX77686_LDO16] 	= {MAX77686_LDO16, MAX77686_OPMODE_STANDBY},	/*HSIC 1.8V*/
	[MAX77686_LDO18] 	= {MAX77686_LDO18, MAX77686_OPMODE_NORMAL},	/*AUDIO*/
	[MAX77686_LDO20] 	= {MAX77686_LDO20, MAX77686_OPMODE_LP},		/*MHL*/
	[MAX77686_LDO25] 	= {MAX77686_LDO25, MAX77686_OPMODE_LP},		/*TOUCH*/
	[MAX77686_LDO26] 	= {MAX77686_LDO26, MAX77686_OPMODE_LP},		/*MHL 3.0V*/

	[MAX77686_BUCK1]	= {MAX77686_BUCK1, MAX77686_OPMODE_STANDBY},	/*MIF*/
	[MAX77686_BUCK2]	= {MAX77686_BUCK2, MAX77686_OPMODE_STANDBY},	/*ARM*/
	[MAX77686_BUCK3]	= {MAX77686_BUCK3, MAX77686_OPMODE_STANDBY},	/*INT*/
	[MAX77686_BUCK4]	= {MAX77686_BUCK4, MAX77686_OPMODE_STANDBY},	/*G3D*/
	[MAX77686_BUCK8]	= {MAX77686_BUCK8, MAX77686_OPMODE_STANDBY},	/*INAND*/
	[MAX77686_BUCK9]	= {MAX77686_BUCK8, MAX77686_OPMODE_STANDBY},	/*ISP 1.2V*/
};

static struct max77686_platform_data __initdata m032_max77686_info = {
	.num_regulators = ARRAY_SIZE(max77686_regulators),
	.regulators = max77686_regulators,
	.irq_gpio	= PMIC0_IRQ,
	.irq_base	= IRQ_BOARD_START,
	.wakeup	= true,

	.opmode_data = max77686_opmode_private_data,
	.ramp_rate = MAX77686_RAMP_RATE_100MV,
	.buck2_gpiodvs = true,
	.buck3_gpiodvs = true,
	.buck4_gpiodvs = true,
	
	.buck234_gpio_dvs = {
		GPIO_DVS1,
		GPIO_DVS2,
		GPIO_DVS3,
	},
	.buck234_gpio_selb = {
		BUCK2_SEL,
		BUCK3_SEL,
		BUCK4_SEL,
	},
	/* default exynos_result_of_asv = 7 */
	/* Arm core voltage */
	.buck2_voltage[0] = 1100000,
	.buck2_voltage[1] = 1050000,
	.buck2_voltage[2] = 1012500,
	.buck2_voltage[3] = 962500,
	.buck2_voltage[4] = 950000,
	.buck2_voltage[5] = 937500,
	.buck2_voltage[6] = 925000,
	.buck2_voltage[7] = 900000,

	/* Int voltage */
	.buck3_voltage[0] = 1000000,
	.buck3_voltage[1] = 975000,
	.buck3_voltage[2] = 950000,
	.buck3_voltage[3] = 925000,
	.buck3_voltage[4] = 900000,
	.buck3_voltage[5] = 875000,
	.buck3_voltage[6] = 862500,
	.buck3_voltage[7] = 850000,

	/* mali400 voltage */
	.buck4_voltage[0] = 1050000,
	.buck4_voltage[1] = 1025000,
	.buck4_voltage[2] = 1000000,
	.buck4_voltage[3] = 975000,
	.buck4_voltage[4] = 950000,
	.buck4_voltage[5] = 925000,
	.buck4_voltage[6] = 900000,
	.buck4_voltage[7] = 875000,
};
#endif

#ifdef CONFIG_MFD_MAX77665
static struct regulator_consumer_supply safeout1_supply[] = {
	REGULATOR_SUPPLY("safeout1", NULL),
};

static struct regulator_consumer_supply safeout2_supply[] = {
	REGULATOR_SUPPLY("safeout2", NULL),
};

static struct regulator_consumer_supply charger_supply[] = {
	REGULATOR_SUPPLY("vinchg1", "max77665-charger"),
};

static struct regulator_consumer_supply flash_led_supply[] = {
	REGULATOR_SUPPLY("flash_led", NULL),
};

static struct regulator_consumer_supply torch_led_supply[] = {
	REGULATOR_SUPPLY("torch_led", NULL),
};

static struct regulator_consumer_supply reverse_supply[] = {
	REGULATOR_SUPPLY("reverse", "m032_usb_det"),
};

static struct regulator_init_data safeout1_init_data = {
	.constraints	= {
		.name		= "safeout1 range",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.boot_on	= 1,
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
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.boot_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(safeout2_supply),
	.consumer_supplies	= safeout2_supply,
};

static struct regulator_init_data charger_init_data = {
	.constraints	= {
		.name		= "CHARGER",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					      REGULATOR_CHANGE_CURRENT,
		.boot_on		= 1,
		.min_uA		= 60000,
		.max_uA		= 2580000,
		.state_mem	= {
			.enabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(charger_supply),
	.consumer_supplies	= charger_supply,
};

static struct regulator_init_data flash_led_init_data = {
	.constraints	= {
		.name		= "FLASH LED",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					      REGULATOR_CHANGE_CURRENT,
		.boot_on		= 0,
		.min_uA		= 15625,
		.max_uA		= 1000000,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(flash_led_supply),
	.consumer_supplies	= flash_led_supply,
};

static struct regulator_init_data torch_led_init_data = {
	.constraints	= {
		.name		= "TORCH LED",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
					      REGULATOR_CHANGE_CURRENT,
		.boot_on		= 0,
		.min_uA		= 15625,
		.max_uA		= 250000,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(torch_led_supply),
	.consumer_supplies	= torch_led_supply,
};

static struct regulator_init_data reverse_init_data = {
	.constraints	= {
		.name		= "REVERSE",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.boot_on		= 0,
		.state_mem	= {
			.disabled	= true,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(reverse_supply),
	.consumer_supplies	= reverse_supply,
};

static struct max77665_regulator_data max77665_regulators[] = {
	{MAX77665_ESAFEOUT1, &safeout1_init_data,},
	{MAX77665_ESAFEOUT2, &safeout2_init_data,},
	{MAX77665_CHARGER, &charger_init_data,},
	{MAX77665_FLASH_LED, &flash_led_init_data,},
	{MAX77665_TORCH_LED, &torch_led_init_data,},
	{MAX77665_REVERSE, &reverse_init_data,},
};
#endif
