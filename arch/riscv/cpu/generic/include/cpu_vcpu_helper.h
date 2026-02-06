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
 * @file cpu_vcpu_helper.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU helper functions
 */
#ifndef _CPU_VCPU_HELPER_H__
#define _CPU_VCPU_HELPER_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/** Function to update environment configuration */
void cpu_vcpu_envcfg_update(vmm_vcpu_t *vcpu, bool nested_virt);

/** Function to update interrupt delegation */
void cpu_vcpu_irq_deleg_update(vmm_vcpu_t *vcpu, bool nested_virt);

/** Function to update G-stage page table */
void cpu_vcpu_gstage_update(vmm_vcpu_t *vcpu, bool nested_virt);

/** Function to dump general registers */
void cpu_vcpu_dump_general_regs(vmm_char_device_t *cdev, arch_regs_t *regs);

/** Function to dump private registers */
void cpu_vcpu_dump_private_regs(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Function to dump exception registers */
void cpu_vcpu_dump_exception_regs(vmm_char_device_t *cdev, uint64_t scause, uint64_t stval, uint64_t htval, uint64_t htinst);

#endif
