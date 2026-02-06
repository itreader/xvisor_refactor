/**
 * Copyright (c) 2002 Rusty Russell
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
 * @file kallsyms.c
 * @author Rusty Russell (rusty@rustcorp.com.au) IBM Corporation
 * @brief source file for trace support functions
 *
 * Changelog:
 * (25/Aug/2004) Paulo Marques (pmarques@grupopie.com)
 *  Changed the compression method from stem compression to "table lookup"
 *  compression.
 * (25/Nov/2011) Jean-Christophe Dubois (jcd@tribudubois.net)
 *  Adapted the file to xvisor
 */

#include <libs/kallsyms.h>
#include <libs/stringlib.h>
#include <vmm_stdio.h>

extern unsigned char _code_end;

/*
 * Expand a compressed symbol data into the resulting uncompressed string,
 * given the offset to where the symbol is in the compressed stream.
 */
__notrace uint32_t kallsyms_expand_symbol(uint32_t off, char *result)
{
    int                  len, skipped_first = 0;
    const unsigned char *tptr, *data;

    /* Get the compressed symbol length from the first symbol byte. */
    data = &kallsyms_names[off];
    len  = *data;
    data++;

    /*
     * Update the offset to return the offset for the next symbol on
     * the compressed stream.
     */
    off += len + 1;

    /*
     * For every byte on the compressed symbol data, copy the table
     * entry for that byte.
     */
    while (len) {
        tptr = &kallsyms_token_table[kallsyms_token_index[*data]];
        data++;
        len--;

        while (*tptr) {
            if (skipped_first) {
                *result = *tptr;
                result++;
            } else {
                skipped_first = 1;
            }

            tptr++;
        }
    }

    *result = '\0';

    /* Return to offset to the next symbol. */
    return off;
}

__notrace uint64_t kallsyms_get_symbol_pos(uint64_t addr, uint64_t *symbolsize, uint64_t *offset)
{
    uint64_t symbol_start = 0, symbol_end = 0;
    uint64_t i, low, high, mid;

    /* This kernel should never had been booted. */
    BUG_ON(!kallsyms_addresses);

    /* Do a binary search on the sorted kallsyms_addresses array. */
    low  = 0;
    high = kallsyms_num_syms;

    while (high - low > 1) {
        mid = low + (high - low) / 2;

        if (kallsyms_addresses[mid] <= addr) {
            low = mid;
        } else {
            high = mid;
        }
    }

    /*
     * Search for the first aliased symbol. Aliased
     * symbols are symbols with the same address.
     */
    while (low && kallsyms_addresses[low - 1] == kallsyms_addresses[low]) {
        --low;
    }

    symbol_start = kallsyms_addresses[low];

    /* Search for next non-aliased symbol. */
    for (i = low + 1; i < kallsyms_num_syms; i++) {
        if (kallsyms_addresses[i] > symbol_start) {
            symbol_end = kallsyms_addresses[i];
            break;
        }
    }

    /* If we found no next symbol, we use the end of the section. */
    if (!symbol_end) {
        symbol_end = (uint64_t)_code_end;
    }

    if (symbolsize) {
        *symbolsize = symbol_end - symbol_start;
    }

    if (offset) {
        *offset = addr - symbol_start;
    }

    return low;
}

/*
 * Find the offset on the compressed stream given and index in the
 * kallsyms array.
 */
__notrace uint32_t kallsyms_get_symbol_offset(uint64_t pos)
{
    const unsigned char *name;
    int                  i;

    /*
     * Use the closest marker we have. We have markers every 256 positions,
     * so that should be close enough.
     */
    name = &kallsyms_names[kallsyms_markers[pos >> 8]];

    /*
     * Sequentially scan all the symbols up to the point we're searching
     * for. Every symbol is stored in a [<len>][<len> bytes of data] format,
     * so we just need to add the len to the current pointer for every
     * symbol we wish to skip.
     */
    for (i = 0; i < (pos & 0xFF); i++) {
        name = name + (*name) + 1;
    }

    return name - kallsyms_names;
}

const __notrace char *kallsyms_lookup(uint64_t addr, uint64_t *symbolsize, uint64_t *offset, char *namebuf)
{
    uint64_t pos;

    namebuf[KSYM_NAME_LEN - 1] = 0;
    namebuf[0]                 = 0;

    pos                        = kallsyms_get_symbol_pos(addr, symbolsize, offset);
    /* Grab name */
    kallsyms_expand_symbol(kallsyms_get_symbol_offset(pos), namebuf);

    return namebuf;
}

