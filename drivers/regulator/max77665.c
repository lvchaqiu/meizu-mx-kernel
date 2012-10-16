/*
 * max77665.c - Regulator driver for the Maxim 77665
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 * Author:  Lvcha qiu <lvcha@meizu.com>
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
 * This driver is based on max8997.c
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max77665.h>
#include <linux/mfd/max77665-private.h>

struct max77665_data {
	struct device *dev;
	struct max77665_dev *iodev;
	int num_regulators;
	struct regulator_dev **rdev;

	u8 saved_states[MAX77665_REG_MAX];
};

struct voltage_map_desc {
	int min;
	int max;
	int step;
	unsigned int n_bits;
};

/* current map in uA */
static const struct voltage_map_desc charger_current_map_desc = {
	.min = 60000, .max = 2580000, .step = 20000, .n_bits = 7,
};

static const struct voltage_map_desc topoff_current_map_desc = {
	.min = 50000, .max = 200000, .step = 10000, .n_bits = 4,
};

/*if both leds are enabled, each led's max current is 625000uA*/
static const struct voltage_map_desc flash_led_current_map_desc = {
	.min = 15625, .max = 625000, .step = 15625, .n_bits = 6,
};

static const struct voltage_map_desc torch_led_current_map_desc = {
	.min = 15625, .max = 250000, .step = 15625, .n_bits = 4,
};

static const struct voltage_map_desc *reg_voltage_map[] = {
	[MAX77665_ESAFEOUT1] = NULL,
	[MAX77665_ESAFEOUT2] = NULL,
	[MAX77665_CHARGER] = &charger_current_map_desc,
	[MAX77665_FLASH_LED] = &flash_led_current_map_desc,
	[MAX77665_TORCH_LED] = &torch_led_current_map_desc,
};

/*
  * clear flash led interrupts by reading 0x0e interrupt register
  * when error happened and don't clear interrupt, the led can not be used
 */
int max77665_clear_interrupts(struct i2c_client *i2c)
{
	u8 val;
	int ret;
	ret = max77665_read_reg(i2c, 0x0e, &val);
	if (!ret && val) pr_info("%s:flash error happened:0x%x\n", __func__, val);

	return ret;
}

static int max77665_list_voltage_safeout(struct regulator_dev *rdev,
					 unsigned int selector)
{
	int rid = rdev_get_id(rdev);

	if (rid == MAX77665_ESAFEOUT1 || rid == MAX77665_ESAFEOUT2) {
		switch (selector) {
		case 0:
			return 4850000;
		case 1:
			return 4900000;
		case 2:
			return 4950000;
		case 3:
			return 3300000;
		default:
			return -EINVAL;
		}
	}

	return -EINVAL;
}

static int max77665_get_enable_register(struct regulator_dev *rdev,
					int *reg, int *mask, int *pattern)
{
	int rid = rdev_get_id(rdev);

	switch (rid) {
	case MAX77665_ESAFEOUT1 ... MAX77665_ESAFEOUT2:
		*reg = MAX77665_CHG_REG_SAFEOUT_CTRL;
		*mask = 0x40 << (rid - MAX77665_ESAFEOUT1);
		*pattern = 0x40 << (rid - MAX77665_ESAFEOUT1);
		break;
	case MAX77665_CHARGER:
		*reg = MAX77665_CHG_REG_CHG_CNFG_00;
		*mask = 0xF;
		*pattern = 0x5;
		break;
	case MAX77665_FLASH_LED:
		*reg = MAX77665_LED_REG_FLASH_EN;
		*mask = MAX77665_FLASH_ENABLE_MASK << MAX77665_FLASH_ENABLE_SHIFT;
		*pattern = MAX77665_FLASH_ENABLE_MASK << MAX77665_FLASH_ENABLE_SHIFT;
		break;
	case MAX77665_TORCH_LED:
		*reg = MAX77665_LED_REG_FLASH_EN;
		*mask = MAX77665_TORCH_ENABLE_MASK << MAX77665_TORCH_ENABLE_SHIFT;
		*pattern = MAX77665_TORCH_ENABLE_MASK << MAX77665_TORCH_ENABLE_SHIFT;
		break;
	case MAX77665_REVERSE:
		*reg = MAX77665_CHG_REG_CHG_CNFG_00;
		*mask = 0xF;
		*pattern = 0xA;
		break;
	default:
		/* Not controllable or not exists */
		return -EINVAL;
	}

	return 0;
}

