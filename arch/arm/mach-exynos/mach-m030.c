/* linux/arch/arm/mach-exynos/mach-m030.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *		http://www.meizu.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/fixed-m030.h>
#include <linux/memblock.h>
#include <linux/delay.h>
#include <linux/cma.h>
#ifdef CONFIG_REGULATOR_MAX8997
#include <linux/mfd/max8997.h>
#include <linux/mfd/max8997-m030.h>
#endif
#ifdef CONFIG_BATTERY_MX
#include <mach/mx_battery.h>
#endif
#ifdef CONFIG_BATTERY_MAX17042
#include <linux/power/max17042_battery.h>
#endif
#ifdef CONFIG_BCM4329
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <plat/sdhci.h>
#endif
#include <linux/mfd/wm8994/pdata.h>
#include <linux/scatterlist.h>
#include <linux/mmc/host.h>
#include <linux/pwm_backlight.h>
#include <linux/gpio_keys.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/power_supply.h>
#include <linux/devfreq-exynos4.h>
#include <linux/platform_data/exynos4_tmu.h>
#include <linux/platform_data/exynos4_ppmu.h>
#include <linux/exynos-cpufreq.h>
#include <mach/exynos_fiq_debugger.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <asm/system.h>

#include <mach/dev-ppmu.h>
#include <mach/dev.h>
#include <mach/regs-clock.h>
#include <mach/map.h>
#include <mach/dwmci.h>
#include <plat/mshci.h>
#ifdef CONFIG_BT
#include <mach/mx-rfkill.h>
#endif
#include <plat/gpio-cfg.h>
#include <plat/regs-serial.h>
#include <plat/exynos4.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/pd.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>
#include <plat/fimc.h>
#include <plat/s5p-mfc.h>
#include <plat/usbgadget.h>
#include <plat/backlight.h>
#include <plat/csis.h>
#include <plat/fimg2d.h>
#include <plat/sysmmu.h>
#include <plat/ehci.h>
#include <mach/dev-sysmmu.h>
#include <linux/mfd/bu26507.h>

#include <media/m6mo_platform.h>
#include <media/ov7690_platform.h>
#include <mach/usb-detect.h>
#ifdef CONFIG_SENSORS_LIS3DH
#include <linux/mx_lis3dh.h>
#endif
#ifdef CONFIG_SENSORS_L3G4200D
#include <linux/mx_l3g4200d.h>
#endif
#ifdef CONFIG_XMM6260_MODEM
#include <mach/modem.h>
#endif
#ifdef CONFIG_VIDEO_TVOUT
#include <plat/tvout.h>
#endif
#ifdef CONFIG_MHL_DRIVER
#include <linux/mhl.h>
#endif
#ifdef	CONFIG_AUDIENCE_A1028
#include <linux/a1028_soc.h>
#endif
#include <linux/mx_audio_platform.h>
#include <linux/mx_spdif_platform.h>

#if defined(CONFIG_S5P_MEM_CMA)
extern void __init mx_reserve_mem(void);
#else
static void __init mx_reserve_mem(void) {};
#endif

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define M030_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define M030_ULCON_DEFAULT	S3C2410_LCON_CS8

#define M030_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)
static struct s3c2410_uartcfg __initdata m030_uartcfgs[] = {
	[0] = {
		.hwport		= 0,
		.flags			= 0,
		.ucon		= M030_UCON_DEFAULT,
		.ulcon		= M030_ULCON_DEFAULT,
		.ufcon		= M030_UFCON_DEFAULT,
#if defined(CONFIG_BT) && defined(CONFIG_MX_SERIAL_TYPE)
		.wake_peer	= bt_uart_wake_peer,
#endif		
	},
	[1] = {
		.hwport		= 1,
		.flags			= 0,
		.ucon		= M030_UCON_DEFAULT,
		.ulcon		= M030_ULCON_DEFAULT,
		.ufcon		= M030_UFCON_DEFAULT,
		
	},
	[2] = {
		.hwport		= 2,
		.flags			= 0,
		.ucon		= M030_UCON_DEFAULT,
		.ulcon		= M030_ULCON_DEFAULT,
		.ufcon		= M030_UFCON_DEFAULT,
	},
#ifndef CONFIG_EXYNOS_FIQ_DEBUGGER
	[3] = {
		.hwport		= 3,
		.flags			= 0,
		.ucon		= M030_UCON_DEFAULT,
		.ulcon		= M030_ULCON_DEFAULT,
		.ufcon		= M030_UFCON_DEFAULT,
	},
#endif	
};

#if defined(CONFIG_BT)
static struct platform_device m030_bt_ctr = {
	.name = "bt_ctr",
	.id = -1,
};
#endif

#if defined(CONFIG_SAMSUNG_DEV_BACKLIGHT)
/* LCD Backlight data */
static struct samsung_bl_gpio_info m030_bl_gpio_info = {
	.no = EXYNOS4_GPD0(0),
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data m030_bl_data = {
	.pwm_id = 0,
	.max_brightness = 255,
	.dft_brightness = 180,		
	.pwm_period_ns  = 78770,
};
#endif

#ifdef CONFIG_EXYNOS4_DEV_DWMCI
static void m030_dwmci_cfg_gpio(int width)
{
	static int pre_width = -1;
	
	unsigned int gpio;

	if (pre_width == width)
		return;

	for (gpio = EXYNOS4_GPK0(0); gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPK1(3); gpio <= EXYNOS4_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3); gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}

