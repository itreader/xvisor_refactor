/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of timer subsystem
 */

#include <arch_cpu_irq.h>
#include <libs/stringlib.h>
#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_per_cpu.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>

/** Control structure for Timer Subsystem */
struct vmm_timer_local_ctrl {
    vmm_timecounter_t  tc;
    vmm_clock_chip_t  *cc;
    bool               started;
    bool               inprocess;
    uint64_t           next_event;
    vmm_timer_event_t *curr;
    vmm_rwlock_t       event_list_lock;
    double_list_t      event_list;
};

static DEFINE_PER_CPU(struct vmm_timer_local_ctrl, tlc);

static vmm_timecounter_t ref_tc;

uint32_t vmm_timer_clocksource_frequency(void)
{
    return vmm_timecounter_clocksource_frequency(&this_cpu(tlc).tc);
}

uint32_t vmm_timer_clock_chip_frequency(void)
{
    return vmm_clock_chip_frequency(this_cpu(tlc).cc);
}

#if defined(CONFIG_PROFILE)
uint64_t __notrace vmm_timer_timestamp_for_profile(void)
{
    return vmm_timecounter_read_for_profile(&this_cpu(tlc).tc);
}
#endif

uint64_t vmm_timer_cycles_to_ns(uint64_t cycles)
{
    irq_flags_t        flags;
    uint64_t           nanosecs;
    vmm_timecounter_t *tc = &this_cpu(tlc).tc;

    arch_cpu_irq_save(flags);
    nanosecs = vmm_clocksource_delta2nsecs(cycles & tc->cs->mask, tc->cs->mult, tc->cs->shift);
    arch_cpu_irq_restore(flags);

    return nanosecs;
}

uint64_t vmm_timer_delta_cycles_to_ns(uint64_t cycles)
{
    irq_flags_t        flags;
    uint64_t           ns_delta, cycles_now, cycles_delta;
    vmm_timecounter_t *tc = &this_cpu(tlc).tc;

    arch_cpu_irq_save(flags);
    cycles_now = tc->cs->read(tc->cs);

    if (cycles > cycles_now) {
        cycles_delta = (cycles - cycles_now) & tc->cs->mask;
    } else {
        cycles_delta = 0;
    }

    ns_delta = vmm_clocksource_delta2nsecs(cycles_delta, tc->cs->mult, tc->cs->shift);
    arch_cpu_irq_restore(flags);

    return ns_delta;
}

uint64_t vmm_timer_timestamp(void)
{
    uint64_t    ret;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    ret = vmm_timecounter_read(&this_cpu(tlc).tc);
    arch_cpu_irq_restore(flags);

    return ret;
}

/* Note: This function must be called with tlcp->event_list_lock held. */
static void __timer_schedule_next_event(struct vmm_timer_local_ctrl *tlcp)
{
    uint64_t           tstamp;
    vmm_timer_event_t *e;

    /* If not started yet or still processing events then we give up */
    if ((tlcp->started == FALSE) || (tlcp->inprocess == TRUE)) {
        return;
    }

    /* If no events, we give up */
    if (list_empty(&tlcp->event_list)) {
        return;
    }

    /* Retrieve first event from list of active events */
    e          = list_entry(list_first(&tlcp->event_list), vmm_timer_event_t, active_head);

    /* Configure clockevent device for first event */
    tlcp->curr = e;
    tstamp     = vmm_timer_timestamp();

    if (tstamp < e->expiry_tstamp) {
        tlcp->next_event = e->expiry_tstamp;
        vmm_clock_chip_program_event(tlcp->cc, tstamp, e->expiry_tstamp);
    } else {
        tlcp->next_event = tstamp;
        vmm_clock_chip_program_event(tlcp->cc, tstamp, tstamp);
    }
}

