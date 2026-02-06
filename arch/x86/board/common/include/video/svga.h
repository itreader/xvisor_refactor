/**
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file fb_console.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Framebuffer console header file.
 */

#ifndef SVGA_H
#define SVGA_H

#include <vmm_host_address_space.h>
#include <vmm_types.h>

#define SVGA_DEFAULT_MODE 0x117

/* RRRRR GGGGGG BBBBB */
#define SVGA_24TO16BPP(x) ((x & 0xF80000) >> 8) | ((x & 0xFC00) >> 5) | ((x & 0xF8) >> 3)

typedef struct svga_mode_info {
    uint16_t attributes;
    uint8_t  windowA, windowB;
    uint16_t granularity;
    uint16_t windowSize;
    uint16_t segmentA, segmentB;
    uint32_t winFuncPtr;                       /* ptr to INT 0x10 Function 0x4F05 */
    uint16_t pitch;                            /* bytes per scan line */

    uint16_t screen_width, screen_height;      /* resolution */
    uint8_t  wChar, yChar, planes, bpp, banks; /* number of banks */
    uint8_t  memoryModel, bankSize, imagePages;
    uint8_t  reserved0;

    // color masks
    uint8_t readMask, redPosition;
    uint8_t greenMask, greenPosition;
    uint8_t blueMask, bluePosition;
    uint8_t reservedMask, reservedPosition;
    uint8_t directColorAttributes;

    uint32_t physbase; /* pointer to LFB in LFB modes */
    uint32_t offScreenMemOff;
    uint16_t offScreenMemSize;
    uint8_t  reserved1[206];
} __attribute__((packed)) svga_mode_info_t;

void              svga_change_mode(uint16_t);
svga_mode_info_t *svga_mode_get_info(uint16_t);
virtual_addr_t    svga_map_fb(physical_addr_t, virtual_size_t);

#endif
