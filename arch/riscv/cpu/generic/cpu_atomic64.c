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
 * @file cpu_atomic64.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V specific 64 bits synchronization mechanisms.
 */

#include <arch_atomic64.h>
#include <arch_barrier.h>
#include <arch_cpu_irq.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_types.h>

#include <riscv_lrsc.h>

uint64_t __lock arch_atomic64_read(atomic64_t *atom)
{
    uint64_t ret = (*(volatile long *)&atom->counter);
    arch_rmb();
    return ret;
}

void __lock arch_atomic64_write(atomic64_t *atom, uint64_t value)
{
    atom->counter = value;
    arch_wmb();
}

#ifndef CONFIG_64BIT
void __lock arch_atomic64_add(atomic64_t *atom, uint64_t value)
{
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    atom->counter += value;
    arch_cpu_irq_restore(flags);
}

void __lock arch_atomic64_sub(atomic64_t *atom, uint64_t value)
{
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    atom->counter -= value;
    arch_cpu_irq_restore(flags);
}

uint64_t __lock arch_atomic64_add_return(atomic64_t *atom, uint64_t value)
{
    uint64_t    temp;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    atom->counter += value;
    temp = atom->counter;
    arch_cpu_irq_restore(flags);

    return temp;
}

uint64_t __lock arch_atomic64_sub_return(atomic64_t *atom, uint64_t value)
{
    uint64_t    temp;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    atom->counter -= value;
    temp = atom->counter;
    arch_cpu_irq_restore(flags);

    return temp;
}

uint64_t __lock arch_atomic64_xchg(atomic64_t *atom, uint64_t newval)
{
    uint64_t    previous;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    previous      = atom->counter;
    atom->counter = newval;
    arch_cpu_irq_restore(flags);

    return previous;
}

uint64_t __lock arch_atomic64_cmpxchg(atomic64_t *atom, uint64_t oldval, uint64_t newval)
{
    uint64_t    previous;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    previous = atom->counter;

    if (previous == oldval) {
        atom->counter = newval;
    }

    arch_cpu_irq_restore(flags);

    return previous;
}
#else

void __lock arch_atomic64_add(atomic64_t *atom, uint64_t value)
{
    __asm__ __volatile__("	amoadd.d zero, %1, %0" : "+A"(atom->counter) : "r"(value) : "memory");
}

void __lock arch_atomic64_sub(atomic64_t *atom, uint64_t value)
{
    __asm__ __volatile__("	amoadd.d zero, %1, %0" : "+A"(atom->counter) : "r"(-value) : "memory");
}

uint64_t __lock arch_atomic64_add_return(atomic64_t *atom, uint64_t value)
{
    uint64_t ret;

    __asm__ __volatile__("	amoadd.d.aqrl  %1, %2, %0" : "+A"(atom->counter), "=r"(ret) : "r"(value) : "memory");

    return ret + value;
}

uint64_t __lock arch_atomic64_sub_return(atomic64_t *atom, uint64_t value)
{
    uint64_t ret;

    __asm__ __volatile__("	amoadd.d.aqrl  %1, %2, %0" : "+A"(atom->counter), "=r"(ret) : "r"(-value) : "memory");

    return ret - value;
}

uint64_t __lock arch_atomic64_xchg(atomic64_t *atom, uint64_t newval)
{
    return xchg(&atom->counter, newval);
}

uint64_t __lock arch_atomic64_cmpxchg(atomic64_t *atom, uint64_t oldval, uint64_t newval)
{
    return cmpxchg(&atom->counter, oldval, newval);
}
#endif
