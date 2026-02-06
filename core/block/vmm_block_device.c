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
 * @file vmm_block_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework source
 */

#include <block/vmm_block_device.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_completion.h>
#include <vmm_device_driver.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Block Device Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY VMM_BLOCK_DEVICE_CLASS_IPRIORITY
#define MODULE_INIT      vmm_block_device_init
#define MODULE_EXIT      vmm_block_device_exit

static BLOCKING_NOTIFIER_CHAIN(bdev_notifier_chain);

int vmm_block_device_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&bdev_notifier_chain, nb);
}

VMM_EXPORT_SYMBOL(vmm_block_device_register_client);

int vmm_block_device_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&bdev_notifier_chain, nb);
}

VMM_EXPORT_SYMBOL(vmm_block_device_unregister_client);

static int __block_device_peek_cache(vmm_block_device_t *block_device, vmm_request_t *r)
{
    vmm_request_queue_t *rq = block_device->rq;

    if (!rq->peek_cache) {
        return VMM_ENOTAVAIL;
    }

    return rq->peek_cache(rq, r);
}

static int __block_device_make_request(vmm_block_device_t *block_device, vmm_request_t *r, bool append_backlog)
{
    int                  rc = VMM_OK;
    vmm_request_queue_t *rq = block_device->rq;

    INIT_LIST_HEAD(&r->head);
    r->block_device = block_device;

    if (rq->pending_count < rq->max_pending) {
        rc = rq->make_request(rq, r);

        if (!rc) {
            rq->pending_count++;
        }
    } else if (rq->backlog_count < U32_MAX) {
        if (append_backlog) {
            list_add_tail(&r->head, &rq->backlog_list);
        } else {
            list_add(&r->head, &rq->backlog_list);
        }

        rq->backlog_count++;
    } else {
        rc = VMM_ENOSPC;
    }

    if (rc) {
        r->block_device = NULL;
    }

    return rc;
}

static void __block_device_done_request(vmm_request_queue_t *rq)
{
    int            rc = VMM_OK;
    vmm_request_t *r;

    if (rq->pending_count) {
        rq->pending_count--;
    }

    if (list_empty(&rq->backlog_list)) {
        return;
    }

    r = list_first_entry(&rq->backlog_list, struct vmm_request, head);
    list_del(&r->head);
    rq->backlog_count--;

    rc = __block_device_make_request(r->block_device, r, FALSE);

    if (rc) {
        vmm_printf("%s: block_device=%p failed with error %d\n", __func__, r->block_device, rc);
        WARN_ON(1);
    }
}

int vmm_block_device_complete_request(vmm_request_t *r)
{
    irq_flags_t          flags;
    vmm_request_queue_t *rq;

    if (!r || !r->block_device || !r->block_device->rq) {
        return VMM_EINVALID;
    }

    rq              = r->block_device->rq;
    r->block_device = NULL;

    if (r->completed) {
        r->completed(r);
    }

    vmm_spin_lock_irq_save(&rq->lock, flags);
    __block_device_done_request(rq);
    vmm_spin_unlock_irq_restore(&rq->lock, flags);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_block_device_complete_request);

int vmm_block_device_fail_request(vmm_request_t *r)
{
    irq_flags_t          flags;
    vmm_request_queue_t *rq;

    if (!r || !r->block_device || !r->block_device->rq) {
        return VMM_EINVALID;
    }

    rq              = r->block_device->rq;
    r->block_device = NULL;

    if (r->failed) {
        r->failed(r);
    }

    vmm_spin_lock_irq_save(&rq->lock, flags);
    __block_device_done_request(rq);
    vmm_spin_unlock_irq_restore(&rq->lock, flags);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_block_device_fail_request);

