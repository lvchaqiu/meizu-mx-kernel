/* linux/drivers/thermal/exynos_thermal.c
  *
  * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
  *		http://www.samsung.com
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  */

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/exynos_thermal.h>


#define MAX_COOLING_DEVICE 4
struct exynos4_thermal_zone {
	unsigned long user_temp;
	unsigned int idle_interval;
	unsigned int active_interval;
	struct thermal_zone_device *therm_dev;
	struct thermal_cooling_device *cool_dev[MAX_COOLING_DEVICE];
	unsigned int cool_dev_size;
	struct platform_device *exynos4_dev;
	struct thermal_sensor_conf *sensor_conf;
};

static struct exynos4_thermal_zone *th_zone;
static BLOCKING_NOTIFIER_HEAD(exynos_tmu_chain_head);

static int exynos4_get_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode *mode)
{
	if (!th_zone->sensor_conf) {
		pr_info("Temperature sensor not initialised\n");
		*mode = THERMAL_DEVICE_DISABLED;
	} else
		*mode = THERMAL_DEVICE_ENABLED;
	return 0;
}

static int exynos4_set_mode(struct thermal_zone_device *thermal,
			    enum thermal_device_mode mode)
{
	if (!th_zone->therm_dev) {
		pr_notice("thermal zone not registered\n");
		return 0;
	}
	if (mode == THERMAL_DEVICE_ENABLED)
		th_zone->therm_dev->polling_delay =
				th_zone->active_interval*1000;
	else
		th_zone->therm_dev->polling_delay =
				th_zone->idle_interval*1000;

	thermal_zone_device_update(th_zone->therm_dev);
	pr_info("thermal polling set for duration=%d sec\n",
				th_zone->therm_dev->polling_delay/1000);
	return 0;
}

/*This may be called from interrupt based temperature sensor*/
void exynos4_report_trigger(void)
{
	unsigned int monitor_temp;

	if (!th_zone || !th_zone->therm_dev)
		return;

	monitor_temp = th_zone->sensor_conf->trip_data.trip_val[0];

	thermal_zone_device_update(th_zone->therm_dev);

	mutex_lock(&th_zone->therm_dev->lock);
	if (th_zone->therm_dev->last_temperature > monitor_temp)
		th_zone->therm_dev->polling_delay =
					th_zone->active_interval*1000;
	else
		th_zone->therm_dev->polling_delay =
					th_zone->idle_interval*1000;
	kobject_uevent(&th_zone->therm_dev->device.kobj, KOBJ_CHANGE);
	mutex_unlock(&th_zone->therm_dev->lock);
}

static int exynos4_get_trip_type(struct thermal_zone_device *thermal, int trip,
				 enum thermal_trip_type *type)
{
	if (trip == 0 || trip == 1 ) 
		*type = THERMAL_TRIP_STATE_ACTIVE;
	else if (trip == 2)
		*type = THERMAL_TRIP_CRITICAL;
	else
		return -EINVAL;

	return 0;
}

static int exynos4_get_trip_temp(struct thermal_zone_device *thermal, int trip,
				 unsigned long *temp)
{
	/*Monitor zone*/
	if (trip == 0)
		*temp = th_zone->sensor_conf->trip_data.trip_val[0];
	/*Warn zone*/
	else if (trip == 1)
		*temp = th_zone->sensor_conf->trip_data.trip_val[1];
	/*Panic zone*/
	else if (trip == 2)
		*temp = th_zone->sensor_conf->trip_data.trip_val[2];
	else
		return -EINVAL;
	/*convert the temperature into millicelsius*/
	*temp = *temp * 1000;

	return 0;
}

static int exynos4_get_crit_temp(struct thermal_zone_device *thermal,
				 unsigned long *temp)
{
	/*Panic zone*/
	*temp = th_zone->sensor_conf->trip_data.trip_val[2];
	/*convert the temperature into millicelsius*/
	*temp = *temp * 1000;
	return 0;
}

static int exynos4_bind(struct thermal_zone_device *thermal,
			struct thermal_cooling_device *cdev)
{
	/* if the cooling device is the one from exynos4 bind it */
	if (cdev != th_zone->cool_dev[0])
		return 0;

	if (thermal_zone_bind_cooling_device(thermal, 0, cdev)) {
		pr_err("error binding cooling dev\n");
		return -EINVAL;
	}
	if (thermal_zone_bind_cooling_device(thermal, 1, cdev)) {
		pr_err("error binding cooling dev\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos4_unbind(struct thermal_zone_device *thermal,
			  struct thermal_cooling_device *cdev)
{
	if (cdev != th_zone->cool_dev[0])
		return 0;
	if (thermal_zone_unbind_cooling_device(thermal, 0, cdev)) {
		pr_err("error unbinding cooling dev\n");
		return -EINVAL;
	}
	if (thermal_zone_unbind_cooling_device(thermal, 1, cdev)) {
		pr_err("error unbinding cooling dev\n");
		return -EINVAL;
	}
	return 0;
}

int register_exynos_tmu_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&exynos_tmu_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_exynos_tmu_notifier);

int unregister_exynos_tmu_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&exynos_tmu_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_exynos_tmu_notifier);

