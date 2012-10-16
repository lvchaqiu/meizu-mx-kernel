#ifndef _ARM_WATCHPOINT_H
#define _ARM_WATCHPOINT_H

#ifdef __KERNEL__

struct task_struct;

#ifndef CONFIG_HW_PERF_EVENTS
struct arch_hw_breakpoint_ctrl {
		u32 __reserved	: 9,
		mismatch	: 1,
				: 9,
		len		: 8,
		type		: 2,
		privilege	: 2,
		enabled		: 1;
};

static inline u32 encode_ctrl_reg(struct arch_hw_breakpoint_ctrl ctrl)
{
	return (ctrl.mismatch << 22) | (ctrl.len << 5) | (ctrl.type << 3) |
		(ctrl.privilege << 1) | ctrl.enabled;
}

static inline void decode_ctrl_reg(u32 reg,
				   struct arch_hw_breakpoint_ctrl *ctrl)
{
	ctrl->enabled	= reg & 0x1;
	reg >>= 1;
	ctrl->privilege	= reg & 0x3;
	reg >>= 2;
	ctrl->type	= reg & 0x3;
	reg >>= 2;
	ctrl->len	= reg & 0xff;
	reg >>= 17;
	ctrl->mismatch	= reg & 0x1;
}

/* Debug architecture numbers. */
#define ARM_DEBUG_ARCH_RESERVED	0	/* In case of ptrace ABI updates. */
#define ARM_DEBUG_ARCH_V6	1
#define ARM_DEBUG_ARCH_V6_1	2
#define ARM_DEBUG_ARCH_V7_ECP14	3
#define ARM_DEBUG_ARCH_V7_MM	4

/* Breakpoint */
#define ARM_BREAKPOINT_EXECUTE	0

/* Watchpoints */
#define ARM_BREAKPOINT_LOAD	1
#define ARM_BREAKPOINT_STORE	2

/* Privilege Levels */
#define ARM_BREAKPOINT_PRIV	1
#define ARM_BREAKPOINT_USER	2

/* Lengths */
#define ARM_BREAKPOINT_LEN_1	0x1
#define ARM_BREAKPOINT_LEN_2	0x3
#define ARM_BREAKPOINT_LEN_4	0xf
#define ARM_BREAKPOINT_LEN_8	0xff

/* Limits */
#define ARM_MAX_BRP		16
#define ARM_MAX_WRP		16
#define ARM_MAX_HBP_SLOTS	(ARM_MAX_BRP + ARM_MAX_WRP)

/* DSCR method of entry bits. */
#define ARM_DSCR_MOE(x)			((x >> 2) & 0xf)
#define ARM_ENTRY_BREAKPOINT		0x1
#define ARM_ENTRY_ASYNC_WATCHPOINT	0x2
#define ARM_ENTRY_SYNC_WATCHPOINT	0xa

/* DSCR monitor/halting bits. */
#define ARM_DSCR_HDBGEN		(1 << 14)
#define ARM_DSCR_MDBGEN		(1 << 15)

/* opcode2 numbers for the co-processor instructions. */
#define ARM_OP2_BVR		4
#define ARM_OP2_BCR		5
#define ARM_OP2_WVR		6
#define ARM_OP2_WCR		7

/* Base register numbers for the debug registers. */
#define ARM_BASE_BVR		64
#define ARM_BASE_BCR		80
#define ARM_BASE_WVR		96
#define ARM_BASE_WCR		112

/* Accessor macros for the debug registers. */
#define ARM_DBG_READ(M, OP2, VAL) do {\
	asm volatile("mrc p14, 0, %0, c0," #M ", " #OP2 : "=r" (VAL));\
} while (0)

#define ARM_DBG_WRITE(M, OP2, VAL) do {\
	asm volatile("mcr p14, 0, %0, c0," #M ", " #OP2 : : "r" (VAL));\
} while (0)

struct notifier_block;

#endif

extern int arm_install_watchpoint(u32 addr, int rw_flag);
extern void arm_unstall_watchpoint(void);

#endif	/* __KERNEL__ */
#endif	/* _ARM_WATCHPOINT_H */
