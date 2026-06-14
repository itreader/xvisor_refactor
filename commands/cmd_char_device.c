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
 * @file cmd_char_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of char_device command
 */

#include <libs/stringlib.h>
#include <vmm_char_device.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command char_device"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_char_device_init
#define MODULE_EXIT      cmd_char_device_exit

static void cmd_char_device_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   char_device help\n");
    vmm_cdev_printf(cdev, "   char_device list\n");
}

static int cmd_char_device_list_iter(vmm_char_device_t *cd, void *data)
{
    int                rc;
    char               path[256];
    vmm_char_device_t *cdev = data;

    if (cd->dev.parent && cd->dev.parent->of_node) {
        rc = vmm_device_tree_getpath(path, sizeof(path), cd->dev.parent->of_node);

        if (rc) {
            vmm_snprintf(path, sizeof(path), "----- (error %d)", rc);
        }
    } else {
        strcpy(path, "-----");
    }

    vmm_cdev_printf(cdev, " %-24s %-53s\n", cd->name, path);

    return VMM_OK;
}

static void cmd_char_device_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-24s %-53s\n", "Name", "Device Path");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_char_device_iterate(NULL, cdev, cmd_char_device_list_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_char_device_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_char_device_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_char_device_list(cdev);
            return VMM_OK;
        }
    }

    if (argc < 3) {
        cmd_char_device_usage(cdev);
        return VMM_ERR_FAIL;
    }

    return VMM_OK;
}

static vmm_command_t cmd_char_device = {
    .name  = "char_device",
    .desc  = "character device commands",
    .usage = cmd_char_device_usage,
    .exec  = cmd_char_device_exec,
};

static int __init cmd_char_device_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_char_device);
}

static void __exit cmd_char_device_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_char_device);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
