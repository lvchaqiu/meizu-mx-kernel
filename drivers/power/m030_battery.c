/*
 *  m030_battery.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2010 Samsung Electronics
 *
 *  <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/android_alarm.h>
#include <linux/async.h>
#include <mach/mx_battery.h>

#ifdef CONFIG_SENSORS_S5PV310
#include <plat/s5p-tmu.h>
#endif

#define CPU_RECOVERY_TEMP	65
#define CPU_HIGH_TEMP		75


#define POLLING_INTERVAL	(40 * 1000)
#define RESUME_INTERVAL		(20 * 1000)
#define BAT_WAKE_INTERVAL	(5 * 60)
#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
#define VF_CHECK_INTERVAL	(10 * 1000)
#endif
#define FULL_CHARGING_TIME	(10 * 60 *  60 * HZ)	/* 10hr */
#define FULL_CHARGING_CURRENT	(20)		/* +-20mA */
#define RECHARGING_TIME		(90 * 60 * HZ)	/* 1.5hr */
#define RESETTING_CHG_TIME	(10 * 60 * HZ)	/* 10Min */
#define ATLEAST_CHG_TIME	(10 * 60 * HZ)	/* 10Min */

#define RECHARGING_VOLTAGE	(4100 * 1000)	/* 4.10 V */
#define RECHARGING_SOC		(98)			/*  98% */

#define FG_T_SOC		0
#define FG_T_VCELL		1
#define FG_T_TEMPER		2
#define FG_T_PSOC		3
#define FG_T_VFOCV		4
#define FG_T_AVGVCELL	5
#define FG_T_CURRENT	6
#define FG_T_TTE		7
#define FG_T_TTF		8
#define FG_Q_SOC		9
#define FG_Q_TEMPER		10

#define ADC_SAMPLING_CNT	7
#define ADC_CH_CHGCURRENT	1
#define ADC_TOTAL_COUNT		5

static int HIGH_BLOCK_TEMP;
static int LOW_BLOCK_TEMP;	

#define CURRENT_OF_FULL_CHG			520
#define TEMP_BLOCK_COUNT			3
#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
#define BAT_DET_COUNT				0
#else
#define BAT_DET_COUNT				1
#endif
#define FULL_CHG_COND_COUNT			3
#define FULL_CHARGE_COND_VOLTAGE    (4150 * 1000)	/* 4.15 V */

#define INIT_CHECK_COUNT	4

enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_MISC,
};

enum batt_full_t {
	BATT_NOT_FULL = 0,
	BATT_FULL,
};

enum {
	BAT_NOT_DETECTED,
	BAT_DETECTED
};

static atomic_t	attatched_usb;

static ssize_t m030_bat_show_property(struct device *dev,
				     struct device_attribute *attr, char *buf);

static ssize_t m030_bat_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count);

struct m030_bat_info {
	struct device *dev;

	char *fuel_gauge_name;
	char *charger_name;

	struct power_supply psy_bat;
	struct power_supply psy_usb;
	struct power_supply psy_ac;

	struct wake_lock vbus_wake_lock;
	struct wake_lock monitor_wake_lock;
	struct wake_lock cable_wake_lock;

	enum cable_type_t cable_type;
	enum batt_full_t batt_full_status;

	unsigned int batt_temp;	/* Battery Temperature (C) */
	int batt_temp_high_cnt;
	int batt_temp_low_cnt;
	int batt_temp_recover_cnt;
	unsigned int batt_health;
	unsigned int batt_vcell;
	unsigned int batt_vfocv;
	unsigned int batt_soc;
	unsigned int batt_raw_soc;
	unsigned int batt_tte;
	unsigned int batt_ttf;
	int batt_current;
	unsigned int polling_interval;
	int charging_status;
	int low_bat_alert;
	struct alarm bat_alarm;


	struct workqueue_struct *monitor_wqueue;
	struct work_struct monitor_work;
	struct work_struct cable_work;
	struct delayed_work resume_work;
	struct delayed_work polling_work;

	unsigned long charging_start_time;
	unsigned long charging_passed_time;
	unsigned long charging_next_time;
	unsigned int recharging_status;
	unsigned int batt_lpm_state;

	int present;
	int present_count;

