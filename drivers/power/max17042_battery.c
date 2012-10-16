/*
 *  max17042-fuelgauge.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2010 Samsung Electronics
 *
 *  based on max17040_battery.c
 *
 *  <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/async.h>
#include <linux/wakelock.h>

#include <mach/gpio-common.h>

static ssize_t sec_fg_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t sec_fg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count);

struct max17042_chip {
	struct i2c_client		*client;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work		work;
	struct power_supply		battery;
	struct max17042_platform_data	*pdata;

	int vcell;			/* battery voltage */
	int avgvcell;		/* average battery voltage */
	int vfocv;		/* calculated battery voltage */
	int soc;			/* battery capacity */
	int raw_soc;		/* fuel gauge raw data */
	int temperature;
	int current_now;
	int tte_now;
	int ttf_now;
	u64 loose_reset_start;
	int fuel_alert_soc;		/* fuel alert threshold */
	bool is_fuel_alerted;	/* fuel alerted */
	struct wake_lock fuel_alert_wake_lock;
	struct wake_lock fuel_reset_wake_lock;
	int factory_mode;
	int test_mode;

	int low_vcell_count;
	volatile int need_refresh;
};

// shut down after 120s
#define STOP_COUNT 12
#define WARN_COUNT 5

static int debug_mode;

#define fuelgauge_info(dev, format, arg...)		\
	({ if (debug_mode) dev_printk(KERN_INFO, dev, format, ##arg); 0; })

static int get_soc_quick(struct i2c_client *client);
static int get_temp_quick(struct i2c_client *client);

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
						  struct max17042_chip,
						  battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		switch (val->intval) {
		case 0:	/*vcell */
			val->intval = chip->vcell;
			break;
		case 1:	/*vfocv */
			val->intval = chip->vfocv;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = chip->avgvcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		switch (val->intval) {
		case 0:	/*normal soc */
			val->intval = chip->soc;
			break;
		case 1: /*raw soc */
			val->intval = chip->raw_soc;
			break;
		case 2: /* quick soc */
			val->intval = get_soc_quick(chip->client);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		switch (val->intval) {
		case 0: /* normal temperature */
			val->intval = chip->temperature / 100;
			break;
		case 1: /* quick temperature */
			val->intval = get_temp_quick(chip->client) / 100;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->current_now;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = chip->tte_now;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = chip->ttf_now;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17042_write_reg(struct i2c_client *client, int reg, u8 * buf)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17042_read_reg(struct i2c_client *client, int reg, u8 * buf)
{
	int ret;

#ifndef NO_READ_I2C_FOR_MAXIM
	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
#else
	return 1;
#endif
}

static void max17042_write_reg_array(struct i2c_client *client,
				struct max17042_reg_data *data, int count)
{
	int i;

	for (i = 0; i < count; ++i)
		max17042_write_reg(client, data[i].reg_addr, (u8 *) &(data[i].reg_data1) );
}

static void max17042_init_regs(struct i2c_client *client)
{
	struct max17042_platform_data *pdata = client->dev.platform_data;

	dev_dbg(&client->dev, "%s\n", __func__);

#ifndef NO_READ_I2C_FOR_MAXIM
	max17042_write_reg_array(client, pdata->init, pdata->init_count);

	max17042_write_reg_array(client, pdata->alert_init, pdata->alert_init_count);
#endif
}

static void max17042_get_vfocv(struct i2c_client *client);
static int max17042_read_vfocv(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	max17042_get_vfocv(client);

	return chip->vfocv;
}

static int max17042_read_vfsoc_unchecked(struct i2c_client *client)
{
	int raw_soc;
	u8 data[2];

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_SOC_VF, data) < 0)
		return 0;

	raw_soc = (data[1] * 100) + (data[0] * 100 / 256);
#else
	raw_soc = 21;
#endif
	return raw_soc;
}

static void max17042_get_current(struct i2c_client *client);
static int max17042_read_current(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	max17042_get_current(client);

	return chip->current_now;
}

static void max17042_get_vcell(struct i2c_client *client);
static int max17042_read_vcell(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	max17042_get_vcell(client);

	return chip->vcell;
}

static void max17042_reset_soc(struct i2c_client *client)
{
	u8 data[2];

#ifndef NO_READ_I2C_FOR_MAXIM
	dev_info(&client->dev, "%s : Before quick-start - "
		"VfOCV(%d), VfSOC(%d)\n",
		__func__, max17042_read_vfocv(client),
		max17042_read_vfsoc_unchecked(client));

	while(1) {
		if (max17042_read_reg(client, MAX17042_REG_MISCCFG, data) < 0)
			return;

		/* Set bit10 makes quick start */
		data[1] |= (0x1 << 2);
		data[1] |= (0x1 << 4);
		max17042_write_reg(client, MAX17042_REG_MISCCFG, data);

		if (max17042_read_reg(client, MAX17042_REG_MISCCFG, data) < 0)
			return;
		if(data[1] & (0x1 << 4)) {
			break;
		}
	}
	printk("reset setp T2 end\n");

	while(1) {
		if (max17042_read_reg(client, MAX17042_REG_MISCCFG, data) < 0)
			return;

		data[1] &= 0xef;
		max17042_write_reg(client, MAX17042_REG_MISCCFG, data);

		if (max17042_read_reg(client, MAX17042_REG_MISCCFG, data) < 0)
			return;
		if((data[1] & (0x1 << 4)) == 0) {
			break;
		}
	}
	printk("reset setp T4 end\n");

	msleep(500);

	data[0] = 0xB8;
	data[1] = 0x0B;
	max17042_write_reg(client, 0x10, data);

	msleep(500);

	dev_info(&client->dev, "%s : After quick-start - "
		"VfOCV(%d), VfSOC(%d)\n",
		__func__, max17042_read_vfocv(client),
		max17042_read_vfsoc_unchecked(client));
#endif

	return;
}

static void max17042_get_tte(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	int fg_tte;

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_TTE, data) < 0)
		return;

	fg_tte = *(u16 *)data * 45 / 8;
	chip->tte_now = fg_tte;
#else
	chip->tte_now = 0;
#endif

	fuelgauge_info(&client->dev, "tte = %d\n", chip->tte_now);
}

static void max17042_get_ttf(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	int full_cap;
	int percent;
	int fg_ttf;
	int current_now;

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_FULL_CAP, data) < 0)
		return;
	full_cap = *(u16 *)data / 2;

	if (max17042_read_reg(client, MAX17042_REG_SOC_REP, data) < 0)
		return;

	percent = data[1];
	percent = min(percent, 100);
	current_now = chip->current_now;

	if(current_now != 0)
		fg_ttf = full_cap * (100 - percent) / 100 * 3600 / current_now;
	else
		fg_ttf = -1;

	chip->ttf_now = fg_ttf;
#else
	chip->ttf_now = 0;
#endif

	fuelgauge_info(&client->dev, "ttf = %d\n", chip->ttf_now);
}

