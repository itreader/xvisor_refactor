/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file cmd_vmsg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief command for virtual messaging subsystem.
 */

#include <libs/stringlib.h>
#include <vio/vmm_vmsg.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command vmsg"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_vmsg_init
#define MODULE_EXIT      cmd_vmsg_exit

static void cmd_vmsg_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   vmsg help\n");
    vmm_cdev_printf(cdev, "   vmsg node_list\n");
    vmm_cdev_printf(cdev, "   vmsg domain_create <domain_name>\n");
    vmm_cdev_printf(cdev, "   vmsg domain_destroy <domain_name>\n");
    vmm_cdev_printf(cdev, "   vmsg domain_list\n");
}

struct cmd_vmsg_list_priv {
    uint32_t           num1;
    uint32_t           num2;
    vmm_char_device_t *cdev;
};

static int cmd_vmsg_node_list_iter(struct vmm_vmsg_node *node, void *data)
{
    struct cmd_vmsg_list_priv *p = data;

    vmm_cdev_printf(
        p->cdev, " %-4d %-26s %-16s %-7s 0x%08x 0x%08x\n", p->num1, vmm_vmsg_node_get_name(node),
        vmm_vmsg_domain_get_name(vmm_vmsg_node_get_domain(node)), vmm_vmsg_node_is_ready(node) ? "RDY" : "NOT-RDY", vmm_vmsg_node_get_addr(node),
        vmm_vmsg_node_get_max_data_len(node));
    p->num1++;

    return VMM_OK;
}

static int cmd_vmsg_node_list(vmm_char_device_t *cdev)
{
    struct cmd_vmsg_list_priv p = {.num1 = 0, .num2 = 0, .cdev = cdev};

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");
    vmm_cdev_printf(cdev, " %-4s %-26s %-16s %-7s %-10s %-10s\n", "Num#", "Node", "Domain", "State", "Address", "Max Data");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");
    vmm_vmsg_node_iterate(NULL, &p, cmd_vmsg_node_list_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "---------------------------------------\n");

    return VMM_OK;
}

static int cmd_vmsg_domain_node_list_iter(struct vmm_vmsg_node *node, void *data)
{
    struct cmd_vmsg_list_priv *p = data;

    if (p->num1 == 0) {
        vmm_cdev_printf(
            p->cdev, " %-5d %-21s +--%-41s\n", p->num2, vmm_vmsg_domain_get_name(vmm_vmsg_node_get_domain(node)), vmm_vmsg_node_get_name(node));
    } else {
        vmm_cdev_printf(p->cdev, " %-5s %-21s +--%-41s\n", "", "", vmm_vmsg_node_get_name(node));
    }

    p->num1++;

    return VMM_OK;
}

static int cmd_vmsg_domain_list_iter(struct vmm_vmsg_domain *domain, void *data)
{
    struct cmd_vmsg_list_priv *p  = data;
    struct cmd_vmsg_list_priv  np = {.num1 = 0, .num2 = 0, .cdev = p->cdev};

    np.num2                       = p->num1;
    vmm_vmsg_domain_node_iterate(domain, NULL, &np, cmd_vmsg_domain_node_list_iter);

    if (!np.num1) {
        vmm_cdev_printf(p->cdev, " %-5d %-21s +--%-41s\n", p->num1, vmm_vmsg_domain_get_name(domain), "");
    }

    p->num1++;

    vmm_cdev_printf(
        p->cdev, "----------------------------------------"
                 "------------------------------\n");

    return VMM_OK;
}

static int cmd_vmsg_domain_list(vmm_char_device_t *cdev)
{
    struct cmd_vmsg_list_priv p = {.num1 = 0, .num2 = 0, .cdev = cdev};

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "------------------------------\n");
    vmm_cdev_printf(cdev, " %-5s %-21s %-41s\n", "Num#", "Domain", "Node List");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "------------------------------\n");
    vmm_vmsg_domain_iterate(NULL, &p, cmd_vmsg_domain_list_iter);

    if (p.num1) {
        goto done;
    }

    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "------------------------------\n");

done:
    return VMM_OK;
}

static int cmd_vmsg_domain_create(vmm_char_device_t *cdev, const char *name)
{
    int                     ret;
    struct vmm_vmsg_domain *domain = vmm_vmsg_domain_find(name);

    if (domain) {
        vmm_cdev_printf(cdev, "Domain already exist\n");
        return VMM_ENOTAVAIL;
    }

    domain = vmm_vmsg_domain_create(name, NULL);

    if (domain) {
        vmm_cdev_printf(cdev, "%s: Created\n", name);
        ret = VMM_OK;
    } else {
        vmm_cdev_printf(cdev, "%s: Failed to create\n", name);
        ret = VMM_EFAIL;
    }

    return ret;
}

static int cmd_vmsg_domain_destroy(vmm_char_device_t *cdev, const char *name)
{
    int                     ret;
    struct vmm_vmsg_domain *domain = vmm_vmsg_domain_find(name);

    if (!domain) {
        vmm_cdev_printf(cdev, "Failed to find domain\n");
        return VMM_ENOTAVAIL;
    }

    if ((ret = vmm_vmsg_domain_destroy(domain))) {
        vmm_cdev_printf(cdev, "%s: Failed to destroy\n", name);
    } else {
        vmm_cdev_printf(cdev, "%s: Destroyed\n", name);
    }

    return ret;
}

static int cmd_vmsg_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc <= 1) {
        goto fail;
    }

    if (strcmp(argv[1], "help") == 0) {
        cmd_vmsg_usage(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "node_list") == 0) && (argc == 2)) {
        return cmd_vmsg_node_list(cdev);
    } else if (strcmp(argv[1], "domain_list") == 0 && (argc == 2)) {
        return cmd_vmsg_domain_list(cdev);
    } else if (strcmp(argv[1], "domain_create") == 0 && (argc == 3)) {
        return cmd_vmsg_domain_create(cdev, argv[2]);
    } else if (strcmp(argv[1], "domain_destroy") == 0 && (argc == 3)) {
        return cmd_vmsg_domain_destroy(cdev, argv[2]);
    }

fail:
    cmd_vmsg_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_vmsg = {
    .name  = "vmsg",
    .desc  = "virtual messaging commands",
    .usage = cmd_vmsg_usage,
    .exec  = cmd_vmsg_exec,
};

static int __init cmd_vmsg_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_vmsg);
}

static void __exit cmd_vmsg_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_vmsg);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
