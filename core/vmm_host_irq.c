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
 * @file vmm_host_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for host interrupts
 */

#include <arch_cpu_irq.h>
#include <arch_host_irq.h>
#include <libs/stringlib.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_extend_irq.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

struct vmm_host_irqs_ctrl {
    vmm_spinlock_t       lock;
    struct vmm_host_irq *irq;
    uint32_t (*active)(uint32_t, uint32_t);
    vmm_cpumask_t default_affinity;
};

static struct vmm_host_irqs_ctrl hirqctrl;

void vmm_handle_per_cpu_irq(struct vmm_host_irq *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    struct vmm_host_irq_action *act;

    if (irq->chip && irq->chip->irq_ack) {
        irq->chip->irq_ack(irq);
    }

    vmm_read_lock_irq_save_lite(&irq->action_lock[cpu], flags);
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
            break;
        }
    }
    vmm_read_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    if (irq->chip && irq->chip->irq_eoi) {
        irq->chip->irq_eoi(irq);
    }
}

void vmm_handle_fast_eoi(struct vmm_host_irq *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    struct vmm_host_irq_action *act;

    vmm_read_lock_irq_save_lite(&irq->action_lock[cpu], flags);
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
            break;
        }
    }
    vmm_read_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    if (irq->chip && irq->chip->irq_eoi) {
        irq->chip->irq_eoi(irq);
    }
}

void vmm_handle_level_irq(struct vmm_host_irq *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    struct vmm_host_irq_action *act;

    if (irq->chip) {
        if (irq->chip->irq_mask_ack) {
            irq->chip->irq_mask_ack(irq);
        } else {
            if (irq->chip->irq_mask) {
                irq->chip->irq_mask(irq);
            }

            if (irq->chip->irq_ack) {
                irq->chip->irq_ack(irq);
            }
        }
    }

    vmm_read_lock_irq_save_lite(&irq->action_lock[cpu], flags);
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
            break;
        }
    }
    vmm_read_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    if (irq->chip) {
        if (!(irq->per_cpu_state[cpu] & VMM_PERCPU_IRQ_STATE_MASKED) && irq->chip->irq_unmask) {
            irq->chip->irq_unmask(irq);
        }
    }
}

void vmm_handle_simple_irq(struct vmm_host_irq *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    struct vmm_host_irq_action *act;

    vmm_read_lock_irq_save_lite(&irq->action_lock[cpu], flags);
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
            break;
        }
    }
    vmm_read_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);
}

struct vmm_host_irq *vmm_host_irq_get(uint32_t hirq)
{
    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return &hirqctrl.irq[hirq];
    }

    return __vmm_host_extend_irq_get(hirq);
}

int vmm_host_generic_irq_exec(uint32_t hirq_no)
{
    uint32_t             cpu;
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq_no))) {
        return VMM_ENOTAVAIL;
    }

    cpu = vmm_smp_processor_id();
    irq->count[cpu]++;
    irq->per_cpu_state[cpu] |= VMM_PERCPU_IRQ_STATE_IN_PROG;

    if (irq->handler) {
        irq->handler(irq, cpu, irq->handler_data);
    }

    irq->per_cpu_state[cpu] &= ~VMM_PERCPU_IRQ_STATE_IN_PROG;

    return VMM_OK;
}

int vmm_host_active_irq_exec(uint32_t cpu_irq_no)
{
    uint32_t hirq_no, exec_count;

    if (!hirqctrl.active) {
        return VMM_ENOTAVAIL;
    }

    /* We only process 16 active host irqs at a time.
     * This avoids infinite irq processing loop caused by
     * spurious interrupts on buggy hardware.
     */
    exec_count = 16;
    hirq_no    = hirqctrl.active(cpu_irq_no, UINT_MAX);

    while (hirq_no < CONFIG_HOST_IRQ_COUNT) {
        vmm_host_generic_irq_exec(hirq_no);

        if (!exec_count--) {
            break;
        }

        hirq_no = hirqctrl.active(cpu_irq_no, hirq_no);
    }

    return VMM_OK;
}