static void max17042_get_current(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	long fg_current;

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_CURRENT, data) < 0)
		return;

	fg_current = *(s16 *)data * 15625 / 100000;
	chip->current_now = fg_current;
#else
	chip->current_now = 0;
#endif

	fuelgauge_info(&client->dev, "current = %d\n", chip->current_now);
}

static void max17042_get_vcell(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_VCELL, data) < 0)
		return;

	chip->vcell = ((data[0] >> 3) + (data[1] << 5)) * 625;

	if (max17042_read_reg(client, MAX17042_REG_AVGVCELL, data) < 0)
		return;

	chip->avgvcell = ((data[0] >> 3) + (data[1] << 5)) * 625;
#else
	chip->vcell = 4000000;
	chip->avgvcell = 4000000;
#endif

	fuelgauge_info(&client->dev, "vcell = %d\n", chip->vcell);
}

static void max17042_get_vfocv(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_VFOCV, data) < 0)
		return;

	chip->vfocv = ((data[0] >> 3) + (data[1] << 5)) * 625;
#else
	chip->vfocv = 4000 * 1000;
#endif

	fuelgauge_info(&client->dev, "vfocv = %d\n", chip->vfocv);
}


static int is_charging(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	int online = 0;
	if (psy != NULL) {
		psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		online = value.intval;
	}
	return !!online;
}

