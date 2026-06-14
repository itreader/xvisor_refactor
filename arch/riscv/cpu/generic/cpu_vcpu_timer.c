/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_vcpu_timer.c
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief RISC-V timer event
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_limits.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_trap.h>
#include <riscv_encoding.h>

struct cpu_vcpu_timer {
    /* virtual-VS mode state */
    uint64_t          vs_next_cycle;
    vmm_timer_event_t vs_time_ev;
    /* S mode state */
    uint64_t          next_cycle;
    vmm_timer_event_t time_nested_ev;
    vmm_timer_event_t time_ev;
};

static inline uint64_t cpu_vcpu_timer_delta(vmm_vcpu_t *vcpu, bool nested_virt)
{
    uint64_t ndelta = 0;

    if (nested_virt) {
        ndelta = riscv_nested_private(vcpu)->htimedelta;
#ifdef CONFIG_32BIT
        ndelta |= ((uint64_t)riscv_nested_private(vcpu)->htimedeltah) << 32;
#endif
    }

    return riscv_guest_private(vcpu->guest)->time_delta + ndelta;
}

bool cpu_vcpu_timer_vs_irq(vmm_vcpu_t *vcpu)
{
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    return t->vs_next_cycle <= (csr_read(CSR_TIME) + cpu_vcpu_timer_delta(vcpu, TRUE));
}

uint64_t cpu_vcpu_timer_vs_cycle(vmm_vcpu_t *vcpu)
{
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    return t->vs_next_cycle;
}

static void cpu_vcpu_timer_vs_expired(vmm_timer_event_t *ev)
{
    vmm_vcpu_t *vcpu = ev->private;

    if (cpu_vcpu_timer_vs_irq(vcpu)) {
        vmm_vcpu_irq_wait_resume(vcpu);
    } else {
        cpu_vcpu_timer_vs_restart(vcpu);
    }
}

void cpu_vcpu_timer_vs_restart(vmm_vcpu_t *vcpu)
{
    uint64_t               vs_delta_ns, vs_next_cycle;
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    /* Stop the VS timer when next timer tick equals U64_MAX */
    if (t->vs_next_cycle == U64_MAX) {
        vmm_timer_event_stop(&t->vs_time_ev);
        return;
    }

    /* Do nothing is Virtual-VS mode IRQ is pending */
    if (cpu_vcpu_timer_vs_irq(vcpu)) {
        vmm_timer_event_stop(&t->vs_time_ev);
        return;
    }

    /* Start the VS timer event */
    vs_next_cycle = t->vs_next_cycle - cpu_vcpu_timer_delta(vcpu, TRUE);
    vs_delta_ns   = vmm_timer_delta_cycles_to_ns(vs_next_cycle);
    vmm_timer_event_start(&t->vs_time_ev, vs_delta_ns);
}

void cpu_vcpu_timer_vs_start(vmm_vcpu_t *vcpu, uint64_t vs_next_cycle)
{
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    /* Save the next VS timer tick value */
    t->vs_next_cycle         = vs_next_cycle;

    /* Restart VS timer */
    cpu_vcpu_timer_vs_restart(vcpu);
}

static void cpu_vcpu_timer_nested_expired(vmm_timer_event_t *ev)
{
    int         rc;
    vmm_vcpu_t *vcpu = ev->private;

    if (!riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
        return;
    }

    /* Redirect trap to invoke nested world switch */
    rc = cpu_vcpu_redirect_vsirq(vcpu, vmm_scheduler_irq_regs(), IRQ_VS_TIMER);
    BUG_ON(rc);
}

static void cpu_vcpu_timer_expired(vmm_timer_event_t *ev)
{
    vmm_vcpu_t            *vcpu = ev->private;
    struct cpu_vcpu_timer *t    = riscv_timer_private(vcpu);

    BUG_ON(!t);

    if (riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
        vmm_vcpu_irq_wait_resume(vcpu);
    } else {
        vmm_vcpu_irq_assert(vcpu, IRQ_VS_TIMER, 0x0);
    }
}

void cpu_vcpu_timer_start(vmm_vcpu_t *vcpu, uint64_t next_cycle)
{
    uint64_t               delta_ns;
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    /* This function should only be called when nested virt is OFF */
    BUG_ON(riscv_nested_virt(vcpu));

    /* Save the next timer tick value */
    t->next_cycle = next_cycle;

    /* If Sstc available then simply update vstimecmp CSRs */
    if (riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
#ifdef CONFIG_32BIT
        csr_write(CSR_VSTIMECMP, (uint32_t)t->next_cycle);
        csr_write(CSR_VSTIMECMPH, (uint32_t)(t->next_cycle >> 32));
#else
        csr_write(CSR_VSTIMECMP, t->next_cycle);
#endif
        return;
    }

    /* Stop the timer when next timer tick equals U64_MAX */
    if (next_cycle == U64_MAX) {
        vmm_timer_event_stop(&t->time_ev);
        vmm_vcpu_irq_clear(vcpu, IRQ_VS_TIMER);
        return;
    }

    /*
     * In RISC-V, we should clear the timer pending bit before
     * programming next one.
     */
    vmm_vcpu_irq_clear(vcpu, IRQ_VS_TIMER);

    /* Start the timer event */
    next_cycle -= cpu_vcpu_timer_delta(vcpu, FALSE);
    delta_ns = vmm_timer_delta_cycles_to_ns(next_cycle);
    vmm_timer_event_start(&t->time_ev, delta_ns);
}