	int test_value;
	int initial_check_count;
	struct proc_dir_entry *entry;

#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
	unsigned int vf_check_interval;
	struct delayed_work vf_check_work;
#endif
};

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property m030_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static enum power_supply_property m030_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int m030_bat_get_fuelgauge_data(struct m030_bat_info *info, int type)
{
	struct power_supply *psy
	    = power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval value;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get fuel gauge ps\n", __func__);
		return -ENODEV;
	}

	switch (type) {
	case FG_T_VCELL:
		value.intval = 0;	/*vcell */
		psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		break;
	case FG_T_VFOCV:
		value.intval = 1;	/*vfocv */
		psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		break;
	case FG_T_AVGVCELL:
		psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_AVG, &value);
		break;
	case FG_T_SOC:
		value.intval = 0;	/*normal soc */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		break;
	case FG_T_PSOC:
		value.intval = 1;	/*raw soc */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		break;
	case FG_Q_SOC:
		value.intval = 2;	/*quick soc */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		break;
	case FG_T_TEMPER:
		value.intval = 0;	/*normal temperature */
		psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
		break;
	case FG_Q_TEMPER:
		value.intval = 1;	/*quick temperature */
		psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
		break;
	case FG_T_CURRENT:
		psy->get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
		break;
	case FG_T_TTE:
		psy->get_property(psy, POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW, &value);
		break;
	case FG_T_TTF:
		psy->get_property(psy, POWER_SUPPLY_PROP_TIME_TO_FULL_NOW, &value);
		break;
	default:
		return -ENODEV;
	}

	return value.intval;
}

