/*
 * Fuel gauge driver for Ti BQ27541
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
 * Author:  Lvcha qiu <lvcha@meizu.com> Chwei <Chwei@meizu.com>
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
 * This driver is based on max17046_battery.c
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/suspend.h>
#include <linux/bq27541-private.h>
#include <linux/bq27541.h>

#define LOG

#ifdef LOG
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>

#define LOG_SIZE 600
#define LOG_LINE_SIZE 70

struct battery_log {
	unsigned long time;
	int vol;
	int cur;
	int temp;
	int cap;
	int full_charge_cap;
	long flags;
	long control_state;
};

static int proc_read = -1;
static int log_index;
static struct battery_log *battery_log_head;
#endif

struct bat_info {
#define LOW_VOLTAGE_VAL		3350000
#define NORMAL_VOLTAGE_VAL	3400000
	int voltage_now;
	int current_now;
	int status;
	int temp;
	int health;
	int capacity;
#define HIGH_BLOCK_TEMP		(450)
#define LOW_BLOCK_TEMP		(0)
#define CHECK_CNT		(2)
	int abnormal_temp_cnt;
	int recover_temp_cnt;
	bool batt_is_low;
};

struct bq27541_chip {
#define REFRESH_POLL		(60 * HZ)
	struct i2c_client *client;
	struct power_supply fuelgauge;
	struct delayed_work battery_dwork;
	struct bat_info		bat_info;
	int wakeup;
	int low_bat_gpio;
	int low_bat_irq;
	int debug;
};

#define bq27541_debug(chip, fmt, arg...) \
do { \
	if (chip->debug) \
		pr_info(fmt, ##arg); \
} while(0)

#ifdef LOG
static int fg_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int index;

	*start = page;

	if(proc_read < 0) {
		if(off == 0)
			proc_read = (log_index + LOG_SIZE - 1) % LOG_SIZE;
		else
			return 0;
	} 

	index = proc_read % LOG_SIZE;
	len = snprintf(page, count, "%lu %d %d %d %d %d 0x%lx 0x%lx\n", 
			battery_log_head[index].time, battery_log_head[index].vol, 
			battery_log_head[index].cur, battery_log_head[index].temp, battery_log_head[index].cap,
			battery_log_head[index].full_charge_cap, battery_log_head[index].flags, 
			battery_log_head[index].control_state);
	proc_read = (proc_read + LOG_SIZE - 1) % LOG_SIZE;

	if(proc_read == log_index) {
		*eof = 1;
		proc_read = -1;
	 } else
		*eof = 0;

	return len;
}
#endif

static ssize_t ba27541_debug(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct bq27541_chip *chip = dev_get_drvdata(dev->parent);
	unsigned long value = simple_strtoul(buf, NULL, 0);

	if(value == 1 || value == 0) {
		chip->debug = value;
	} else {
		int ret;

		pr_info("fuelgauge reg 0x%02x read\n", (unsigned short)value);
		ret = i2c_smbus_read_word_data(chip->client, value);
		if (ret >= 0) {
			pr_info("value is 0x%04x\n", ret);
		} else {
			pr_info("value read error(%d)\n", ret);
		}
	}

	return count;
}

static DEVICE_ATTR(debug, S_IWUSR | S_IRUGO, NULL, ba27541_debug);

static enum power_supply_property bq27541_battery_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CHARGE_TYPE
};

static int bq27541_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct bq27541_chip *chip = container_of(psy, struct bq27541_chip, fuelgauge);
	int ret;
	short data;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->bat_info.voltage_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = chip->bat_info.current_now;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->bat_info.status;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chip->bat_info.temp;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->bat_info.health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->bat_info.capacity;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_AI_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_CURRENT_AVG = %d\n", (s16)data*1000);
			val->intval = data*1000;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_CC_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_CYCLE_COUNT = %d\n", data);
			val->intval = data;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_FCC_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_CHARGE_FULL = %d\n", data*1000);
			val->intval = data * 1000;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_TTE_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW = %d\n", data*60);
			val->intval = data*60;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_TTECP_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG = %d\n", ret*60);
			val->intval = ret*60;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_TTF_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_TIME_TO_FULL_NOW = %d\n", data*60);
			val->intval = data*60;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_NAC_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_CHARGE_NOW = %d\n", data);
			val->intval = data;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_DCAP_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN = %d\n", data*1000);
			val->intval = data * 1000;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_AE_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_ENERGY_NOW = %d\n", data*1000);
			val->intval = data * 1000;
		} else {
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_FLAGS_LSB);
		if (ret >= 0) {
			data = (short)ret;
			bq27541_debug(chip, "POWER_SUPPLY_PROP_CHARGE_TYPE:0x%x\n", data);
			if (data & 0x100) {
				val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			} else {
				val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			}
		} else {
			return -EINVAL;
		}
		break;
	default:
		bq27541_debug(chip, "invalid psp = %d\n", psp);
		return -EINVAL;
	}
	return 0;
}

static irqreturn_t lowbat_irq_thread(int irq, void *dev_id)
{
	struct bq27541_chip *chip = dev_id;
	pr_info("------- %s\n", __func__);

	if (!gpio_get_value(chip->low_bat_gpio)) {
		if(delayed_work_pending(&chip->battery_dwork))
			cancel_delayed_work(&chip->battery_dwork);
		schedule_delayed_work(&chip->battery_dwork, HZ/2);
	}
	return IRQ_HANDLED;
}

static void check_battery_temp(struct bq27541_chip *chip)
{
	int temp;
	struct bat_info *info = &chip->bat_info;
	int health = info->health;
	int ret;

	ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_TEMP_LSB);
	if(ret < 0) {
		pr_info("get bat_info POWER_SUPPLY_PROP_TEMP on bq27541CMD_TEMP_LSB error\n");
		return;
	}

	temp = ((int)(short)ret) - 2731;
	bq27541_debug(chip, "battery temp %d\n", temp);
	chip->bat_info.temp = temp;

	if(temp > HIGH_BLOCK_TEMP || temp < LOW_BLOCK_TEMP) {
		if(info->health == POWER_SUPPLY_HEALTH_GOOD)
			if(info->abnormal_temp_cnt < CHECK_CNT)
				info->abnormal_temp_cnt += 1;
		pr_info("battery temp abnormal, temp %d count %d\n", temp, info->abnormal_temp_cnt);
	} else {
		if(info->health != POWER_SUPPLY_HEALTH_GOOD) {
			pr_info("battery temp return normal, temp %d count %d\n", temp, info->recover_temp_cnt);
			if(info->recover_temp_cnt < CHECK_CNT)
				info->recover_temp_cnt += 1;
		}
	}

	if(info->abnormal_temp_cnt >= CHECK_CNT) {
		health = (temp > HIGH_BLOCK_TEMP) ? POWER_SUPPLY_HEALTH_OVERHEAT: POWER_SUPPLY_HEALTH_COLD;
		info->abnormal_temp_cnt = 0;
		info->recover_temp_cnt = 0;
	} else if(info->recover_temp_cnt >= CHECK_CNT) {
		health = POWER_SUPPLY_HEALTH_GOOD;
		info->abnormal_temp_cnt = 0;
		info->recover_temp_cnt = 0;
	}
	info->health = health;
}

static void get_battery_info(struct bq27541_chip *chip)
{
	int ret;
	short data;

	ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_VOLT_LSB);
	if (ret >= 0) {
		data = (short)ret;
		bq27541_debug(chip, "POWER_SUPPLY_PROP_VOLTAGE_NOW = %dmV\n", data);
		chip->bat_info.voltage_now = data*1000;
		if(chip->bat_info.voltage_now >= NORMAL_VOLTAGE_VAL) {
			chip->bat_info.batt_is_low = false;
		} else if(chip->bat_info.voltage_now <= LOW_VOLTAGE_VAL) {
			pr_info("======voltage too low\n");
			chip->bat_info.batt_is_low = true;
		}
	} else {
		pr_info("get bat_info POWER_SUPPLY_PROP_VOLTAGE_NOW on bq27541CMD_VOLT_LSB error\n");
	}

	ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_AI_LSB);
	if (ret >= 0) {
		data = (short)ret;
		bq27541_debug(chip, "POWER_SUPPLY_PROP_CURRENT_NOW = %d\n", (s16)data*1000);
		chip->bat_info.current_now = data*1000;
	} else {
		pr_info("get bat_info POWER_SUPPLY_PROP_CURRENT_NOW on bq27541CMD_AI_LSB error\n");
	}

	check_battery_temp(chip);
	ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_FLAGS_LSB);
	if (ret >= 0) {
		data = (short)ret;

		if (data & 0x200) {
			bq27541_debug(chip, "POWER_SUPPLY_STATUS_FULL\n");
			chip->bat_info.status = POWER_SUPPLY_STATUS_FULL;
		} else if (data & 0x1) {
			bq27541_debug(chip, "POWER_SUPPLY_STATUS_DISCHARGING\n");
			chip->bat_info.status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			bq27541_debug(chip, "POWER_SUPPLY_STATUS_CHARGING\n");
			chip->bat_info.status = POWER_SUPPLY_STATUS_CHARGING;
		}

		if (data & 0xc000) {
			bq27541_debug(chip, "POWER_SUPPLY_HEALTH_OVERHEAT\n");
			chip->bat_info.health = POWER_SUPPLY_HEALTH_OVERHEAT;
		}
	} else {
		pr_info("get bat_info POWER_SUPPLY_PROP_STATUS and POWER_SUPPLY_PROP_HEALTH on bq27541CMD_FLAGS_LSB error\n");
	}

	ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_SOC_LSB);
	if (ret >= 0) {
		data = (short)ret;
		bq27541_debug(chip, "POWER_SUPPLY_PROP_CAPACITY = %d\n", data);
		if (chip->bat_info.batt_is_low)
			chip->bat_info.capacity = 0;
		else {
			chip->bat_info.capacity = data;
		}
	} else {
		pr_info("get bat_info POWER_SUPPLY_PROP_CAPACITY on bq27541CMD_SOC_LSB error\n");
	}

#ifdef LOG
	do {
		int ret;
		short data;

		battery_log_head[log_index].time = current_kernel_time().tv_sec;
		battery_log_head[log_index].vol = chip->bat_info.voltage_now;
		battery_log_head[log_index].cur = chip->bat_info.current_now;
		battery_log_head[log_index].temp = chip->bat_info.temp;

		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_SOC_LSB);
		if (ret >= 0) {
			data = (short)ret;
			battery_log_head[log_index].cap = data;
		} else {
			battery_log_head[log_index].cap = -1;
		}

		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_FCC_LSB);
		if (ret >= 0) {
			data = (short)ret;
			battery_log_head[log_index].full_charge_cap = data;
		} else {
			battery_log_head[log_index].full_charge_cap = -1;
		}

		ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_FLAGS_LSB);
		if (ret >= 0) {
			data = (short)ret;
			battery_log_head[log_index].flags = data;
		} else {
			battery_log_head[log_index].flags = -1;
		}

		ret = i2c_smbus_write_word_data(chip->client, bq27541CMD_CNTL_LSB, 0x0000);
		if (!ret) {
			msleep(10);
			ret = i2c_smbus_read_word_data(chip->client, bq27541CMD_CNTL_LSB);
			if (ret >= 0) {
				data = (short)ret;
				battery_log_head[log_index].control_state = data;
			} else {
				battery_log_head[log_index].control_state = -1;
			}
		}

		log_index = (log_index + 1) % LOG_SIZE;
	} while(0);
#endif
}

static void battery_work(struct work_struct *work)
{
	struct bq27541_chip *chip = container_of(work, struct bq27541_chip, battery_dwork.work);

	get_battery_info(chip);
	power_supply_changed(&chip->fuelgauge);

	if(!delayed_work_pending(&chip->battery_dwork))
		schedule_delayed_work(&chip->battery_dwork, REFRESH_POLL);
}

static int __devinit bq27541_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq27541_platform_data *pdata = client->dev.platform_data;
	struct bq27541_chip *chip;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->bat_info.health = POWER_SUPPLY_HEALTH_GOOD;
	chip->debug = 0;

	chip->client = client;
	chip->low_bat_gpio = pdata->low_bat_gpio;
	chip->wakeup = pdata->wakeup;

	i2c_set_clientdata(client, chip);

	chip->fuelgauge.name		= "fuelgauge";
	chip->fuelgauge.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->fuelgauge.get_property= bq27541_get_property;
	chip->fuelgauge.properties	= bq27541_battery_props;
	chip->fuelgauge.num_properties	= ARRAY_SIZE(bq27541_battery_props);

	ret = power_supply_register(&client->dev, &chip->fuelgauge);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto err_free;
	}

	/* initialize fuel gauge registers */
	ret = i2c_smbus_write_word_data(client, bq27541CMD_AR_LSB,
						AVERAGE_DISCHARGE_CURRENT_MA);
	if (ret) {
		dev_err(&client->dev, "failed: initial fuel gauge\n");
		goto err_init;
	}

	INIT_DELAYED_WORK(&chip->battery_dwork, battery_work);

	chip->low_bat_irq = ret = gpio_to_irq(chip->low_bat_gpio);
	if (ret < 0) {
		dev_err(&client->dev, "failed: get irq number\n");
		goto err_init;
	}

	ret = request_threaded_irq(chip->low_bat_irq, NULL, lowbat_irq_thread, 
					IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
					"low_bat", chip);
	if (ret < 0) {
		dev_err(&client->dev, "failed: request irq%d\n", chip->low_bat_irq);
		goto err_init;
	}

	ret = device_create_file(chip->fuelgauge.dev, &dev_attr_debug);
	if (ret) {
		dev_err(&client->dev, "failed: device_create_file\n");
		goto err_irq;
	}

