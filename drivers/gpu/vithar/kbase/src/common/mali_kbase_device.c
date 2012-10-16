/*
 *
 * (C) COPYRIGHT 2010-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



/**
 * @file mali_kbase_device.c
 * Base kernel device APIs
 */

#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>

#define GPU_NUM_ADDRESS_SPACES 4
#define GPU_NUM_JOB_SLOTS 3

/* This array is referenced at compile time, it cannot be made static... */
const kbase_device_info kbase_dev_info[] = {
	{
		KBASE_MALI_T6XM,
		(KBASE_FEATURE_HAS_MODEL_PMU),
	},
	{
		KBASE_MALI_T6F1,
		(KBASE_FEATURE_NEEDS_REG_DELAY |
		 KBASE_FEATURE_DELAYED_PERF_WRITE_STATUS |
		 KBASE_FEATURE_HAS_16BIT_PC),
	},
	{
		KBASE_MALI_T601,
	},
	{
		KBASE_MALI_T604,
	},
	{
		KBASE_MALI_T608,
	},
};

void kbasep_as_do_poke(osk_workq_work * work);
void kbasep_reset_timer_callback(void *data);
void kbasep_reset_timeout_worker(osk_workq_work *data);

kbase_device *kbase_device_create(const kbase_device_info *dev_info)
{
	kbase_device *kbdev;
	osk_error osk_err;
	int i; /* i used after the for loop, don't reuse ! */

	kbdev = osk_calloc(sizeof(*kbdev));
	if (!kbdev)
	{
		goto fail;
	}

	kbdev->dev_info = dev_info;

	/* NOTE: Add Property Query here */
	kbdev->nr_address_spaces = GPU_NUM_ADDRESS_SPACES;
	kbdev->nr_job_slots = GPU_NUM_JOB_SLOTS;
	kbdev->job_slot_features[0] =
		  KBASE_JSn_FEATURE_NULL_JOB
		| KBASE_JSn_FEATURE_SET_VALUE_JOB
		| KBASE_JSn_FEATURE_CACHE_FLUSH_JOB
		| KBASE_JSn_FEATURE_FRAGMENT_JOB;
	kbdev->job_slot_features[1] =
		  KBASE_JSn_FEATURE_NULL_JOB
		| KBASE_JSn_FEATURE_SET_VALUE_JOB
		| KBASE_JSn_FEATURE_CACHE_FLUSH_JOB
		| KBASE_JSn_FEATURE_COMPUTE_JOB
		| KBASE_JSn_FEATURE_VERTEX_JOB
		| KBASE_JSn_FEATURE_GEOMETRY_JOB
		| KBASE_JSn_FEATURE_TILER_JOB
		| KBASE_JSn_FEATURE_FUSED_JOB;
	kbdev->job_slot_features[2] =
		  KBASE_JSn_FEATURE_NULL_JOB
		| KBASE_JSn_FEATURE_SET_VALUE_JOB
		| KBASE_JSn_FEATURE_CACHE_FLUSH_JOB
		| KBASE_JSn_FEATURE_COMPUTE_JOB
		| KBASE_JSn_FEATURE_VERTEX_JOB
		| KBASE_JSn_FEATURE_GEOMETRY_JOB;

	osk_err = osk_spinlock_irq_init(&kbdev->mmu_mask_change, OSK_LOCK_ORDER_MMU_MASK);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_dev;
	}

	for (i = 0; i < kbdev->nr_address_spaces; i++)
	{
		const char format[] = "mali_mmu%d";
		char name[sizeof(format)];
#if BASE_HW_ISSUE_8316
		const char poke_format[] = "mali_mmu%d_poker";
		char poke_name[sizeof(poke_format)];
		if (0 > cutils_cstr_snprintf(poke_name, sizeof(poke_name), poke_format, i))
		{
			goto free_workqs;
		}
#endif /* BASE_HW_ISSUE_8316 */

		if (0 > cutils_cstr_snprintf(name, sizeof(name), format, i))
		{
			goto free_workqs;
		}

		kbdev->as[i].number = i;
		kbdev->as[i].fault_addr = 0ULL;
		osk_err = osk_workq_init(&kbdev->as[i].pf_wq, name, 0);
		if (OSK_ERR_NONE != osk_err)
		{
			goto free_workqs;
		}
		osk_err = osk_mutex_init(&kbdev->as[i].transaction_mutex, OSK_LOCK_ORDER_AS);
		if (OSK_ERR_NONE != osk_err)
		{
			osk_workq_term(&kbdev->as[i].pf_wq);
			goto free_workqs;
		}
#if BASE_HW_ISSUE_8316
		osk_err = osk_workq_init(&kbdev->as[i].poke_wq, poke_name, 0);
		if (OSK_ERR_NONE != osk_err)
		{
			osk_workq_term(&kbdev->as[i].pf_wq);
			osk_mutex_term(&kbdev->as[i].transaction_mutex);
			goto free_workqs;
		}
		osk_workq_work_init(&kbdev->as[i].poke_work, kbasep_as_do_poke);
		osk_err = osk_timer_init(&kbdev->as[i].poke_timer);
		if (OSK_ERR_NONE != osk_err)
		{
			osk_workq_term(&kbdev->as[i].poke_wq);
			osk_workq_term(&kbdev->as[i].pf_wq);
			osk_mutex_term(&kbdev->as[i].transaction_mutex);
			goto free_workqs;
		}
		osk_timer_callback_set(&kbdev->as[i].poke_timer, kbasep_as_poke_timer_callback , &kbdev->as[i]);
		osk_atomic_set(&kbdev->as[i].poke_refcount, 0);
#endif /* BASE_HW_ISSUE_8316 */
	}
	/* don't change i after this point */

	osk_err = osk_spinlock_init(&kbdev->hwcnt_lock, OSK_LOCK_ORDER_HWCNT);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_workqs;
	}

	kbdev->hwcnt_in_progress = MALI_FALSE;
	kbdev->hwcnt_is_setup = MALI_FALSE;
	kbdev->hwcnt_state = KBASE_INSTR_STATE_DISABLED;
	osk_err = osk_waitq_init(&kbdev->hwcnt_waitqueue);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_hwcnt_lock;
	}

	if (OSK_ERR_NONE != osk_workq_init(&kbdev->reset_workq, "Mali reset workqueue", 0))
	{
		goto free_hwcnt_waitq;
	}

	osk_workq_work_init(&kbdev->reset_work, kbasep_reset_timeout_worker);

	osk_err = osk_waitq_init(&kbdev->reset_waitq);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_reset_workq;
	}

	osk_err = osk_timer_init(&kbdev->reset_timer);
	if (OSK_ERR_NONE != osk_err)
	{
		goto free_reset_waitq;
	}
	osk_timer_callback_set(&kbdev->reset_timer, kbasep_reset_timer_callback, kbdev);

	return kbdev;

