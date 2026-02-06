/**
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
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
 * @file arch_math.h
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Architecture specific math related functions
 */

#ifndef __ARCH_MATH_H__
#define __ARCH_MATH_H__

#include <vmm_types.h>

static inline uint64_t arch_udiv64(uint64_t value, uint64_t divisor)
{
    return value / divisor;
}

static inline uint64_t arch_umod64(uint64_t value, uint64_t divisor)
{
    return value % divisor;
}

static inline int64_t arch_sdiv64(int64_t value, int64_t divisor)
{
    return value / divisor;
}

static inline int64_t arch_smod64(int64_t value, int64_t divisor)
{
    return value % divisor;
}

static inline uint32_t arch_udiv32(uint32_t value, uint32_t divisor)
{
    return value / divisor;
}

static inline uint32_t arch_umod32(uint32_t value, uint32_t divisor)
{
    return value % divisor;
}

static inline int32_t arch_sdiv32(int32_t value, int32_t divisor)
{
    return value / divisor;
}

static inline int32_t arch_smod32(int32_t value, int32_t divisor)
{
    return value % divisor;
}

#endif /* __ARCH_MATH_H__ */
