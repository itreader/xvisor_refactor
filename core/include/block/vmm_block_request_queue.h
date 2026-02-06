/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_block_request_queue.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for generic block_device request queue
 */

#ifndef __VMM_BLOCKRQ_H__
#define __VMM_BLOCKRQ_H__

#include <block/vmm_block_device.h>
#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>
#include <vmm_workqueue.h>

struct vmm_block_request_queue;
typedef struct vmm_block_request_queue vmm_block_request_queue_t;

/** Representation of generic request queue operations */
typedef struct vmm_block_request_queue_t_ops {
    int (*read)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private);
    int (*read_cache)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private);
    int (*write)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private);
    int (*write_cache)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private);
    int (*abort)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private);
    void (*flush)(vmm_block_request_queue_t *brq, void *private);
} vmm_block_request_queue_ops_t;

/** Representation of generic request queue */
struct vmm_block_request_queue {
    char                                 name[VMM_FIELD_NAME_SIZE];
    uint32_t                             max_pending;
    bool                                 async_rw;
    const vmm_block_request_queue_ops_t *ops;
    void *private;

    uint32_t       wq_page_count;
    virtual_addr_t wq_page_va;
    vmm_spinlock_t wq_lock;
    double_list_t  wq_rw_free_list;
    double_list_t  wq_w_free_list;
    double_list_t  wq_pending_list;

    struct vmm_workqueue *wait_queue;

    vmm_request_queue_t rq;
};

#define vmm_rq_to_block_request_queue(__rq) container_of(__rq, vmm_block_request_queue_t, rq)

/** Get generic block_device request queue from request queue pointer */
static inline vmm_block_request_queue_t *vmm_block_request_queue_from_rq(vmm_request_queue_t *rq)
{
    return (vmm_block_request_queue_t *)rq->private;
}

/** Get request queue pointer from generic block_device request queue */
static inline vmm_request_queue_t *vmm_block_request_queue_to_rq(vmm_block_request_queue_t *brq)
{
    return &brq->rq;
}

/** Mark async request done */
void vmm_block_request_queue_async_done(vmm_block_request_queue_t *brq, vmm_request_t *r, int error);

/** Queue custom work on request queue */
int vmm_block_request_queue_queue_work(vmm_block_request_queue_t *brq, void (*w_func)(vmm_block_request_queue_t *, void *), void *w_private);

/** Destroy generic block_device request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
int vmm_block_request_queue_destroy(vmm_block_request_queue_t *brq);

/** Create generic block_device request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
vmm_block_request_queue_t *vmm_block_request_queue_create(
    const char *name, uint32_t max_pending, bool async_rw, const vmm_block_request_queue_ops_t *ops, void *private);

#endif
