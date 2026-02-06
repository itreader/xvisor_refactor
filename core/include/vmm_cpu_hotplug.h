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
 * @file vmm_cpu_hotplug.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for CPU hotplug notifiers
 */

#ifndef __VMM_CPU_HOTPLUG_H__
#define __VMM_CPU_HOTPLUG_H__

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_types.h>

enum vmm_cpu_hotplug_states {
    VMM_CPU_HOTPLUG_STATE_OFFLINE = 0,
    VMM_CPU_HOTPLUG_STATE_HOST_IRQ,
    VMM_CPU_HOTPLUG_STATE_CLOCKSOURCE,
    VMM_CPU_HOTPLUG_STATE_CLOCKCHIP,
    VMM_CPU_HOTPLUG_STATE_TIMER,
    VMM_CPU_HOTPLUG_STATE_DELAY,
    VMM_CPU_HOTPLUG_STATE_SMP_SYNC_IPI,
    VMM_CPU_HOTPLUG_STATE_SCHEDULER,
    VMM_CPU_HOTPLUG_STATE_SMP_ASYNC_IPI,
    VMM_CPU_HOTPLUG_STATE_WORKQUEUE
};

#define VMM_CPU_HOTPLUG_STATE_ONLINE U32_MAX

struct vmm_cpu_hotplug_notify;
typedef struct vmm_cpu_hotplug_notify vmm_cpu_hotplug_notify_t;

struct vmm_cpu_hotplug_notify {
    /* Private */
    double_list_t               head;
    /* Public */
    enum vmm_cpu_hotplug_states state;
    char                        name[VMM_FIELD_NAME_SIZE];
    int (*startup)(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu);
    int (*teardown)(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu);
};

/** Get hotplug state of given CPU
 *  Note: This function can be called even before vmm_cpu_hotplug_init()
 *  is called.
 */
uint32_t vmm_cpu_hotplug_get_state(uint32_t cpu);

/* Set specified hotplug state for current CPU */
int vmm_cpu_hotplug_set_state(uint32_t state);

/** Register CPU hotplug notifiers */
int vmm_cpu_hotplug_register(vmm_cpu_hotplug_notify_t *cpu_hotplug, bool invoke_startup);

/** UnRegister CPU hotplug notifiers */
int vmm_cpu_hotplug_unregister(vmm_cpu_hotplug_notify_t *cpu_hotplug);

/** Initialize CPU hotplug notifiers */
int vmm_cpu_hotplug_init(void);

#endif /* __VMM_CPU_HOTPLUG_H__ */