static int max77665_reg_is_enabled(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret, reg, mask, pattern;
	u8 val;

	ret = max77665_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret == -EINVAL)
		return 1;	/* "not controllable" */
	else if (ret)
		return ret;

	ret = max77665_read_reg(i2c, reg, &val);
	if (ret)
		return ret;

	return (val & mask) == pattern;
}

static int max77665_reg_enable(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret, reg, mask, pattern;

	ret = max77665_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77665_update_reg(i2c, reg, pattern, mask);
}

static int max77665_reg_disable(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret, reg, mask, pattern;
	int rid = rdev_get_id(rdev);

	if (rid == MAX77665_FLASH_LED || rid == MAX77665_TORCH_LED) {
		ret = max77665_clear_interrupts(i2c);
		if (ret) return ret;
	}

	ret = max77665_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	return max77665_update_reg(i2c, reg, ~pattern, mask);
}

static int max77665_charger_reg_enable(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret = 0;

	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_00, 0x05, 0xF);

	return ret;
}

static int max77665_charger_reg_disable(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret = 0;

	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_00, 0x04, 0xF);

	return ret;
}

static int max77665_reverse_reg_enable(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret = 0;
	printk("#### %s\n", __func__);

	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_00, 0x0A, 0xF);

	return ret;
}

static int max77665_reverse_reg_disable(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret = 0;
	printk("#### %s\n", __func__);

	ret = max77665_update_reg(i2c, MAX77665_CHG_REG_CHG_CNFG_00, 0x04, 0xF);

	return ret;
}

static int max77665_get_voltage_register(struct regulator_dev *rdev,
					 int *_reg, int *_shift, int *_mask)
{
	int rid = rdev_get_id(rdev);
	int reg, shift = 0, mask = 0x3f;

	switch (rid) {
	case MAX77665_ESAFEOUT1 ... MAX77665_ESAFEOUT2:
		reg = MAX77665_CHG_REG_SAFEOUT_CTRL;
		shift = (rid == MAX77665_ESAFEOUT2) ? 2 : 0;
		mask = 0x3;
		break;
	case MAX77665_CHARGER:
		reg = MAX77665_CHG_REG_CHG_CNFG_09;
		shift = 0;
		mask = 0x7f;
		break;
	case MAX77665_FLASH_LED:
		reg = MAX77665_LED_REG_IFLASH1;
		shift = MAX77665_FLASH1_CURRENT_SHIFT;
		mask = MAX77665_FLASH1_CURRENT_MASK;
		break;
	case MAX77665_TORCH_LED:
		reg = MAX77665_LED_REG_ITORCH;
		shift = MAX77665_TORCH1_CURRENT_SHIFT;
		mask = MAX77665_TORCH1_CURRENT_MASK;
		break;
	default:
		return -EINVAL;
	}

	*_reg = reg;
	*_shift = shift;
	*_mask = mask;

	return 0;
}

static int max77665_list_voltage(struct regulator_dev *rdev,
				 unsigned int selector)
{
	const struct voltage_map_desc *desc;
	int rid = rdev_get_id(rdev);
	int val;

	if (rid >= ARRAY_SIZE(reg_voltage_map) || rid < 0)
		return -EINVAL;

	desc = reg_voltage_map[rid];
	if (desc == NULL)
		return -EINVAL;

	/* the first four codes for charger current are all 60mA */
	if (rid == MAX77665_CHARGER) {
		if (selector <= 3)
			selector = 0;
		else
			selector -= 3;
	}

	val = desc->min + desc->step * selector;
	if (val > desc->max)
		return -EINVAL;

	return val;
}

static int max77665_get_voltage(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int reg, shift, mask, ret;
	u8 val;

	ret = max77665_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77665_read_reg(i2c, reg, &val);
	if (ret)
		return ret;

	val >>= shift;
	val &= mask;

	if (rdev->desc && rdev->desc->ops && rdev->desc->ops->list_voltage)
		return rdev->desc->ops->list_voltage(rdev, val);

	/*
	 * max77665_list_voltage returns value for any rdev with voltage_map,
	 * which works for "CHARGER" and "CHARGER TOPOFF" that do not have
	 * list_voltage ops (they are current regulators).
	 */
	return max77665_list_voltage(rdev, val);
}

