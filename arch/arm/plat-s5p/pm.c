/* linux/arch/arm/plat-s5p/pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S5P Power Manager (Suspend-To-RAM) support
 *
 * Based on arch/arm/plat-s3c24xx/pm.c
 * Copyright (c) 2004,2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/suspend.h>
#include <linux/io.h>

#include <mach/regs-gpio.h>
#include <plat/pm.h>

#define PFX "s5p pm: "

/* s3c_pm_configure_extint
 *
 * configure all external interrupt pins
*/

#ifdef CONFIG_MX_SERIAL_TYPE
static inline int s3c_clear_pending(unsigned long mask)
{
	unsigned int tmp, i;

	/* to clear pending of unmasked eint */
	for (i=0; i<4; i++) {
		tmp = (mask>>(i*8)) & 0xff;
		__raw_writel(tmp, S5P_EINT_PEND(i));
		tmp = __raw_readl(S5P_EINT_PEND(i));
		pr_debug(PFX "S5P_EINT_PEND%d = 0x%02x\n", i, tmp);
	}
	return 0;
}
#else
static inline int s3c_clear_pending(unsigned long mask) {return 0;}
#endif

void s3c_pm_configure_extint(void)
{
	/* added by lvcha to fix show eint
	  * wakeup name error issue
	  */
	s3c_clear_pending(s3c_irqwake_eintmask);
}

void s3c_pm_restore_core(void)
{
	/* nothing here yet */
}

void s3c_pm_save_core(void)
{
	/* nothing here yet */
}

