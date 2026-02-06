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
 * @file arch_vcpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific VCPU operations
 */
#ifndef _ARCH_VCPU_H__
#define _ARCH_VCPU_H__

#include <arch_regs.h>
#include <vmm_types.h>

struct vmm_char_device;
struct vmm_vcpu;
typedef struct vmm_vcpu        vmm_vcpu_t;
typedef struct vmm_char_device vmm_char_device_t;

/** Architecture specific VCPU Initialization */
int arch_vcpu_init(vmm_vcpu_t *vcpu);

/** Architecture specific VCPU De-initialization (or Cleanup) */
int arch_vcpu_deinit(vmm_vcpu_t *vcpu);

/** VCPU context switch function
 *  NOTE: The vcpu_on_thread pointer is VCPU being switched out.
 *  NOTE: The vcpu pointer is VCPU being switched in.
 *  NOTE: This function is called with sched_lock held for
 *  both vcpu_on_thread and vcpu.
 *  NOTE: The pointer to arch_regs_t represents register state
 *  saved by interrupt handlers or arch_vcpu_preempt_orphan().
 */
void arch_vcpu_switch(vmm_vcpu_t *vcpu_on_thread, vmm_vcpu_t *vcpu, arch_regs_t *regs);

/** VCPU post context switch function
 *  NOTE: The vcpu pointer is VCPU being switched in.
 *  NOTE: The pointer to arch_regs_t represents register state
 *  saved by interrupt handlers or arch_vcpu_preempt_orphan().
 */
void arch_vcpu_post_switch(vmm_vcpu_t *vcpu, arch_regs_t *regs);

/** Forcefully preempt current Orphan VCPU (or current Thread)
 *  NOTE: This functions is always called with irqs saved
 *  on the stack of current Orphan VCPU.
 *  NOTE: The core code expects that this function will save
 *  context and call vmm_scheduler_preempt_orphan() with a
 *  pointer to saved arch_regs_t.
 */
void arch_vcpu_preempt_orphan(void);

/** Print architecture specific registers of a VCPU */
void arch_vcpu_regs_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Print architecture specific stats for a VCPU */
void arch_vcpu_stat_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu);

/** Get count of VCPU interrupts */
uint32_t arch_vcpu_irq_count(vmm_vcpu_t *vcpu);

/** Get priority for given VCPU interrupt number */
uint32_t arch_vcpu_irq_priority(vmm_vcpu_t *vcpu, uint32_t irq_no);

/** Assert VCPU interrupt
 *  NOTE: This function is called asynchronusly in any context.
 *  NOTE: This function needs to protect any common ressource that could
 *  be used concurently by other arch_vcpu_irq_xyz() functions.
 */
int arch_vcpu_irq_assert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason);

/** Check if we can execute multiple VCPU interrupts
 *  NOTE: This function is always called in context of the VCPU (i.e.
 *  in Normal context).
 *  NOTE: This function needs to protect any common ressource that could
 *  be used concurently by other arch_vcpu_irq_xyz() functions.
 */
bool arch_vcpu_irq_can_execute_multiple(vmm_vcpu_t *vcpu, arch_regs_t *regs);

/** Execute VCPU interrupt
 *  NOTE: This function is always called in context of the VCPU (i.e.
 *  in Normal context).
 *  NOTE: This function needs to protect any common ressource that could
 *  be used concurently by other arch_vcpu_irq_xyz() functions.
 */
int arch_vcpu_irq_execute(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t irq_no, uint64_t reason);

/** Force clear VCPU interrupt
 *  NOTE: This function is always called in context of the VCPU (i.e.
 *  in Normal context).
 *  NOTE: This function needs to protect any common ressource that could
 *  be used concurently by other arch_vcpu_irq_xyz() functions.
 */
int arch_vcpu_irq_clear(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason);

/** Deassert VCPU interrupt
 *  NOTE: This function is called asynchronusly in any context.
 *  NOTE: This function needs to protect any common ressource that could
 *  be used concurently by other arch_vcpu_irq_xyz() functions.
 */
int arch_vcpu_irq_deassert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason);

/** VCPU IRQ pending status
 *  NOTE: This function is always called in context of the VCPU (i.e.
 *  in Normal context).
 *  NOTE: This function needs to protect any common ressource that could
 *  be used concurently by other arch_vcpu_irq_xyz() functions.
 */
bool arch_vcpu_irq_pending(vmm_vcpu_t *vcpu);

#endif
