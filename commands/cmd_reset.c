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
 * @file cmd_reset.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of reset command
 */

#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command reset"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_reset_init
#define MODULE_EXIT      cmd_reset_exit

static void cmd_reset_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage: ");
    vmm_cdev_printf(cdev, "   reset\n");
}

static int cmd_reset_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    /* Reset the hypervisor */
    vmm_reset();
    return VMM_OK;
}

static vmm_command_t cmd_reset = {
    .name  = "reset",
    .desc  = "reset hypervisor",
    .usage = cmd_reset_usage,
    .exec  = cmd_reset_exec,
};

static int __init cmd_reset_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_reset);
}

static void __exit cmd_reset_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_reset);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
