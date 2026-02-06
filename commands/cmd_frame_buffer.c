/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cmd_frame_buffer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of fb command
 */

#include <drv/frame_buffer.h>
#include <libs/image_loader.h>
#include <libs/stringlib.h>
#include <vmm_command_manager.h>
#include <vmm_delay.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "Command fb"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_frame_buffer_init
#define MODULE_EXIT      cmd_frame_buffer_exit

static void cmd_frame_buffer_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   fb help\n");
    vmm_cdev_printf(cdev, "   fb list\n");
    vmm_cdev_printf(cdev, "   fb info <fb_name>\n");
    vmm_cdev_printf(cdev, "   fb blank <fb_name> <value>\n");
    vmm_cdev_printf(
        cdev, "   fb fillrect <fb_name> <x> <y> <w> <h> <c> "
              "[<rop>]\n");
    vmm_cdev_printf(cdev, "   fb image <fb_name> <image_path> [<x>] [<y>]\n");
}

static int cmd_frame_buffer_list_iter(struct frame_buffer_info *info, void *data)
{
    int                rc;
    char               path[256];
    vmm_char_device_t *cdev = data;

    if (info->dev.parent && info->dev.parent->of_node) {
        rc = vmm_device_tree_getpath(path, sizeof(path), info->dev.parent->of_node);

        if (rc) {
            vmm_snprintf(path, sizeof(path), "----- (error %d)", rc);
        }
    } else {
        strcpy(path, "-----");
    }

    vmm_cdev_printf(cdev, " %-16s %-20s %-40s\n", info->name, info->fix.id, path);

    return VMM_OK;
}

static void cmd_frame_buffer_list(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    vmm_cdev_printf(cdev, " %-16s %-20s %-40s\n", "Name", "ID", "Device Path");
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
    fb_iterate(NULL, cdev, cmd_frame_buffer_list_iter);
    vmm_cdev_printf(
        cdev, "----------------------------------------"
              "----------------------------------------\n");
}

static int cmd_frame_buffer_info(vmm_char_device_t *cdev, struct frame_buffer_info *info)
{
    uint32_t    i;
    const char *str;

    vmm_cdev_printf(cdev, "Name   : %s\n", info->name);
    vmm_cdev_printf(cdev, "ID     : %s\n", info->fix.id);

    switch (info->fix.type) {
        case FB_TYPE_PACKED_PIXELS:
            str = "Packed Pixels";
            break;

        case FB_TYPE_PLANES:
            str = "Non interleaved planes";
            break;

        case FB_TYPE_INTERLEAVED_PLANES:
            str = "Interleaved planes";
            break;

        case FB_TYPE_TEXT:
            str = "Text/attributes";
            break;

        case FB_TYPE_VGA_PLANES:
            str = "EGA/VGA planes";
            break;

        default:
            str = "Unknown";
            break;
    };

    vmm_cdev_printf(cdev, "Type   : %s\n", str);

    switch (info->fix.visual) {
        case FB_VISUAL_MONO01:
            str = "Monochrome 1=Black 0=White";
            break;

        case FB_VISUAL_MONO10:
            str = "Monochrome 0=Black 1=White";
            break;

        case FB_VISUAL_TRUECOLOR:
            str = "True color";
            break;

        case FB_VISUAL_PSEUDOCOLOR:
            str = "Pseudo color";
            break;

        case FB_VISUAL_DIRECTCOLOR:
            str = "Direct color";
            break;

        case FB_VISUAL_STATIC_PSEUDOCOLOR:
            str = "Pseudo color readonly";
            break;

        default:
            str = "Unknown";
            break;
    };

    vmm_cdev_printf(cdev, "Visual : %s\n", str);

    vmm_cdev_printf(cdev, "Xres   : %d\n", info->var.xres);

    vmm_cdev_printf(cdev, "Yres   : %d\n", info->var.yres);

    vmm_cdev_printf(cdev, "BPP    : %d\n", info->var.bits_per_pixel);

    if (info->fix.visual == FB_VISUAL_TRUECOLOR || info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
        vmm_cdev_printf(cdev, "CMAP   : \n");

        for (i = info->cmap.start; i < info->cmap.len; i++) {
            vmm_cdev_printf(
                cdev,
                "  color%d: red=0x%x green=0x%x "
                "blue=0x%x\n",
                i, info->cmap.red[i], info->cmap.green[i], info->cmap.blue[i]);
        }

        vmm_cdev_printf(cdev, "\n");
    }

    return VMM_OK;
}

