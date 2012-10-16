/*
 * Battery driver for Maxim MAX77665
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  Lvcha qiu <lvcha@meizu.com> Chwei <Chwei@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/mfd/max77665.h>
#include <linux/mfd/max77665-private.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/android_alarm.h>
#include <mach/usb-detect.h>

#define MAX_USB_CURRENT 460
#define MAX_AC_CURRENT 1000
#define TEMP_CHECK_DELAY (120 * HZ)
#define WAKE_ALARM_INT (120)

struct max77665_charger
{
	struct device *dev;
	struct power_supply psy_usb;
	struct power_supply psy_ac;
	struct power_supply psy_charger;
	struct max77665_dev *iodev;
	struct wake_lock wake_lock;
	struct delayed_work dwork;
	struct delayed_work poll_dwork;
	struct regulator *ps;
	struct mutex mutex_t;
	struct notifier_block usb_host_notifier;

	enum cable_status_t {
		CABLE_TYPE_NONE = 0,
		CABLE_TYPE_USB,
		CABLE_TYPE_AC,
		CABLE_TYPE_UNKNOW,
	} cable_status;

	enum chg_status_t {
		CHG_STATUS_FAST,
		CHG_STATUS_DONE,
		CHG_STATUS_RECHG,
	} chg_status;

	bool chgin;
	int chgin_irq;
	int chr_pin;
	int chgin_ilim_usb;	/* 60mA ~ 500mA */
	int chgin_ilim_ac;	/* 60mA ~ 2.58A */
	int (*usb_attach) (bool);

	struct alarm		alarm;
	bool usb_host_insert;
};

static BLOCKING_NOTIFIER_HEAD(max77665_charger_chain_head);

int register_max77665_charger_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&max77665_charger_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_max77665_charger_notifier);

int unregister_max77665_charger_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&max77665_charger_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_max77665_charger_notifier);

static int max77665_charger_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&max77665_charger_chain_head, val, NULL)
			== NOTIFY_BAD) ? -EINVAL : 0;
}

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property max77665_power_props[] =
{
	POWER_SUPPLY_PROP_ONLINE,
};

static int max77665_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77665_charger *charger =
		container_of(psy, struct max77665_charger, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (charger->cable_status == CABLE_TYPE_USB);
	
	return 0;
}

static int max77665_ac_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77665_charger *charger =
		container_of(psy, struct max77665_charger, psy_ac);
	
	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (charger->cable_status == CABLE_TYPE_AC);
	
	return 0;
}

static enum power_supply_property max77665_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
};

static int max77665_charger_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct max77665_charger *charger =
		container_of(psy, struct max77665_charger, psy_charger);
	
	if (psp != POWER_SUPPLY_PROP_STATUS)
		return -EINVAL;

	if(charger->cable_status == CABLE_TYPE_USB || 
		charger->cable_status == CABLE_TYPE_AC) {
		if(charger->chg_status == CHG_STATUS_FAST)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_FULL;
	} else {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	
	return 0;
}

static void set_alarm(struct max77665_charger *chg, int seconds)
{
	ktime_t interval = ktime_set(seconds, 0);
	ktime_t now = alarm_get_elapsed_realtime();
	ktime_t next = ktime_add(now, interval);

	pr_info("set alarm after %d seconds\n", seconds);
	alarm_start_range(&chg->alarm, next, next);
}

static void charger_bat_alarm(struct alarm *alarm)
{
	struct max77665_charger *chg = container_of(alarm, struct max77665_charger, alarm);

	wake_lock_timeout(&chg->wake_lock, 3 * HZ);
	set_alarm(chg, WAKE_ALARM_INT);
}

