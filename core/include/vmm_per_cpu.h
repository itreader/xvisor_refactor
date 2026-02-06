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
 * @file vmm_per_cpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for per-cpu areas
 */

#ifndef __VMM_PERCPU_H__
#define __VMM_PERCPU_H__

#include <vmm_types.h>

#define DEFINE_PER_CPU(type, name)  __per_cpu __typeof__(type) per_cpu_##name

#define DECLARE_PER_CPU(type, name) extern __typeof__(type) per_cpu__##name

#ifdef CONFIG_SMP

#include <arch_smp.h>

extern virtual_addr_t __per_cpu_offset[CONFIG_CPU_COUNT];

#define RELOC_HIDE(ptr, off) ({ (typeof(ptr))((virtual_addr_t)(ptr) + (off)); })

#define this_cpu(var)        (*RELOC_HIDE(&per_cpu_##var, __per_cpu_offset[arch_smp_id()]))

#define per_cpu(var, cpu)    (*RELOC_HIDE(&per_cpu_##var, __per_cpu_offset[(cpu)]))

#else

#define this_cpu(var)     per_cpu_##var

#define per_cpu(var, cpu) per_cpu_##var

#endif

#define get_cpu_var(var) this_cpu(var)

#define put_cpu_var(var)

/** Retrive per-cpu offset of current cpu */
virtual_addr_t vmm_per_cpu_current_offset(void);

/** Initialize per-cpu areas */
int vmm_per_cpu_init(void);

#endif /* __VMM_PERCPU_H__ */
