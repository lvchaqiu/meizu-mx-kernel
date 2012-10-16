/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.h
 * Platform specific Mali driver functions
 */

#ifndef __MALI_ORION_M400_H__
#define __MALI_ORION_M400_H__

#include "mali_osk.h"
 
#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_CPU_EXYNOS4210

/** @brief Platform specific setup and initialisation of MALI
 *
 * This is called from the entrypoint of the driver to initialize the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_orion_platform_init(void);

/** @brief Platform specific deinitialisation of MALI
 *
 * This is called on the exit of the driver to terminate the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_orion_platform_deinit(void);

/** @brief Platform specific powerdown sequence of MALI
 *
 * Call as part of platform init if there is no PMM support, else the
 * PMM will call it.
 * There are three power modes defined:
 *  1) MALI_POWER_MODE_ON
 *  2) MALI_POWER_MODE_LIGHT_SLEEP
 *  3) MALI_POWER_MODE_DEEP_SLEEP
 * MALI power management module transitions to MALI_POWER_MODE_LIGHT_SLEEP mode when MALI is idle
 * for idle timer (software timer defined in mali_pmm_policy_jobcontrol.h) duration, MALI transitions
 * to MALI_POWER_MODE_LIGHT_SLEEP mode during timeout if there are no more jobs queued.
 * MALI power management module transitions to MALI_POWER_MODE_DEEP_SLEEP mode when OS does system power
 * off.
 * Customer has to add power down code when MALI transitions to MALI_POWER_MODE_LIGHT_SLEEP or MALI_POWER_MODE_DEEP_SLEEP
 * mode.
 * MALI_POWER_MODE_ON mode is entered when the MALI is to powered up. Some customers want to control voltage regulators during
 * the whole system powers on/off. Customer can track in this function whether the MALI is powered up from
 * MALI_POWER_MODE_LIGHT_SLEEP or MALI_POWER_MODE_DEEP_SLEEP mode and manage the voltage regulators as well.
 * @param power_mode defines the power modes
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_orion_platform_power_mode_change(mali_power_mode power_mode);


/** @brief Platform specific handling of GPU utilization data
 *
 * When GPU utilization data is enabled, this function will be
 * periodically called.
 *
 * @param utilization The workload utilization of the Mali GPU. 0 = no utilization, 256 = full utilization.
 */
void mali_orion_gpu_utilization_handler(u32 utilization);

/** @brief Setting the power domain of MALI
 *
 * This function sets the power domain of MALI if Linux run time power management is enabled
 *
 * @param dev Reference to struct platform_device (defined in linux) used by MALI GPU
 */
void set_mali_orion_parent_power_domain(struct platform_device* dev);

#ifdef CONFIG_REGULATOR
int mali_orion_regulator_get_usecount(void);
void mali_orion_regulator_disable(void);
void mali_orion_regulator_enable(void);
void mali_orion_regulator_set_voltage(int min_uV, int max_uV);
#endif
mali_bool mali_orion_clk_set_rate(unsigned int clk, unsigned int mhz);
unsigned long mali_orion_clk_get_rate(void);
void mali_orion_clk_put(mali_bool binc_mali_clk);

#if MALI_PMM_RUNTIME_JOB_CONTROL_ON
_mali_osk_errcode_t mali_orion_platform_powerdown(u32 cores);
_mali_osk_errcode_t mali_orion_platform_powerup(u32 cores);
#endif

_mali_osk_errcode_t orion_g3d_power_domain_control(int bpower_on);

#if USING_MALI_PMM
#if MALI_POWER_MGMT_TEST_SUITE
/** @brief function to get status of individual cores
 *
 * This function is used by power management test suite to get the status of powered up/down the number
 * of cores
 * @param utilization The workload utilization of the Mali GPU. 0 = no utilization, 256 = full utilization.
 */
u32 pmu_orion_get_power_up_down_info(void);
#endif
#endif

#if MALI_DVFS_ENABLED
mali_bool init_mali_orion_dvfs_status(int step);
void deinit_mali_orion_dvfs_status(void);
mali_bool mali_orion_dvfs_handler(u32 utilization);
int mali_orion_dvfs_is_running(void);
void mali_orion_dvfs_late_resume(void);
int get_mali_orion_dvfs_control_status(void);
mali_bool set_mali_orion_dvfs_current_step(unsigned int step);
void mali_orion_default_step_set(int step, mali_bool boostup);
#endif

#else //CONFIG_CPU_EXYNOS4210

