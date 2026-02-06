/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file cmd_wboxtest.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Command file for white-box testing.
 */

#include <libs/stringlib.h>
#include <libs/white_box_test.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command white_box_test"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_wboxtest_init
#define MODULE_EXIT      cmd_wboxtest_exit

static void cmd_wboxtest_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   white_box_test help\n");
    vmm_cdev_printf(cdev, "   white_box_test test_list\n");
    vmm_cdev_printf(cdev, "   white_box_test group_list\n");
    vmm_cdev_printf(cdev, "   white_box_test run_all <iterations>\n");
    vmm_cdev_printf(
        cdev, "   white_box_test run_tests <iterations> <test0_name>"
              " <test1_name> ... <testN_name>\n");
    vmm_cdev_printf(
        cdev, "   white_box_test run_groups <iterations> <group0_name>"
              " <group1_name> ... <groupN_name>\n");
}

struct cmd_wboxtest_test_list_args {
    uint32_t           index;
    vmm_char_device_t *cdev;
};

static void cmd_wboxtest_test_list_iter(struct white_box_test *test, void *data)
{
    struct cmd_wboxtest_test_list_args *args = data;

    vmm_cdev_printf(args->cdev, " %-7d %-35s %-35s\n", args->index++, test->group->name, test->name);
}

static void cmd_wboxtest_test_list(vmm_char_device_t *cdev)
{
    struct cmd_wboxtest_test_list_args args;

    args.index = 0;
    args.cdev  = cdev;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-7s %-35s %-35s\n", "#", "Group Name", "Test Name");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    wboxtest_iterate(cmd_wboxtest_test_list_iter, &args);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

struct cmd_wboxtest_group_list_args {
    uint32_t           index;
    vmm_char_device_t *cdev;
};

static void cmd_wboxtest_group_list_iter(struct wboxtest_group *group, void *data)
{
    struct cmd_wboxtest_group_list_args *args = data;

    vmm_cdev_printf(args->cdev, " %-7d %-35s %-35d\n", args->index++, group->name, group->test_count);
}

static void cmd_wboxtest_group_list(vmm_char_device_t *cdev)
{
    struct cmd_wboxtest_group_list_args args;

    args.index = 0;
    args.cdev  = cdev;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-7s %-35s %-35s\n", "#", "Group Name", "Test Count");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    wboxtest_group_iterate(cmd_wboxtest_group_list_iter, &args);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static void cmd_wboxtest_run_all(vmm_char_device_t *cdev, uint32_t iterations)
{
    wboxtest_run_all(cdev, iterations);
}

static void cmd_wboxtest_run_tests(vmm_char_device_t *cdev, uint32_t iterations, int test_count, char **test_names)
{
    wboxtest_run_tests(cdev, iterations, test_count, test_names);
}

static void cmd_wboxtest_run_groups(vmm_char_device_t *cdev, uint32_t iterations, int group_count, char **group_names)
{
    wboxtest_run_groups(cdev, iterations, group_count, group_names);
}

static int cmd_wboxtest_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    uint32_t iterations;

    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_wboxtest_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "test_list") == 0) {
            cmd_wboxtest_test_list(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "group_list") == 0) {
            cmd_wboxtest_group_list(cdev);
            return VMM_OK;
        }
    } else if (argc == 3) {
        iterations = strtoul(argv[2], NULL, 10);

        if (strcmp(argv[1], "run_all") == 0) {
            cmd_wboxtest_run_all(cdev, iterations);
            return VMM_OK;
        }
    } else if (argc > 3) {
        iterations = strtoul(argv[2], NULL, 10);

        if (strcmp(argv[1], "run_tests") == 0) {
            cmd_wboxtest_run_tests(cdev, iterations, argc - 3, &argv[3]);
            return VMM_OK;
        } else if (strcmp(argv[1], "run_groups") == 0) {
            cmd_wboxtest_run_groups(cdev, iterations, argc - 3, &argv[3]);
            return VMM_OK;
        }
    }

    cmd_wboxtest_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_wboxtest = {
    .name  = "white_box_test",
    .desc  = "commands for white-box testing",
    .usage = cmd_wboxtest_usage,
    .exec  = cmd_wboxtest_exec,
};

static int __init cmd_wboxtest_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_wboxtest);
}

static void __exit cmd_wboxtest_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_wboxtest);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
