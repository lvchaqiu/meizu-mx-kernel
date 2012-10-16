/*
 * Built-in autotest framework
 *
 * Copyright (C) 2012 Meizu Co,. Ltd.
 *
 * Author: Wu Zhangjin <falcon@meizu.com> or <wuzhangjin@gmail.com>
 * Update: Sun Apr 29 10:43:54 CST 2012
 */

#define pr_fmt(fmt) "AUTOTEST: " fmt

#include <linux/autotest/core.h>

static void autotest_run_tests(void)
{
	start_reboot_thread();
	start_suspend_thread();
}

static int autotest_thread(void *data)
{
	pr_info("%s: start autotest\n", __func__);
	if (boot_from_crash()) {
		/* Start a thread to dump out the last kernel message */
		start_dump_thread();
	}

	pr_info("%s: Start run autotest test cases\n", __func__);
	autotest_run_tests();

	return 0;
}

void __init start_autotest(void)
{
	struct task_struct *autotest_task;

	pr_info("%s: Start autotest thread\n", __func__);
	autotest_task = kthread_run(autotest_thread, NULL, "autotest/daemon");
	if (IS_ERR(autotest_task))
		pr_err("%s: Failed to create autotest_thread\n", __func__);
}