	pre_width = width;
}

static struct dw_mci_board __initdata m030_dwmci_pdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
					    DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps				= MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio			= m030_dwmci_cfg_gpio,
};
#endif

#if defined(CONFIG_MFD_WM8994)
static struct regulator_consumer_supply wm8958_avdd1_supply =
	REGULATOR_SUPPLY("AVDD1", "0-001a");

static struct regulator_consumer_supply wm8958_dcvdd_supply =
	REGULATOR_SUPPLY("DCVDD", "0-001a");

static struct regulator_init_data wm8958_ldo1_data = {
	.constraints	= {
		.name		= "AVDD1",
		.min_uV 		= 2400000,
		.max_uV 		= 3100000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8958_avdd1_supply,
};

static struct regulator_init_data wm8958_ldo2_data = {
	.constraints	= {
		.name		= "DCVDD",
		.min_uV 		= 1000000,
		.max_uV 		= 1300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &wm8958_dcvdd_supply,
};

static struct wm8994_pdata wm8958_platform_data = {
	/* configure gpio1 function: 0x0001(Logic level input/output) */
	.gpio_defaults[0] = 0x0001,
	/* If the i2s0 and i2s2 is enabled simultaneously */
	.gpio_defaults[7] = 0x8100, /* GPIO8  DACDAT3 in */
	.gpio_defaults[8] = 0x0100, /* GPIO9  ADCDAT3 out */
	.gpio_defaults[9] = 0x0100, /* GPIO10 LRCLK3  out */
	.gpio_defaults[10] = 0x0100,/* GPIO11 BCLK3   out */
	.ldo[0] = { M030_GPIO_CODEC_LDO1, NULL, &wm8958_ldo1_data },
	.ldo[1] = { M030_GPIO_CODEC_LDO2, NULL, &wm8958_ldo2_data },
};
#endif
#ifdef	CONFIG_AUDIENCE_A1028
/*a1028*/
static struct a1028_platform_data  a1028_data = {
	.gpio_wakeup			= M030_GPIO_NOISE_WAKE,
	.gpio_reset			= M030_GPIO_NOISE_RST,	
};
#endif
#ifdef CONFIG_I2C_S3C2410
static struct i2c_board_info __initdata i2c_devs0[] = {
#if defined(CONFIG_MFD_WM8994)
	{
		I2C_BOARD_INFO("wm8958", (0x34>>1)),
		.platform_data	= &wm8958_platform_data,
	},
#endif
#ifdef	CONFIG_AUDIENCE_A1028
	{
		I2C_BOARD_INFO("a1028_soc", 0x3E),
		.platform_data	= &a1028_data,
	},
#endif
};
#endif//CONFIG_I2C_S3C2410

#ifdef CONFIG_S3C_DEV_I2C3
/* I2C3 */
static struct i2c_board_info __initdata i2c_devs3[]  = {
#ifdef CONFIG_REGULATOR_MAX8997
	{
		I2C_BOARD_INFO("max8997", (0xcc>>1)),
		.platform_data	= &m030_max8997_info,
		.irq = IRQ_EINT(5),
	}
#endif//CONFIG_REGULATOR_MAX8997
};
#endif//CONFIG_S3C_DEV_I2C3

/* max8997 FUELGAUGE*/
#ifdef CONFIG_BATTERY_MAX17042
struct max17042_reg_data max17042_init_data[] = {
	{ MAX17042_REG_CGAIN,		0x00,	0x40 },
	{ MAX17042_REG_MISCCFG,		0x03,	0x00 },
	{ MAX17042_REG_LEARNCFG,	0x06,	0x26 },
	{ MAX17042_REG_CONFIG,		0x1C,	0x22 },
	{ MAX17042_REG_FILTERCFG,	0x84,	0x8E },
};

struct max17042_reg_data max17042_alert_init_data[] = {
	/* SALRT Threshold setting to 6% => 5% wake lock */
	{ MAX17042_REG_SALRT_TH,	0x06,	0xFF },
	/* VALRT Threshold setting (disable) */
	{ MAX17042_REG_VALRT_TH,	0x00,	0xFF },
	/* TALRT Threshold setting (disable) */
	{ MAX17042_REG_TALRT_TH,	0x80,	0x7F },
};

bool max17042_is_low_batt(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}

