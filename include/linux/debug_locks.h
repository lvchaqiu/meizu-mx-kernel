#ifndef __LINUX_DEBUG_LOCKING_H
#define __LINUX_DEBUG_LOCKING_H

#include <linux/kernel.h>
#include <asm/atomic.h>
#include <asm/system.h>

struct task_struct;

extern int debug_locks;
extern int debug_locks_silent;


static inline int __debug_locks_off(void)
{
	return xchg(&debug_locks, 0);
}

#ifdef CONFIG_PANIC_ON_LOCK_BUG
#define panic_on_lock_bug() \
	panic("CONFIG_PANIC_ON_LOCK_BUG=y enabled for locks debugging!\n");
#else
#define panic_on_lock_bug()             do {} while (0)
#endif

static inline void dump_stack_and_panic(void)
{
	dump_stack();
	panic_on_lock_bug();
}

/*
 * Generic 'turn off all lock debugging' function:
 */
extern int debug_locks_off(void);

#define DEBUG_LOCKS_WARN_ON(c)						\
({									\
	int __ret = 0;							\
									\
	if (!oops_in_progress && unlikely(c)) {				\
		if (debug_locks_off() && !debug_locks_silent)		\
			WARN_ON(1);					\
		__ret = 1;						\
	}								\
	__ret;								\
})

#ifdef CONFIG_SMP
# define SMP_DEBUG_LOCKS_WARN_ON(c)			DEBUG_LOCKS_WARN_ON(c)
#else
# define SMP_DEBUG_LOCKS_WARN_ON(c)			do { } while (0)
#endif

#ifdef CONFIG_DEBUG_LOCKING_API_SELFTESTS
  extern void locking_selftest(void);
#else
# define locking_selftest()	do { } while (0)
#endif

struct task_struct;

#ifdef CONFIG_LOCKDEP
extern void debug_show_all_locks(void);
extern void debug_show_held_locks(struct task_struct *task);
extern void debug_check_no_locks_freed(const void *from, unsigned long len);
extern void debug_check_no_locks_held(struct task_struct *task);
#else
static inline void debug_show_all_locks(void)
{
}

static inline void debug_show_held_locks(struct task_struct *task)
{
}

static inline void
debug_check_no_locks_freed(const void *from, unsigned long len)
{
}

static inline void
debug_check_no_locks_held(struct task_struct *task)
{
}
#endif

#endif
