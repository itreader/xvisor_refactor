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
 * @file vmm_virtual_disk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for virtual disk framework
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vio/vmm_virtual_disk.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC      "Virtual Disk Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VIRTUAL_DISK_IPRIORITY)
#define MODULE_INIT      vmm_virtual_disk_init
#define MODULE_EXIT      vmm_virtual_disk_exit

struct vmm_virtual_disk_ctrl {
    vmm_mutex_t                   virtual_disk_list_lock;
    double_list_t                 virtual_disk_list;
    vmm_blocking_notifier_chain_t notifier_chain;
    vmm_notifier_block_t          block_client;
};

static struct vmm_virtual_disk_ctrl vdctrl;

int vmm_virtual_disk_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&vdctrl.notifier_chain, nb);
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_register_client);

int vmm_virtual_disk_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&vdctrl.notifier_chain, nb);
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_unregister_client);

static void virtual_disk_req_completed(vmm_request_t *r)
{
    struct vmm_virtual_disk_request *vreq         = container_of(r, struct vmm_virtual_disk_request, r);
    struct vmm_virtual_disk         *virtual_disk = vreq->virtual_disk;

    if (virtual_disk->completed) {
        virtual_disk->completed(virtual_disk, vreq);
    }

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d\n", __func__, virtual_disk->name, (uint64_t)r->lba, r->bcnt);
}

static void virtual_disk_req_failed(vmm_request_t *r)
{
    struct vmm_virtual_disk_request *vreq         = container_of(r, struct vmm_virtual_disk_request, r);
    struct vmm_virtual_disk         *virtual_disk = vreq->virtual_disk;

    if (virtual_disk->failed) {
        virtual_disk->failed(virtual_disk, vreq);
    }

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d\n", __func__, virtual_disk->name, (uint64_t)r->lba, r->bcnt);
}

void vmm_virtual_disk_set_request_type(struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type)
{
    if (!vreq) {
        return;
    }

    switch (type) {
        case VMM_VIRTUAL_DISK_REQUEST_READ:
            vreq->r.type = VMM_REQUEST_READ;
            break;

        case VMM_VIRTUAL_DISK_REQUEST_WRITE:
            vreq->r.type = VMM_REQUEST_WRITE;
            break;

        default:
            vreq->r.type = VMM_REQUEST_UNKNOWN;
            break;
    };
}

enum vmm_virtual_disk_request_type vmm_virtual_disk_get_request_type(struct vmm_virtual_disk_request *vreq)
{
    enum vmm_virtual_disk_request_type type;

    if (!vreq) {
        return VMM_VIRTUAL_DISK_REQUEST_UNKNOWN;
    }

    switch (vreq->r.type) {
        case VMM_REQUEST_READ:
            type = VMM_VIRTUAL_DISK_REQUEST_READ;
            break;

        case VMM_REQUEST_WRITE:
            type = VMM_VIRTUAL_DISK_REQUEST_WRITE;
            break;

        default:
            type = VMM_VIRTUAL_DISK_REQUEST_UNKNOWN;
            break;
    };

    return type;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_get_request_type);

void vmm_virtual_disk_set_request_len(struct vmm_virtual_disk_request *vreq, uint32_t data_len)
{
    irq_flags_t              flags;
    struct vmm_virtual_disk *virtual_disk;

    if (!vreq || !vreq->virtual_disk) {
        return;
    }

    virtual_disk = vreq->virtual_disk;
    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);
    vreq->r.bcnt = udiv32(data_len, virtual_disk->block_size) * virtual_disk->block_factor;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_set_request_len);

uint32_t vmm_virtual_disk_get_request_len(struct vmm_virtual_disk_request *vreq)
{
    uint32_t                 ret = 0;
    irq_flags_t              flags;
    struct vmm_virtual_disk *virtual_disk;

    if (!vreq || !vreq->virtual_disk) {
        return 0;
    }

    virtual_disk = vreq->virtual_disk;
    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);
    ret = udiv32(vreq->r.bcnt, virtual_disk->block_factor) * virtual_disk->block_size;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    return ret;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_get_request_len);

int vmm_virtual_disk_submit_request(
    struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type, uint64_t lba, void *data,
    uint32_t data_len)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk || !vreq || !data) {
        return VMM_EINVALID;
    }

    if (data_len < virtual_disk->block_size) {
        return VMM_EINVALID;
    }

    if ((type < VMM_VIRTUAL_DISK_REQUEST_READ) || (VMM_VIRTUAL_DISK_REQUEST_WRITE < type)) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        vreq->virtual_disk = virtual_disk;
        vmm_virtual_disk_set_request_type(vreq, type);
        vreq->r.lba       = (lba + virtual_disk->blk->start_lba) * virtual_disk->block_factor;
        vreq->r.bcnt      = udiv32(data_len, virtual_disk->block_size) * virtual_disk->block_factor;
        vreq->r.data      = data;
        vreq->r.completed = virtual_disk_req_completed;
        vreq->r.failed    = virtual_disk_req_failed;
        vreq->r.private   = NULL;
        rc                = vmm_block_device_submit_request(virtual_disk->blk, &vreq->r);
    } else {
        virtual_disk->failed(virtual_disk, vreq);
        rc = VMM_ENODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d rc=%d\n", __func__, virtual_disk->name, (uint64_t)vreq->r.lba, vreq->r.bcnt, rc);

    return rc;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_submit_request);

