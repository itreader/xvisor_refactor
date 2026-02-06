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
 * @file cpu_vcpu_coproc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for coprocessor access
 */
#ifndef _CPU_VCPU_COPROC_H__
#define _CPU_VCPU_COPROC_H__

#include <vmm_manager.h>
#include <vmm_types.h>

typedef bool (*cpu_coproc_ldcstc_accept)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8);

typedef bool (*cpu_coproc_ldcstc_done)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t index, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8);

typedef uint32_t (*cpu_coproc_ldcstc_read)(
    vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t index, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8);

typedef void (*cpu_coproc_ldcstc_write)(
    vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t index, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8, uint32_t data);

typedef bool (*cpu_coproc_read2)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t CRm, uint32_t *data, uint32_t *data2);

typedef bool (*cpu_coproc_write2)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t CRm, uint32_t data, uint32_t data2);

typedef bool (*cpu_coproc_data_process)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRd, uint32_t CRn, uint32_t CRm);

typedef bool (*cpu_coproc_read)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t *data);

typedef bool (*cpu_coproc_write)(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t data);

struct cpu_vcpu_coproc {
    uint32_t                 cpnum;
    cpu_coproc_ldcstc_accept ldcstc_accept;
    cpu_coproc_ldcstc_done   ldcstc_done;
    cpu_coproc_ldcstc_read   ldcstc_read;
    cpu_coproc_ldcstc_write  ldcstc_write;
    cpu_coproc_read2         read2;
    cpu_coproc_write2        write2;
    cpu_coproc_data_process  data_process;
    cpu_coproc_read          read;
    cpu_coproc_write         write;
};

/** Retrive a coprocessor with given number */
struct cpu_vcpu_coproc *cpu_vcpu_coproc_get(uint32_t cpnum);

#endif /* _CPU_VCPU_COPROC_H */