	if (!(psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value)))
		if (value.intval > MX_BATTERY_SOC_3_6)
			return false;

	return true;
}
EXPORT_SYMBOL(max17042_is_low_batt);

static int max17042_low_batt_cb(int critical)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}
	if(critical)
		value.intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		value.intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	return psy->set_property(psy, POWER_SUPPLY_PROP_CAPACITY_LEVEL, &value);
}

static struct max17042_platform_data exynos4_max17042_info = {
	.low_batt_cb = max17042_low_batt_cb,
	.init = max17042_init_data,
	.init_count = sizeof(max17042_init_data) / sizeof(max17042_init_data[0]),
	.alert_init = max17042_alert_init_data,
	.alert_init_count = sizeof(max17042_alert_init_data) / sizeof(max17042_alert_init_data[0]),
	.alert_gpio = M030_INT4_FUG,
	.alert_irq = 0,
	.enable_current_sense = false,
	.enable_gauging_temperature = true,
};
#endif /*CONFIG_FUELGAUGE_MAX17042*/
/* sec_battery */
#ifdef CONFIG_BATTERY_MX
static struct mx_bat_platform_data m030_bat_pdata = {
	.fuel_gauge_name	= "max17042-fuelgauge",
	.charger_name		= "max8997-charger",
};

static struct platform_device m030_battery_device = {
	.name = "m030-battery",
	.id = -1,
	.dev.platform_data = &m030_bat_pdata,
};
#endif  //CONFIG_BATTERY_MX
#ifdef CONFIG_S3C_DEV_I2C5
/* I2C5 */
static struct i2c_board_info __initdata i2c_devs5[]  = {
#ifdef CONFIG_BATTERY_MAX17042
	{
		I2C_BOARD_INFO("max17042", 0x6c>>1),
		.platform_data	= &exynos4_max17042_info,
		.irq = IRQ_EINT(4),
	},
#endif//CONFIG_FUELGAUGE_MAX17042
};
#endif//CONFIG_S3C_DEV_I2C5

/* I2C8 */
static struct i2c_gpio_platform_data gpio_i2c8_data  = {
	.sda_pin		= M030_GPIO_MHL_SDA,
	.scl_pin		= M030_GPIO_MHL_SCL,
	.udelay			= 5,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

struct platform_device s3c_device_gpio_i2c8 = {
	.name	= "i2c-gpio",
	.id	= 8,
	.dev	= {
		.platform_data = &gpio_i2c8_data
	},
};

/*st lis3dh accelerometer sensor*/
#ifdef CONFIG_SENSORS_LIS3DH
static struct i2c_gpio_platform_data gpio_i2c9_data = {
	.sda_pin = M030_GPIO_GSEN_SDA,
	.scl_pin = M030_GPIO_GSEN_SCL,
	.udelay = 2,   /*the scl frequency is (500 / udelay) kHz*/
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_gpio_i2c9 = {
	.name = "i2c-gpio",
	.id = 9,
	.dev.platform_data = &gpio_i2c9_data,
};

static struct lis3dh_acc_platform_data lis3dh_platdata = {
	.poll_interval = 200,   /*default poll delay 200 ms*/
	.min_interval = 10,
	
	.g_range = LIS3DH_ACC_G_2G,

	.axis_map_x = 1,  /*x=-x, y=-y, z=z*/
	.axis_map_y = 0,
	.axis_map_z = 2,

	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 0,
};

static struct i2c_board_info i2c_devs9[] __initdata = {
	{I2C_BOARD_INFO("lis3dh", 0x19),
	.platform_data = &lis3dh_platdata,},
};
#endif

/*akm8975C compass sensor*/
#ifdef CONFIG_SENSORS_AKM8975C
static struct i2c_gpio_platform_data gpio_i2c10_data = {
	.sda_pin = M030_GPIO_COMP_SDA,
	.scl_pin = M030_GPIO_COMP_SCL,
	.udelay = 2,   /*the scl frequency is (500 / udelay) kHz*/
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_gpio_i2c10 = {
	.name = "i2c-gpio",
	.id = 10,
	.dev.platform_data = &gpio_i2c10_data,
};

static struct i2c_board_info i2c_devs10[] __initdata = {
	{I2C_BOARD_INFO("akm8975", 0x0c)},
};
#endif

/*st l3g4200d gyroscope sensor*/
#ifdef CONFIG_SENSORS_L3G4200D
static struct i2c_gpio_platform_data gpio_i2c11_data = {
	.sda_pin = M030_GPIO_GYRO_SDA,
	.scl_pin = M030_GPIO_GYRO_SCL,
	.udelay = 2,   /*the scl frequency is (500 / udelay) kHz*/
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_gpio_i2c11 = {
	.name = "i2c-gpio",
	.id = 11,
	.dev.platform_data = &gpio_i2c11_data,
};

static struct l3g4200d_platform_data l3g4200d_platdata = {
	.fs_range = L3G4200D_FS_2000DPS,

