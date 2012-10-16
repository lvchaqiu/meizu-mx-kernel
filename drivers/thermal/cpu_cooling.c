/*
  *  linux/drivers/thermal/cpu_cooling.c
  *
  *  Copyright (C) 2011	Samsung Electronics Co., Ltd(http://www.samsung.com)
  *  Copyright (C) 2011  Amit Daniel <amit.kachhap@xxxxxxxxxx>
  *
  * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; version 2 of the License.
  *
  *  This program is distributed in the hope that it will be useful, but
  *  WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  *  General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License along
  *  with this program; if not, write to the Free Software Foundation, Inc.,
  *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
  *
  * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>

#ifdef CONFIG_EXYNOS_CPUFREQ
#include <linux/exynos-cpufreq.h>
#endif

#ifdef CONFIG_CPU_FREQ
	struct cpufreq_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	struct freq_pctg_table *tab_ptr;
	unsigned int tab_size;
	unsigned int cpufreq_state;
	const struct cpumask *allowed_cpus;
	struct list_head node;
};

static LIST_HEAD(cooling_cpufreq_list);
static DEFINE_MUTEX(cooling_cpufreq_lock);
static DEFINE_IDR(cpufreq_idr);
static struct cpufreq_cooling_device *notify_cpufreq;
//static DEFINE_PER_CPU(unsigned int, max_policy_freq);
#endif /*CONFIG_CPU_FREQ*/

#ifdef CONFIG_HOTPLUG_CPU
struct hotplug_cooling_device {
	int id;
	struct thermal_cooling_device *cool_dev;
	unsigned int hotplug_state;
	const struct cpumask *allowed_cpus;
	struct list_head node;
};

static LIST_HEAD(cooling_cpuhotplug_list);
static DEFINE_MUTEX(cooling_cpuhotplug_lock);
static DEFINE_IDR(cpuhotplug_idr);
#endif /*CONFIG_HOTPLUG_CPU*/

static int get_idr(struct idr *idr, struct mutex *lock, int *id)
{
	int err;
again:
	if (unlikely(idr_pre_get(idr, GFP_KERNEL) == 0))
		return -ENOMEM;

	if (lock)
		mutex_lock(lock);
	err = idr_get_new(idr, NULL, id);
	if (lock)
		mutex_unlock(lock);
	if (unlikely(err == -EAGAIN))
		goto again;
	else if (unlikely(err))
		return err;

	*id = *id & MAX_ID_MASK;
	return 0;
}

static void release_idr(struct idr *idr, struct mutex *lock, int id)
{
	if (lock)
		mutex_lock(lock);
	idr_remove(idr, id);
	if (lock)
		mutex_unlock(lock);
}

#ifdef CONFIG_CPU_FREQ
/*Below codes defines functions to be used for cpufreq as cooling device*/
static bool is_cpufreq_valid(int cpu)
{
	struct cpufreq_policy policy;
	if (!cpufreq_get_policy(&policy, cpu))
		return true;
	return false;
}

static void cpufreq_update_policy_max(struct cpufreq_cooling_device *cpufreq_device,
				struct cpufreq_policy *policy)
{
	struct freq_pctg_table *th_table;
	unsigned long max_freq = 0;
	unsigned int th_pctg = 0, level;

	level = cpufreq_device->cpufreq_state;

	if (level > 0) {
		th_table = 
			&(cpufreq_device->tab_ptr[level - 1]);
		th_pctg = th_table->freq_clip_pctg;
	}

	max_freq = (policy->cpuinfo.max_freq * (100 - th_pctg)) / 100;
	max_freq = (max_freq / (100*1000)) * (100*1000);

	policy->cpu_freq_limit = max_freq;
	
	pr_info("%s: level:%d, th_pctg:%d, max_freq:%lu\n",
				 __func__, level, th_pctg, max_freq);
}

static int cpufreq_apply_cooling(struct cpufreq_cooling_device *cpufreq_device,
				unsigned long cooling_state)
{
	struct cpufreq_policy *policy=NULL;
	int this_cpu = smp_processor_id();

	if (!is_cpufreq_valid(this_cpu))
		return 0;

	if (cooling_state > cpufreq_device->tab_size)
		return -EINVAL;

	/*Check if last cooling level is same as current cooling level*/
	if (cpufreq_device->cpufreq_state == cooling_state)
		return 0;

	cpufreq_device->cpufreq_state = cooling_state;

	/*cpufreq thermal notifier uses this cpufreq device pointer*/
	notify_cpufreq = cpufreq_device;
	policy = cpufreq_cpu_get(0);
	if (!policy)
		goto err_policy;

	cpufreq_update_policy_max(cpufreq_device, policy);

	cpufreq_cpu_put(policy);

	return 0;

err_policy:
	WARN_ON(1);
	return -EINVAL;
}


/*
  * cpufreq cooling device callback functions
  */
static int cpufreq_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpufreq_cooling_device *cpufreq_device = NULL;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_device, &cooling_cpufreq_list, node)
		if (cpufreq_device && cpufreq_device->cool_dev == cdev)
			break;

	mutex_unlock(&cooling_cpufreq_lock);
	if (!cpufreq_device || cpufreq_device->cool_dev != cdev)
		return -EINVAL;

	*state = cpufreq_device->tab_size;
	return 0;
}

