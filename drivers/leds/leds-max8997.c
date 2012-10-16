/*
 *  max8997-strobe.c
 *  MAXIM 8997 flash led  control driver
 *
 *  Copyright (C) 2011 Meizu Tech Co.Ltd
 *
 *  <caoziqiang@meizu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *  @date       2011-8-29
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
#include <linux/mfd/max8997-strobe.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>

struct strobe_data {
	u8		boost_mode;
	u8 		led_mode;
	u8		low_battery_detect;	
	u8	    timer_mode;	
	u8	    is_enabled;	
	u32		timer_duration;
	char *              name;
	struct device		*dev;
	struct device       	*localdev;
	struct max8997_dev	*max8997;
	struct class 		*strobe_class;
	struct mutex     	strobe_lock;
};
/*global strobe_data structure.*/
static struct strobe_data * gsdata = NULL;

static int max8997_strobe_enable(struct i2c_client *i2c)
{
	int ret = 0;

	ret = max8997_update_reg(i2c,STROBE_BOOST_CNTL,
			(1<<BOOST_CNTL_EN_SHIFT),BOOST_CNTL_EN_MASK);
	if(ret < 0 )
		goto err;

	if(0){
		int i = 0;
		u8 content;
		printk("======the strobe drvier register content start======\n");
		for(i = 0x63; i <= 0x6e;i++){
			max8997_read_reg(i2c,i,&content);
			printk("addr (0x%02x) \tcontent is (0x%02x)\n",i,content);
		}
		printk("======the strobe drvier register content end======\n");
	}

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static int max8997_strobe_disable(struct i2c_client *i2c)
{
	int ret = 0;

	ret = max8997_update_reg(i2c,STROBE_BOOST_CNTL,
			(0<<BOOST_CNTL_EN_SHIFT),BOOST_CNTL_EN_MASK);
	if(ret < 0 )
		goto err;

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static int max8997_strobe_set_timer_duration(struct i2c_client *i2c, int miliseconds)
{
	int ret = 0,count =0 ;

	if(miliseconds < MIN_TIMER_DURATION || miliseconds >MAX_TIMER_DURATION){
		pr_warn("invalide strobe timer duration, restrained it in (25->800)ms\n");
	}

	if(miliseconds < MIN_TIMER_DURATION)
		miliseconds = MIN_TIMER_DURATION;
	if(miliseconds > MAX_TIMER_DURATION)
		miliseconds = MAX_TIMER_DURATION;
	count = miliseconds/MIN_TIMER_DURATION -1;

	ret = max8997_update_reg(i2c,STROBE_FLASH_CNTL,count<<FLASH_CNTL_TMR_DUR_SHIFT,FLASH_CNTL_TMR_DUR_MASK);
	if(ret < 0 )
		goto err;

	ret = max8997_update_reg(i2c, STROBE_FLASH_CNTL, 0x1 << FLASH_CNTL_TMR_MODE_SHIFT, FLASH_CNTL_TMR_MODE_MASK);
	if (ret < 0)
		goto err;
	ret = max8997_update_reg(i2c, STROBE_FLASH_CNTL, 0x1 << FLASH_CNTL_TMR_CNTL_SHIFT, FLASH_CNTL_TMR_CNTL_MASK); 
	if (ret < 0) 
		goto err;
	
	return 0; 
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static int max8997_strobe_set_movie_cur(struct i2c_client *i2c, int cur)
{
	int ret = 0,count =0;

	int cur_val = cur * 1000;

	if (cur_val < MIN_MOVIE_CUR || cur > MAX_MOVIE_CUR) {
		pr_warn("invalide movi cur val, restrained it in (16->250)mA\n");
	}

	if (cur_val < MIN_MOVIE_CUR)
		cur_val = MIN_MOVIE_CUR; 
	else if(cur_val > MAX_MOVIE_CUR)
		cur_val = MAX_MOVIE_CUR;

	count = cur_val/MIN_MOVIE_CUR - 1;
	do { 
		u8 content = 0;
		max8997_read_reg(i2c, STROBE_MOVIE_CUR, &content);
		pr_info("%s, original movie current value:0x%x\n",  __func__, content);
	} while(0);


	ret = max8997_update_reg(i2c, STROBE_MOVIE_CUR, count << MOVIE_CUR_CNTL_SHIFT, MOVIE_CUR_CNTL_MASK);
	if(ret < 0 )
		goto err;
	do { 
		u8 content = 0;
		max8997_read_reg(i2c, STROBE_MOVIE_CUR, &content);
		pr_info("%s, set movie current value:0x%x\n",  __func__, content);
	} while(0);

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static int max8997_strobe_set_flash_cur(struct i2c_client *i2c, int cur)
{
	int ret = 0,count =0;

    int cur_val = cur * 1000;

	if (cur_val < MIN_FLASH_CUR || cur > MAX_FLASH_CUR) {
		pr_warn("invalide flash cur val, restrained it in (24->750)mA\n");
	}

	if (cur_val < MIN_FLASH_CUR)
		cur_val = MIN_FLASH_CUR; 
    else if(cur_val > MAX_FLASH_CUR)
		cur_val = MAX_FLASH_CUR;
	
    count = cur_val/MIN_FLASH_CUR - 1;

	ret = max8997_update_reg(i2c, STROBE_FLASH1_CUR, count << FLASH_CUR_CNTL_SHIFT, FLASH_CUR_CNTL_MASK);
	if(ret < 0 )
		goto err;
	
    ret = max8997_update_reg(i2c, STROBE_FLASH2_CUR, count << FLASH_CUR_CNTL_SHIFT, FLASH_CUR_CNTL_MASK);
	if(ret < 0 )
		goto err;

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static int max8997_strobe_set_mode(struct i2c_client *i2c, int mode)
{
	int ret = 0;

	if(mode > STROBE_BOTH_BY_FLED_EN || mode < STROBE_NONE)
		return -EINVAL;
	switch(mode)
	{
		case STROBE_NONE:
			ret = max8997_write_reg(i2c,STROBE_LED_CNTL,0x0);
			if(ret < 0 )
				goto err;
			break;
		case STROBE_FLASH_ONLY:
			ret = max8997_write_reg(i2c, STROBE_LED_CNTL, 0x3);
			if(ret < 0 )
				goto err;            
			break;
		case STROBE_MOVIE_ONLY:
			ret = max8997_write_reg(i2c,STROBE_LED_CNTL,0x18);
			if(ret < 0 )
				goto err;            
			break;
		case STROBE_BOTH:
			ret = max8997_write_reg(i2c,STROBE_LED_CNTL,0x1b);
			if(ret < 0 )
				goto err;         
			break;
		case STROBE_FLASH_BY_FLED_EN:
			ret = max8997_write_reg(i2c,STROBE_LED_CNTL,0x05);
			if(ret < 0 )
				goto err;     
			break;
		case STROBE_MOVIE_BY_FLED_EN:
			ret = max8997_write_reg(i2c,STROBE_LED_CNTL,0x38);
			if(ret < 0 )
				goto err;  
			break;
		case STROBE_BOTH_BY_FLED_EN:
			ret = max8997_write_reg(i2c,STROBE_LED_CNTL,0x3f);
			if(ret < 0 )
				goto err;  
			break;
	}

	return 0;
err:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static ssize_t enable_show(struct device* dev, struct device_attribute* attr, char *buf)
{  
	//struct strobe_data *hdata =(struct strobe_data*)dev_get_drvdata(dev);
	return sprintf(buf,"strobe is %s!\n", gsdata->is_enabled?"enabled":"disabled");
}

static ssize_t enable_store(struct device* dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	int value = 0;

	if(sscanf(buf,"%d",&value)!=1)
		return -EINVAL;

	mutex_lock(&sdata->strobe_lock);
	if(value){
		max8997_strobe_enable(sdata->max8997->i2c);
        gsdata->is_enabled = 1;
	}else{
		max8997_strobe_disable(sdata->max8997->i2c);
        gsdata->is_enabled = 0;
	}
	mutex_unlock(&sdata->strobe_lock);

	return size;
}
static DEVICE_ATTR(enable, S_IRUGO|S_IWUGO, enable_show, enable_store);

static ssize_t dump_reg_store(struct device* dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
    int i = 0;
    u8 content;

    mutex_lock(&sdata->strobe_lock);
    printk("======the strobe drvier register content start======\n");
    for (i = 0x63; i <= 0x6e; i++){
        max8997_read_reg(sdata->max8997->i2c, i, &content);
        printk("addr (0x%02x) \tcontent is (0x%02x)\n", i, content);
    }
    printk("======the strobe drvier register content end========\n");
    mutex_unlock(&sdata->strobe_lock);

    return size;
}
static DEVICE_ATTR(dump_reg, S_IRUGO|S_IWUGO, NULL, dump_reg_store);

static ssize_t mode_show(struct device *dev, struct device_attribute* attr, char* buf)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	u8  mode = 0, count=0;
	int    ret =0;

	mutex_lock(&sdata->strobe_lock);
	ret = max8997_read_reg(sdata->max8997->i2c,STROBE_LED_CNTL,&mode);   
	if(ret < 0 ){
		pr_err("register read error!\n");
		mutex_unlock(&sdata->strobe_lock);
		return 0;
	}
	mutex_unlock(&sdata->strobe_lock);

	if((mode&LED_CNTL_FLASH_EN_MASK)!=0 && (mode&LED_CNTL_FLASH_EN_MASK)!=4)
		count+=1;
	if((mode&LED_CNTL_MOVIE_EN_MASK)!=0 && (mode&LED_CNTL_MOVIE_EN_MASK)!=4)
		count+=1<<1;

	return sprintf(buf,"%d\n",count);
}
static ssize_t mode_store(struct device* dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	int value = 0,ret =0;

	if(sscanf(buf,"%d",&value)!=1)
		return -EINVAL;

	mutex_lock(&sdata->strobe_lock);
	ret = max8997_strobe_set_mode(sdata->max8997->i2c,value);
	if(ret < 0)
		pr_err("something wrong when change flash led mode!\n");
	mutex_unlock(&sdata->strobe_lock);

	return size;
}

DEVICE_ATTR(mode,S_IALLUGO,mode_show,mode_store);
static ssize_t duration_show(struct device *dev, struct device_attribute* attr, char* buf)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	u8  mode = 0, count=0;
	int    ret =0;

	mutex_lock(&sdata->strobe_lock);
	ret = max8997_read_reg(sdata->max8997->i2c,STROBE_FLASH_CNTL,&mode);   
	if(ret < 0 ){
		pr_err("register read error!\n");
		mutex_unlock(&sdata->strobe_lock);
		return 0;
	}
	mutex_unlock(&sdata->strobe_lock);

	count = (mode & FLASH_CNTL_TMR_DUR_MASK) >> FLASH_CNTL_TMR_DUR_SHIFT;

	return sprintf(buf,"%d\n",(count+1)*25);
}

static ssize_t duration_store(struct device* dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	int value = 0;
	printk("%s %d\n",__func__,__LINE__);

	if(sscanf(buf,"%d",&value)!=1)
		return -EINVAL;

	mutex_lock(&sdata->strobe_lock);
	max8997_strobe_set_timer_duration(sdata->max8997->i2c,value);
	mutex_unlock(&sdata->strobe_lock);

	return size;
}
DEVICE_ATTR(duration,S_IRUGO|S_IWUGO,duration_show,duration_store);

static ssize_t movie_cur_show(struct device *dev, struct device_attribute* attr, char* buf)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	u8  cur = 0, count=0;
	int    ret =0;

	mutex_lock(&sdata->strobe_lock);
	ret = max8997_read_reg(sdata->max8997->i2c, STROBE_MOVIE_CUR, &cur);   
	if(ret < 0 ){
		pr_err("register read error!\n");
		mutex_unlock(&sdata->strobe_lock);
		return 0;
	}
	mutex_unlock(&sdata->strobe_lock);
	count = (cur & MOVIE_CUR_CNTL_MASK) >> MOVIE_CUR_CNTL_SHIFT;

	return sprintf(buf, "%d\n", (count+1)*15625/1000);
}

static ssize_t movie_cur_store(struct device* dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	int value = 0;

	if(sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
    
    printk("%s %d, value:%d\n",__func__,__LINE__, value);

	mutex_lock(&sdata->strobe_lock);
    max8997_strobe_set_movie_cur(sdata->max8997->i2c, value);
	mutex_unlock(&sdata->strobe_lock);

	return size;
}
DEVICE_ATTR(movie_cur, S_IRUGO|S_IWUGO, movie_cur_show, movie_cur_store);

static ssize_t flash_cur_show(struct device *dev, struct device_attribute* attr, char* buf)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	u8  cur = 0, count=0;
	int    ret =0;

	mutex_lock(&sdata->strobe_lock);
	ret = max8997_read_reg(sdata->max8997->i2c, STROBE_FLASH1_CUR, &cur);   
	if(ret < 0 ){
		pr_err("register read error!\n");
		mutex_unlock(&sdata->strobe_lock);
		return 0;
	}
	mutex_unlock(&sdata->strobe_lock);
	count = (cur & FLASH_CUR_CNTL_MASK) >> FLASH_CUR_CNTL_SHIFT;

	return sprintf(buf, "%d\n", (count+1)*23440/1000);
}

static ssize_t flash_cur_store(struct device* dev,struct device_attribute *attr, const char *buf, size_t size)
{
	struct strobe_data *sdata = (struct strobe_data*)dev_get_drvdata(dev);
	int value = 0;

	if(sscanf(buf, "%d", &value) != 1)
		return -EINVAL;
	
    printk("%s %d, value:%d\n",__func__,__LINE__, value);

	mutex_lock(&sdata->strobe_lock);
    max8997_strobe_set_flash_cur(sdata->max8997->i2c, value);
	mutex_unlock(&sdata->strobe_lock);

	return size;
}
DEVICE_ATTR(flash_cur, S_IRUGO|S_IWUGO, flash_cur_show, flash_cur_store);

static int max8997_strobe_current_init(struct i2c_client *i2c)
{
	int ret = 0;

	ret = max8997_write_reg(i2c,STROBE_FLASH1_CUR,0x16<<3);//500mA
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,STROBE_FLASH2_CUR,0x16<<3);//500mA we should not use this.
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,STROBE_MOVIE_CUR, 0x0);  //set to minimal movie current
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,STROBE_GSMB_CUR,0xc0);//default
	if(ret < 0 )
		goto err_config;

	ret = max8997_write_reg(i2c,STROBE_BOOST_CNTL,0x00);//default
	if(ret < 0 )
		goto err_config;

	return 0;
err_config:
	pr_err("%s error! %d\n",__func__,ret);
	return ret;
}

