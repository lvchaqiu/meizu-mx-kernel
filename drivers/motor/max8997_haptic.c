/*
 *  max8997-haptic.c
 *  MAXIM 8997 haptic interface driver
 *
 *  Copyright (C) 2011 Meizu Tech Co.Ltd
 *
 *  <caoziqiang@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *  @date       2011-8-26
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
#include <linux/kdev_t.h>

#ifdef CONFIG_ANDROID_TIMED_OUTPUT
#include <linux/timed_output.h>
#endif

/*MAX8997 haptic control registers*/
#define HAPTIC_GEN_STATUS       	(0X00)  //Read-only
#define HAPTIC_CONF1			    (0X01)
#define HAPTIC_CONF2			    (0X02)
#define HAPTIC_CHANNEL		        (0X03)
#define HAPTIC_CYC1		            (0X04)
#define HAPTIC_CYC2		            (0X05)
#define HAPTIC_SIGP1		        (0X06)
#define HAPTIC_SIGP2		        (0X07)
#define HAPTIC_SIGP3		        (0X08)
#define HAPTIC_SIGP4		        (0X09)
#define HAPTIC_SIGDC1		        (0X0A)
#define HAPTIC_SIGDC2		        (0X0B)
#define HAPTIC_PWMDC1			    (0X0C)
#define HAPTIC_PWMDC2			    (0X0D)
#define HAPTIC_PWMDC3			    (0X0E)
#define HAPTIC_PWMDC4			    (0X0F)
#define HAPTIC_PART_REVISION_CODE	(0X10) //Read-only
/*reg shift & mask*/
#define HAPTIC_CONF1_SCF_SHIFT		(0)
#define HAPTIC_CONF1_SCF_MASK		(0X7<<HAPTIC_CONF1_SCF_SHIFT)
#define HAPTIC_CONF1_MSU_SHIFT		(3)
#define HAPTIC_CONF1_MSU_MASK		(0X7<<HAPTIC_CONF1_MSU_SHIFT)
#define HAPTIC_CONF1_CONT_SHIFT		(6)
#define HAPTIC_CONF1_CONT_MASK		(0X1<<HAPTIC_CONF1_CONT_SHIFT)
#define HAPTIC_CONF1_INV_SHIFT		(7)
#define HAPTIC_CONF1_INV_MASK		(0X1<<HAPTIC_CONF1_INV_SHIFT)

#define HAPTIC_CONF2_PDIV_SHIFT		(0)
#define HAPTIC_CONF2_PDIV_MASK		(0X3<<HAPTIC_CONF2_PDIV_SHIFT)
#define HAPTIC_CONF2_HTYP_SHIFT		(5)
#define HAPTIC_CONF2_HTYP_MASK		(0X1<<HAPTIC_CONF2_HTYP_SHIFT)
#define HAPTIC_CONF2_MEN_SHIFT		(6)
#define HAPTIC_CONF2_MEN_MASK		(0X1<<HAPTIC_CONF2_MEN_SHIFT)
#define HAPTIC_CONF2_MODE_SHIFT		(7)
#define HAPTIC_CONF2_MODE_MASK		(0X1<<HAPTIC_CONF2_MODE_SHIFT)

#define HAPTIC_CHANNEL_PWMDCA_SHIFT	(0)
#define HAPTIC_CHANNEL_PWMDCA_MASK	(0X3<<HAPTIC_CHANNEL_PWMDCA_SHIFT)
#define HAPTIC_CHANNEL_SIGDCA_SHIFT	(2)
#define HAPTIC_CHANNEL_SIGDCA_MASK	(0X3<<HAPTIC_CHANNEL_SIGDCA_SHIFT)
#define HAPTIC_CHANNEL_SIGPA_SHIFT	(4)
#define HAPTIC_CHANNEL_SIGPA_MASK	(0X3<<HAPTIC_CHANNEL_SIGPA_SHIFT)
#define HAPTIC_CHANNEL_CYCA_SHIFT	(6)
#define HAPTIC_CHANNEL_CYCA_MASK	(0X3<<HAPTIC_CHANNEL_CYCA_SHIFT)

