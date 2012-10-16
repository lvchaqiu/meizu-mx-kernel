#ifndef _LINUX_AUTOTEST_H
#define _LINUX_AUTOTEST_H

#include <linux/kthread.h>
#include <linux/random.h>

/* From 1 to max, max <= 255 */
static inline int get_random(unsigned int max)
{
	unsigned short random = 0;

#ifdef CONFIG_AUTOTEST_RANDOM
	get_random_bytes(&random, 2);
#endif
	return 1 + (random & max);
}

/* From (time) to (time * max) */
static inline int get_random_val(unsigned int max, unsigned int val)
{
	return get_random(max) * val;
}

#define get_random_secs(max, val) get_random_val(max, val)

static inline int get_random_msecs(unsigned int max, unsigned int secs)
{
	return get_random_secs(max, secs) * MSEC_PER_SEC;
}

#ifdef CONFIG_AUTOTEST
extern void start_autotest(void);
#else
#define start_autotest()	do {} while (0)
#endif

#ifdef CONFIG_DUMP_LAST_KMSG
extern void start_dump_thread(void);
extern int boot_from_crash(void);
extern int dump_last_kmsg(char *log_file);
#else
#define start_dump_thread()	do { } while (0)
#define boot_from_crash()       (0)
#define dump_last_kmsg(f)       do { } while (0)
#endif

#ifdef CONFIG_AUTOTEST_REBOOT
extern void start_reboot_thread(void);
#else
#define start_reboot_thread()	do { } while (0)
#endif

#ifdef CONFIG_AUTOTEST_SUSPEND
extern void start_suspend_thread(void);
#else
#define start_suspend_thread()	do { } while (0)
#endif

#ifdef CONFIG_SUSPEND_TIME
struct rtc_time;
extern int suspend_time_suspend(struct rtc_time before);
extern int suspend_time_resume(struct rtc_time after);
#else
#define suspend_time_suspend(t)	do { } while (0)
#define suspend_time_resume(t)	do { } while (0)
#endif

#ifdef CONFIG_UBOOT_RESUME_TIME
extern void log_suspend_wake_time(void);
extern void log_suspend_return_time(void);
#else
#define log_suspend_wake_time()	do { } while (0)
#define log_suspend_return_time()	do { } while (0)
#endif

#endif /* _LINUX_AUTOTEST_H */