static __devinit int max8997_strobe_probe(struct platform_device *pdev)
{
	struct max8997_dev *max8997 = dev_get_drvdata(pdev->dev.parent);
	struct i2c_client *i2c = max8997->i2c;
	struct strobe_data *strobe;
	int ret = 0;

	dev_info(&pdev->dev, "%s : MAX8997 Flash Led Driver Loading\n", __func__);

	strobe = kzalloc(sizeof(*strobe), GFP_KERNEL);
	if (!strobe)
		return -ENOMEM;

	strobe->dev = &pdev->dev;
	strobe->max8997 = max8997;

	mutex_init(&strobe->strobe_lock);
	strobe->strobe_class =class_create(THIS_MODULE,"flashled");
	strobe->localdev = device_create(strobe->strobe_class,NULL,
			MKDEV(0, pdev->id), NULL, "strobe");
	strobe->name = "strobe"; //just for test.
	strobe->boost_mode = BOOST_ADAPTIVE;
	strobe->led_mode = STROBE_NONE;
	strobe->timer_duration = 800;//ms
	strobe->is_enabled = 0;

	ret = device_create_file(strobe->localdev,&dev_attr_enable);
	if(ret < 0 )
		goto err_kfree;
	ret = device_create_file(strobe->localdev,&dev_attr_mode);
	if(ret < 0 )
		goto err_kfree;
	ret = device_create_file(strobe->localdev,&dev_attr_duration);
	if(ret < 0 )
		goto err_kfree;

    ret = device_create_file(strobe->localdev, &dev_attr_movie_cur);
	if(ret < 0 )
		goto err_kfree;

	ret = device_create_file(strobe->localdev, &dev_attr_flash_cur);
	if(ret < 0 )
		goto err_kfree;
	
    ret = device_create_file(strobe->localdev, &dev_attr_dump_reg);
	if(ret < 0 )
		goto err_kfree;

	platform_set_drvdata(pdev, (void*)strobe);
	dev_set_drvdata(strobe->localdev,(void*)strobe);

	max8997_strobe_current_init(i2c);
	max8997_strobe_set_mode(i2c,strobe->led_mode);
	max8997_strobe_set_timer_duration(i2c,strobe->timer_duration);

	gsdata = strobe;

	return 0;

err_kfree:
	device_destroy(strobe->strobe_class,MKDEV(0,pdev->id));
	kfree(strobe);
	return ret;
}