static int max17042_recalc_soc(struct i2c_client *client)
{
	u8 data[2];
	u16 avgvcell = 0;
	u16 vfocv = 0;
	u32 temp_vfocv = 0;
	int soc;

#ifndef NO_READ_I2C_FOR_MAXIM
	max17042_read_reg(client, MAX17042_REG_AVGVCELL, data);
	avgvcell = (data[1] << 8);
	avgvcell |= data[0];

	max17042_read_reg(client, MAX17042_REG_VFOCV, data);
	vfocv = (data[1] << 8);
	vfocv |= data[0];

	temp_vfocv = (4 * vfocv + 1 * avgvcell) / 5;

	data[1] = temp_vfocv >> 8;
	data[0] = 0x00FF & temp_vfocv;
	dev_info(&client->dev, "forced write to vfocv %d mV\n",
		 (temp_vfocv >> 4) * 125 / 100);
	max17042_write_reg(client, MAX17042_REG_VFOCV, data);

	msleep(200);

	max17042_read_reg(client, MAX17042_REG_SOC_VF, data);
	soc = min((int)data[1], 100);

	max17042_read_reg(client, MAX17042_REG_VCELL, data);
	dev_info(&client->dev, "new vcell = %d, vfocv = %d, soc = %d\n",
		 ((data[0] >> 3) + (data[1] << 5)) * 625 / 1000,
		 max17042_read_vfocv(client), soc);
#else
	soc = 100;
#endif

	return soc;
}

static s32 dump_reg(struct i2c_client *client, int reg, char *data)
{
	int retries;
	for(retries = 0; retries < 5; ++retries) {
		int ret = max17042_read_reg(client, reg, data);
		if(ret >= 0){
			return 0;
		}
		printk("%s err, reg=%d, retry \n", __FUNCTION__, reg);
		msleep_interruptible(5);
	}
	return -1;
}

static int dump_fuelgauge(struct i2c_client *client, char *buffer)
{
	unsigned int reg;
	char *buf;
	buf = buffer;

	for(reg = 0x00; reg <= 0xFF; reg += 1) {
		if(dump_reg(client, reg, buf + reg * 2))
			return 0;
	}
#if 0
	for(reg = 0x00; reg <= 0xFF; reg +=1) {
		printk("0x%02x=0x%04x ", reg, ((unsigned short *)buf)[reg]);
		if((reg + 1) % 5 == 0)
			printk("\n");
	}
	return 0;
#endif
	return (0xFF + 1) * 2;
}

static int fuelgauge_loose_check(struct i2c_client *client, int raw_soc)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	int vfocv = 0;
	int reset = 0;

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_VFOCV, data) < 0)
		return 1;
	vfocv = ((data[0] >> 3) + (data[1] << 5)) * 625;

	if(vfocv > 4000 * 1000) {
		if(raw_soc < 60) {
			reset = 1;
		}
	} else if(vfocv > 3800 * 1000) {
		if(raw_soc < 30) {
			reset = 1;
		}
	} else if(vfocv > 3700 * 1000) {
		if(raw_soc < 10) {
			reset = 1;
		}
	}
#endif
	fuelgauge_info(&chip->client->dev, "vfocv %d, soc %d, reset %d\n", vfocv, raw_soc, reset);
	return reset;
}

