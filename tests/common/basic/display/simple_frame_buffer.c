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
 * @file simple_frame_buffer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SimpleFB driver source
 */

#include <arch_io.h>
#include <display/simple_frame_buffer.h>
#include <libfdt/fdt_support.h>

#define SIMPLE_FRAME_BUFFER_MAGIC_OFFSET      0x00
#define SIMPLE_FRAME_BUFFER_VENDOR_OFFSET     0x04
#define SIMPLE_FRAME_BUFFER_VERSION_OFFSET    0x08
#define SIMPLE_FRAME_BUFFER_MODE_OFFSET       0x10
#define SIMPLE_FRAME_BUFFER_WIDTH_OFFSET      0x50
#define SIMPLE_FRAME_BUFFER_HEIGHT_OFFSET     0x54
#define SIMPLE_FRAME_BUFFER_STRIDE_OFFSET     0x58
#define SIMPLE_FRAME_BUFFER_FB_BASE_MS_OFFSET 0x5c
#define SIMPLE_FRAME_BUFFER_FB_BASE_LS_OFFSET 0x60

uint32_t simple_frame_buffer_magic(virtual_addr_t base)
{
    return arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_MAGIC_OFFSET));
}

uint32_t simple_frame_buffer_vendor(virtual_addr_t base)
{
    return arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_VENDOR_OFFSET));
}

uint32_t simple_frame_buffer_version(virtual_addr_t base)
{
    return arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_VERSION_OFFSET));
}

uint32_t simple_frame_buffer_mode(virtual_addr_t base, char *mode, uint32_t mode_size)
{
    uint32_t       len;
    virtual_addr_t mbase = base + SIMPLE_FRAME_BUFFER_MODE_OFFSET;

    if (!mode || !mode_size) {
        return 0;
    }

    if (mode_size > 16) {
        mode_size = 16;
    }

    len = 0;

    while (len < mode_size) {
        mode[len] = arch_readl((void *)(mbase + len * 0x4)) & 0xff;
        len++;
    }

    mode[mode_size - 1] = '\0';

    return len;
}

uint32_t simple_frame_buffer_width(virtual_addr_t base)
{
    return arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_WIDTH_OFFSET));
}

uint32_t simple_frame_buffer_height(virtual_addr_t base)
{
    return arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_HEIGHT_OFFSET));
}

uint32_t simple_frame_buffer_stride(virtual_addr_t base)
{
    return arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_STRIDE_OFFSET));
}

physical_addr_t simple_frame_buffer_fb_base(virtual_addr_t base)
{
    uint32_t ms = arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_FB_BASE_MS_OFFSET));
    uint32_t ls = arch_readl((void *)(base + SIMPLE_FRAME_BUFFER_FB_BASE_LS_OFFSET));

    return (physical_addr_t)(((uint64_t)ms << 32) | ((uint64_t)ls));
}

void simple_frame_buffer_fdt_fixup(virtual_addr_t base, void *fdt_addr)
{
    char     mode[16];
    uint32_t width, height, stride, mode_len;

    mode_len = simple_frame_buffer_mode(base, mode, sizeof(mode));

    if (!mode_len) {
        return;
    }

    width  = simple_frame_buffer_width(base);
    height = simple_frame_buffer_height(base);
    stride = simple_frame_buffer_stride(base);

    do_fixup_by_compat(fdt_addr, "simple-framebuffer", "format", mode, mode_len, 1);
    do_fixup_by_compat_u32(fdt_addr, "simple-framebuffer", "width", width, 1);
    do_fixup_by_compat_u32(fdt_addr, "simple-framebuffer", "height", height, 1);
    do_fixup_by_compat_u32(fdt_addr, "simple-framebuffer", "stride", stride, 1);
}