void vmm_host_irq_set_active_callback(uint32_t (*active)(uint32_t, uint32_t))
{
    hirqctrl.active = active;
}

uint32_t vmm_host_irq_count(void)
{
    return CONFIG_HOST_IRQ_COUNT;
}

int __vmm_host_irq_set_hwirq(uint32_t hirq, uint32_t hwirq)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_EFAIL;
    }

    if (!(irq->state & VMM_IRQ_STATE_EXTENDED)) {
        irq->hwirq = hwirq;
    }

    return VMM_OK;
}

uint32_t vmm_host_irq_get_hwirq(uint32_t hirq)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return hirq;
    }

    return irq->hwirq;
}

int vmm_host_irq_set_chip(uint32_t hirq, struct vmm_host_irq_chip *chip)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_EFAIL;
    }

    irq->chip = chip;
    return VMM_OK;
}

struct vmm_host_irq_chip *vmm_host_irq_get_chip(struct vmm_host_irq *irq)
{
    return (irq) ? irq->chip : NULL;
}

int vmm_host_irq_set_chip_data(uint32_t hirq, void *chip_data)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_EFAIL;
    }

    irq->chip_data = chip_data;
    return VMM_OK;
}

void *vmm_host_irq_get_chip_data(struct vmm_host_irq *irq)
{
    return (irq) ? irq->chip_data : NULL;
}

int vmm_host_irq_set_msi_data(uint32_t hirq, void *msi_data)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_EFAIL;
    }

    irq->msi_data = msi_data;
    return VMM_OK;
}

void *vmm_host_irq_get_msi_data(struct vmm_host_irq *irq)
{
    return (irq) ? irq->msi_data : NULL;
}

int vmm_host_irq_set_handler(uint32_t hirq, vmm_host_irq_handler_t handler)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_EFAIL;
    }

    irq->handler = handler;
    return VMM_OK;
}

vmm_host_irq_handler_t vmm_host_irq_get_handler(uint32_t hirq)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return NULL;
    }

    return irq->handler;
}

int vmm_host_irq_set_handler_data(uint32_t hirq, void *data)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_EFAIL;
    }

    irq->handler_data = data;
    return VMM_OK;
}

void *vmm_host_irq_get_handler_data(uint32_t hirq)
{
    struct vmm_host_irq *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return NULL;
    }

    return irq->handler_data;
}

int vmm_host_irq_set_affinity(uint32_t hirq, const vmm_cpumask_t *dest, bool force)
{
    int                  rc = VMM_OK;
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (!dest || vmm_host_irq_is_per_cpu(irq)) {
        return VMM_EINVALID;
    }

    if (irq->chip && irq->chip->irq_set_affinity) {
        irq->state |= VMM_IRQ_STATE_AFFINITY_SET;
        rc = irq->chip->irq_set_affinity(irq, dest, force);
    }

    if (rc == VMM_OK) {
        vmm_cpumask_copy(&irq->affinity, dest);
    }

    return rc;
}

int vmm_host_irq_set_type(uint32_t hirq, uint32_t type)
{
    int                  rc = VMM_EFAIL;
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    type &= VMM_IRQ_TYPE_SENSE_MASK;

    if (type == VMM_IRQ_TYPE_NONE) {
        return VMM_OK;
    }

    if (irq->chip && irq->chip->irq_set_type) {
        rc = irq->chip->irq_set_type(irq, type);
    } else {
        return VMM_OK;
    }

    if (rc == VMM_OK) {
        irq->state &= ~VMM_IRQ_STATE_TRIGGER_MASK;
        irq->state |= type;

        if (type & VMM_IRQ_TYPE_LEVEL_MASK) {
            irq->state |= VMM_IRQ_STATE_LEVEL;
        } else {
            irq->state &= ~VMM_IRQ_STATE_LEVEL;
        }
    }

    return rc;
}

