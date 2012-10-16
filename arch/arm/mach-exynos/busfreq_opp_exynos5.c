/* linux/arch/arm/mach-exynos/busfreq_opp_exynos5.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS4 - BUS clock frequency scaling support with OPP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/opp.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

#include <asm/mach-types.h>

#include <mach/ppmu.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/regs-mem.h>
#include <mach/cpufreq.h>
#include <mach/dev.h>
#include <mach/busfreq_exynos5.h>

#include <plat/map-s5p.h>
#include <plat/cpu.h>
#include <plat/clock.h>

#define BUSFREQ_DEBUG	1

static DEFINE_MUTEX(busfreq_lock);
BLOCKING_NOTIFIER_HEAD(exynos_busfreq_notifier_list);

struct busfreq_control {
	struct opp *lock[PPMU_TYPE_END];
	struct device *dev[PPMU_TYPE_END];
};

static struct busfreq_control bus_ctrl;

void update_busfreq_stat(struct busfreq_data *data,
		enum ppmu_type type, unsigned int index)
{
#ifdef BUSFREQ_DEBUG
	unsigned long long cur_time = get_jiffies_64();
	data->time_in_state[type][index] =
		cputime64_add(data->time_in_state[type][index], cputime_sub(cur_time, data->last_time[type]));
	data->last_time[type] = cur_time;
#endif
}


static unsigned long __maybe_unused step_up(struct busfreq_data *data,
				enum ppmu_type type, int step)
{
	int i;
	struct opp *opp;
	unsigned long newfreq = data->curr_freq[type];

	if (data->max_freq[type] == data->curr_freq[type])
		return newfreq;

	for (i = 0; i < step; i++) {
		newfreq += 1;
		opp = opp_find_freq_ceil(data->dev[type], &newfreq);

		if (opp_get_freq(opp) == data->max_freq[type])
			break;
	}

	return newfreq;
}

unsigned long step_down(struct busfreq_data *data,
			enum ppmu_type type, int step)
{
	int i;
	struct opp *opp;
	unsigned long newfreq = data->curr_freq[type];

	if (data->min_freq[type] == data->curr_freq[type])
		return newfreq;

	for (i = 0; i < step; i++) {
		newfreq -= 1;
		opp = opp_find_freq_floor(data->dev[type], &newfreq);

		if (opp_get_freq(opp) == data->min_freq[type])
			break;
	}

	return newfreq;
}

static void _target(struct busfreq_data *data,
		enum ppmu_type type, unsigned long newfreq)
{
	struct opp *opp;
	unsigned int voltage;
	int index;

	opp = opp_find_freq_exact(data->dev[type], newfreq, true);

	if (bus_ctrl.lock[type]) {
		opp = bus_ctrl.lock[type];
		newfreq = opp_get_freq(opp);
	}

	index = data->get_table_index(newfreq, type);

	if (newfreq == 0 || newfreq == data->curr_freq[type] ||
			data->use == false) {
		update_busfreq_stat(data, type, index);
		return;
	}

	voltage = opp_get_voltage(opp);

	if (newfreq > data->curr_freq[type]) {
		regulator_set_voltage(data->vdd_reg[type], voltage,
				voltage + 25000);
		if (type == PPMU_MIF && data->busfreq_prepare)
			data->busfreq_prepare(index);
	}

	data->target(type, index);

	if (newfreq < data->curr_freq[type]) {
		if (type == PPMU_MIF && data->busfreq_post)
			data->busfreq_post(index);
		regulator_set_voltage(data->vdd_reg[type], voltage,
				voltage + 25000);
	}
	data->curr_freq[type] = newfreq;

	update_busfreq_stat(data, type, index);
}

static void exynos_busfreq_timer(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct busfreq_data *data = container_of(delayed_work, struct busfreq_data,
			worker);
	int i;
	struct opp *opp[PPMU_TYPE_END];
	unsigned long newfreq;

	data->monitor(data, &opp[PPMU_MIF], &opp[PPMU_INT]);

	ppmu_start(data->dev[PPMU_MIF]);

	mutex_lock(&busfreq_lock);

	for (i = PPMU_MIF; i < PPMU_TYPE_END; i++) {
		newfreq = opp_get_freq(opp[i]);
		_target(data, i, newfreq);
	}

	mutex_unlock(&busfreq_lock);
	queue_delayed_work(system_freezable_wq, &data->worker, data->sampling_rate);
}

static int exynos_buspm_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct busfreq_data *data = container_of(this, struct busfreq_data,
			exynos_buspm_notifier);
	int i;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&busfreq_lock);
		for (i = PPMU_MIF; i < PPMU_TYPE_END; i++)
			_target(data, i, data->max_freq[i]);
		mutex_unlock(&busfreq_lock);
		data->use = false;
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		data->use = true;
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static int exynos_busfreq_reboot_event(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct busfreq_data *data = container_of(this, struct busfreq_data,
			exynos_reboot_notifier);
	int i;
	struct opp *opp;
	unsigned int voltage[PPMU_TYPE_END];
	for (i = PPMU_MIF; i < PPMU_TYPE_END; i++) {
		opp = opp_find_freq_exact(data->dev[i], data->max_freq[i], true);
		voltage[i] = opp_get_voltage(opp);

		regulator_set_voltage(data->vdd_reg[i], voltage[i], voltage[i] + 25000);
	}
	data->use = false;

	printk(KERN_INFO "REBOOT Notifier for BUSFREQ\n");
	return NOTIFY_DONE;
}

static int exynos_busfreq_request_event(struct notifier_block *this,
		unsigned long req_newfreq, void *device)
{
	struct busfreq_data *data = container_of(this, struct busfreq_data,
			exynos_request_notifier);
	int i;
	struct opp *opp[PPMU_TYPE_END];
	unsigned long newfreq[PPMU_TYPE_END];
	unsigned long freq;

	if (req_newfreq == 0 || data->use == false)
		return -EINVAL;

	mutex_lock(&busfreq_lock);

	newfreq[PPMU_MIF] = (req_newfreq / 1000) * 1000;
	newfreq[PPMU_INT] = (req_newfreq % 1000) * 1000;

	for (i = PPMU_MIF; i < PPMU_TYPE_END; i++) {
		opp[i] = opp_find_freq_ceil(data->dev[i], &newfreq[i]);
		freq = opp_get_freq(opp[i]);
		_target(data, i, freq);
	}

	mutex_unlock(&busfreq_lock);
	printk(KERN_INFO "REQUEST Notifier for BUSFREQ\n");
	return NOTIFY_DONE;
}

int exynos_busfreq_lock(unsigned int nId,
	enum busfreq_level_request busfreq_level)
{
	return 0;
}

void exynos_busfreq_lock_free(unsigned int nId)
{
}

static ssize_t show_level_lock(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(bus_ctrl.dev[PPMU_MIF]);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);
	int len = 0;
	unsigned long mif_freq, int_freq;

	mif_freq = bus_ctrl.lock[PPMU_MIF] == NULL ? 0 : opp_get_freq(bus_ctrl.lock[PPMU_MIF]);
	int_freq = bus_ctrl.lock[PPMU_INT] == NULL ? 0 : opp_get_freq(bus_ctrl.lock[PPMU_INT]);

	len = sprintf(buf, "Current Freq(MIF/INT) : (%lu - %lu)\n",
			data->curr_freq[PPMU_MIF], data->curr_freq[PPMU_INT]);
	len += sprintf(buf + len, "Current Lock Freq(MIF/INT) : (%lu - %lu\n)", mif_freq, int_freq);

	return len;
}

static ssize_t store_level_lock(struct device *device, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(bus_ctrl.dev[PPMU_MIF]);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);
	struct opp *opp[PPMU_TYPE_END];
	unsigned long freq[PPMU_TYPE_END];
	int i;
	int ret;

	ret = sscanf(buf, "%lu %lu", &freq[PPMU_MIF], &freq[PPMU_INT]);
	if (freq[PPMU_MIF] == 0 || freq[PPMU_INT] == 0 || ret != 2) {
		pr_info("Release bus level lock.\n");
		bus_ctrl.lock[PPMU_MIF] = NULL;
		bus_ctrl.lock[PPMU_INT] = NULL;
		return count;
	}

	for (i = PPMU_MIF; i < PPMU_TYPE_END; i++) {
		if (freq[i] > data->max_freq[i])
			freq[i] = data->max_freq[i];

		opp[i] = opp_find_freq_ceil(bus_ctrl.dev[i], &freq[i]);
		bus_ctrl.lock[i] = opp[i];
	}
	pr_info("Lock Freq : MIF/INT(%lu - %lu)\n", opp_get_freq(opp[PPMU_MIF]), opp_get_freq(opp[PPMU_INT]));
	return count;
}

static ssize_t show_locklist(struct device *device,
		struct device_attribute *attr, char *buf)
{
	return dev_lock_list(bus_ctrl.dev[PPMU_MIF], buf);
}

static ssize_t show_time_in_state(struct device *device,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(bus_ctrl.dev[PPMU_MIF]);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);
	struct busfreq_table *table;
	ssize_t len = 0;
	int i;

	table = data->table[PPMU_MIF];
	len += sprintf(buf, "%s\n", "MIF stat");
	for (i = LV_0; i < LV_MIF_END; i++)
		len += sprintf(buf + len, "%u %llu\n", table[i].mem_clk,
				(unsigned long long)cputime64_to_clock_t(data->time_in_state[PPMU_MIF][i]));

	table = data->table[PPMU_INT];
	len += sprintf(buf + len, "\n%s\n", "INT stat");
	for (i = LV_0; i < LV_INT_END; i++)
		len += sprintf(buf + len, "%u %llu\n", table[i].mem_clk,
				(unsigned long long)cputime64_to_clock_t(data->time_in_state[PPMU_INT][i]));
	return len;
}

static DEVICE_ATTR(curr_freq, 0666, show_level_lock, store_level_lock);
static DEVICE_ATTR(lock_list, 0666, show_locklist, NULL);
static DEVICE_ATTR(time_in_state, 0666, show_time_in_state, NULL);

static struct attribute *busfreq_attributes[] = {
	&dev_attr_curr_freq.attr,
	&dev_attr_lock_list.attr,
	&dev_attr_time_in_state.attr,
	NULL
};

int exynos_request_register(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&exynos_busfreq_notifier_list, n);
}

void exynos_request_apply(unsigned long freq, bool fix, bool disable)
{
	blocking_notifier_call_chain(&exynos_busfreq_notifier_list, freq, NULL);
}

static __devinit int exynos_busfreq_probe(struct platform_device *pdev)
{
	struct busfreq_data *data;

	data = kzalloc(sizeof(struct busfreq_data), GFP_KERNEL);
	if (!data) {
		pr_err("Unable to create busfreq_data struct.\n");
		return -ENOMEM;
	}

	data->exynos_buspm_notifier.notifier_call =
		exynos_buspm_notifier_event;
	data->exynos_reboot_notifier.notifier_call =
		exynos_busfreq_reboot_event;
	data->busfreq_attr_group.attrs = busfreq_attributes;
	data->exynos_request_notifier.notifier_call =
		exynos_busfreq_request_event;

	INIT_DELAYED_WORK(&data->worker, exynos_busfreq_timer);

	if (soc_is_exynos5250()) {
		data->init = exynos5250_init;
	} else {
		pr_err("Unsupport device type.\n");
		goto err_busfreq;
	}

	if (data->init(&pdev->dev, data)) {
		pr_err("Failed to init busfreq.\n");
		goto err_busfreq;
	}

	bus_ctrl.dev[PPMU_MIF] =  data->dev[PPMU_MIF];
	bus_ctrl.dev[PPMU_INT] =  data->dev[PPMU_INT];

	data->last_time[PPMU_MIF] = get_jiffies_64();
	data->last_time[PPMU_INT] = get_jiffies_64();

	data->busfreq_kobject = kobject_create_and_add("busfreq",
				&cpu_sysdev_class.kset.kobj);
	if (!data->busfreq_kobject)
		pr_err("Failed to create busfreq kobject.!\n");

	if (sysfs_create_group(data->busfreq_kobject, &data->busfreq_attr_group))
		pr_err("Failed to create attributes group.!\n");

	if (register_pm_notifier(&data->exynos_buspm_notifier)) {
		pr_err("Failed to setup buspm notifier\n");
		goto err_busfreq;
	}

	data->use = true;

	if (register_reboot_notifier(&data->exynos_reboot_notifier))
		pr_err("Failed to setup reboot notifier\n");

	if (exynos_request_register(&data->exynos_request_notifier))
		pr_err("Failed to setup request notifier\n");

	platform_set_drvdata(pdev, data);

	queue_delayed_work(system_freezable_wq, &data->worker, data->sampling_rate);
	return 0;

err_busfreq:
	if (!IS_ERR(data->vdd_reg[PPMU_INT]))
		regulator_put(data->vdd_reg[PPMU_INT]);

	if (!IS_ERR(data->vdd_reg[PPMU_MIF]))
		regulator_put(data->vdd_reg[PPMU_MIF]);

	kfree(data);
	return -ENODEV;
}

static __devexit int exynos_busfreq_remove(struct platform_device *pdev)
{
	struct busfreq_data *data = platform_get_drvdata(pdev);

	unregister_pm_notifier(&data->exynos_buspm_notifier);
	unregister_reboot_notifier(&data->exynos_reboot_notifier);
	regulator_put(data->vdd_reg[PPMU_INT]);
	regulator_put(data->vdd_reg[PPMU_MIF]);
	sysfs_remove_group(data->busfreq_kobject, &data->busfreq_attr_group);
	kfree(data);

	return 0;
}

static int exynos_busfreq_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);

	if (data->busfreq_suspend)
		data->busfreq_suspend();
	return 0;
}

static int exynos_busfreq_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct busfreq_data *data = (struct busfreq_data *)platform_get_drvdata(pdev);
	ppmu_reset(dev);

	if (data->busfreq_resume)
		data->busfreq_resume();
	return 0;
}

static const struct dev_pm_ops exynos_busfreq_pm = {
	.suspend = exynos_busfreq_suspend,
	.resume = exynos_busfreq_resume,
};

static struct platform_driver exynos_busfreq_driver = {
	.probe  = exynos_busfreq_probe,
	.remove = __devexit_p(exynos_busfreq_remove),
	.driver = {
		.name   = "exynos-busfreq",
		.owner  = THIS_MODULE,
		.pm     = &exynos_busfreq_pm,
	},
};

static int __init exynos_busfreq_init(void)
{
	return platform_driver_register(&exynos_busfreq_driver);
}
late_initcall(exynos_busfreq_init);

static void __exit exynos_busfreq_exit(void)
{
	platform_driver_unregister(&exynos_busfreq_driver);
}
module_exit(exynos_busfreq_exit);
