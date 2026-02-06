/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vmm_virtual_disk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual disk framework
 */

/* The virtual disk framework helps disk controller emulators
 * in emulating disk read/write operations irrespective to
 * disk controller type. It also provides a convient way of
 * tracking various virtual disk instances of a guest.
 *
 * Each virtual disk can be attached to a block device. If a
 * block device attached to virtual disk is unregistered then
 * virtual disk is dettached automatically.
 *
 * All IO on virtual disk have to be done using opaque struct
 * vmm_virtual_disk_request. The struct vmm_virtual_disk_request is a wrapper
 * struct on-top of struct vmm_request. The emulators don't need
 * to explicity fill properties of vmm_virtual_disk_request because
 * vmm_virtual_disk_submit_request() will automatically fill it. If
 * the emulators still need access to individual properties of
 * vmm_virtual_disk_request then they will have to use vmm_virtual_disk APIs.
 */

#ifndef _VMM_VIRTUAL_DISK_H__
#define _VMM_VIRTUAL_DISK_H__

#include <block/vmm_block_device.h>
#include <libs/list.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_VIRTUAL_DISK_IPRIORITY (VMM_BLOCK_DEVICE_CLASS_IPRIORITY + 1)

struct vmm_virtual_disk_request;
struct vmm_virtual_disk;

/** Types of block IO request */
enum vmm_virtual_disk_request_type {
    VMM_VIRTUAL_DISK_REQUEST_UNKNOWN = 0,
    VMM_VIRTUAL_DISK_REQUEST_READ    = 1,
    VMM_VIRTUAL_DISK_REQUEST_WRITE   = 2
};

/** Representation of a virtual disk request  */
struct vmm_virtual_disk_request {
    struct vmm_virtual_disk *virtual_disk;
    vmm_request_t            r;
};

/** Representation of a virtual disk */
struct vmm_virtual_disk {
    double_list_t head;
    char          name[VMM_FIELD_NAME_SIZE];
    uint32_t      block_size;

    void (*attached)(struct vmm_virtual_disk *);
    void (*detached)(struct vmm_virtual_disk *);
    void (*completed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *);
    void (*failed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *);

    vmm_spinlock_t      block_lock; /* Protect blk pointer */
    vmm_block_device_t *blk;
    uint32_t            block_factor;

    void *private;
};

/* Notifier event when virtual disk is created */
#define VMM_VIRTUAL_DISK_EVENT_CREATE  0x01
/* Notifier event when virtual disk is destroyed */
#define VMM_VIRTUAL_DISK_EVENT_DESTROY 0x02

/** Representation of virtual disk notifier event */
struct vmm_virtual_disk_event {
    struct vmm_virtual_disk *virtual_disk;
    void                    *data;
};

/** Register a notifier client to receive virtual disk events */
int vmm_virtual_disk_register_client(vmm_notifier_block_t *nb);

/** Unregister a notifier client to not receive virtual disk events */
int vmm_virtual_disk_unregister_client(vmm_notifier_block_t *nb);

/** Set virtual_disk pointer of given virtual disk request */
static inline void vmm_virtual_disk_set_request_disk(struct vmm_virtual_disk_request *vreq, struct vmm_virtual_disk *virtual_disk)
{
    if (vreq) {
        vreq->virtual_disk = virtual_disk;
    }
}

/** Get virtual_disk pointer of given virtual disk request */
static inline struct vmm_virtual_disk *vmm_virtual_disk_get_request_disk(struct vmm_virtual_disk_request *vreq)
{
    return (vreq) ? vreq->virtual_disk : NULL;
}

/** Set type of given virtual disk request */
void vmm_virtual_disk_set_request_type(struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type);

/** Get type of given virtual disk request */
enum vmm_virtual_disk_request_type vmm_virtual_disk_get_request_type(struct vmm_virtual_disk_request *vreq);

/** Set lba of given virtual disk request */
static inline void vmm_virtual_disk_set_request_lba(struct vmm_virtual_disk_request *vreq, uint64_t lba)
{
    if (vreq) {
        vreq->r.lba = lba;
    }
}

/** Get lba of given virtual disk request */
static inline uint64_t vmm_virtual_disk_get_request_lba(struct vmm_virtual_disk_request *vreq)
{
    return (vreq) ? vreq->r.lba : 0;
}

/** Set data of given virtual disk request */
static inline void vmm_virtual_disk_set_request_data(struct vmm_virtual_disk_request *vreq, void *data)
{
    if (vreq) {
        vreq->r.data = data;
    }
}

/** Get data of given virtual disk request */
static inline void *vmm_virtual_disk_get_request_data(struct vmm_virtual_disk_request *vreq)
{
    return (vreq) ? vreq->r.data : NULL;
}

/** Set data length of given virtual disk request
 *  NOTE: This function will only work if vreq->virtual_disk is set
 */
void vmm_virtual_disk_set_request_len(struct vmm_virtual_disk_request *vreq, uint32_t data_len);

/** Get data length of given virtual disk request
 *  NOTE: This function will only work if vreq->virtual_disk is set
 */
uint32_t vmm_virtual_disk_get_request_len(struct vmm_virtual_disk_request *vreq);

/** Retrive private context of virtual disk */
static inline void *vmm_virtual_disk_private(struct vmm_virtual_disk *virtual_disk)
{
    return (virtual_disk) ? virtual_disk->private : NULL;
}

/** Submit IO request to virtual disk */
int vmm_virtual_disk_submit_request(
    struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type, uint64_t lba, void *data,
    uint32_t data_len);

/* Abort IO request from virtual disk */
int vmm_virtual_disk_abort_request(struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq);

/** Flush cached IO from virtual disk */
int vmm_virtual_disk_flush_cache(struct vmm_virtual_disk *virtual_disk);

/** Name of virtual disk */
static inline const char *vmm_virtual_disk_name(struct vmm_virtual_disk *virtual_disk)
{
    return (virtual_disk) ? virtual_disk->name : NULL;
}

/** Block size of virtual disk */
static inline uint32_t vmm_virtual_disk_block_size(struct vmm_virtual_disk *virtual_disk)
{
    return (virtual_disk) ? virtual_disk->block_size : 0;
}

/** Block count of virtual disk based on attached block device */
uint64_t vmm_virtual_disk_capacity(struct vmm_virtual_disk *virtual_disk);

/** Current block device attached to virtual disk */
int vmm_virtual_disk_current_block_device(struct vmm_virtual_disk *virtual_disk, char *buf, uint32_t buf_len);

/** Attach block device to virtual disk */
void vmm_virtual_disk_attach_block_device(struct vmm_virtual_disk *virtual_disk, const char *bdev_name);

/** Detach block device from virtual disk */
void vmm_virtual_disk_detach_block_device(struct vmm_virtual_disk *virtual_disk);

/** Create a virtual disk */
struct vmm_virtual_disk *vmm_virtual_disk_create(
    const char *name, uint32_t block_size, void (*attached)(struct vmm_virtual_disk *), void (*detached)(struct vmm_virtual_disk *),
    void (*completed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *),
    void (*failed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *), void *private);

/** Destroy a virtual disk */
int vmm_virtual_disk_destroy(struct vmm_virtual_disk *virtual_disk);

/** Find a virtual disk with given name */
struct vmm_virtual_disk *vmm_virtual_disk_find(const char *name);

/** Iterate over each virtual disk */
int vmm_virtual_disk_iterate(struct vmm_virtual_disk *start, void *data, int (*fn)(struct vmm_virtual_disk *virtual_disk, void *data));

/** Count of available virtual disks */
uint32_t vmm_virtual_disk_count(void);

#endif
