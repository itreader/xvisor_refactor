/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cmd_clock.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of clk command
 */

#include <drv/clk.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command clk"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_clock_init
#define MODULE_EXIT      cmd_clock_exit

static void cmd_clock_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage: \n");
    vmm_cdev_printf(cdev, "   clk help\n");
    vmm_cdev_printf(cdev, "   clk dump\n");
    vmm_cdev_printf(cdev, "   clk summary\n");
}

static int cmd_clock_dump(vmm_char_device_t *cdev)
{
    return clock_dump(cdev);
}

static int cmd_clock_summary(vmm_char_device_t *cdev)
{
    return clock_summary_show(cdev);
}

static int cmd_clock_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc != 2) {
        goto err;
    }

    if (strcmp(argv[1], "help") == 0) {
        cmd_clock_usage(cdev);
        return VMM_OK;
    } else if (strcmp(argv[1], "dump") == 0) {
        return cmd_clock_dump(cdev);
    } else if (strcmp(argv[1], "summary") == 0) {
        return cmd_clock_summary(cdev);
    }

err:
    cmd_clock_usage(cdev);
    return VMM_EINVALID;
}

static vmm_command_t cmd_clock = {
    .name  = "clk",
    .desc  = "clk commands",
    .usage = cmd_clock_usage,
    .exec  = cmd_clock_exec,
};

static int __init cmd_clock_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_clock);
}

static void __exit cmd_clock_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_clock);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
