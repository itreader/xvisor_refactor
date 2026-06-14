/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cmd_virtual_screen.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of virtual_screen command
 */

#include <libs/stringlib.h>
#include <libs/virtual_screen.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command virtual_screen"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_virtual_screen_init
#define MODULE_EXIT      cmd_virtual_screen_exit

static void cmd_virtual_screen_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   virtual_screen help\n");
    vmm_cdev_printf(cdev, "   virtual_screen device_list <guest_name>\n");
    vmm_cdev_printf(
        cdev, "   virtual_screen hard_bind <guest_name> "
              "[<fb_name>] [<virtual_display_name>] "
              "[<vkeyboard_name>] [<vmouse_name>]\n");
    vmm_cdev_printf(
        cdev, "   virtual_screen soft_bind <guest_name> "
              "[<refresh_rate>] [<fb_name>] [<virtual_display_name>] "
              "[<vkeyboard_name>] [<vmouse_name>]\n");
    vmm_cdev_printf(cdev, "   virtual_screen unbind [<fb_name>]\n");
}

struct virtual_screen_iter {
    bool                        found;
    bool                        print;
    vmm_char_device_t          *cdev;
    struct vmm_guest           *guest;
    struct vmm_virtual_display *vdis;
    struct vmm_vkeyboard       *vkbd;
    struct vmm_vmouse          *vmou;
};

static int cmd_virtual_screen_virtual_display_iter(struct vmm_virtual_display *vdis, void *data)
{
    struct virtual_screen_iter *iter = data;

    if (!strncmp(vdis->name, iter->guest->name, strlen(iter->guest->name))) {
        if (iter->print) {
            if (!iter->found) {
                vmm_cdev_printf(iter->cdev, " (default)");
            } else {
                vmm_cdev_printf(iter->cdev, "          ");
            }

            vmm_cdev_printf(iter->cdev, " %s\n", vdis->name);
        }

        if (!iter->vdis) {
            iter->vdis = vdis;
        }

        iter->found = TRUE;
    }

    return VMM_OK;
}

static int cmd_virtual_screen_vkeyboard_iter(struct vmm_vkeyboard *vkbd, void *data)
{
    struct virtual_screen_iter *iter = data;

    if (!strncmp(vkbd->name, iter->guest->name, strlen(iter->guest->name))) {
        if (iter->print) {
            if (!iter->found) {
                vmm_cdev_printf(iter->cdev, " (default)");
            } else {
                vmm_cdev_printf(iter->cdev, "          ");
            }

            vmm_cdev_printf(iter->cdev, " %s\n", vkbd->name);
        }

        if (!iter->vkbd) {
            iter->vkbd = vkbd;
        }

        iter->found = TRUE;
    }

    return VMM_OK;
}

static int cmd_virtual_screen_vmouse_iter(struct vmm_vmouse *vmou, void *data)
{
    struct virtual_screen_iter *iter = data;

    if (!strncmp(vmou->name, iter->guest->name, strlen(iter->guest->name))) {
        if (iter->print) {
            if (!iter->found) {
                vmm_cdev_printf(iter->cdev, " (default)");
            } else {
                vmm_cdev_printf(iter->cdev, "          ");
            }

            vmm_cdev_printf(iter->cdev, " %s\n", vmou->name);
        }

        if (!iter->vmou) {
            iter->vmou = vmou;
        }

        iter->found = TRUE;
    }

    return VMM_OK;
}

