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
 * @file ram_backed_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RAM backed block device driver.
 */

#include <block/vmm_block_request_queue.h>
#include <drv/ram_backed_device.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_device_driver.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>

#define MODULE_DESC      "RAM Backed Block Driver"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (RBD_IPRIORITY)
#define MODULE_INIT      ram_backed_device_driver_init
#define MODULE_EXIT      ram_backed_device_driver_exit

static LIST_HEAD(ram_backed_device_list);
static DEFINE_SPINLOCK(ram_backed_device_list_lock);

static int ram_backed_device_read_cache(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private)
{
    struct ram_backed_device *d = private;
    physical_addr_t           pa;
    physical_size_t           size;

    pa   = d->addr + r->lba * RBD_BLOCK_SIZE;
    size = r->bcnt * RBD_BLOCK_SIZE;

    vmm_host_memory_read(pa, r->data, size, TRUE);

    return VMM_OK;
}

static int ram_backed_device_write_cache(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private)
{
    struct ram_backed_device *d = private;
    physical_addr_t           pa;
    physical_size_t           size;

    pa   = d->addr + r->lba * RBD_BLOCK_SIZE;
    size = r->bcnt * RBD_BLOCK_SIZE;

    vmm_host_memory_write(pa, r->data, size, TRUE);

    return VMM_OK;
}

static vmm_block_request_queue_ops_t ram_backed_device_rq_ops = {
    .read_cache = ram_backed_device_read_cache, .write_cache = ram_backed_device_write_cache};

static struct ram_backed_device *__ram_backed_device_create(
    vmm_device_t *dev, const char *name, physical_addr_t pa, physical_size_t size, bool ignore_overlap)
{
    struct ram_backed_device  *d;
    irq_flags_t                flags;
    vmm_block_request_queue_t *brq;

    if (!name) {
        return NULL;
    }

    d = vmm_zalloc(sizeof(struct ram_backed_device));

    if (!d) {
        goto free_nothing;
    }

    INIT_LIST_HEAD(&d->head);
    d->addr         = pa;
    d->size         = size;

    d->block_device = vmm_block_device_alloc();

    if (!d->block_device) {
        goto free_rbd;
    }

    /* Setup block device instance */
    strncpy(d->block_device->name, name, VMM_FIELD_NAME_SIZE);
    strncpy(d->block_device->desc, "RAM backed block device", VMM_FIELD_DESC_SIZE);
    d->block_device->dev.parent = dev;
    d->block_device->flags      = VMM_BLOCK_DEVICE_RW;
    d->block_device->start_lba  = 0;
    d->block_device->num_blocks = udiv64(d->size, RBD_BLOCK_SIZE);
    d->block_device->block_size = RBD_BLOCK_SIZE;

    /* Setup request queue for block device instance */
    brq                         = vmm_block_request_queue_create(name, 8, FALSE, &ram_backed_device_rq_ops, d);

    if (!brq) {
        goto free_bdev;
    }

    d->block_device->rq = vmm_block_request_queue_to_rq(brq);

    /* Register block device instance */
    if (vmm_block_device_register(d->block_device)) {
        goto free_bdev_rq;
    }

    /* Reserve RAM space If required */
    if (ignore_overlap) {
        physical_addr_t check_pa = d->addr;

        while (check_pa < (d->addr + d->size)) {
            if (vmm_host_ram_frame_isfree(check_pa)) {
                if (vmm_host_ram_reserve(check_pa, VMM_PAGE_SIZE)) {
                    goto unreg_bdev;
                }
            }

            check_pa += VMM_PAGE_SIZE;
        }
    } else {
        if (vmm_host_ram_reserve(d->addr, d->size)) {
            goto unreg_bdev;
        }
    }

    /* Add to list of RBD instances */
    vmm_spin_lock_irq_save(&ram_backed_device_list_lock, flags);
    list_add_tail(&d->head, &ram_backed_device_list);
    vmm_spin_unlock_irq_restore(&ram_backed_device_list_lock, flags);

    return d;

unreg_bdev:
    vmm_block_device_unregister(d->block_device);
free_bdev_rq:
    vmm_block_request_queue_destroy(vmm_rq_to_block_request_queue(d->block_device->rq));
free_bdev:
    vmm_block_device_free(d->block_device);
free_rbd:
    vmm_free(d);
free_nothing:
    return NULL;
}

