/*
 * kernel/power/suspend_test.c - Suspend to RAM and standby test facility.
 *
 * Copyright (c) 2009 Pavel Machek <pavel@ucw.cz>
 *
 * This file is released under the GPLv2.
 */

#define pr_fmt(fmt) "AUTOTEST_SUSPEND: " fmt

#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/syscore_ops.h>
#include <linux/autotest/core.h>

#include "power.h"

/*
 * We test the system suspend code by setting an RTC wakealarm a short
 * time in the future, then suspending.  Suspending the devices won't
 * normally take long ... some systems only need a few milliseconds.
 *
 * The time it takes is system-specific though, so when we test this
 * during system bootup we allow a LOT of time.
 */
#define TEST_SUSPEND_SECONDS	10

static unsigned long suspend_test_start_time;
unsigned long suspend_test_suspend_time;
extern int write_something_to_emmc(char* logfile);

void suspend_test_start(void)
{
	/* FIXME Use better timebase than "jiffies", ideally a clocksource.
	 * What we want is a hardware counter that will work correctly even
	 * during the irqs-are-off stages of the suspend/resume cycle...
	 */
	suspend_test_start_time = jiffies;
}

void suspend_test_finish(const char *label)
{
	long nj = jiffies - suspend_test_start_time;
	unsigned msec;

	msec = jiffies_to_msecs(abs(nj));
	pr_info("PM: %s took %d.%03d seconds\n", label,
			msec / 1000, msec % 1000);

	/* We want to emulate all potential wake sources, don't warm here. */
#if 0
	/* Warning on suspend means the RTC alarm period needs to be
	 * larger -- the system was sooo slooowwww to suspend that the
	 * alarm (should have) fired before the system went to sleep!
	 *
	 * Warning on either suspend or resume also means the system
	 * has some performance issues.  The stack dump of a WARN_ON
	 * is more likely to get the right attention than a printk...
	 */
	WARN(msec > (suspend_test_suspend_time * 1000),
	     "Component: %s, time: %u\n", label, msec);
#endif
}

/*
 * To test system suspend, we need a hands-off mechanism to resume the
 * system.  RTCs wake alarms are a common self-contained mechanism.
 */

static char		warn_no_rtc[] =
		KERN_WARNING "PM: no wakealarm-capable RTC driver is ready\n";

static void set_wakealarm(void)
{
	ssize_t retval;
	unsigned long now, alarm;
	struct rtc_wkalrm alm;
	struct rtc_device *rtc;

	rtc = rtc_class_open(CONFIG_WAKEALARM_RTC);
	if (!rtc) {
		printk(warn_no_rtc);
		return;
	}

	/* Only request alarms that trigger in the future.  Disable them
	 * by writing another time, e.g. 0 meaning Jan 1 1970 UTC.
	 */
	retval = rtc_read_time(rtc, &alm.time);
	log_suspend_wake_time();
	if (retval < 0)
		goto close_rtc;
	rtc_tm_to_time(&alm.time, &now);

	suspend_time_suspend(alm.time);

	alarm = now + suspend_test_suspend_time;
	if (alarm > now) {
		/* Avoid accidentally clobbering active alarms; we can't
		 * entirely prevent that here, without even the minimal
		 * locking from the /dev/rtcN api.
		 */
		retval = rtc_read_alarm(rtc, &alm);
		if (retval < 0)
			goto close_rtc;

		alm.enabled = 1;
	} else {
		alm.enabled = 0;

		/* Provide a valid future alarm time.  Linux isn't EFI,
		 * this time won't be ignored when disabling the alarm.
		 */
		alarm = now + 300;
	}
	rtc_time_to_tm(alarm, &alm.time);

	/* The standard interface is rtc_set_alarm(), but the new m03x
	 * phones adds a standalone boot alarm and gives up the old rtc
	 * driver, so, we only have one standard alarm but this standard
	 * one is reserved for the user-space android, hence, we
	 * must use the boot alarm interface instead below.
	 */
	rtc_set_boot_alarm(rtc, &alm);

close_rtc:
	rtc_class_close(rtc);
}

static void restore_wakealarm(void)
{
	struct rtc_wkalrm	alm;
	struct rtc_device	*rtc = NULL;
	struct rtc_time		after;

	rtc = rtc_class_open(CONFIG_WAKEALARM_RTC);
	if (!rtc) {
		printk(warn_no_rtc);
		return;
	}
	rtc_read_time(rtc, &after);
	suspend_time_resume(after);

	/* Some platforms can't detect that the alarm triggered the
	 * wakeup, or (accordingly) disable it after it afterwards.
	 * It's supposed to give oneshot behavior; cope.
	 */
	alm.enabled = false;
	rtc_set_boot_alarm(rtc, &alm);
	rtc_class_close(rtc);
}

void setup_test_suspend(unsigned long suspend_time)
{
	/* test_suspend only set the suspend time, the real suspend is
	 * triggered by system itself
	 */
	suspend_test_suspend_time = suspend_time;
}
static int test_suspend_notify_pm(struct notifier_block *nb, unsigned long event, void *buf)
{
	switch (event) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		set_wakealarm();
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_RESTORE_PREPARE:	/* do we need this ?? */
		restore_wakealarm();
		write_something_to_emmc("/data/test_emmc.log");
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block pm_notifier = {
	.notifier_call = test_suspend_notify_pm,
};

static int __init test_suspend_init(void)
{
	int ret = 0;

	ret = register_pm_notifier(&pm_notifier);
	if (ret)
		pr_err("can't register pm notifier\n");

	return ret;
}
late_initcall(test_suspend_init);
