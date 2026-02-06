/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @file mtdblock.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief A very simple version of MTD block device part.
 */

#include <block/vmm_block_device.h>
#include <block/vmm_block_request_queue.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <vmm_stdio.h>
#include "mtdcore.h"

static void mtd_block_device_erase_callback(struct erase_info *info)
{
    /* Nothing to do here. */
}

static int mtd_block_device_erase_write(vmm_request_t *r, physical_addr_t off, physical_size_t len, struct mtd_info *mtd)
{
    struct erase_info info;
    uint32_t          retlen = 0;

    info.mtd                 = mtd;
    info.addr                = off;
    info.len                 = len;
    info.callback            = mtd_block_device_erase_callback;

    if (mtd_erase(mtd, &info)) {
        dev_err(&r->block_device->dev, "Erasing at 0x%08X failed\n", off);
        return VMM_EIO;
    }

    if (mtd_write(mtd, off, len, &retlen, r->data)) {
        dev_err(&r->block_device->dev, "Writing at 0x%08X failed\n", off);
        return VMM_EIO;
    }

    if (retlen < len) {
        dev_warn(
            &r->block_device->dev,
            "Only 0x%X/0x%X bytes have been "
            "written at 0x%08X\n",
            retlen, len, off);
        return VMM_EIO;
    }

    return VMM_OK;
}

int mtd_block_device_read(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private)
{
    struct mtd_info *mtd    = private;
    uint32_t         retlen = 0;

    physical_addr_t off     = r->lba << mtd->erase_size_shift;
    physical_size_t len     = r->bcnt << mtd->erase_size_shift;

    mtd_read(mtd, off, len, &retlen, r->data);

    if (retlen < len) {
        return VMM_EIO;
    }

    return VMM_OK;
}

int mtd_block_device_write(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private)
{
    struct mtd_info *mtd = private;
    physical_addr_t  off = r->lba << mtd->erase_size_shift;
    physical_size_t  len = r->bcnt << mtd->erase_size_shift;

    while (mtd_block_isbad(mtd, off)) {
        vmm_printf("%s: block at 0x%X is bad, skipping...\n", __func__, off);
        off += mtd->erase_size;
    }

    return mtd_block_device_erase_write(r, off, len, mtd);
}

void mtd_block_device_flush(vmm_block_request_queue_t *brq, void *private)
{
    /* Nothing to do here. */
}

static vmm_block_request_queue_ops_t mtd_block_device_rq_ops = {
    .read = mtd_block_device_read, .write = mtd_block_device_write, .flush = mtd_block_device_flush};

void mtdblock_add(struct mtd_info *mtd)
{
    int                        err          = 0;
    vmm_block_device_t        *block_device = NULL;
    vmm_block_request_queue_t *brq          = NULL;

    if (NULL == (block_device = vmm_block_device_alloc())) {
        dev_err(&mtd->dev, "Failed to allocate MTD block device\n");
        return;
    }

    /* Setup block device instance */
    strncpy(block_device->name, mtd->name, sizeof(block_device->name));
    strncpy(block_device->desc, "MTD m25p80 NOR flash block device", VMM_FIELD_DESC_SIZE);
    block_device->dev.private = mtd;
    block_device->flags       = VMM_BLOCK_DEVICE_RW;
    block_device->start_lba   = 0;
    block_device->num_blocks  = mtd->size >> mtd->erase_size_shift;
    block_device->block_size  = mtd->erase_size;

    /* Setup request queue for block device instance */
    brq                       = vmm_block_request_queue_create(mtd->name, 128, FALSE, &mtd_block_device_rq_ops, mtd);

    if (!brq) {
        vmm_block_device_free(block_device);
        return;
    }

    block_device->rq = vmm_block_request_queue_to_rq(brq);

    /* Register block device instance */
    if (VMM_OK != (err = vmm_block_device_register(block_device))) {
        vmm_block_request_queue_destroy(brq);
        vmm_block_device_free(block_device);
        dev_err(&mtd->dev, "Failed to register MTD block device\n");
    }
}

void mtdblock_remove(struct mtd_info *mtd)
{
    vmm_block_device_t        *block_device = NULL;
    vmm_block_request_queue_t *brq          = NULL;

    if (NULL == (block_device = vmm_block_device_find(mtd->name))) {
        return;
    }

    brq = vmm_block_request_queue_from_rq(block_device->rq);

    vmm_block_device_unregister(block_device);

    if (brq) {
        vmm_block_request_queue_destroy(brq);
    }

    vmm_block_device_free(block_device);
}

static struct mtd_notifier mtdblock_notify = {
    .add    = mtdblock_add,
    .remove = mtdblock_remove,
};

int __init init_mtdblock(void)
{
    register_mtd_user(&mtdblock_notify);

    return VMM_OK;
}

void __exit cleanup_mtdblock(void)
{
    unregister_mtd_user(&mtdblock_notify);
}

VMM_DECLARE_MODULE("MTD Core", "Jimmy Durand Wesolowski", "GPL", MTD_IPRIORITY + 1, init_mtdblock, cleanup_mtdblock);