static int max77665_charger_types(struct max77665_charger *charger)
{
#define MA_TO_UA 1000
	enum cable_status_t cable_status = charger->cable_status;
	int chgin_ilim = 0;
	int ret;

	switch (cable_status) {
	case CABLE_TYPE_USB:	//USB input current 500mA
		chgin_ilim = charger->chgin_ilim_usb *MA_TO_UA;
		break;
	case CABLE_TYPE_AC:	//AC input current 1200mA
		chgin_ilim = charger->chgin_ilim_ac * MA_TO_UA;
		break;
	default:
		chgin_ilim = 0;
		break;
	}

	if (chgin_ilim) {
		/* set ilim cur */
		ret = regulator_set_current_limit(charger->ps, chgin_ilim, MAX_AC_CURRENT*MA_TO_UA);
		if (ret) {
			pr_err("failed to set current limit\n");
			return ret;
		}

	}

	if(delayed_work_pending(&charger->poll_dwork))
		cancel_delayed_work(&charger->poll_dwork);
	schedule_delayed_work_on(0, &charger->poll_dwork, 0);

	return 0;
}

static void max77665_work_func(struct work_struct *work)
{
	struct max77665_charger *charger =
		container_of(work, struct max77665_charger, dwork.work);
	enum cable_status_t cable_status = CABLE_TYPE_NONE;

	mutex_lock(&charger->mutex_t);

	if (charger->chgin) {
		if(mx_is_usb_dock_insert()) {
			pr_info("found dock inserted, treat it as AC\n");
			cable_status = CABLE_TYPE_AC;
		} else {
			if (charger->usb_attach && !charger->usb_attach(true)) {
				msleep(2000);

				if (gpio_get_value(charger->chr_pin)) {
					cable_status = CABLE_TYPE_AC;
				} else {
					cable_status = CABLE_TYPE_USB;
				}
			}
		}
	} else {
		charger->cable_status = CABLE_TYPE_NONE;
	}

	charger->cable_status = cable_status;
	max77665_charger_types(charger);
	power_supply_changed(&charger->psy_ac);
	power_supply_changed(&charger->psy_usb);

	if (cable_status != CABLE_TYPE_USB) {
		msleep(500);
		if (charger->usb_attach)
			charger->usb_attach(false);
		max77665_charger_notifier_call_chain(0);
	} else
		max77665_charger_notifier_call_chain(1);

	wake_unlock(&charger->wake_lock);

	mutex_unlock(&charger->mutex_t);
}

static void max77665_poll_work_func(struct work_struct *work)
{
	struct max77665_charger *charger =
		container_of(work, struct max77665_charger, poll_dwork.work);
	struct power_supply *fuelgauge_ps
		= power_supply_get_by_name("fuelgauge");
	union power_supply_propval val;
	int battery_health = POWER_SUPPLY_HEALTH_GOOD;


	mutex_lock(&charger->mutex_t);
	
	if (fuelgauge_ps)
		if(fuelgauge_ps->get_property(fuelgauge_ps, POWER_SUPPLY_PROP_HEALTH, &val) == 0)
			battery_health = val.intval;

	if (charger->chg_status == CHG_STATUS_FAST ||
			charger->chg_status == CHG_STATUS_RECHG) {
		struct i2c_client *i2c = charger->iodev->i2c;
		u8 reg_data;

		if(max77665_read_reg(i2c, MAX77665_CHG_REG_CHG_DETAILS_01, &reg_data) >= 0) {
			if((reg_data & 0x0F) == 0x04) {
				charger->chg_status = CHG_STATUS_DONE;
			}
		}
	}

	if ( charger->cable_status == CABLE_TYPE_USB ||
			charger->cable_status == CABLE_TYPE_AC) {

		if(battery_health != POWER_SUPPLY_HEALTH_GOOD) {
			if (regulator_is_enabled(charger->ps)) {
				printk("----------battery unhealthy, disable charging\n");
				regulator_disable(charger->ps);
			}
		} else {
			if (regulator_is_enabled(charger->ps)) {
				if (charger->chg_status == CHG_STATUS_DONE && fuelgauge_ps) {
					int soc = 100;
					if(fuelgauge_ps->get_property(fuelgauge_ps, POWER_SUPPLY_PROP_CAPACITY, &val) == 0)
						soc = val.intval;
					if(soc <= 98) {
						regulator_disable(charger->ps);
						msleep(500);
						regulator_enable(charger->ps);
						charger->chg_status = CHG_STATUS_RECHG;
					}
				}
			} else {
				printk("----------battery healthy good, enable charging\n");
				regulator_enable(charger->ps);
			}
		}

		schedule_delayed_work_on(0, &charger->poll_dwork, TEMP_CHECK_DELAY);
	} else {
		if (regulator_is_enabled(charger->ps)) {
			printk("--------------charger remove, disable charging\n");
			regulator_disable(charger->ps);
		}
	}
	mutex_unlock(&charger->mutex_t);
}