	.axis_map_x = 1,  /*x=y, y=-x, z=-z*/
	.axis_map_y = 0,
	.axis_map_z = 2,

	.negate_x = 0,   
	.negate_y = 1,
	.negate_z = 1,
};

static struct i2c_board_info i2c_devs11[] __initdata = {
	{I2C_BOARD_INFO("l3g4200d", 0x69),
	.platform_data = &l3g4200d_platdata,},
};
#endif

/*sharp gp2ap020a00f sensor*/
#ifdef CONFIG_SENSORS_GP2AP020A00F
static struct i2c_gpio_platform_data gpio_i2c12_data = {
	.sda_pin = M030_GPIO_GP2A_SDA,
	.scl_pin = M030_GPIO_GP2A_SCL,
	.udelay = 2,   /*the scl frequency is (500 / udelay) kHz*/
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_gpio_i2c12 = {
	.name = "i2c-gpio",
	.id = 12,
	.dev.platform_data = &gpio_i2c12_data,
};

static struct i2c_board_info i2c_devs12[] __initdata = {
	{I2C_BOARD_INFO("gp2ap020a00f", 0x39),
	.irq = M030_INT28_IR,},
};
#endif
/*ROHM bu26507gul led*/
#ifdef CONFIG_MFD_BU26507
static struct i2c_gpio_platform_data gpio_i2c13_data = {
	.sda_pin = M030_GPIO_LED_SDA,
	.scl_pin = M030_GPIO_LED_SCL,
	.udelay = 5,   /*the scl frequency is (500 / udelay) kHz*/
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
	.scl_is_output_only = 0,
};

static struct platform_device s3c_device_gpio_i2c13 = {
	.name = "i2c-gpio",
	.id = 13,
	.dev.platform_data = &gpio_i2c13_data,
};

static struct bu26507_platform_data __initdata bu26507gul = {
	.name = "m030_led",
	.sync_pin = M030_GPIO_LED_SYNC,
	.reset_pin = M030_GPIO_LED_RST,
	.led_on = {1,1,0,1,0,1,0,1,0,1},
};

static struct i2c_board_info __initdata i2c_devs13[]  = {
	{
		I2C_BOARD_INFO("bu26507-led", 0x74),
		.platform_data = &bu26507gul,
	},
};
#endif

/*keyboard*/
#ifdef CONFIG_KEYBOARD_GPIO
#define KEYPAD_FILTER_TIME 50
struct gpio_keys_button m030_keypad_table[] = {
	{
		.code = KEY_POWER,
		.gpio = M030_GPIO_KEY_OFF,
		.active_low = 1,
		.desc = "key_power",
		.type = EV_KEY,
		.wakeup = 1,
		.debounce_interval =  KEYPAD_FILTER_TIME,
		.can_disable = 0,
	},{
		.code = KEY_VOLUMEUP,
		.gpio = M030_GPIO_KEY_UP,
		.active_low = 1,
		.desc = "key_volume+",
		.type = EV_KEY,
		.wakeup = 0,
		.debounce_interval = KEYPAD_FILTER_TIME,
		.can_disable = 0,
	},{
		.code = KEY_VOLUMEDOWN,
		.gpio = M030_GPIO_KEY_DOWN,
		.active_low = 1,
		.desc = "key_volume-",
		.type = EV_KEY,
		.wakeup = 0,
		.debounce_interval = KEYPAD_FILTER_TIME,
		.can_disable = 0,
	},{
		.code = KEY_HOME,
		.gpio = M030_GPIO_KEY_HOME,
		.active_low = 1,
		.desc = "key_home",
		.type = EV_KEY,
		.wakeup = 1,
		.debounce_interval = KEYPAD_FILTER_TIME,
		.can_disable = 0,
	},{
		.code = KEY_OK,
		.gpio = M030_GPIO_KEY_OK,
		.active_low = 1,
		.desc = "key_ok",
		.type = EV_KEY,
		.wakeup = 0,
		.debounce_interval = KEYPAD_FILTER_TIME,
		.can_disable = 0,
	},{
		.code = KEY_BACK,
		.gpio = M030_GPIO_KEY_BACK,
		.active_low = 1,
		.desc = "key_back",
		.type = EV_KEY,
		.wakeup = 0,
		.debounce_interval = KEYPAD_FILTER_TIME,
		.can_disable = 0,
	},
};

static struct gpio_keys_platform_data m030_keypad_data = {
	.buttons	= m030_keypad_table,
	.nbuttons	= ARRAY_SIZE(m030_keypad_table),
};

static struct platform_device m030_keypad = {
	.name	= "gpio-keys",
	.dev	= {
			.platform_data = &m030_keypad_data,
	},
	.id	= -1,
};
#endif

static struct usb_detect_platform_data usb_detect_data = {
	.usb_vbus_gpio = M030_INT21_USB,
	.usb_host_gpio = M030_INT19_USB_MHL,
	.usb_dock_gpio = -1,
};

static struct platform_device usb_detect_device = {
	.name	 = "m030_usb_det",
	.id = -1,
	.dev	 = {
		.platform_data    = &usb_detect_data,
	},
	.num_resources	= 0,
	.resource 	= NULL,
};

#ifdef CONFIG_VIDEO_FIMG2D
static struct fimg2d_platdata __initdata fimg2d_data  = {
	.hw_ver = 0x30,
	.parent_clkname = "mout_g2d0",
	.clkname = "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate = 267 * 1000000,	/* 266 Mhz */
};
#endif
#ifdef CONFIG_XMM6260_MODEM
static struct modem_platform_data xmm6260_data = {
	.name = "xmm6260",
	.gpio_phone_on = M030_GPIO_MODEM_ON,
	.gpio_cp_reset = M030_GPIO_MODEM_RST,
	.gpio_pmu_reset = M030_GPIO_MODEM_RST_FULL,
	.gpio_ipc_slave_wakeup = M030_GPIO_SLAVE_WAKEUP,
	.gpio_ipc_host_wakeup = M030_GPIO_HOST_WAKEUP,
	.gpio_suspend_request = M030_GPIO_REQUEST_SUSPEND,
	.gpio_active_state = M030_GPIO_HOST_ACTIVE,
	.gpio_cp_dump_int = M030_GPIO_MODEM_DUMP_INT,
	.gpio_cp_reset_int = M030_GPIO_MODEM_RESET_INT,
	.gpio_sim_detect_int = M030_INT16_SIMDETECT,
	.wakeup = 1,
};

static struct platform_device xmm6260_modem = {
	.name = "xmm_modem",
	.id = -1,

