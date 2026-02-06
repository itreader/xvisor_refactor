/**
 * Copyright (c) 2011 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_waitqueue.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for Orphan VCPU (or Thread) wait queue.
 */

#ifndef __VMM_WAITQUEUE_H__
#define __VMM_WAITQUEUE_H__

#include <libs/list.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

typedef struct vmm_wait_queue {
    vmm_spinlock_t lock;
    double_list_t  vcpu_list;
    uint32_t       vcpu_count;
    void *private;
} vmm_wait_queue_t;

#define INIT_WAITQUEUE(__wq, __p)             \
    do {                                      \
        INIT_SPIN_LOCK(&((__wq)->lock));      \
        INIT_LIST_HEAD(&((__wq)->vcpu_list)); \
        (__wq)->vcpu_count = 0;               \
        (__wq)->private    = (__p);           \
    } while (0);

#define __WAITQUEUE_INITIALIZER(__wq, __p)                    \
    {                                                         \
        .lock       = __SPINLOCK_INITIALIZER((__wq).lock),    \
        .vcpu_list  = {&(__wq).vcpu_list, &(__wq).vcpu_list}, \
        .vcpu_count = 0,                                      \
        .private    = (__p),                                  \
}

#define DECLARE_WAITQUEUE(__n, __p) vmm_wait_queue_t __n = __WAITQUEUE_INITIALIZER(__n, __p)

/** Lowlevel waitqueue sleep.
 *  Note: This function should only be called with wait_queue->lock held using
 *  any vmm_spin_lock_xxx() API except lite APIs
 *  Note: This function can only be called from Orphan Context
 */
int __vmm_waitqueue_sleep(vmm_wait_queue_t *wait_queue, uint64_t *timeout_nsecs);

/** Lowlevel waitqueue wakeup first VCPU.
 *  Note: This function should only be called with wait_queue->lock held using
 *  any vmm_spin_lock_xxx() API except lite APIs
 *  Note: This function can be called from any context
 */
int __vmm_waitqueue_wakefirst(vmm_wait_queue_t *wait_queue);

/** Lowlevel waitqueue wakeup all VCPUs.
 *  Note: This function should only be called with wait_queue->lock held using
 *  any vmm_spin_lock_xxx() API except lite APIs
 *  Note: This function can be called from any context
 */
int __vmm_waitqueue_wakeall(vmm_wait_queue_t *wait_queue);

/** Waiting VCPU count */
uint32_t vmm_waitqueue_count(vmm_wait_queue_t *wait_queue);

/** Put current VCPU to sleep on given waitqueue */
int vmm_waitqueue_sleep(vmm_wait_queue_t *wait_queue);

/** Put current VCPU to sleep on given waitqueue at most for timeout usecs */
int vmm_waitqueue_sleep_timeout(vmm_wait_queue_t *wait_queue, uint64_t *timeout_usecs);

/** Wakeup VCPU from its waitqueue */
int vmm_waitqueue_wake(vmm_vcpu_t *vcpu);

/** Force VCPU out of waitqueue */
int vmm_waitqueue_forced_remove(vmm_vcpu_t *vcpu);

/** Wakeup first VCPU in a given waitqueue */
int vmm_waitqueue_wakefirst(vmm_wait_queue_t *wait_queue);

/** Wakeup all VCPUs in a given waitqueue */
int vmm_waitqueue_wakeall(vmm_wait_queue_t *wait_queue);

/**
 * Sleep until a condition gets true
 * @condition: a C expression for the event to wait for
 */
#define vmm_waitqueue_sleep_event(wait_queue, condition)     \
    do {                                                     \
        BUG_ON(!vmm_scheduler_orphan_context());             \
        for (;;) {                                           \
            if (condition)                                   \
                break;                                       \
            vmm_waitqueue_sleep_timeout((wait_queue), NULL); \
        }                                                    \
    } while (0)

/**
 * Sleep until a condition gets true or a timeout elapses
 * @wait_queue: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 * @timeout: timeout in nano-seconds
 */
#define vmm_waitqueue_sleep_event_timeout(wait_queue, condition, timeout) \
    do {                                                                  \
        uint64_t _tout = *(timeout);                                      \
        for (;;) {                                                        \
            if (condition)                                                \
                break;                                                    \
            vmm_waitqueue_sleep_timeout((wait_queue), &_tout);            \
            *(timeout) = _tout;                                           \
            if (!_tout)                                                   \
                break;                                                    \
        }                                                                 \
        *(timeout) = _tout;                                               \
    } while (0)

#endif /* __VMM_WAITQUEUE_H__ */
