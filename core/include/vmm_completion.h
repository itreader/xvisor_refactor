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
 * @file vmm_completion.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file of completion events for Orphan VCPU (or Thread).
 */

#ifndef __VMM_COMPLETION_H__
#define __VMM_COMPLETION_H__

#include <vmm_waitqueue.h>

/** Completion event structure */
struct vmm_completion {
    uint32_t         done;
    vmm_wait_queue_t wait_queue;
};

typedef struct vmm_completion vmm_completion_t;

/** Initialize completion event */
#define INIT_COMPLETION(cptr)                        \
    do {                                             \
        (cptr)->done = 0;                            \
        INIT_WAITQUEUE(&(cptr)->wait_queue, (cptr)); \
    } while (0)

/** Re-initialize completion event.
 *
 * This macro should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
#define REINIT_COMPLETION(cptr) \
    do {                        \
        (cptr)->done = 0;       \
    } while (0)

#define __COMPLETION_INITIALIZER(cmpl)                                     \
    {                                                                      \
        .done       = 0,                                                   \
        .wait_queue = __WAITQUEUE_INITIALIZER((cmpl).wait_queue, &(cmpl)), \
    }

#define DECLARE_COMPLETION(cmpl) vmm_completion_t cmpl = __COMPLETION_INITIALIZER(cmpl)

/** Check if completion is done */
bool vmm_completion_done(vmm_completion_t *cmpl);

/** Wait for completion */
int vmm_completion_wait(vmm_completion_t *cmpl);

/** Wait for completion for given timeout */
int vmm_completion_wait_timeout(vmm_completion_t *cmpl, uint64_t *timeout);

/** Signal completion and wake first sleeping Orphan VCPU */
int vmm_completion_complete(vmm_completion_t *cmpl);

/** Signal completion once and wake first sleeping Orphan VCPU
 *  Note: Signal completion once will only consider first complete
 *  call. If complete signal was already done then subsequent
 *  complete calls are ignored. This function can help avoid
 *  unwanted wake calls by clubing multiple complete calls into
 *  one signal completion.
 */
int vmm_completion_complete_once(vmm_completion_t *cmpl);

/** Signal completion and wake all sleeping Orphan VCPUs */
int vmm_completion_complete_all(vmm_completion_t *cmpl);

#endif /* __VMM_COMPLETION_H__ */
