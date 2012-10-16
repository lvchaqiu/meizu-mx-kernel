/* linux/arch/arm/mach-exynos/mx-wifi.c
 *
 * Copyright (C) 2012 Meizu Technology Co.Ltd, Zhuhai, China
 *		http://www.meizu.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/mmc/host.h>

#include <asm/mach-types.h>

#include <plat/devs.h>
#include <plat/irqs.h>
#include <plat/sdhci.h>
#include <plat/gpio-cfg.h>

static int wl_wake;
static int wl_host_wake;
static int wl_rst;
static int bt_rst;
static int wl_power;
static int bt_power;
static int wl_cs;
static DEFINE_MUTEX(wifi_mutex);


#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM	16
 
static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

extern int dhd_card_insert(void);
extern void dhd_card_remove(void);
extern void need_test_firmware(void);
extern void reset_firmware_type(void);

ssize_t wifi_card_state_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	int wl_on ;

	wl_on =  gpio_get_value(wl_rst);
	return sprintf(buf, "%d\n",  wl_on);
}


ssize_t wifi_card_state_store(struct device *dev,
									struct device_attribute *attr,	const char *buf, size_t count)
{	
	unsigned long value = simple_strtoul(buf, NULL, 10);
	int wl_inserded;
	int wl_on;
	int ret = count;

	mutex_lock(&wifi_mutex);

	wl_inserded = !gpio_get_value(wl_cs);
	wl_on = gpio_get_value(wl_rst);

	printk("wl_on %d, wl_inserded %d\n", wl_on, wl_inserded);
	if(value) {
		if(!wl_inserded && !wl_on){
			if(value == 2)
				need_test_firmware();
			dhd_card_insert();
		} else {
			printk("wifi card already insert\n");
			ret = -EBUSY;
		}
	} else {
		if(wl_inserded && wl_on) {
			dhd_card_remove();
			reset_firmware_type();
		} else {
			printk("wifi card already remove\n");
			ret = -EBUSY;
		}
	}

	mutex_unlock(&wifi_mutex);
	return ret;
}

void wifi_card_set_power(int onoff)
{
	if(onoff) {
		//wlan host wakeup:   int 20, pull down 
		s3c_gpio_cfgpin(wl_host_wake, S3C_GPIO_SPECIAL(15));  
		s3c_gpio_setpull(wl_host_wake, S3C_GPIO_PULL_DOWN);  

		//wake: output ,  0 
		s3c_gpio_cfgpin(wl_wake, S3C_GPIO_OUTPUT);
		gpio_set_value(wl_wake,  0);

		//wlan reset: output ,1
		s3c_gpio_cfgpin(wl_rst,  S3C_GPIO_OUTPUT);
		gpio_set_value(wl_rst, 1);

		//wlan power:output ,1
		s3c_gpio_cfgpin(wl_power, S3C_GPIO_OUTPUT);
		gpio_set_value(wl_power, 1);		

		//bt power
		s3c_gpio_cfgpin(bt_power, S3C_GPIO_OUTPUT);
		gpio_set_value(bt_power, 1);
	} else {
		gpio_set_value(wl_rst, 0);

		if (!gpio_get_value(bt_rst) ) {
			gpio_set_value(bt_power, 0);
			gpio_set_value(wl_power, 0);
		}
	}
}

static int wlan_power_en(int onoff)
{
	if (gpio_get_value(wl_cs)) {
		WARN(1, "WL_WIFICS is HI\n");
	} else {
		/* must be mmc card detected pin low */
		if (onoff) {
			wifi_card_set_power(1);
			msleep(200);
		} else {
			wifi_card_set_power(0);
		}
	}
	return 0;
}

static int wlan_reset_en(int onoff)
{
	gpio_set_value(wl_rst, onoff ? 1 : 0);
	return 0;
}

static int wlan_carddetect_en(int onoff)
{
	if(onoff) {
		exynos4_setup_sdhci3_cfg_gpio(NULL, 4);
	} else {
		exynos4_setup_sdhci3_cfg_gpio(NULL, 0);
	}
	msleep(10);

	gpio_set_value(wl_cs, !onoff);	

	msleep(400);
	return 0;
}

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

