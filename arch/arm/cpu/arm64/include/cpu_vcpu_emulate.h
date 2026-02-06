/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file cpu_vcpu_emulate.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Inferace for hardware assisted instruction emulation
 */
#ifndef _CPU_VCPU_EMULATE_H__
#define _CPU_VCPU_EMULATE_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/** Emulate WFI/WFE instruction */
int cpu_vcpu_emulate_wfi_wfe(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate MCR/MRC CP15 instruction */
int cpu_vcpu_emulate_mcr_mrc_cp15(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate MCRR/MRRC CP15 instruction */
int cpu_vcpu_emulate_mcrr_mrrc_cp15(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate MCR/MRC CP14 instruction */
int cpu_vcpu_emulate_mcr_mrc_cp14(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate LDC/STC CP14 instruction */
int cpu_vcpu_emulate_ldc_stc_cp14(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate Advanced SIMD or FPU register access */
int cpu_vcpu_emulate_simd_fp_regs(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate FPU execution faults from Aarch32 */
int cpu_vcpu_emulate_fpexec_a32(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate FPU execution faults from Aarch64 */
int cpu_vcpu_emulate_fpexec_a64(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate MRC (or VMRS) to CP10 instruction */
int cpu_vcpu_emulate_vmrs(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate MCRR/MRRC to CP14 instruction */
int cpu_vcpu_emulate_mcrr_mrrc_cp14(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate HVC instruction for A32 guest */
int cpu_vcpu_emulate_hvc32(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate HVC instruction for A64 guest */
int cpu_vcpu_emulate_hvc64(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate SMC instruction for A32 guest */
int cpu_vcpu_emulate_smc32(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate SMC instruction for A64 guest */
int cpu_vcpu_emulate_smc64(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate MRS/MSR/System Regs access */
int cpu_vcpu_emulate_msr_mrs_system(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Emulate Load instruction */
int cpu_vcpu_emulate_load(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss, physical_addr_t ipa);

/** Emulate Store instruction */
int cpu_vcpu_emulate_store(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss, physical_addr_t ipa);

#endif