static int max17042_reset_por(struct i2c_client *client);
static int import_battery_model(struct i2c_client *client);
static void max17042_get_soc(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	int raw_soc;
	int need_correct = 0;

	if(chip->factory_mode) {
		chip->raw_soc = 21;
		chip->soc = 21;
	} else {
#ifndef NO_READ_I2C_FOR_MAXIM
		if (max17042_read_reg(client, MAX17042_REG_SOC_VF, data) < 0)
			return;
		raw_soc = (data[1] * 100) + (data[0] * 100 / 256);
		fuelgauge_info(&chip->client->dev, "raw_soc(%d)(0x%02x%02x)\n", raw_soc, data[1], data[0]);

		raw_soc = min(raw_soc / 100, 100);

		if(chip->need_refresh && raw_soc != 0) {
			fuelgauge_info(&chip->client->dev, "diff maybe not true, calac the diff next time\n");
			chip->need_refresh = 0;
		} else {
			if(raw_soc == 0) {
				printk("raw_soc zero error, need to reset the fuelgauge\n");
				need_correct = 1;
			} else {
				int diff;
				if (chip->raw_soc > raw_soc)
					diff = chip->raw_soc - raw_soc;
				else
					diff = raw_soc - chip->raw_soc;
				if(diff > 5) {
					printk("soc diff error, need to reset the fuelgauge\n");
					need_correct = 1;
				}
			}

		}

		if(need_correct) {
			wake_lock(&chip->fuel_reset_wake_lock);
			while(1) {
				int diff;

				max17042_reset_por(chip->client);
				import_battery_model(client);
				max17042_init_regs(client);

				raw_soc = max17042_recalc_soc(client);

				if (chip->raw_soc > raw_soc)
					diff = chip->raw_soc - raw_soc;
				else
					diff = raw_soc - chip->raw_soc;
				if(diff < 10)
					break;
				msleep(5000);
			}
			wake_unlock(&chip->fuel_reset_wake_lock);
		} else {
			if(fuelgauge_loose_check(client, raw_soc)) {
				u64 elapse = (u64)msecs_to_jiffies(30 * 60 * 1000);
				if( chip->loose_reset_start + elapse >= get_jiffies_64()){
					printk("fuelgauge loose check error, but reset too frequent\n");
				} else {
					wake_lock(&chip->fuel_reset_wake_lock);
					printk("fuelgauge loose check error, reset the fuelgauge\n");
					chip->loose_reset_start = get_jiffies_64();

					max17042_read_reg(client, MAX17042_REG_VFOCV, data);
					max17042_reset_por(client);
					import_battery_model(client);
					max17042_write_reg(client, MAX17042_REG_VFOCV, data);
					msleep(200);

					wake_unlock(&chip->fuel_reset_wake_lock);

					if (max17042_read_reg(client, MAX17042_REG_SOC_VF, data) < 0)
						return;
					raw_soc = (data[1] * 100) + (data[0] * 100 / 256);
					fuelgauge_info(&chip->client->dev, "raw_soc(%d)(0x%02x%02x)\n", raw_soc, data[1], data[0]);
					raw_soc = min(raw_soc / 100, 100);
				}
			}
		}

		chip->raw_soc = raw_soc;

		if(is_charging()) {
			chip->low_vcell_count = 0;
			chip->soc = (raw_soc <= 5) ? 0 : raw_soc;
		} else {
			int soc_temp = 100;

			if(raw_soc <= 12) {
				if(chip->vcell < 3260 * 1000) {
					chip->low_vcell_count += 6;
				} else if(chip->vcell < 3400 * 1000) {
					chip->low_vcell_count += 1;
				}

				if(chip->low_vcell_count){
					printk("low_vcell_count %d\n", chip->low_vcell_count);
					if(chip->low_vcell_count >= STOP_COUNT) {
						if(raw_soc > 8)
							chip->raw_soc = raw_soc = max17042_recalc_soc(client);
						printk("to STOP the device (raw_soc:%d)\n", chip->raw_soc);
						soc_temp = 0;
					} else if(chip->low_vcell_count >= WARN_COUNT) {
						soc_temp = min(chip->soc, 7);
					} else {
						soc_temp = (chip->soc >= 9) ? 9 : (chip->soc >= 7 ? 7 : 0);
					}
				}
			}
			soc_temp = min(raw_soc, soc_temp);
			chip->soc = (soc_temp <= 5) ? 0 : soc_temp;
		}

		if(chip->soc == 0 || chip->low_vcell_count) {
			printk("low battery, notify\n");
			chip->pdata->low_batt_cb(chip->soc == 0);
		}

#else
	raw_soc = 100;
	chip->soc = 100;
#endif
	}
}

static void max17042_get_temperature(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	s32 temper = 0;

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_TEMPERATURE, data) < 0)
		return;

	/* data[] store 2's compliment format number */
	if (data[1] & (0x1 << 7)) {
		/* Negative */
		temper = ((~(data[1])) & 0xFF) + 1;
		temper *= (-1000);
	} else {
		temper = data[1] & 0x7F;
		temper *= 1000;
		temper += data[0] * 39 / 10;
	}
#else
	temper = 40 * 1000;
#endif

	if(chip->factory_mode || chip->test_mode)
		temper = 30 * 1000;
	chip->temperature = temper;

	fuelgauge_info(&client->dev, "Temperature = %d\n", chip->temperature);

}

static void max17042_get_version(struct i2c_client *client)
{
	u8 data[2];

#ifndef NO_READ_I2C_FOR_MAXIM
	if (max17042_read_reg(client, MAX17042_REG_VERSION, data) < 0)
		return;

	dev_info(&client->dev, "MAX17042 Fuel-Gauge Ver %d%d\n",
			data[0], data[1]);
#endif
}

static void max17042_work(struct work_struct *work)
{
	struct max17042_chip *chip;

	chip = container_of(work, struct max17042_chip, work.work);

	max17042_get_vcell(chip->client);
	max17042_get_current(chip->client);
	max17042_get_vfocv(chip->client);
	max17042_get_soc(chip->client);
	max17042_get_tte(chip->client);
	max17042_get_ttf(chip->client);

	if (chip->pdata->enable_gauging_temperature)
		max17042_get_temperature(chip->client);

#ifdef LOG_REG_FOR_MAXIM
	{
		int reg;
		int i;
		u8	data[2];
		u8	buf[1024];

		i = 0;
		for (reg = 0; reg < 0x50; reg++) {
			max17042_read_reg(chip->client, reg, data);
			i += sprintf(buf + i, "0x%02x%02xh,", data[1], data[0]);
		}
		printk(KERN_INFO "%s", buf);

		i = 0;
		for (reg = 0xe0; reg < 0x100; reg++) {
			max17042_read_reg(chip->client, reg, data);
			i += sprintf(buf + i, "0x%02x%02xh,", data[1], data[0]);
		}
		printk(KERN_INFO "%s", buf);
	}
#endif

	queue_delayed_work(chip->monitor_wqueue, &chip->work, MAX17042_LONG_DELAY);
}

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

