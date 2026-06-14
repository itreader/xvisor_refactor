/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file cmd_share_memory.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of share_memory command
 */

#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_modules.h>
#include <vmm_share_memory.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command share_memory"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_share_memory_init
#define MODULE_EXIT      cmd_share_memory_exit

static void cmd_share_memory_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   share_memory help\n");
    vmm_cdev_printf(cdev, "   share_memory list\n");
    vmm_cdev_printf(cdev, "   share_memory create <name> <phys_size> [<align_order>]\n");
    vmm_cdev_printf(cdev, "   share_memory destroy <name>\n");
}

static int cmd_share_memory_list_iter(vmm_share_memory_t *share_memory, void *private)
{
    vmm_char_device_t *cdev = private;
    char               addr[32], size[32], aorder[32], rcount[32];

    vmm_snprintf(addr, sizeof(addr), "0x%" PRIPADDR, vmm_share_memory_get_addr(share_memory));
    vmm_snprintf(size, sizeof(size), "0x%" PRIPADDR, vmm_share_memory_get_size(share_memory));
    vmm_snprintf(aorder, sizeof(aorder), "%d", vmm_share_memory_get_align_order(share_memory));
    vmm_snprintf(rcount, sizeof(rcount), "%ld", vmm_share_memory_get_ref_count(share_memory));
    vmm_cdev_printf(cdev, "%-16s %-18s %-18s %-12s %-12s\n", vmm_share_memory_get_name(share_memory), addr, size, aorder, rcount);

    return VMM_OK;
}

static int cmd_share_memory_list(vmm_char_device_t *cdev)
{
    int rc;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, "%-16s %-18s %-18s %-12s %-12s\n", "Name", "Physical Address", "Physical Size", "Align Order", "Ref Count");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    rc = vmm_share_memory_iterate(cmd_share_memory_list_iter, cdev);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");

    return rc;
}

static int cmd_share_memory_create(vmm_char_device_t *cdev, const char *name, physical_size_t size, uint32_t align_order)
{
    vmm_share_memory_t *share_memory;

    share_memory = vmm_share_memory_create(name, size, align_order, NULL);

    if (VMM_IS_ERR_OR_NULL(share_memory)) {
        vmm_cdev_printf(cdev, "Failed to create %s shared memory\n", name);
        return VMM_ERR_FAIL;
    }

    vmm_cdev_printf(cdev, "Created %s shared memory\n", name);

    return VMM_OK;
}

static int cmd_share_memory_destroy(vmm_char_device_t *cdev, const char *name)
{
    vmm_share_memory_t *share_memory = vmm_share_memory_find_byname(name);

    if (!share_memory) {
        vmm_cdev_printf(cdev, "Failed to find %s shared memory\n", name);
        return VMM_ERR_NOTAVAIL;
    }

    vmm_share_memory_dref(share_memory);
    vmm_share_memory_destroy(share_memory);

    vmm_cdev_printf(cdev, "Destroyed %s shared memory\n", name);

    return VMM_OK;
}

static int cmd_share_memory_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    physical_size_t size;
    uint32_t        align_order = VMM_PAGE_SHIFT;

    if (argc <= 1) {
        goto fail;
    }

    if (strcmp(argv[1], "help") == 0) {
        cmd_share_memory_usage(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "list") == 0) && (argc == 2)) {
        return cmd_share_memory_list(cdev);
    } else if ((strcmp(argv[1], "create") == 0) && ((argc == 5) || (argc == 4))) {
        size = (physical_size_t)strtoull(argv[3], NULL, 0);

        if (argc == 5) {
            align_order = atoi(argv[4]);
        }

        return cmd_share_memory_create(cdev, argv[2], size, align_order);
    } else if ((strcmp(argv[1], "destroy") == 0) && (argc == 3)) {
        return cmd_share_memory_destroy(cdev, argv[2]);
    }

fail:
    cmd_share_memory_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_share_memory = {
    .name  = "share_memory",
    .desc  = "shared memory commands",
    .usage = cmd_share_memory_usage,
    .exec  = cmd_share_memory_exec,
};

static int __init cmd_share_memory_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_share_memory);
}

static void __exit cmd_share_memory_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_share_memory);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
