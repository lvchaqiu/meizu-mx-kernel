/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"
#include <plat/cpu.h>
#include "orion-m400/mali_orionm400.h"
#include "pegasus-m400/mali_pegasusm400.h"

int mali_gpu_clk 	=		160;
int mali_gpu_vol     =               1100000;        /* 1.10V for ASV */
int  gpu_power_state = 0;

_mali_osk_errcode_t mali_platform_init(void)
{
	if(soc_is_exynos4210())
	{
		return mali_orion_platform_init();
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){
		return mali_pegasus_platform_init();
	}
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	if(soc_is_exynos4210())
	{
		return mali_orion_platform_deinit();
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		return mali_pegasus_platform_deinit();
	}
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
	if(soc_is_exynos4210())
	{
		return mali_orion_platform_power_mode_change(power_mode);
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		return mali_pegasus_platform_power_mode_change(power_mode);
	}
	MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
	if(soc_is_exynos4210())
	{
		mali_orion_gpu_utilization_handler(utilization);
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_gpu_utilization_handler(utilization);
	}
}

void set_mali_parent_power_domain(struct platform_device* dev)
{
	if(soc_is_exynos4210())
	{
		set_mali_orion_parent_power_domain(dev);
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		set_mali_pegasus_parent_power_domain(dev);
	}
}

#if MALI_PMM_RUNTIME_JOB_CONTROL_ON
_mali_osk_errcode_t mali_platform_powerdown(u32 cores)
{
	if(soc_is_exynos4210())
	{
		mali_orion_platform_powerdown(cores);
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_platform_powerdown(cores);
	}
	MALI_SUCCESS;	
}
_mali_osk_errcode_t mali_platform_powerup(u32 cores)
{
	if(soc_is_exynos4210())
	{
		mali_orion_platform_powerup(cores);
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_platform_powerup(cores);
	}
	MALI_SUCCESS;	
}
#endif
#if USING_MALI_PMM
#if MALI_POWER_MGMT_TEST_SUITE
u32 pmu_get_power_up_down_info(void)
{
	if(soc_is_exynos4210())
	{
		pmu_orion_get_power_up_down_info();
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		pmu_pegasus_get_power_up_down_info();
	}
	return 0;
}
#endif
#endif
#ifdef CONFIG_REGULATOR
int mali_regulator_get_usecount(void)
{

	if(soc_is_exynos4210())
	{
		mali_orion_regulator_get_usecount();
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_regulator_get_usecount();
	}
	return 0;
}
void mali_regulator_disable(void)
{

	if(soc_is_exynos4210())
	{
		mali_orion_regulator_disable();
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_regulator_disable();
	}

}
void mali_regulator_enable(void)
{
	if(soc_is_exynos4210())
	{
		mali_orion_regulator_enable();
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_regulator_enable();
	}
}
void mali_regulator_set_voltage(int min_uV, int max_uV)
{
	if(soc_is_exynos4210())
	{
		mali_orion_regulator_set_voltage(min_uV, max_uV);
	}else if(soc_is_exynos4212() || soc_is_exynos4412()){

		mali_pegasus_regulator_set_voltage(min_uV, max_uV);
	}
}
#endif