static irqreturn_t max77665_charger_isr(int irq, void *dev_id)
{
	struct max77665_charger *charger = dev_id;
	struct i2c_client *i2c = charger->iodev->i2c;
	u8 reg_data;
	int ret;
	int chgin = 0;

	wake_lock(&charger->wake_lock);

	ret = max77665_read_reg(i2c, MAX77665_CHG_REG_CHG_INT_OK, &reg_data);
	if (unlikely(ret < 0)) {
		pr_err("Failed to read MAX77665_CHG_REG_CHG_INT: %d\n", ret);
		wake_unlock(&charger->wake_lock);
		return IRQ_HANDLED;
	} else 
		chgin = !!(reg_data & 0x40);

	pr_info("-----%s %s\n", __func__, chgin ? "insert" : "remove");
	if(charger->usb_host_insert) {
		pr_info("usb host insert, dismiss this isr\n");
		wake_unlock(&charger->wake_lock);
		return IRQ_HANDLED;
	}

	if(charger->chgin != chgin) {
		alarm_cancel(&charger->alarm);
		if(chgin)
			set_alarm(charger, WAKE_ALARM_INT);

		if (delayed_work_pending(&charger->dwork))
			cancel_delayed_work(&charger->dwork);

		charger->chgin = chgin;
		charger->chg_status = CHG_STATUS_FAST;
		schedule_delayed_work_on(0, &charger->dwork, msecs_to_jiffies(150));
	} else {
		pr_info("unstable charger isr, dismiss this isr\n");
		msleep(2000);
		wake_unlock(&charger->wake_lock);
	}

	return IRQ_HANDLED;
}

static __devinit int max77665_init(struct max77665_charger *charger)
{
	struct max77665_dev *iodev = dev_get_drvdata(charger->dev->parent);
	struct max77665_platform_data *pdata = dev_get_platdata(iodev->dev);	
	struct i2c_client *i2c = iodev->i2c;
	int ret = EINVAL;
	u8 reg_data = 0;

	/* Unlock protected registers */
	ret = max77665_write_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_06, 0x0C);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX8957_REG_CHG_CNFG_06: %d\n", ret);
		goto error;
	}

	reg_data = max((u8)MAX77665_FCHGTIME_4H, (u8)pdata->fast_charge_timer);
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_01, reg_data, 0x7<<0);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_01: %d\n", ret);
		goto error;
	}

	reg_data = max((u8)MAX77665_CHG_RSTRT_100MV, (u8)pdata->charging_restart_thresold);
	reg_data <<= 4;
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_01, reg_data, 0x3<<4);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_01: %d\n", ret);
		goto error;
	}

	reg_data = 1<<7;
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_01, reg_data, 0x1<<7);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_01: %d\n", ret);
		goto error;
	}

	reg_data = (min(MAX_AC_CURRENT, pdata->fast_charge_current) /CHG_CC_STEP);
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_02, reg_data, 0x3f<<0);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_02: %d\n", ret);
		goto error;
	}

	reg_data = max((u8)MAX77665_CHG_TO_ITH_100MA, (u8)pdata->top_off_current_thresold);
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_03, reg_data, 0x7<<0);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_03: %d\n", ret);
		goto error;
	}

	reg_data = max((u8)MAX77665_CHG_TO_TIME_10MIN, (u8)pdata->top_off_current_thresold);
	reg_data <<= 3;
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_03, reg_data, 0x7<<3);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_03: %d\n", ret);
		goto error;
	}

	reg_data = max((u8)MAX77665_CHG_CV_PRM_4200MV, (u8)pdata->charger_termination_voltage);
	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_04, reg_data, 0x1f<<0);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_04: %d\n", ret);
		goto error;
	}

	/* Lock protected registers */
	ret = max77665_write_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_06, 0x00);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX8957_REG_CHG_CNFG_06: %d\n", ret);
		goto error;
	}

	/* disable muic ctrl */
	ret = max77665_write_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_00, 0x24);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_00: %d\n", ret);
		goto error;
	}

	// Maximum Input Current Limit Selection.
	pdata->chgin_ilim_usb = min(MAX_USB_CURRENT, pdata->chgin_ilim_usb);
	ret = max77665_write_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_09, pdata->chgin_ilim_usb/CHGIN_ILIM_STEP);
	if (unlikely(ret)) {
		dev_err(charger->dev, "Failed to set MAX77665_CHG_REG_CHG_CNFG_09: %d\n", ret);
		goto error;
	}

	return 0;