static int __devexit max8997_strobe_remove(struct platform_device *pdev)
{
	struct strobe_data *strobe = platform_get_drvdata(pdev);

	device_remove_file(strobe->localdev, &dev_attr_enable);
	device_remove_file(strobe->localdev, &dev_attr_mode);
	device_remove_file(strobe->localdev, &dev_attr_duration);
	device_remove_file(strobe->localdev, &dev_attr_movie_cur);
	device_remove_file(strobe->localdev, &dev_attr_flash_cur);
	device_remove_file(strobe->localdev, &dev_attr_dump_reg);
	device_destroy(strobe->strobe_class,MKDEV(0,pdev->id));
	dev_set_drvdata(strobe->localdev,NULL);
	dev_set_drvdata(&pdev->dev,NULL);
	class_destroy(strobe->strobe_class);
	mutex_destroy(&strobe->strobe_lock);
	if(strobe)
		kfree(strobe);

	return 0;
}
static int max8997_strobe_suspend(struct device *dev)
{
    int ret = 0;
    if (gsdata->is_enabled) {
        ret = max8997_update_reg(gsdata->max8997->i2c, STROBE_BOOST_CNTL, (0<<BOOST_CNTL_EN_SHIFT), BOOST_CNTL_EN_MASK);
    }
	return ret;
}