static int m030_bat_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct m030_bat_info *info = container_of(ps, struct m030_bat_info,
						 psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->charging_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = info->batt_temp;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->cable_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = info->batt_vcell;
		if (val->intval == -1)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = info->batt_current;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (info->charging_status == POWER_SUPPLY_STATUS_FULL) {
			val->intval = 100;
			break;
		}
		val->intval = info->batt_soc;
		if (val->intval == -1)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = info->batt_tte;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = info->batt_ttf;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int m030_bat_set_property(struct power_supply *ps,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct m030_bat_info *info = container_of(ps, struct m030_bat_info,
						 psy_bat);
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		dev_info(info->dev, "%s: topoff intr\n", __func__);
		if (val->intval != POWER_SUPPLY_STATUS_FULL)
			return -EINVAL;

		if (info->batt_full_status == BATT_NOT_FULL) {
			info->recharging_status = false;
			info->batt_full_status = BATT_FULL;
			info->charging_status = POWER_SUPPLY_STATUS_FULL;
			/* disable charging */
			value.intval = POWER_SUPPLY_STATUS_DISCHARGING;
			psy->set_property(psy, POWER_SUPPLY_PROP_STATUS,
					  &value);
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		if (val->intval == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL) {
			dev_info(info->dev, "%s: lowbatt intr\n", __func__);
			info->low_bat_alert = 1;
		}
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* cable is attached or detached. called by USB switch(MUIC) */
		dev_info(info->dev, "%s: cable was changed(%d)\n", __func__,
			 val->intval);
		switch (val->intval) {
		case POWER_SUPPLY_TYPE_BATTERY:
			info->cable_type = CABLE_TYPE_NONE;
			break;
		case POWER_SUPPLY_TYPE_MAINS:
			info->cable_type = CABLE_TYPE_AC;
			break;
		case POWER_SUPPLY_TYPE_USB:
			info->cable_type = CABLE_TYPE_USB;
			break;
		case POWER_SUPPLY_TYPE_MISC:
			info->cable_type = CABLE_TYPE_MISC;
			break;
		default:
			return -EINVAL;
		}
		wake_lock(&info->cable_wake_lock);
		queue_work(info->monitor_wqueue, &info->cable_work);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int m030_usb_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct m030_bat_info *info = container_of(ps, struct m030_bat_info,
						 psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (info->cable_type == CABLE_TYPE_USB);

	return 0;
}

static int m030_ac_get_property(struct power_supply *ps,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct m030_bat_info *info = container_of(ps, struct m030_bat_info,
						 psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (info->cable_type == CABLE_TYPE_AC) ||
			(info->cable_type == CABLE_TYPE_MISC);

	return 0;
}

static int m030_bat_check_temper(struct m030_bat_info *info)
{
	int temp;

	int health = info->batt_health;

	temp = m030_bat_get_fuelgauge_data(info, FG_T_TEMPER);
	info->batt_temp = temp;

	if (temp >= HIGH_BLOCK_TEMP) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT &&
		    health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			if (info->batt_temp_high_cnt < TEMP_BLOCK_COUNT)
				info->batt_temp_high_cnt++;
			dev_info(info->dev, "%s: high count = %d temp = %d\n",
				__func__, info->batt_temp_high_cnt, temp);
	} else if (temp <= LOW_BLOCK_TEMP) {
		if (health != POWER_SUPPLY_HEALTH_COLD &&
		    health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			if (info->batt_temp_low_cnt < TEMP_BLOCK_COUNT)
				info->batt_temp_low_cnt++;
			dev_info(info->dev, "%s: low count = %d temp = %d\n",
				__func__, info->batt_temp_low_cnt, temp);
	} else {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    health == POWER_SUPPLY_HEALTH_COLD)
			if (info->batt_temp_recover_cnt < TEMP_BLOCK_COUNT) {
				info->batt_temp_recover_cnt++;
				dev_info(info->dev, "%s: recovery count = %d temp = %d\n",
						__func__, info->batt_temp_recover_cnt, temp);
			}
	}

	if (info->batt_temp_high_cnt >= TEMP_BLOCK_COUNT) {
		info->batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		info->batt_temp_high_cnt = 0;
		info->batt_temp_low_cnt = 0;
		info->batt_temp_recover_cnt = 0;
	}
	else if (info->batt_temp_low_cnt >= TEMP_BLOCK_COUNT) {
		info->batt_health = POWER_SUPPLY_HEALTH_COLD;
		info->batt_temp_high_cnt = 0;
		info->batt_temp_low_cnt = 0;
		info->batt_temp_recover_cnt = 0;
	}
	else if (info->batt_temp_recover_cnt >= TEMP_BLOCK_COUNT) {
		info->batt_health = POWER_SUPPLY_HEALTH_GOOD;
		info->batt_temp_high_cnt = 0;
		info->batt_temp_low_cnt = 0;
		info->batt_temp_recover_cnt = 0;
	}

	dev_dbg(info->dev, "%s: temp=%d\n", __func__, temp);

	return temp;
}

static void m030_bat_update_info(struct m030_bat_info *info)
{
	info->batt_raw_soc = m030_bat_get_fuelgauge_data(info, FG_T_PSOC);
	info->batt_soc = m030_bat_get_fuelgauge_data(info, FG_T_SOC);
	info->batt_vcell = m030_bat_get_fuelgauge_data(info, FG_T_VCELL);
	info->batt_vfocv = m030_bat_get_fuelgauge_data(info, FG_T_VFOCV);
	info->batt_current = m030_bat_get_fuelgauge_data(info, FG_T_CURRENT);
	info->batt_tte = m030_bat_get_fuelgauge_data(info, FG_T_TTE);
	info->batt_ttf = m030_bat_get_fuelgauge_data(info, FG_T_TTF);
}

#ifdef CONFIG_MHL_DRIVER
extern bool mhl_cable_status(void);
#else
static inline bool mhl_cable_status(void) {
	return 0;
}
#endif

static void reset_cable_type(struct m030_bat_info *info, bool enable)
{
	printk("%s call\n", __func__);
	if(enable) {
		if(info->cable_type == CABLE_TYPE_USB) {
			if(mhl_cable_status()) {
				dev_info(info->dev, "found mhl in USB mode, treat it as AC\n");
				info->cable_type = CABLE_TYPE_MISC; //reset the cable type to MISC
			} else {
#if 0
				if(atomic_read(&attatched_usb) == 0) {
					dev_info(info->dev, "adc read check failed, usb not found, AC instead\n");
					info->cable_type = CABLE_TYPE_AC; //reset the cable type to AC
				} 
#endif
			}
		}
	}
}

static int m030_bat_enable_charging_main(struct m030_bat_info *info, bool enable)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval val_type, val_chg_current, val_topoff;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	info->batt_full_status = BATT_NOT_FULL;

	if (enable) {		/* Enable charging */
		switch (info->cable_type) {
		case CABLE_TYPE_USB:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			val_chg_current.intval = 450;	/* mA */
			break;
		case CABLE_TYPE_AC:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			val_chg_current.intval = 950;	/* mA */
			break;
		case CABLE_TYPE_MISC:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			val_chg_current.intval = 950;	/* mA */
			break;
		default:
			dev_err(info->dev, "%s: Invalid func use\n", __func__);
			return -EINVAL;
		}

		/* Set charging current */
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW,
					&val_chg_current);
		if (ret) {
			dev_err(info->dev, "%s: fail to set charging cur(%d)\n",
				__func__, ret);
			return ret;
		}

		/* Set topoff current */
		val_topoff.intval = 50;
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_CHARGE_FULL,
					&val_topoff);
		if (ret) {
			dev_err(info->dev, "%s: fail to set topoff cur(%d)\n",
				__func__, ret);
			return ret;
		}

		/*Reset charging start time only in initial charging start */
		if (info->charging_start_time == 0) {
			info->charging_start_time = jiffies;
			info->charging_next_time = (60 * HZ);
		}
	} else {			/* Disable charging */
		val_type.intval = POWER_SUPPLY_STATUS_DISCHARGING;

		info->charging_start_time = 0;
		info->charging_passed_time = 0;
		info->charging_next_time = 0;
	}

	ret = psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &val_type);
	if (ret) {
		dev_err(info->dev, "%s: fail to set charging status(%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int m030_bat_enable_charging(struct m030_bat_info *info, bool enable)
{
	if (enable && (info->batt_health != POWER_SUPPLY_HEALTH_GOOD)) {
		info->charging_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		dev_info(info->dev, "%s: Battery is NOT good!!!\n", __func__);
		return -EPERM;
	}

	return m030_bat_enable_charging_main(info, enable);
}

static void m030_bat_resume_work_internal(struct m030_bat_info *info)
{
	wake_lock(&info->monitor_wake_lock);

	schedule_delayed_work(&info->polling_work, msecs_to_jiffies(RESUME_INTERVAL));

#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
	schedule_delayed_work(&info->vf_check_work,
			msecs_to_jiffies(VF_CHECK_INTERVAL));
#endif
	
	return ;
}

static void m030_bat_resume_work(struct work_struct *work)
{
	struct m030_bat_info *info;
	info = container_of(work, struct m030_bat_info, resume_work.work);
	m030_bat_resume_work_internal(info);
}

static void bat_set_alarm(struct m030_bat_info *info, int seconds)
{
	ktime_t interval = ktime_set(seconds, 0);
	ktime_t now = alarm_get_elapsed_realtime();
	ktime_t next = ktime_add(now, interval);

	pr_info("set alarm after %d seconds\n", seconds);
	alarm_start_range(&info->bat_alarm, next, next);
}

static void m030_bat_alarm(struct alarm *alarm)
{
	struct m030_bat_info *info = 
			container_of(alarm, struct m030_bat_info, bat_alarm);

	pr_info("----------%s occur\n", __func__);
	if(info->cable_type == CABLE_TYPE_NONE) {
		pr_info("not charging disable battery alarm\n");
		alarm_try_to_cancel(&info->bat_alarm);
	} else {
		bat_set_alarm(info, BAT_WAKE_INTERVAL);
	}
}

static void m030_bat_cable_work(struct work_struct *work)
{
	struct m030_bat_info *info = container_of(work, struct m030_bat_info,
						 cable_work);

	switch (info->cable_type) {
	case CABLE_TYPE_NONE:
		if (!m030_bat_enable_charging(info, false)) {
			info->batt_full_status = BATT_NOT_FULL;
			info->recharging_status = false;
			info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
			info->batt_health = POWER_SUPPLY_HEALTH_GOOD;
			info->batt_temp_high_cnt = 0;
			info->batt_temp_low_cnt = 0;
			info->batt_temp_recover_cnt = 0;
			atomic_set(&attatched_usb, 0);
			alarm_cancel(&info->bat_alarm);
			wake_lock_timeout(&info->vbus_wake_lock, HZ * 5);
		}
		break;
	case CABLE_TYPE_USB:
	case CABLE_TYPE_AC:
	case CABLE_TYPE_MISC:
		if (!m030_bat_enable_charging(info, true)) {
			info->charging_status = POWER_SUPPLY_STATUS_CHARGING;
			alarm_cancel(&info->bat_alarm);
			bat_set_alarm(info, BAT_WAKE_INTERVAL);
			wake_lock_timeout(&info->vbus_wake_lock, HZ * 5);
		}
		break;
	default:
		dev_err(info->dev, "%s: Invalid cable type\n", __func__);
		break;;
	}

	power_supply_changed(&info->psy_ac);
	power_supply_changed(&info->psy_usb);

	wake_unlock(&info->cable_wake_lock);
}

static bool m030_bat_charging_time_management(struct m030_bat_info *info)
{
	if (info->charging_start_time == 0) {
		dev_dbg(info->dev, "%s: charging_start_time has never\
			 been used since initializing\n", __func__);
		return false;
	}

	if (jiffies >= info->charging_start_time)
		info->charging_passed_time =
			jiffies - info->charging_start_time;
	else
		info->charging_passed_time =
			0xFFFFFFFF - info->charging_start_time + jiffies;

	switch (info->charging_status) {
	case POWER_SUPPLY_STATUS_FULL:
		if (time_after(info->charging_passed_time,
				(unsigned long)RECHARGING_TIME) &&
			info->recharging_status == true) {
			if (!m030_bat_enable_charging(info, false)) {
				info->recharging_status = false;
				dev_info(info->dev,
						"%s: Recharging timer expired\n", __func__);
				return true;
			}
		} else if(	info->recharging_status == true &&
					info->batt_raw_soc == 100 &&
					info->batt_current < FULL_CHARGING_CURRENT &&
					info->batt_current > -FULL_CHARGING_CURRENT &&
					time_after(info->charging_passed_time,
						(unsigned long)ATLEAST_CHG_TIME)) {
			if (!m030_bat_enable_charging(info, false)) {
				info->recharging_status = false;
				dev_info(info->dev,
						"%s: ReCharging charge full\n", __func__);
				return true;
			}
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (time_after(info->charging_passed_time,
					(unsigned long)FULL_CHARGING_TIME)) {
			if(	info->batt_current < FULL_CHARGING_CURRENT &&
				info->batt_current > -FULL_CHARGING_CURRENT) {
				if (!m030_bat_enable_charging(info, false)) {
					info->charging_status = POWER_SUPPLY_STATUS_FULL;
					dev_info(info->dev,
							"%s: Charging timer expired\n", __func__);
					return true;
				}
			}
		}
		if (time_after(info->charging_passed_time,
					info->charging_next_time)) {
			/*reset current in charging status */
			reset_cable_type(info, true);
			if (!m030_bat_enable_charging(info, true)) {
			info->charging_next_time =
					info->charging_passed_time + RESETTING_CHG_TIME;

				dev_dbg(info->dev,
					 "%s: Reset charging current\n", __func__);
			}
		} else if(	info->batt_raw_soc == 100 &&
			info->batt_current < FULL_CHARGING_CURRENT &&
			info->batt_current > -FULL_CHARGING_CURRENT &&
			time_after(info->charging_passed_time,
			       (unsigned long)ATLEAST_CHG_TIME)) {
			if (!m030_bat_enable_charging(info, false)) {
				info->charging_status =
					POWER_SUPPLY_STATUS_FULL;

				dev_info(info->dev,
						"%s: Charging charge full\n", __func__);
				return true;
			}
		}
		break;
	default:
		dev_info(info->dev, "%s: Undefine Battery Status\n", __func__);
		return false;
	}

	dev_dbg(info->dev, "Time past : %u secs\n",
		jiffies_to_msecs(info->charging_passed_time) / 1000);

	return false;
}

static void m030_bat_check_vf(struct m030_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	int ret;

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &value);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to get status(%d)\n",
			__func__, ret);
		return;
	}

	if (value.intval == BAT_NOT_DETECTED) {
		if (info->present_count < BAT_DET_COUNT)
			info->present_count++;
		else {
			info->present = BAT_NOT_DETECTED;
			if (info->batt_health == POWER_SUPPLY_HEALTH_GOOD)
				info->batt_health =
				    POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		}
	} else {
		info->present = BAT_DETECTED;
		if ((info->batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) ||
		    (info->batt_health == POWER_SUPPLY_HEALTH_UNKNOWN))
			info->batt_health = POWER_SUPPLY_HEALTH_GOOD;
		info->present_count = 0;
	}

	return;
}

