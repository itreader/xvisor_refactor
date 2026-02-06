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
 * @file cpu_vcpu_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling vcpu interrupts
 */

#include <arch_vcpu.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_inject.h>
#include <vmm_error.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>

uint32_t arch_vcpu_irq_count(vmm_vcpu_t *vcpu)
{
    return CPU_IRQ_NR;
}

uint32_t arch_vcpu_irq_priority(vmm_vcpu_t *vcpu, uint32_t irq_no)
{
    uint32_t ret = 3;

    switch (irq_no) {
        case CPU_RESET_IRQ:
            ret = 0;
            break;

        case CPU_UNDEF_INST_IRQ:
            ret = 1;
            break;

        case CPU_SOFT_IRQ:
            ret = 2;
            break;

        case CPU_PREFETCH_ABORT_IRQ:
            ret = 2;
            break;

        case CPU_DATA_ABORT_IRQ:
            ret = 2;
            break;

        case CPU_HYP_TRAP_IRQ:
            ret = 2;
            break;

        case CPU_EXTERNAL_IRQ:
            ret = 2;
            break;

        case CPU_EXTERNAL_FIQ:
            ret = 2;
            break;

        default:
            break;
    };

    return ret;
}

int arch_vcpu_irq_assert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    uint32_t    hcr;
    bool        update_hcr;
    irq_flags_t flags;

    /* Skip IRQ & FIQ if VGIC available */
    if (arm_vgic_avail(vcpu) && ((irq_no == CPU_EXTERNAL_IRQ) || (irq_no == CPU_EXTERNAL_FIQ))) {
        return VMM_OK;
    }

    vmm_spin_lock_irq_save_lite(&arm_private(vcpu)->hcr_lock, flags);

    hcr        = arm_private(vcpu)->hcr;
    update_hcr = FALSE;

    switch (irq_no) {
        case CPU_EXTERNAL_IRQ:
            hcr |= HCR_VI_MASK;
            /* VI bit will be cleared on deassertion */
            update_hcr = TRUE;
            break;

        case CPU_EXTERNAL_FIQ:
            hcr |= HCR_VF_MASK;
            /* VF bit will be cleared on deassertion */
            update_hcr = TRUE;
            break;

        default:
            break;
    };

    if (update_hcr) {
        arm_private(vcpu)->hcr = hcr;

        if (vmm_scheduler_current_vcpu() == vcpu) {
            write_hcr(hcr);
        }
    }

    vmm_spin_unlock_irq_restore_lite(&arm_private(vcpu)->hcr_lock, flags);

    return VMM_OK;
}

bool arch_vcpu_irq_can_execute_multiple(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    return FALSE;
}

int arch_vcpu_irq_execute(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t irq_no, uint64_t reason)
{
    int         rc;
    irq_flags_t flags;

    /* Skip IRQ & FIQ if VGIC available */
    if (arm_vgic_avail(vcpu) && ((irq_no == CPU_EXTERNAL_IRQ) || (irq_no == CPU_EXTERNAL_FIQ))) {
        arm_vgic_irq_pending(vcpu);
        return VMM_OK;
    }

    /* Undefined, Data abort, and Prefetch abort
     * can only be emulated in normal context.
     */
    switch (irq_no) {
        case CPU_UNDEF_INST_IRQ:
            rc = cpu_vcpu_inject_undef(vcpu, regs);
            break;

        case CPU_PREFETCH_ABORT_IRQ:
            rc = cpu_vcpu_inject_pabt(vcpu, regs);
            break;

        case CPU_DATA_ABORT_IRQ:
            rc = cpu_vcpu_inject_dabt(vcpu, regs, (virtual_addr_t)reason);
            break;

        default:
            rc = VMM_OK;
            break;
    };

    /* Update HCR in HW */
    vmm_spin_lock_irq_save_lite(&arm_private(vcpu)->hcr_lock, flags);

    write_hcr(arm_private(vcpu)->hcr);

    vmm_spin_unlock_irq_restore_lite(&arm_private(vcpu)->hcr_lock, flags);

    return rc;
}

int arch_vcpu_irq_clear(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    /* We don't implement this. */
    return VMM_OK;
}

int arch_vcpu_irq_deassert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    uint32_t    hcr;
    bool        update_hcr;
    irq_flags_t flags;

    /* Skip IRQ & FIQ if VGIC available */
    if (arm_vgic_avail(vcpu) && ((irq_no == CPU_EXTERNAL_IRQ) || (irq_no == CPU_EXTERNAL_FIQ))) {
        return VMM_OK;
    }

    vmm_spin_lock_irq_save_lite(&arm_private(vcpu)->hcr_lock, flags);

    hcr        = arm_private(vcpu)->hcr;
    update_hcr = FALSE;

    switch (irq_no) {
        case CPU_EXTERNAL_IRQ:
            hcr &= ~HCR_VI_MASK;
            update_hcr = TRUE;
            break;

        case CPU_EXTERNAL_FIQ:
            hcr &= ~HCR_VF_MASK;
            update_hcr = TRUE;
            break;

        default:
            break;
    };

    if (update_hcr) {
        arm_private(vcpu)->hcr = hcr;

        if (vmm_scheduler_current_vcpu() == vcpu) {
            write_hcr(hcr);
        }
    }

    vmm_spin_unlock_irq_restore_lite(&arm_private(vcpu)->hcr_lock, flags);

    return VMM_OK;
}

bool arch_vcpu_irq_pending(vmm_vcpu_t *vcpu)
{
    return arm_vgic_irq_pending(vcpu);
}
