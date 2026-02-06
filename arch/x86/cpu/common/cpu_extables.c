/**
 * Copyright (c) 2015 Himanshu Chauhan.
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
 * @file cpu_exception_tables.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief VMM exception fixup handler.
 */

#include <cpu_exception_tables.h>

static inline uint64_t ex_insn_addr(const struct vmm_exception_table_entry *x)
{
    return (uint64_t)&x->insn + x->insn;
}

static inline uint64_t ex_fixup_addr(const struct vmm_exception_table_entry *x)
{
    return (uint64_t)&x->fixup + x->fixup;
}

int fixup_exception(struct arch_regs *regs)
{
    const struct vmm_exception_table_entry *fixup;
    // uint64_t new_ip;

    fixup = vmm_exception_table_search(regs->rip);

    if (fixup) {
        // new_ip = ex_fixup_addr(fixup);
        regs->rip = fixup->fixup;  // new_ip;

        return 1;
    }

    return 0;
}
