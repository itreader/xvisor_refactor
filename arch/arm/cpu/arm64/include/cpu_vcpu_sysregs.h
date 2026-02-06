/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file cpu_vcpu_sysregs.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header file for VCPU sysreg, cp15 and cp14 emulation
 */
#ifndef _CPU_VCPU_SYSREGS_H__
#define _CPU_VCPU_SYSREGS_H__

#include <vmm_char_device.h>
#include <vmm_manager.h>
#include <vmm_types.h>

/** Read one CP15 register */
bool cpu_vcpu_cp15_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t *data);

/** Write one CP15 register */
bool cpu_vcpu_cp15_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t data);

/** Read one CP14 register */
bool cpu_vcpu_cp14_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t *data);

/** Write one CP14 register */
bool cpu_vcpu_cp14_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t data);

/** Read one sysreg */
bool cpu_vcpu_sysregs_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t iss_sysreg, uint64_t *data);

/** Write one sysreg */
bool cpu_vcpu_sysregs_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t iss_sysreg, uint64_t data);

/** Save sysregs context */
void cpu_vcpu_sysregs_save(vmm_vcpu_t *vcpu);

/** Restore sysregs context */
void cpu_vcpu_sysregs_restore(vmm_vcpu_t *vcpu);

/** Print sysregs context for given VCPU */
void cpu_vcpu_sysregs_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Initialize sysregs context for given VCPU */
int cpu_vcpu_sysregs_init(vmm_vcpu_t *vcpu, uint32_t cpuid);

/** DeInitialize sysregs context for given VCPU */
int cpu_vcpu_sysregs_deinit(vmm_vcpu_t *vcpu);

#endif /* _CPU_VCPU_SYSREGS_H__ */
