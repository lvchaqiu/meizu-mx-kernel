/*
 * Reboot test case for Built-in autotest framework
 *
 * Copyright (C) 2012 Meizu Co,. Ltd.
 *
 * Author: Wu Zhangjin <falcon@meizu.com> or <wuzhangjin@gmail.com>
 * Update: Sun Apr 29 12:45:37 CST 2012
 */

#define pr_fmt(fmt) "AUTOTEST_REBOOT: " fmt

#include <linux/autotest/core.h>
#include <linux/wakelock.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/bootmode.h>

static struct wake_lock reboot_wake_lock;

static int reboot_thread(void *data)
{
	unsigned int reboot_cycle;

	reboot_cycle = get_random_msecs(10, CONFIG_AUTOTEST_REBOOT_CYCLE);

	pr_info("%s: Enter into reboot_thread\n", __func__);

	pr_info("%s: Sleep %d ms for reboot\n", __func__, reboot_cycle);
	msleep_interruptible(reboot_cycle);

	pr_info("%s: Sync all changes to disk\n", __func__);
	/* Sync the changes from cache to disk */
	sys_sync();

#ifdef CONFIG_AUTOTEST_SUSPEND
	pr_info("%s: Stop suspend just before sending out restart command\n", __func__);
	wake_lock(&reboot_wake_lock);
#endif

	pr_info("%s: Send out the restart command ...\n", __func__);
	kernel_restart(NULL);

	return 0;
}

void start_reboot_thread(void)
{
	struct task_struct *reboot_task;

	wake_lock_init(&reboot_wake_lock, WAKE_LOCK_SUSPEND, "autotest_reboot");

#ifndef CONFIG_AUTOTEST_SUSPEND
	/* Stop suspend when doing reboot test */
	wake_lock(&reboot_wake_lock);
#endif

	pr_info("%s: Start reboot test thread\n", __func__);
	reboot_task = kthread_run(reboot_thread, NULL, "reboottest/daemon");
	if (IS_ERR(reboot_task))
		pr_err("%s: Fail to create reboot_thread\n", __func__);
}
