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
 * @file cmd_virtual_display.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of virtual_display command
 */

#include <libs/stringlib.h>
#include <vio/vmm_virtual_display.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command virtual_display"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_virtual_display_init
#define MODULE_EXIT      cmd_virtual_display_exit

static void cmd_virtual_display_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   virtual_display help\n");
    vmm_cdev_printf(cdev, "   virtual_display list\n");
}

static int cmd_virtual_display_list_iter(struct vmm_virtual_display *vdis, void *data)
{
    vmm_char_device_t *cdev = data;

    vmm_cdev_printf(cdev, " %-39s\n", vdis->name);

    return VMM_OK;
}

static void cmd_virtual_display_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-39s\n", "Name");
    vmm_cdev_printf(cdev, "----------------------------------------\n");
    vmm_virtual_display_iterate(NULL, cdev, cmd_virtual_display_list_iter);
    vmm_cdev_printf(cdev, "----------------------------------------\n");
}

static int cmd_virtual_display_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_virtual_display_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_virtual_display_list(cdev);
            return VMM_OK;
        }
    }

    cmd_virtual_display_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_virtual_display = {
    .name  = "virtual_display",
    .desc  = "virtual display commands",
    .usage = cmd_virtual_display_usage,
    .exec  = cmd_virtual_display_exec,
};

static int __init cmd_virtual_display_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_virtual_display);
}

static void __exit cmd_virtual_display_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_virtual_display);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
