/*
 *  max8997-charger.c
 *  MAXIM 8997 charger interface driver
 *
 *  Copyright (C) 2010 Samsung Electronics
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
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max8997.h>
#include <linux/mfd/max8997-private.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/gpio.h>
#include <plat/adc.h>
#include <mach/gpio-m030.h>

/* MAX8997_REG_STATUS4 */
#define DCINOK_SHIFT		1
#define DCINOK_MASK		(1 << DCINOK_SHIFT)
#define DETBAT_SHIFT		2
#define DETBAT_MASK		(1 << DETBAT_SHIFT)

/*  MAX8997_REG_MBCCTRL1 */
#define TFCH_SHIFT		4
#define TFCH_MASK		(7 << TFCH_SHIFT)

/*  MAX8997_REG_MBCCTRL2 */
#define MBCHOSTEN_SHIFT		6
#define VCHGR_FC_SHIFT		7
#define MBCHOSTEN_MASK		(1 << MBCHOSTEN_SHIFT)
#define VCHGR_FC_MASK		(1 << VCHGR_FC_SHIFT)

/*  MAX8997_REG_MBCCTRL3 */
#define MBCCV_SHIFT		0
#define MBCCV_MASK		(0xF << MBCCV_SHIFT)

/* MAX8997_REG_MBCCTRL4 */
#define MBCICHFC_SHIFT		0
#define MBCICHFCSET_SHIFT	4
#define MBCICHFCSET			(0x1 << MBCICHFCSET_SHIFT)
#define MBCCTRL4_MASK		0xFF

/* MAX8997_REG_MBCCTRL5 */
#define ITOPOFF_SHIFT		0
#define ITOPOFF_MASK		(0xF << ITOPOFF_SHIFT)

/* MAX8997_REG_MBCCTRL6 */
#define AUTOSTOP_SHIFT		5
#define AUTOSTOP_MASK		(1 << AUTOSTOP_SHIFT)

#define MAX_ADC_READ_COUNT	4


struct charger_work_info {
	struct delayed_work usb_attach_work;
	int charger_type;
};

static struct charger_work_info charger_work_info;
static int charger_topoff_cb(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}

	value.intval = POWER_SUPPLY_STATUS_FULL;
	return psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
}

static int max8997_power_set_charger(int insert)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	pr_debug("max8997_power_set_charger %d\n", insert);

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}

	value.intval = insert;
	return psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
}

static BLOCKING_NOTIFIER_HEAD(max8997_charger_chain_head);

int register_max8997_charger_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&max8997_charger_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_max8997_charger_notifier);

int unregister_max8997_charger_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&max8997_charger_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_max8997_charger_notifier);

static int max8997_charger_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&max8997_charger_chain_head, val, NULL)
			== NOTIFY_BAD) ? -EINVAL : 0;
}

extern struct platform_device s3c_device_usbgadget;

static void usb_attach_work(struct work_struct *work)
{
#ifdef CONFIG_USB_GADGET
	int ret = 0;
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);
	if (gadget) {
		if (charger_work_info.charger_type == POWER_SUPPLY_TYPE_USB) {
			printk("usb connect\n");
			ret = usb_gadget_vbus_connect(gadget);
		} else if (charger_work_info.charger_type == POWER_SUPPLY_TYPE_BATTERY){
			printk("usb disconnect\n");
			msleep(500);
			ret = usb_gadget_vbus_disconnect(gadget);
		}
	}
	if(ret) {
		printk("====%s failed, schedule again\n", __func__);
		schedule_delayed_work(&charger_work_info.usb_attach_work, msecs_to_jiffies(200));
	} else {
		max8997_charger_notifier_call_chain(charger_work_info.charger_type != POWER_SUPPLY_TYPE_BATTERY);
	}
#else
		max8997_charger_notifier_call_chain(charger_work_info.charger_type != POWER_SUPPLY_TYPE_BATTERY);
#endif
}

enum {
	BAT_NOT_DETECTED,
	BAT_DETECTED
};

struct chg_data {
	struct device			*dev;
	struct max8997_dev		*max8997;
	struct power_supply		psy_bat;
	struct max8997_power_data	*power;
	int				irq_topoff;
	int				irq_chgins;
	int				irq_chgrm;
#if defined(CONFIG_USB_GADGET) && defined(CONFIG_S3C_ADC)
	struct s3c_adc_client *client;
#endif
	bool is_usb_insert;
};