int vmm_host_irq_mark_per_cpu(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_PER_CPU;
    return VMM_OK;
}

int vmm_host_irq_unmark_per_cpu(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_PER_CPU;
    return VMM_OK;
}

int vmm_host_irq_mark_routed(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_ROUTED;
    return VMM_OK;
}

int vmm_host_irq_unmark_routed(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_ROUTED;
    return VMM_OK;
}

int vmm_host_irq_get_routed_state(uint32_t hirq, uint32_t *val, uint32_t mask)
{
    struct vmm_host_irq      *irq;
    struct vmm_host_irq_chip *chip;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (NULL == (chip = vmm_host_irq_get_chip(irq))) {
        return VMM_ENOTAVAIL;
    }

    if (!chip->irq_get_routed_state) {
        return VMM_EINVALID;
    }

    *val = chip->irq_get_routed_state(irq, mask);

    return VMM_OK;
}

int vmm_host_irq_set_routed_state(uint32_t hirq, uint32_t val, uint32_t mask)
{
    struct vmm_host_irq      *irq;
    struct vmm_host_irq_chip *chip;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (NULL == (chip = vmm_host_irq_get_chip(irq))) {
        return VMM_ENOTAVAIL;
    }

    if (!chip->irq_set_routed_state) {
        return VMM_EINVALID;
    }

    chip->irq_set_routed_state(irq, val, mask);

    return VMM_OK;
}

int vmm_host_irq_mark_ipi(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_IPI;
    return VMM_OK;
}

int vmm_host_irq_unmark_ipi(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_IPI;
    return VMM_OK;
}

int vmm_host_irq_mark_chained(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_CHAINED;
    return VMM_OK;
}

int vmm_host_irq_unmark_chained(uint32_t hirq)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_CHAINED;
    return VMM_OK;
}

bool vmm_host_irq_is_masked(struct vmm_host_irq *irq)
{
    uint32_t per_cpu_state;

    if (!irq) {
        return FALSE;
    }

    per_cpu_state = irq->per_cpu_state[vmm_smp_processor_id()];
    return (per_cpu_state & VMM_PERCPU_IRQ_STATE_MASKED) ? TRUE : FALSE;
}

int vmm_host_irq_unmask(uint32_t hirq)
{
    uint32_t             cpu;
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (irq->chip) {
        if (irq->chip->irq_enable) {
            irq->chip->irq_enable(irq);
        } else if (irq->chip->irq_unmask) {
            irq->chip->irq_unmask(irq);
        }

        if (vmm_host_irq_is_per_cpu(irq)) {
            irq->per_cpu_state[vmm_smp_processor_id()] &= ~VMM_PERCPU_IRQ_STATE_MASKED;
        } else {
            for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
                irq->per_cpu_state[cpu] &= ~VMM_PERCPU_IRQ_STATE_MASKED;
            }
        }
    }

    return VMM_OK;
}

int vmm_host_irq_mask(uint32_t hirq)
{
    uint32_t             cpu;
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (irq->chip) {
        if (irq->chip->irq_disable) {
            irq->chip->irq_disable(irq);
        } else if (irq->chip->irq_mask) {
            irq->chip->irq_mask(irq);
        }

        if (vmm_host_irq_is_per_cpu(irq)) {
            irq->per_cpu_state[vmm_smp_processor_id()] |= VMM_PERCPU_IRQ_STATE_MASKED;
        } else {
            for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
                irq->per_cpu_state[cpu] |= VMM_PERCPU_IRQ_STATE_MASKED;
            }
        }
    }

    return VMM_OK;
}

int vmm_host_irq_raise(uint32_t hirq, const vmm_cpumask_t *dest)
{
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (irq->chip && irq->chip->irq_raise) {
        irq->chip->irq_raise(irq, dest);
    }

    return VMM_OK;
}

