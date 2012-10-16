#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#define TORCH_MAX_CURRENT 250000   /*250000uA*/

struct torch_led {
	struct led_classdev cdev;
	struct regulator *regulator;
	bool is_enabled;
};

static int torch_led_enable(struct torch_led *led, bool enable)
{
	int ret = 0;

	if (led->is_enabled == enable) return 0;

	if (enable) 
		ret = regulator_enable(led->regulator);
	else 
		ret = regulator_disable(led->regulator);

	if (!ret) led->is_enabled = enable;
	
	return ret;
}

static int torch_led_set_current(struct torch_led *led,
				enum led_brightness value)
{
	return regulator_set_current_limit(led->regulator, value, TORCH_MAX_CURRENT);
}

static void torch_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct torch_led *led =
			container_of(led_cdev, struct torch_led, cdev);
	int ret;

	/*value is from 0 to 250mA*/
	ret = torch_led_set_current(led, value * 1000);  /*change current in uA unit*/
	if (ret) {
		pr_err("failed to torch_led_set_current\n");
	} else {
		torch_led_enable(led, !!value);
	}
	
}

static int __devinit torch_led_probe(struct platform_device *pdev)
{
	struct torch_led *led;
	struct regulator *regulator;
	int ret = 0;

	led = kzalloc(sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		pr_err("%s():request memory failed\n", __FUNCTION__);
		return -ENOMEM;
	}

	regulator = regulator_get(NULL, "torch_led");
	if (IS_ERR(regulator)) {
		pr_err("%s():regulator get failed\n", __FUNCTION__);
		ret = -EINVAL;
		goto err_regulator_get;
	}

	led->regulator = regulator;
	led->is_enabled = false;
	led->cdev.name = "torch_led";
	led->cdev.brightness_set = torch_led_brightness_set;
	//led->cdev.flags |= LED_CORE_SUSPENDRESUME;
	led->cdev.brightness = 0;
	platform_set_drvdata(pdev, led);

	ret = led_classdev_register(&pdev->dev, &led->cdev);
	if (ret < 0) {
		pr_err("%s():led register failed\n", __FUNCTION__);
		goto err_led_register;
	}

	dev_info(&pdev->dev,"probed\n");
	
	return 0;

err_led_register:
	regulator_put(led->regulator);
err_regulator_get:
	kfree(led);
	return ret;
}

static int __devexit torch_led_remove(struct platform_device *pdev)
{
	struct torch_led *led = platform_get_drvdata(pdev);

	regulator_put(led->regulator);
	led_classdev_unregister(&led->cdev);
	kfree(led);

	return 0;
}

static struct platform_driver torch_led_driver = {
	.driver = {
		.name  = "torch-led",
		.owner = THIS_MODULE,
	},
	.probe  = torch_led_probe,
	.remove = __devexit_p(torch_led_remove),
};

static int __init torch_led_init(void)
{
	return platform_driver_register(&torch_led_driver);
}
module_init(torch_led_init);

static void __exit torch_led_exit(void)
{
	platform_driver_unregister(&torch_led_driver);
}
module_exit(torch_led_exit);


MODULE_AUTHOR("Jerry Mo");
MODULE_DESCRIPTION("MAX8997 and MAX77665 TORCH LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:torch-led");
