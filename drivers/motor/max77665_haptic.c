/*
 *  MAX77665-haptic controller driver
 *
 *  Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *  Chwei <chwei@meizu.com>
 *
 * This program is not provided / owned by Maxim Integrated Products.
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
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max77665.h>
#include <linux/mfd/max77665-private.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/timed_output.h>
#include <linux/pwm.h>
#include <linux/clk.h>

#define HAPTIC_CONF2_PDIV_SHIFT		(0)
#define HAPTIC_CONF2_PDIV_MASK		(0x3<<HAPTIC_CONF2_PDIV_SHIFT)
#define HAPTIC_CONF2_HTYP_SHIFT		(5)
#define HAPTIC_CONF2_HTYP_MASK		(0x1<<HAPTIC_CONF2_HTYP_SHIFT)
#define HAPTIC_CONF2_MEN_SHIFT		(6)
#define HAPTIC_CONF2_MEN_MASK		(0x1<<HAPTIC_CONF2_MEN_SHIFT)
#define HAPTIC_CONF2_MODE_SHIFT		(7)
#define HAPTIC_CONF2_MODE_MASK		(0x1<<HAPTIC_CONF2_MODE_SHIFT)

#define MAX_TIMEOUT		25000
#define LSEN	(1<<7)	/* Low sys dac enable */
#define LSDEN	(0<<7)
#define LSEN_MASK	(1<<7)

struct haptic_data {
	char *name;
	struct device *dev;
	struct i2c_client *client;
	struct i2c_client *pmic;
	struct hrtimer          timer;
	struct mutex haptic_mutex;
	int max_timeout;
	struct work_struct motor_work;
	struct timed_output_dev tdev;
	struct pwm_device *pwm;
	u16 duty;
	u16 period;
};