static void m030_bat_monitor_work(struct work_struct *work)
{
	struct m030_bat_info *info = container_of(work, struct m030_bat_info,
						 monitor_work);

#ifndef MX_BATTERY_INDEPEDENT_VF_CHECK
	m030_bat_check_vf(info);
#endif

	m030_bat_update_info(info);

	m030_bat_check_temper(info);

	if(info->low_bat_alert) {
		if(info->batt_raw_soc >= 8)
			info->low_bat_alert = 0;
		else
			info->batt_soc = 0;
	}
	if (m030_bat_charging_time_management(info))
		goto full_charged;

	if (info->cable_type == CABLE_TYPE_NONE &&
	    info->charging_status == POWER_SUPPLY_STATUS_FULL) {
		dev_err(info->dev, "invalid full state\n");
		info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (info->test_value) {
		switch (info->test_value) {
		case 1: /*full status */
			info->charging_status = POWER_SUPPLY_STATUS_FULL;
			break;
		case 2: /*low temperature */
			info->batt_health = POWER_SUPPLY_HEALTH_COLD;
			break;
		case 3: /*high temperature */
			info->batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case 4: /*over voltage */
			info->batt_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			break;
		case 5: /*abnormal battery */
			info->batt_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			break;
		}
	}

	switch (info->charging_status) {
	case POWER_SUPPLY_STATUS_FULL:
		if (((info->batt_vcell < RECHARGING_VOLTAGE) || (info->batt_raw_soc <  RECHARGING_SOC)) &&
		    info->recharging_status == false) {
			if (!m030_bat_enable_charging(info, true)) {
			info->recharging_status = true;

			dev_info(info->dev,
					 "%s: Start Recharging, Vcell = %d\n",
					 __func__, info->batt_vcell);
			}
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		switch (info->batt_health) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_DEAD:
		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			if (!m030_bat_enable_charging(info, false)) {
			info->charging_status =
			    POWER_SUPPLY_STATUS_NOT_CHARGING;
			info->recharging_status = false;

				dev_info(info->dev, "%s: Not charging\n",
					 __func__);
			}
			break;
		default:
			break;
		}
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		dev_dbg(info->dev, "%s: Discharging\n", __func__);
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (info->batt_health == POWER_SUPPLY_HEALTH_GOOD) {
			dev_info(info->dev, "%s: recover health state\n",
				 __func__);
			if (info->cable_type != CABLE_TYPE_NONE) {
				if (!m030_bat_enable_charging(info, true))
				info->charging_status
				    = POWER_SUPPLY_STATUS_CHARGING;
			} else
				info->charging_status
				    = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	default:
		dev_info(info->dev, "%s: Undefined Battery Status\n", __func__);
		return;
	}

full_charged:
	dev_dbg(info->dev,
		"soc(%d), vfocv(%d), vcell(%d), temp(%d), current(%d), charging(%d), health(%d)\n",
		info->batt_soc, info->batt_vfocv / 1000,
		info->batt_vcell / 1000, info->batt_temp / 10,
		info->batt_current,
		info->charging_status, info->batt_health);

	power_supply_changed(&info->psy_bat);

	wake_unlock(&info->monitor_wake_lock);

	return;
}

#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
static void m030_bat_vf_check_work(struct work_struct *work)
{
	struct m030_bat_info *info;
	info = container_of(work, struct m030_bat_info, vf_check_work.work);

	m030_bat_check_vf(info);

	if (info->batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
		dev_info(info->dev, "%s: Battery Disconnected\n", __func__);

		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	}

	schedule_delayed_work(&info->vf_check_work,
			      msecs_to_jiffies(VF_CHECK_INTERVAL));
}
#endif

static void m030_bat_polling_work(struct work_struct *work)
{
	struct m030_bat_info *info;
	info = container_of(work, struct m030_bat_info, polling_work.work);

	wake_lock(&info->monitor_wake_lock);
	queue_work(info->monitor_wqueue, &info->monitor_work);

	if (info->initial_check_count) {
		schedule_delayed_work(&info->polling_work, HZ);
		info->initial_check_count--;
	} else
	schedule_delayed_work(&info->polling_work,
			      msecs_to_jiffies(info->polling_interval));
}

#define SEC_BATTERY_ATTR(_name)		\
{									\
	.attr = {	.name = #_name,		\
				.mode = 0664,},		\
	.show = m030_bat_show_property,	\
	.store = m030_bat_store,			\
}

static struct device_attribute m030_battery_attrs[] = {
	SEC_BATTERY_ATTR(batt_vol),
	SEC_BATTERY_ATTR(batt_soc),
	SEC_BATTERY_ATTR(batt_vfocv),
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(charging_source),
	SEC_BATTERY_ATTR(batt_full_check),
	SEC_BATTERY_ATTR(batt_temp_check),
	SEC_BATTERY_ATTR(batt_temp_spec),
	SEC_BATTERY_ATTR(batt_test_value),
	SEC_BATTERY_ATTR(fg_psoc),
	SEC_BATTERY_ATTR(batt_lpm_state),
};

enum {
	BATT_VOL = 0,
	BATT_SOC,
	BATT_VFOCV,
	BATT_TEMP,
	CHARGING_SOURCE,
	BATT_FULL_CHECK,
	BATT_TEMP_CHECK,
	BATT_TEMP_SPEC,
	BATT_TEST_VALUE,
	BATT_FG_PSOC,
	BATT_LPM_STATE,
};

static ssize_t m030_bat_show_property(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct m030_bat_info *info = dev_get_drvdata(dev->parent);
	int i = 0, val;
	const ptrdiff_t off = attr - m030_battery_attrs;

	switch (off) {
	case BATT_VOL:
		val = m030_bat_get_fuelgauge_data(info, FG_T_AVGVCELL);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_SOC:
		val = m030_bat_get_fuelgauge_data(info, FG_T_SOC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_VFOCV:
		val = m030_bat_get_fuelgauge_data(info, FG_T_VFOCV);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", info->batt_temp);
		break;
	case CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       info->cable_type);
		break;
	case BATT_FULL_CHECK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       (info->charging_status ==
				POWER_SUPPLY_STATUS_FULL)
			? 1 : 0);
		break;
	case BATT_TEMP_CHECK:
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"%d\n", info->batt_health);
		break;
	case BATT_TEMP_SPEC:
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"(HIGH: %d,   LOW: %d)\n",
			       HIGH_BLOCK_TEMP / 10, 
			       LOW_BLOCK_TEMP / 10);
		break;
	case BATT_TEST_VALUE:
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"0-normal, 1-full, 2-low, 3-high, 4-over, 5-cf (%d)\n",
			info->test_value);
		break;
	case BATT_FG_PSOC:
		val = m030_bat_get_fuelgauge_data(info, FG_T_PSOC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_LPM_STATE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->batt_lpm_state);
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t m030_bat_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret = 0, x = 0;
	const ptrdiff_t off = attr - m030_battery_attrs;
	struct m030_bat_info *info = dev_get_drvdata(dev->parent);

	switch (off) {
	case BATT_TEST_VALUE:
		if (sscanf(buf, "%d\n", &x) == 1) {
			info->test_value = x;
			printk("%s : test case : %d\n", __func__,
			       info->test_value);
			ret = count;
		}
		break;
	case BATT_LPM_STATE:
		if (sscanf(buf, "%d\n", &x) == 1) {
			info->batt_lpm_state = x;
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int m030_bat_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(m030_battery_attrs); i++) {
		rc = device_create_file(dev, &m030_battery_attrs[i]);
		if (rc)
			goto m030_attrs_failed;
	}
	goto succeed;

m030_attrs_failed:
	while (i--)
		device_remove_file(dev, &m030_battery_attrs[i]);
succeed:
	return rc;
}