static enum power_supply_property max8997_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

/* vf check */
static bool max8997_check_detbat(struct chg_data *chg)
{
	struct i2c_client *i2c = chg->max8997->i2c;
	u8 data = 0;
	int ret;

	ret = max8997_read_reg(i2c, MAX8997_REG_STATUS4, &data);

	if (ret < 0) {
		dev_err(chg->dev, "%s: max8997_read_reg error(%d)\n", __func__,
				ret);
		return ret;
	}

	if (data & DETBAT_MASK)
		printk("%s: batt not detected(0x%x)\n", __func__, data);

	return data & DETBAT_MASK;
}

/* whether charging enabled or not */
static bool max8997_check_vdcin(struct chg_data *chg)
{
	struct i2c_client *i2c = chg->max8997->i2c;
	u8 data = 0;
	int ret;

	ret = max8997_read_reg(i2c, MAX8997_REG_STATUS4, &data);

	if (ret < 0) {
		dev_err(chg->dev, "%s: max8997_read_reg error(%d)\n", __func__,
				ret);
		return ret;
	}

	return data & DCINOK_MASK;
}
#ifdef CONFIG_MHL_DRIVER
extern bool mhl_cable_status(void);
#else
static inline bool mhl_cable_status(void) {
	return 0;
}
#endif

#if defined(CONFIG_USB_GADGET) && defined(CONFIG_S3C_ADC)
static int get_charger_type(struct chg_data *chg)
{
	int insert_type;
	int read_count = 0;
	int adc_value;
		
	if (!chg->is_usb_insert) {
		dev_info(chg->dev, "remove...\n");
		return POWER_SUPPLY_TYPE_BATTERY;
	}
	if(mhl_cable_status()) {
		dev_info(chg->dev, "MHL insert, we treat it as AC insert\n");
		return POWER_SUPPLY_TYPE_MISC;
	} 

	while(read_count++ < MAX_ADC_READ_COUNT) {
		gpio_direction_output(chg->power->vbus_gpio, 1);
		msleep(10);
		adc_value = s3c_adc_read(chg->client, chg->power->adc_ch);
		dev_info(chg->dev, "-----Read ADC %d\n", adc_value);
		gpio_direction_input(chg->power->vbus_gpio);

		if (unlikely( adc_value < 0)) {
			dev_err(chg->dev, "Read ADC channel %d failed, count %d\n", chg->power->adc_ch, read_count);
			insert_type = -1;
			msleep(10);
			continue;
		} else {
			if(adc_value <= 128) { // usb insert
				dev_info(chg->dev, "usb insert\n");
				insert_type = POWER_SUPPLY_TYPE_USB;
				break;
			}else if( 640 <= adc_value && adc_value <= 1000 ) { // standard ac insert
				dev_info(chg->dev, "ac insert\n");
				insert_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			} else {
				dev_info(chg->dev, "error state, adc value is %d, count %d\n", adc_value, read_count);
				insert_type = -1;
				msleep(10);
				if(read_count< MAX_ADC_READ_COUNT)
					continue;
				else {
					dev_info(chg->dev, "treat as ac insert\n");
					insert_type = POWER_SUPPLY_TYPE_MAINS;
					break;
				}
			}
		}
	}
	return insert_type;
}
#endif

static int max8997_chg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(psy, struct chg_data, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		chg->is_usb_insert = max8997_check_vdcin(chg);
		if (chg->is_usb_insert) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
#if defined(CONFIG_USB_GADGET) && defined(CONFIG_S3C_ADC)
		charger_work_info.charger_type = get_charger_type(chg);
#else
		charger_work_info.charger_type = chg->is_usb_insert ? POWER_SUPPLY_TYPE_USB : POWER_SUPPLY_TYPE_BATTERY;
#endif
		schedule_delayed_work(&charger_work_info.usb_attach_work, 0);
		max8997_power_set_charger(charger_work_info.charger_type);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (max8997_check_detbat(chg))
			val->intval = BAT_NOT_DETECTED;
		else
			val->intval = BAT_DETECTED;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		val->intval = 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max8997_disable_charging(struct chg_data *chg)
{
	struct i2c_client *i2c = chg->max8997->i2c;
	int ret;
	u8 mask;

	dev_info(chg->dev, "%s: disable charging\n", __func__);
	mask = MBCHOSTEN_MASK | VCHGR_FC_MASK;
	ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL2, 0, mask);
	if (ret < 0)
		dev_err(chg->dev, "%s: fail update reg!!!\n", __func__);

	return ret;
}