#define SEC_FG_ATTR(_name)			\
{									\
	.attr = {	.name = #_name,		\
				.mode = 0664,},		\
	.show = sec_fg_show_property,	\
	.store = sec_fg_store,			\
}

static struct device_attribute sec_fg_attrs[] = {
	SEC_FG_ATTR(fg_reset_soc),
	SEC_FG_ATTR(fg_read_soc),
	SEC_FG_ATTR(fg_current),
	SEC_FG_ATTR(fg_reset),
	SEC_FG_ATTR(fg_test),
	SEC_FG_ATTR(fg_debug),
	SEC_FG_ATTR(dump_reg),
};

enum {
	FG_RESET_SOC = 0,
	FG_READ_SOC,
	FG_CURRENT,
	FG_RESET,
	FG_TEST,
	FG_DEBUG,
	DUMP_REG,
};

static ssize_t sec_fg_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17042_chip *chip = container_of(psy,
						  struct max17042_chip,
						  battery);

	int i = 0;
	const ptrdiff_t off = attr - sec_fg_attrs;

	switch (off) {
	case FG_READ_SOC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			max17042_read_vfsoc_unchecked(chip->client));
		break;
	case FG_CURRENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			max17042_read_current(chip->client));
		break;
	case FG_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			chip->test_mode);
		break;
	case FG_DEBUG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			debug_mode);
		break;
	case DUMP_REG:
		i += dump_fuelgauge(chip->client, buf);
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t sec_fg_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17042_chip *chip = container_of(psy,
						  struct max17042_chip,
						  battery);

	int x = 0;
	int ret = 0;
	const ptrdiff_t off = attr - sec_fg_attrs;

	switch (off) {
	case FG_RESET_SOC:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 1) {
				cancel_delayed_work_sync(&chip->work);
				max17042_reset_soc(chip->client);
				chip->need_refresh = 1;
				queue_delayed_work(chip->monitor_wqueue, &chip->work, MAX17042_SHORT_DELAY);
			}
			ret = count;
		}
		break;
	case FG_RESET:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 1) {
				cancel_delayed_work_sync(&chip->work);
				max17042_reset_por(chip->client);
				import_battery_model(chip->client);
				max17042_init_regs(chip->client);
				chip->need_refresh = 1;
				queue_delayed_work(chip->monitor_wqueue, &chip->work, MAX17042_SHORT_DELAY);
			}
			ret = count;
		}
		break;
	case FG_TEST:
		if (sscanf(buf, "%d\n", &x) == 1) {
			chip->test_mode = !!x;
			ret = count;
		}
		break;
	case FG_DEBUG:
		if (sscanf(buf, "%d\n", &x) == 1) {
			debug_mode = !!x;
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int fuelgauge_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sec_fg_attrs); i++) {
		rc = device_create_file(dev, &sec_fg_attrs[i]);
		if (rc)
			goto fg_attrs_failed;
	}
	goto succeed;

fg_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_fg_attrs[i]);
succeed:
	return rc;
}

static bool max17042_check_status(struct i2c_client *client)
{
	u8 data[2];
	bool ret = false;

#ifndef NO_READ_I2C_FOR_MAXIM
	/* check if Smn was generated */
	if (max17042_read_reg(client, MAX17042_REG_STATUS, data) < 0)
		return ret;

	dev_info(&client->dev, "%s : status_reg(%02x%02x)\n", __func__, data[1], data[0]);

	/* minimum SOC threshold exceeded. */
	if ( data[1] & (0x1 << 2)) {
		printk("-------------------- soc threshold exceeded---------------------\n");
		ret = true;
	}

	/* clear status reg */
	if (!ret) {
		data[1] = 0;
		max17042_write_reg(client, MAX17042_REG_STATUS, data);
		msleep(200);
	}
#else
	ret = true;
#endif

	return ret;
}