int vmm_block_device_submit_request(vmm_block_device_t *block_device, vmm_request_t *r)
{
    int                  rc;
    irq_flags_t          flags;
    vmm_request_queue_t *rq;

    if (!block_device || !r || !block_device->rq) {
        rc = VMM_EFAIL;
        goto failed;
    }

    rq = block_device->rq;

    if ((r->type == VMM_REQUEST_WRITE) && !(block_device->flags & VMM_BLOCK_DEVICE_RW)) {
        rc = VMM_EINVALID;
        goto failed;
    }

    if (block_device->num_blocks < r->bcnt) {
        rc = VMM_ERANGE;
        goto failed;
    }

    if ((r->lba < block_device->start_lba) || ((block_device->start_lba + block_device->num_blocks) <= r->lba)) {
        rc = VMM_ERANGE;
        goto failed;
    }

    if ((block_device->start_lba + block_device->num_blocks) < (r->lba + r->bcnt)) {
        rc = VMM_ERANGE;
        goto failed;
    }

    if (rq->peek_cache) {
        vmm_spin_lock_irq_save(&rq->lock, flags);
        rc = __block_device_peek_cache(block_device, r);
        vmm_spin_unlock_irq_restore(&rq->lock, flags);

        if (rc == VMM_OK) {
            if (r->completed) {
                r->completed(r);
            }

            return VMM_OK;
        } else if (rc != VMM_ENOTAVAIL) {
            if (r->failed) {
                r->failed(r);
            }

            return rc;
        } else {
            rc = VMM_OK;
        }
    }

    if (rq->make_request) {
        vmm_spin_lock_irq_save(&rq->lock, flags);
        rc = __block_device_make_request(block_device, r, TRUE);
        vmm_spin_unlock_irq_restore(&rq->lock, flags);

        if (rc) {
            return rc;
        }
    } else {
        rc = VMM_EFAIL;
        goto failed;
    }

    return VMM_OK;

failed:
    vmm_block_device_fail_request(r);
    return rc;
}

VMM_EXPORT_SYMBOL(vmm_block_device_submit_request);

int vmm_block_device_abort_request(vmm_request_t *r)
{
    int                 rc;
    irq_flags_t         flags;
    vmm_block_device_t *block_device;

    if (!r || !r->block_device || !r->block_device->rq) {
        return VMM_EFAIL;
    }

    block_device = r->block_device;

    if (block_device->rq->abort_request) {
        vmm_spin_lock_irq_save(&block_device->rq->lock, flags);
        rc = block_device->rq->abort_request(block_device->rq, r);
        vmm_spin_unlock_irq_restore(&block_device->rq->lock, flags);

        if (rc) {
            return rc;
        }
    }

    return vmm_block_device_fail_request(r);
}

VMM_EXPORT_SYMBOL(vmm_block_device_abort_request);

int vmm_block_device_flush_cache(vmm_block_device_t *block_device)
{
    int         rc;
    irq_flags_t flags;

    if (!block_device || !block_device->rq) {
        return VMM_EFAIL;
    }

    if (block_device->rq->flush_cache) {
        vmm_spin_lock_irq_save(&block_device->rq->lock, flags);
        rc = block_device->rq->flush_cache(block_device->rq);
        vmm_spin_unlock_irq_restore(&block_device->rq->lock, flags);

        if (rc) {
            return rc;
        }
    }

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_block_device_flush_cache);

struct block_device_rw {
    bool             failed;
    vmm_request_t    req;
    vmm_completion_t done;
};

static void block_device_rw_completed(vmm_request_t *req)
{
    struct block_device_rw *rw = req->private;

    if (!rw) {
        return;
    }

    rw->failed = FALSE;
    vmm_completion_complete(&rw->done);
}

static void block_device_rw_failed(vmm_request_t *req)
{
    struct block_device_rw *rw = req->private;

    if (!rw) {
        return;
    }

    rw->failed = TRUE;
    vmm_completion_complete(&rw->done);
}

static int block_device_rw_blocks(vmm_block_device_t *block_device, enum vmm_request_type type, uint8_t *buf, uint64_t lba, uint64_t bcnt)
{
    int                    rc;
    struct block_device_rw rw;

    rw.failed        = FALSE;
    rw.req.type      = type;
    rw.req.lba       = block_device->start_lba + lba;
    rw.req.bcnt      = bcnt;
    rw.req.data      = buf;
    rw.req.private   = &rw;
    rw.req.completed = block_device_rw_completed;
    rw.req.failed    = block_device_rw_failed;
    INIT_COMPLETION(&rw.done);

    if ((rc = vmm_block_device_submit_request(block_device, &rw.req))) {
        return rc;
    }

    vmm_completion_wait(&rw.done);

    if (rw.failed) {
        return VMM_EFAIL;
    }

    return VMM_OK;
}