static inline int max77665_get_voltage_proper_val(
		const struct voltage_map_desc *desc,
		int min_vol, int max_vol)
{
	int i = 0;

	if (desc == NULL)
		return -EINVAL;

	if (max_vol < desc->min || min_vol > desc->max)
		return -EINVAL;

	while (desc->min + desc->step * i < min_vol &&
			desc->min + desc->step * i < desc->max)
		i++;

	if (desc->min + desc->step * i > max_vol)
		return -EINVAL;

	if (i >= (1 << desc->n_bits))
		return -EINVAL;

	return i;
}

/*set flash or torch current, here we set 2 leds' current*/
static int max77665_flash_set_current(struct regulator_dev *rdev, int cur)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int i, ret, rid = rdev_get_id(rdev);
	u8 reg[2], shift[2], mask[2];  /*led1 and led2 parameters*/

	switch (rid) {
	case MAX77665_FLASH_LED:
		reg[0] = MAX77665_LED_REG_IFLASH1;
		shift[0] = MAX77665_FLASH1_CURRENT_SHIFT;
		mask[0] = MAX77665_FLASH1_CURRENT_MASK;
		reg[1] = MAX77665_LED_REG_IFLASH2;
		shift[1] = MAX77665_FLASH2_CURRENT_SHIFT;
		mask[1] = MAX77665_FLASH2_CURRENT_MASK;
		break;
	case MAX77665_TORCH_LED:
		reg[0] = MAX77665_LED_REG_ITORCH;
		shift[0] = MAX77665_TORCH1_CURRENT_SHIFT;
		mask[0] = MAX77665_TORCH1_CURRENT_MASK;
		reg[1] = MAX77665_LED_REG_ITORCH;
		shift[1] = MAX77665_TORCH2_CURRENT_SHIFT;
		mask[1] = MAX77665_TORCH2_CURRENT_MASK;
		break;
	default:
		return -EINVAL;
	}

	/*set 2 led current*/
	for (i = 0; i < 2; i++) {
		ret = max77665_update_reg(i2c, reg[i], cur << shift[i], mask[i] << shift[i]);
		if (ret)
			return ret;
	}
	
	return ret;
}

static int max77665_set_current(struct regulator_dev *rdev,
				int min_uV, int max_uV)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int min_vol = min_uV, max_vol = max_uV;
	const struct voltage_map_desc *desc;
	int rid = rdev_get_id(rdev);
	int reg, shift = 0, mask, ret;
	int i;

	desc = reg_voltage_map[rid];

	i = max77665_get_voltage_proper_val(desc, min_vol, max_vol);
	if (i < 0)
		return i;

	switch (rid) {
	case MAX77665_CHARGER:
		/* the first four codes for charger current are all 60mA */
		i += 3;

		ret = max77665_get_voltage_register(rdev, &reg, &shift, &mask);
		if (ret)
			return ret;

		ret = max77665_update_reg(i2c, reg, i << shift, mask << shift);
		break;
	case MAX77665_FLASH_LED:
	case MAX77665_TORCH_LED:
		ret = max77665_flash_set_current(rdev, i);
		break;
	default:
		ret = -EINVAL;
	}
	
	return ret;
}

static const int safeoutvolt[] = {
	3300000,
	4850000,
	4900000,
	4950000,
};

/* For SAFEOUT1 and SAFEOUT2 */
static int max77665_set_voltage_safeout(struct regulator_dev *rdev,
					int min_uV, int max_uV,
					unsigned *selector)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int rid = rdev_get_id(rdev);
	int reg, shift = 0, mask, ret;
	int i = 0;
	u8 val;

	if (rid != MAX77665_ESAFEOUT1 && rid != MAX77665_ESAFEOUT2)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(safeoutvolt); i++) {
		if (min_uV <= safeoutvolt[i] && max_uV >= safeoutvolt[i])
			break;
	}

	if (i >= ARRAY_SIZE(safeoutvolt))
		return -EINVAL;

	if (i == 0)
		val = 0x3;
	else
		val = i - 1;

	ret = max77665_get_voltage_register(rdev, &reg, &shift, &mask);
	if (ret)
		return ret;

	ret = max77665_update_reg(i2c, reg, val << shift, mask << shift);
	*selector = val;

	return ret;
}