static int max8997_strobe_resume(struct device *dev)
{
    int ret = 0;
    if (gsdata->is_enabled) {
        ret = max8997_update_reg(gsdata->max8997->i2c, STROBE_BOOST_CNTL, (1<<BOOST_CNTL_EN_SHIFT), BOOST_CNTL_EN_MASK);
    }
	return ret;
}

static const struct dev_pm_ops max8997_strobe_pm_ops = {
	.suspend	= max8997_strobe_suspend,
	.resume		= max8997_strobe_resume,
};

static struct platform_driver max8997_strobe_driver = {
	.driver = {
		.name = "max8997-flash",
		.owner = THIS_MODULE,
		.pm = &max8997_strobe_pm_ops,
	},
	.probe = max8997_strobe_probe,
	.remove = __devexit_p(max8997_strobe_remove),
};
static int __init max8997_strobe_init(void)
{
	return platform_driver_register(&max8997_strobe_driver);
}

static void __exit max8997_strobe_exit(void)
{
	platform_driver_register(&max8997_strobe_driver);
}

int osron_flash_led_mode_set(strobe_mode mode)
{
	if(!gsdata)
		return -ENODEV;

	return max8997_strobe_set_mode(gsdata->max8997->i2c,mode);
}
EXPORT_SYMBOL(osron_flash_led_mode_set);

