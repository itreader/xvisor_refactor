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
 * @file cmd_virtual_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of virtual_input command
 */

#include <libs/stringlib.h>
#include <vio/vmm_keymaps.h>
#include <vio/vmm_virtual_input.h>
#include <vmm_command_manager.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command virtual_input"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_virtual_input_init
#define MODULE_EXIT      cmd_virtual_input_exit

static void cmd_virtual_input_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   virtual_input help\n");
    vmm_cdev_printf(cdev, "   virtual_input keyboards\n");
    vmm_cdev_printf(
        cdev, "   virtual_input keyboard_event <vkeyboard_name> "
              "<keycode>\n");
    vmm_cdev_printf(cdev, "   virtual_input mouses\n");
    vmm_cdev_printf(
        cdev, "   virtual_input mouse_event <vmouse_name> "
              "<dx> <dy> <dz> <left|right|middle|none>\n");
}

static int cmd_virtual_input_keyboards_iter(struct vmm_vkeyboard *vk, void *data)
{
    int                ledstate;
    const char        *num_lock, *caps_lock, *scroll_lock;
    vmm_char_device_t *cdev = data;

    ledstate                = vmm_vkeyboard_get_ledstate(vk);

    if (ledstate & VMM_NUM_LOCK_LED) {
        num_lock = "ON";
    } else {
        num_lock = "OFF";
    }

    if (ledstate & VMM_CAPS_LOCK_LED) {
        caps_lock = "ON";
    } else {
        caps_lock = "OFF";
    }

    if (ledstate & VMM_SCROLL_LOCK_LED) {
        scroll_lock = "ON";
    } else {
        scroll_lock = "OFF";
    }

    vmm_cdev_printf(cdev, " %-45s %-10s %-10s %-10s\n", vk->name, num_lock, caps_lock, scroll_lock);

    return VMM_OK;
}

static int cmd_virtual_input_keyboards(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-45s %-10s %-10s %-10s\n", "Name", "NumLock", "CapsLock", "ScrollLock");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_vkeyboard_iterate(NULL, cdev, cmd_virtual_input_keyboards_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    return VMM_OK;
}

static int cmd_virtual_input_keyboard_event(vmm_char_device_t *cdev, const char *vkeyboard_name, int keyc, char **keyv)
{
    int                   k;
    uint64_t              keycode;
    struct vmm_vkeyboard *vk = vmm_vkeyboard_find(vkeyboard_name);

    if (!vk) {
        vmm_cdev_printf(cdev, "Error: virtual keyboard %s not found\n", vkeyboard_name);
        return VMM_ERR_NODEV;
    }

    /* Press the Keys (or Key Down) */
    for (k = 0; k < keyc; k++) {
        keycode = strtoul(keyv[k], NULL, 0);

        if (keycode & SCANCODE_GREY) {
            vmm_vkeyboard_event(vk, SCANCODE_EMUL0, -1);
        }

        vmm_vkeyboard_event(vk, keycode & SCANCODE_KEYCODEMASK, vmm_vkeycode2vkey(keycode));
    }

    /* Release the Keys (or Key Up) */
    for (k = keyc - 1; 0 <= k; k--) {
        keycode = strtoul(keyv[k], NULL, 0);

        if (keycode & SCANCODE_GREY) {
            vmm_vkeyboard_event(vk, SCANCODE_EMUL0, -1);
        }

        vmm_vkeyboard_event(vk, keycode | SCANCODE_UP, vmm_vkeycode2vkey(keycode));
    }

    return VMM_OK;
}

static int cmd_virtual_input_mouse_event(
    vmm_char_device_t *cdev, const char *vmouse_name, const char *dxstr, const char *dystr, const char *dzstr, const char *button)
{
    int                dx, dy, dz, buttons_state;
    struct vmm_vmouse *vm = vmm_vmouse_find(vmouse_name);

    if (!vm) {
        vmm_cdev_printf(cdev, "Error: virtual mouse %s not found\n", vmouse_name);
        return VMM_ERR_NODEV;
    }

    /* Determine mouse displacement */
    dx            = atoi(dxstr);
    dy            = atoi(dystr);
    dz            = atoi(dzstr);

    /* Determine button state */
    buttons_state = 0;

    if (strcmp(button, "left") == 0) {
        buttons_state |= VMM_MOUSE_LBUTTON;
    } else if (strcmp(button, "middle") == 0) {
        buttons_state |= VMM_MOUSE_MBUTTON;
    } else if (strcmp(button, "right") == 0) {
        buttons_state |= VMM_MOUSE_RBUTTON;
    }

    /* Trigger mouse event */
    vmm_vmouse_event(vm, dx, dy, dz, buttons_state);

    return VMM_OK;
}

static int cmd_virtual_input_mouses_iter(struct vmm_vmouse *vm, void *data)
{
    uint32_t           gw, gh, gr;
    const char        *is_abs;
    vmm_char_device_t *cdev = data;

    if (vmm_vmouse_is_absolute(vm)) {
        is_abs = "Yes";
    } else {
        is_abs = "No";
    }

    gw = vmm_vmouse_get_graphics_width(vm);
    gh = vmm_vmouse_get_graphics_height(vm);
    gr = vmm_vmouse_get_graphics_rotation(vm);
    vmm_cdev_printf(cdev, " %-45s %-8s %-6d %-7d %-8d\n", vm->name, is_abs, gw, gh, gr);

    return VMM_OK;
}

static int cmd_virtual_input_mouses(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-45s %-8s %-6s %-7s %-8s\n", "Name", "Absolute", "Width", "Height", "Rotation");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_vmouse_iterate(NULL, cdev, cmd_virtual_input_mouses_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");

    return VMM_OK;
}

static int cmd_virtual_input_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_virtual_input_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "keyboards") == 0) {
            return cmd_virtual_input_keyboards(cdev);
        } else if (strcmp(argv[1], "mouses") == 0) {
            return cmd_virtual_input_mouses(cdev);
        }
    } else if (argc > 2) {
        if ((argc > 3) && strcmp(argv[1], "keyboard_event") == 0) {
            return cmd_virtual_input_keyboard_event(cdev, argv[2], argc - 3, &argv[3]);
        } else if ((argc > 6) && strcmp(argv[1], "mouse_event") == 0) {
            return cmd_virtual_input_mouse_event(cdev, argv[2], argv[3], argv[4], argv[5], argv[6]);
        }
    }

    cmd_virtual_input_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_virtual_input = {
    .name  = "virtual_input",
    .desc  = "virtual input device commands",
    .usage = cmd_virtual_input_usage,
    .exec  = cmd_virtual_input_exec,
};

static int __init cmd_virtual_input_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_virtual_input);
}

static void __exit cmd_virtual_input_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_virtual_input);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