static int m030_bat_is_charging(struct m030_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to get status(%d)\n", __func__,
			ret);
		return ret;
	}

	return value.intval;
}

static int m030_bat_read_proc(char *buf, char **start,
			off_t offset, int count, int *eof, void *data)
{
	struct m030_bat_info *info = data;
	struct timespec cur_time;
	ktime_t ktime;
	int len = 0;

	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	len = sprintf(buf, "%lu, %u, %u, %u, %u, %d, \
%u, %u, %u, %u, %d, %lu\n",
		cur_time.tv_sec,
		info->batt_raw_soc,
		info->batt_soc,
		info->batt_vfocv,
		info->batt_vcell,
		info->batt_full_status,
		info->recharging_status,
		info->batt_health,
		info->charging_status,
		info->present,
		info->cable_type,
		info->charging_passed_time);

    return len;
}

#ifdef CONFIG_SENSORS_S5PV310
static int cpu_temp_notifier_call(struct notifier_block *this,
					unsigned long value, void *_cmd)
{
	if(value < CPU_RECOVERY_TEMP)
		HIGH_BLOCK_TEMP = 450;
	else if(value > CPU_HIGH_TEMP)
		HIGH_BLOCK_TEMP = 500;
	
	return NOTIFY_DONE;
}

static struct notifier_block hotplug_tmu_notifier = {
	.notifier_call = cpu_temp_notifier_call,
};
#endif