uint64_t vmm_block_device_rw(vmm_block_device_t *block_device, enum vmm_request_type type, uint8_t *buf, uint64_t off, uint64_t len)
{
    uint8_t *tbuf = NULL;
    uint64_t tmp, first_lba, first_off, first_len;
    uint64_t middle_lba, middle_len;
    uint64_t last_lba, last_len;

    BUG_ON(!vmm_scheduler_orphan_context());

    if (!buf || !block_device || !len) {
        return 0;
    }

    if ((type != VMM_REQUEST_READ) && (type != VMM_REQUEST_WRITE)) {
        return 0;
    }

    if ((type == VMM_REQUEST_WRITE) && !(block_device->flags & VMM_BLOCK_DEVICE_RW)) {
        return 0;
    }

    tmp = block_device->num_blocks * block_device->block_size;

    if ((off >= tmp) || ((off + len) > tmp)) {
        return 0;
    }

    first_lba = udiv64(off, block_device->block_size);
    first_off = off - first_lba * block_device->block_size;

    if (first_off) {
        first_len = block_device->block_size - first_off;
        first_len = (first_len < len) ? first_len : len;
    } else {
        if (len < block_device->block_size) {
            first_len = len;
        } else {
            first_len = 0;
        }
    }

    off += first_len;
    len -= first_len;

    middle_lba = udiv64(off, block_device->block_size);
    middle_len = udiv64(len, block_device->block_size) * block_device->block_size;

    off += middle_len;
    len -= middle_len;

    last_lba = udiv64(off, block_device->block_size);
    last_len = len;

    if (first_len || last_len) {
        tbuf = vmm_malloc(block_device->block_size);

        if (!tbuf) {
            return 0;
        }
    }

    tmp = 0;

    if (first_len) {
        if (block_device_rw_blocks(block_device, VMM_REQUEST_READ, tbuf, first_lba, 1)) {
            goto done;
        }

        if (type == VMM_REQUEST_WRITE) {
            memcpy(&tbuf[first_off], buf, first_len);

            if (block_device_rw_blocks(block_device, VMM_REQUEST_WRITE, tbuf, first_lba, 1)) {
                goto done;
            }
        } else {
            memcpy(buf, &tbuf[first_off], first_len);
        }

        buf += first_len;
        tmp += first_len;
    }

    if (middle_len) {
        if (block_device_rw_blocks(block_device, type, buf, middle_lba, udiv64(middle_len, block_device->block_size))) {
            goto done;
        }

        buf += middle_len;
        tmp += middle_len;
    }

    if (last_len) {
        if (block_device_rw_blocks(block_device, VMM_REQUEST_READ, tbuf, last_lba, 1)) {
            goto done;
        }

        if (type == VMM_REQUEST_WRITE) {
            memcpy(&tbuf[0], buf, last_len);

            if (block_device_rw_blocks(block_device, VMM_REQUEST_WRITE, tbuf, last_lba, 1)) {
                goto done;
            }
        } else {
            memcpy(buf, &tbuf[0], last_len);
        }

        tmp += last_len;
    }

done:

    if (first_len || last_len) {
        vmm_free(tbuf);
    }

    return tmp;
}

VMM_EXPORT_SYMBOL(vmm_block_device_rw);

vmm_block_device_t *vmm_block_device_alloc(void)
{
    vmm_block_device_t *block_device;

    block_device = vmm_zalloc(sizeof(vmm_block_device_t));

    if (!block_device) {
        return NULL;
    }

    INIT_LIST_HEAD(&block_device->head);
    INIT_MUTEX(&block_device->child_lock);
    block_device->child_count = 0;
    INIT_LIST_HEAD(&block_device->child_list);
    block_device->rq = NULL;

    return block_device;
}

VMM_EXPORT_SYMBOL(vmm_block_device_alloc);

void vmm_block_device_free(vmm_block_device_t *block_device)
{
    vmm_free(block_device);
}

VMM_EXPORT_SYMBOL(vmm_block_device_free);

static vmm_class_t bdev_class = {
    .name = VMM_BLOCK_DEVICE_CLASS_NAME,
};

int vmm_block_device_register(vmm_block_device_t *block_device)
{
    int                           rc;
    struct vmm_block_device_event event;

    if (!block_device || !block_device->rq) {
        return VMM_EFAIL;
    }

    if (!(block_device->flags & VMM_BLOCK_DEVICE_RDONLY) && !(block_device->flags & VMM_BLOCK_DEVICE_RW)) {
        return VMM_EINVALID;
    }

    vmm_device_driver_initialize_device(&block_device->dev);

    if (strlcpy(block_device->dev.name, block_device->name, sizeof(block_device->dev.name)) >= sizeof(block_device->dev.name)) {
        return VMM_EOVERFLOW;
    }

    block_device->dev.class = &bdev_class;
    vmm_device_driver_set_data(&block_device->dev, block_device);

    rc = vmm_device_driver_register_device(&block_device->dev);

    if (rc) {
        return rc;
    }

    /* Broadcast register event */
    event.block_device = block_device;
    event.data         = NULL;
    vmm_blocking_notifier_call(&bdev_notifier_chain, VMM_BLOCK_DEVICE_EVENT_REGISTER, &event);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_block_device_register);

