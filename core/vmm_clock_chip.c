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
 * @file vmm_clock_chip.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation to clockchip management
 */

#include <arch_timer.h>
#include <vmm_clock_chip.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_error.h>
#include <vmm_host_irq.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

/** Control structure for clock chip manager */
struct vmm_clock_chip_ctrl {
    vmm_spinlock_t lock;
    double_list_t  clock_chip_list;
};

static struct vmm_clock_chip_ctrl ccctrl;

static void default_event_handler(vmm_clock_chip_t *cc)
{
    /* Just ignore. Do nothing. */
}

void vmm_clock_chip_set_event_handler(vmm_clock_chip_t *cc, void (*event_handler)(vmm_clock_chip_t *))
{
    if (cc && event_handler) {
        cc->event_handler = event_handler;
    }
}

int vmm_clock_chip_program_event(vmm_clock_chip_t *cc, uint64_t now_ns, uint64_t expires_ns)
{
    uint64_t clc;
    uint64_t delta;

    if (expires_ns < now_ns) {
        return VMM_EFAIL;
    }

    if (cc->mode != VMM_CLOCKCHIP_MODE_ONESHOT) {
        return 0;
    }

    delta          = expires_ns - now_ns;
    cc->next_event = expires_ns;

    if (delta > cc->max_delta_ns) {
        delta = cc->max_delta_ns;
    }

    if (delta < cc->min_delta_ns) {
        delta = cc->min_delta_ns;
    }

    clc = delta * cc->mult;
    clc >>= cc->shift;

    return cc->set_next_event((uint64_t)clc, cc);
}

void vmm_clock_chip_set_mode(vmm_clock_chip_t *cc, vmm_clock_chip_mode_e mode)
{
    if (cc && cc->mode != mode) {
        cc->set_mode(mode, cc);
        cc->mode = mode;

        /* Multiplicator of 0 is invalid and we'd crash on it. */
        if (mode == VMM_CLOCKCHIP_MODE_ONESHOT) {
            if (!cc->mult) {
                vmm_panic("%s: clockchip mult=0 not allowed\n", __func__);
            }
        }
    }
}

int vmm_clock_chip_register(vmm_clock_chip_t *cc)
{
    bool              found;
    irq_flags_t       flags;
    vmm_clock_chip_t *cct;

    if (!cc) {
        return VMM_EFAIL;
    }

    cct   = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    list_for_each_entry(cct, &ccctrl.clock_chip_list, head)
    {
        if (cct == cc) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);
        return VMM_EFAIL;
    }

    INIT_LIST_HEAD(&cc->head);
    cc->event_handler = default_event_handler;
    cc->bound_on      = UINT_MAX;
    list_add_tail(&cc->head, &ccctrl.clock_chip_list);

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return VMM_OK;
}

int vmm_clock_chip_unregister(vmm_clock_chip_t *cc)
{
    bool              found;
    irq_flags_t       flags;
    vmm_clock_chip_t *cct;

    if (!cc) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    if (list_empty(&ccctrl.clock_chip_list)) {
        vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);
        return VMM_EFAIL;
    }

    cct   = NULL;
    found = FALSE;
    list_for_each_entry(cct, &ccctrl.clock_chip_list, head)
    {
        if (cct == cc) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);
        return VMM_ENOTAVAIL;
    }

    list_del(&cc->head);

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return VMM_OK;
}

vmm_clock_chip_t *vmm_clock_chip_bind_best(uint32_t host_cpu)
{
    int                  best_rating;
    irq_flags_t          flags;
    const vmm_cpumask_t *mask;
    vmm_clock_chip_t    *cc, *best_cc;

    if (CONFIG_CPU_COUNT <= host_cpu) {
        return NULL;
    }

    mask        = vmm_cpumask_of(host_cpu);
    cc          = NULL;
    best_cc     = NULL;
    best_rating = 0;

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    list_for_each_entry(cc, &ccctrl.clock_chip_list, head)
    {
        if ((cc->rating > best_rating) && (cc->bound_on == UINT_MAX) && vmm_cpumask_intersects(cc->cpumask, mask)) {
            best_cc     = cc;
            best_rating = cc->rating;
        }
    }

    if (best_cc) {
        vmm_host_irq_set_affinity(best_cc->hirq, mask, TRUE);
        best_cc->bound_on = host_cpu;
    }

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return best_cc;
}

int vmm_clock_chip_unbind(vmm_clock_chip_t *cc)
{
    irq_flags_t flags;

    if (!cc) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);
    cc->bound_on = UINT_MAX;
    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return VMM_OK;
}

vmm_clock_chip_t *vmm_clock_chip_get(int index)
{
    bool              found;
    irq_flags_t       flags;
    vmm_clock_chip_t *cc;

    if (index < 0) {
        return NULL;
    }

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    cc    = NULL;
    found = FALSE;

    list_for_each_entry(cc, &ccctrl.clock_chip_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    if (!found) {
        return NULL;
    }

    return cc;
}

uint32_t vmm_clock_chip_count(void)
{
    uint32_t          retval = 0;
    irq_flags_t       flags;
    vmm_clock_chip_t *cc;

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    list_for_each_entry(cc, &ccctrl.clock_chip_list, head)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return retval;
}

int __weak arch_clock_chip_init(void)
{
    /* Default weak implementation in-case
     * architecture does not provide one.
     */
    return VMM_OK;
}

static void __init clockchip_nidtable_found(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int                   err;
    vmm_clock_chip_init_t init_fn = match->data;

    if (!init_fn) {
        return;
    }

    err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE

    if (err) {
        vmm_printf("%s: CPU%d Init %s node failed (error %d)\n", __func__, vmm_smp_processor_id(), node->name, err);
    }

#else
    (void)err;
#endif
}

static int clockchip_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int rc;

    /* Initialize arch specific clockchips */
    if ((rc = arch_clock_chip_init())) {
        return rc;
    }

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t clockchip_cpu_hotplug = {
    .name    = "CLOCKCHIP",
    .state   = VMM_CPU_HOTPLUG_STATE_CLOCKCHIP,
    .startup = clockchip_startup,
};

int __init vmm_clock_chip_init(void)
{
    const struct vmm_device_tree_nodeid *clock_chip_matches;

    /* Initialize clockchip list lock */
    INIT_SPIN_LOCK(&ccctrl.lock);

    /* Initialize clockchip list */
    INIT_LIST_HEAD(&ccctrl.clock_chip_list);

    /* Probe all device tree nodes matching
     * clockchip nodeid table enteries.
     */
    clock_chip_matches = vmm_device_tree_nidtable_create_matches("clockchip");

    if (clock_chip_matches) {
        vmm_device_tree_iterate_matching(NULL, clock_chip_matches, clockchip_nidtable_found, NULL);
    }

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&clockchip_cpu_hotplug, TRUE);
}