static irqreturn_t max17042_irq_thread(int irq, void *irq_data)
{
	u8 data[2];
	bool max17042_alert_status = false;
	struct max17042_chip *chip = irq_data;

	wake_lock(&chip->fuel_alert_wake_lock);
	msleep(200);
	if (max17042_read_reg(chip->client, MAX17042_REG_CONFIG, data) < 0) {
		dev_err(&chip->client->dev, "i2c may be not ready\n");
		wake_unlock(&chip->fuel_alert_wake_lock);
		return IRQ_HANDLED;
	}

	max17042_get_soc(chip->client);
	max17042_get_vcell(chip->client);

	max17042_alert_status = max17042_check_status(chip->client);

	if (max17042_alert_status) {
		int clear_alert = 1;
		if(chip->vcell < 3500 * 1000) {
			if (!(chip->is_fuel_alerted)) {
				if(chip->pdata->low_batt_cb) {
					if(!chip->pdata->low_batt_cb(true)) {
						//Disable Alert (Aen = 0)
						data[0] &= (~(0x1 << 2));
						chip->is_fuel_alerted = true;
					} else
						clear_alert = 0;
				}
			} else {
				dev_err(&chip->client->dev, "already alerted\n");
			}
		}

		if(clear_alert) {
			//clear the interrupt
			data[1] |= (0x1 << 3);
		}
	} else {
		data[1] &= (~(0x1 << 3));
	}
	max17042_write_reg(chip->client, MAX17042_REG_CONFIG, data);


	max17042_read_reg(chip->client, MAX17042_REG_VCELL, data);
	dev_info(&chip->client->dev, "%s : MAX17042_REG_VCELL(%02x%02x)\n",
		__func__, data[1], data[0]);

	max17042_read_reg(chip->client, MAX17042_REG_TEMPERATURE, data);
	dev_info(&chip->client->dev, "%s : MAX17042_REG_TEMPERATURE(%02x%02x)\n",
		__func__, data[1], data[0]);

	max17042_read_reg(chip->client, MAX17042_REG_CONFIG, data);
	dev_info(&chip->client->dev, "%s : MAX17042_REG_CONFIG(%02x%02x)\n",
		__func__, data[1], data[0]);

	max17042_read_reg(chip->client, MAX17042_REG_VFOCV, data);
	dev_info(&chip->client->dev, "%s : MAX17042_REG_VFOCV(%02x%02x)\n",
		__func__, data[1], data[0]);

	max17042_read_reg(chip->client, MAX17042_REG_SOC_VF, data);
	dev_info(&chip->client->dev, "%s : MAX17042_REG_SOC_VF(%02x%02x)\n",
		__func__, data[1], data[0]);

	wake_unlock(&chip->fuel_alert_wake_lock);
	return IRQ_HANDLED;
}

static int max17042_irq_init(struct max17042_chip *chip)
{
	int ret;

	chip->is_fuel_alerted = false;

	/* Request irq */
	if (chip->pdata->alert_irq) {
		ret = request_threaded_irq(chip->pdata->alert_irq, NULL,
				max17042_irq_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "max17042 fuel alert", chip);
		if (ret) {
			dev_err(&chip->client->dev, "failed to reqeust IRQ\n");
			return ret;
		}

		ret = enable_irq_wake(chip->pdata->alert_irq);
		if (ret < 0)
			dev_err(&chip->client->dev, "failed to enable wakeup src %d\n", ret);
	}
	return 0;
}

static int max17042_set_reg(struct i2c_client *client, int reg, u16 value, int retry)
{
	int ret;
	u16 data;
	int retries = 0;

	while(1) {

		ret = max17042_write_reg(client, reg, (u8 *)&value);
		if(retry == 0)
			return ret < 0 ? 1 : 0;

		ret = max17042_read_reg(client, reg, (u8 *)&data);

		retries++;

		if(ret < 0) {
			if(retry > 5)
				return 1;
			else 
				continue;
		}
		if(data == value)
			return 0;
	}
	return 1;
}

