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
 * @file cpu_vcpu_ptrauth.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for VCPU Pointer Authentication
 */
#ifndef _CPU_VCPU_PTRAUTH_H__
#define _CPU_VCPU_PTRAUTH_H__

#include <vmm_char_device.h>
#include <vmm_manager.h>
#include <vmm_types.h>

/** Save PTRAUTH context for given VCPU */
void cpu_vcpu_ptrauth_save(vmm_vcpu_t *vcpu);

/** Restore PTRAUTH context for given VCPU */
void cpu_vcpu_ptrauth_restore(vmm_vcpu_t *vcpu);

/** Print PTRAUTH context for given VCPU */
void cpu_vcpu_ptrauth_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Initialize PTRAUTH context for given VCPU */
int cpu_vcpu_ptrauth_init(vmm_vcpu_t *vcpu);

/** DeInitialize PTRAUTH context for given VCPU */
int cpu_vcpu_ptrauth_deinit(vmm_vcpu_t *vcpu);

#endif /* _CPU_VCPU_PTRAUTH_H__ */