/* Note: This function must be called with ev->active_lock held. */
static void __timer_event_stop(vmm_timer_event_t *ev)
{
    irq_flags_t                  flags;
    struct vmm_timer_local_ctrl *tlcp;

    if (!ev->active_state) {
        return;
    }

    tlcp = &per_cpu(tlc, ev->active_hcpu);

    vmm_write_lock_irq_save_lite(&tlcp->event_list_lock, flags);

    ev->active_state = FALSE;
    list_del(&ev->active_head);
    ev->expiry_tstamp = 0;

    vmm_write_unlock_irq_restore_lite(&tlcp->event_list_lock, flags);
}

/* This is called from interrupt context. We need to protect the
 * event list when manipulating it.
 */
static void timer_clock_chip_event_handler(vmm_clock_chip_t *cc)
{
    irq_flags_t                  flags, flags1;
    vmm_timer_event_t           *e;
    struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

    vmm_read_lock_irq_save_lite(&tlcp->event_list_lock, flags);

    tlcp->inprocess = TRUE;

    /* Process expired active events */
    while (!list_empty(&tlcp->event_list)) {
        e = list_entry(list_first(&tlcp->event_list), vmm_timer_event_t, active_head);

        /* Current timestamp */
        if (e->expiry_tstamp <= vmm_timer_timestamp()) {
            /* Unlock event list for processing expired event */
            vmm_read_unlock_irq_restore_lite(&tlcp->event_list_lock, flags);
            /* Set current CPU event to NULL */
            tlcp->curr = NULL;
            /* Stop expired active event */
            vmm_spin_lock_irq_save_lite(&e->active_lock, flags1);
            __timer_event_stop(e);
            vmm_spin_unlock_irq_restore_lite(&e->active_lock, flags1);
            /* Call event handler */
            e->handler(e);
            /* Lock back event list */
            vmm_read_lock_irq_save_lite(&tlcp->event_list_lock, flags);
        } else {
            /* No more expired events */
            break;
        }
    }

    tlcp->inprocess = FALSE;

    /* Schedule next timer event */
    __timer_schedule_next_event(tlcp);

    vmm_read_unlock_irq_restore_lite(&tlcp->event_list_lock, flags);
}

bool vmm_timer_event_pending(vmm_timer_event_t *ev)
{
    bool        ret;
    irq_flags_t flags;

    if (!ev) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&ev->active_lock, flags);
    ret = ev->active_state;
    vmm_spin_unlock_irq_restore_lite(&ev->active_lock, flags);

    return ret;
}

uint64_t vmm_timer_event_expiry_time(vmm_timer_event_t *ev)
{
    uint64_t    exp_time;
    irq_flags_t flags;

    if (!ev) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&ev->active_lock, flags);
    exp_time = ev->expiry_tstamp;
    vmm_spin_unlock_irq_restore_lite(&ev->active_lock, flags);

    return exp_time;
}

int vmm_timer_event_start2(vmm_timer_event_t *ev, uint64_t duration_nsecs, uint64_t *ret_expiry_tstamp)
{
    uint32_t                     host_cpu;
    uint64_t                     tstamp;
    bool                         found_pos = FALSE;
    irq_flags_t                  flags, flags1;
    vmm_timer_event_t           *e = NULL;
    struct vmm_timer_local_ctrl *tlcp;

    if (!ev) {
        return VMM_EFAIL;
    }

    host_cpu = vmm_smp_processor_id();
    tlcp     = &per_cpu(tlc, host_cpu);
    tstamp   = vmm_timer_timestamp();

    vmm_spin_lock_irq_save_lite(&ev->active_lock, flags);

    __timer_event_stop(ev);

    ev->expiry_tstamp  = tstamp + duration_nsecs;
    ev->duration_nsecs = duration_nsecs;
    ev->active_state   = TRUE;
    ev->active_hcpu    = host_cpu;

    if (ret_expiry_tstamp) {
        *ret_expiry_tstamp = ev->expiry_tstamp;
    }

    vmm_write_lock_irq_save_lite(&tlcp->event_list_lock, flags1);

    list_for_each_entry(e, &tlcp->event_list, active_head)
    {
        if (ev->expiry_tstamp < e->expiry_tstamp) {
            found_pos = TRUE;
            break;
        }
    }

    if (!found_pos) {
        list_add_tail(&ev->active_head, &tlcp->event_list);
    } else {
        list_add_tail(&ev->active_head, &e->active_head);
    }

    __timer_schedule_next_event(tlcp);

    vmm_write_unlock_irq_restore_lite(&tlcp->event_list_lock, flags1);

    vmm_spin_unlock_irq_restore_lite(&ev->active_lock, flags);

    return VMM_OK;
}

