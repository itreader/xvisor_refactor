/**
 * Copyright (C) 2014 Anup Patel.
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
 * @file scsi_disk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SCSI disk library
 */

#include <libs/scsi_disk.h>
#include <libs/stringlib.h>
#include <vmm_cache.h>
#include <vmm_device_driver.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "SCSI Disk Library"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (SCSI_DISK_IPRIORITY)
#define MODULE_INIT      NULL
#define MODULE_EXIT      NULL

#undef _DEBUG
#ifdef _DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static int scsi_disk_rq_read(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private)
{
    void               *data;
    int                 rc, retry;
    uint64_t            lba, bcnt, blksz;
    unsigned short      blks;
    struct scsi_request srb;
    struct scsi_disk   *disk = private;

    bcnt                     = (uint64_t)r->bcnt;
    data                     = r->data;
    lba                      = (uint64_t)r->lba;
    blksz                    = disk->info.blksz;

    while (bcnt) {
        blks  = (disk->blks_per_xfer < bcnt) ? disk->blks_per_xfer : bcnt;

        retry = 3;
        rc    = VMM_OK;

        while (retry) {
            INIT_SCSI_REQUEST(&srb, disk->info.lun, data, blks * blksz);
            rc = scsi_read10(&srb, lba, blks, disk->tr, disk->tr_private);

            if (rc == VMM_OK) {
                break;
            }

            rc = scsi_request_sense(&srb, disk->tr, disk->tr_private);

            if (rc) {
                return rc;
            }

            if ((srb.sense_buf[2] == 0x02) && (srb.sense_buf[12] == 0x3a)) {
                return VMM_ERR_NODEV;
            }

            retry--;
        }

        if (!retry && rc != VMM_OK) {
            return rc;
        }

        lba += blks;
        bcnt -= blks;
        data += blks * blksz;
    }

    return VMM_OK;
}

static int scsi_disk_rq_write(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private)
{
    void               *data;
    int                 rc, retry;
    uint64_t            lba, bcnt, blksz;
    unsigned short      blks;
    struct scsi_request srb;
    struct scsi_disk   *disk = private;

    bcnt                     = (uint64_t)r->bcnt;
    data                     = r->data;
    lba                      = (uint64_t)r->lba;
    blksz                    = disk->info.blksz;

    while (bcnt) {
        blks  = (disk->blks_per_xfer < bcnt) ? disk->blks_per_xfer : bcnt;

        retry = 3;
        rc    = VMM_OK;

        while (retry) {
            INIT_SCSI_REQUEST(&srb, disk->info.lun, data, blks * blksz);
            rc = scsi_write10(&srb, lba, blks, disk->tr, disk->tr_private);

            if (rc == VMM_OK) {
                break;
            }

            rc = scsi_request_sense(&srb, disk->tr, disk->tr_private);

            if (rc) {
                return rc;
            }

            if ((srb.sense_buf[2] == 0x02) && (srb.sense_buf[12] == 0x3a)) {
                return VMM_ERR_NODEV;
            }

            retry--;
        }

        if (!retry && rc != VMM_OK) {
            return rc;
        }

        lba += blks;
        bcnt -= blks;
        data += blks * blksz;
    }

    return VMM_OK;
}

static void scsi_disk_rq_flush(vmm_block_request_queue_t *brq, void *private)
{
    /* Nothing to do here. */
}

static vmm_block_request_queue_ops_t scsi_rq_ops = {.read = scsi_disk_rq_read, .write = scsi_disk_rq_write, .flush = scsi_disk_rq_flush};

struct scsi_disk *scsi_create_disk(
    const char *name, uint32_t lun, uint32_t max_pending, unsigned short blks_per_xfer, vmm_device_t *dev, struct scsi_transport *tr,
    void *tr_private)
{
    int               err  = 0;
    struct scsi_disk *disk = NULL;

    if (!name || !max_pending || !blks_per_xfer || !tr || !tr->transport || !tr->reset) {
        return VMM_ERR_RR_PTR(VMM_ERR_INVALID);
    }

    /* Reset SCSI transport */
    err = scsi_reset(tr, tr_private);

    if (err) {
        return VMM_ERR_RR_PTR(err);
    }

    /* Alloc SCSI disk */
    disk = vmm_zalloc(sizeof(*disk));

    if (!disk) {
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    disk->blks_per_xfer = blks_per_xfer;
    disk->tr            = tr;
    disk->tr_private    = tr_private;

    /* Get SCSI info */
    err                 = scsi_get_info(&disk->info, lun, disk->tr, disk->tr_private);

    if (err) {
        vmm_free(disk);
        return VMM_ERR_RR_PTR(err);
    }

    /* Alloc block device instance */
    if (!(disk->block_device = vmm_block_device_alloc())) {
        vmm_free(disk);
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    /* Setup block device instance */
    strncpy(disk->block_device->name, name, sizeof(disk->block_device->name));
    vmm_snprintf(disk->block_device->desc, sizeof(disk->block_device->desc), "%s %s %s", disk->info.vendor, disk->info.product, disk->info.revision);
    disk->block_device->dev.parent = dev;
    disk->block_device->flags      = (disk->info.readonly) ? VMM_BLOCK_DEVICE_RDONLY : VMM_BLOCK_DEVICE_RW;
    disk->block_device->start_lba  = 0;
    disk->block_device->num_blocks = disk->info.capacity;
    disk->block_device->block_size = disk->info.blksz;

    /* Setup request queue for block device instance */
    disk->brq                      = vmm_block_request_queue_create(name, max_pending, FALSE, &scsi_rq_ops, disk);

    if (!disk->brq) {
        vmm_block_device_free(disk->block_device);
        vmm_free(disk);
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    disk->block_device->rq = vmm_block_request_queue_to_rq(disk->brq);

    /* Register block device instance */
    if ((err = vmm_block_device_register(disk->block_device))) {
        vmm_block_request_queue_destroy(disk->brq);
        vmm_block_device_free(disk->block_device);
        vmm_free(disk);
        return VMM_ERR_RR_PTR(err);
    }

    return disk;
}

VMM_ERR_XPORT_SYMBOL(scsi_create_disk);

int scsi_destroy_disk(struct scsi_disk *disk)
{
    if (!disk) {
        return VMM_ERR_INVALID;
    }

    vmm_block_device_unregister(disk->block_device);
    vmm_block_request_queue_destroy(disk->brq);
    vmm_block_device_free(disk->block_device);
    vmm_free(disk);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(scsi_destroy_disk);

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
