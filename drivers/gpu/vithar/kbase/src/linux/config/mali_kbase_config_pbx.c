/*
 *
 * (C) COPYRIGHT 2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#include <linux/ioport.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/src/linux/mali_kbase_config_linux.h>
#include <ump/ump_common.h>

static kbase_io_resources io_resources =
{
	.job_irq_number   = 70,
	.mmu_irq_number   = 70,
	.gpu_irq_number   = 70,
	.io_memory_region =
	{
		.start = 0xFC010000,
		.end   = 0xFC010000 + (4096 * 5) - 1
	}
};

static kbase_attribute config_attributes[] = {
	{
		KBASE_CONFIG_ATTR_MEMORY_PER_PROCESS_LIMIT,
		256 * 1024 * 1024UL /* 256MB */
	},
	{
		KBASE_CONFIG_ATTR_UMP_DEVICE,
		UMP_DEVICE_Z_SHIFT
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
		256 * 1024 * 1024UL /* 256MB */
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
		KBASE_MEM_PERF_SLOW
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
		130
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
		130
	},

	{
		KBASE_CONFIG_ATTR_JS_SCHEDULING_TICK_NS,
		577000000 /* 0.577s: vexpress settings, scaled by clock frequency (5000/130) */
	},

	{
		KBASE_CONFIG_ATTR_JS_SOFT_STOP_TICKS,
		1 /* between 0.577s and 1.154s before soft-stop a job */
	},

	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_SS,
		133 /* 77s before hard-stop */
	},

	{
		KBASE_CONFIG_ATTR_JS_HARD_STOP_TICKS_NSS,
		40000 /* 6.4hrs before NSS hard-stop (5000/130 times slower than vexpress) */
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_SS,
		200 /* 115s before resetting GPU */
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TICKS_NSS,
		40067 /* 6.4 hrs before resetting GPU */
	},

	{
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		3000 /* 3s before cancelling stuck jobs */
	},

	{
		KBASE_CONFIG_ATTR_JS_CTX_TIMESLICE_NS,
		38000000 /* 38ms: vexpress settings, scaled by clock frequency (5000/130) */
	},

	{
		KBASE_CONFIG_ATTR_END,
		0
	}
};

kbase_platform_config platform_config =
{
		.attributes                = config_attributes,
		.io_resources              = &io_resources,
		.midgard_type              = KBASE_MALI_T6XM
};