int vmm_host_irq_compose_msi_msg(uint32_t hirq, struct vmm_msi_msg *msg)
{
    struct vmm_host_irq *irq;

    if (!msg) {
        return VMM_EINVALID;
    }

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (!irq->chip || !irq->chip->irq_compose_msi_msg) {
        return VMM_ENOSYS;
    }

    irq->chip->irq_compose_msi_msg(irq, msg);
    return VMM_OK;
}

int vmm_host_irq_find(uint32_t hirq_start, uint32_t state_mask, uint32_t *hirq)
{
    uint32_t             ite;
    bool                 found = FALSE;
    struct vmm_host_irq *irq;

    if ((CONFIG_HOST_IRQ_COUNT <= hirq_start) || !hirq) {
        return VMM_EINVALID;
    }

    if (!state_mask) {
        return VMM_ENOTAVAIL;
    }

    for (ite = hirq_start; ite < CONFIG_HOST_IRQ_COUNT; ite++) {
        if (NULL == (irq = vmm_host_irq_get(ite))) {
            continue;
        }

        if ((irq->state & state_mask) == state_mask) {
            found = TRUE;
            *hirq = ite;
            break;
        }
    }

    return (found) ? VMM_OK : VMM_ENOTAVAIL;
}

static int host_irq_register(struct vmm_host_irq *irq, const char *name, vmm_host_irq_function_t func, void *dev, uint32_t cpu)
{
    bool                        found;
    irq_flags_t                 flags;
    struct vmm_host_irq_action *act;

    vmm_write_lock_irq_save_lite(&irq->action_lock[cpu], flags);

    found = FALSE;
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->dev == dev) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);
        return VMM_EFAIL;
    }

    irq->name = name;
    act       = vmm_zalloc(sizeof(struct vmm_host_irq_action));

    if (!act) {
        vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);
        return VMM_ENOMEM;
    }

    INIT_LIST_HEAD(&act->head);
    act->func = func;
    act->dev  = dev;

    list_add_tail(&act->head, &irq->action_list[cpu]);

    vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    return VMM_OK;
}

int vmm_host_irq_register(uint32_t hirq, const char *name, vmm_host_irq_function_t func, void *dev)
{
    int                  rc;
    uint32_t             cpu;
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    if (vmm_host_irq_is_per_cpu(irq)) {
        rc = host_irq_register(irq, name, func, dev, vmm_smp_processor_id());

        if (rc) {
            return rc;
        }
    } else {
        for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
            rc = host_irq_register(irq, name, func, dev, cpu);

            if (rc) {
                return rc;
            }
        }
    }

    return vmm_host_irq_unmask(hirq);
}

static int host_irq_unregister(struct vmm_host_irq *irq, void *dev, uint32_t cpu, bool *disable)
{
    bool                        found;
    irq_flags_t                 flags;
    struct vmm_host_irq_action *act;

    vmm_write_lock_irq_save_lite(&irq->action_lock[cpu], flags);
    found = FALSE;
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->dev == dev) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);
        return VMM_EFAIL;
    }

    list_del(&act->head);
    vmm_free(act);

    if (list_empty(&irq->action_list[cpu])) {
        *disable = TRUE;
    }

    vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    return VMM_OK;
}

int vmm_host_irq_unregister(uint32_t hirq, void *dev)
{
    int                  rc;
    uint32_t             cpu;
    bool                 disable;
    struct vmm_host_irq *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ENOTAVAIL;
    }

    disable = FALSE;

    if (vmm_host_irq_is_per_cpu(irq)) {
        rc = host_irq_unregister(irq, dev, vmm_smp_processor_id(), &disable);

        if (rc) {
            return rc;
        }
    } else {
        for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
            rc = host_irq_unregister(irq, dev, cpu, &disable);

            if (rc) {
                return rc;
            }
        }
    }

    if (disable) {
        return vmm_host_irq_mask(hirq);
    }

    return VMM_OK;
}

