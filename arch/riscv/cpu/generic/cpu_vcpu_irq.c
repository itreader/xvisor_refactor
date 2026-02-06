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
 * @file cpu_vcpu_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling vcpu interrupts
 */

#include <arch_vcpu.h>
#include <riscv_csr.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>

uint32_t arch_vcpu_irq_count(vmm_vcpu_t *vcpu)
{
    return ARCH_BITS_PER_LONG;
}

uint32_t arch_vcpu_irq_priority(vmm_vcpu_t *vcpu, uint32_t irq_no)
{
    /* Same priority for all VCPU interrupts */
    return 2;
}

int arch_vcpu_irq_assert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    /* Nothing to do here. */
    return VMM_OK;
}

bool arch_vcpu_irq_can_execute_multiple(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    return TRUE;
}

int arch_vcpu_irq_execute(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t irq_no, uint64_t reason)
{
    uint64_t irq_mask;

    if (irq_no >= ARCH_BITS_PER_LONG) {
        return VMM_EINVALID;
    }

    irq_mask = 1UL << irq_no;

    csr_set(CSR_HVIP, irq_mask);
    riscv_private(vcpu)->hvip = csr_read(CSR_HVIP);

    return VMM_OK;
}

int arch_vcpu_irq_clear(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    uint64_t irq_mask;

    if (irq_no >= ARCH_BITS_PER_LONG) {
        return VMM_EINVALID;
    }

    irq_mask = 1UL << irq_no;

    csr_clear(CSR_HVIP, irq_mask);
    riscv_private(vcpu)->hvip = csr_read(CSR_HVIP);

    return VMM_OK;
}

int arch_vcpu_irq_deassert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason)
{
    /* Nothing to do here. */
    return VMM_OK;
}

bool arch_vcpu_irq_pending(vmm_vcpu_t *vcpu)
{
    riscv_private(vcpu)->hip = csr_read(CSR_HIP);
    riscv_private(vcpu)->hie = csr_read(CSR_HIE);
    return (riscv_private(vcpu)->hip & riscv_private(vcpu)->hie) ? TRUE : FALSE;
}
