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
 * @file cpu_smp_ops_default.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Default SMP operations
 */

#include <arch_barrier.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_smp.h>

#include <cpu_smp_ops.h>

static void __init smp_default_ops_init(void)
{
    /* For now nothing to do here. */
}

static int __init smp_default_cpu_init(vmm_device_tree_node_t *node, uint32_t cpu)
{
    /* For now nothing to do here. */
    return VMM_OK;
}

static int __init smp_default_cpu_prepare(uint32_t cpu)
{
    /* For now nothing to do here. */
    return VMM_OK;
}

static int __init smp_default_cpu_boot(uint32_t cpu)
{
    /* Update the pen release flag. */
    smp_write_pen_release(smp_logical_map(cpu));

    /* Wait for some-time */
    vmm_udelay(100000);

    /* Check pen value */
    if (smp_read_pen_release() != HARTID_INVALID) {
        return VMM_ENOSYS;
    }

    return VMM_OK;
}

static void __cpuinit smp_default_cpu_postboot(void)
{
    /* Let the primary processor know we're out of the pen. */
    smp_write_pen_release(HARTID_INVALID);
}

struct smp_operations smp_default_ops = {
    .name         = "default",
    .ops_init     = smp_default_ops_init,
    .cpu_init     = smp_default_cpu_init,
    .cpu_prepare  = smp_default_cpu_prepare,
    .cpu_boot     = smp_default_cpu_boot,
    .cpu_postboot = smp_default_cpu_postboot,
};