static int cmd_frame_buffer_fillrect(vmm_char_device_t *cdev, struct frame_buffer_info *info, int argc, char **argv)
{
    uint32_t                     color_start, color_len;
    struct frame_buffer_fillrect rect;

    if (argc < 5) {
        cmd_frame_buffer_usage(cdev);
        return VMM_EFAIL;
    }

    memset(&rect, 0, sizeof(struct frame_buffer_fillrect));
    rect.dx     = strtoul(argv[0], NULL, 10);
    rect.dy     = strtoul(argv[1], NULL, 10);
    rect.width  = strtoul(argv[2], NULL, 10);
    rect.height = strtoul(argv[3], NULL, 10);
    rect.color  = strtoul(argv[4], NULL, 16);

    if (info->var.xres <= rect.dx) {
        vmm_cdev_printf(cdev, "Error: x should be less than %d\n", info->var.xres);
        return VMM_EINVALID;
    }

    if (info->var.yres <= rect.dy) {
        vmm_cdev_printf(cdev, "Error: y should be less than %d\n", info->var.yres);
        return VMM_EINVALID;
    }

    if (info->var.xres <= (rect.dx + rect.width)) {
        vmm_cdev_printf(cdev, "Error: x+width should be less than %d\n", info->var.xres);
        return VMM_EINVALID;
    }

    if (info->var.yres <= (rect.dy + rect.height)) {
        vmm_cdev_printf(cdev, "Error: y+height should be less than %d\n", info->var.yres);
        return VMM_EINVALID;
    }

    if (info->fix.visual == FB_VISUAL_TRUECOLOR || info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
        color_start = info->cmap.start;
        color_len   = info->cmap.len;
    } else {
        color_start = 0;
        color_len   = (1 << info->var.bits_per_pixel);
    }

    if ((rect.color < color_start) || ((color_start + color_len) <= rect.color)) {
        vmm_cdev_printf(
            cdev,
            "Color error, it should be "
            "0x%x <= color < 0x%x\n",
            color_start, color_start + color_len);
        return VMM_EINVALID;
    }

    if (argc > 5) {
        rect.rop = strtol(argv[5], NULL, 10);
    }

    if (!info->fbops || !info->fbops->frame_buffer_fillrect) {
        vmm_cdev_printf(cdev, "FB fillrect operation not defined\n");
        return VMM_ENOTAVAIL;
    }

    vmm_cdev_printf(cdev, "X: %d, Y: %d, W: %d, H: %d, color: %d\n", rect.dx, rect.dy, rect.width, rect.height, rect.color);
    info->fbops->frame_buffer_fillrect(info, &rect);

    return VMM_OK;
}

static int cmd_frame_buffer_image(vmm_char_device_t *cdev, struct frame_buffer_info *info, int argc, char **argv)
{
    int                       err = VMM_OK;
    uint32_t                  w   = 0;
    uint32_t                  h   = 0;
    struct frame_buffer_image image;
    struct image_format       fmt;

    if (argc < 1) {
        cmd_frame_buffer_usage(cdev);
        return VMM_EFAIL;
    }

    memset(&image, 0, sizeof(image));
    memset(&fmt, 0, sizeof(fmt));

    fmt.bits_per_pixel   = info->var.bits_per_pixel;
    fmt.red.offset       = info->var.red.offset;
    fmt.red.length       = info->var.red.length;
    fmt.red.msb_right    = info->var.red.msb_right;
    fmt.blue.offset      = info->var.blue.offset;
    fmt.blue.length      = info->var.blue.length;
    fmt.blue.msb_right   = info->var.blue.msb_right;
    fmt.green.offset     = info->var.green.offset;
    fmt.green.length     = info->var.green.length;
    fmt.green.msb_right  = info->var.green.msb_right;
    fmt.transp.offset    = info->var.transp.offset;
    fmt.transp.length    = info->var.transp.length;
    fmt.transp.msb_right = info->var.transp.msb_right;

    if (VMM_OK != (err = image_load(argv[0], &fmt, &image))) {
        vmm_cdev_printf(cdev, "Error, failed to load image \"%s\" (%d)\n", argv[0], err);
        return err;
    }

    if (argc >= 2) {
        image.dx = strtol(argv[1], NULL, 10);
    } else if (image.width < info->var.xres) {
        image.dx = (info->var.xres - image.width) / 2;
    } else {
        image.dx = 0;
    }

    if (argc >= 3) {
        image.dy = strtol(argv[2], NULL, 10);
    } else if (image.height < info->var.yres) {
        image.dy = (info->var.yres - image.height) / 2;
    } else {
        image.dy = 0;
    }

    if (image.width < info->var.xres) {
        w = image.width;
    } else {
        w = info->var.xres - 1;
    }

    if (image.height < info->var.yres) {
        h = image.height;
    } else {
        h = info->var.yres - 1;
    }

    err = image_draw(info, &image, image.dx, image.dy, w, h);

    image_release(&image);

    return err;
}

