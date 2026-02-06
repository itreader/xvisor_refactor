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
 * @file cpu_vcpu_helper.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU helper functions
 */
#ifndef _CPU_VCPU_HELPER_H__
#define _CPU_VCPU_HELPER_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/* Function to halt VCPU */
void cpu_vcpu_halt(vmm_vcpu_t *vcpu, arch_regs_t *regs);

/* Function to read a VCPU register of given mode */
uint32_t cpu_vcpu_regmode_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t mode, uint32_t reg_num);

/* Function to write a VCPU register of given mode */
void cpu_vcpu_regmode_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t mode, uint32_t reg_num, uint32_t reg_val);

/* Function to read a VCPU register of current mode */
uint32_t cpu_vcpu_reg_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t reg_num);

/* Function to write a VCPU register of current mode */
void cpu_vcpu_reg_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t reg_num, uint32_t reg_val);

/* Function to retrieve SPSR of a VCPU */
uint32_t cpu_vcpu_spsr_retrieve(vmm_vcpu_t *vcpu, uint32_t mode);

/* Function to update SPSR of a VCPU */
int cpu_vcpu_spsr_update(vmm_vcpu_t *vcpu, uint32_t mode, uint32_t new_spsr);

/* Function to dump user registers */
void cpu_vcpu_dump_user_reg(arch_regs_t *regs);

#endif
