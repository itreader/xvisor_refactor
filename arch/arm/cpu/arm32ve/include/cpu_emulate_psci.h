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
#include <cpu_vcpu_helper.h>
#include <vmm_types.h>

static inline uint32_t emulate_psci_version(vmm_vcpu_t *vcpu)
{
    return arm_guest_private(vcpu->guest)->psci_version;
}

static inline bool emulate_psci_is_32bit(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    return TRUE;
}

static inline void emulate_psci_set_thumb(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    regs->cpsr |= CPSR_THUMB_ENABLED;
}

static inline bool emulate_psci_is_be(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    return (regs->cpsr & CPSR_BE_ENABLED) ? TRUE : FALSE;
}

static inline void emulate_psci_set_be(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    regs->cpsr |= CPSR_BE_ENABLED;
}

static inline uint64_t emulate_psci_get_reg(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t reg)
{
    return (uint64_t)cpu_vcpu_reg_read(vcpu, regs, reg);
}

static inline void emulate_psci_set_reg(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t reg, uint64_t val)
{
    cpu_vcpu_reg_write(vcpu, regs, reg, (uint32_t)val);
}

static inline void emulate_psci_set_pc(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint64_t val)
{
    regs->pc = (uint32_t)val;
}

static inline uint64_t emulate_psci_get_mpidr(vmm_vcpu_t *vcpu)
{
    return arm_private(vcpu)->cp15.c0_mpidr;
}

#endif /* __CPU_EMULATE_PSCI_H__ */