static int max8997_set_charging_current(struct chg_data *chg, int chg_current)
{
	struct i2c_client *i2c = chg->max8997->i2c;
	int ret;
	u8 val;

	if (chg_current < 200 || chg_current > 950)
		return -EINVAL;

	val = ((chg_current - 200) / 50) & 0xf;

	dev_info(chg->dev, "%s: charging current=%d\n", __func__, chg_current);
	ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL4,
		(val << MBCICHFC_SHIFT) | MBCICHFCSET , MBCCTRL4_MASK);
	if (ret)
		dev_err(chg->dev, "%s: fail to write chg current(%d)\n",
				__func__, ret);

	return ret;
}

static int max8997_enable_charging(struct chg_data *chg)
{
	struct i2c_client *i2c = chg->max8997->i2c;
	int ret;
	u8 val, mask;

	/* set auto stop disable */
	ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL6,
			(0 << AUTOSTOP_SHIFT), AUTOSTOP_MASK);
	if (ret)
		dev_err(chg->dev, "%s: failt to disable autostop(%d)\n",
				__func__, ret);

	/* set fast charging enable and main battery charging enable */
	val = (1 << MBCHOSTEN_SHIFT) | (1 << VCHGR_FC_SHIFT);
	mask = MBCHOSTEN_MASK | VCHGR_FC_MASK;
	ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL2, val, mask);
	if (ret)
		dev_err(chg->dev, "%s: failt to enable charging(%d)\n",
				__func__, ret);

	return ret;
}

