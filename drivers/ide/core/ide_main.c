/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file ide_main.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE device driver main source.
 */

#include <drv/ide/ide_core.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_limits.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_timer.h>

#define MODULE_NAME      ide_core
#define MODULE_DESC      "IDE Framework"
#define MODULE_AUTHOR    "Himanshu Chauhan"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY IDE_CORE_IPRIORITY
#define MODULE_INIT      ide_core_init
#define MODULE_EXIT      ide_core_exit

/*
 * Protected list of ide hosts.
 */
static DEFINE_MUTEX(ide_drive_list_mutex);
static LIST_HEAD(ide_drive_list);
static uint32_t ide_drive_count;

static char *drive_names[MAX_IDE_CHANNELS][MAX_IDE_DRIVES_PER_CHAN] = {
    {
     "hda0", "hda1",
     },
    {
     "hda2",      "hda3",
     }
};

static int ide_make_request(vmm_request_queue_t *rq, vmm_request_t *r);
static int ide_abort_request(vmm_request_queue_t *rq, vmm_request_t *r);

static int __init_ide_drive(struct ide_drive *drive)
{
    int                 rc = VMM_OK;
    vmm_block_device_t *block_device;
    uint8_t             chan, did;

    if (!drive) {
        return VMM_EFAIL;
    }

    if (drive->block_device) {
        return VMM_OK;
    }

    /* Allocate new block device instance */
    drive->block_device = vmm_block_device_alloc();

    if (!drive->block_device) {
        rc = VMM_ENOMEM;
        goto detect_freecard_fail;
    }

    block_device = drive->block_device;

    chan         = drive->channel->id;
    did          = drive->drive;

    /* Setup block device instance */
    vmm_snprintf(block_device->name, sizeof(block_device->name), "%s", drive_names[chan][did]);
    vmm_snprintf(block_device->desc, sizeof(block_device->desc), "%s", drive->model);

    block_device->dev.parent = drive->dev;
    block_device->flags      = VMM_BLOCK_DEVICE_RW;
    block_device->start_lba  = 0;
    block_device->block_size = drive->block_size;
    block_device->num_blocks = drive->size;

    /* Setup request queue for block device instance */
    block_device->rq         = vmm_zalloc(sizeof(vmm_request_queue_t));

    if (!block_device->rq) {
        goto detect_freebdev_fail;
    }

    INIT_REQUEST_QUEUE(block_device->rq, U32_MAX, NULL, ide_make_request, ide_abort_request, NULL, drive);

    rc = vmm_block_device_register(drive->block_device);

    if (rc) {
        goto detect_freerq_fail;
    }

    rc = VMM_OK;
    goto detect_done;

detect_freerq_fail:
    vmm_free(drive->block_device->rq);
detect_freebdev_fail:
    vmm_block_device_free(drive->block_device);
detect_freecard_fail:
detect_done:
    return rc;
}

static uint32_t __ide_bwrite(struct ide_drive *drive, uint64_t start, uint32_t blkcnt, const void *src)
{
    return drive->io_ops.block_write(drive, start, blkcnt, src);
}

static uint32_t __ide_bread(struct ide_drive *drive, uint64_t start, uint32_t blkcnt, void *dst)
{
    return drive->io_ops.block_read(drive, start, blkcnt, dst);
}

static int __ide_block_device_request(struct ide_drive *drive, vmm_request_queue_t *rq, vmm_request_t *r)
{
    int      rc;
    uint32_t cnt;

    if (!r) {
        return VMM_EFAIL;
    }

    if (!drive || !rq) {
        vmm_block_device_fail_request(r);
        return VMM_EFAIL;
    }

    switch (r->type) {
        case VMM_REQUEST_READ:
            cnt = __ide_bread(drive, r->lba, r->bcnt, r->data);

            if (cnt == r->bcnt) {
                vmm_block_device_complete_request(r);
                rc = VMM_OK;
            } else {
                vmm_block_device_fail_request(r);
                rc = VMM_EIO;
            }

            break;

        case VMM_REQUEST_WRITE:
            cnt = __ide_bwrite(drive, r->lba, r->bcnt, r->data);

            if (cnt == r->bcnt) {
                vmm_block_device_complete_request(r);
                rc = VMM_OK;
            } else {
                vmm_block_device_fail_request(r);
                rc = VMM_EIO;
            }

            break;

        default:
            vmm_block_device_fail_request(r);
            rc = VMM_EFAIL;
            break;
    };

    return rc;
}