static int max77665_reg_do_nothing(struct regulator_dev *rdev)
{
	return 0;
}

static int max77665_reg_disable_suspend(struct regulator_dev *rdev)
{
	struct max77665_data *max77665 = rdev_get_drvdata(rdev);
	struct i2c_client *i2c = max77665->iodev->i2c;
	int ret, reg, mask, pattern;
	int rid = rdev_get_id(rdev);
	
	ret = max77665_get_enable_register(rdev, &reg, &mask, &pattern);
	if (ret)
		return ret;

	max77665_read_reg(i2c, reg, &max77665->saved_states[rid]);

	dev_dbg(&rdev->dev, "Full Power-Off for %s (%xh -> %xh)\n",
		rdev->desc->name, max77665->saved_states[rid] & mask,
		(~pattern) & mask);
	return max77665_update_reg(i2c, reg, ~pattern, mask);
}

static struct regulator_ops max77665_safeout_ops = {
	.list_voltage		= max77665_list_voltage_safeout,
	.is_enabled		= max77665_reg_is_enabled,
	.enable			= max77665_reg_enable,
	.disable			= max77665_reg_disable,
	.get_voltage		= max77665_get_voltage,
	.set_voltage		= max77665_set_voltage_safeout,
	.set_suspend_enable	= max77665_reg_do_nothing,
	.set_suspend_disable	= max77665_reg_disable_suspend,
};

static struct regulator_ops max77665_charger_ops = {
	.is_enabled		= max77665_reg_is_enabled,
	.enable			= max77665_charger_reg_enable,
	.disable			= max77665_charger_reg_disable,
	.get_current_limit	= max77665_get_voltage,
	.set_current_limit	= max77665_set_current,
	.set_suspend_enable	= max77665_reg_do_nothing,
	.set_suspend_disable	= max77665_reg_do_nothing,
};

static struct regulator_ops max77665_flash_led_ops = {
	.is_enabled		= max77665_reg_is_enabled,
	.enable			= max77665_reg_enable,
	.disable			= max77665_reg_disable,
	.get_current_limit	= max77665_get_voltage,
	.set_current_limit	= max77665_set_current,
	.set_suspend_enable	= max77665_reg_do_nothing,
	.set_suspend_disable	= max77665_reg_do_nothing,
};

static struct regulator_ops max77665_torch_led_ops = {
	.is_enabled		= max77665_reg_is_enabled,
	.enable			= max77665_reg_enable,
	.disable			= max77665_reg_disable,
	.get_current_limit	= max77665_get_voltage,
	.set_current_limit	= max77665_set_current,
	.set_suspend_enable	= max77665_reg_do_nothing,
	.set_suspend_disable	= max77665_reg_do_nothing,
};

static struct regulator_ops max77665_reverse_ops = {
	.is_enabled		= max77665_reg_is_enabled,
	.enable			= max77665_reverse_reg_enable,
	.disable		= max77665_reverse_reg_disable,
	.set_suspend_enable	= max77665_reg_do_nothing,
	.set_suspend_disable	= max77665_reg_do_nothing,
};

static struct regulator_desc regulators[] = {
	{
		.name = "ESAFEOUT1",
		.id = MAX77665_ESAFEOUT1,
		.ops = &max77665_safeout_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}, {
		.name = "ESAFEOUT2",
		.id = MAX77665_ESAFEOUT2,
		.ops = &max77665_safeout_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}, {
		.name = "CHARGER",
		.id = MAX77665_CHARGER,
		.ops = &max77665_charger_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	}, {
		.name = "FLASH LED",
		.id = MAX77665_FLASH_LED,
		.ops = &max77665_flash_led_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	}, {
		.name = "TORCH LED",
		.id = MAX77665_TORCH_LED,
		.ops = &max77665_torch_led_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	}, {
		.name = "REVERSE",
		.id = MAX77665_REVERSE,
		.ops = &max77665_reverse_ops,
		.type = REGULATOR_CURRENT,
		.owner = THIS_MODULE,
	}
};

