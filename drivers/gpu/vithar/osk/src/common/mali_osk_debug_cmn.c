/*
 *
 * (C) COPYRIGHT 2008-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */




#include <osk/mali_osk.h>
#include "mali_osk_compile_asserts.h"

/**
 * @brief Contains the module names (modules in the same order as for the osk_module enumeration)
 * @sa oskp_module_to_str
 */
static const char* CONST oskp_str_modules[] =
{
	"UNKNOWN",     /**< Unknown module */
	"OSK",         /**< OSK */
	"UKK",         /**< UKK */
	"BASE_MMU",    /**< Base MMU */
	"BASE_JD",     /**< Base Job Dispatch */
	"BASE_JM",     /**< Base Job Manager */
	"BASE_CORE",   /**< Base Core */
	"BASE_MEM",    /**< Base Memory */
	"BASE_EVENT",  /**< Base Event */
	"BASE_CTX",    /**< Base Context */
	"BASE_PM",     /**< Base Power Management */
	"UMP",         /**< UMP */
};

#define MODULE_STRING_ARRAY_SIZE (sizeof(oskp_str_modules)/sizeof(oskp_str_modules[0]))

INLINE void oskp_cmn_compile_time_assertions(void)
{
	/*
	 * If this assert triggers you have forgotten to update oskp_str_modules
	 * when you added a module to the osk_module enum
	 * */
	CSTD_COMPILE_TIME_ASSERT(OSK_MODULES_ALL == MODULE_STRING_ARRAY_SIZE );
}

const char* oskp_module_to_str(const osk_module module)
{
	if( MODULE_STRING_ARRAY_SIZE <= module)
	{
		return "";
	}
	return oskp_str_modules[module];
}