static int exynos_tmu_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&exynos_tmu_chain_head, val, NULL)
			== NOTIFY_BAD) ? -EINVAL : 0;
}

static int exynos4_get_temp(struct thermal_zone_device *thermal,
			       unsigned long *temp)
{
	void *data;
	int ret;

	if (!th_zone->sensor_conf) {
		pr_debug("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	if (th_zone->user_temp) {
		*temp = th_zone->user_temp;
		pr_debug("user_temp mode\n");
	} else {
		data = th_zone->sensor_conf->private_data;
		*temp = th_zone->sensor_conf->read_temperature(data);
		/*convert the temperature into millicelsius*/
		*temp = *temp * 1000;
	}
	pr_debug("%s: temp is :%lu\n", __func__, *temp);
	
	ret = exynos_tmu_notifier_call_chain(*temp);
	if (ret < 0) {
		pr_err("%s notifier_call_chain failed!\n", __func__);
	}
	
	return 0;
}

static ssize_t exynos4_user_temp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", th_zone->user_temp);
}

static ssize_t exynos4_user_temp_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long user_temp = 0;

	sscanf(buf, "%lu\n", &user_temp);
	
	th_zone->user_temp = user_temp*1000;

	return count;
}

/*usr_temp: for test*/
static DEVICE_ATTR(user_temp, 0644,exynos4_user_temp_show,
		   exynos4_user_temp_store);


/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops exynos4_dev_ops = {
	.bind = exynos4_bind,
	.unbind = exynos4_unbind,
	.get_temp = exynos4_get_temp,
	.get_mode = exynos4_get_mode,
	.set_mode = exynos4_set_mode,
	.get_trip_type = exynos4_get_trip_type,
	.get_trip_temp = exynos4_get_trip_temp,
	.get_crit_temp = exynos4_get_crit_temp,
};

int exynos4_register_thermal(struct thermal_sensor_conf *sensor_conf)
{
	int ret, count, tab_size;
	struct freq_pctg_table *tab_ptr;

	if (!sensor_conf || !sensor_conf->read_temperature) {
		pr_err("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	th_zone = kzalloc(sizeof(struct exynos4_thermal_zone), GFP_KERNEL);
	if (!th_zone) {
		ret = -ENOMEM;
		goto err_unregister;
	}

	th_zone->sensor_conf = sensor_conf;

	tab_ptr = (struct freq_pctg_table *)sensor_conf->cooling_data.freq_data;
	tab_size = sensor_conf->cooling_data.freq_pctg_count;

	/*Register the cpufreq cooling device*/
	th_zone->cool_dev_size = 1;
	count = 0;
	th_zone->cool_dev[count] = cpufreq_cooling_register(
			(struct freq_pctg_table *)&(tab_ptr[count]),
			tab_size, cpumask_of(0));

	if (IS_ERR(th_zone->cool_dev[count])) {
		pr_err("Failed to register cpufreq cooling device\n");
		ret = -EINVAL;
		th_zone->cool_dev_size = 0;
		goto err_unregister;
	}

	th_zone->therm_dev = thermal_zone_device_register(sensor_conf->name,
				3, NULL, &exynos4_dev_ops, 0, 0, 0, 1000);
	if (IS_ERR(th_zone->therm_dev)) {
		pr_err("Failed to register thermal zone device\n");
		ret = -EINVAL;
		goto err_unregister;
	}

	th_zone->active_interval = 2;
	th_zone->idle_interval = 10;

	exynos4_set_mode(th_zone->therm_dev, THERMAL_DEVICE_DISABLED);
	
	ret = device_create_file(&th_zone->therm_dev->device, &dev_attr_user_temp);
	if (ret)
		goto err_unregister;

	pr_debug("Exynos: Kernel Thermal management registered\n");

	return 0;

err_unregister:
	exynos4_unregister_thermal();
	return ret;
}
EXPORT_SYMBOL(exynos4_register_thermal);

void exynos4_unregister_thermal(void)
{
	unsigned int i;

	for (i = 0; i < th_zone->cool_dev_size; i++) {
		if (th_zone && th_zone->cool_dev[i])
			cpufreq_cooling_unregister(th_zone->cool_dev[i]);
	}

	if (th_zone && th_zone->therm_dev)
		thermal_zone_device_unregister(th_zone->therm_dev);

	kfree(th_zone);

	pr_info("Exynos: Kernel Thermal management unregistered\n");
}
EXPORT_SYMBOL(exynos4_unregister_thermal);
