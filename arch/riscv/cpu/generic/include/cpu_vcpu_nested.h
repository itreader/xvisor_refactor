/**
 * Copyright (c) 2022 Ventana Micro Systems Inc.
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
 * @file cpu_vcpu_nested.h
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief header of VCPU nested functions
 */

#ifndef _CPU_VCPU_NESTED_H__
#define _CPU_VCPU_NESTED_H__

#include <vmm_types.h>

struct vmm_vcpu;
struct cpu_vcpu_trap;
struct arch_regs;
typedef struct vmm_vcpu vmm_vcpu_t;
/** Function to flush nested software TLB */
void                    cpu_vcpu_nested_software_tlb_flush(vmm_vcpu_t *vcpu, physical_addr_t guest_gpa, physical_size_t guest_gpa_size);

/** Function to init nested state */
int cpu_vcpu_nested_init(vmm_vcpu_t *vcpu);

/** Function to reset nested state */
void cpu_vcpu_nested_reset(vmm_vcpu_t *vcpu);

/** Function to initialize nested state */
void cpu_vcpu_nested_deinit(vmm_vcpu_t *vcpu);

/** Function to dump nested registers */
void cpu_vcpu_nested_dump_regs(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Function to access nested non-virt CSRs */
int cpu_vcpu_nested_smode_csr_rmw(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t csr_num, uint64_t *val, uint64_t new_val, uint64_t wr_mask);

/** Function to access nested virt CSRs */
int cpu_vcpu_nested_hext_csr_rmw(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t csr_num, uint64_t *val, uint64_t new_val, uint64_t wr_mask);

/** Function to handle nested page fault */
int cpu_vcpu_nested_page_fault(vmm_vcpu_t *vcpu, bool trap_from_smode, const struct cpu_vcpu_trap *trap, struct cpu_vcpu_trap *out_trap);

/** Function to handle nested hfence.vvma instruction */
void cpu_vcpu_nested_hfence_vvma(vmm_vcpu_t *vcpu, uint64_t *vaddr, uint32_t *asid);

/** Function to handle nested hfence.gvma instruction */
void cpu_vcpu_nested_hfence_gvma(vmm_vcpu_t *vcpu, physical_addr_t *gaddr, uint32_t *vmid);

/**
 * Function to handle nested hlv instruction
 * @returns (< 0) error code upon failure and (>= 0) trap return value
 * upon success
 */
int cpu_vcpu_nested_hlv(
    vmm_vcpu_t *vcpu, uint64_t vaddr, bool hlvx, void *data, uint64_t len, uint64_t *out_scause, uint64_t *out_stval, uint64_t *out_htval);

/**
 * Function to handle nested hsv instruction
 * @returns (< 0) error code upon failure and (>= 0) trap return value
 * upon success
 */
int cpu_vcpu_nested_hsv(
    vmm_vcpu_t *vcpu, uint64_t vaddr, const void *data, uint64_t len, uint64_t *out_scause, uint64_t *out_stval, uint64_t *out_htval);

enum nested_set_virt_event {
    NESTED_SET_VIRT_EVENT_TRAP = 0,
    NESTED_SET_VIRT_EVENT_SRET,
};

/**
 * Function to change nested virtualization state
 * NOTE: This can also update Guest hstatus.SPV and hstatus.SPVP bits
 */
void cpu_vcpu_nested_set_virt(vmm_vcpu_t *vcpu, struct arch_regs *regs, enum nested_set_virt_event event, bool virt, bool spvp, bool gva);

/** Function to take virtual-VS mode interrupts */
void cpu_vcpu_nested_take_vsirq(vmm_vcpu_t *vcpu, struct arch_regs *regs);

#endif
