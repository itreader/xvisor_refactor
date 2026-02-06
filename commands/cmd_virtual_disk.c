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
 * @file cmd_virtual_disk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of virtual_disk command
 */

#include <libs/stringlib.h>
#include <vio/vmm_virtual_disk.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command virtual_disk"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_virtual_disk_init
#define MODULE_EXIT      cmd_virtual_disk_exit

static void cmd_virtual_disk_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   virtual_disk help\n");
    vmm_cdev_printf(cdev, "   virtual_disk list\n");
    vmm_cdev_printf(cdev, "   virtual_disk info <virtual_disk_name>\n");
    vmm_cdev_printf(cdev, "   virtual_disk detach <virtual_disk_name>\n");
    vmm_cdev_printf(cdev, "   virtual_disk attach <virtual_disk_name> <block_device_name>\n");
}

static int cmd_virtual_disk_list_iter(struct vmm_virtual_disk *virtual_disk, void *data)
{
    int                rc;
    char               bname[VMM_FIELD_NAME_SIZE];
    vmm_char_device_t *cdev = data;

    rc                      = vmm_virtual_disk_current_block_device(virtual_disk, bname, sizeof(bname));
    vmm_cdev_printf(
        cdev, " %-30s %-17d %-30s\n", vmm_virtual_disk_name(virtual_disk), vmm_virtual_disk_block_size(virtual_disk), (rc == VMM_OK) ? bname : "---");

    return VMM_OK;
}

static void cmd_virtual_disk_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-30s %-17s %-30s\n", "Name", "Block Size", "Attached Block Device");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_virtual_disk_iterate(NULL, cdev, cmd_virtual_disk_list_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_virtual_disk_info(vmm_char_device_t *cdev, const char *virtual_disk_name)
{
    struct vmm_virtual_disk *virtual_disk = vmm_virtual_disk_find(virtual_disk_name);

    if (!virtual_disk) {
        vmm_cdev_printf(cdev, "Failed to find virtual disk\n");
        return VMM_ENODEV;
    }

    vmm_cdev_printf(
        cdev,
        "Name        : %s\n"
        "Block Size  : %" PRIu32 "\n"
        "Block Factor: %" PRIu32 "\n"
        "Capacity    : %" PRIu64 "\n"
        "Block Device: %s\n",
        vmm_virtual_disk_name(virtual_disk), vmm_virtual_disk_block_size(virtual_disk), virtual_disk->block_factor,
        vmm_virtual_disk_capacity(virtual_disk), virtual_disk->blk ? virtual_disk->blk->name : "NONE");

    return VMM_OK;
}

static int cmd_virtual_disk_detach(vmm_char_device_t *cdev, const char *virtual_disk_name)
{
    struct vmm_virtual_disk *virtual_disk = vmm_virtual_disk_find(virtual_disk_name);

    if (!virtual_disk) {
        vmm_cdev_printf(cdev, "Failed to find virtual disk\n");
        return VMM_ENODEV;
    }

    vmm_virtual_disk_detach_block_device(virtual_disk);

    return VMM_OK;
}

static int cmd_virtual_disk_attach(vmm_char_device_t *cdev, const char *virtual_disk_name, const char *bdev_name)
{
    struct vmm_virtual_disk *virtual_disk = vmm_virtual_disk_find(virtual_disk_name);

    if (!virtual_disk) {
        vmm_cdev_printf(cdev, "Failed to find virtual disk\n");
        return VMM_ENODEV;
    }

    vmm_virtual_disk_attach_block_device(virtual_disk, bdev_name);

    return VMM_OK;
}

static int cmd_virtual_disk_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_virtual_disk_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_virtual_disk_list(cdev);
            return VMM_OK;
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "detach") == 0) {
            return cmd_virtual_disk_detach(cdev, argv[2]);
        } else if (strcmp(argv[1], "info") == 0) {
            return cmd_virtual_disk_info(cdev, argv[2]);
        }
    } else if (argc == 4) {
        if (strcmp(argv[1], "attach") == 0) {
            return cmd_virtual_disk_attach(cdev, argv[2], argv[3]);
        }
    }

    cmd_virtual_disk_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_virtual_disk = {
    .name  = "virtual_disk",
    .desc  = "virtual disk commands",
    .usage = cmd_virtual_disk_usage,
    .exec  = cmd_virtual_disk_exec,
};

static int __init cmd_virtual_disk_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_virtual_disk);
}

static void __exit cmd_virtual_disk_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_virtual_disk);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