static int max17042_write_model(struct i2c_client *client)
{
	static const u16 config_data[] = {
		0x7E00, 0xB580, 0xB830, 0xB900, 0xBB10,
		0xBC20, 0xBC80, 0xBCE0, 0xBDB0, 0xBE90,
		0xC060, 0xC210, 0xC400, 0xC740, 0xCBE0,
		0xCFF0, 0x0080, 0x0900, 0x1900, 0x0B00,
		0x1300, 0x2E00, 0x2E00, 0x2A10, 0x1CA0,
		0x0BE0, 0x0DE0, 0x0AD0, 0x0CD0, 0x08F0,
		0x09D0, 0x09D0, 0x0100, 0x0100, 0x0100,
		0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
		0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 
		0x0100, 0x0100, 0x0100,
	};
	static const u8 reg[] = { 0x80, 0x90, 0xA0};
	u16 data[16];
	int i;
	char *buf;
	for(i = 0; i < 3; i ++) {
		int retries = 0;
		while(retries < 5) {
			buf = (char *)(config_data + i * 16);

			i2c_smbus_write_i2c_block_data(client, reg[i], 32, buf);
			i2c_smbus_read_i2c_block_data(client, reg[i], 32, (char *)data);
			if(memcmp(buf, data, 32)){
				printk("%s LINE=%d, error\n", __FUNCTION__, __LINE__);
				retries++;
				continue;
			} else 
				break;
		}
		if(retries >= 5)
			return 1;
	}
	return 0;
}

static int max17042_write_param(struct i2c_client *client)
{
	if(max17042_set_reg(client, 0x38, 0x0082, 1) < 0)
		return 1;
	if(max17042_set_reg(client, 0x39, 0x3074, 1) < 0)
		return 1;
	return 0;
}

static void check_fuelgauge(struct i2c_client *client)
{
	int vcell = max17042_read_vcell(client);
	if(vcell < 3200 * 1000) {
		printk("vcell(%d) too lowneed to reset the fuelgauge\n", vcell);
		max17042_reset_por(client);
	}
}

static int import_battery_model(struct i2c_client *client)
{
	u16 data;

	//T2:
	if(max17042_set_reg(client, MAX17042_REG_CONFIG, 0x2210, 0) < 0)
		return -2;
	printk("%s T2 ok\n", __FUNCTION__);

	//T4:
	if(max17042_set_reg(client, 0x62, 0x0059, 1) < 0)
		return -4;
	if(max17042_set_reg(client, 0x63, 0x00C4, 1) < 0)
		return -4;
	printk("%s T4 ok\n", __FUNCTION__);

	//T5:
	if(max17042_write_model(client))
		return -5;
	printk("%s T5 ok\n", __FUNCTION__);

	//T8:
	if(max17042_set_reg(client, 0x62, 0x0000, 1) < 0)
		return -8;
	if(max17042_set_reg(client, 0x63, 0x0000, 1) < 0)
		return -8;
	printk("%s T8 ok\n", __FUNCTION__);

	//T10:
	if(max17042_write_param(client))
		return -10;
	printk("%s T10 ok\n", __FUNCTION__);

	//T11:
	msleep(350);
	if(max17042_set_reg(client, 0x10, 0x0BB8, 1) < 0)
		return -11;
	printk("%s T11 ok\n", __FUNCTION__);

	//T12:
	if(max17042_read_reg(client, MAX17042_REG_STATUS, (u8 *)&data) < 0)
		return -12;
	data &= 0xFFFD;
	if(max17042_set_reg(client, MAX17042_REG_STATUS, data, 1) < 0)
		return -12;
	msleep(350);
	printk("%s T12 ok\n", __FUNCTION__);

	return 0;
}

static int max17042_reset_por(struct i2c_client *client)
{
	int POR;
	//Lock the model
	if(max17042_set_reg(client, 0x62, 0x0000, 1) < 0)
		return 1;
	if(max17042_set_reg(client, 0x63, 0x0000, 1) < 0)
		return 1;
	if(max17042_set_reg(client, 0x00, 0x0000, 1) < 0)
		return 1;

	do {
		u8 data[2];
		//send softPOR
		if(max17042_set_reg(client, 0x60, 0x000F, 0) < 0) {
			printk("send sofPOR error\n");
			continue;
		}
		msleep(2);

		if (max17042_read_reg(client, MAX17042_REG_STATUS, data) < 0) {
			printk("get fuelgauge POR error\n");
			continue;
		}
		POR = data[0] & (0x1 << 1);
	} while(POR == 0);
	msleep(100);

	printk("por end\n");
	return 0;
}

static int init_fuelgauge(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	int POR;

	if(max17042_read_reg(client, MAX17042_REG_STATUS, data) < 0)
		return -1;
	POR = data[0] & (0x1 << 1);
	if(POR) {
		printk("fuelgauge POR\n");
		import_battery_model(client);
	} else {
		max17042_read_reg(client, MAX17042_REG_VFOCV, data);
		max17042_reset_por(client);
		import_battery_model(client);
		max17042_write_reg(client, MAX17042_REG_VFOCV, data);
		msleep(200);
	}
	chip->loose_reset_start = get_jiffies_64();
	return 0;
}