static int cpufreq_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpufreq_cooling_device *cpufreq_device = NULL;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_device, &cooling_cpufreq_list, node)
		if (cpufreq_device && cpufreq_device->cool_dev == cdev)
			break;

	mutex_unlock(&cooling_cpufreq_lock);
	if (!cpufreq_device || cpufreq_device->cool_dev != cdev)
		return -EINVAL;
	*state = cpufreq_device->cpufreq_state;
	return 0;
}

/*This cooling may be as PASSIVE/ACTIVE type*/
static int cpufreq_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct cpufreq_cooling_device *cpufreq_device = NULL;
	
	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_device, &cooling_cpufreq_list, node)
		if (cpufreq_device && cpufreq_device->cool_dev == cdev)
			break;

	mutex_unlock(&cooling_cpufreq_lock);
	if (!cpufreq_device || cpufreq_device->cool_dev != cdev)
		return -EINVAL;

	cpufreq_apply_cooling(cpufreq_device, state);

	return 0;
}

/* bind cpufreq callbacks to cpufreq cooling device */
static struct thermal_cooling_device_ops cpufreq_cooling_ops = {
	.get_max_state = cpufreq_get_max_state,
	.get_cur_state = cpufreq_get_cur_state,
	.set_cur_state = cpufreq_set_cur_state,
};


struct thermal_cooling_device *cpufreq_cooling_register(
	struct freq_pctg_table *tab_ptr, unsigned int tab_size,
	const struct cpumask *mask_val)
{
	struct thermal_cooling_device *cool_dev;
	struct cpufreq_cooling_device *cpufreq_dev = NULL;
	unsigned int count = 0;
	char dev_name[THERMAL_NAME_LENGTH];
	int ret = 0, id = 0;

	if (tab_ptr == NULL || tab_size == 0)
		return ERR_PTR(-EINVAL);

	list_for_each_entry(cpufreq_dev, &cooling_cpufreq_list, node)
		count++;

	cpufreq_dev =
		kzalloc(sizeof(struct cpufreq_cooling_device), GFP_KERNEL);

	if (!cpufreq_dev)
		return ERR_PTR(-ENOMEM);

	cpufreq_dev->tab_ptr = tab_ptr;
	cpufreq_dev->tab_size = tab_size;
	cpufreq_dev->allowed_cpus = mask_val;

	ret = get_idr(&cpufreq_idr, &cooling_cpufreq_lock, &cpufreq_dev->id);
	if (ret) {
		kfree(cpufreq_dev);
		return ERR_PTR(-EINVAL);
	}

	sprintf(dev_name, "thermal-cpufreq-%d", cpufreq_dev->id);

	cool_dev = thermal_cooling_device_register(dev_name, cpufreq_dev,
						&cpufreq_cooling_ops);
	if (!cool_dev) {
		release_idr(&cpufreq_idr, &cooling_cpufreq_lock,
						cpufreq_dev->id);
		kfree(cpufreq_dev);
		return ERR_PTR(-EINVAL);
	}
	cpufreq_dev->id = id;
	cpufreq_dev->cool_dev = cool_dev;
	mutex_lock(&cooling_cpufreq_lock);
	list_add_tail(&cpufreq_dev->node, &cooling_cpufreq_list);
	mutex_unlock(&cooling_cpufreq_lock);
	return cool_dev;
}
EXPORT_SYMBOL(cpufreq_cooling_register);

void cpufreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct cpufreq_cooling_device *cpufreq_dev = NULL;
	unsigned int count = 0;

	mutex_lock(&cooling_cpufreq_lock);
	list_for_each_entry(cpufreq_dev, &cooling_cpufreq_list, node) {
	if (cpufreq_dev && cpufreq_dev->cool_dev == cdev)
			break;
		count++;
	}

	if (!cpufreq_dev || cpufreq_dev->cool_dev != cdev) {
		mutex_unlock(&cooling_cpufreq_lock);
		return;
	}

	list_del(&cpufreq_dev->node);
	mutex_unlock(&cooling_cpufreq_lock);
	thermal_cooling_device_unregister(cpufreq_dev->cool_dev);
	release_idr(&cpufreq_idr, &cooling_cpufreq_lock, cpufreq_dev->id);
	kfree(cpufreq_dev);
}
EXPORT_SYMBOL(cpufreq_cooling_unregister);
#endif /*CONFIG_CPU_FREQ*/


#ifdef CONFIG_HOTPLUG_CPU
/*
 * cpu hotplug cooling device callback functions
 */
static int cpuhotplug_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct hotplug_cooling_device *hotplug_dev = NULL;

	mutex_lock(&cooling_cpuhotplug_lock);
	list_for_each_entry(hotplug_dev, &cooling_cpuhotplug_list, node)
		if (hotplug_dev && hotplug_dev->cool_dev == cdev)
			break;

	mutex_unlock(&cooling_cpuhotplug_lock);
	if (!hotplug_dev || hotplug_dev->cool_dev != cdev)
		return -EINVAL;
	*state = 1;
	return 0;
}