int vmm_timer_event_restart(vmm_timer_event_t *ev)
{
    if (!ev) {
        return VMM_EFAIL;
    }

    return vmm_timer_event_start(ev, ev->duration_nsecs);
}

int vmm_timer_event_stop(vmm_timer_event_t *ev)
{
    irq_flags_t flags;

    if (!ev) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save_lite(&ev->active_lock, flags);

    __timer_event_stop(ev);

    vmm_spin_unlock_irq_restore_lite(&ev->active_lock, flags);

    return VMM_OK;
}

bool vmm_timer_started(void)
{
    return this_cpu(tlc).started;
}

void vmm_timer_start(void)
{
    uint64_t                     tstamp;
    struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

    vmm_clock_chip_set_mode(tlcp->cc, VMM_CLOCKCHIP_MODE_ONESHOT);

    tstamp           = vmm_timer_timestamp();

    tlcp->next_event = tstamp + tlcp->cc->min_delta_ns;

    tlcp->started    = TRUE;

    vmm_clock_chip_program_event(tlcp->cc, tstamp, tlcp->next_event);
}

void vmm_timer_stop(void)
{
    struct vmm_timer_local_ctrl *tlcp = &this_cpu(tlc);

    vmm_clock_chip_set_mode(tlcp->cc, VMM_CLOCKCHIP_MODE_SHUTDOWN);

    tlcp->started = FALSE;
}

static int timer_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int                          rc;
    struct vmm_timer_local_ctrl *tlcp = &per_cpu(tlc, cpu);

    /* Clear timer control structure */
    memset(tlcp, 0, sizeof(*tlcp));

    /* Initialize Per CPU event status */
    tlcp->started   = FALSE;
    tlcp->inprocess = FALSE;

    /* Initialize Per CPU current event pointer */
    tlcp->curr      = NULL;

    /* Initialize Per CPU event list */
    INIT_RW_LOCK(&tlcp->event_list_lock);
    INIT_LIST_HEAD(&tlcp->event_list);

    /* Bind suitable clockchip to current host CPU */
    tlcp->cc = vmm_clock_chip_bind_best(cpu);

    if (!tlcp->cc) {
        vmm_printf("%s: No clockchip for CPU%d\n", __func__, cpu);
        return VMM_ENODEV;
    }

    /* Update event handler of clockchip */
    vmm_clock_chip_set_event_handler(tlcp->cc, &timer_clock_chip_event_handler);

    /* Initialize timecounter wrapper of secondary CPUs
     * such that time stamps visible on all CPUs is same;
     */
    if ((rc = vmm_timecounter_init(&tlcp->tc, ref_tc.cs, vmm_timecounter_read(&ref_tc)))) {
        return rc;
    }

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t timer_cpu_hotplug = {
    .name    = "TIMER",
    .state   = VMM_CPU_HOTPLUG_STATE_TIMER,
    .startup = timer_startup,
};

int __init vmm_timer_init(void)
{
    int                rc;
    vmm_clocksource_t *cs;

    /* Find suitable clocksource */
    if (!(cs = vmm_clocksource_best())) {
        vmm_printf("%s: No clocksource found\n", __func__);
        return VMM_ENODEV;
    }

    /* Initialize reference timecounter wrapper */
    if ((rc = vmm_timecounter_init(&ref_tc, cs, 0))) {
        return rc;
    }

    /* Start reference timecounter */
    if ((rc = vmm_timecounter_start(&ref_tc))) {
        return rc;
    }

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&timer_cpu_hotplug, TRUE);
}