static int get_soc_quick(struct i2c_client *client)
{
	int soc = max17042_read_vfsoc_unchecked(client);
	fuelgauge_info(&client->dev, "raw soc %d\n", soc);
	soc = min(soc / 100, 100);
	fuelgauge_loose_check(client, soc);
	soc = (soc <= 5) ? 0 : soc;
	return soc;
}

static int get_temp_quick(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);
	max17042_get_temperature(client);
	return chip->temperature;
}
static void __devinit async_max17042_probe(void *async_data, async_cookie_t cookie)
{
	struct i2c_client *client = (struct i2c_client *)async_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	int i;
	struct max17042_reg_data *data;
	int ret;
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return;

	if(!mx_is_factory_test_mode(MX_FACTORY_TEST_ALL))
		chip->factory_mode = 0;
	else {
		printk("factory mode\n");
		chip->factory_mode = 1;
	}

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "max17042-fuelgauge";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.set_property	= NULL;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);

	chip->low_vcell_count = 0;
	chip->need_refresh = 1;

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return;
	}

	check_fuelgauge(client);
	if(init_fuelgauge(client))
		printk("init fuelgauge error\n");

	/* initialize fuel gauge registers */
	max17042_init_regs(client);

#ifndef NO_READ_I2C_FOR_MAXIM
	/* register low batt intr */
	chip->pdata->alert_irq = gpio_to_irq(chip->pdata->alert_gpio);

	wake_lock_init(&chip->fuel_alert_wake_lock, WAKE_LOCK_SUSPEND, "fuel_alerted");
	wake_lock_init(&chip->fuel_reset_wake_lock, WAKE_LOCK_SUSPEND, "fuel_reset");

	data = chip->pdata->alert_init;
	for (i = 0; i < chip->pdata->alert_init_count; i += 1)
		if(data[i].reg_addr == MAX17042_REG_SALRT_TH)
			chip->fuel_alert_soc = data[i].reg_data1;

	dev_info(&client->dev, "fuel alert soc (%d)\n", chip->fuel_alert_soc);
	ret = max17042_irq_init(chip);
	if (ret)
		goto err_kfree;

#endif

	max17042_get_version(client);

	// get some the battery info first
	{
		int soc = max17042_read_vfsoc_unchecked(chip->client);
		soc = min(soc / 100, 100);
		chip->soc = (soc <= 5) ? 0 : soc;
		max17042_get_temperature(chip->client);
	}

	/* create fuelgauge attributes */
	fuelgauge_create_attrs(chip->battery.dev);

	debug_mode = 0;

	chip->monitor_wqueue =
	    create_singlethread_workqueue("fuelgauge_wq");
	if (!chip->monitor_wqueue) {
		dev_err(&client->dev, "%s: fail to create workqueue\n", __func__);
		goto err_kfree;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, max17042_work);
	queue_delayed_work(chip->monitor_wqueue, &chip->work, msecs_to_jiffies(1000));

	return;

err_kfree:
	wake_lock_destroy(&chip->fuel_alert_wake_lock);
	wake_lock_destroy(&chip->fuel_reset_wake_lock);
	kfree(chip);
	return;
}

static int __devinit max17042_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	async_schedule(async_max17042_probe, client);
	return 0;
}
static int __devexit max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	flush_workqueue(chip->monitor_wqueue);
	cancel_delayed_work_sync(&chip->work);
	destroy_workqueue(chip->monitor_wqueue);

	power_supply_unregister(&chip->battery);
	wake_lock_destroy(&chip->fuel_alert_wake_lock);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM

static int max17042_suspend(struct i2c_client *client, pm_message_t state)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->work);
	return 0;
}

static int max17042_resume(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	chip->need_refresh = 1;
	queue_delayed_work(chip->monitor_wqueue, &chip->work, MAX17042_LONG_DELAY);
	return 0;
}

#else

#define max17042_suspend NULL
#define max17042_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id max17042_id[] = {
	{"max17042", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= "max17042",
	},
	.probe		= max17042_probe,
	.remove		= __devexit_p(max17042_remove),
	.suspend	= max17042_suspend,
	.resume		= max17042_resume,
	.id_table	= max17042_id,
};

static int __init max17042_init(void)
{
	return i2c_add_driver(&max17042_i2c_driver);
}

module_init(max17042_init);

static void __exit max17042_exit(void)
{
	i2c_del_driver(&max17042_i2c_driver);
}

module_exit(max17042_exit);

MODULE_AUTHOR("<ms925.kim@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");

