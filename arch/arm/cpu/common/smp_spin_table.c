/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file smp_spin_table.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Spin Table SMP operations
 *
 * Adapted from linux/arch/arm64/kernel/smp_spin_table.c
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * The original source is licensed under GPL.
 */

#include <arch_barrier.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_smp.h>

#include <cpu_defines.h>
#include <smp_ops.h>

static physical_addr_t clear_addr[CONFIG_CPU_COUNT];
static physical_addr_t release_addr[CONFIG_CPU_COUNT];

static void __init smp_spin_table_ops_init(void)
{
    /* For now nothing to do here. */
}

static int __init smp_spin_table_cpu_init(vmm_device_tree_node_t *node, uint32_t cpu)
{
    int             rc;
    physical_addr_t pa;

    /* Map release address */
    rc = vmm_device_tree_read_physaddr(node, VMM_DEVICE_TREE_CPU_RELEASE_ADDR_ATTR_NAME, &pa);

    if (rc) {
        release_addr[cpu] = 0x0;
    } else {
        release_addr[cpu] = pa;
    }

    /* Map clear address */
    rc = vmm_device_tree_read_physaddr(node, VMM_DEVICE_TREE_CPU_CLEAR_ADDR_ATTR_NAME, &pa);

    if (rc) {
        clear_addr[cpu] = 0x0;
    } else {
        clear_addr[cpu] = pa;
    }

    return VMM_OK;
}

extern uint8_t _start_secondary;

static int __init smp_spin_table_cpu_prepare(uint32_t cpu)
{
    int rc;
#ifdef CONFIG_ARM64
    uint64_t val = 0;
#else
    uint32_t val = 0;
#endif
    physical_addr_t _start_secondary_pa;
#ifndef CONFIG_ARM64
    const vmm_cpumask_t *mask = get_cpu_mask(cpu);
#endif

    /* Get physical address secondary startup code */
    rc = vmm_host_va2pa((virtual_addr_t)&_start_secondary, &_start_secondary_pa);

    if (rc) {
        return rc;
    }

    /* Write to clear address */
    if (clear_addr[cpu]) {
        arch_wmb();
        val = ~0x0;
        vmm_host_memory_write(clear_addr[cpu], &val, sizeof(val), FALSE);
    }

    /* Write to release address */
    if (release_addr[cpu]) {
        arch_wmb();
        val = _start_secondary_pa;
        vmm_host_memory_write(release_addr[cpu], &val, sizeof(val), FALSE);
    }

#ifdef CONFIG_ARM64
    /* Send an event to wake up the secondary CPU. */
    asm volatile("sev");
#else
    /* Wakeup target cpu from wfe/wfi by sending an IPI */
    vmm_host_irq_raise(0, mask);
#endif

    return VMM_OK;
}

static int __init smp_spin_table_cpu_boot(uint32_t cpu)
{
    /* Update the pen release flag. */
    smp_write_pen_release(smp_logical_map(cpu));

    /* Send an event to wake up the secondary CPU. */
    asm volatile("sev");

    /* Wait for some-time */
    vmm_udelay(100000);

    /* Check pen value */
    if (smp_read_pen_release() != MPIDR_INVALID) {
        return VMM_ENOSYS;
    }

    return VMM_OK;
}

static void __cpuinit smp_spin_table_cpu_postboot(void)
{
    /* Let the primary processor know we're out of the pen. */
    smp_write_pen_release(MPIDR_INVALID);
}

static struct smp_operations smp_spin_table_ops = {
    .name         = "spin-table",
    .ops_init     = smp_spin_table_ops_init,
    .cpu_init     = smp_spin_table_cpu_init,
    .cpu_prepare  = smp_spin_table_cpu_prepare,
    .cpu_boot     = smp_spin_table_cpu_boot,
    .cpu_postboot = smp_spin_table_cpu_postboot,
};
SMP_OPS_DECLARE(smp_spin_table, &smp_spin_table_ops);
