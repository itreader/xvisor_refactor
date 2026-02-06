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
 * @file mtdchar.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief A very simple version of MTD character device part.
 */

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <vmm_char_device.h>
#include <vmm_stdio.h>
#include "mtdcore.h"

#define MTD_IOCTL_CMD_ERASE 0x1

static void mtd_char_device_erase_cb(__maybe_unused struct erase_info *info)
{
    vmm_completion_t *compl = (vmm_completion_t *)info->private;

    complete(compl );
}

int mtd_char_device_ioctl(vmm_char_device_t *cdev, int cmd, void *arg)
{
    struct mtd_info  *mtd = cdev->private;
    struct erase_info info;
    size_t            off = 0;
    vmm_completion_t compl ;
    uint64_t timeout = 100000;

    switch (cmd) {
        case MTD_IOCTL_CMD_ERASE:
            INIT_COMPLETION(&compl );
            info.mtd      = mtd;
            info.addr     = (uint32_t)arg;
            info.len      = mtd->erase_size;
            info.callback = mtd_char_device_erase_cb;
            info.private  = (uint64_t) & compl ;

            if (mtd_erase(mtd, &info)) {
                dev_err(&cdev->dev, "Erasing at 0x%08X failed\n", off);
                return VMM_EFAIL;
            }

            vmm_completion_wait_timeout(&compl, &timeout);
            break;

        default:
            dev_err(&cdev->dev, "Unknown command 0x%X\n", cmd);
            return VMM_EFAIL;
    }

    return VMM_OK;
}

uint32_t mtd_char_device_read(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool sleep)
{
    struct mtd_info *mtd    = cdev->private;
    uint32_t         retlen = 0;

    if (mtd_read(mtd, *off, len, &retlen, dest)) {
        dev_err(&cdev->dev, "Writing at 0x%p failed\n", off);
        return VMM_EFAIL;
    }

    *off += retlen;

    return VMM_OK;
}

uint32_t mtd_char_device_write(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool sleep)
{
    struct mtd_info *mtd    = cdev->private;
    uint32_t         retlen = 0;
    uint32_t         block  = 0;

    block                   = *off & ~mtd->erase_size_mask;

    if (mtd_block_isbad(mtd, block)) {
        dev_err(&cdev->dev, "Block at %" PRIX32 " failed\n", block);
        return VMM_EFAIL;
    }

    if (mtd_write(mtd, *off, len, &retlen, src)) {
        dev_err(&cdev->dev, "Writing at 0x%p failed\n", off);
        return VMM_EFAIL;
    }

    *off += retlen;

    return VMM_OK;
}

void mtdchar_add(struct mtd_info *mtd)
{
    vmm_char_device_t *cdev = NULL;

    if (NULL == (cdev = vmm_zalloc(sizeof(struct vmm_char_device)))) {
        dev_err(
            &mtd->dev, "Failed to allocate MTD character "
                       "device\n");
        return;
    }

    strncpy(cdev->name, mtd->name, sizeof(cdev->name));
    cdev->ioctl   = mtd_char_device_ioctl;
    cdev->read    = mtd_char_device_read;
    cdev->write   = mtd_char_device_write;
    cdev->private = mtd;

    if (VMM_OK != vmm_char_device_register(cdev)) {
        vmm_free(cdev);
        dev_err(&mtd->dev, "Failed to register MTD char device\n");
    }
}

void mtdchar_remove(struct mtd_info *mtd)
{
    vmm_char_device_t *cdev = NULL;

    if (NULL == (cdev = vmm_char_device_find(mtd->name))) {
        return;
    }

    vmm_char_device_unregister(cdev);
}

static struct mtd_notifier mtdchar_notify = {
    .add    = mtdchar_add,
    .remove = mtdchar_remove,
};

int __init init_mtdchar(void)
{
    register_mtd_user(&mtdchar_notify);

    return VMM_OK;
}

void __exit cleanup_mtdchar(void)
{
    unregister_mtd_user(&mtdchar_notify);
}
