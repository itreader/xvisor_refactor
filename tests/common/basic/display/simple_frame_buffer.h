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
 * @file simple_frame_buffer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief SimpleFB driver source
 */
#ifndef __SIMPLE_FRAME_BUFFER_H__
#define __SIMPLE_FRAME_BUFFER_H__

#include <arch_types.h>

uint32_t        simple_frame_buffer_magic(virtual_addr_t base);
uint32_t        simple_frame_buffer_vendor(virtual_addr_t base);
uint32_t        simple_frame_buffer_version(virtual_addr_t base);
uint32_t        simple_frame_buffer_mode(virtual_addr_t base, char *mode, uint32_t mode_size);
uint32_t        simple_frame_buffer_width(virtual_addr_t base);
uint32_t        simple_frame_buffer_height(virtual_addr_t base);
uint32_t        simple_frame_buffer_stride(virtual_addr_t base);
physical_addr_t simple_frame_buffer_fb_base(virtual_addr_t base);
void            simple_frame_buffer_fdt_fixup(virtual_addr_t base, void *fdt_addr);

#endif
