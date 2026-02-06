/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file stack_trace.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Library for stack-tracing
 */

#include <libs/kallsyms.h>
#include <libs/stack_trace.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>

/* Dummy implementation of arch_save_stack_trace to make
 * stack_trace arch support optional.
 */
void __weak arch_save_stack_trace(struct stack_trace *trace) {}

void print_stack_trace(struct stack_trace *trace)
{
    int  i;
    char symname[KSYM_NAME_LEN];

    if (WARN_ON(!trace->entries)) {
        return;
    }

    for (i = 0; i < trace->nr_entries; i++) {
        kallsyms_sprint_symbol(symname, trace->entries[i]);
        vmm_printf("0x%08lX %s\n", trace->entries[i], symname);
    }
}

void dump_stack_trace(void)
{
    struct stack_trace trace;
    uint64_t           entries[12];

    trace.nr_entries  = 0;
    trace.max_entries = 12;
    trace.entries     = entries;
    trace.skip        = 2;

    arch_save_stack_trace(&trace);
    print_stack_trace(&trace);
}