void cpu_vcpu_timer_delta_update(vmm_vcpu_t *vcpu, bool nested_virt)
{
    uint64_t               delta_ns, new_delta;
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    new_delta                = cpu_vcpu_timer_delta(vcpu, nested_virt);
#ifdef CONFIG_32BIT
    csr_write(CSR_HTIMEDELTA, (uint32_t)new_delta);
    csr_write(CSR_HTIMEDELTAH, (uint32_t)(new_delta >> 32));
#else
    csr_write(CSR_HTIMEDELTA, new_delta);
#endif

    if (!riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
        return;
    }

    if (nested_virt) {
#ifdef CONFIG_32BIT
        t->next_cycle = csr_swap(CSR_VSTIMECMP, -1UL);
        t->next_cycle |= (uint64_t)csr_swap(CSR_VSTIMECMPH, -1UL) << 32;
#else
        t->next_cycle = csr_swap(CSR_VSTIMECMP, -1UL);
#endif

        if (t->next_cycle != U64_MAX) {
            delta_ns = t->next_cycle - cpu_vcpu_timer_delta(vcpu, FALSE);
            delta_ns = vmm_timer_delta_cycles_to_ns(delta_ns);
            vmm_timer_event_start(&t->time_nested_ev, delta_ns);
        }
    } else {
        vmm_timer_event_stop(&t->time_nested_ev);

#ifdef CONFIG_32BIT
        csr_write(CSR_VSTIMECMP, (uint32_t)t->next_cycle);
        csr_write(CSR_VSTIMECMPH, (uint32_t)(t->next_cycle >> 32));
#else
        csr_write(CSR_VSTIMECMP, t->next_cycle);
#endif
    }
}

void cpu_vcpu_timer_save(vmm_vcpu_t *vcpu)
{
    uint64_t               delta_ns;
    struct cpu_vcpu_timer *t;

    if (!riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
        return;
    }

    t = riscv_timer_private(vcpu);

    if (riscv_nested_virt(vcpu)) {
        vmm_timer_event_stop(&t->time_nested_ev);
    } else {
#ifdef CONFIG_32BIT
        t->next_cycle = csr_swap(CSR_VSTIMECMP, -1UL);
        t->next_cycle |= (uint64_t)csr_swap(CSR_VSTIMECMPH, -1UL) << 32;
#else
        t->next_cycle = csr_swap(CSR_VSTIMECMP, -1UL);
#endif
    }

    if (t->next_cycle != U64_MAX) {
        delta_ns = t->next_cycle - cpu_vcpu_timer_delta(vcpu, FALSE);
        delta_ns = vmm_timer_delta_cycles_to_ns(delta_ns);
        vmm_timer_event_start(&t->time_ev, delta_ns);
    }
}

void cpu_vcpu_timer_restore(vmm_vcpu_t *vcpu)
{
    uint64_t               delta_ns, time_delta;
    struct cpu_vcpu_timer *t = riscv_timer_private(vcpu);

    time_delta               = cpu_vcpu_timer_delta(vcpu, riscv_nested_virt(vcpu));
#ifdef CONFIG_32BIT
    csr_write(CSR_HTIMEDELTA, (uint32_t)time_delta);
    csr_write(CSR_HTIMEDELTAH, (uint32_t)(time_delta >> 32));
#else
    csr_write(CSR_HTIMEDELTA, time_delta);
#endif

    if (!riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
        return;
    }

    vmm_timer_event_stop(&t->time_ev);

    if (riscv_nested_virt(vcpu)) {
        if (t->next_cycle != U64_MAX) {
            delta_ns = t->next_cycle - cpu_vcpu_timer_delta(vcpu, FALSE);
            delta_ns = vmm_timer_delta_cycles_to_ns(delta_ns);
            vmm_timer_event_start(&t->time_nested_ev, delta_ns);
        }
    } else {
#ifdef CONFIG_32BIT
        csr_write(CSR_VSTIMECMP, (uint32_t)t->next_cycle);
        csr_write(CSR_VSTIMECMPH, (uint32_t)(t->next_cycle >> 32));
#else
        csr_write(CSR_VSTIMECMP, t->next_cycle);
#endif
    }
}

int cpu_vcpu_timer_init(vmm_vcpu_t *vcpu, void **timer)
{
    struct cpu_vcpu_timer *t;

    if (!vcpu || !timer) {
        return VMM_ERR_INVALID;
    }

    if (!(*timer)) {
        *timer = vmm_zalloc(sizeof(struct cpu_vcpu_timer));

        if (!(*timer)) {
            return VMM_ERR_NOMEM;
        }

        t = *timer;
        INIT_TIMER_EVENT(&t->vs_time_ev, cpu_vcpu_timer_vs_expired, vcpu);
        INIT_TIMER_EVENT(&t->time_nested_ev, cpu_vcpu_timer_nested_expired, vcpu);
        INIT_TIMER_EVENT(&t->time_ev, cpu_vcpu_timer_expired, vcpu);
    } else {
        t = *timer;
    }

    t->vs_next_cycle = U64_MAX;
    vmm_timer_event_stop(&t->vs_time_ev);

    t->next_cycle = U64_MAX;
    vmm_timer_event_stop(&t->time_nested_ev);
    vmm_timer_event_stop(&t->time_ev);

    if (riscv_isa_extension_available(riscv_private(vcpu)->isa, SSTC)) {
        riscv_private(vcpu)->henvcfg |= ENVCFG_STCE;
    }

    return VMM_OK;
}

int cpu_vcpu_timer_deinit(vmm_vcpu_t *vcpu, void **timer)
{
    struct cpu_vcpu_timer *t;

    if (!vcpu || !timer || !(*timer)) {
        return VMM_ERR_INVALID;
    }

    t = *timer;

    vmm_timer_event_stop(&t->vs_time_ev);
    vmm_timer_event_stop(&t->time_nested_ev);
    vmm_timer_event_stop(&t->time_ev);
    vmm_free(t);

    return VMM_OK;
}
