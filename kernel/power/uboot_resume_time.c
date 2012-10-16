/*
 * kernel/power/uboot_resume_time.c - Log time spent in uboot during resume
 *
 * Copyright (c) 2012 Meizu Co., Ltd
 * Author: Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * This file is released under the GPLv2.
 */

#define pr_fmt(fmt) "AUTOTEST_UBOOT_RESUME_TIME: " fmt

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/rtc.h>
#include <linux/syscore_ops.h>
#include <linux/autotest/core.h>

#include "power.h"

static bool suspend_sucess;
static struct timespec suspend_wake_time;
static struct timespec suspend_return_time;
static unsigned long long sleep_time_bins[32];
static unsigned long total_resume;
static struct timespec total_uboot_resume_time = {0, 0};

extern unsigned long suspend_test_suspend_time;

#ifdef CONFIG_DEBUG_FS
static int uboot_resume_time_debug_show(struct seq_file *s, void *data)
{
	int bin;
	unsigned long avg_msecs;

	seq_printf(s, "time (msecs)  count\n");
	seq_printf(s, "------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (sleep_time_bins[bin] == 0)
			continue;
		seq_printf(s, "%4d - %4d %8llu\n",
			bin ? 1 << (bin - 1) : 0, 1 << bin,
				sleep_time_bins[bin]);
	}

	avg_msecs = (total_uboot_resume_time.tv_sec * MSEC_PER_SEC +
		total_uboot_resume_time.tv_nsec / NSEC_PER_MSEC) / total_resume;

	seq_printf(s, "total: %lu.%03lu seconds, %lu times, avg %lu msecs\n",
		total_uboot_resume_time.tv_sec,
		total_uboot_resume_time.tv_nsec / NSEC_PER_MSEC,
		total_resume, avg_msecs);
	return 0;
}

static int uboot_resume_time_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_resume_time_debug_show, NULL);
}

static const struct file_operations uboot_resume_time_debug_fops = {
	.open		= uboot_resume_time_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init uboot_resume_time_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("uboot_resume_time", 0755, NULL, NULL,
		&uboot_resume_time_debug_fops);
	if (!d) {
		pr_err("Failed to create uboot_resume_time debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(uboot_resume_time_debug_init);
#endif

void log_suspend_wake_time(void)
{
	struct timespec suspend_time = {0, 0};

	read_hr_persistent_clock(&suspend_wake_time);
	suspend_time.tv_sec = suspend_test_suspend_time,
	suspend_wake_time = timespec_add(suspend_wake_time, suspend_time);
}

void log_suspend_return_time(void)
{
	read_hr_persistent_clock(&suspend_return_time);
}

void show_suspend_statistic(void)
{
	struct timespec diff;
	unsigned long diff_msecs, avg_msecs;

	if (!suspend_sucess)
		return;
	else
		suspend_sucess = 0;

	diff = timespec_sub(suspend_return_time, suspend_wake_time);
	/* Filter the possible wrong value */
	if (diff.tv_sec < 0 || diff.tv_sec > 100)
		diff.tv_sec = 0;

	diff_msecs = diff.tv_sec * MSEC_PER_SEC + diff.tv_nsec / NSEC_PER_MSEC;
	sleep_time_bins[fls(diff_msecs)]++;
	total_uboot_resume_time = timespec_add(total_uboot_resume_time, diff);
	total_resume++;

	avg_msecs = (total_uboot_resume_time.tv_sec * MSEC_PER_SEC +
		total_uboot_resume_time.tv_nsec / NSEC_PER_MSEC) / total_resume;

	pr_info("Resume spent %lu.%03lu seconds in uboot, total %lu.%03lu seconds, %lu times, avg %lu msecs\n",
		diff.tv_sec, diff.tv_nsec / NSEC_PER_MSEC,
		total_uboot_resume_time.tv_sec, total_uboot_resume_time.tv_nsec / NSEC_PER_MSEC,
		total_resume, avg_msecs);
}

static int uboot_resume_time_notify_pm(struct notifier_block *nb, unsigned long event, void *buf)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_RESTORE_PREPARE:	/* do we need this ?? */
		show_suspend_statistic();
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block pm_notifier = {
	.notifier_call = uboot_resume_time_notify_pm,
};

static int uboot_resume_time_syscore_suspend(void)
{
	suspend_sucess = 1;

	return 0;
}

static struct syscore_ops uboot_resume_time_syscore_ops = {
	.suspend = uboot_resume_time_syscore_suspend,
};

static int __init uboot_resume_time_init(void)
{
	int ret = 0;

	register_syscore_ops(&uboot_resume_time_syscore_ops);
	ret = register_pm_notifier(&pm_notifier);
	if (ret)
		pr_err("can't register pm notifier\n");

	return ret;
}
late_initcall(uboot_resume_time_init);