int osron_flash_led_enable(int enable)
{
	if(!gsdata)
		return -ENODEV;
	if(enable){
		return max8997_strobe_enable(gsdata->max8997->i2c);
	}else{
		return max8997_strobe_disable(gsdata->max8997->i2c);
	}
}
EXPORT_SYMBOL(osron_flash_led_enable);

int osron_flash_led_set_duration(int timer_duration)
{
	if(!gsdata)
		return -ENODEV;

	return max8997_strobe_set_timer_duration(gsdata->max8997->i2c,timer_duration);
}
EXPORT_SYMBOL(osron_flash_led_set_duration);

int osron_flash_led_set_current(int cur)
{
	if(!gsdata)
		return -ENODEV;

	return max8997_strobe_set_flash_cur(gsdata->max8997->i2c, cur);
}
EXPORT_SYMBOL(osron_flash_led_set_current);

int max8997_camera_set_flashled(int mode, int cur, int msec)
{
	max8997_strobe_set_flash_cur(gsdata->max8997->i2c, cur);
	max8997_strobe_set_timer_duration(gsdata->max8997->i2c, msec);
	max8997_strobe_set_mode(gsdata->max8997->i2c, mode);
	return 0;
}
EXPORT_SYMBOL(max8997_camera_set_flashled);

int max8997_camera_enable_flashled(int enable)
{
    int ret = 0;
    if (enable) {
        if (!gsdata->is_enabled) {
            ret = max8997_update_reg(gsdata->max8997->i2c, STROBE_BOOST_CNTL, (1<<BOOST_CNTL_EN_SHIFT), BOOST_CNTL_EN_MASK);
            gsdata->is_enabled = 1;
        }
    } else {
        if (gsdata->is_enabled) {
            ret = max8997_update_reg(gsdata->max8997->i2c, STROBE_BOOST_CNTL, (0<<BOOST_CNTL_EN_SHIFT), BOOST_CNTL_EN_MASK);
            gsdata->is_enabled = 0;
        }
    }
    return ret;
}
EXPORT_SYMBOL(max8997_camera_enable_flashled);

module_init(max8997_strobe_init);
module_exit(max8997_strobe_exit);

MODULE_DESCRIPTION("MAXIM 8997 flash led control driver");
MODULE_AUTHOR("<caoziqiang@meizu.com>");
MODULE_LICENSE("GPL");
