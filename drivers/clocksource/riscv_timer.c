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
 * @file riscv_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V timer clocksource and clockchips
 */

#include <libs/mathlib.h>
#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

#include <cpu_hwcap.h>
#include <cpu_sbi.h>
#include <riscv_csr.h>
#include <riscv_encoding.h>
#include <riscv_timex.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static uint64_t riscv_timer_read(vmm_clocksource_t *cs)
{
    return get_cycles64();
}

static int __init riscv_timer_clocksource_init(vmm_device_tree_node_t *node)
{
    int                rc;
    uint32_t           hart_id = 0;
    uint64_t           current_hart_id;
    vmm_clocksource_t *cs;

    rc = vmm_smp_map_hwid(vmm_smp_processor_id(), &current_hart_id);

    if (rc) {
        vmm_lerror("riscv-timer-clocksource", "failed to get current hart id\n");
        return rc;
    }

    rc = riscv_node_to_hartid(node, &hart_id);

    if (rc) {
        vmm_lerror("riscv-timer-clocksource", "failed to get node %s hart id\n", node->name);
        return rc;
    }

    if (current_hart_id != hart_id) {
        return VMM_OK;
    }

    /* Create riscv timer clocksource */
    cs = vmm_zalloc(sizeof(vmm_clocksource_t));

    if (!cs) {
        vmm_lerror("riscv-timer-clocksource", "failed to allocate clocksource\n");
        return VMM_ERR_NOMEM;
    }

    cs->name   = "riscv-timer";
    cs->rating = 400;
    cs->read   = &riscv_timer_read;
    cs->mask   = VMM_CLOCKSOURCE_MASK(64);
    cs->freq   = riscv_timer_hz;
    vmm_clocks_calc_mult_shift(&cs->mult, &cs->shift, riscv_timer_hz, VMM_NSEC_PER_SEC, 10);
    cs->private = NULL;

    /* Register riscv timer clocksource */
    rc          = vmm_clocksource_register(cs);

    if (rc) {
        vmm_lerror("riscv-timer-clocksource", "failed to register clocksource\n");
        vmm_free(cs);
        return rc;
    }

    vmm_init_printf(
        "riscv-timer: registered clocksource @ %ldHz%s\n", riscv_timer_hz, (riscv_isa_extension_available(NULL, SSTC)) ? " using Sstc" : "");
    return VMM_OK;
}

VMM_CLOCKSOURCE_INIT_DECLARE(riscvclock_src, "riscv", riscv_timer_clocksource_init);

static void riscv_timer_set_mode(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc)
{
    /* For now, nothing to do here. */
}

static int riscv_timer_set_next_event(uint64_t evt, vmm_clock_chip_t *unused)
{
    csr_set(sie, SIE_STIE);
    sbi_set_timer(get_cycles64() + evt);

    return VMM_OK;
}

static int riscv_timer_sstc_set_next_event(uint64_t evt, vmm_clock_chip_t *unused)
{
    uint64_t next = get_cycles64() + evt;

    csr_set(sie, SIE_STIE);
#ifdef CONFIG_32BIT
    csr_write(CSR_STIMECMP, (uint32_t)next);
    csr_write(CSR_STIMECMPH, (uint32_t)(next >> 32));
#else
    csr_write(CSR_STIMECMP, next);
#endif

    return VMM_OK;
}

static vmm_irq_return_t riscv_timer_handler(int irq, void *dev)
{
    vmm_clock_chip_t *cc = dev;

    /*
     * There are no direct SBI calls to clear pending timer interrupt bit.
     * Disable timer interrupt to ignore pending interrupt until next
     * interrupt.
     */
    csr_clear(sie, SIE_STIE);
    cc->event_handler(cc);

    return VMM_IRQ_HANDLED;
}

static int riscv_timer_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int               rc;
    vmm_clock_chip_t *cc;

    /* Create riscv timer clockchip */
    cc = vmm_zalloc(sizeof(vmm_clock_chip_t));

    if (!cc) {
        return VMM_ERR_FAIL;
    }

    cc->name     = "riscv-timer";
    cc->hirq     = IRQ_S_TIMER;
    cc->rating   = 400;
    cc->cpumask  = vmm_cpumask_of(cpu);
    cc->features = VMM_CLOCKCHIP_FEAT_ONESHOT;
    cc->freq     = riscv_timer_hz;
    vmm_clocks_calc_mult_shift(&cc->mult, &cc->shift, VMM_NSEC_PER_SEC, riscv_timer_hz, 10);
    cc->min_delta_ns = vmm_clock_chip_delta2ns(0xF, cc);
    cc->max_delta_ns = vmm_clock_chip_delta2ns(0x7FFFFFFF, cc);
    cc->set_mode     = &riscv_timer_set_mode;

    if (riscv_isa_extension_available(NULL, SSTC)) {
        cc->set_next_event = &riscv_timer_sstc_set_next_event;
    } else {
        cc->set_next_event = &riscv_timer_set_next_event;
    }

    cc->private = NULL;

    /* Register riscv timer clockchip */
    rc          = vmm_clock_chip_register(cc);

    if (rc) {
        goto fail_free_cc;
    }

    /* Ensure that timer interrupt bit zero in the sip CSR */
    if (riscv_isa_extension_available(NULL, SSTC)) {
        csr_write(CSR_STIMECMP, -1UL);
#ifdef CONFIG_32BIT
        csr_write(CSR_STIMECMPH, -1UL);
#endif
    } else {
        sbi_set_timer(U64_MAX);
    }

    /* Register irq handler for riscv timer */
    rc = vmm_host_irq_register(IRQ_S_TIMER, "riscv-timer", &riscv_timer_handler, cc);

    if (rc) {
        goto fail_unreg_cc;
    }

    return VMM_OK;

fail_unreg_cc:
    vmm_clock_chip_unregister(cc);
fail_free_cc:
    vmm_free(cc);
    return rc;
}

static vmm_cpu_hotplug_notify_t riscv_timer_cpu_hotplug = {
    .name    = "RISCV_TIMER",
    .state   = VMM_CPU_HOTPLUG_STATE_CLOCKCHIP,
    .startup = riscv_timer_startup,
};

static int __init riscv_timer_clock_chip_init(vmm_device_tree_node_t *node)
{
    int      rc;
    uint32_t hart_id;
    uint64_t current_hart_id;

    rc = vmm_smp_map_hwid(vmm_smp_processor_id(), &current_hart_id);

    if (rc) {
        vmm_lerror("riscv-timer-clockchip", "failed to get current hart id\n");
        return rc;
    }

    rc = riscv_node_to_hartid(node, &hart_id);

    if (rc) {
        vmm_lerror("riscv-timer-clockchip", "failed to get node %s hart id\n", node->name);
        return rc;
    }

    if (current_hart_id != hart_id) {
        return VMM_OK;
    }

    rc = vmm_cpu_hotplug_register(&riscv_timer_cpu_hotplug, TRUE);

    if (rc) {
        vmm_lerror("riscv-timer-clockchip", "failed to register cpu_hotplug\n");
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKCHIP_INIT_DECLARE(riscvclock_chip, "riscv", riscv_timer_clock_chip_init);