static __devinit int m030_bat_probe(struct platform_device *pdev)
{
	struct mx_bat_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct m030_bat_info *info;
	struct power_supply *psy;
	union power_supply_propval value;
	int ret = 0;

	LOW_BLOCK_TEMP = 0;
	HIGH_BLOCK_TEMP = 450;
#ifdef CONFIG_SENSORS_S5PV310
	register_s5p_tmu_notifier(&hotplug_tmu_notifier);
#endif

	dev_info(&pdev->dev, "%s: MX Battery Driver Loading\n", __func__);

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	if (!pdata->fuel_gauge_name || !pdata->charger_name) {
		dev_err(info->dev, "%s: no fuel gauge or charger name\n",
			__func__);
		goto err_kfree;
	}
	info->fuel_gauge_name = pdata->fuel_gauge_name;
	info->charger_name = pdata->charger_name;
	
	info->psy_bat.name = "battery",
	info->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	info->psy_bat.properties = m030_battery_props,
	info->psy_bat.num_properties = ARRAY_SIZE(m030_battery_props),
	info->psy_bat.get_property = m030_bat_get_property,
	info->psy_bat.set_property = m030_bat_set_property,
	info->psy_usb.name = "usb",
	info->psy_usb.type = POWER_SUPPLY_TYPE_USB,
	info->psy_usb.supplied_to = supply_list,
	info->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	info->psy_usb.properties = m030_power_props,
	info->psy_usb.num_properties = ARRAY_SIZE(m030_power_props),
	info->psy_usb.get_property = m030_usb_get_property,
	info->psy_ac.name = "ac",
	info->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	info->psy_ac.supplied_to = supply_list,
	info->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	info->psy_ac.properties = m030_power_props,
	info->psy_ac.num_properties = ARRAY_SIZE(m030_power_props),
	info->psy_ac.get_property = m030_ac_get_property;

	wake_lock_init(&info->vbus_wake_lock, WAKE_LOCK_SUSPEND,
		       "m030-vbus-present");
	wake_lock_init(&info->monitor_wake_lock, WAKE_LOCK_SUSPEND,
		       "m030-battery-monitor");
	wake_lock_init(&info->cable_wake_lock, WAKE_LOCK_SUSPEND,
		       "m030-battery-cable");
	
	alarm_init(&info->bat_alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP, m030_bat_alarm);

	psy = power_supply_get_by_name(info->charger_name);

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger\n", __func__);
		return -ENODEV;
	}

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &value);

	if (ret < 0) {
		dev_err(info->dev, "%s: fail to get status(%d)\n",
			__func__, ret);
		return -ENODEV;
	}

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &info->psy_bat);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_bat\n",
			__func__);
		goto err_wake_lock;
	}

	ret = power_supply_register(&pdev->dev, &info->psy_usb);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_usb\n",
			__func__);
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &info->psy_ac);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_ac\n", __func__);
		goto err_supply_unreg_usb;
	}

	/* create sec detail attributes */
	m030_bat_create_attrs(info->psy_bat.dev);

	info->entry = create_proc_entry("batt_info_proc", S_IRUGO, NULL);
	if (!info->entry)
		dev_err(info->dev, "%s: failed to create proc_entry\n",
			__func__);
    else {
		info->entry->read_proc = m030_bat_read_proc;
		info->entry->data = (struct m030_bat_info *)info;
    }

	info->monitor_wqueue =
	    create_freezable_workqueue(dev_name(&pdev->dev));
	if (!info->monitor_wqueue) {
		dev_err(info->dev, "%s: fail to create workqueue\n", __func__);
		goto err_supply_unreg_ac;
	}

	INIT_WORK(&info->monitor_work, m030_bat_monitor_work);
	INIT_WORK(&info->cable_work, m030_bat_cable_work);

	INIT_DELAYED_WORK_DEFERRABLE(&info->resume_work,
		m030_bat_resume_work);

	INIT_DELAYED_WORK_DEFERRABLE(&info->polling_work,
		m030_bat_polling_work);

