/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cmd_thread.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command file for hypervisor threads control.
 */

#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command thread"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_thread_init
#define MODULE_EXIT      cmd_thread_exit

static void cmd_thread_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   thread help\n");
    vmm_cdev_printf(cdev, "   thread list\n");
}

static void cmd_thread_list(vmm_char_device_t *cdev)
{
    int           rc, index, count;
    char          state[10], name[VMM_FIELD_NAME_SIZE];
    vmm_thread_t *thread_info;
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-6s %-7s %-10s %-53s\n", "ID ", "Prio", "State", "Name");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    count = vmm_threads_count();

    for (index = 0; index < count; index++) {
        thread_info = vmm_threads_index2thread(index);

        switch (vmm_threads_get_state(thread_info)) {
            case VMM_THREAD_STATE_CREATED:
                strcpy(state, "Created");
                break;

            case VMM_THREAD_STATE_RUNNING:
                strcpy(state, "Running");
                break;

            case VMM_THREAD_STATE_SLEEPING:
                strcpy(state, "Sleeping");
                break;

            case VMM_THREAD_STATE_STOPPED:
                strcpy(state, "Stopped");
                break;

            default:
                strcpy(state, "Invalid");
                break;
        }

        if ((rc = vmm_threads_get_name(name, thread_info))) {
            strcpy(name, "(NA)");
        }

        vmm_cdev_printf(cdev, " %-6d %-7d %-10s %-53s\n", vmm_threads_get_id(thread_info), vmm_threads_get_priority(thread_info), state, name);
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_thread_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_thread_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_thread_list(cdev);
            return VMM_OK;
        }
    }

    cmd_thread_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_thread = {
    .name  = "thread",
    .desc  = "control commands for threads",
    .usage = cmd_thread_usage,
    .exec  = cmd_thread_exec,
};

static int __init cmd_thread_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_thread);
}

static void __exit cmd_thread_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_thread);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
