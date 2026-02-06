/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file cpu_vcpu_trap.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU trap handling
 */
#ifndef _CPU_VCPU_TRAP_H__
#define _CPU_VCPU_TRAP_H__

#include <vmm_const.h>

#define RISCV_VCPU_TRAP_SEPC   (0 * __SIZEOF_POINTER__)
#define RISCV_VCPU_TRAP_SCAUSE (1 * __SIZEOF_POINTER__)
#define RISCV_VCPU_TRAP_STVAL  (2 * __SIZEOF_POINTER__)
#define RISCV_VCPU_TRAP_HTVAL  (3 * __SIZEOF_POINTER__)
#define RISCV_VCPU_TRAP_HTINST (4 * __SIZEOF_POINTER__)

#ifndef __ASSEMBLY__

#include <vmm_manager.h>
#include <vmm_types.h>

struct cpu_vcpu_trap {
    uint64_t sepc;
    uint64_t scause;
    uint64_t stval;
    uint64_t htval;
    uint64_t htinst;
};

enum trap_return {
    TRAP_RETURN_OK = 0,
    TRAP_RETURN_ILLEGAL_INSN,
    TRAP_RETURN_VIRTUAL_INSN,
    TRAP_RETURN_CONTINUE
};

void cpu_vcpu_update_trap(vmm_vcpu_t *vcpu, arch_regs_t *regs);

void cpu_vcpu_redirect_smode_trap(arch_regs_t *regs, struct cpu_vcpu_trap *trap, bool prev_spp);

void cpu_vcpu_redirect_trap(vmm_vcpu_t *vcpu, arch_regs_t *regs, struct cpu_vcpu_trap *trap);

int cpu_vcpu_sret_insn(vmm_vcpu_t *vcpu, arch_regs_t *regs, ulong insn);

int cpu_vcpu_page_fault(vmm_vcpu_t *vcpu, arch_regs_t *regs, struct cpu_vcpu_trap *trap);

int cpu_vcpu_general_fault(vmm_vcpu_t *vcpu, arch_regs_t *regs, struct cpu_vcpu_trap *trap);

int cpu_vcpu_virtual_insn_fault(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint64_t stval);

int cpu_vcpu_redirect_vsirq(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint64_t irq);

#endif

#endif