static int __init mx_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0 ; i < WLAN_SKB_BUF_NUM ; i++) {
		wlan_static_skb[i] = dev_alloc_skb(
				((i < (WLAN_SKB_BUF_NUM / 2)) ? 4096 : 8192));

		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wifi_mem_array[i].mem_ptr =
			kzalloc(wifi_mem_array[i].size, GFP_KERNEL);

		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	return 0;

err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}

static struct wifi_platform_data wifi_pdata = {
	.set_power = wlan_power_en,
	.set_reset = wlan_reset_en,
	.set_carddetect = wlan_carddetect_en,
	.mem_prealloc = wlan_mem_prealloc,
};

static struct resource m030_wifi_resources[] = {
	[0] = {
		.name = "bcm4329_wlan_irq",
		.start = IRQ_EINT(8),
		.end 	 = IRQ_EINT(8),
		.flags = IORESOURCE_IRQ |
			    IRQF_TRIGGER_HIGH |
			    IRQF_ONESHOT,
	},
};

static struct resource m032_wifi_resources[] = {
	[0] = {
		.name = "bcm4329_wlan_irq",
		.start = IRQ_EINT(19),
		.end 	 = IRQ_EINT(19),
		.flags = IORESOURCE_IRQ |
			    IRQF_TRIGGER_HIGH |
			    IRQF_ONESHOT,
	},
};

static struct platform_device mx_wifi = {
	.name = "bcm4329_wlan",
	.id = -1,
	.num_resources = 1,
	.dev = {
		.platform_data = &wifi_pdata,
	},
};

#ifdef CONFIG_S3C_DEV_HSMMC3
static void sdhci_wifi_set_power(unsigned int power_mode)
{
	if (!gpio_get_value(wl_cs) && power_mode == 1) {
		wifi_card_set_power(1);
	} else {
		wifi_card_set_power(0);
	}
}

static struct s3c_sdhci_platdata __initdata mx_hsmmc3_pdata  = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_SDHCI_CLKGATE
	.host_caps2	= MMC_CAP2_CLOCK_GATING,
#endif	
	.set_power	= sdhci_wifi_set_power,
};
#endif

static int __init mx_wifi_init(void)
{
	int ret;

	if (machine_is_m030()) {
		wl_wake 	= EXYNOS4_GPF0(5);
		wl_host_wake= EXYNOS4_GPX1(0);
		wl_rst		= EXYNOS4_GPF1(0);
		bt_rst		= EXYNOS4_GPF2(1);
		wl_power 	= EXYNOS4_GPF1(5);
		bt_power	= EXYNOS4_GPF2(3);
		wl_cs		= EXYNOS4_GPZ(6);
		mx_wifi.resource = m030_wifi_resources;
	} else {
		wl_wake 	= EXYNOS4_GPY5(3);
		wl_host_wake= EXYNOS4_GPX2(3);
		wl_rst		= EXYNOS4_GPY5(1);
		bt_rst		= EXYNOS4_GPY5(5);
		wl_power 	= EXYNOS4_GPY6(3);
		bt_power 	= EXYNOS4_GPY6(7);
		wl_cs		= EXYNOS4_GPY6(4);
		mx_wifi.resource = m032_wifi_resources;
	}

#ifdef CONFIG_S3C_DEV_HSMMC3
	ret  = platform_device_register(&s3c_device_hsmmc3);
	if (ret) return ret;
	s3c_sdhci3_set_platdata(&mx_hsmmc3_pdata);
#endif

	ret = platform_device_register(&mx_wifi);
	if (ret) goto err1;

	ret = mx_init_wifi_mem();
	if (ret) goto err2;

	return 0;

err2:
	platform_device_unregister(&mx_wifi);
err1:
#ifdef CONFIG_S3C_DEV_HSMMC3
	platform_device_unregister(&s3c_device_hsmmc3);
#endif	
	return ret;
}

device_initcall(mx_wifi_init);
