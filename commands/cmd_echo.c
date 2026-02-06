/**
 * Copyright (c) 2015 Himanshu Chauhan
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
 * @file cmd_echo.c
 * @author Himanshu Chauhan <hchauhan@xvisor-x86.org>
 * @brief Implementation of echo command
 */

#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command echo"
#define MODULE_AUTHOR    "Himanshu Chauhan"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_echo_init
#define MODULE_EXIT      cmd_echo_exit

static void cmd_echo_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage: ");
    vmm_cdev_printf(cdev, "   echo <message>\n");
}

static int cmd_echo_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    int print_new_line = 1;
    int i;

    if (argc < 2) {
        cmd_echo_usage(cdev);
        return VMM_EFAIL;
    }

    if (!strcmp(argv[1], "-e")) {
        print_new_line = 0;
    }

    for (i = 1; i < argc; i++) {
        vmm_cdev_printf(cdev, "%s ", argv[i]);
    }

    if (print_new_line) {
        vmm_cdev_printf(cdev, "\n");
    }

    return VMM_OK;
}

static vmm_command_t cmd_echo = {
    .name  = "echo",
    .desc  = "Echo given message on standard output",
    .usage = cmd_echo_usage,
    .exec  = cmd_echo_exec,
};

static int __init cmd_echo_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_echo);
}

static void __exit cmd_echo_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_echo);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
