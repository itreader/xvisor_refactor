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
 * @file cmd_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of input command
 */

#include <drv/input.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command input"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_input_init
#define MODULE_EXIT      cmd_input_exit

static void cmd_input_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   input help\n");
    vmm_cdev_printf(cdev, "   input devices\n");
    vmm_cdev_printf(cdev, "   input handlers\n");
}

static int cmd_input_deviceices_iter(input_device_t *idev, void *data)
{
    char               id[27];
    vmm_char_device_t *cdev = data;

    vmm_snprintf(id, sizeof(id), "0x%02x:0x%02x:0x%02x:0x%04x", idev->id.bustype, idev->id.vendor, idev->id.product, idev->id.version);
    vmm_cdev_printf(cdev, " %-18s %-32s %-27s\n", idev->phys, idev->name, id);

    return VMM_OK;
}

static void cmd_input_deviceices(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-18s %-32s %-27s\n", "Phys", "Name", "Bus:Vendor:Product:Version");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    input_iterate_device(NULL, cdev, cmd_input_deviceices_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static void cmd_input_handlers(vmm_char_device_t *cdev)
{
    int                   num, count;
    struct input_handler *ihnd;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-10s %-67s\n", "Num", "Name");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    count = input_count_handler();

    for (num = 0; num < count; num++) {
        ihnd = input_get_handler(num);
        vmm_cdev_printf(cdev, " %-10d %-67s\n", num, ihnd->name);
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_input_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_input_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "devices") == 0) {
            cmd_input_deviceices(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "handlers") == 0) {
            cmd_input_handlers(cdev);
            return VMM_OK;
        }
    }

    cmd_input_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_input = {
    .name  = "input",
    .desc  = "input device commands",
    .usage = cmd_input_usage,
    .exec  = cmd_input_exec,
};

static int __init cmd_input_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_input);
}

static void __exit cmd_input_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_input);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
