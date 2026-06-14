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
 * @file vmm_pixel_ops.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 像素转换辅助API头文件
 *
 * The header has been largely adapted from QEMU sources:
 * include/ui/pixel_ops.h
 *
 * The original source is licensed under GPL.
 */

#ifndef __VMM_PIXEL_OPS_H_
#define __VMM_PIXEL_OPS_H_
#include "vmm_types.h"

static inline uint32_t rgb_to_pixel8(uint32_t r, uint32_t g, uint32_t b)
{
    return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
}

static inline uint32_t rgb_to_pixel15(uint32_t r, uint32_t g, uint32_t b)
{
    return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}

static inline uint32_t rgb_to_pixel15bgr(uint32_t r, uint32_t g, uint32_t b)
{
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
}

static inline uint32_t rgb_to_pixel16(uint32_t r, uint32_t g, uint32_t b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline uint32_t rgb_to_pixel16bgr(uint32_t r, uint32_t g, uint32_t b)
{
    return ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
}

static inline uint32_t rgb_to_pixel24(uint32_t r, uint32_t g, uint32_t b)
{
    return (r << 16) | (g << 8) | b;
}

static inline uint32_t rgb_to_pixel24bgr(uint32_t r, uint32_t g, uint32_t b)
{
    return (b << 16) | (g << 8) | r;
}

static inline uint32_t rgb_to_pixel32(uint32_t r, uint32_t g, uint32_t b)
{
    return (r << 16) | (g << 8) | b;
}

static inline uint32_t rgb_to_pixel32bgr(uint32_t r, uint32_t g, uint32_t b)
{
    return (b << 16) | (g << 8) | r;
}

#endif /* __VMM_PIXEL_OPS_H_ */