#define HAPTIC_CYC0_SHIFT	(0)
#define HAPTIC_CYC0_MASK		(0XF<<HAPTIC_CYC0_SHIFT)
#define HAPTIC_CYC1_SHIFT	(4)
#define HAPTIC_CYC1_MASK		(0XF<<HAPTIC_CYC1_SHIFT)
#define HAPTIC_CYC2_SHIFT	(0)
#define HAPTIC_CYC2_MASK		(0XF<<HAPTIC_CYC2_SHIFT)
#define HAPTIC_CYC3_SHIFT	(4)
#define HAPTIC_CYC3_MASK		(0XF<<HAPTIC_CYC3_SHIFT)


#define HAPTIC_SIGP1_MASK		(0XFF)
#define HAPTIC_SIGP2_MASK		(0XFF)
#define HAPTIC_SIGP3_MASK		(0XFF)
#define HAPTIC_SIGP4_MASK		(0XFF)

#define HAPTIC_SIGDC0_SHIFT	(0)
#define HAPTIC_SIGDC0_MASK	(0XF<<HAPTIC_SIGDC0_SHIFT)
#define HAPTIC_SIGDC1_SHIFT	(4)
#define HAPTIC_SIGDC1_MASK	(0XF<<HAPTIC_SIGDC1_SHIFT)
#define HAPTIC_SIGDC2_SHIFT	(0)
#define HAPTIC_SIGDC2_MASK	(0XF<<HAPTIC_SIGDC2_SHIFT)
#define HAPTIC_SIGDC3_SHIFT	(4)
#define HAPTIC_SIGDC3_MASK	(0XF<<HAPTIC_SIGDC3_SHIFT)

#define HAPTIC_PWMDC0_MASK		(0X3F)
#define HAPTIC_PWMDC1_MASK		(0X3F)
#define HAPTIC_PWMDC2_MASK		(0X3F)
#define HAPTIC_PWMDC3_MASK		(0X3F)

#define HAPTIC_REG_NUM          0x11

#define MAX_TIMEOUT      2500

struct haptic_data {
	char *                  name;
	struct device		*dev;
	struct device           *localdev;
	struct max8997_dev	*max8997;
	struct class 		*motorclass;
	struct hrtimer          timer;
	struct spinlock         motor_lock;
	unsigned int            max_timeout;
	struct work_struct      motor_work;
	
#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	struct timed_output_dev tdev;
#endif
};