free_reset_waitq:
	osk_waitq_term(&kbdev->reset_waitq);
free_reset_workq:
	osk_workq_term(&kbdev->reset_workq);
free_hwcnt_waitq:
	osk_waitq_term(&kbdev->hwcnt_waitqueue);
free_hwcnt_lock:
	osk_spinlock_term(&kbdev->hwcnt_lock);
free_workqs:
	while (i > 0)
	{
		i--;
		osk_mutex_term(&kbdev->as[i].transaction_mutex);
		osk_workq_term(&kbdev->as[i].pf_wq);
#if BASE_HW_ISSUE_8316
		osk_workq_term(&kbdev->as[i].poke_wq);
		osk_timer_term(&kbdev->as[i].poke_timer);
#endif /* BASE_HW_ISSUE_8316 */
	}
	osk_spinlock_irq_term(&kbdev->mmu_mask_change);
free_dev:
	osk_free(kbdev);
fail:
	return NULL;
}

void kbase_device_destroy(kbase_device *kbdev)
{
	int i;

	osk_timer_term(&kbdev->reset_timer);
	osk_waitq_term(&kbdev->reset_waitq);
	osk_workq_term(&kbdev->reset_workq);

	for (i = 0; i < kbdev->nr_address_spaces; i++)
	{
		osk_mutex_term(&kbdev->as[i].transaction_mutex);
		osk_workq_term(&kbdev->as[i].pf_wq);
#if BASE_HW_ISSUE_8316
		osk_workq_term(&kbdev->as[i].poke_wq);
		osk_timer_term(&kbdev->as[i].poke_timer);
#endif /* BASE_HW_ISSUE_8316 */
	}

	osk_spinlock_term(&kbdev->hwcnt_lock);
	osk_waitq_term(&kbdev->hwcnt_waitqueue);

	osk_free(kbdev);
}

int kbase_device_has_feature(kbase_device *kbdev, u32 feature)
{
	return !!(kbdev->dev_info->features & feature);
}

kbase_midgard_type kbase_device_get_type(kbase_device *kbdev)
{
	return kbdev->dev_info->dev_type;
}
KBASE_EXPORT_TEST_API(kbase_device_get_type)

#if KBASE_REGISTER_TRACE_ENABLED

void kbase_device_trace_buffer_install(kbase_context * kctx, u32 * tb, size_t size)
{
	OSK_ASSERT(kctx);
	OSK_ASSERT(tb);

	/* set up the header */
	/* magic number in the first 4 bytes */
	tb[0] = 0x45435254;
	/* Store (write offset = 0, wrap counter = 0, transaction active = no)
	 * write offset 0 means never written.
	 * Offsets 1 to (wrap_offset - 1) used to store values when trace started
	 */
	tb[1] = 0;

	/* install trace buffer */
	osk_spinlock_irq_lock(&kctx->jctx.tb_lock);
	kctx->jctx.tb_wrap_offset = size / 8;
	kctx->jctx.tb = tb;
	osk_spinlock_irq_unlock(&kctx->jctx.tb_lock);
}