	.dev = {
		.platform_data = &xmm6260_data,
	},
};
#endif
#ifdef CONFIG_WAKEUP_ASSIST
static struct platform_device wakeup_assist_device = {
	.name   = "wakeup_assist",
};
#endif
#if defined(CONFIG_SND_SAMSUNG_I2S)
/*low: BB  high:AP*/
static void m030_audio_switch(int High)
{
	int err;
	/* LDO1 */
	err = gpio_request(M030_GPIO_SWITCH_SEL, "GPF0");
	if (err)
		printk(KERN_ERR "#### failed to request M030_GPIO_SWITCH_SEL switch####\n");

	if (High) {
		s3c_gpio_setpull(M030_GPIO_SWITCH_SEL, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(M030_GPIO_SWITCH_SEL,S3C_GPIO_INPUT);
	} else 	{
		s3c_gpio_setpull(M030_GPIO_SWITCH_SEL, S3C_GPIO_PULL_DOWN);
		s3c_gpio_cfgpin(M030_GPIO_SWITCH_SEL,S3C_GPIO_INPUT);
	}

	gpio_free(M030_GPIO_SWITCH_SEL);
};

static struct mx_audio_platform_data m030_audio_data = {
	.audio_switch = m030_audio_switch,
};

static struct platform_device m030_audio_device={
	.name = "mx-audio",
	.id = -1,
	.dev = {
		.platform_data = &m030_audio_data,
	},
};
#endif
#ifdef CONFIG_SND_SOC_SAMSUNG_SMDK_SPDIF
static void m030_spdif_output_enable(int bEn)
{
	int err;
	err = gpio_request(M030_GPIO_SPDIF_EN, "GPF0");
	if (err)
		printk(KERN_ERR "#### failed to request M030_GPIO_SWITCH_SEL switch####\n");

	if(!bEn) {
		s3c_gpio_cfgpin(M030_GPIO_SPDIF_EN, S3C_GPIO_OUTPUT);  
		s3c_gpio_setpull(M030_GPIO_SPDIF_EN,  S3C_GPIO_PULL_NONE);
		gpio_set_value(M030_GPIO_SPDIF_EN,  1);
	} else {
		s3c_gpio_cfgpin(M030_GPIO_SPDIF_EN, S3C_GPIO_OUTPUT);  
		s3c_gpio_setpull(M030_GPIO_SPDIF_EN,  S3C_GPIO_PULL_NONE);
		gpio_set_value(M030_GPIO_SPDIF_EN,  0);
	}

	gpio_free(M030_GPIO_SPDIF_EN);
}

static struct mx_spdif_platform_data m030_spdif_data={
	.spdif_output_enable = m030_spdif_output_enable,
};

static struct platform_device m030_spdif_device={
	.name = "mx-spdif",
	.id = -1,
	.dev = {
		.platform_data = &m030_spdif_data,
	},
};
#endif
/* USB EHCI */
#ifdef CONFIG_USB_EHCI_S5P
static struct s5p_ehci_platdata m030_ehci_pdata;

static void __init m030_ehci_init(void)
{
	struct s5p_ehci_platdata *pdata = &m030_ehci_pdata;

	s5p_ehci_set_platdata(pdata);
}
#endif

#ifdef CONFIG_UBLOX_GPS
static struct platform_device ublox_gps = {
	.name = "ublox-gps",
	.id = -1,
};
#endif

#ifdef CONFIG_EXYNOS4_DEV_PPMU
/* image, peril, mfc_l, g3d */
static struct exynos4_ppmu_pd __initdata ppmu_pd_dmc_l = {
	.name = "ppmu_dmc_l",
	.count[0] = 0xfff80000U,
	.event[0] = RW_BUSY,
	.event[1] = RD_LATENCY,
	.event[2] = WR_LATENCY,
	.event[3] = RW_DATA,
};

/* lcd, fsys, cam, perir, mfc_r */
static struct exynos4_ppmu_pd __initdata ppmu_pd_dmc_r = {
	.name = "ppmu_dmc_r",
	.count[0] = 0xfff80000U,
	.event[0] = RW_BUSY,
	.event[1] = RD_REQ,
	.event[2] = WR_REQ,
	.event[3] = RW_DATA,
};

static struct exynos4_ppmu_pd __initdata ppmu_pd_cpu = {
	.name = "ppmu_cpu",
	.count[0] = 0xf0000000U,
	.event[0] = RW_BUSY,
	.event[1] = RD_REQ,
	.event[2] = WR_REQ,
	.event[3] = RW_DATA,
};
#endif

#ifdef CONFIG_BUSFREQ_OPP
static struct device_domain busfreq;
static struct platform_device exynos4_busfreq = {
	.id	= -1,
	.name	= "exynos-busfreq",
};
#endif

#ifdef CONFIG_CPU_FREQ
static struct exynos_cpufreq_platdata __initdata m030_cpufreq_pd = {
	.regulator = "vdd_arm",
	.gpio_dvfs = true,
	.polling_ms = 50,	//50mS
};
#endif

static struct platform_device __initdata *m030_devices[]  = {
#ifdef CONFIG_WAKEUP_ASSIST
	&wakeup_assist_device,
#endif
	&usb_detect_device,
	&s3c_device_adc,
#if defined(CONFIG_ARCH_EXYNOS4) && defined(CONFIG_EXYNOS_DEV_PD)
	&exynos4_device_pd[PD_MFC],
	&exynos4_device_pd[PD_G3D],
	&exynos4_device_pd[PD_LCD0],
	&exynos4_device_pd[PD_CAM],
	&exynos4_device_pd[PD_TV],
	&exynos4_device_pd[PD_GPS],
	&exynos4_device_pd[PD_GPS_ALIVE],
#endif

/*keyboard*/
#ifdef CONFIG_KEYBOARD_GPIO
	&m030_keypad,
#endif

	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c3,
	&s3c_device_i2c5,
	&s3c_device_i2c6,	
	&s3c_device_i2c7,

#if defined(CONFIG_MHL_DRIVER)
	&s3c_device_gpio_i2c8,
#endif

#ifdef CONFIG_SENSORS_LIS3DH
	&s3c_device_gpio_i2c9,
#endif

#ifdef CONFIG_SENSORS_AKM8975C
	&s3c_device_gpio_i2c10,
#endif

#ifdef CONFIG_SENSORS_L3G4200D
	&s3c_device_gpio_i2c11,
#endif

#ifdef CONFIG_SENSORS_GP2AP020A00F
	&s3c_device_gpio_i2c12,
#endif

#ifdef  CONFIG_MFD_BU26507
	&s3c_device_gpio_i2c13,
#endif
#if defined(CONFIG_REGULATOR_FIXED_VOLTAGE)
	&m030_fixed_voltage0,
	&m030_fixed_voltage2,
	&m030_fixed_voltage3,
	&m030_fixed_voltage4,
	&m030_fixed_voltage5,
	&m030_fixed_voltage6,
	&m030_fixed_voltage7,
	&m030_fixed_voltage8,
	&wm8958_fixed_voltage0,
	&wm8958_fixed_voltage1,
#endif

#ifdef CONFIG_XMM6260_MODEM
	&xmm6260_modem,
#endif

#ifdef CONFIG_EXYNOS_IOMMU
	&SYSMMU_PLATDEV(2d),
#endif
#if defined(CONFIG_EXYNOS4_DEV_DWMCI)
	&exynos_device_dwmci,
#endif
#ifdef CONFIG_EXYNOS4_DEV_MSHC
	&s3c_device_mshci,
#endif
#if defined(CONFIG_FB_S5P)
	&s3c_device_fb,
#endif
#if defined(CONFIG_FB_S5P_MIPI_DSIM)
	&s5p_device_mipi_dsim0,
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	&s5p_device_fimg2d,
#endif
#if defined(CONFIG_VIDEO_FIMC)
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_fimc3,
#endif
#if defined(CONFIG_VIDEO_JPEG) || defined(CONFIG_VIDEO_JPEG_MX)
	&s5p_device_jpeg,
#endif
#if defined(CONFIG_VIDEO_MFC5X)
	&s5p_device_mfc,
	&s5p_device_mfc_l,
	&s5p_device_mfc_r,
#endif
#if defined(CONFIG_SND_SAMSUNG_I2S)
	&exynos_device_i2s0,
	&exynos_device_pcm1,	
	&samsung_asoc_dma,
	&samsung_asoc_idma,
	&m030_audio_device,
#endif
#ifdef CONFIG_SND_SOC_SAMSUNG_SMDK_SPDIF	
	&exynos_device_spdif,
	&m030_spdif_device,
#endif
#ifdef CONFIG_VIDEO_TVOUT
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	&s3c_device_csis0,
#endif

#ifdef CONFIG_S3C2410_WATCHDOG
	&s3c_device_wdt,
#endif

#if defined(CONFIG_S3C_DEV_RTC)	
	&s3c_device_rtc,
#endif

#ifdef CONFIG_BT
	&m030_bt_ctr,
#endif
#if defined(CONFIG_USB_GADGET)	
	&s3c_device_usbgadget,
#endif
#if defined(CONFIG_BATTERY_MX)
	&m030_battery_device,
#endif

#ifdef CONFIG_CPU_FREQ
	&exynos_device_cpufreq,
#endif

#ifdef CONFIG_EXYNOS4_DEV_PPMU
	&PPMU_PLATDEV(dmc_l),
	&PPMU_PLATDEV(dmc_r),
	&PPMU_PLATDEV(cpu),
#endif

#ifdef CONFIG_EXYNOS4_BUS_DEVFREQ
	&exynos_device_busfreq,
#endif

#ifdef CONFIG_BUSFREQ_OPP
	&exynos4_busfreq,
#endif

#ifdef CONFIG_SENSORS_EXYNOS4_TMU
	&exynos4_device_tmu,
#endif

#ifdef CONFIG_UBLOX_GPS
	&ublox_gps,
#endif

#ifdef CONFIG_VIDEO_ROTATOR
	&s5p_device_rotator,
#endif
};

#define S3C2410_RTCCON	      (0x40)
#define S3C2410_RTCCON_CLKOUTEN  (1<<9)

static void __init m030_map_io(void)
{
	clk_xusbxti.rate = 24000000;

	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(m030_uartcfgs, ARRAY_SIZE(m030_uartcfgs));
	writel((readl(EXYNOS4_CLKDIV_PERIL0) & ~(0xf)) | 0x7, EXYNOS4_CLKDIV_PERIL0);

	/* enable rtc clock output to wifi/bt sleeping, GPS */
	do {
		unsigned int rtc_con;
		static struct clk *rtc_clk;

		rtc_clk = clk_get(NULL, "rtc");
		if (!IS_ERR(rtc_clk))
			clk_enable(rtc_clk);

		rtc_con = readw(S3C_VA_RTC + S3C2410_RTCCON);
		rtc_con |= S3C2410_RTCCON_CLKOUTEN;
		rtc_con &= ~(0xF<<4);
		writew(rtc_con, S3C_VA_RTC + S3C2410_RTCCON);
	} while (0);

	mx_reserve_mem();
}

#ifdef CONFIG_EXYNOS_IOMMU
static void __init m030_sysmmu_init(void)
{

	ASSIGN_SYSMMU_POWERDOMAIN(2d, &exynos4_device_pd[PD_LCD0].dev);
#ifdef CONFIG_VIDEO_FIMG2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(2d).dev, &s5p_device_fimg2d.dev);
#endif
}
#endif


static void __init m030_gpio_irq_init(void)
{
	unsigned gpio;
	int ret;

	gpio = M030_GPIO_CAMERA0_EINT;
	ret = gpio_request(gpio, "M6MO_INT");
	ret = s5p_register_gpio_interrupt(gpio);
	gpio_free(gpio);
}

static void __init m030_machine_init(void)
{
	system_rev = 0x4210;

#ifdef CONFIG_EXYNOS_FIQ_DEBUGGER
	exynos_serial_debug_init(CONFIG_S3C_LOWLEVEL_UART_PORT, 0);
#endif

	m030_gpio_irq_init();
	
#ifdef CONFIG_I2C_S3C2410
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
#endif
#ifdef CONFIG_S3C_DEV_I2C1
	s3c_i2c1_set_platdata(NULL);
#endif
#ifdef CONFIG_S3C_DEV_I2C3
	s3c_i2c3_set_platdata(NULL);
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));
#endif
#ifdef CONFIG_S3C_DEV_I2C5
	s3c_i2c5_set_platdata(NULL);
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
#endif

#ifdef CONFIG_SENSORS_LIS3DH
	i2c_register_board_info(9, i2c_devs9, ARRAY_SIZE(i2c_devs9));
#endif

#ifdef CONFIG_SENSORS_AKM8975C
	i2c_register_board_info(10, i2c_devs10, ARRAY_SIZE(i2c_devs10));
#endif

#ifdef CONFIG_SENSORS_L3G4200D
	i2c_register_board_info(11, i2c_devs11, ARRAY_SIZE(i2c_devs11));
#endif

#ifdef CONFIG_SENSORS_GP2AP020A00F
	i2c_register_board_info(12, i2c_devs12, ARRAY_SIZE(i2c_devs12));
#endif

#ifdef  CONFIG_MFD_BU26507
	i2c_register_board_info(13, i2c_devs13, ARRAY_SIZE(i2c_devs13));
#endif

#if defined(CONFIG_SAMSUNG_DEV_BACKLIGHT)
	samsung_bl_set(&m030_bl_gpio_info, &m030_bl_data);
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimg2d.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif
#if defined(CONFIG_VIDEO_JPEG) || defined(CONFIG_VIDEO_JPEG_MX)
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_jpeg.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	exynos_dwmci_set_platdata(&m030_dwmci_pdata);
#endif

