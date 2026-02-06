/**
 * Copyright (c) 2011-2013 Sting Cheng.
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
 * @file cpu_vcpu_cp14.h
 * @author Sting Cheng (sting.cheng@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for VCPU cp14 (Debug, Trace, and ThumbEE) emulation
 */
#ifndef _CPU_VCPU_CP14_H__
#define _CPU_VCPU_CP14_H__

#include <vmm_char_device.h>
#include <vmm_manager.h>
#include <vmm_types.h>

/** Read one registers from CP14 */
bool cpu_vcpu_cp14_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t *data);

/** Write one registers to CP14 */
bool cpu_vcpu_cp14_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRn, uint32_t CRm, uint32_t data);

/** Save CP14 context for given VCPU */
void cpu_vcpu_cp14_save(vmm_vcpu_t *vcpu);

/** Restore CP14 context for given VCPU */
void cpu_vcpu_cp14_restore(vmm_vcpu_t *vcpu);

/** Print CP14 context for given VCPU */
void cpu_vcpu_cp14_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Initialize CP14 context for given VCPU */
int cpu_vcpu_cp14_init(vmm_vcpu_t *vcpu);

/** DeInitialize CP14 context for given VCPU */
int cpu_vcpu_cp14_deinit(vmm_vcpu_t *vcpu);

#endif /* _CPU_VCPU_CP14_H__ */
