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
 * @file arch_sections.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief interface for accessing VMM sections
 */
#ifndef _ARCH_SECTIONS_H__
#define _ARCH_SECTIONS_H__

#include <vmm_types.h>

/** Overall code */
virtual_addr_t  arch_code_vaddr_start(void);
physical_addr_t arch_code_paddr_start(void);
virtual_size_t  arch_code_size(void);

/** Exception table */
extern uint8_t __start___ex_table;
extern uint8_t __stop___ex_table;

static inline virtual_addr_t arch_exception_table_start(void)
{
    return (virtual_addr_t)&__start___ex_table;
}

static inline virtual_addr_t arch_exception_table_end(void)
{
    return (virtual_size_t)&__stop___ex_table;
}

/** Module table */
extern uint8_t _modtable_start;
extern uint8_t _modtable_end;

static inline virtual_addr_t arch_modtable_vaddr(void)
{
    return (virtual_addr_t)&_modtable_start;
}

static inline virtual_size_t arch_modtable_size(void)
{
    return (virtual_size_t)(&_modtable_end - &_modtable_start);
}

/** PerCPU section */
extern uint8_t _per_cpu_start;
extern uint8_t _per_cpu_end;

static inline virtual_addr_t arch_per_cpu_vaddr(void)
{
    return (virtual_addr_t)&_per_cpu_start;
}

static inline virtual_size_t arch_per_cpu_size(void)
{
    return (virtual_size_t)(&_per_cpu_end - &_per_cpu_start);
}

/** Init section */
extern uint8_t _init_start;
extern uint8_t _init_end;

static inline virtual_addr_t arch_init_vaddr(void)
{
    return (virtual_addr_t)(&_init_start);
}

static inline virtual_size_t arch_init_size(void)
{
    return (virtual_size_t)(&_init_end - &_init_start);
}

/** Device tree nodeid table */
extern uint8_t _nidtable_start;
extern uint8_t _nidtable_end;

static inline virtual_addr_t arch_nidtable_vaddr(void)
{
    return (virtual_addr_t)&_nidtable_start;
}

static inline virtual_size_t arch_nidtable_size(void)
{
    return (virtual_size_t)(&_nidtable_end - &_nidtable_start);
}

#endif