static int max8997_chg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	int ret, reg_val;
	struct chg_data *chg = container_of(psy, struct chg_data, psy_bat);
	struct i2c_client *i2c = chg->max8997->i2c;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW: /* Set charging current */
		ret = max8997_set_charging_current(chg, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:	/* Enable/Disable charging */
		if (val->intval == POWER_SUPPLY_STATUS_CHARGING)
			ret = max8997_enable_charging(chg);
		else
			ret = max8997_disable_charging(chg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL: /* Set recharging current */
		if (val->intval < 50 || val->intval > 200) {
			dev_err(chg->dev, "%s: invalid topoff current(%d)\n",
					__func__, val->intval);
			return -EINVAL;
		}
		reg_val = (val->intval - 50) / 10;

		dev_info(chg->dev, "%s: Set toppoff current to 0x%x\n",
				__func__, reg_val);
		ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL5,
				(reg_val << ITOPOFF_SHIFT), ITOPOFF_MASK);
		if (ret) {
			dev_err(chg->dev, "%s: max8997 update reg error(%d)\n",
					__func__, ret);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static irqreturn_t max8997_chg_topoff_irq(int irq, void *data)
{
	charger_topoff_cb();
	printk("Topoff IRQ occurred!\n");
	return IRQ_HANDLED;
}

static irqreturn_t max8997_chg_charger_irq(int irq, void *data)
{
	struct chg_data *chg = data;

#if defined(CONFIG_USB_GADGET) && defined(CONFIG_S3C_ADC)
	chg->is_usb_insert = (irq == chg->irq_chgins);
	charger_work_info.charger_type = get_charger_type(chg);
#else
	if(irq == chg->irq_chgins) {
		charger_work_info.charger_type = POWER_SUPPLY_TYPE_USB;
	} else {
		charger_work_info.charger_type = POWER_SUPPLY_TYPE_BATTERY;
	}
#endif
	schedule_delayed_work(&charger_work_info.usb_attach_work, 0);
	max8997_power_set_charger(charger_work_info.charger_type);

	return IRQ_HANDLED;
}

static __devinit int max8997_charger_probe(struct platform_device *pdev)
{
	struct max8997_dev *max8997 = dev_get_drvdata(pdev->dev.parent);
	struct max8997_platform_data *pdata = dev_get_platdata(max8997->dev);
	struct i2c_client *i2c = max8997->i2c;
	struct chg_data *chg;
	int ret = 0;

	chg = kzalloc(sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->dev = &pdev->dev;
	chg->max8997 = max8997;
	chg->power = pdata->power;
	chg->irq_topoff = max8997->irq_base + MAX8997_PMICIRQ_TOPOFFR;
	chg->irq_chgins = max8997->irq_base + MAX8997_PMICIRQ_CHGINS;
	chg->irq_chgrm = max8997->irq_base + MAX8997_PMICIRQ_CHGRM;

	chg->psy_bat.name = "max8997-charger",
	chg->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	chg->psy_bat.properties = max8997_battery_props,
	chg->psy_bat.num_properties = ARRAY_SIZE(max8997_battery_props),
	chg->psy_bat.get_property = max8997_chg_get_property,
	chg->psy_bat.set_property = max8997_chg_set_property,

	platform_set_drvdata(pdev, chg);

	/* TODO: configure by platform data*/
	ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL1, /* Disable */
		(0x7 << TFCH_SHIFT), TFCH_MASK);
	if (ret < 0)
		goto err_kfree;

	/* TODO: configure by platform data*/
	ret = max8997_update_reg(i2c, MAX8997_REG_MBCCTRL3, /* 4.2V */
		(0x0 << MBCCV_SHIFT), MBCCV_MASK);
	if (ret < 0)
		goto err_kfree;

#if defined(CONFIG_USB_GADGET) && defined(CONFIG_S3C_ADC)
	ret = gpio_request(chg->power->vbus_gpio, "vbus_gpio");
	chg->client = s3c_adc_register(pdev, NULL, NULL, 0);
	if (IS_ERR(chg->client)) {
		dev_err(&pdev->dev, "cannot register adc\n");
		ret = PTR_ERR(chg->client);
		goto err_kfree;
	}
#endif
	INIT_DELAYED_WORK_DEFERRABLE(&charger_work_info.usb_attach_work, usb_attach_work);

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &chg->psy_bat);
	if (ret) {
		pr_err("Failed to register power supply psy_bat\n");
		goto err_kfree;
	}

	ret = request_threaded_irq(chg->irq_topoff, NULL,
			max8997_chg_topoff_irq, 0, "chg-topoff", chg);
	if (ret < 0)
		dev_err(&pdev->dev, "%s: fail to request topoff IRQ: %d: %d\n",
				__func__, chg->irq_topoff, ret);

	ret = request_threaded_irq(chg->irq_chgins, NULL,
			max8997_chg_charger_irq, 0, "chg-insert", chg);
	if (ret < 0)
		dev_err(&pdev->dev, "%s: fail to request chgins IRQ: %d: %d\n",
				__func__, chg->irq_chgins, ret);

	ret = request_threaded_irq(chg->irq_chgrm, NULL,
			max8997_chg_charger_irq, 0, "chg-remove", chg);
	if (ret < 0)
		dev_err(&pdev->dev, "%s: fail to request chgrm IRQ: %d: %d\n",
				__func__, chg->irq_chgrm, ret);
	dev_info(&pdev->dev, "%s : MAX8997 Charger Driver Load Success!\n", __func__);
	return 0;

err_kfree:
	kfree(chg);
	return ret;
}

static int __devexit max8997_charger_remove(struct platform_device *pdev)
{
	struct chg_data *chg = platform_get_drvdata(pdev);

	power_supply_unregister(&chg->psy_bat);
#if defined(CONFIG_USB_GADGET) && defined(CONFIG_S3C_ADC)
	gpio_free(chg->power->vbus_gpio);
	s3c_adc_release(chg->client);
#endif

	kfree(chg);

	return 0;
}

static int max8997_charger_suspend(struct device *dev)
{
	return 0;
}

static int max8997_charger_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops max8997_charger_pm_ops = {
	.suspend	= max8997_charger_suspend,
	.resume		= max8997_charger_resume,
};

static struct platform_driver max8997_charger_driver = {
	.driver = {
		.name = "max8997-charger",
		.owner = THIS_MODULE,
		.pm = &max8997_charger_pm_ops,
	},
	.probe = max8997_charger_probe,
	.remove = __devexit_p(max8997_charger_remove),
};

static int __init max8997_charger_init(void)
{
	return platform_driver_register(&max8997_charger_driver);
}

static void __exit max8997_charger_exit(void)
{
	platform_driver_register(&max8997_charger_driver);
}

module_init(max8997_charger_init);
module_exit(max8997_charger_exit);

MODULE_DESCRIPTION("MAXIM 8997 charger control driver");
MODULE_AUTHOR("<ms925.kim@samsung.com>");
MODULE_LICENSE("GPL");