static int haptic_clk_on(struct device *dev, bool en)
{
	struct clk *vibetonz_clk = NULL;

	vibetonz_clk = clk_get(dev, "timers");
	pr_debug("[VIB] DEV NAME %s %lu\n",
		 dev_name(dev), clk_get_rate(vibetonz_clk));

	if (IS_ERR(vibetonz_clk)) {
		pr_err("[VIB] failed to get clock for the motor\n");
		goto err_clk_get;
	}

	if (en)
		clk_enable(vibetonz_clk);
	else
		clk_disable(vibetonz_clk);

	clk_put(vibetonz_clk);
	return 0;

err_clk_get:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

static int max77665_haptic_on(struct haptic_data *chip, bool en)
{
	int ret = 0;

	if (en) {
		ret = max77665_update_reg(chip->pmic, MAX77665_PMIC_REG_LSCNFG,
				LSEN, LSEN_MASK);
		ret = max77665_update_reg(chip->client,
					MAX77665_HAPTIC_REG_CONFIG2, 
					(0x1 << HAPTIC_CONF2_MEN_SHIFT),
					HAPTIC_CONF2_MEN_MASK);

		ret = pwm_config(chip->pwm, chip->duty,
			   chip->period);
		ret = pwm_enable(chip->pwm);

	} else {
		pwm_disable(chip->pwm);
		ret = max77665_update_reg(chip->client,
					MAX77665_HAPTIC_REG_CONFIG2, 
					(0x0 << HAPTIC_CONF2_MEN_SHIFT),
					HAPTIC_CONF2_MEN_MASK);
		ret = max77665_update_reg(chip->pmic, MAX77665_PMIC_REG_LSCNFG,
				LSDEN, LSEN_MASK);
	}
	return ret;
}

static int max77665_haptic_disable(struct i2c_client *i2c)
{
	int ret = 0;
	ret = max77665_update_reg(i2c,MAX77665_HAPTIC_REG_CONFIG2,
			(0x0<<HAPTIC_CONF2_MEN_SHIFT),HAPTIC_CONF2_MEN_MASK);
	if(ret < 0 )
		goto err;

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static void motor_work_func(struct work_struct *work)
{
	struct haptic_data *chip = container_of(work, struct haptic_data, motor_work);

	max77665_haptic_disable(chip->client);
}

static enum hrtimer_restart motor_timer_func(struct hrtimer *timer)
{
	struct haptic_data *chip = container_of(timer, struct haptic_data, timer);

	schedule_work(&chip->motor_work);

	return HRTIMER_NORESTART;
}

static int haptic_get_time(struct timed_output_dev *tdev)
{
	struct haptic_data *chip =
		container_of(tdev, struct haptic_data, tdev);

	if (hrtimer_active(&chip->timer)) {
		ktime_t r = hrtimer_get_remaining(&chip->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;	
}

static void haptic_enable(struct timed_output_dev *tdev, int value)
{
	struct haptic_data *chip =
		container_of(tdev, struct haptic_data, tdev);

	mutex_lock(&chip->haptic_mutex);

	hrtimer_cancel(&chip->timer);
	if (value > 0) {
		value = min(value, chip->max_timeout);
		hrtimer_start(&chip->timer, ktime_set(value/1000, (value%1000)*1000000),
						HRTIMER_MODE_REL);
	} 

	max77665_haptic_on(chip, !!value);

	mutex_unlock(&chip->haptic_mutex);
}

static __devinit int max77665_haptic_probe(struct platform_device *pdev)
{
	struct max77665_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77665_platform_data *max77665_pdata
		= dev_get_platdata(iodev->dev);
	struct max77665_haptic_platform_data *pdata
		= max77665_pdata->haptic_pdata;
	struct haptic_data *chip;
	u8 config = 0;
	int ret = 0;

	dev_info(&pdev->dev, "%s : MAX77665 Haptic Driver Loading\n", __func__);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->client = iodev->haptic;
	chip->pmic = iodev->i2c;
	chip->period = pdata->pwm_period;
	chip->duty = pdata->pwm_duty;

	hrtimer_init(&chip->timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	chip->timer.function = motor_timer_func;

	mutex_init(&chip->haptic_mutex);
	INIT_WORK(&chip->motor_work, motor_work_func);
	chip->pwm = pwm_request(pdata->pwm_channel_id, "vibrator");
	if (IS_ERR(chip->pwm)) {
		pr_err("[VIB] Failed to request pwm\n");
		ret = -EFAULT;
		goto err_pwm;
	}
	pwm_config(chip->pwm, chip->period / 2, chip->period);

	/* max77693_haptic_init */
	if (pdata->type == MAX77665_HAPTIC_LRA)
		config |= 1<<7;
	if (pdata->mode == MAX77665_INTERNAL_MODE)
		config |= 1<<5;
	config |= pdata->pwm_divisor;
	ret = max77665_write_reg(chip->client,
				 MAX77665_HAPTIC_REG_CONFIG2, config);
	/* init done */
	haptic_clk_on(chip->dev, true);

	chip->name = "vibrator";
	chip->max_timeout = MAX_TIMEOUT;
	chip->tdev.name = chip->name;
	chip->tdev.get_time = haptic_get_time;
	chip->tdev.enable = haptic_enable;
	ret = timed_output_dev_register(&chip->tdev);
	if (ret < 0) {
		pr_err("[VIB] Failed to register timed_output : %d\n", ret);
		ret = -EFAULT;
		goto err_timed_output;
	}	

	platform_set_drvdata(pdev, chip);

	return 0;

err_timed_output:
	pwm_free(chip->pwm);
err_pwm:
	INIT_WORK(&chip->motor_work, NULL);
	kfree(chip);
	return ret;
}

static int __devexit max77665_haptic_remove(struct platform_device *pdev)
{
	struct haptic_data *chip = platform_get_drvdata(pdev);

	mutex_destroy(&chip->haptic_mutex);
	platform_set_drvdata(pdev, NULL);
	hrtimer_cancel(&chip->timer);
	timed_output_dev_unregister(&chip->tdev);

	if(chip)
		kfree(chip);
	return 0;
}

static int max77665_haptic_suspend(struct device *dev)
{
	struct haptic_data *chip = dev_get_drvdata(dev);
	hrtimer_cancel(&chip->timer);
	max77665_haptic_on(chip, false);
	haptic_clk_on(chip->dev, false);
	return 0;
}

static int max77665_haptic_resume(struct device *dev)
{
	struct haptic_data *chip = dev_get_drvdata(dev);
	haptic_clk_on(chip->dev, true);
	return 0;
}

static const struct dev_pm_ops max77665_haptic_pm_ops = {
	.suspend	= max77665_haptic_suspend,
	.resume	= max77665_haptic_resume,
};

static struct platform_driver max77665_haptic_driver = {
	.driver = {
		.name = "max77665-haptic",
		.owner = THIS_MODULE,
		.pm = &max77665_haptic_pm_ops,
	},
	.probe = max77665_haptic_probe,
	.remove = __devexit_p(max77665_haptic_remove),
};

static int __init max77665_haptic_init(void)
{
	return platform_driver_register(&max77665_haptic_driver);
}

static void __exit max77665_haptic_exit(void)
{
	platform_driver_unregister(&max77665_haptic_driver);
}

module_init(max77665_haptic_init);
module_exit(max77665_haptic_exit);

MODULE_DESCRIPTION("MAXIM 77665 haptic control driver");
MODULE_AUTHOR("Chwei chwei@meizu.com");
MODULE_LICENSE("GPLV2");