/* Lookup the address for this symbol. Returns 0 if not found. */
__notrace uint64_t kallsyms_lookup_name(const char *name)
{
    char     namebuf[KSYM_NAME_LEN];
    uint64_t i;
    uint32_t off;

    for (i = 0, off = 0; i < kallsyms_num_syms; i++) {
        off = kallsyms_expand_symbol(off, namebuf);

        if (strcmp(namebuf, name) == 0) {
            return kallsyms_addresses[i];
        }
    }

    return 0;
}

__notrace int kallsyms_on_each_symbol(int (*fn)(void *, const char *, uint64_t), void *data)
{
    char     namebuf[KSYM_NAME_LEN];
    uint64_t i;
    uint32_t off;
    int      ret;

    for (i = 0, off = 0; i < kallsyms_num_syms; i++) {
        off = kallsyms_expand_symbol(off, namebuf);
        ret = fn(data, namebuf, kallsyms_addresses[i]);

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/*
 * Lookup an address but don't bother to find any names.
 */
__notrace int kallsyms_lookup_size_offset(uint64_t addr, uint64_t *symbolsize, uint64_t *offset)
{
    return !!kallsyms_get_symbol_pos(addr, symbolsize, offset);
}

/* Look up a kernel symbol and return it in a text buffer. */
static __notrace int __sprint_symbol(char *buffer, uint64_t address, int symbol_offset)
{
    const char *name;
    uint64_t    offset, size;
    int         len;

    address += symbol_offset;
    name = kallsyms_lookup(address, &size, &offset, buffer);

    if (!name) {
        return vmm_sprintf(buffer, "0x%lx", address);
    }

    if (name != buffer) {
        strcpy(buffer, name);
    }

    len = strlen(buffer);
    buffer += len;
    offset -= symbol_offset;

    len += vmm_sprintf(buffer, "+%#lx/%#lx", offset, size);

    return len;
}

/**
 * kallsyms_sprint_symbol - Look up a kernel symbol and return it in a text buffer
 * @buffer: buffer to be stored
 * @address: address to lookup
 *
 * This function looks up a kernel symbol with @address and stores its name,
 * offset, size and module name to @buffer if possible. If no symbol was found,
 * just saves its @address as is.
 *
 * This function returns the number of bytes stored in @buffer.
 */
__notrace int kallsyms_sprint_symbol(char *buffer, uint64_t address)
{
    return __sprint_symbol(buffer, address, 0);
}

/**
 * kallsyms_sprint_backtrace - Look up a backtrace symbol and return it in a text buffer
 * @buffer: buffer to be stored
 * @address: address to lookup
 *
 * This function is for stack backtrace and does the same thing as
 * sprint_symbol() but with modified/decreased @address. If there is a
 * tail-call to the function marked "noreturn", gcc optimized out code after
 * the call so that the stack-saved return address could point outside of the
 * caller. This function ensures that kallsyms will find the original caller
 * by decreasing @address.
 *
 * This function returns the number of bytes stored in @buffer.
 */
__notrace int kallsyms_sprint_backtrace(char *buffer, uint64_t address)
{
    return __sprint_symbol(buffer, address, -1);
}

__notrace int kallsyms_lookup_symbol_name(uint64_t addr, char *symname)
{
    uint64_t pos;
    symname[0]                 = '\0';
    symname[KSYM_NAME_LEN - 1] = '\0';

    pos                        = kallsyms_get_symbol_pos(addr, NULL, NULL);
    /* Grab name */
    kallsyms_expand_symbol(kallsyms_get_symbol_offset(pos), symname);
    return 0;
}

__notrace int kallsyms_lookup_symbol_attrs(uint64_t addr, uint64_t *size, uint64_t *offset, char *name)
{
    uint64_t pos;
    name[0]                 = '\0';
    name[KSYM_NAME_LEN - 1] = '\0';

    pos                     = kallsyms_get_symbol_pos(addr, size, offset);
    /* Grab name */
    kallsyms_expand_symbol(kallsyms_get_symbol_offset(pos), name);
    return 0;
}
