/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file cmd_block_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of block_device command
 */

#include <block/vmm_block_device.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command block_device"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_BLOCK_DEVICE_CLASS_IPRIORITY + 1)
#define MODULE_INIT      cmd_block_device_init
#define MODULE_EXIT      cmd_block_device_exit

static void cmd_block_device_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   block_device help\n");
    vmm_cdev_printf(cdev, "   block_device list\n");
    vmm_cdev_printf(cdev, "   block_device info <name>\n");
    vmm_cdev_printf(cdev, "   block_device dump8 <name> [length] [offset]\n");
}

static int cmd_block_device_info(vmm_char_device_t *cdev, vmm_block_device_t *block_device)
{
    vmm_cdev_printf(cdev, "Name       : %s\n", block_device->name);
    vmm_cdev_printf(cdev, "Parent     : %s\n", (block_device->parent) ? block_device->parent->name : "---");
    vmm_cdev_printf(cdev, "Description: %s\n", block_device->desc);
    vmm_cdev_printf(cdev, "Access     : %s\n", (block_device->flags & VMM_BLOCK_DEVICE_RW) ? "Read-Write" : "Read-Only");
    vmm_cdev_printf(cdev, "Start LBA  : %" PRIu64 "\n", block_device->start_lba);
    vmm_cdev_printf(cdev, "Block Size : %" PRIu32 "\n", block_device->block_size);
    vmm_cdev_printf(cdev, "Block Count: %" PRIu64 "\n", block_device->num_blocks);

    return VMM_OK;
}

static int cmd_block_device_list_iter(vmm_block_device_t *block_device, void *data)
{
    vmm_char_device_t *cdev = data;

    vmm_cdev_printf(
        cdev, " %-16s %-16s %-16" PRIu64 " %-11" PRIu32 " %-16" PRIu64 "\n", block_device->name,
        (block_device->parent) ? block_device->parent->name : "---", block_device->start_lba, block_device->block_size, block_device->num_blocks);

    return VMM_OK;
}

static void cmd_block_device_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-16s %-16s %-16s %-11s %-16s\n", "Name", "Parent", "Start LBA", "Blk Sz", "Blk Cnt");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_block_device_iterate(NULL, cdev, cmd_block_device_list_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_block_device_dump8(vmm_char_device_t *cdev, vmm_block_device_t *block_device, int argc, char *argv[])
{
#define BLOCK_DEVICE_DUMP_BUF_SZ 128
    uint8_t  data[BLOCK_DEVICE_DUMP_BUF_SZ];
    uint64_t off = 0, count = block_device->block_size;
    uint64_t i, pos, size, rdsz;

    if (argc >= 1) {
        count = strtoul(argv[0], NULL, 10);
    }

    if (!count) {
        vmm_cdev_printf(cdev, "Error, 0 data to read\n");
        return VMM_EINVALID;
    }

    if (argc >= 2) {
        if (argv[1][0] && (argv[1][1] == 'x')) {
            off = strtoull(argv[1], NULL, 16);
        } else {
            off = strtoull(argv[1], NULL, 10);
        }
    }

    pos = 0;

    while (count) {
        size = (count < BLOCK_DEVICE_DUMP_BUF_SZ) ? count : BLOCK_DEVICE_DUMP_BUF_SZ;

        rdsz = vmm_block_device_rw(block_device, VMM_REQUEST_READ, data, off, size);

        if (rdsz != size) {
            vmm_cdev_printf(cdev, "Error, read %" PRId64 " byte(s)\n", rdsz);
            break;
        }

        for (i = 0; i < rdsz; i++) {
            if (pos % 8 == 0) {
                vmm_cdev_printf(cdev, "%" PRIx64 ":", i + off);
            }

            vmm_cdev_printf(cdev, " 0x%02x", data[i]);

            if (pos % 8 == 7) {
                vmm_cdev_printf(cdev, "\n");
            }

            pos++;
        }

        count -= size;
        off += size;
    }

    if (pos % 8 != 0) {
        vmm_cdev_printf(cdev, "\n");
    }

    return VMM_OK;
}

static int cmd_block_device_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    vmm_block_device_t *block_device = NULL;

    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_block_device_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_block_device_list(cdev);
            return VMM_OK;
        }
    } else if (argc >= 3) {
        block_device = vmm_block_device_find(argv[2]);

        if (!block_device) {
            vmm_cdev_printf(cdev, "Error: cannot find block_device %s\n", argv[2]);
            return VMM_EINVALID;
        }

        if (strcmp(argv[1], "info") == 0) {
            return cmd_block_device_info(cdev, block_device);
        } else if (strcmp(argv[1], "dump8") == 0) {
            return cmd_block_device_dump8(cdev, block_device, argc - 3, argv + 3);
        }
    }

    cmd_block_device_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_block_device = {
    .name  = "block_device",
    .desc  = "block device commands",
    .usage = cmd_block_device_usage,
    .exec  = cmd_block_device_exec,
};

static int __init cmd_block_device_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_block_device);
}

static void __exit cmd_block_device_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_block_device);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
