/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2012 Meizu Co,. Ltd.
 * Author: Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * Copyright (C) 2009, 2010 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

/*
 * HW_breakpoint: a unified kernel/user-space hardware breakpoint facility,
 * using the CPU's debug registers.
 */
#define pr_fmt(fmt) "watchpoint: " fmt

#include <linux/errno.h>
#include <linux/hardirq.h>
#include <linux/cpu.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/signal.h>
#include <asm/watchpoint.h>
#include <asm/system.h>
#include <asm/traps.h>
#include <asm-generic/siginfo.h>

/* Debug architecture version. */
static u8 debug_arch;

#define READ_WB_REG_CASE(OP2, M, VAL)		\
	case ((OP2 << 4) + M):			\
		ARM_DBG_READ(c ## M, OP2, VAL); \
		break

#define WRITE_WB_REG_CASE(OP2, M, VAL)		\
	case ((OP2 << 4) + M):			\
		ARM_DBG_WRITE(c ## M, OP2, VAL);\
		break

#define GEN_READ_WB_REG_CASES(OP2, VAL)		\
	READ_WB_REG_CASE(OP2, 0, VAL);		\
	READ_WB_REG_CASE(OP2, 1, VAL);		\
	READ_WB_REG_CASE(OP2, 2, VAL);		\
	READ_WB_REG_CASE(OP2, 3, VAL);		\
	READ_WB_REG_CASE(OP2, 4, VAL);		\
	READ_WB_REG_CASE(OP2, 5, VAL);		\
	READ_WB_REG_CASE(OP2, 6, VAL);		\
	READ_WB_REG_CASE(OP2, 7, VAL);		\
	READ_WB_REG_CASE(OP2, 8, VAL);		\
	READ_WB_REG_CASE(OP2, 9, VAL);		\
	READ_WB_REG_CASE(OP2, 10, VAL);		\
	READ_WB_REG_CASE(OP2, 11, VAL);		\
	READ_WB_REG_CASE(OP2, 12, VAL);		\
	READ_WB_REG_CASE(OP2, 13, VAL);		\
	READ_WB_REG_CASE(OP2, 14, VAL);		\
	READ_WB_REG_CASE(OP2, 15, VAL)

#define GEN_WRITE_WB_REG_CASES(OP2, VAL)	\
	WRITE_WB_REG_CASE(OP2, 0, VAL);		\
	WRITE_WB_REG_CASE(OP2, 1, VAL);		\
	WRITE_WB_REG_CASE(OP2, 2, VAL);		\
	WRITE_WB_REG_CASE(OP2, 3, VAL);		\
	WRITE_WB_REG_CASE(OP2, 4, VAL);		\
	WRITE_WB_REG_CASE(OP2, 5, VAL);		\
	WRITE_WB_REG_CASE(OP2, 6, VAL);		\
	WRITE_WB_REG_CASE(OP2, 7, VAL);		\
	WRITE_WB_REG_CASE(OP2, 8, VAL);		\
	WRITE_WB_REG_CASE(OP2, 9, VAL);		\
	WRITE_WB_REG_CASE(OP2, 10, VAL);	\
	WRITE_WB_REG_CASE(OP2, 11, VAL);	\
	WRITE_WB_REG_CASE(OP2, 12, VAL);	\
	WRITE_WB_REG_CASE(OP2, 13, VAL);	\
	WRITE_WB_REG_CASE(OP2, 14, VAL);	\
	WRITE_WB_REG_CASE(OP2, 15, VAL)

static void write_wb_reg(int n, u32 val)
{
	switch (n) {
	GEN_WRITE_WB_REG_CASES(ARM_OP2_BVR, val);
	GEN_WRITE_WB_REG_CASES(ARM_OP2_BCR, val);
	GEN_WRITE_WB_REG_CASES(ARM_OP2_WVR, val);
	GEN_WRITE_WB_REG_CASES(ARM_OP2_WCR, val);
	default:
		pr_warning("attempt to write to unknown breakpoint "
				"register %d\n", n);
	}
	isb();
}

/* Determine debug architecture. */
static u8 get_debug_arch(void)
{
	u32 didr;

	/* Do we implement the extended CPUID interface? */
	if (WARN_ONCE((((read_cpuid_id() >> 16) & 0xf) != 0xf),
	    "CPUID feature registers not supported. "
	    "Assuming v6 debug is present.\n"))
		return ARM_DEBUG_ARCH_V6;

	ARM_DBG_READ(c0, 0, didr);
	return (didr >> 16) & 0xf;
}

/*
 * In order to access the breakpoint/watchpoint control registers,
 * we must be running in debug monitor mode. Unfortunately, we can
 * be put into halting debug mode at any time by an external debugger
 * but there is nothing we can do to prevent that.
 */
static int enable_monitor_mode(void)
{
	u32 dscr;
	int ret = 0;

	ARM_DBG_READ(c1, 0, dscr);

	/* Ensure that halting mode is disabled. */
	if (WARN_ONCE(dscr & ARM_DSCR_HDBGEN,
			"halting debug mode enabled. Unable to access hardware resources.\n")) {
		ret = -EPERM;
		goto out;
	}

	/* If monitor mode is already enabled, just return. */
	if (dscr & ARM_DSCR_MDBGEN)
		goto out;

	/* Write to the corresponding DSCR. */
	switch (get_debug_arch()) {
	case ARM_DEBUG_ARCH_V6:
	case ARM_DEBUG_ARCH_V6_1:
		ARM_DBG_WRITE(c1, 0, (dscr | ARM_DSCR_MDBGEN));
		break;
	case ARM_DEBUG_ARCH_V7_ECP14:
		ARM_DBG_WRITE(c2, 2, (dscr | ARM_DSCR_MDBGEN));
		break;
	default:
		ret = -ENODEV;
		goto out;
	}

	/* Check that the write made it through. */
	ARM_DBG_READ(c1, 0, dscr);
	if (!(dscr & ARM_DSCR_MDBGEN))
		ret = -EPERM;

out:
	return ret;
}

/*
 * Install a watchpoint.
 *
 * @addr: The virtual address want to watch
 * @rw: monitor the accessing type, read, write or read & write
 *      available choices: ARM_BREAKPOINT_LOAD, ARM_BREAKPOINT_STORE
 *      or ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE.
 */
int arm_install_watchpoint(u32 addr, int rw_flag)
{
	struct arch_hw_breakpoint_ctrl ctrl;
	u32 ctrl_encoded;

	/* Init */
	addr &= ~0x3;			/* 4 bytes aligned */
	ctrl.type = rw_flag;
	ctrl.len = ARM_BREAKPOINT_LEN_4;
	ctrl.privilege = ARM_BREAKPOINT_PRIV;	/* watch kernel space */
	ctrl.mismatch = 0;

	pr_info("%s: installed watchpoint at 0x%x\n", __func__, addr);

	/* Step 1. Disable the watchpoint being set. */
	write_wb_reg(ARM_BASE_WCR, 0);
	/* Step 2. Write address to the DBGDSWVR, leaving the bottom 3 bits zero. */
	write_wb_reg(ARM_BASE_WVR, addr);
	/* Step 3. Determine the ctrl value to use. */
	ctrl_encoded = encode_ctrl_reg(ctrl) | 0x1;
	/* Step 4. Write the mask and control register to enable the watchpoint. */
	write_wb_reg(ARM_BASE_WCR, ctrl_encoded);

	return 0;
}

void arm_uninstall_watchpoint(void)
{
	write_wb_reg(ARM_BASE_WCR, 0);
}

/*
 * One-time initialisation.
 */
static void reset_ctrl_regs(void *info)
{
	int i, cpu = smp_processor_id();
	u32 dbg_power;
	cpumask_t *cpumask = info;

	/*
	 * v7 debug contains save and restore registers so that debug state
	 * can be maintained across low-power modes without leaving the debug
	 * logic powered up. It is IMPLEMENTATION DEFINED whether we can access
	 * the debug registers out of reset, so we must unlock the OS Lock
	 * Access Register to avoid taking undefined instruction exceptions
	 * later on.
	 */
	if (debug_arch >= ARM_DEBUG_ARCH_V7_ECP14) {
		/*
		 * Ensure sticky power-down is clear (i.e. debug logic is
		 * powered up).
		 */
		asm volatile("mrc p14, 0, %0, c1, c5, 4" : "=r" (dbg_power));
		if ((dbg_power & 0x1) == 0) {
			pr_warning("CPU %d debug is powered down!\n", cpu);
			cpumask_or(cpumask, cpumask, cpumask_of(cpu));
			return;
		}

		/*
		 * Unconditionally clear the lock by writing a value
		 * other than 0xC5ACCE55 to the access register.
		 */
		asm volatile("mcr p14, 0, %0, c1, c0, 4" : : "r" (0));
		isb();

		/*
		 * Clear any configured vector-catch events before
		 * enabling monitor mode.
		 */
		asm volatile("mcr p14, 0, %0, c0, c7, 0" : : "r" (0));
		isb();
	}

	if (enable_monitor_mode())
		return;

	for (i = 0; i < 1; ++i) {
		write_wb_reg(ARM_BASE_WCR + i, 0UL);
		write_wb_reg(ARM_BASE_WVR + i, 0UL);
	}
}

static int __cpuinit dbg_reset_notify(struct notifier_block *self,
				      unsigned long action, void *cpu)
{
	if (action == CPU_ONLINE)
		smp_call_function_single((int)cpu, reset_ctrl_regs, NULL, 1);
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata dbg_reset_nb = {
	.notifier_call = dbg_reset_notify,
};

/*
 * Called from either the Data Abort Handler [watchpoint]
 */
static int watchpoint_handler(unsigned long addr, unsigned int fsr,
				 struct pt_regs *regs)
{
	arm_uninstall_watchpoint();
	pr_info("watchpoint fired: address = 0x%lx\n", addr);
	__show_regs(regs);
	__backtrace();

	return 0;
}

static int __init arch_watchpoint_init(void)
{
	cpumask_t cpumask = { CPU_BITS_NONE };

	/*
	 * Reset the breakpoint resources. We assume that a halting
	 * debugger will leave the world in a nice state for us.
	 */
	on_each_cpu(reset_ctrl_regs, &cpumask, 1);

	/* Register debug fault handler. */
	hook_fault_code(2, watchpoint_handler, SIGTRAP, TRAP_HWBKPT,
			"watchpoint debug exception");

	/* Register hotplug notifier. */
	register_cpu_notifier(&dbg_reset_nb);

	pr_info("%s\n", __func__);

	return 0;
}
arch_initcall(arch_watchpoint_init);