int vmm_virtual_disk_abort_request(struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk || !vreq) {
        return VMM_EINVALID;
    }

    if (vreq->virtual_disk != virtual_disk) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        rc = vmm_block_device_abort_request(&vreq->r);
    } else {
        rc = VMM_ENODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d rc=%d\n", __func__, virtual_disk->name, (uint64_t)vreq->r.lba, vreq->r.bcnt, rc);

    return rc;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_abort_request);

int vmm_virtual_disk_flush_cache(struct vmm_virtual_disk *virtual_disk)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        rc = vmm_block_device_flush_cache(virtual_disk->blk);
    } else {
        rc = VMM_ENODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    DPRINTF("%s: virtual_disk=%s rc=%d\n", __func__, virtual_disk->name, rc);

    return rc;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_flush_cache);

uint64_t vmm_virtual_disk_capacity(struct vmm_virtual_disk *virtual_disk)
{
    uint64_t    ret = 0;
    irq_flags_t flags;

    if (!virtual_disk) {
        return 0;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        ret = udiv64(virtual_disk->blk->num_blocks, virtual_disk->block_factor);
    } else {
        ret = 0;
    }

    ret = (virtual_disk->blk) ? virtual_disk->blk->num_blocks : 0;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    return ret;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_capacity);

int vmm_virtual_disk_current_block_device(struct vmm_virtual_disk *virtual_disk, char *name, uint32_t name_len)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk || !name || !name_len) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        strncpy(name, virtual_disk->blk->name, name_len);
        rc = VMM_OK;
    } else {
        rc = VMM_ENODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    return rc;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_current_block_device);

struct virtual_disk_attach_priv {
    struct vmm_virtual_disk *virtual_disk;
    const char              *bdev_name;
};

static int virtual_disk_attach_iter(vmm_block_device_t *dev, void *data)
{
    bool                             attached;
    irq_flags_t                      flags;
    struct virtual_disk_attach_priv *ap           = data;
    const char                      *bdev_name    = ap->bdev_name;
    struct vmm_virtual_disk         *virtual_disk = ap->virtual_disk;

    if (strncmp(dev->name, bdev_name, sizeof(dev->name)) == 0) {
        attached = FALSE;
        vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

        if (!virtual_disk->blk && (dev->block_size <= virtual_disk->block_size) && !umod32(virtual_disk->block_size, dev->block_size)) {
            virtual_disk->blk          = dev;
            virtual_disk->block_factor = udiv32(virtual_disk->block_size, virtual_disk->blk->block_size);
            attached                   = TRUE;
        }

        vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

        if (attached && virtual_disk->attached) {
            virtual_disk->attached(virtual_disk);
        }
    }

    return VMM_OK;
}

void vmm_virtual_disk_attach_block_device(struct vmm_virtual_disk *virtual_disk, const char *bdev_name)
{
    struct virtual_disk_attach_priv ap;

    if (!virtual_disk || !bdev_name) {
        return;
    }

    ap.virtual_disk = virtual_disk;
    ap.bdev_name    = bdev_name;
    vmm_block_device_iterate(NULL, &ap, virtual_disk_attach_iter);
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_attach_block_device);

void vmm_virtual_disk_detach_block_device(struct vmm_virtual_disk *virtual_disk)
{
    bool        detached;
    irq_flags_t flags;

    if (!virtual_disk) {
        return;
    }

    detached = FALSE;
    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        vmm_block_device_flush_cache(virtual_disk->blk);
        detached = TRUE;
    }

    virtual_disk->blk          = NULL;
    virtual_disk->block_factor = 1;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    if (detached && virtual_disk->detached) {
        virtual_disk->detached(virtual_disk);
    }
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_detach_block_device);