void kbase_device_trace_buffer_uninstall(kbase_context * kctx)
{
	OSK_ASSERT(kctx);
	osk_spinlock_irq_lock(&kctx->jctx.tb_lock);
	kctx->jctx.tb = NULL;
	kctx->jctx.tb_wrap_offset = 0;
	osk_spinlock_irq_unlock(&kctx->jctx.tb_lock);
}

void kbase_device_trace_register_access(kbase_context * kctx, kbase_reg_access_type type, u16 reg_offset, u32 reg_value)
{
	osk_spinlock_irq_lock(&kctx->jctx.tb_lock);
	if (kctx->jctx.tb)
	{
		u16 wrap_count;
		u16 write_offset;
		osk_atomic dummy; /* osk_atomic_set called to use memory barriers until OSK get's them */
		u32 * tb = kctx->jctx.tb;
		u32 header_word;

		header_word = tb[1];
		OSK_ASSERT(0 == (header_word & 0x1));

		wrap_count = (header_word >> 1) & 0x7FFF;
		write_offset = (header_word >> 16) & 0xFFFF;

		/* mark as transaction in progress */
		tb[1] |= 0x1;
		osk_atomic_set(&dummy, 1);

		/* calculate new offset */
		write_offset++;
		if (write_offset == kctx->jctx.tb_wrap_offset)
		{
			/* wrap */
			write_offset = 1;
			wrap_count++;
			wrap_count &= 0x7FFF; /* 15bit wrap counter */
		}

		/* store the trace entry at the selected offset */
		tb[write_offset * 2 + 0] = (reg_offset & ~0x3) | ((type == REG_WRITE) ? 0x1 : 0x0);
		tb[write_offset * 2 + 1] = reg_value;

		osk_atomic_set(&dummy, 1);

		/* new header word */
		header_word = (write_offset << 16) | (wrap_count << 1) | 0x0; /* transaction complete */
		tb[1] = header_word;
	}
	osk_spinlock_irq_unlock(&kctx->jctx.tb_lock);
}
#endif /* KBASE_REGISTER_TRACE_ENABLED */

void kbase_reg_write(kbase_device *kbdev, u16 offset, u32 value, kbase_context * kctx)
{
	OSK_PRINT_INFO(OSK_BASE_CORE, "w: reg %04x val %08x", offset, value);
	kbase_os_reg_write(kbdev, offset, value);
#if KBASE_REGISTER_TRACE_ENABLED
	if (kctx) kbase_device_trace_register_access(kctx, REG_WRITE, offset, value);
#endif /* KBASE_REGISTER_TRACE_ENABLED */
}
KBASE_EXPORT_TEST_API(kbase_reg_write)

u32 kbase_reg_read(kbase_device *kbdev, u16 offset, kbase_context * kctx)
{
	u32 val;
	val = kbase_os_reg_read(kbdev, offset);
	OSK_PRINT_INFO(OSK_BASE_CORE, "r: reg %04x val %08x", offset, val);
#if KBASE_REGISTER_TRACE_ENABLED
	if (kctx) kbase_device_trace_register_access(kctx, REG_READ, offset, val);
#endif /* KBASE_REGISTER_TRACE_ENABLED */
	return val;
}
KBASE_EXPORT_TEST_API(kbase_reg_read)

void kbase_report_gpu_fault(kbase_device *kbdev, int multiple)
{
	u32 status;
	u64 address;

	status = kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_FAULTSTATUS), NULL);
	address = (u64)kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_FAULTADDRESS_HI), NULL) << 32;
	address |= kbase_reg_read(kbdev, GPU_CONTROL_REG(GPU_FAULTADDRESS_LO), NULL);

	OSK_PRINT_WARN(OSK_BASE_CORE, "GPU Fault 0x08%x (%s) at 0x%016llx", status, kbase_exception_name(status), address);
	if (multiple)
	{
		OSK_PRINT_WARN(OSK_BASE_CORE, "There were multiple GPU faults - some have not been reported\n");
	}
}

void kbase_gpu_interrupt(kbase_device * kbdev, u32 val)
{
	if (val & GPU_FAULT)
	{
		kbase_report_gpu_fault(kbdev, val & MULTIPLE_GPU_FAULTS);
	}

	if (val & RESET_COMPLETED)
	{
		kbase_pm_reset_done(kbdev);
	}

	if (val & PRFCNT_SAMPLE_COMPLETED)
	{
		kbase_instr_hwcnt_sample_done(kbdev);
	}

	if (val & CLEAN_CACHES_COMPLETED)
	{
		kbase_clean_caches_done(kbdev);
	}

	kbase_reg_write(kbdev, GPU_CONTROL_REG(GPU_IRQ_CLEAR), val, NULL);

	/* kbase_pm_check_transitions must be called after the IRQ has been cleared. This is because it might trigger
	 * further power transitions and we don't want to miss the interrupt raised to notify us that these further
	 * transitions have finished.
	 */
	if (val & (POWER_CHANGED_ALL | POWER_CHANGED_SINGLE))
	{
		kbase_pm_check_transitions(kbdev);
	}
}

