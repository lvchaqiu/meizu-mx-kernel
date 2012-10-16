/*
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *
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
 */

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <mach/mx_battery.h>
#include <mach/gpio-common.h>
#include <linux/earlysuspend.h>

struct m032_bat_info {
	struct device *dev;

	char *fuel_gauge_name;
	char *charger_name;

	struct power_supply psy_bat;
	struct early_suspend battery_resume;
};

static enum power_supply_property m032_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int m032_bat_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct m032_bat_info *info = container_of(ps, struct m032_bat_info,
						 psy_bat);

	struct power_supply *fuelgauge_ps
	    = power_supply_get_by_name(info->fuel_gauge_name);

	struct power_supply *charger_ps
	    = power_supply_get_by_name(info->charger_name);

	if (!fuelgauge_ps || !charger_ps) {
		dev_err(info->dev, "%s: fail to get fuelgauge or charger ps\n", __func__);
		return -ENODEV;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		if(fuelgauge_ps->get_property(fuelgauge_ps, psp, val) || val->intval != POWER_SUPPLY_STATUS_FULL)
			return charger_ps->get_property(charger_ps, psp, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		do {
			union power_supply_propval value;

			if(fuelgauge_ps->get_property(fuelgauge_ps, psp, val))
				val->intval = 0;

			if(!charger_ps->get_property(charger_ps, POWER_SUPPLY_PROP_STATUS, &value)) {

				if(value.intval == POWER_SUPPLY_STATUS_FULL) {
					val->intval = 100;
				} else if(value.intval == POWER_SUPPLY_STATUS_CHARGING) {
					if(!fuelgauge_ps->get_property(fuelgauge_ps, POWER_SUPPLY_PROP_STATUS, &value) && 
						value.intval == POWER_SUPPLY_STATUS_FULL)
						val->intval = 100;
				}
			}

			if(val->intval <= 2)
				val->intval = 0;

		} while(0);
		break;
	default:
		return fuelgauge_ps->get_property(fuelgauge_ps, psp, val);
	}
	return 0;
}

static int m032_factory_bat_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = 21;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void battery_resume_func(struct early_suspend *s)
{
	struct m032_bat_info *info = container_of(s, struct m032_bat_info, battery_resume);
	power_supply_changed(&info->psy_bat);
}

static __devinit int m032_bat_probe(struct platform_device *pdev)
{
	struct mx_bat_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct m032_bat_info *info;
	struct power_supply *psy;
	int ret = 0;

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

	info->psy_bat.name = "battery";
	info->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	info->psy_bat.properties = m032_battery_props;
	info->psy_bat.num_properties = ARRAY_SIZE(m032_battery_props);
	info->psy_bat.get_property = m032_bat_get_property;

	psy = power_supply_get_by_name(info->charger_name);

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger\n", __func__);
		goto err_kfree;
	}

	psy = power_supply_get_by_name(info->fuel_gauge_name);

	if (!psy) {
		dev_err(info->dev, "%s: fail to get fuel gauge\n", __func__);
		info->psy_bat.get_property = m032_factory_bat_get_property;
	}

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &info->psy_bat);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_bat\n",
				__func__);
		goto err_kfree;
	}

	info->battery_resume.resume = battery_resume_func;
	info->battery_resume.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 10;
	register_early_suspend(&info->battery_resume);

	return 0;

err_kfree:
	kfree(info);
	return ret;
}

static int __devexit m032_bat_remove(struct platform_device *pdev)
{
	struct m032_bat_info *info = platform_get_drvdata(pdev);

	power_supply_unregister(&info->psy_bat);

	kfree(info);

	return 0;
}

static int m032_bat_suspend(struct device *dev)
{
	return 0;
}

static int m032_bat_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops m032_bat_pm_ops = {
	.suspend = m032_bat_suspend,
	.resume = m032_bat_resume,
};

static struct platform_driver m032_bat_driver = {
	.driver = {
		   .name = "m032-battery",
		   .owner = THIS_MODULE,
		   .pm = &m032_bat_pm_ops,
		   },
	.probe = m032_bat_probe,
	.remove = __devexit_p(m032_bat_remove),
};

static int __init m032_bat_init(void)
{
	return platform_driver_register(&m032_bat_driver);
}

static void __exit m032_bat_exit(void)
{
	platform_driver_unregister(&m032_bat_driver);
}

late_initcall(m032_bat_init);
module_exit(m032_bat_exit);

MODULE_DESCRIPTION("MX battery driver");
MODULE_AUTHOR("jgmai <jgmai@meizu.com>");
MODULE_LICENSE("GPL");