struct ram_backed_device *ram_backed_device_create(const char *name, physical_addr_t pa, physical_size_t size, bool ignore_overlap)
{
    return __ram_backed_device_create(NULL, name, pa, size, ignore_overlap);
}

VMM_EXPORT_SYMBOL(ram_backed_device_create);

void ram_backed_device_destroy(struct ram_backed_device *d)
{
    irq_flags_t flags;

    /* Sanity check */
    if (!d) {
        return;
    }

    /* Remove from list of RBD instances */
    vmm_spin_lock_irq_save(&ram_backed_device_list_lock, flags);
    list_del(&d->head);
    vmm_spin_unlock_irq_restore(&ram_backed_device_list_lock, flags);

    /* Unreserver RAM space */
    vmm_host_ram_free(d->addr, d->size);

    /* Unregister block device */
    vmm_block_device_unregister(d->block_device);

    /* Free block device request queue */
    vmm_block_request_queue_destroy(vmm_rq_to_block_request_queue(d->block_device->rq));

    /* Free block device */
    vmm_block_device_free(d->block_device);

    /* Free RBD instance */
    vmm_free(d);
}

VMM_EXPORT_SYMBOL(ram_backed_device_destroy);

struct ram_backed_device *ram_backed_device_find(const char *name)
{
    bool                      found;
    double_list_t            *l;
    struct ram_backed_device *d;
    irq_flags_t               flags;

    if (!name) {
        return NULL;
    }

    found = FALSE;
    d     = NULL;

    vmm_spin_lock_irq_save(&ram_backed_device_list_lock, flags);

    list_for_each(l, &ram_backed_device_list)
    {
        d = list_entry(l, struct ram_backed_device, head);

        if (strcmp(d->block_device->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&ram_backed_device_list_lock, flags);

    if (!found) {
        return NULL;
    }

    return d;
}

VMM_EXPORT_SYMBOL(ram_backed_device_find);

struct ram_backed_device *ram_backed_device_get(int index)
{
    bool                      found;
    double_list_t            *l;
    struct ram_backed_device *retval;
    irq_flags_t               flags;

    if (index < 0) {
        return NULL;
    }

    retval = NULL;
    found  = FALSE;

    vmm_spin_lock_irq_save(&ram_backed_device_list_lock, flags);

    list_for_each(l, &ram_backed_device_list)
    {
        retval = list_entry(l, struct ram_backed_device, head);

        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&ram_backed_device_list_lock, flags);

    if (!found) {
        return NULL;
    }

    return retval;
}

VMM_EXPORT_SYMBOL(ram_backed_device_get);

uint32_t ram_backed_device_count(void)
{
    uint32_t       retval = 0;
    double_list_t *l;
    irq_flags_t    flags;

    vmm_spin_lock_irq_save(&ram_backed_device_list_lock, flags);

    list_for_each(l, &ram_backed_device_list)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&ram_backed_device_list_lock, flags);

    return retval;
}

VMM_EXPORT_SYMBOL(ram_backed_device_count);

static int ram_backed_device_driver_probe(vmm_device_t *dev)
{
    int             rc;
    physical_addr_t pa;
    physical_size_t size;

    rc = vmm_device_tree_regaddr(dev->of_node, &pa, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_regsize(dev->of_node, &size, 0);

    if (rc) {
        return rc;
    }

    dev->private = __ram_backed_device_create(dev, dev->name, pa, size, false);

    if (!dev->private) {
        return VMM_EFAIL;
    }

    return VMM_OK;
}

static int ram_backed_device_driver_remove(vmm_device_t *dev)
{
    ram_backed_device_destroy(dev->private);

    return VMM_OK;
}

static struct vmm_device_tree_nodeid ram_backed_device_devid_table[] = {
    {.compatible = "ram_backed_device"},
    {/* end of list */},
};

static vmm_driver_t ram_backed_device_driver = {
    .name        = "ram_backed_device",
    .match_table = ram_backed_device_devid_table,
    .probe       = ram_backed_device_driver_probe,
    .remove      = ram_backed_device_driver_remove,
};

static int __init ram_backed_device_driver_init(void)
{
    return vmm_device_driver_register_driver(&ram_backed_device_driver);
}

static void __exit ram_backed_device_driver_exit(void)
{
    vmm_device_driver_unregister_driver(&ram_backed_device_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