static int max8997_haptic_enable(struct i2c_client *i2c)
{
	int ret = 0;

	ret = max8997_update_reg(i2c, HAPTIC_CONF2,
			(0x1 << HAPTIC_CONF2_MEN_SHIFT), HAPTIC_CONF2_MEN_MASK);
	if(ret < 0 )
		goto err;
	
	ret = max8997_write_reg(i2c, HAPTIC_CHANNEL, 0x66);
	if(ret < 0 )
		goto err;

#if 0
	{
		int i = 0;
		u8 content;
		printk("======the haptic motor drvier register content start======\n");
		for(i = 0; i <= 0x10;i++){
			max8997_read_reg(i2c,i,&content);
			printk("addr (0x%02x) \tcontent is (0x%02x)\n",i,content);
		}
		printk("======the haptic motor drvier register content end======\n");
	}
#endif
	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static int max8997_haptic_disable(struct i2c_client *i2c)
{
	int ret = 0;
	ret = max8997_update_reg(i2c,HAPTIC_CONF2,
			(0X0<<HAPTIC_CONF2_MEN_SHIFT),HAPTIC_CONF2_MEN_MASK);
	if(ret < 0 )
		goto err;

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static void motor_work_func(struct work_struct *work)
{
	struct haptic_data *hdata =(struct haptic_data*)container_of(work, struct haptic_data, motor_work);

	max8997_haptic_disable(hdata->max8997->haptic);
}
static enum hrtimer_restart motor_timer_func(struct hrtimer *timer)
{
	struct haptic_data *hdata =(struct haptic_data*)container_of(timer, struct haptic_data, timer);

	schedule_work(&hdata->motor_work);

	return HRTIMER_NORESTART;
}
#ifndef CONFIG_ANDROID_TIMED_OUTPUT
static int motor_get_time(struct hrtimer *timer)
{
	if (hrtimer_active(timer)) {
		ktime_t r = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static ssize_t enable_show(struct device* dev,struct device_attribute *attr, char *buf)
{  
	struct haptic_data *hdata =(struct haptic_data*)dev_get_drvdata(dev);
	int remain_time = 0;

	remain_time = motor_get_time(&hdata->timer);
	return sprintf(buf,"%d\n",remain_time);
}

static ssize_t enable_store(struct device* dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct haptic_data *hdata = (struct haptic_data*)dev_get_drvdata(dev);
	int value = 0;
	unsigned long flag;

	if(sscanf(buf,"%d",&value)!=1)
		return -EINVAL;
	if(value>0)
		max8997_haptic_enable(hdata->max8997->haptic);
	else
		max8997_haptic_disable(hdata->max8997->haptic);

	spin_lock_irqsave(&hdata->motor_lock,flag);
	hrtimer_cancel(&hdata->timer);
	if(value){
		//msleep(value);
		//max8997_haptic_disable(hdata->max8997->haptic);
		if(value > hdata->max_timeout)
			value = hdata->max_timeout;

		hrtimer_start(&hdata->timer,
				ktime_set(value/1000,(value%1000)*1000000),HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&hdata->motor_lock,flag);
	
	return size;
}

static DEVICE_ATTR(enable,S_IRUGO|S_IWUGO,enable_show,enable_store);

static ssize_t regdump_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct haptic_data *hdata = (struct haptic_data*)dev_get_drvdata(dev);
	unsigned long index, len = 1;
	char index_str[33];
	char len_str[33] = "0x11";
	unsigned char val;
	int i, error;
	int ret;

	ret = sscanf(buf, "%s %s", index_str, len_str);
	if ((ret != 2) && (ret != 1)) {
		dev_err(dev, "Invalid values!\n");
		return -EINVAL;
	}

	if (strict_strtoul(index_str, 0, &index) < 0)
		return -EINVAL;;
	if (strict_strtoul(len_str, 0, &len) < 0)
		return -EINVAL;;
	
	if (index > HAPTIC_REG_NUM -1)
		index = HAPTIC_REG_NUM - 1;

	if (index + len > HAPTIC_REG_NUM)
		len = HAPTIC_REG_NUM + 1 - index;

	printk(KERN_ERR"dump reg index:%ld, len:%ld:\n", index, len);

	for (i = 0; i < len; i++) {
		error = max8997_read_reg(hdata->max8997->haptic,
				index + i, &val);
		if (error)
			return -EINVAL;
		if (i%10 == 0 && i != 0)
			printk(KERN_ERR"\n");
		printk(KERN_ERR"0x%02x ", val);
	}
	printk("\n");

	return count;
}

static DEVICE_ATTR(regdump, S_IRUGO | S_IWUGO, NULL, regdump_store);

static ssize_t regwrite_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct haptic_data *hdata = (struct haptic_data*)dev_get_drvdata(dev);
	unsigned long index, val;
	char index_str[33];
	char val_str[33];
	int ret;

	ret = sscanf(buf, "%s %s", index_str, val_str);
	if (ret != 2) {
		dev_err(dev, "Invalid!!Input format:reg_type index value\n");
		return -EINVAL;
	}
	if (strict_strtoul(index_str, 0, &index) < 0)
		return -EINVAL;;
	if (strict_strtoul(val_str, 0, &val) < 0)
		return -EINVAL;;

	printk(KERN_INFO"write reg:%ld, value:0x%lx\n", index, val);

	max8997_write_reg(hdata->max8997->haptic, index, val);

	return count;
}

static DEVICE_ATTR(regwrite, S_IRUGO | S_IWUGO, NULL, regwrite_store);
#endif
static int max8997_haptic_set_config(struct i2c_client *i2c)
{
	int ret = 0;

	ret = max8997_update_reg(i2c,HAPTIC_CONF1,
			(0<<HAPTIC_CONF1_SCF_SHIFT), HAPTIC_CONF1_SCF_MASK);
	if(ret < 0 )
		goto err_config;
	ret = max8997_update_reg(i2c,HAPTIC_CONF1,
			(1<<HAPTIC_CONF1_CONT_SHIFT),HAPTIC_CONF1_CONT_MASK);
	if(ret < 0 )
		goto err_config;
	ret = max8997_update_reg(i2c,HAPTIC_CONF1,
			(0<<HAPTIC_CONF1_MSU_SHIFT),HAPTIC_CONF1_MSU_MASK);
	if(ret < 0 )
		goto err_config;

	ret = max8997_update_reg(i2c,HAPTIC_CONF1,
			(1<<HAPTIC_CONF1_INV_SHIFT),HAPTIC_CONF1_INV_MASK);
	if(ret < 0 )
		goto err_config;

	ret = max8997_update_reg(i2c,HAPTIC_CONF2,
			(2<<HAPTIC_CONF2_PDIV_SHIFT), HAPTIC_CONF2_PDIV_MASK);
	if(ret < 0 )
		goto err_config;

	ret = max8997_update_reg(i2c,HAPTIC_CONF2,
			(0x0 << HAPTIC_CONF2_HTYP_SHIFT), HAPTIC_CONF2_HTYP_MASK);
	if(ret < 0 )
		goto err_config;

	ret = max8997_update_reg(i2c,HAPTIC_CONF2,
			(0X0<<HAPTIC_CONF2_MODE_SHIFT),HAPTIC_CONF2_MODE_MASK);
	if(ret < 0 )
		goto err_config;

	/*TODO configure the look-up table which determines the PWM output pulse and the feed back waveform
	  what should be the best duty cycle configuration? according to the practical application.
	  */
	ret = max8997_write_reg(i2c,HAPTIC_CYC1,0x88);
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,HAPTIC_CYC2,0x88);
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,HAPTIC_SIGP1,0xff);
	if(ret < 0 )
		goto err_config;
	ret = max8997_write_reg(i2c,HAPTIC_SIGP2,0xff);
	if(ret < 0 )
		goto err_config;
	ret = max8997_write_reg(i2c,HAPTIC_SIGP3,0xff);
	if(ret < 0 )
		goto err_config;
	ret = max8997_write_reg(i2c,HAPTIC_SIGP4,0xff);
	if(ret < 0 )
		goto err_config;
	ret = max8997_write_reg(i2c,HAPTIC_SIGDC1,0x44);
	if(ret < 0 )
		goto err_config;
	ret = max8997_write_reg(i2c,HAPTIC_SIGDC2,0x44);
	if(ret < 0 )
		goto err_config;
	ret = max8997_write_reg(i2c,HAPTIC_PWMDC1,47);
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,HAPTIC_PWMDC2,47);
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,HAPTIC_PWMDC3,47);
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,HAPTIC_PWMDC4,47);
	if(ret < 0 )
		goto err_config;
	return 0;
