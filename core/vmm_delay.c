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
 * @file vmm_delay.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for soft delay subsystem
 */

#include <arch_cpu.h>
#include <arch_delay.h>
#include <libs/mathlib.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_waitqueue.h>

static uint64_t loops_per_msec[CONFIG_CPU_COUNT];
static uint64_t loops_per_usec[CONFIG_CPU_COUNT];
static uint64_t loops_per_nsec[CONFIG_CPU_COUNT];

static void nanosec_sleep(uint64_t nsecs)
{
    int              rc;
    vmm_wait_queue_t wait_queue;

    INIT_WAITQUEUE(&wait_queue, NULL);

    rc = vmm_waitqueue_sleep_timeout(&wait_queue, &nsecs);

    if (rc != VMM_ETIMEDOUT) {
        vmm_printf("%s: sleep timeout failed (error %d)\n", __func__, rc);
        WARN_ON(1);
    }
}

void vmm_usleep(uint64_t usecs)
{
    nanosec_sleep((uint64_t)usecs * 1000ULL);
}

void vmm_msleep(uint64_t msecs)
{
    nanosec_sleep((uint64_t)msecs * 1000000ULL);
}

void vmm_ssleep(uint64_t secs)
{
    nanosec_sleep((uint64_t)secs * 1000000000ULL);
}

void vmm_ndelay(uint64_t nsecs)
{
    uint64_t    lpnsec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpnsec = loops_per_nsec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    arch_delay_loop(nsecs * lpnsec);
}

void vmm_udelay(uint64_t usecs)
{
    uint64_t    lpusec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpusec = loops_per_usec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    arch_delay_loop(usecs * lpusec);
}

void vmm_mdelay(uint64_t msecs)
{
    uint64_t    lpmsec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpmsec = loops_per_msec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    arch_delay_loop(msecs * lpmsec);
}

void vmm_sdelay(uint64_t secs)
{
    uint32_t    i;
    uint64_t    lpmsec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpmsec = loops_per_msec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    for (i = 0; i < secs; i++) {
        arch_delay_loop(1000 * lpmsec);
    }
}

uint64_t vmm_delay_estimate_cpu_mhz(uint32_t cpu)
{
    return arch_delay_loop_cycles(loops_per_usec[cpu]);
}

uint64_t vmm_delay_estimate_cpu_khz(uint32_t cpu)
{
    return arch_delay_loop_cycles(loops_per_msec[cpu]);
}

void vmm_delay_recaliberate(void)
{
    uint64_t    nsecs, tstamp;
    irq_flags_t flags;
    uint32_t    cpu = vmm_smp_processor_id();

    arch_cpu_irq_save(flags);

    tstamp = vmm_timer_timestamp();

    arch_delay_loop(1000000);

    nsecs               = vmm_timer_timestamp() - tstamp;

    loops_per_nsec[cpu] = udiv64(1000000ULL, nsecs);
    loops_per_usec[cpu] = udiv64(1000ULL * 1000000ULL, nsecs);
    loops_per_msec[cpu] = udiv64(1000000ULL * 1000000ULL, nsecs);

    arch_cpu_irq_restore(flags);
}

static int delay_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    vmm_delay_recaliberate();

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t delay_cpu_hotplug = {
    .name    = "DELAY",
    .state   = VMM_CPU_HOTPLUG_STATE_DELAY,
    .startup = delay_startup,
};

int __init vmm_delay_init(void)
{
    uint32_t i;

    /* Clear everything */
    for (i = 0; i < CONFIG_CPU_COUNT; i++) {
        loops_per_msec[i] = 0;
        loops_per_usec[i] = 0;
    }

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&delay_cpu_hotplug, TRUE);
}