#ifdef LOG
	do {
		create_proc_read_entry("fuelgauge_info", 0, NULL, fg_proc_read, NULL);

		battery_log_head = kzalloc(sizeof(struct battery_log) * LOG_SIZE, GFP_KERNEL);
		if(battery_log_head == NULL)
			goto err_irq;
		log_index = 0;
	} while(0);
#endif

	get_battery_info(chip);
	schedule_delayed_work(&chip->battery_dwork, REFRESH_POLL);

	ret = device_init_wakeup(&client->dev, chip->wakeup);
	if (ret < 0) {
		dev_err(&client->dev, "failed: device_init_wakeup\n");
		goto err_irq;
	}

	return 0;

err_irq:
	free_irq(chip->low_bat_irq, chip);
err_init:
	power_supply_unregister(&chip->fuelgauge);
err_free:
	kfree(chip);
	return ret;
}

static int __devexit bq27541_remove(struct i2c_client *client)
{
	struct bq27541_chip *chip = i2c_get_clientdata(client);

	free_irq(chip->low_bat_irq, chip);
	power_supply_unregister(&chip->fuelgauge);
	i2c_set_clientdata(chip->client, NULL);
	kfree(chip);
	return 0;
}

static const struct i2c_device_id bq27541_id[] = {
	{ "bq27541", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17046_id);

#ifdef CONFIG_PM
static int bq27541_suspend(struct device *dev)
{
	struct bq27541_chip *chip = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&chip->battery_dwork);
	enable_irq_wake(chip->low_bat_irq);

	return 0;
}

