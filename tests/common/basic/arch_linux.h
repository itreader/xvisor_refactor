/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file arch_linux.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for arch specific linux booting
 */
#ifndef __ARCH_LINUX_H__
#define __ARCH_LINUX_H__

#include <arch_types.h>

void arch_start_linux_prep(uint64_t kernel_addr, uint64_t fdt_addr, uint64_t initrd_addr, uint64_t initrd_size);

void arch_start_linux_jump(uint64_t kernel_addr, uint64_t fdt_addr, uint64_t initrd_addr, uint64_t initrd_size);

#endif
