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
 * @file vmm_scheduler.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for hypervisor scheduler
 */
#ifndef _VMM_SCHEDULER_H__
#define _VMM_SCHEDULER_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/** Disable pre-emption of current VCPU */
void vmm_scheduler_preempt_disable(void);

/** Enable pre-emption of current VCPU */
void vmm_scheduler_preempt_enable(void);

/** Preempt Orphan VCPU (Must be called from somewhere) */
void vmm_scheduler_preempt_orphan(arch_regs_t *regs);

/** Force re-scheduling on given host CPU */
int vmm_scheduler_force_resched(uint32_t host_cpu);

/** Retrive general VCPU statistics */
int vmm_scheduler_stats(
    vmm_vcpu_t *vcpu, uint32_t *state, uint8_t *priority, uint32_t *host_cpu, uint32_t *reset_count, uint64_t *last_reset_nsecs,
    uint64_t *ready_nsecs, uint64_t *running_nsecs, uint64_t *paused_nsecs, uint64_t *halted_nsecs, uint64_t *system_nsecs);

/** Change the vcpu state
 *  (Do not call this function directly.)
 *  (Always prefer vmm_manager_vcpu_xxx() APIs for vcpu state change.)
 */
int vmm_scheduler_state_change(vmm_vcpu_t *vcpu, uint32_t new_state);

/** Retrive host CPU assigned to given VCPU */
int vmm_scheduler_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu);

/** Check host CPU assigned to given VCPU is current host CPU */
bool vmm_scheduler_check_current_hcpu(vmm_vcpu_t *vcpu);

/** Update host CPU assigned to given VCPU */
int vmm_scheduler_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu);

/** Enter IRQ Context (Must be called from somewhere) */
void vmm_scheduler_irq_enter(arch_regs_t *regs, bool vcpu_context);

/** Retreive register pointer saved for IRQ/Normal Context */
arch_regs_t *vmm_scheduler_irq_regs(void);

/** Exit IRQ Context (Must be called from somewhere) */
void vmm_scheduler_irq_exit(arch_regs_t *regs);

/** Check whether we are in IRQ context */
bool vmm_scheduler_irq_context(void);

/** Check whether we are in Orphan VCPU context */
bool vmm_scheduler_orphan_context(void);

/** Check whether we are in Normal VCPU context */
bool vmm_scheduler_normal_context(void);

/** Count number ready VCPUs with given priority on a host CPU */
uint32_t vmm_scheduler_ready_count(uint32_t host_cpu, uint8_t priority);

/** Get scheduler sampling period in nanosecs */
uint64_t vmm_scheduler_get_sample_period(uint32_t host_cpu);

/** Set scheduler sampling period in nanosecs */
void vmm_scheduler_set_sample_period(uint32_t host_cpu, uint64_t period);

/** Last sampled irq time in nanosecs for given host CPU */
uint64_t vmm_scheduler_irq_time(uint32_t host_cpu);

/** Last sampled idle time in nanosecs for given host CPU */
uint64_t vmm_scheduler_idle_time(uint32_t host_cpu);

/** Retrive idle vcpu for given host CPU */
vmm_vcpu_t *vmm_scheduler_idle_vcpu(uint32_t host_cpu);

/** Retrive current vcpu */
vmm_vcpu_t *vmm_scheduler_current_vcpu(void);

/** Retrive current priority */
uint8_t vmm_scheduler_current_priority(void);

/** Retrive current guest */
struct vmm_guest *vmm_scheduler_current_guest(void);

/** Yield current vcpu (Should not be called in IRQ context) */
/* 主动让出VCPU的调度 */
void vmm_scheduler_yield(void);

/** Initialize scheduler */
int vmm_scheduler_init(void);

#endif
