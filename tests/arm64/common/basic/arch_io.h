/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file arch_io.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for common I/O functions
 */

#ifndef __ARCH_IO_H__
#define __ARCH_IO_H__

#include <arch_types.h>

static inline uint32_t arch_readl(void *addr)
{
    return *((uint32_t *)addr);
}

static inline void arch_writel(uint32_t data, void *addr)
{
    *((uint32_t *)addr) = data;
}

static inline uint16_t arch_readw(void *addr)
{
    return *((uint16_t *)addr);
}

static inline void arch_writew(uint16_t data, void *addr)
{
    *((uint16_t *)addr) = data;
}

static inline uint8_t arch_readb(void *addr)
{
    return *((uint8_t *)addr);
}

static inline void arch_writeb(uint8_t data, void *addr)
{
    *((uint8_t *)addr) = data;
}

#endif /* __ARCH_IO_H__ */
