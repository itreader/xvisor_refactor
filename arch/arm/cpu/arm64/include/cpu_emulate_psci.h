/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cpu_emulate_psci.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU specific functions for PSCI Emulation
 */

#ifndef __CPU_EMULATE_PSCI_H__
#define __CPU_EMULATE_PSCI_H__

#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <vmm_types.h>

static inline uint32_t emulate_psci_version(vmm_vcpu_t *vcpu)
{
    return arm_guest_private(vcpu->guest)->psci_version;
}

static inline bool emulate_psci_is_32bit(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    return (regs->pstate & PSR_MODE32) ? TRUE : FALSE;
}

static inline void emulate_psci_set_thumb(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    regs->pstate |= CPSR_THUMB_ENABLED;
}

static inline bool emulate_psci_is_be(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    if (regs->pstate & PSR_MODE32) {
        return (regs->pstate & CPSR_BE_ENABLED) ? TRUE : FALSE;
    }

    return (mrs(sctlr_el1) & SCTLR_EE_MASK) ? TRUE : FALSE;
}

static inline void emulate_psci_set_be(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    if (regs->pstate & PSR_MODE32) {
        regs->pstate |= CPSR_BE_ENABLED;
        return;
    }

    arm_private(vcpu)->sysregs.sctlr_el1 |= SCTLR_EE_MASK;
}

static inline uint64_t emulate_psci_get_reg(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t reg)
{
    return (uint64_t)cpu_vcpu_reg_read(vcpu, regs, reg);
}

static inline void emulate_psci_set_reg(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t reg, uint64_t val)
{
    cpu_vcpu_reg_write(vcpu, regs, reg, (uint64_t)val);
}

static inline void emulate_psci_set_pc(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint64_t val)
{
    regs->pc = (uint64_t)val;
}

static inline uint64_t emulate_psci_get_mpidr(vmm_vcpu_t *vcpu)
{
    return arm_private(vcpu)->sysregs.mpidr_el1;
}

#endif /* __CPU_EMULATE_PSCI_H__ */