struct vmm_virtual_disk *vmm_virtual_disk_create(
    const char *name, uint32_t block_size, void (*attached)(struct vmm_virtual_disk *), void (*detached)(struct vmm_virtual_disk *),
    void (*completed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *),
    void (*failed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *), void *private)
{
    bool                          found;
    struct vmm_virtual_disk      *virtual_disk;
    struct vmm_virtual_disk_event event;

    if (!name || !block_size || !completed || !failed) {
        return NULL;
    }

    virtual_disk = NULL;
    found        = FALSE;

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        if (strcmp(name, virtual_disk->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return NULL;
    }

    virtual_disk = vmm_zalloc(sizeof(struct vmm_virtual_disk));

    if (!virtual_disk) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return NULL;
    }

    INIT_LIST_HEAD(&virtual_disk->head);

    if (strlcpy(virtual_disk->name, name, sizeof(virtual_disk->name)) >= sizeof(virtual_disk->name)) {
        vmm_free(virtual_disk);
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return NULL;
    }

    virtual_disk->block_size = block_size;
    virtual_disk->attached   = attached;
    virtual_disk->detached   = detached;
    virtual_disk->completed  = completed;
    virtual_disk->failed     = failed;
    INIT_SPIN_LOCK(&virtual_disk->block_lock);
    virtual_disk->blk          = NULL;
    virtual_disk->block_factor = 1;
    virtual_disk->private      = private;

    list_add_tail(&virtual_disk->head, &vdctrl.virtual_disk_list);

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    /* Broadcast create event */
    event.virtual_disk = virtual_disk;
    event.data         = NULL;
    vmm_blocking_notifier_call(&vdctrl.notifier_chain, VMM_VIRTUAL_DISK_EVENT_CREATE, &event);

    return virtual_disk;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_create);

int vmm_virtual_disk_destroy(struct vmm_virtual_disk *virtual_disk)
{
    bool                          found;
    struct vmm_virtual_disk      *vd;
    struct vmm_virtual_disk_event event;

    if (!virtual_disk) {
        return VMM_EFAIL;
    }

    /* Detach current block device */
    vmm_virtual_disk_detach_block_device(virtual_disk);

    /* Broadcast destroy event */
    event.virtual_disk = virtual_disk;
    event.data         = NULL;
    vmm_blocking_notifier_call(&vdctrl.notifier_chain, VMM_VIRTUAL_DISK_EVENT_DESTROY, &event);

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    if (list_empty(&vdctrl.virtual_disk_list)) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return VMM_EFAIL;
    }

    vd    = NULL;
    found = FALSE;

    list_for_each_entry(vd, &vdctrl.virtual_disk_list, head)
    {
        if (strcmp(vd->name, virtual_disk->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return VMM_ENOTAVAIL;
    }

    list_del(&vd->head);

    vmm_free(vd);

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_destroy);

struct vmm_virtual_disk *vmm_virtual_disk_find(const char *name)
{
    bool                     found;
    struct vmm_virtual_disk *virtual_disk;

    if (!name) {
        return NULL;
    }

    found        = FALSE;
    virtual_disk = NULL;

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        if (strcmp(virtual_disk->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    if (!found) {
        return NULL;
    }

    return virtual_disk;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_find);

int vmm_virtual_disk_iterate(struct vmm_virtual_disk *start, void *data, int (*fn)(struct vmm_virtual_disk *virtual_disk, void *data))
{
    int                      rc          = VMM_OK;
    bool                     start_found = (start) ? FALSE : TRUE;
    struct vmm_virtual_disk *vd          = NULL;

    if (!fn) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(vd, &vdctrl.virtual_disk_list, head)
    {
        if (!start_found) {
            if (start && start == vd) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vd, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return rc;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_iterate);

uint32_t vmm_virtual_disk_count(void)
{
    uint32_t                 retval = 0;
    struct vmm_virtual_disk *virtual_disk;

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return retval;
}

VMM_EXPORT_SYMBOL(vmm_virtual_disk_count);

static int virtual_disk_block_notification(vmm_notifier_block_t *nb, uint64_t evt, void *data)
{
    irq_flags_t                    flags;
    struct vmm_virtual_disk       *virtual_disk;
    struct vmm_block_device_event *e = data;

    if (evt != VMM_BLOCK_DEVICE_EVENT_UNREGISTER) {
        /* We are only interested in unregister events so,
         * don't care about this event.
         */
        return NOTIFY_DONE;
    }

    /* Lock virtual disk list */
    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    /* Find virtual disk using block device */
    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

        if (virtual_disk->blk == e->block_device) {
            virtual_disk->blk          = NULL;
            virtual_disk->block_factor = 1;
        }

        vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);
    }

    /* Unlock virtual disk list */
    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return NOTIFY_OK;
}

static int __init vmm_virtual_disk_init(void)
{
    memset(&vdctrl, 0, sizeof(vdctrl));

    INIT_MUTEX(&vdctrl.virtual_disk_list_lock);
    INIT_LIST_HEAD(&vdctrl.virtual_disk_list);
    BLOCKING_INIT_NOTIFIER_CHAIN(&vdctrl.notifier_chain);

    vdctrl.block_client.notifier_call = &virtual_disk_block_notification;
    vdctrl.block_client.priority      = 0;
    vmm_block_device_register_client(&vdctrl.block_client);

    return VMM_OK;
}

static void __exit vmm_virtual_disk_exit(void)
{
    vmm_block_device_unregister_client(&vdctrl.block_client);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