static int cpuhotplug_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct hotplug_cooling_device *hotplug_dev = NULL;

	mutex_lock(&cooling_cpuhotplug_lock);
	list_for_each_entry(hotplug_dev, &cooling_cpuhotplug_list, node)
		if (hotplug_dev && hotplug_dev->cool_dev == cdev)
			break;

	mutex_unlock(&cooling_cpuhotplug_lock);
	if (!hotplug_dev || hotplug_dev->cool_dev != cdev)
		return -EINVAL;

	/*
	* This cooling device may be of type ACTIVE, so state field can
	* be 0 or 1
	*/
	*state = hotplug_dev->hotplug_state;
	return 0;
}

/*This cooling may be as ACTIVE type*/
static int cpuhotplug_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	int cpuid, this_cpu = smp_processor_id();
	struct hotplug_cooling_device *hotplug_dev = NULL;

	mutex_lock(&cooling_cpuhotplug_lock);
	list_for_each_entry(hotplug_dev, &cooling_cpuhotplug_list, node)
	if (hotplug_dev && hotplug_dev->cool_dev == cdev)
			break;

	mutex_unlock(&cooling_cpuhotplug_lock);
	if (!hotplug_dev || hotplug_dev->cool_dev != cdev)
		return -EINVAL;

	if (hotplug_dev->hotplug_state == state)
		return 0;

	/*
	* This cooling device may be of type ACTIVE, so state field can
	* be 0 or 1
	*/
	if (state == 1) {
		for_each_cpu(cpuid, hotplug_dev->allowed_cpus) {
			if (cpu_online(cpuid) && (cpuid != this_cpu))
				cpu_down(cpuid);
		}
	} else if (state == 0) {
		for_each_cpu(cpuid, hotplug_dev->allowed_cpus) {
			if (!cpu_online(cpuid) && (cpuid != this_cpu))
				cpu_up(cpuid);
		}
	} else {
		return -EINVAL;
	}

	hotplug_dev->hotplug_state = state;

	return 0;
}

/* bind hotplug callbacks to cpu hotplug cooling device */
static struct thermal_cooling_device_ops cpuhotplug_cooling_ops = {
	.get_max_state = cpuhotplug_get_max_state,
	.get_cur_state = cpuhotplug_get_cur_state,
	.set_cur_state = cpuhotplug_set_cur_state,
};

struct thermal_cooling_device *cpuhotplug_cooling_register(
	const struct cpumask *mask_val)
{
	struct thermal_cooling_device *cool_dev;
	struct hotplug_cooling_device *hotplug_dev;
	int ret = 0;
	char dev_name[THERMAL_NAME_LENGTH];

	hotplug_dev =
		kzalloc(sizeof(struct hotplug_cooling_device), GFP_KERNEL);

	if (!hotplug_dev)
		return ERR_PTR(-ENOMEM);

	ret = get_idr(&cpuhotplug_idr, &cooling_cpuhotplug_lock,
			&hotplug_dev->id);
	if (ret) {
		kfree(hotplug_dev);
		return ERR_PTR(-EINVAL);
	}

	sprintf(dev_name, "cpu-hotplug-%u", hotplug_dev->id);

	hotplug_dev->hotplug_state = 0;
	hotplug_dev->allowed_cpus = mask_val;
	cool_dev = thermal_cooling_device_register(dev_name, hotplug_dev,
						&cpuhotplug_cooling_ops);
	if (!cool_dev) {
		release_idr(&cpuhotplug_idr, &cooling_cpuhotplug_lock,
				hotplug_dev->id);
		kfree(hotplug_dev);
		return ERR_PTR(-EINVAL);
	}

	hotplug_dev->cool_dev = cool_dev;
	mutex_lock(&cooling_cpuhotplug_lock);
	list_add_tail(&hotplug_dev->node, &cooling_cpuhotplug_list);
	mutex_unlock(&cooling_cpuhotplug_lock);

	return cool_dev;
}
EXPORT_SYMBOL(cpuhotplug_cooling_register);

void cpuhotplug_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct hotplug_cooling_device *hotplug_dev = NULL;

	mutex_lock(&cooling_cpuhotplug_lock);
	list_for_each_entry(hotplug_dev, &cooling_cpuhotplug_list, node)
		if (hotplug_dev && hotplug_dev->cool_dev == cdev)
			break;

	if (!hotplug_dev || hotplug_dev->cool_dev != cdev) {
		mutex_unlock(&cooling_cpuhotplug_lock);
	return;
	}

	list_del(&hotplug_dev->node);
	mutex_unlock(&cooling_cpuhotplug_lock);
	thermal_cooling_device_unregister(hotplug_dev->cool_dev);
	release_idr(&cpuhotplug_idr, &cooling_cpuhotplug_lock,
						hotplug_dev->id);
	kfree(hotplug_dev);
}
EXPORT_SYMBOL(cpuhotplug_cooling_unregister);
#endif /*CONFIG_HOTPLUG_CPU*/
