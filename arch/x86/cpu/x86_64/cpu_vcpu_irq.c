/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling vcpu interrupts
 */

#include <arch_cpu.h>
#include <arch_guest_helper.h>
#include <cpu_vm.h>
#include <vmm_error.h>
#include <vmm_vcpu_irq.h>

uint32_t arch_vcpu_irq_count(vmm_vcpu_t *vcpu)
{
    return 256;
}

uint32_t arch_vcpu_irq_priority(vmm_vcpu_t *vcpu, uint32_t irq_no)
{
    /* all at same priority */
    return 1;
}

/*
 * NOTE:
 *
 * arch_vcpu_irq_XXassert has to be called by the last PIC in the
 * chain. For example, if software has configured 8259 along with
 * LAPIC, this function should be finally called by LAPIC. All other
 * PICs should become slave of LAPIC.
 */
int arch_vcpu_irq_assert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    struct x86_vcpu_priv *vcpu_private = NULL;
    vcpu_priv                          = x86_vcpu_private(vcpu);

    mark_guest_interrupt_pending(vcpu_private->hw_context, irq_no);

    return VMM_OK;
}

bool arch_vcpu_irq_can_execute_multiple(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    return FALSE;
}

/* FIXME: */
int arch_vcpu_irq_execute(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t irq_no, uint64_t reason)
{
    return VMM_OK;
}

int arch_vcpu_irq_clear(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    /* We don't implement this. */
    return VMM_OK;
}

int arch_vcpu_irq_deassert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    struct x86_vcpu_priv *vcpu_private = NULL;
    irq_flags_t           flags;
    vcpu_private = x86_vcpu_private(vcpu);

    vmm_spin_lock_irq_save_lite(&vcpu_private->lock, flags);

    if (vcpu_private->int_pending != irq_no) {
        vmm_printf(
            "%s: WARNING!!! IRQ %d on vcpu %s not active to deassert! Currently active: %d\n", __func__, irq_no, vcpu->name,
            vcpu_private->int_pending);
        vmm_spin_unlock_irq_restore_lite(&vcpu_private->lock, flags);
        return VMM_ERR_FAIL;
    }

    x86_vcpu_private(vcpu)->int_pending = irq_no;
    vmm_spin_unlock_irq_restore_lite(&vcpu_private->lock, flags);

    return VMM_OK;
}

bool arch_vcpu_irq_pending(vmm_vcpu_t *vcpu)
{
    return false;
}
