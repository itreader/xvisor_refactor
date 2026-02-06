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
 * @file cpu_vcpu_vfp.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Sting Cheng (sting.cheng@gmail.com)
 * @brief Header file for VCPU VFP emulation
 */
#ifndef _CPU_VCPU_VFP_H__
#define _CPU_VCPU_VFP_H__

#include <vmm_char_device.h>
#include <vmm_manager.h>
#include <vmm_types.h>

/** Save VFP context for given VCPU */
void cpu_vcpu_vfp_save(vmm_vcpu_t *vcpu);

/** Restore VFP context for given VCPU */
void cpu_vcpu_vfp_restore(vmm_vcpu_t *vcpu);

/** Handle VFP trap for given VCPU */
int cpu_vcpu_vfp_trap(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t il, uint32_t iss);

/** Print VFP context for given VCPU */
void cpu_vcpu_vfp_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Initialize VFP context for given VCPU */
int cpu_vcpu_vfp_init(vmm_vcpu_t *vcpu);

/** DeInitialize VFP context for given VCPU */
int cpu_vcpu_vfp_deinit(vmm_vcpu_t *vcpu);

#endif /* _CPU_VCPU_VFP_H__ */
