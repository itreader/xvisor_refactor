/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_vcpu_timer.h
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief generic timer for vcpu
 */

#ifndef _CPU_VCPU_TIMER_H__
#define _CPU_VCPU_TIMER_H__

#include <vmm_types.h>

struct vmm_vcpu;
typedef struct vmm_vcpu vmm_vcpu_t;
bool                    cpu_vcpu_timer_vs_irq(vmm_vcpu_t *vcpu);
uint64_t                cpu_vcpu_timer_vs_cycle(vmm_vcpu_t *vcpu);
void                    cpu_vcpu_timer_vs_restart(vmm_vcpu_t *vcpu);
void                    cpu_vcpu_timer_vs_start(vmm_vcpu_t *vcpu, uint64_t next_vs_cycle);

void cpu_vcpu_timer_start(vmm_vcpu_t *vcpu, uint64_t next_cycle);
void cpu_vcpu_timer_delta_update(vmm_vcpu_t *vcpu, bool nested_virt);
void cpu_vcpu_timer_save(vmm_vcpu_t *vcpu);
void cpu_vcpu_timer_restore(vmm_vcpu_t *vcpu);
int  cpu_vcpu_timer_init(vmm_vcpu_t *vcpu, void **timer);
int  cpu_vcpu_timer_deinit(vmm_vcpu_t *vcpu, void **timer);

#endif