/** @brief Platform specific setup and initialisation of MALI
 *
 * This is called from the entrypoint of the driver to initialize the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_orion_platform_init(void){MALI_SUCCESS;}

/** @brief Platform specific deinitialisation of MALI
 *
 * This is called on the exit of the driver to terminate the platform
 *
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_orion_platform_deinit(void){MALI_SUCCESS;}

/** @brief Platform specific powerdown sequence of MALI
 *
 * Call as part of platform init if there is no PMM support, else the
 * PMM will call it.
 * There are three power modes defined:
 *  1) MALI_POWER_MODE_ON
 *  2) MALI_POWER_MODE_LIGHT_SLEEP
 *  3) MALI_POWER_MODE_DEEP_SLEEP
 * MALI power management module transitions to MALI_POWER_MODE_LIGHT_SLEEP mode when MALI is idle
 * for idle timer (software timer defined in mali_pmm_policy_jobcontrol.h) duration, MALI transitions
 * to MALI_POWER_MODE_LIGHT_SLEEP mode during timeout if there are no more jobs queued.
 * MALI power management module transitions to MALI_POWER_MODE_DEEP_SLEEP mode when OS does system power
 * off.
 * Customer has to add power down code when MALI transitions to MALI_POWER_MODE_LIGHT_SLEEP or MALI_POWER_MODE_DEEP_SLEEP
 * mode.
 * MALI_POWER_MODE_ON mode is entered when the MALI is to powered up. Some customers want to control voltage regulators during
 * the whole system powers on/off. Customer can track in this function whether the MALI is powered up from
 * MALI_POWER_MODE_LIGHT_SLEEP or MALI_POWER_MODE_DEEP_SLEEP mode and manage the voltage regulators as well.
 * @param power_mode defines the power modes
 * @return _MALI_OSK_ERR_OK on success otherwise, a suitable _mali_osk_errcode_t error.
 */
_mali_osk_errcode_t mali_orion_platform_power_mode_change(mali_power_mode power_mode){MALI_SUCCESS;}


/** @brief Platform specific handling of GPU utilization data
 *
 * When GPU utilization data is enabled, this function will be
 * periodically called.
 *
 * @param utilization The workload utilization of the Mali GPU. 0 = no utilization, 256 = full utilization.
 */
void mali_orion_gpu_utilization_handler(u32 utilization){}

/** @brief Setting the power domain of MALI
 *
 * This function sets the power domain of MALI if Linux run time power management is enabled
 *
 * @param dev Reference to struct platform_device (defined in linux) used by MALI GPU
 */
void set_mali_orion_parent_power_domain(struct platform_device* dev){}

#ifdef CONFIG_REGULATOR
int mali_orion_regulator_get_usecount(void){return 0;}
void mali_orion_regulator_disable(void){}
void mali_orion_regulator_enable(void){}
void mali_orion_regulator_set_voltage(int min_uV, int max_uV){}
#endif
mali_bool mali_orion_clk_set_rate(unsigned int clk, unsigned int mhz){return MALI_TRUE;}
unsigned long mali_orion_clk_get_rate(void){return 0;}
void mali_orion_clk_put(mali_bool binc_mali_clk){}

#if MALI_PMM_RUNTIME_JOB_CONTROL_ON
_mali_osk_errcode_t mali_orion_platform_powerdown(u32 cores){MALI_SUCCESS;}
_mali_osk_errcode_t mali_orion_platform_powerup(u32 cores){MALI_SUCCESS;}
#endif

_mali_osk_errcode_t orion_g3d_power_domain_control(int bpower_on){MALI_SUCCESS;}

#if USING_MALI_PMM
#if MALI_POWER_MGMT_TEST_SUITE
/** @brief function to get status of individual cores
 *
 * This function is used by power management test suite to get the status of powered up/down the number
 * of cores
 * @param utilization The workload utilization of the Mali GPU. 0 = no utilization, 256 = full utilization.
 */
u32 pmu_orion_get_power_up_down_info(void){return 0;}
#endif
#endif

#if MALI_DVFS_ENABLED
mali_bool init_mali_orion_dvfs_status(int step){return MALI_TRUE;}
void deinit_mali_orion_dvfs_status(void){}
mali_bool mali_orion_dvfs_handler(u32 utilization){return MALI_TRUE;}
int mali_orion_dvfs_is_running(void){return 0;}
void mali_orion_dvfs_late_resume(void){}
int get_mali_orion_dvfs_control_status(void){return 0;}
mali_bool set_mali_orion_dvfs_current_step(unsigned int step){return MALI_TRUE;}
void mali_orion_default_step_set(int step, mali_bool boostup){}
#endif

#endif//CONFIG_CPU_EXYNOS4210

#ifdef __cplusplus
}
#endif
#endif