	s5p_device_mfc.dev.parent = &exynos4_device_pd[PD_MFC].dev;
	exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 200 * MHZ);

#if defined(CONFIG_USB_GADGET)
	s5p_usbgadget_set_platdata(NULL);
#endif
#ifdef CONFIG_EXYNOS_IOMMU
	m030_sysmmu_init();
#endif
#ifdef CONFIG_USB_EHCI_S5P
	m030_ehci_init();
#endif

#ifdef CONFIG_CPU_FREQ
	exynos_cpufreq_set_platdata(&m030_cpufreq_pd);
#endif

#ifdef CONFIG_EXYNOS4_DEV_PPMU
	exynos_ppmu_set_pd(&ppmu_pd_dmc_l, &PPMU_PLATDEV(dmc_l));
	exynos_ppmu_set_pd(&ppmu_pd_dmc_r, &PPMU_PLATDEV(dmc_r));
	exynos_ppmu_set_pd(&ppmu_pd_cpu, &PPMU_PLATDEV(cpu));
#endif

#ifdef CONFIG_EXYNOS4_BUS_DEVFREQ
	exynos_busfreq_set_platdata();
#endif

#ifdef CONFIG_BUSFREQ_OPP
	dev_add(&busfreq, &exynos4_busfreq.dev);
#endif

#ifdef CONFIG_SENSORS_EXYNOS4_TMU
	exynos4_tmu_set_platdata();
#endif

	platform_add_devices(m030_devices, ARRAY_SIZE(m030_devices));
}

MACHINE_START(M030, "MX")
	/* Maintainer: Wenbin Wu <wenbinwu@meizu.com> */
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq		= exynos4_init_irq,
	.map_io		= m030_map_io,
	.init_machine	= m030_machine_init,
	.timer		= &exynos4_timer,
MACHINE_END
