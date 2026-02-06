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
 * @file vmm_block_device.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework header
 */

#ifndef __VMM_BLOCK_DEVICE_H_
#define __VMM_BLOCK_DEVICE_H_

#include <vmm_device_driver.h>
#include <vmm_limits.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_BLOCK_DEVICE_CLASS_NAME      "block"
#define VMM_BLOCK_DEVICE_CLASS_IPRIORITY 1

struct vmm_request;
struct vmm_request_queue;
struct vmm_block_device;
typedef struct vmm_request       vmm_request_t;
typedef struct vmm_request_queue vmm_request_queue_t;
typedef struct vmm_block_device  vmm_block_device_t;

/** Types of block IO request */
enum vmm_request_type {
    VMM_REQUEST_UNKNOWN = 0,
    VMM_REQUEST_READ    = 1,
    VMM_REQUEST_WRITE   = 2
};

/** Representation of a block IO request */
struct vmm_request {
    double_list_t head;

    vmm_block_device_t *block_device; /* No need to set this field.
                                       * submit_request() will set this field.
                                       */

    enum vmm_request_type type;
    uint64_t              lba;
    uint32_t              bcnt;
    void                 *data;

    void (*completed)(vmm_request_t *);
    void (*failed)(vmm_request_t *);
    void *private;
};

/** Representation of a block IO request queue */
struct vmm_request_queue {
    /* Lock to protect the request queue operations */
    vmm_spinlock_t lock;

    /* Max pending requests */
    uint32_t max_pending;

    /* Pending (or in-flight) request count */
    uint32_t pending_count;

    /* Backlog request count */
    uint32_t backlog_count;

    /* Backlog request list */
    double_list_t backlog_list;

    /* Note: if peek_cache succeeds then we assume
     * request completed successfully.
     *
     * Note: if peek_cache returns VMM_ENOTAVAIL then
     * we try make_request()
     *
     * Note: if peek_cache returns any other error then
     * we assume request failed.
     */
    int (*peek_cache)(vmm_request_queue_t *rq, vmm_request_t *r);

    /* Note: make_request must ensure that it calls
     *
     * vmm_block_device_complete_request()
     * OR
     * vmm_block_device_fail_request()
     *
     * for every request that it gets
     */
    int (*make_request)(vmm_request_queue_t *rq, vmm_request_t *r);

    /* Note: abort_request will be called for successfully
     * submited request only
     */
    int (*abort_request)(vmm_request_queue_t *rq, vmm_request_t *r);

    /* Note: This is an optional callback only required
     * if request queue does block caching
     */
    int (*flush_cache)(vmm_request_queue_t *rq);

    void *private;
};

#define INIT_REQUEST_QUEUE(__rq, __max_pending, __peek_cache, __make_request, __abort_request, __flush_request, __private) \
    do {                                                                                                                   \
        INIT_SPIN_LOCK(&(__rq)->lock);                                                                                     \
        (__rq)->max_pending   = (__max_pending);                                                                           \
        (__rq)->pending_count = 0;                                                                                         \
        (__rq)->backlog_count = 0;                                                                                         \
        INIT_LIST_HEAD(&(__rq)->backlog_list);                                                                             \
        (__rq)->peek_cache    = (__peek_cache);                                                                            \
        (__rq)->make_request  = (__make_request);                                                                          \
        (__rq)->abort_request = (__abort_request);                                                                         \
        (__rq)->flush_cache   = (__flush_request);                                                                         \
        (__rq)->private       = (__private);                                                                               \
    } while (0)

/* Block device flags */
#define VMM_BLOCK_DEVICE_RDONLY 0x00000001
#define VMM_BLOCK_DEVICE_RW     0x00000002

/** Block device */
struct vmm_block_device {
    double_list_t       head;
    vmm_block_device_t *parent;

    vmm_mutex_t   child_lock;
    uint32_t      child_count;
    double_list_t child_list;