int vmm_block_device_add_child(vmm_block_device_t *block_device, uint64_t start_lba, uint64_t num_blocks)
{
    int                 rc;
    vmm_block_device_t *child_bdev;

    if (!block_device) {
        return VMM_EFAIL;
    }

    if (block_device->num_blocks < num_blocks) {
        return VMM_ERANGE;
    }

    if ((start_lba < block_device->start_lba) || ((block_device->start_lba + block_device->num_blocks) <= start_lba)) {
        return VMM_ERANGE;
    }

    if ((block_device->start_lba + block_device->num_blocks) < (start_lba + num_blocks)) {
        return VMM_ERANGE;
    }

    child_bdev             = vmm_block_device_alloc();
    child_bdev->parent     = block_device;
    child_bdev->dev.parent = &block_device->dev;
    vmm_mutex_lock(&block_device->child_lock);
    vmm_snprintf(child_bdev->name, sizeof(child_bdev->name), "%sp%d", block_device->name, block_device->child_count);

    if (strlcpy(child_bdev->desc, block_device->desc, sizeof(child_bdev->desc)) >= sizeof(child_bdev->desc)) {
        rc = VMM_EOVERFLOW;
        goto free_block_device;
    }

    block_device->child_count++;
    list_add_tail(&child_bdev->head, &block_device->child_list);
    vmm_mutex_unlock(&block_device->child_lock);
    child_bdev->flags      = block_device->flags;
    child_bdev->start_lba  = start_lba;
    child_bdev->num_blocks = num_blocks;
    child_bdev->block_size = block_device->block_size;
    child_bdev->rq         = block_device->rq;

    rc                     = vmm_block_device_register(child_bdev);

    if (rc) {
        goto remove_from_list;
    }

    return rc;

remove_from_list:
    vmm_mutex_lock(&block_device->child_lock);
    list_del(&child_bdev->head);
    vmm_mutex_unlock(&block_device->child_lock);
free_block_device:
    vmm_block_device_free(child_bdev);
    return rc;
}

VMM_EXPORT_SYMBOL(vmm_block_device_add_child);

int vmm_block_device_unregister(vmm_block_device_t *block_device)
{
    int                           rc;
    vmm_block_device_t           *child_bdev;
    struct vmm_block_device_event event;

    if (!block_device) {
        return VMM_EFAIL;
    }

    /* Unreg & free child block devices */
    vmm_mutex_lock(&block_device->child_lock);

    while (!list_empty(&block_device->child_list)) {
        child_bdev = list_first_entry(&block_device->child_list, vmm_block_device_t, head);
        list_del(&child_bdev->head);

        if ((rc = vmm_block_device_unregister(child_bdev))) {
            vmm_mutex_unlock(&block_device->child_lock);
            return rc;
        }

        vmm_block_device_free(child_bdev);
    }

    vmm_mutex_unlock(&block_device->child_lock);

    /* Broadcast unregister event */
    event.block_device = block_device;
    event.data         = NULL;
    vmm_blocking_notifier_call(&bdev_notifier_chain, VMM_BLOCK_DEVICE_EVENT_UNREGISTER, &event);

    return vmm_device_driver_unregister_device(&block_device->dev);
}

VMM_EXPORT_SYMBOL(vmm_block_device_unregister);

vmm_block_device_t *vmm_block_device_find(const char *name)
{
    vmm_device_t *dev;

    dev = vmm_device_driver_class_find_device_by_name(&bdev_class, name);

    if (!dev) {
        return NULL;
    }

    return vmm_device_driver_get_data(dev);
}

VMM_EXPORT_SYMBOL(vmm_block_device_find);

struct block_device_iterate_priv {
    void *data;
    int (*fn)(vmm_block_device_t *dev, void *data);
};

static int block_device_iterate(vmm_device_t *dev, void *data)
{
    struct block_device_iterate_priv *p            = data;
    vmm_block_device_t               *block_device = vmm_device_driver_get_data(dev);

    return p->fn(block_device, p->data);
}

int vmm_block_device_iterate(vmm_block_device_t *start, void *data, int (*fn)(vmm_block_device_t *dev, void *data))
{
    vmm_device_t                    *st = (start) ? &start->dev : NULL;
    struct block_device_iterate_priv p;

    if (!fn) {
        return VMM_EINVALID;
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&bdev_class, st, &p, block_device_iterate);
}

VMM_EXPORT_SYMBOL(vmm_block_device_iterate);

uint32_t vmm_block_device_count(void)
{
    return vmm_device_driver_class_device_count(&bdev_class);
}

VMM_EXPORT_SYMBOL(vmm_block_device_count);

static int __init vmm_block_device_init(void)
{
    vmm_init_printf("block device framework\n");

    return vmm_device_driver_register_class(&bdev_class);
}

static void __exit vmm_block_device_exit(void)
{
    vmm_device_driver_unregister_class(&bdev_class);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
