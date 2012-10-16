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
 * @file
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#ifndef _OSK_ARCH_TIMERS_H
#define _OSK_ARCH_TIMERS_H

#ifndef _OSK_H_
#error "Include mali_osk.h directly"
#endif

OSK_STATIC_INLINE osk_error osk_timer_init(osk_timer * const tim)
{
	OSK_ASSERT(NULL != tim);
	init_timer(&tim->timer);
	OSK_DEBUG_CODE(	tim->active = MALI_FALSE );
	OSK_ASSERT(0 ==	object_is_on_stack(tim));
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_on_stack_init(osk_timer * const tim)
{
	OSK_ASSERT(NULL != tim);
	init_timer_on_stack(&tim->timer);
	OSK_DEBUG_CODE(	tim->active = MALI_FALSE );
	OSK_ASSERT(0 !=	object_is_on_stack(tim));
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_start(osk_timer *tim, u32 delay)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != tim->timer.function);
	OSK_ASSERT(0 != delay);
	tim->timer.expires = jiffies + ((delay * HZ + 999) / 1000);
	add_timer(&tim->timer);
	OSK_DEBUG_CODE(	tim->active = MALI_TRUE );
	return OSK_ERR_NONE;
}

OSK_STATIC_INLINE osk_error osk_timer_modify(osk_timer *tim, u32 delay)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != tim->timer.function);
	OSK_ASSERT(0 != delay);
	mod_timer(&tim->timer, jiffies + ((delay * HZ + 999) / 1000));
	OSK_DEBUG_CODE(	tim->active = MALI_TRUE );
	return OSK_ERR_NONE;
}


OSK_STATIC_INLINE void osk_timer_stop(osk_timer *tim)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != tim->timer.function);
	del_timer_sync(&tim->timer);
	OSK_DEBUG_CODE( tim->active = MALI_FALSE );
}

OSK_STATIC_INLINE void osk_timer_callback_set(osk_timer *tim, osk_timer_callback callback, void *data)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(NULL != callback);
	OSK_DEBUG_CODE(
		if (MALI_FALSE == tim->active)
		{
		}
	);
	/* osk_timer_callback uses void * for the callback parameter instead of unsigned long in Linux */
	tim->timer.function = (void (*)(unsigned long))callback;
	tim->timer.data = (unsigned long)data;
}

OSK_STATIC_INLINE void osk_timer_term(osk_timer *tim)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(0 ==	object_is_on_stack(tim));
	OSK_DEBUG_CODE(
		if (MALI_FALSE == tim->active)
		{
		}
	);
	/* Nothing to do */
}

OSK_STATIC_INLINE void osk_timer_on_stack_term(osk_timer *tim)
{
	OSK_ASSERT(NULL != tim);
	OSK_ASSERT(0 !=	object_is_on_stack(tim));
	OSK_DEBUG_CODE(
		if (MALI_FALSE == tim->active)
		{
		}
	);
	destroy_timer_on_stack(&tim->timer);
}

#endif /* _OSK_ARCH_TIMERS_H_ */