    char         name[VMM_FIELD_NAME_SIZE];
    char         desc[VMM_FIELD_DESC_SIZE];
    vmm_device_t dev;

    uint32_t flags;
    uint64_t start_lba;
    uint64_t num_blocks;
    uint32_t block_size;

    vmm_request_queue_t *rq;

    /* NOTE: partition management uses part_manager_sign and
     * part_manager_priv for its own use.
     * NOTE: part_manager_sign will be unique to partition style
     * NOTE: part_manager_sign=0x0 is reserved and means unknown
     * partition style
     */
    uint32_t part_manager_sign;    /* To be used for partition management */
    void    *part_manager_private; /* To be used for partition management */
};

/* Notifier event when block device is registered */
#define VMM_BLOCK_DEVICE_EVENT_REGISTER   0x01
/* Notifier event when block device is unregistered */
#define VMM_BLOCK_DEVICE_EVENT_UNREGISTER 0x02

/** Representation of block device notifier event */
struct vmm_block_device_event {
    vmm_block_device_t *block_device;
    void               *data;
};

/** Register a notifier client to receive block device events */
int vmm_block_device_register_client(vmm_notifier_block_t *nb);

/** Unregister a notifier client to not receive block device events */
int vmm_block_device_unregister_client(vmm_notifier_block_t *nb);

/** Size of block device in bytes */
static inline uint64_t vmm_block_device_total_size(vmm_block_device_t *block_device)
{
    return (block_device) ? block_device->num_blocks * block_device->block_size : 0;
}

/** Generic block IO complete request */
int vmm_block_device_complete_request(vmm_request_t *r);

/** Generic block IO fail request */
int vmm_block_device_fail_request(vmm_request_t *r);

/** Generic block IO submit request */
int vmm_block_device_submit_request(vmm_block_device_t *block_device, vmm_request_t *r);

/** Generic block IO abort request */
int vmm_block_device_abort_request(vmm_request_t *r);

/** Generic block IO flush cached data
 *  Note: block device request queue might cache blocks for
 *  better performance. This API is a hint to request queue
 *  that dirty cached blocks need to written back.
 */
int vmm_block_device_flush_cache(vmm_block_device_t *block_device);

/** Generic block IO read/write
 *  Note: This is a blocking API hence must be
 *  called from Orphan (or Thread) Context
 */
uint64_t vmm_block_device_rw(vmm_block_device_t *block_device, enum vmm_request_type type, uint8_t *buf, uint64_t off, uint64_t len);

/** Generic block IO read */
#define vmm_block_device_read(block_device, dst, off, len)  vmm_block_device_rw((block_device), VMM_REQUEST_READ, (dst), (off), (len))

/** Generic block IO write */
#define vmm_block_device_write(block_device, src, off, len) vmm_block_device_rw((block_device), VMM_REQUEST_WRITE, (src), (off), (len))

/** Allocate block device */
vmm_block_device_t *vmm_block_device_alloc(void);

/** Free block device */
void vmm_block_device_free(vmm_block_device_t *block_device);

/** Register block device to device driver framework
 *  Note: Block device must have RDONLY or RW flag set.
 */
int vmm_block_device_register(vmm_block_device_t *block_device);

/** Add child block device and register it. */
int vmm_block_device_add_child(vmm_block_device_t *block_device, uint64_t start_lba, uint64_t num_blocks);

/** Unregister block device from device driver framework */
int vmm_block_device_unregister(vmm_block_device_t *block_device);

/** Find a block device in device driver framework */
vmm_block_device_t *vmm_block_device_find(const char *name);

/** Iterate over each block device */
int vmm_block_device_iterate(vmm_block_device_t *start, void *data, int (*fn)(vmm_block_device_t *dev, void *data));

/** Count number of block devices */
uint32_t vmm_block_device_count(void);

#endif /* __VMM_BLOCK_DEVICE_H_ */