err_config:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

#ifdef	CONFIG_ANDROID_TIMED_OUTPUT
static int haptic_get_time(struct timed_output_dev *tdev)
{
	struct haptic_data	*hdata =
		container_of(tdev, struct haptic_data, tdev);

	if (hrtimer_active(&hdata->timer)) {
		ktime_t r = hrtimer_get_remaining(&hdata->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;	
}

static void haptic_enable(struct timed_output_dev *tdev, int value)
{
	struct haptic_data	*hdata =
		container_of(tdev, struct haptic_data, tdev);
	unsigned long	flag;

	if(value>0)
		max8997_haptic_enable(hdata->max8997->haptic);
	else
		max8997_haptic_disable(hdata->max8997->haptic);

	spin_lock_irqsave(&hdata->motor_lock,flag);
	hrtimer_cancel(&hdata->timer);
	if(value){
		if(value > hdata->max_timeout)
			value = hdata->max_timeout;

		hrtimer_start(&hdata->timer,
				ktime_set(value/1000,(value%1000)*1000000),HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&hdata->motor_lock,flag);

}
#endif

static __devinit int max8997_haptic_probe(struct platform_device *pdev)
{
	struct max8997_dev *max8997 = dev_get_drvdata(pdev->dev.parent);
	struct i2c_client *i2c = max8997->haptic;
	struct haptic_data *haptic;
	int ret = 0;

	dev_info(&pdev->dev, "%s : MAX8997 Haptic Driver Loading\n", __func__);

	haptic = kzalloc(sizeof(*haptic), GFP_KERNEL);
	if (!haptic)
		return -ENOMEM;

	haptic->dev = &pdev->dev;
	haptic->max8997 = max8997;

	hrtimer_init(&haptic->timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	haptic->timer.function = motor_timer_func;

	spin_lock_init(&haptic->motor_lock);
	INIT_WORK(&haptic->motor_work, motor_work_func);

#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	haptic->name = "vibrator"; 
	haptic->max_timeout = MAX_TIMEOUT;
	haptic->tdev.name = haptic->name;
	haptic->tdev.get_time = haptic_get_time;
	haptic->tdev.enable = haptic_enable;
	ret = timed_output_dev_register(&haptic->tdev);
	
	if (ret < 0) {
		timed_output_dev_unregister(&haptic->tdev);
		kfree(haptic);
		return ret;
	}	
#else
	haptic->motorclass =class_create(THIS_MODULE,"timed_output");
	haptic->localdev = device_create(haptic->motorclass,NULL,
			MKDEV(0, pdev->id), NULL, "vibrator");
	haptic->name = "vibrator";  
	haptic->max_timeout = 25000;

	ret = device_create_file(haptic->localdev, &dev_attr_enable);
	if(ret < 0 )
		goto err_kfree1;
	
	ret = device_create_file(haptic->localdev, &dev_attr_regdump);
	if(ret < 0 )
		goto err_kfree2;
	
	ret = device_create_file(haptic->localdev, &dev_attr_regwrite);
	if(ret < 0 )
		goto err_kfree3;

	platform_set_drvdata(pdev, (void*)haptic);
	dev_set_drvdata(haptic->localdev,(void*)haptic);
#endif

	max8997_haptic_set_config(i2c);
	return 0;
#ifndef CONFIG_ANDROID_TIMED_OUTPUT
err_kfree3:
	device_remove_file(haptic->localdev, &dev_attr_regdump);
err_kfree2:
	device_remove_file(haptic->localdev, &dev_attr_enable);
err_kfree1:
	device_destroy(haptic->motorclass,MKDEV(0,pdev->id));
	kfree(haptic);
	return ret;
#endif
}

static int __devexit max8997_haptic_remove(struct platform_device *pdev)
{
	struct haptic_data *haptic = platform_get_drvdata(pdev);

	hrtimer_cancel(&haptic->timer);

#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	timed_output_dev_unregister(&haptic->tdev);
#else 
	device_remove_file(haptic->localdev, &dev_attr_enable);
	device_destroy(haptic->motorclass,MKDEV(0,pdev->id));
	dev_set_drvdata(haptic->localdev,NULL);
	dev_set_drvdata(&pdev->dev,NULL);
	class_destroy(haptic->motorclass);
#endif

	if(haptic)
		kfree(haptic);
	return 0;
}

static int max8997_haptic_suspend(struct device *dev)
{
	return 0;
}

static int max8997_haptic_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops max8997_haptic_pm_ops = {
	.suspend	= max8997_haptic_suspend,
	.resume		= max8997_haptic_resume,
};

static struct platform_driver max8997_haptic_driver = {
	.driver = {
		.name = "max8997-haptic",
		.owner = THIS_MODULE,
		.pm = &max8997_haptic_pm_ops,
	},
	.probe = max8997_haptic_probe,
	.remove = __devexit_p(max8997_haptic_remove),
};
static int __init max8997_haptic_init(void)
{
	return platform_driver_register(&max8997_haptic_driver);
}

static void __exit max8997_haptic_exit(void)
{
	platform_driver_register(&max8997_haptic_driver);
}

module_init(max8997_haptic_init);
module_exit(max8997_haptic_exit);

MODULE_DESCRIPTION("MAXIM 8997 haptic control driver");
MODULE_AUTHOR("<caoziqiang@meizu.com> <zhengkl@meizu.com>");
MODULE_LICENSE("GPL");

