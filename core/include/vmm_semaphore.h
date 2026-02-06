/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_semaphore.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of sempahore locks for Orphan VCPU (or Thread).
 */

#ifndef __VMM_SEMAPHORE_H__
#define __VMM_SEMAPHORE_H__

#include <vmm_types.h>
#include <vmm_waitqueue.h>

/** Semaphore lock structure */
typedef struct vmm_semaphore {
    uint32_t         limit;
    uint32_t         value;
    double_list_t    res_list;
    vmm_wait_queue_t wait_queue;
} vmm_semaphore_t;

/** Initialize semaphore lock */
#define INIT_SEMAPHORE(__sem, __lim, __val)            \
    do {                                               \
        (__sem)->limit = (__lim);                      \
        (__sem)->value = (__val);                      \
        INIT_LIST_HEAD(&((__sem)->res_list));          \
        INIT_WAITQUEUE(&(__sem)->wait_queue, (__sem)); \
    } while (0)

#define __SEMAPHORE_INITIALIZER(__sem, __lim, __val)                         \
    {                                                                        \
        .limit      = (__lim),                                               \
        .value      = (__val),                                               \
        .res_list   = {&(__sem).res_list, &(__sem).res_list},                \
        .wait_queue = __WAITQUEUE_INITIALIZER((__sem).wait_queue, &(__sem)), \
}

#define DEFINE_SEMAPHORE(__sem, __lim, __val) vmm_semaphore_t __sem = __SEMAPHORE_INITIALIZER(__sem, __lim, __val)

/** Ger semaphore available count */
uint32_t vmm_semaphore_avail(vmm_semaphore_t *sem);

/** Get maximum value (or limit) of semaphore */
uint32_t vmm_semaphore_limit(vmm_semaphore_t *sem);

/** Release (or increment) semaphore */
int vmm_semaphore_up(vmm_semaphore_t *sem);

/** Acquire (or decrement) semaphore */
int vmm_semaphore_down(vmm_semaphore_t *sem);

/** Acquire (or decrement) semaphore with timeout */
int vmm_semaphore_down_timeout(vmm_semaphore_t *sem, uint64_t *timeout);

#endif /* __VMM_SEMAPHORE_H__ */
