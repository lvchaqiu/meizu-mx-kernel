/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/slab.h>

struct pwm_bl_data {
	struct pwm_device	*pwm;
	struct device		*dev;
	unsigned int		period;
	unsigned int		lth_brightness;
	atomic_t 		bl_suspended;	
	int			(*notify)(struct device *,
					  int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
};

static void pwm_backlight_early_suspend(struct early_suspend *);
static void pwm_backlight_late_resume(struct early_suspend *);

/*optimize for meizu mx adjust range(128-255)*/
#define ANDROID_MIN_VALUE		128
static inline int liner_adjust(int brightness, int max)
{
	int min = ANDROID_MIN_VALUE;
	int value = 1;
	
	if (brightness == 0)
		return 0;
	if (brightness <= min)
		return 1; //never close the pwm when
	else if (brightness >= max)
		return max;
	value = (brightness - min)*2;
	value = (value*value)/max;
	if(value>max)
		return max;
	return value>1?value:1;
}

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	struct platform_pwm_backlight_data *data = pb->dev->platform_data;		
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;

	if (atomic_read(&pb->bl_suspended) == 1)
		return 0;	
	
	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	brightness = liner_adjust(brightness, max);
	
	if (brightness == 0) {
		if (data->exit)
			data->exit(pb->dev);		
		pwm_config(pb->pwm, 0, pb->period);	
		pwm_disable(pb->pwm);
	} else {
		brightness = pb->lth_brightness +
			(brightness * (pb->period - pb->lth_brightness) / max);
		pwm_config(pb->pwm, brightness, pb->period);
		pwm_enable(pb->pwm);
		if (data->init)
			data->init(pb->dev);		
	}
	return 0;
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
	.check_fb	= pwm_backlight_check_fb,
};

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm_period_ns;
	pb->notify = data->notify;
	pb->check_fb = data->check_fb;
	pb->lth_brightness = data->lth_brightness *
		(data->pwm_period_ns / data->max_brightness);
	pb->dev = &pdev->dev;

	pb->pwm = pwm_request(data->pwm_id, "backlight");
	if (IS_ERR(pb->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for backlight\n");
		ret = PTR_ERR(pb->pwm);
		goto err_pwm;
	} else
		dev_dbg(&pdev->dev, "got pwm for backlight\n");

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);

#ifdef CONFIG_EARLYSUSPEND
	bl->early_suspends.suspend = pwm_backlight_early_suspend;
	bl->early_suspends.resume = pwm_backlight_late_resume;
	bl->early_suspends.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN-1;
	register_early_suspend(&bl->early_suspends);
#endif

	return 0;

err_bl:
	pwm_free(pb->pwm);
err_pwm:
	kfree(pb);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	pwm_free(pb->pwm);
	kfree(pb);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_EARLYSUSPEND
static void pwm_backlight_early_suspend(struct early_suspend *h)

{
	struct backlight_device *bl = container_of(h, struct backlight_device, early_suspends);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	struct platform_pwm_backlight_data *data = pb->dev->platform_data;
	atomic_set(&pb->bl_suspended, 1);
	
	if (data->exit)
		data->exit(pb->dev);
	
	if (pb->notify)
		pb->notify(pb->dev, 0);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
}

static void pwm_backlight_late_resume(struct early_suspend *h)
{
	struct backlight_device *bl =
		container_of(h, struct backlight_device, early_suspends);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	struct platform_pwm_backlight_data *data = pb->dev->platform_data;	
	atomic_set(&pb->bl_suspended, 0);	
	
	backlight_update_status(bl);
	if (data->init) {
		data->init(pb->dev);
	}

}
#endif

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name	= "pwm-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
};

static int __init pwm_backlight_init(void)
{
	return platform_driver_register(&pwm_backlight_driver);
}
module_init(pwm_backlight_init);

static void __exit pwm_backlight_exit(void)
{
	platform_driver_unregister(&pwm_backlight_driver);
}
module_exit(pwm_backlight_exit);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");