#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
	INIT_DELAYED_WORK_DEFERRABLE(&info->vf_check_work,
		m030_bat_vf_check_work);
#endif


	info->present = value.intval;

	if (info->present == BAT_NOT_DETECTED)
		info->batt_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	else
		info->batt_health = POWER_SUPPLY_HEALTH_GOOD;

	info->charging_status = m030_bat_is_charging(info);
	if (info->charging_status < 0)
		info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;

	info->present_count = 0;
	info->batt_temp_high_cnt = 0;
	info->batt_temp_low_cnt = 0;
	info->batt_temp_recover_cnt = 0;
	info->low_bat_alert = 0;

	info->initial_check_count = INIT_CHECK_COUNT;

	info->charging_start_time = 0;

	info->polling_interval = POLLING_INTERVAL;

	info->test_value = 0;
	schedule_delayed_work(&info->polling_work, msecs_to_jiffies(2 * 1000));

#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
	schedule_delayed_work(&info->vf_check_work, msecs_to_jiffies(2 * 1000));
#endif
	//get some info first
	{
		/* wait for fuel gauge ready*/
		async_synchronize_full();
		info->batt_soc = m030_bat_get_fuelgauge_data(info, FG_T_SOC);
	}

	return 0;

err_supply_unreg_ac:
	power_supply_unregister(&info->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&info->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&info->psy_bat);