static int cmd_virtual_screen_device_list(vmm_char_device_t *cdev, const char *guest_name)
{
    struct vmm_guest          *guest = NULL;
    struct virtual_screen_iter iter;

    guest = vmm_manager_guest_find(guest_name);

    if (!guest) {
        vmm_cdev_printf(cdev, "Failed to find guest %s\n", guest_name);
        return VMM_ERR_NOTAVAIL;
    }

    vmm_cdev_printf(cdev, "Virtual Display List\n");
    iter.found = FALSE;
    iter.print = TRUE;
    iter.cdev  = cdev;
    iter.guest = guest;
    iter.vdis  = NULL;
    iter.vkbd  = NULL;
    iter.vmou  = NULL;
    vmm_virtual_display_iterate(NULL, &iter, cmd_virtual_screen_virtual_display_iter);
    vmm_cdev_printf(cdev, "\n");

    vmm_cdev_printf(cdev, "Virtual Keyboard List\n");
    iter.found = FALSE;
    iter.print = TRUE;
    iter.cdev  = cdev;
    iter.guest = guest;
    iter.vdis  = NULL;
    iter.vkbd  = NULL;
    iter.vmou  = NULL;
    vmm_vkeyboard_iterate(NULL, &iter, cmd_virtual_screen_vkeyboard_iter);
    vmm_cdev_printf(cdev, "\n");

    vmm_cdev_printf(cdev, "Virtual Mouse List\n");
    iter.found = FALSE;
    iter.print = TRUE;
    iter.cdev  = cdev;
    iter.guest = guest;
    iter.vdis  = NULL;
    iter.vkbd  = NULL;
    iter.vmou  = NULL;
    vmm_vmouse_iterate(NULL, &iter, cmd_virtual_screen_vmouse_iter);
    vmm_cdev_printf(cdev, "\n");

    return VMM_OK;
}

static int cmd_virtual_screen_bind(
    vmm_char_device_t *cdev, bool is_hard, const char *guest_name, const char *refresh_rate, const char *fb_name, const char *virtual_display_name,
    const char *vkeyboard_name, const char *vmouse_name)
{
    int                         rc;
    uint32_t                    rate, ekey[3];
    struct frame_buffer_info   *info;
    struct vmm_guest           *guest = NULL;
    struct vmm_virtual_display *vdis  = NULL;
    struct vmm_vkeyboard       *vkbd  = NULL;
    struct vmm_vmouse          *vmou  = NULL;
    struct virtual_screen_iter  iter;

    guest = vmm_manager_guest_find(guest_name);

    if (!guest) {
        vmm_cdev_printf(cdev, "Failed to find guest %s\n", guest_name);
        return VMM_ERR_NOTAVAIL;
    }

    if (refresh_rate) {
        rate = (uint32_t)strtoul(refresh_rate, NULL, 10);
    } else {
        rate = VSCREEN_REFRESH_RATE_GOOD;
    }

    if ((rate < VSCREEN_REFRESH_RATE_MIN) || (VSCREEN_REFRESH_RATE_MAX < rate)) {
        vmm_cdev_printf(cdev, "Invalid refresh rate %d\n", rate);
        vmm_cdev_printf(
            cdev,
            "Refresh rate should be "
            "between %d and %d\n",
            VSCREEN_REFRESH_RATE_MIN, VSCREEN_REFRESH_RATE_MAX);
        return VMM_ERR_INVALID;
    }

    if (fb_name) {
        info = fb_find(fb_name);
    } else {
        info = fb_find("fb0");
    }

    if (!info) {
        vmm_cdev_printf(cdev, "Failed to find frame_buffer_info %s\n", fb_name);
        return VMM_ERR_NOTAVAIL;
    }

    if (virtual_display_name) {
        vdis = vmm_virtual_display_find(virtual_display_name);
    } else {
        iter.found = FALSE;
        iter.print = FALSE;
        iter.cdev  = cdev;
        iter.guest = guest;
        iter.vdis  = NULL;
        iter.vkbd  = NULL;
        iter.vmou  = NULL;
        vmm_virtual_display_iterate(NULL, &iter, cmd_virtual_screen_virtual_display_iter);
        vdis = (iter.found) ? iter.vdis : NULL;
    }

    if (!vdis) {
        vmm_cdev_printf(
            cdev, "Failed to find virtual display%s %s\n", (virtual_display_name) ? "" : " for guest",
            (virtual_display_name) ? virtual_display_name : guest->name);
        return VMM_ERR_NOTAVAIL;
    }

    if (vkeyboard_name) {
        vkbd = vmm_vkeyboard_find(vkeyboard_name);
    } else {
        iter.found = FALSE;
        iter.print = FALSE;
        iter.cdev  = cdev;
        iter.guest = guest;
        iter.vdis  = NULL;
        iter.vkbd  = NULL;
        iter.vmou  = NULL;
        vmm_vkeyboard_iterate(NULL, &iter, cmd_virtual_screen_vkeyboard_iter);
        vkbd = (iter.found) ? iter.vkbd : NULL;
    }

    if (!vkbd && vkeyboard_name) {
        vmm_cdev_printf(cdev, "Failed to find virtual keyboard %s\n", vkeyboard_name);
        return VMM_ERR_NOTAVAIL;
    }

    if (vmouse_name) {
        vmou = vmm_vmouse_find(vmouse_name);
    } else {
        iter.found = FALSE;
        iter.print = FALSE;
        iter.cdev  = cdev;
        iter.guest = guest;
        iter.vdis  = NULL;
        iter.vkbd  = NULL;
        iter.vmou  = NULL;
        vmm_vmouse_iterate(NULL, &iter, cmd_virtual_screen_vmouse_iter);
        vmou = (iter.found) ? iter.vmou : NULL;
    }

    if (!vmou && vmouse_name) {
        vmm_cdev_printf(cdev, "Failed to find virtual mouse %s\n", vmouse_name);
        return VMM_ERR_NOTAVAIL;
    }

    ekey[0] = KEY_ESC;
    ekey[1] = KEY_X;
    ekey[2] = KEY_Q;

    vmm_cdev_printf(cdev, "Guest name      : %s\n", guest->name);

    if (!is_hard) {
        vmm_cdev_printf(cdev, "Refresh rate    : %d per-second\n", rate);
    }

    vmm_cdev_printf(cdev, "Escape Keys     : ESC+X+Q\n");
    vmm_cdev_printf(cdev, "Frame buffer    : %s\n", info->name);
    vmm_cdev_printf(cdev, "Virtual display : %s\n", vdis->name);
    vmm_cdev_printf(cdev, "Virtual keyboard: %s\n", (vkbd) ? vkbd->name : "---");
    vmm_cdev_printf(cdev, "Virtual mouse   : %s\n", (vmou) ? vmou->name : "---");

    if (is_hard) {
        rc = virtual_screen_hard_bind(ekey[0], ekey[1], ekey[2], info, vdis, vkbd, vmou);
    } else {
        rc = virtual_screen_soft_bind(rate, ekey[0], ekey[1], ekey[2], info, vdis, vkbd, vmou);
    }

    return rc;
}