static __devinit int max77665_pmic_probe(struct platform_device *pdev)
{
	struct max77665_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77665_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct regulator_dev **rdev;
	struct max77665_data *max77665;
	struct i2c_client *i2c;
	int i, ret, size;

	if (!pdata) {
		pr_info("[%s:%d] !pdata\n", __FILE__, __LINE__);
		dev_err(pdev->dev.parent, "No platform init data supplied.\n");
		return -ENODEV;
	}

	max77665 = kzalloc(sizeof(*max77665), GFP_KERNEL);
	if (!max77665) {
		pr_info("[%s:%d] if (!max77665)\n", __FILE__, __LINE__);
		return -ENOMEM;
	}
	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max77665->rdev = kzalloc(size, GFP_KERNEL);
	if (!max77665->rdev) {
		pr_info("[%s:%d] if (!max77665->rdev)\n", __FILE__, __LINE__);
		kfree(max77665);
		return -ENOMEM;
	}

	rdev = max77665->rdev;
	max77665->dev = &pdev->dev;
	max77665->iodev = iodev;
	max77665->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, max77665);
	i2c = max77665->iodev->i2c;
	pr_info("[%s:%d] pdata->num_regulators:%d\n", __FILE__, __LINE__,
		pdata->num_regulators);
	for (i = 0; i < pdata->num_regulators; i++) {

		const struct voltage_map_desc *desc;
		int id = pdata->regulators[i].id;
		pr_info("[%s:%d] for in pdata->num_regulators:%d\n", __FILE__,
			__LINE__, pdata->num_regulators);
		desc = reg_voltage_map[id];
		if (id == MAX77665_ESAFEOUT1 || id == MAX77665_ESAFEOUT2)
			regulators[id].n_voltages = 4;

		rdev[i] = regulator_register(&regulators[id], max77665->dev,
					     pdata->regulators[i].initdata,
					     max77665);
		if (IS_ERR(rdev[i])) {
			ret = PTR_ERR(rdev[i]);
			dev_err(max77665->dev, "regulator init failed for %d\n",
				id);
			rdev[i] = NULL;
			goto err;
		}
	}

#ifdef CONFIG_MX_SERIAL_TYPE
	/* No active discharge safeout1 and safeout2 */
	max77665_update_reg(i2c, MAX77665_CHG_REG_SAFEOUT_CTRL, 0x0, 0x3<<4);
	/* Disable safeout2 */
	max77665_update_reg(i2c, MAX77665_CHG_REG_SAFEOUT_CTRL, 0x0, 0x1<<7);
#endif

	return 0;
 err:
	pr_info("[%s:%d] err:\n", __FILE__, __LINE__);
	for (i = 0; i < max77665->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	platform_set_drvdata(pdev, NULL);
	kfree(max77665->rdev);
	kfree(max77665);
	return ret;
}

static int __devexit max77665_pmic_remove(struct platform_device *pdev)
{
	struct max77665_data *max77665 = platform_get_drvdata(pdev);
	struct regulator_dev **rdev = max77665->rdev;
	int i;

	for (i = 0; i < max77665->num_regulators; i++)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(max77665->rdev);
	kfree(max77665);

	return 0;
}

static const struct platform_device_id max77665_pmic_id[] = {
	{"max77665-safeout", 0},
	{},
};

MODULE_DEVICE_TABLE(platform, max77665_pmic_id);

static struct platform_driver max77665_pmic_driver = {
	.driver = {
		   .name = "max77665-safeout",
		   .owner = THIS_MODULE,
		   },
	.probe = max77665_pmic_probe,
	.remove = __devexit_p(max77665_pmic_remove),
	.id_table = max77665_pmic_id,
};

static int __init max77665_pmic_init(void)
{
	return platform_driver_register(&max77665_pmic_driver);
}

subsys_initcall(max77665_pmic_init);

static void __exit max77665_pmic_cleanup(void)
{
	platform_driver_unregister(&max77665_pmic_driver);
}

module_exit(max77665_pmic_cleanup);

MODULE_DESCRIPTION("MAXIM 77665 Regulator Driver");
MODULE_AUTHOR("Sukdong Kim <Sukdong.Kim@samsung.com>");
MODULE_LICENSE("GPLV2");
