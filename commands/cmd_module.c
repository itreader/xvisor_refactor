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
 * @file cmd_module.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of module command
 */

#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command module"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_module_init
#define MODULE_EXIT      cmd_module_exit

static void cmd_module_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   module help\n");
    vmm_cdev_printf(cdev, "   module list\n");
    vmm_cdev_printf(cdev, "   module info <index>\n");
    vmm_cdev_printf(cdev, "   module unload <index>\n");
}

static void cmd_module_list(vmm_char_device_t *cdev)
{
    int           num, count;
    vmm_module_t *mod;
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-5s %-25s %-25s %-10s %-11s\n", "Num", "Name", "Author", "License", "Type");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    count = vmm_modules_count();

    for (num = 0; num < count; num++) {
        mod = vmm_modules_getmodule(num);
        vmm_cdev_printf(
            cdev, " %-5d %-25s %-25s %-10s %-11s\n", num, mod->name, mod->author, mod->license, vmm_modules_isbuiltin(mod) ? "built-in" : "loadable");
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, "Total %d modules\n", count);
}

static int cmd_module_info(vmm_char_device_t *cdev, uint32_t index)
{
    vmm_module_t *mod;

    mod = vmm_modules_getmodule(index);

    if (!mod) {
        return VMM_EFAIL;
    }

    vmm_cdev_printf(cdev, "Name:        %s\n", mod->name);
    vmm_cdev_printf(cdev, "Description: %s\n", mod->desc);
    vmm_cdev_printf(cdev, "Author:      %s\n", mod->author);
    vmm_cdev_printf(cdev, "License:     %s\n", mod->license);
    vmm_cdev_printf(cdev, "iPriority:   %d\n", mod->ipriority);
    vmm_cdev_printf(cdev, "Type:        %s\n", vmm_modules_isbuiltin(mod) ? "built-in" : "loadable");

    return VMM_OK;
}

static int cmd_module_unload(vmm_char_device_t *cdev, uint32_t index)
{
    int           rc = VMM_OK;
    vmm_module_t *mod;

    mod = vmm_modules_getmodule(index);

    if (!mod) {
        return VMM_EFAIL;
    }

    if (vmm_modules_isbuiltin(mod)) {
        vmm_cdev_printf(cdev, "Can't unload built-in module\n");
        return VMM_EFAIL;
    }

    if ((rc = vmm_modules_unload(mod))) {
        vmm_cdev_printf(cdev, "Failed to unload module (error %d)\n", rc);
    } else {
        vmm_cdev_printf(cdev, "Unloaded module succesfully\n");
    }

    return rc;
}

static int cmd_module_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    int index;

    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_module_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_module_list(cdev);
            return VMM_OK;
        }
    }

    if (argc < 3) {
        cmd_module_usage(cdev);
        return VMM_EFAIL;
    }

    if (strcmp(argv[1], "info") == 0) {
        index = atoi(argv[2]);
        return cmd_module_info(cdev, index);
    } else if (strcmp(argv[1], "unload") == 0) {
        index = atoi(argv[2]);
        return cmd_module_unload(cdev, index);
    } else {
        cmd_module_usage(cdev);
        return VMM_EFAIL;
    }

    return VMM_OK;
}

static vmm_command_t cmd_module = {
    .name  = "module",
    .desc  = "module related commands",
    .usage = cmd_module_usage,
    .exec  = cmd_module_exec,
};

static int __init cmd_module_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_module);
}

static void __exit cmd_module_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_module);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