static int bq27541_resume(struct device *dev)
{
	struct bq27541_chip *chip = dev_get_drvdata(dev);

	disable_irq_wake(chip->low_bat_irq);
	get_battery_info(chip);
	schedule_delayed_work(&chip->battery_dwork, REFRESH_POLL);
	return 0;
}

static const struct dev_pm_ops bq27541_pm_ops = {
	.suspend		= bq27541_suspend,
	.resume		= bq27541_resume,
};
#else
#define bq27541_pm_ops NULL
#endif

static struct i2c_driver bq27541_i2c_driver = {
	.driver	= {
		.name  = "bq27541",
		.owner = THIS_MODULE,
		.pm = &bq27541_pm_ops,
	},
	.probe	= bq27541_probe,
	.remove	= __devexit_p(bq27541_remove),
	.id_table	= bq27541_id,
};

static int __init bq27541_init(void)
{
	return i2c_add_driver(&bq27541_i2c_driver);
}
module_init(bq27541_init);

static void __exit bq27541_exit(void)
{
	i2c_del_driver(&bq27541_i2c_driver);
}
module_exit(bq27541_exit);

MODULE_DESCRIPTION("Battery driver for TI BQ27541");
MODULE_AUTHOR("Lvcha qiu <lvcha@meizu.com>;Chwei <Chwei@meizu.com>");
MODULE_LICENSE("GPLV2");
