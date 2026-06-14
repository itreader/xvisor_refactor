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
 * @file cmd_vsdaemon.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vsdaemon command
 */

#include <libs/stringlib.h>
#include <libs/vsdaemon.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command vsdaemon"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_vsdaemon_init
#define MODULE_EXIT      cmd_vsdaemon_exit

static void cmd_vsdaemon_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   vsdaemon help\n");
    vmm_cdev_printf(cdev, "   vsdaemon transport_list\n");
    vmm_cdev_printf(cdev, "   vsdaemon list\n");
    vmm_cdev_printf(
        cdev, "   vsdaemon create <transport_name>"
              " <vserial_name> <daemon_name> ...\n");
    vmm_cdev_printf(
        cdev, "      vsdaemon create management_terminal"
              " <vserial_name> <daemon_name>\n");
    vmm_cdev_printf(
        cdev, "      vsdaemon create char_device"
              " <vserial_name> <daemon_name> <char_device_name>\n");
    vmm_cdev_printf(
        cdev, "      vsdaemon create telnet"
              " <vserial_name> <daemon_name> <port_number>\n");
    vmm_cdev_printf(cdev, "   vsdaemon destroy <daemon_name>\n");
}

static void cmd_vsdaemon_transport_list(vmm_char_device_t *cdev)
{
    int                        i, count;
    struct vsdaemon_transport *trans;

    vmm_cdev_printf(cdev, "------------------------------\n");
    vmm_cdev_printf(cdev, " %-4s %-24s\n", "#", "Transport Name");
    vmm_cdev_printf(cdev, "------------------------------\n");

    count = vsdaemon_transport_count();

    for (i = 0; i < count; i++) {
        trans = vsdaemon_transport_get(i);

        if (!trans) {
            continue;
        }

        vmm_cdev_printf(cdev, " %-4d %-24s\n", i, trans->name);
    }

    vmm_cdev_printf(cdev, "------------------------------\n");
}

static void cmd_vsdaemon_list(vmm_char_device_t *cdev)
{
    int              i, count;
    struct vsdaemon *vsd;

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-4s %-24s %-24s %-24s\n", "#", "Daemon Name", "Transport Name", "Vserial Name");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");

    count = vsdaemon_count();

    for (i = 0; i < count; i++) {
        if (!(vsd = vsdaemon_get(i))) {
            continue;
        }

        vmm_cdev_printf(cdev, " %-4d %-24s %-24s %-24s\n", i, vsd->name, vsd->trans->name, vsd->vser->name);
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_vsdaemon_create(vmm_char_device_t *cdev, const char *trans, const char *vser, const char *name, int argc, char **argv)
{
    int ret = VMM_ERR_FAIL;

    if ((ret = vsdaemon_create(trans, vser, name, argc, argv))) {
        vmm_cdev_printf(
            cdev,
            "Error: failed to create "
            "%s vsdaemon for %s\n",
            trans, vser);
    } else {
        vmm_cdev_printf(cdev, "Created vsdaemon %s successfully\n", name);
    }

    return ret;
}

static int cmd_vsdaemon_destroy(vmm_char_device_t *cdev, const char *name)
{
    int ret = VMM_ERR_FAIL;

    if ((ret = vsdaemon_destroy(name))) {
        vmm_cdev_printf(cdev, "Failed to destroy vsdaemon %s\n", name);
    }

    return ret;
}

static int cmd_vsdaemon_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc <= 1) {
        goto fail;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_vsdaemon_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "transport_list") == 0) {
            cmd_vsdaemon_transport_list(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_vsdaemon_list(cdev);
            return VMM_OK;
        }
    }

    if (argc < 3) {
        goto fail;
    }

    if ((strcmp(argv[1], "create") == 0) && (argc >= 5)) {
        return cmd_vsdaemon_create(cdev, argv[2], argv[3], argv[4], argc - 5, &argv[5]);
    } else if ((strcmp(argv[1], "destroy") == 0) && (argc == 3)) {
        return cmd_vsdaemon_destroy(cdev, argv[2]);
    }

fail:
    cmd_vsdaemon_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_vsdaemon = {
    .name  = "vsdaemon",
    .desc  = "commands for vserial daemons",
    .usage = cmd_vsdaemon_usage,
    .exec  = cmd_vsdaemon_exec,
};

static int __init cmd_vsdaemon_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_vsdaemon);
}

static void __exit cmd_vsdaemon_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_vsdaemon);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
