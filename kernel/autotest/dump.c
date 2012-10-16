/*
 * Log dump support for Built-in autotest framework
 *
 * Copyright (C) 2012 Meizu Co,. Ltd.
 *
 * Author: Wu Zhangjin <falcon@meizu.com> or <wuzhangjin@gmail.com>
 * Update: Sun Apr 29 10:43:54 CST 2012
 */

#define pr_fmt(fmt) "AUTOTEST_DUMP: " fmt

#include <linux/autotest/core.h>
#include <linux/wakelock.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#define DISK_MOUNT_TIME	CONFIG_DISK_MOUNT_TIME	/* secs */

struct completion dump;

static int dump_thread(void *data)
{
	int err = 0;
	char log_file_name[100];
	char backup_log_file_name[100];

	pr_info("%s: Enter into dump_thread\n", __func__);

	/* Create the log directory */
	err = sys_mkdir(CONFIG_LAST_KMSG_LOG_DIR, 0755);
	if (err < 0 && err != -EEXIST) {
		/* It doesn't matter if it already exists */
		goto out;
	}
	/* Dump the last kernel log */
	sprintf(log_file_name, "%s/%s", CONFIG_LAST_KMSG_LOG_DIR, CONFIG_LAST_KMSG_LOG_FILE);
	pr_info("%s: Get log file name: %s\n", __func__, log_file_name);
	err = dump_last_kmsg(log_file_name);
	if (err) {
		pr_err("%s: Failed dump kernel log to %s\n", __func__, log_file_name);
		/* Create the backup log directory */
		err = sys_mkdir(CONFIG_BACKUP_LAST_KMSG_LOG_DIR, 0755);
		if (err < 0 && err != -EEXIST) {
			/* It doesn't matter if it already exists */
			goto out;
		}
		sprintf(backup_log_file_name, "%s/%s", CONFIG_BACKUP_LAST_KMSG_LOG_DIR, CONFIG_BACKUP_LAST_KMSG_LOG_FILE);
		pr_info("%s: Get backup log file name: %s\n", __func__, backup_log_file_name);
		err = dump_last_kmsg(backup_log_file_name);
		if (err) {
			pr_err("%s: Failed dump kernel log to %s\n", __func__, backup_log_file_name);
			goto out;
		} else {
			pr_info("%s: kernel log file dumped to %s\n", __func__, backup_log_file_name);
		}
	} else {
		pr_info("%s: kernel log file dumped to %s\n", __func__, log_file_name);
	}

out:
	complete_and_exit(&dump, 0);

	return err;
}

void start_dump_thread(void)
{
	struct task_struct *dump_task;
	struct wake_lock dump_wake_lock;

	init_completion(&dump);
	wake_lock_init(&dump_wake_lock, WAKE_LOCK_SUSPEND, "autotest_dump");

	/* Stop suspend and allows the admin to dump the logs*/
	wake_lock(&dump_wake_lock);

	pr_info("%s: Waiting for the disk being mounted ...\n", __func__);
	msleep_interruptible(DISK_MOUNT_TIME * MSEC_PER_SEC);

	pr_info("%s: Start dump thread\n", __func__);
	dump_task = kthread_run(dump_thread, NULL, "dump/daemon");
	if (IS_ERR(dump_task))
		pr_err("%s: Fail to create dump_thread\n", __func__);

	pr_info("%s: Waiting for the dump thread to dump the logs ...\n", __func__);
	wait_for_completion_interruptible(&dump);

	pr_info("%s: Finished log dump\n", __func__);
	wake_unlock(&dump_wake_lock);
}
