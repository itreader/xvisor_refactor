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
 * @file cmd_stdio.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of stdio command
 */

#include <libs/stringlib.h>
#include <vmm_char_device.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command stdio"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_stdio_init
#define MODULE_EXIT      cmd_stdio_exit

static void cmd_stdio_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   stdio help\n");
    vmm_cdev_printf(cdev, "   stdio device\n");
    vmm_cdev_printf(cdev, "   stdio change_device <char_device_name>\n");
    vmm_cdev_printf(cdev, "   stdio loglevel\n");
    vmm_cdev_printf(cdev, "   stdio change_loglevel <loglevel>\n");
}

static int cmd_stdio_device(vmm_char_device_t *cdev)
{
    vmm_char_device_t *cd;
    cd = vmm_stdio_device();

    if (!cd) {
        vmm_cdev_printf(cdev, "Current Device : ---\n");
    } else {
        vmm_cdev_printf(cdev, "Current Device : %s\n", cd->name);
    }

    return VMM_OK;
}

static int cmd_stdio_change_device(vmm_char_device_t *cdev, char *char_device_name)
{
    int                ret;
    vmm_char_device_t *cd = vmm_char_device_find(char_device_name);

    if (cd) {
        vmm_cdev_printf(cdev, "New I/O Device: %s\n", cd->name);

        if ((ret = vmm_stdio_change_device(cd))) {
            vmm_cdev_printf(cdev, "Failed to change device %s\n", cd->name);
            return ret;
        }
    } else {
        vmm_cdev_printf(cdev, "Device %s not found\n", char_device_name);
        return VMM_EFAIL;
    }

    return VMM_OK;
}

static int cmd_stdio_loglevel(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Current Log Level : %ld\n", vmm_stdio_loglevel());

    return VMM_OK;
}

static int cmd_stdio_change_loglevel(vmm_char_device_t *cdev, int loglevel)
{
    vmm_stdio_change_loglevel(loglevel);

    return VMM_OK;
}

static int cmd_stdio_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_stdio_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "device") == 0) {
            return cmd_stdio_device(cdev);
        } else if (strcmp(argv[1], "loglevel") == 0) {
            return cmd_stdio_loglevel(cdev);
        }
    }

    if (argc < 3) {
        cmd_stdio_usage(cdev);
        return VMM_EFAIL;
    }

    if (strcmp(argv[1], "change_device") == 0) {
        return cmd_stdio_change_device(cdev, argv[2]);
    } else if (strcmp(argv[1], "change_loglevel") == 0) {
        long loglevel = strtol(argv[2], NULL, 10);
        return cmd_stdio_change_loglevel(cdev, loglevel);
    } else {
        cmd_stdio_usage(cdev);
        return VMM_EFAIL;
    }

    return VMM_OK;
}

static vmm_command_t cmd_stdio = {
    .name  = "stdio",
    .desc  = "standard I/O configuration",
    .usage = cmd_stdio_usage,
    .exec  = cmd_stdio_exec,
};

static int __init cmd_stdio_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_stdio);
}

static void __exit cmd_stdio_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_stdio);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
