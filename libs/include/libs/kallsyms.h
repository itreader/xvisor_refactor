/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file kallsyms.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file for kallsyms related functions
 */

#ifndef __KALLSYMS_H_
#define __KALLSYMS_H_
#include <vmm_types.h>

/*
 * Max supported symbol length
 */
#define KSYM_NAME_LEN 128

/*
 * Tell the compiler that the count isn't in the small data section if the arch
 * has one (eg: FRV).
 */
extern const uint64_t       kallsyms_num_syms __attribute__((weak, section(".rodata")));
extern const uint64_t       kallsyms_addresses[] __attribute__((weak));
extern const unsigned char  kallsyms_names[] __attribute__((weak));
extern const unsigned char  kallsyms_token_table[] __attribute__((weak));
extern const unsigned short kallsyms_token_index[] __attribute__((weak));
extern const uint64_t       kallsyms_markers[] __attribute__((weak));

/* Lookup the address for a symbol. Returns 0 if not found. */
uint64_t kallsyms_lookup_name(const char *name);

uint32_t kallsyms_expand_symbol(uint32_t off, char *result);
uint32_t kallsyms_get_symbol_offset(uint64_t pos);

uint64_t kallsyms_get_symbol_pos(uint64_t addr, uint64_t *symbolsize, uint64_t *offset);

/* Call a function on each kallsyms symbol in the core kernel */
int kallsyms_on_each_symbol(int (*fn)(void *, const char *, uint64_t), void *data);

int kallsyms_lookup_size_offset(uint64_t addr, uint64_t *symbolsize, uint64_t *offset);

/* Lookup an address. */
const char *kallsyms_lookup(uint64_t addr, uint64_t *symbolsize, uint64_t *offset, char *namebuf);

/* Look up a kernel symbol and return it in a text buffer. */
int kallsyms_sprint_symbol(char *buffer, uint64_t address);
int kallsyms_sprint_backtrace(char *buffer, uint64_t address);

int kallsyms_lookup_symbol_name(uint64_t addr, char *symname);
int kallsyms_lookup_symbol_attrs(uint64_t addr, uint64_t *size, uint64_t *offset, char *name);

#endif