err_wake_lock:
	wake_lock_destroy(&info->vbus_wake_lock);
	wake_lock_destroy(&info->monitor_wake_lock);
	wake_lock_destroy(&info->cable_wake_lock);
err_kfree:
	kfree(info);

	return ret;
}

static int __devexit m030_bat_remove(struct platform_device *pdev)
{
	struct m030_bat_info *info = platform_get_drvdata(pdev);

	remove_proc_entry("batt_info_proc", NULL);

	flush_workqueue(info->monitor_wqueue);
	destroy_workqueue(info->monitor_wqueue);

	alarm_cancel(&info->bat_alarm);
	cancel_delayed_work(&info->resume_work);
	cancel_delayed_work(&info->polling_work);
#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
	cancel_delayed_work(&info->vf_check_work);
#endif

	power_supply_unregister(&info->psy_bat);
	power_supply_unregister(&info->psy_usb);
	power_supply_unregister(&info->psy_ac);

	wake_lock_destroy(&info->vbus_wake_lock);
	wake_lock_destroy(&info->monitor_wake_lock);
	wake_lock_destroy(&info->cable_wake_lock);

	kfree(info);

	return 0;
}


void m030_cable_check_status(int flag)
{
	if(flag) {
		printk("********%s usb found status(%d)\n", __func__, flag);
		atomic_set(&attatched_usb, flag);
	}
	return;
}
EXPORT_SYMBOL(m030_cable_check_status);

static int m030_bat_suspend(struct device *dev)
{
	struct m030_bat_info *info = dev_get_drvdata(dev);

	cancel_work_sync(&info->monitor_work);

	cancel_delayed_work(&info->resume_work);
	cancel_delayed_work(&info->polling_work);
#ifdef MX_BATTERY_INDEPEDENT_VF_CHECK
	cancel_delayed_work(&info->vf_check_work);
#endif

	return 0;
}

static int m030_bat_resume(struct device *dev)
{
	struct m030_bat_info *info = dev_get_drvdata(dev);
	int soc = m030_bat_get_fuelgauge_data(info, FG_Q_SOC);
	int temper = m030_bat_get_fuelgauge_data(info, FG_Q_TEMPER);

	pr_debug("%s,soc %d, temper %d check\n\n", __func__, soc, temper);
	if(		(soc == 0) || 
			(info->charging_status == POWER_SUPPLY_STATUS_FULL) ||
			(info->batt_health != POWER_SUPPLY_HEALTH_GOOD) || 
			(temper >= HIGH_BLOCK_TEMP || temper <= LOW_BLOCK_TEMP) ) {
		pr_debug("%s need full check\n", __func__);
		m030_bat_resume_work_internal(info);
	} else {
		info->batt_soc = soc;
		info->batt_temp = temper;
		power_supply_changed(&info->psy_bat);
		schedule_delayed_work(&info->resume_work, msecs_to_jiffies(RESUME_INTERVAL));
	}
	return 0;
}

static const struct dev_pm_ops m030_bat_pm_ops = {
	.suspend = m030_bat_suspend,
	.resume = m030_bat_resume,
};

static struct platform_driver m030_bat_driver = {
	.driver = {
		   .name = "m030-battery",
		   .owner = THIS_MODULE,
		   .pm = &m030_bat_pm_ops,
		   },
	.probe = m030_bat_probe,
	.remove = __devexit_p(m030_bat_remove),
};

static int __init m030_bat_init(void)
{
	return platform_driver_register(&m030_bat_driver);
}

static void __exit m030_bat_exit(void)
{
	platform_driver_unregister(&m030_bat_driver);
}

late_initcall(m030_bat_init);
module_exit(m030_bat_exit);

MODULE_DESCRIPTION("M030 battery driver");
MODULE_AUTHOR("<>");
MODULE_LICENSE("GPL");