static int cmd_virtual_screen_unbind(vmm_char_device_t *cdev, const char *fb_name)
{
    struct frame_buffer_info *info;

    if (fb_name) {
        info = fb_find(fb_name);
    } else {
        info = fb_find("fb0");
    }

    if (!info) {
        vmm_cdev_printf(cdev, "Failed to find frame_buffer_info\n");
        return VMM_ERR_NODEV;
    }

    return virtual_screen_unbind(info);
}

static int cmd_virtual_screen_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc < 2) {
        goto cmd_virtual_screen_fail;
    }

    if (strcmp(argv[1], "help") == 0) {
        cmd_virtual_screen_usage(cdev);
        return VMM_OK;
    } else if ((strcmp(argv[1], "device_list") == 0) && (argc == 3)) {
        return cmd_virtual_screen_device_list(cdev, argv[2]);
    } else if ((strcmp(argv[1], "soft_bind") == 0) && (argc > 2)) {
        return cmd_virtual_screen_bind(
            cdev, FALSE, argv[2], (argc > 3) ? argv[3] : NULL, (argc > 4) ? argv[4] : NULL, (argc > 5) ? argv[5] : NULL, (argc > 6) ? argv[6] : NULL,
            (argc > 7) ? argv[7] : NULL);
    } else if ((strcmp(argv[1], "hard_bind") == 0) && (argc > 2)) {
        return cmd_virtual_screen_bind(
            cdev, TRUE, argv[2], NULL, (argc > 3) ? argv[3] : NULL, (argc > 4) ? argv[4] : NULL, (argc > 5) ? argv[5] : NULL,
            (argc > 6) ? argv[6] : NULL);
    } else if ((strcmp(argv[1], "unbind") == 0) && (argc <= 3)) {
        return cmd_virtual_screen_unbind(cdev, (argc == 3) ? argv[2] : NULL);
    }

cmd_virtual_screen_fail:
    cmd_virtual_screen_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_virtual_screen = {
    .name  = "virtual_screen",
    .desc  = "virtual screen commands",
    .usage = cmd_virtual_screen_usage,
    .exec  = cmd_virtual_screen_exec,
};

static int __init cmd_virtual_screen_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_virtual_screen);
}

static void __exit cmd_virtual_screen_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_virtual_screen);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