error:
	return ret;
}

static int charger_usb_host_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct max77665_charger *chg =
		container_of(this, struct max77665_charger, usb_host_notifier);
	int rtn = NOTIFY_DONE;

	pr_info("%s (%lu)\n", __func__, event);
	switch (event) {
	case USB_HOST_INSERT:
		chg->usb_host_insert = true;
		break;
	case USB_HOST_REMOVE:
		chg->usb_host_insert = false;
		break;
	default:
		break;
	}
	return rtn;
}

static __devinit int max77665_charger_probe(struct platform_device *pdev)
{
	struct max77665_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77665_platform_data *pdata = dev_get_platdata(iodev->dev);	
	struct max77665_charger *charger;
	u8 reg_data;
	int ret = EINVAL;

	charger = kzalloc(sizeof(struct max77665_charger), GFP_KERNEL);
	if (unlikely(!charger))
		return -ENOMEM;

	platform_set_drvdata(pdev, charger);

	charger->iodev = iodev;
	charger->dev = &pdev->dev;
	charger->usb_attach = pdata->usb_attach;
	charger->chgin_ilim_usb = pdata->chgin_ilim_usb;
	charger->chgin_ilim_ac = pdata->chgin_ilim_ac;
	charger->chr_pin = pdata->charger_pin;

	mutex_init(&charger->mutex_t);
	wake_lock_init(&charger->wake_lock, WAKE_LOCK_SUSPEND, pdata->name);
	INIT_DELAYED_WORK(&charger->dwork, max77665_work_func);
	INIT_DELAYED_WORK(&charger->poll_dwork, max77665_poll_work_func);

	charger->ps = regulator_get(charger->dev, pdata->supply);
	if (IS_ERR(charger->ps)) {
		dev_err(&pdev->dev, "Failed to regulator_get: %ld\n", PTR_ERR(charger->ps));
		goto err_free;
	}

	charger->psy_charger.name = "charger";
	charger->psy_charger.type = POWER_SUPPLY_TYPE_BATTERY;
	charger->psy_charger.properties = max77665_charger_props,
	charger->psy_charger.num_properties = ARRAY_SIZE(max77665_charger_props),
	charger->psy_charger.get_property = max77665_charger_get_property,
	ret = power_supply_register(&pdev->dev, &charger->psy_charger);
	if (unlikely(ret != 0)) {
		dev_err(&pdev->dev, "Failed to power_supply_register psy_charger: %d\n", ret);
		goto err_put;
	}

	charger->psy_usb.name = "usb";
	charger->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	charger->psy_usb.supplied_to = supply_list,
	charger->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	charger->psy_usb.properties = max77665_power_props,
	charger->psy_usb.num_properties = ARRAY_SIZE(max77665_power_props),
	charger->psy_usb.get_property = max77665_usb_get_property,
	ret = power_supply_register(&pdev->dev, &charger->psy_usb);
	if (unlikely(ret != 0)) {
		dev_err(&pdev->dev, "Failed to power_supply_register psy_usb: %d\n", ret);
		goto err_unregister0;
	}

	charger->psy_ac.name = "ac";
	charger->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	charger->psy_ac.supplied_to = supply_list,
	charger->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	charger->psy_ac.properties = max77665_power_props,
	charger->psy_ac.num_properties = ARRAY_SIZE(max77665_power_props),
	charger->psy_ac.get_property = max77665_ac_get_property,
	ret = power_supply_register(&pdev->dev, &charger->psy_ac);
	if (unlikely(ret != 0)) {
		dev_err(&pdev->dev, "Failed to power_supply_register psy_ac: %d\n", ret);
		goto err_unregister1;
	}

	ret = max77665_init(charger);

	alarm_init(&charger->alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
				charger_bat_alarm);

	ret = max77665_read_reg(iodev->i2c, MAX77665_CHG_REG_CHG_INT_OK, &reg_data);
	if (unlikely(ret < 0))
		pr_err("Failed to read MAX77665_CHG_REG_CHG_INT: %d\n", ret);
	else {
		if (reg_data & 0x40) {	// CHGIN
			charger->chgin = true;
			schedule_delayed_work_on(0, &charger->dwork, 0);
			set_alarm(charger, WAKE_ALARM_INT);
		}
	}

	charger->chgin_irq = pdata->irq_base + MAX77665_CHG_IRQ_CHGIN_I;
	ret = request_threaded_irq(charger->chgin_irq, 0, max77665_charger_isr,
			0, pdev->name, charger);
	if (unlikely(ret < 0)) {
		dev_err(&pdev->dev, "max77665: failed to request IRQ	%d\n", charger->chgin_irq);
		goto err_unregister2;
	}

	charger->usb_host_insert = false;
	charger->usb_host_notifier.notifier_call = charger_usb_host_notifier_event;
	register_mx_usb_notifier(&charger->usb_host_notifier);

	return 0;

err_unregister2:
	alarm_cancel(&charger->alarm);
	power_supply_unregister(&charger->psy_ac);
err_unregister1:	
	power_supply_unregister(&charger->psy_usb);
err_unregister0:	
	power_supply_unregister(&charger->psy_charger);
err_put:
	regulator_put(charger->ps);
err_free:
	wake_lock_destroy(&charger->wake_lock);
	platform_set_drvdata(pdev, NULL);
	kfree(charger);
	return ret;
}