static int ide_io_thread(void *tdata)
{
    irq_flags_t          f;
    double_list_t       *l;
    struct ide_drive_io *io;
    struct ide_drive    *drive = (struct ide_drive *)tdata;

    while (1) {
        if (vmm_completion_wait(&drive->io_avail) != VMM_OK) {
            vmm_printf("Failed to wait on completion.\n");
            return VMM_EFAIL;
        }

        vmm_spin_lock_irq_save(&drive->io_list_lock, f);

        if (list_empty(&drive->io_list)) {
            vmm_spin_unlock_irq_restore(&drive->io_list_lock, f);
            continue;
        }

        l = list_pop(&drive->io_list);
        vmm_spin_unlock_irq_restore(&drive->io_list_lock, f);

        io = list_entry(l, struct ide_drive_io, head);

        vmm_mutex_lock(&drive->lock);
        __ide_block_device_request(drive, io->rq, io->r);
        vmm_mutex_unlock(&drive->lock);

        vmm_free(io);
    }

    return VMM_OK;
}

static int ide_make_request(vmm_request_queue_t *rq, vmm_request_t *r)
{
    irq_flags_t          f;
    struct ide_drive_io *io;
    struct ide_drive    *drive;

    if (!r || !rq || !rq->private) {
        return VMM_EFAIL;
    }

    drive = rq->private;

    io    = vmm_zalloc(sizeof(struct ide_drive_io));

    if (!io) {
        return VMM_ENOMEM;
    }

    INIT_LIST_HEAD(&io->head);
    io->rq = rq;
    io->r  = r;

    vmm_spin_lock_irq_save(&drive->io_list_lock, f);
    list_add_tail(&io->head, &drive->io_list);
    vmm_spin_unlock_irq_restore(&drive->io_list_lock, f);

    vmm_completion_complete(&drive->io_avail);

    return VMM_OK;
}

static int ide_abort_request(vmm_request_queue_t *rq, vmm_request_t *r)
{
    bool                 found;
    irq_flags_t          f;
    double_list_t       *l;
    struct ide_drive_io *io;
    struct ide_drive    *drive;

    if (!r || !rq || !rq->private) {
        return VMM_EFAIL;
    }

    drive = rq->private;

    vmm_spin_lock_irq_save(&drive->io_list_lock, f);

    found = FALSE;
    list_for_each(l, &drive->io_list)
    {
        io = list_entry(l, struct ide_drive_io, head);

        if (io->r == r && io->rq == rq) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        list_del(&io->head);
        vmm_free(io);
    }

    vmm_spin_unlock_irq_restore(&drive->io_list_lock, f);

    return VMM_OK;
}

static vmm_irq_return_t handle_ata_interrupt(int irq_no, void *dev)
{
    struct ide_drive *drive = (struct ide_drive *)dev;

    vmm_completion_complete(&drive->dev_intr);

    return VMM_IRQ_HANDLED;
}

int ide_add_drive(struct ide_drive *drive)
{
    char name[32];

    if (!drive || drive->io_thread) {
        return VMM_EFAIL;
    }

    vmm_mutex_lock(&ide_drive_list_mutex);

    drive->io_thread = NULL;
    INIT_LIST_HEAD(&drive->link);
    INIT_LIST_HEAD(&drive->io_list);
    INIT_SPIN_LOCK(&drive->io_list_lock);
    INIT_COMPLETION(&drive->io_avail);
    INIT_COMPLETION(&drive->dev_intr);
    INIT_MUTEX(&drive->lock);

    if (__init_ide_drive(drive) != VMM_OK) {
        vmm_mutex_unlock(&ide_drive_list_mutex);
        vmm_printf("%s: IDE Block layer int failed\n", __func__);
        return VMM_EFAIL;
    }

    vmm_snprintf(name, 32, "%s", drive_names[drive->channel->id][drive->drive]);
    drive->io_thread = vmm_threads_create(name, ide_io_thread, drive, VMM_THREAD_DEF_PRIORITY, VMM_THREAD_DEF_TIME_SLICE);

    if (!drive->io_thread) {
        vmm_mutex_unlock(&ide_drive_list_mutex);
        return VMM_EFAIL;
    }

    if (drive->channel->id) {
        vmm_host_irq_register(SECONDARY_ATA_CHANNEL_IRQ, "ATA-15", handle_ata_interrupt, drive);
    } else {
        vmm_host_irq_register(PRIMARY_ATA_CHANNEL_IRQ, "ATA-14", handle_ata_interrupt, drive);
    }

    ide_drive_count++;
    list_add_tail(&drive->link, &ide_drive_list);

    vmm_mutex_unlock(&ide_drive_list_mutex);

    vmm_threads_start(drive->io_thread);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(ide_add_drive);

static int __init ide_core_init(void)
{
    /* Nothing to be done until ATA core registers a drive */
    return VMM_OK;
}

static void __exit ide_core_exit(void)
{
    /* Nothing to be done. */
}

VMM_DECLARE_MODULE2(MODULE_NAME, MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
