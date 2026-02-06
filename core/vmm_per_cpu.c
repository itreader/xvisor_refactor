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
 * @file vmm_per_cpu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of per-cpu areas
 */

#include <arch_sections.h>
#include <libs/stringlib.h>
#include <vmm_cpumask.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_page_pool.h>
#include <vmm_per_cpu.h>

#ifdef CONFIG_SMP

virtual_addr_t __per_cpu_vaddr[CONFIG_CPU_COUNT]  = {0};
virtual_addr_t __per_cpu_offset[CONFIG_CPU_COUNT] = {0};

int __init vmm_per_cpu_init(void)
{
    uint32_t       cpu, pgcount;
    virtual_addr_t base = arch_per_cpu_vaddr();
    virtual_size_t size = arch_per_cpu_size();

    size                = VMM_ROUNDUP2_PAGE_SIZE(size);
    pgcount             = size / VMM_PAGE_SIZE;

    __per_cpu_vaddr[0]  = base;
    __per_cpu_offset[0] = 0;

    for_each_possible_cpu(cpu)
    {
        __per_cpu_vaddr[cpu] = vmm_page_pool_alloc(VMM_PAGE_POOL_NORMAL, pgcount);

        if (!__per_cpu_vaddr[cpu]) {
            return VMM_ENOMEM;
        }

        __per_cpu_offset[cpu] = __per_cpu_vaddr[cpu] - base;
        memset((void *)__per_cpu_vaddr[cpu], 0, VMM_PAGE_SIZE * pgcount);
    }

    return VMM_OK;
}

#else

int __init vmm_per_cpu_init(void)
{
    /* Don't require to do anything for UP */
    return VMM_OK;
}

#endif