static __devexit int max77665_charger_remove(struct platform_device *pdev)
{
	struct max77665_charger *charger = platform_get_drvdata(pdev);

	alarm_cancel(&charger->alarm);
	cancel_delayed_work_sync(&charger->poll_dwork);

	free_irq(charger->chgin_irq, charger);
	power_supply_unregister(&charger->psy_usb);
	power_supply_unregister(&charger->psy_ac);
	platform_set_drvdata(pdev, NULL);
	kfree(charger);
	return 0;
}

#ifdef CONFIG_PM
static int max77665_suspend(struct device *dev)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);

	dev_dbg(charger->dev, "%s\n", __func__);
	cancel_delayed_work_sync(&charger->poll_dwork);

	return 0;
}

static int max77665_resume(struct device *dev)
{
	struct max77665_charger *charger = dev_get_drvdata(dev);

	dev_dbg(charger->dev, "%s\n", __func__);
	schedule_delayed_work_on(0, &charger->poll_dwork, HZ);

	return 0;
}

static const struct dev_pm_ops max77665_pm_ops = {
	.suspend		= max77665_suspend,
	.resume		= max77665_resume,
};
#else
#define max77665_pm_ops NULL
#endif

static struct platform_driver max77665_charger_driver =
{
	.driver = {
		.name = "max77665-charger",
		.owner = THIS_MODULE,
		.pm = &max77665_pm_ops,
	},
	.probe = max77665_charger_probe,
	.remove = __devexit_p(max77665_charger_remove),
};

static int __init max77665_charger_init(void)
{
	return platform_driver_register(&max77665_charger_driver);
}
late_initcall(max77665_charger_init);

static void __exit max77665_charger_exit(void)
{
	platform_driver_unregister(&max77665_charger_driver);
}
module_exit(max77665_charger_exit);

MODULE_DESCRIPTION("Charger driver for MAX77665");
MODULE_AUTHOR("Lvcha qiu <lvcha@meizu.com>;Chwei <Chwei@meizu.com>");
MODULE_LICENSE("GPLV2");
