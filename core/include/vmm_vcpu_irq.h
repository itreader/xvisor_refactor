/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_vcpu_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for vcpu interrupts
 */
#ifndef _VMM_VCPU_IRQ_H__
#define _VMM_VCPU_IRQ_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/** Process interrupts for current vcpu
 *  Note: Don't call this function directly it's meant to be called
 *  from vmm_scheduler only.
 */
void vmm_vcpu_irq_process(vmm_vcpu_t *vcpu, arch_regs_t *regs);

/** Assert an irq to given vcpu */
void vmm_vcpu_irq_assert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason);

/** Force clear irq of given vcpu
 *  Note: Given vcpu has to be the curent vcpu.
 */
void vmm_vcpu_irq_clear(vmm_vcpu_t *vcpu, uint32_t irq_no);

/** Deassert active irq of given vcpu */
void vmm_vcpu_irq_deassert(vmm_vcpu_t *vcpu, uint32_t irq_no);

/** Forcefully resume given VCPU if waiting for irq */
int vmm_vcpu_irq_wait_resume(vmm_vcpu_t *vcpu);

/** Wait for irq on given vcpu with some timeout
 *  Note: Given VCPU has to be the curent VCPU.
 */
int vmm_vcpu_irq_wait_timeout(vmm_vcpu_t *vcpu, uint64_t nsecs);

/** Wait for irq on given vcpu indefinetly (no timeout) */
#define vmm_vcpu_irq_wait(vcpu) vmm_vcpu_irq_wait_timeout(vcpu, 0)

/** Current state of Wait for irq on given vcpu */
bool vmm_vcpu_irq_wait_state(vmm_vcpu_t *vcpu);

/** Initialize interrupts for given vcpu */
int vmm_vcpu_irq_init(vmm_vcpu_t *vcpu);

/** Deinitialize interrupts for given vcpu */
int vmm_vcpu_irq_deinit(vmm_vcpu_t *vcpu);

#endif