int __weak arch_host_irq_init(void)
{
    /* Default weak implementation in-case
     * architecture does not provide one.
     */
    return VMM_OK;
}

/**
 * Initialize a vmm_host_irq structure
 * Warning: The associated IRQ must be disabled!
 */
void __vmm_host_irq_init_desc(struct vmm_host_irq *irq, uint32_t hirq, uint32_t hwirq, uint32_t state)
{
    uint32_t cpu = 0;

    if (!irq) {
        return;
    }

    irq->num   = hirq;
    irq->hwirq = hwirq;
    irq->name  = NULL;
    irq->state = state;
    irq->state |= VMM_IRQ_TYPE_NONE;
    vmm_cpumask_copy(&irq->affinity, &hirqctrl.default_affinity);

    for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
        irq->per_cpu_state[cpu] = VMM_PERCPU_IRQ_STATE_MASKED;
        irq->count[cpu]         = 0;
    }

    irq->chip         = NULL;
    irq->chip_data    = NULL;
    irq->msi_data     = NULL;
    irq->handler      = NULL;
    irq->handler_data = NULL;

    for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
        INIT_RW_LOCK(&irq->action_lock[cpu]);
        INIT_LIST_HEAD(&irq->action_list[cpu]);
    }
}

static int host_irq_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int ret;

    /* Initialize board specific PIC */
    if ((ret = arch_host_irq_init())) {
        return ret;
    }

    /* Setup interrupts in CPU */
    if ((ret = arch_cpu_irq_setup())) {
        return ret;
    }

    /* Enable interrupts in CPU */
    arch_cpu_irq_enable();

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t host_irq_cpu_hotplug = {
    .name    = "HOST_IRQ",
    .state   = VMM_CPU_HOTPLUG_STATE_HOST_IRQ,
    .startup = host_irq_startup,
};

static void __init host_irq_nidtable_found(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int                 err;
    vmm_host_irq_init_t init_fn = match->data;

    if (!init_fn) {
        return;
    }

    err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE

    if (err) {
        vmm_printf("%s: CPU%d Init %s node failed (error %d)\n", __func__, vmm_smp_processor_id(), node->name, err);
    }

#else
    (void)err;
#endif
}

int __init vmm_host_irq_init(void)
{
    int                                  ret;
    uint32_t                             ite;
    const struct vmm_device_tree_nodeid *matches;

    /* Clear the memory of control structure */
    memset(&hirqctrl, 0, sizeof(hirqctrl));

    /* Initialize spin lock */
    INIT_SPIN_LOCK(&hirqctrl.lock);

    /* Setup default host IRQ affinity */
    vmm_cpumask_setall(&hirqctrl.default_affinity);

    /* Allocate memory for irq array */
    hirqctrl.irq = vmm_malloc(sizeof(struct vmm_host_irq) * CONFIG_HOST_IRQ_COUNT);

    if (!hirqctrl.irq) {
        return VMM_ENOMEM;
    }

    /* Reset the handler array */
    for (ite = 0; ite < CONFIG_HOST_IRQ_COUNT; ite++) {
        __vmm_host_irq_init_desc(&hirqctrl.irq[ite], ite, ite, 0);
    }

    /* Initialize extended host IRQs */
    ret = vmm_host_extend_irq_init();

    if (ret != VMM_OK) {
        return ret;
    }

    /* Initialize host IRQ Domains */
    ret = vmm_host_irq_domain_init();

    if (ret != VMM_OK) {
        return ret;
    }

    /* Probe all device tree nodes matching
     * host irq nodeid table enteries.
     */
    matches = vmm_device_tree_nidtable_create_matches("host_irq");

    if (matches) {
        vmm_device_tree_iterate_matching(NULL, matches, host_irq_nidtable_found, NULL);
    }

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&host_irq_cpu_hotplug, TRUE);
}
