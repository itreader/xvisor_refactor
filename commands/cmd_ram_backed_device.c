/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cmd_ram_backed_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of ram_backed_device command
 */

#include <drv/ram_backed_device.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command ram_backed_device"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_ram_backed_device_init
#define MODULE_EXIT      cmd_ram_backed_device_exit

static void cmd_ram_backed_device_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   ram_backed_device help\n");
    vmm_cdev_printf(cdev, "   ram_backed_device list\n");
    vmm_cdev_printf(cdev, "   ram_backed_device create <name> <phys_addr> <phys_size>\n");
    vmm_cdev_printf(cdev, "   ram_backed_device destroy <name>\n");
}

static int cmd_ram_backed_device_list(vmm_char_device_t *cdev)
{
    int                       num, count;
    char                      addr[32], size[32];
    struct ram_backed_device *d;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-32s %-22s %-22s\n", "Name", "Physical Address", "Physical Size");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    count = ram_backed_device_count();

    for (num = 0; num < count; num++) {
        d = ram_backed_device_get(num);
        vmm_snprintf(addr, sizeof(addr), "0x%" PRIPADDR, d->addr);
        vmm_snprintf(size, sizeof(size), "0x%" PRIPADDR, d->size);
        vmm_cdev_printf(cdev, " %-32s %-22s %-22s\n", d->block_device->name, addr, size);
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");

    return VMM_OK;
}

static int cmd_ram_backed_device_create(vmm_char_device_t *cdev, const char *name, physical_addr_t addr, physical_size_t size)
{
    struct ram_backed_device *d;

    d = ram_backed_device_create(name, addr, size, false);

    if (!d) {
        vmm_cdev_printf(cdev, "Failed to create %s RBD instance\n", name);
        return VMM_ERR_FAIL;
    }

    vmm_cdev_printf(cdev, "Created %s RBD instance\n", name);

    return VMM_OK;
}

static int cmd_ram_backed_device_destroy(vmm_char_device_t *cdev, const char *name)
{
    struct ram_backed_device *d = ram_backed_device_find(name);

    if (!d) {
        vmm_cdev_printf(cdev, "Failed to find %s RBD instance\n", name);
        return VMM_ERR_NOTAVAIL;
    }

    ram_backed_device_destroy(d);

    vmm_cdev_printf(cdev, "Destroyed %s RBD instance\n", name);

    return VMM_OK;
}

static int cmd_ram_backed_device_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    physical_addr_t addr;
    physical_size_t size;

    if (argc <= 1) {
        goto fail;
    }

    if (strcmp(argv[1], "help") == 0) {
        cmd_ram_backed_device_usage(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "list") == 0) && (argc == 2)) {
        return cmd_ram_backed_device_list(cdev);
    } else if ((strcmp(argv[1], "create") == 0) && (argc == 5)) {
        addr = (physical_addr_t)strtoull(argv[3], NULL, 0);
        size = (physical_size_t)strtoull(argv[4], NULL, 0);
        return cmd_ram_backed_device_create(cdev, argv[2], addr, size);
    } else if ((strcmp(argv[1], "destroy") == 0) && (argc == 3)) {
        return cmd_ram_backed_device_destroy(cdev, argv[2]);
    }

fail:
    cmd_ram_backed_device_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_ram_backed_device = {
    .name  = "ram_backed_device",
    .desc  = "ram backed block device commands",
    .usage = cmd_ram_backed_device_usage,
    .exec  = cmd_ram_backed_device_exec,
};

static int __init cmd_ram_backed_device_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_ram_backed_device);
}

static void __exit cmd_ram_backed_device_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_ram_backed_device);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