static int cmd_frame_buffer_blank(vmm_char_device_t *cdev, struct frame_buffer_info *info, int argc, char **argv)
{
    int blank = 0;

    if (argc < 1) {
        cmd_frame_buffer_usage(cdev);
        return VMM_EFAIL;
    }

    if (!info->fbops || !info->fbops->fb_blank) {
        vmm_cdev_printf(cdev, "FB 'blank' operation not defined\n");
        return VMM_EFAIL;
    }

    blank = strtol(argv[0], NULL, 10);

    switch (blank) {
        case FB_BLANK_POWERDOWN:
            vmm_cdev_printf(cdev, "Setting '%s' blank to power down\n", info->name);
            break;

        case FB_BLANK_VSYNC_SUSPEND:
            vmm_cdev_printf(cdev, "Setting '%s' blank to vsync suspend\n", info->name);
            break;

        case FB_BLANK_HSYNC_SUSPEND:
            vmm_cdev_printf(cdev, "Setting '%s' blank to hsync suspend\n", info->name);
            break;

        case FB_BLANK_NORMAL:
            vmm_cdev_printf(cdev, "Setting '%s' blank to normal\n", info->name);
            break;

        case FB_BLANK_UNBLANK:
            vmm_cdev_printf(cdev, "Setting '%s' blank to unblank\n", info->name);
            break;
    }

    if (info->fbops->fb_blank(blank, info)) {
        return VMM_EFAIL;
    }

    return VMM_OK;
}

static int cmd_frame_buffer_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    struct frame_buffer_info *info = NULL;

    if (argc == 2) {
        if (strcmp(argv[1], "help") == 0) {
            cmd_frame_buffer_usage(cdev);
            return VMM_OK;
        } else if (strcmp(argv[1], "list") == 0) {
            cmd_frame_buffer_list(cdev);
            return VMM_OK;
        }
    }

    if (argc <= 2) {
        cmd_frame_buffer_usage(cdev);
        return VMM_EFAIL;
    }

    info = fb_find(argv[2]);

    if (!info) {
        vmm_cdev_printf(cdev, "Error: Invalid FB %s\n", argv[2]);
        return VMM_EFAIL;
    }

    if (strcmp(argv[1], "info") == 0) {
        return cmd_frame_buffer_info(cdev, info);
    } else if (0 == strcmp(argv[1], "blank")) {
        return cmd_frame_buffer_blank(cdev, info, argc - 3, argv + 3);
    } else if (0 == strcmp(argv[1], "fillrect")) {
        return cmd_frame_buffer_fillrect(cdev, info, argc - 3, argv + 3);
    } else if (0 == strcmp(argv[1], "image")) {
        return cmd_frame_buffer_image(cdev, info, argc - 3, argv + 3);
    }

    return VMM_EFAIL;
}

static vmm_command_t cmd_frame_buffer = {
    .name  = "fb",
    .desc  = "frame buffer commands",
    .usage = cmd_frame_buffer_usage,
    .exec  = cmd_frame_buffer_exec,
};

static int __init cmd_frame_buffer_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_frame_buffer);
}

static void __exit cmd_frame_buffer_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_frame_buffer);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
