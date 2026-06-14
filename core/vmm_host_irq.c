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
 * @brief 主机中断源代码
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

/**
 * struct vmm_host_irqs_ctrl
 *
 * @brief 主机中断控制器全局控制结构，保存中断数组指针、默认亲和性和
 * 活动中断回调等全局状态。
 */
struct vmm_host_irqs_ctrl {
    vmm_spinlock_t       lock;            /**< 保护对全局结构访问的自旋锁 */
    vmm_host_irq_t *irq;                  /**< 指向vmm_host_irq_t数组的指针 */
    uint32_t (*active)(uint32_t, uint32_t); /**< 平台回调，获取活跃的主机中断号 */
    vmm_cpumask_t default_affinity;       /**< 默认的CPU亲和性掩码 */
};

/* 全局主机中断控制实例 */
static struct vmm_host_irqs_ctrl hirqctrl;

/**
 * vmm_handle_per_cpu_irq
 *
 * @brief 处理每CPU类型的中断
 * @param irq 要处理的主机中断描述符
 * @param cpu 当前处理该中断的 CPU 编号
 * @param data 处理器上下文或额外数据（未使用，可为 NULL）
 */
void vmm_handle_per_cpu_irq(vmm_host_irq_t *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    vmm_host_irq_action_t *act;

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

/**
 * vmm_handle_fast_eoi
 *
 * @brief 处理Fast EOI中断控制器的中断
 * 然后调用芯片的 EOI 回调（若存在）。
 * @param irq 主机中断描述符
 * @param cpu 当前 CPU 编号
 * @param data 可选上下文指针
 */
void vmm_handle_fast_eoi(vmm_host_irq_t *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    vmm_host_irq_action_t *act;

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

/**
 * vmm_handle_level_irq
 *
 * @brief 处理电平触发类型的中断，在处理前进行屏蔽和应答
 * 操作（由芯片实现决定），遍历动作链表执行回调，处理完成后根据
 * 状态对中断进行解屏蔽。
 * @param irq 主机中断描述符
 * @param cpu 当前 CPU 编号
 * @param data 可选上下文指针
 */
void vmm_handle_level_irq(vmm_host_irq_t *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    vmm_host_irq_action_t *act;

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

/**
 * vmm_handle_simple_irq
 *
 * @brief 简单的中断处理函数（通常用于边沿触发），仅遍历动作链表并调用回调。
 * @param irq 主机中断描述符
 * @param cpu 当前 CPU 编号
 * @param data 可选上下文指针
 */
void vmm_handle_simple_irq(vmm_host_irq_t *irq, uint32_t cpu, void *data)
{
    irq_flags_t                 flags;
    vmm_host_irq_action_t *act;

    vmm_read_lock_irq_save_lite(&irq->action_lock[cpu], flags);
    list_for_each_entry(act, &irq->action_list[cpu], head)
    {
        if (act->func(irq->num, act->dev) == VMM_IRQ_HANDLED) {
            break;
        }
    }
    vmm_read_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);
}

/**
 * vmm_host_irq_get
 *
 * @brief 根据主机中断号获取对应的 `中断结构` 实例。
 * 若中断号位于扩展区（extended），将委托给扩展实现获取实例。
 * @param hirq 主机中断号
 * @return 指向 `vmm_host_irq_t` 的指针，若未找到则返回 NULL
 */
vmm_host_irq_t *vmm_host_irq_get(uint32_t hirq)
{
    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        return &hirqctrl.irq[hirq];
    }

    return __vmm_host_extend_irq_get(hirq);
}

/**
 * vmm_host_generic_irq_exec
 *
 * @brief 通用中断上报接口，由平台或上层中断控制器调用以通知 VMM
 * 某个主机中断发生。函数会执行计数、标记 in-progress、调用 handler 并清理状态。
 * @param hirq_no 主机中断号
 * @return VMM_OK 成功，或错误码（例如 VMM_ERR_NOTAVAIL）
 */
int vmm_host_generic_irq_exec(uint32_t hirq_no)
{
    uint32_t             cpu;
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq_no))) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * vmm_host_active_irq_exec
 *
 * @brief 处理CPU本地可见的活动中断序列
 * 回调获取活动中断号，并限定最大处理次数以避免硬件错误导致的无限循环。
 * @param cpu_irq_no CPU 本地可见的中断号
 * @return VMM_OK 成功，或错误码（如回调未设置返回 VMM_ERR_NOTAVAIL）
 */
int vmm_host_active_irq_exec(uint32_t cpu_irq_no)
{
    uint32_t hirq_no;
    uint32_t exec_count;

    if (!hirqctrl.active) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * vmm_host_irq_set_active_callback
 *
 * @brief 设置用于检索活动主机中断号的回调函数。
 * @param active 回调函数指针，签名为 `uint32_t (*)(uint32_t, uint32_t)`
 */
void vmm_host_irq_set_active_callback(uint32_t (*active)(uint32_t, uint32_t))
{
    hirqctrl.active = active;
}

/**
 * vmm_host_irq_count
 *
 * @brief 返回 VMM 支持的主机中断数量（配置宏 主机中断配置数量）。
 * @return CONFIG_HOST_IRQ_COUNT
 */
uint32_t vmm_host_irq_count(void)
{
    return CONFIG_HOST_IRQ_COUNT;
}

/**
 * __vmm_host_irq_set_hw_irq
 *
 * @brief（内部）：为主机中断设置底层硬件 IRQ 编号（仅当中断未标记为扩展时生效）。
 * @param hirq 主机中断号
 * @param hw_irq_num 硬件 IRQ 编号
 * @return VMM_OK 成功，或错误码
 */
int __vmm_host_irq_set_hw_irq(uint32_t hirq, uint32_t hw_irq_num)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_FAIL;
    }

    if (!(irq->state & VMM_IRQ_STATE_EXTENDED)) {
        irq->hw_irq_num = hw_irq_num;
    }

    return VMM_OK;
}

/**
 * vmm_host_irq_get_hw_irq
 *
 * @brief 获取与指定主机中断关联的硬件 IRQ 编号，若找不到描述符则返回输入编号。
 * @param hirq 主机中断号
 * @return 对应的 hw_irq 编号或输入值
 */
uint32_t vmm_host_irq_get_hw_irq(uint32_t hirq)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return hirq;
    }

    return irq->hw_irq_num;
}

/**
 * vmm_host_irq_set_chip
 *
 * @brief 为指定主机中断绑定一个 `中断芯片` 芯片抽象。
 * @param hirq 主机中断号
 * @param chip 芯片抽象指针
 * @return VMM_OK 成功，或错误码
 */
int vmm_host_irq_set_chip(uint32_t hirq, vmm_host_irq_chip_t *chip)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_FAIL;
    }

    irq->chip = chip;
    return VMM_OK;
}

/**
 * vmm_host_irq_get_chip
 *
 * @brief 从中断实例获取已绑定的中断芯片抽象指针，若无则返回 NULL。
 * @param irq 指向主机中断实例的指针
 * @return 芯片抽象指针或 NULL
 */
vmm_host_irq_chip_t *vmm_host_irq_get_chip(vmm_host_irq_t *irq)
{
    return (irq) ? irq->chip : NULL;
}

/**
 * vmm_host_irq_set_chip_data
 *
 * @brief 为指定中断设置芯片的私有数据指针，供芯片回调使用。
 * @param hirq 主机中断号
 * @param chip_data 芯片私有数据指针
 * @return VMM_OK 成功，或错误码
 */
int vmm_host_irq_set_chip_data(uint32_t hirq, void *chip_data)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_FAIL;
    }

    irq->chip_data = chip_data;
    return VMM_OK;
}

/**
 * vmm_host_irq_get_chip_data
 *
 * @brief 获取中断实例的芯片私有数据指针。
 * @param irq 指向主机中断实例的指针
 * @return 芯片私有数据指针或 NULL
 */
void *vmm_host_irq_get_chip_data(vmm_host_irq_t *irq)
{
    return (irq) ? irq->chip_data : NULL;
}

/**
 * vmm_host_irq_set_msi_data
 *
 * @brief 为指定中断设置 MSI 私有数据指针，用于 MSI 相关的芯片/平台实现。
 * @param hirq 主机中断号
 * @param msi_data MSI 私有数据指针
 * @return VMM_OK 成功，或错误码
 */
int vmm_host_irq_set_msi_data(uint32_t hirq, void *msi_data)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_FAIL;
    }

    irq->msi_data = msi_data;
    return VMM_OK;
}

/**
 * vmm_host_irq_get_msi_data
 *
 * @brief 获取中断实例的 MSI 私有数据指针。
 * @param irq 指向主机中断实例的指针
 * @return MSI 私有数据指针或 NULL
 */
void *vmm_host_irq_get_msi_data(vmm_host_irq_t *irq)
{
    return (irq) ? irq->msi_data : NULL;
}

/**
 * vmm_host_irq_set_handler
 *
 * @brief 设置高级中断处理回调（handler），用于处理复杂中断分发流程。
 * @param hirq 主机中断号
 * @param handler 回调函数指针
 * @return VMM_OK 成功，或错误码
 */
int vmm_host_irq_set_handler(uint32_t hirq, vmm_host_irq_handler_t handler)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_FAIL;
    }

    irq->handler = handler;
    return VMM_OK;
}

/**
 * vmm_host_irq_get_handler
 *
 * @brief 获取指定中断的 handler 回调指针，若不存在返回 NULL。
 * @param hirq 主机中断号
 * @return handler 回调指针或 NULL
 */
vmm_host_irq_handler_t vmm_host_irq_get_handler(uint32_t hirq)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return NULL;
    }

    return irq->handler;
}

/**
 * vmm_host_irq_set_handler_data
 *
 * @brief 为指定中断设置 handler 的上下文数据指针。
 * @param hirq 主机中断号
 * @param data handler 的上下文数据指针
 * @return VMM_OK 成功，或错误码
 */
int vmm_host_irq_set_handler_data(uint32_t hirq, void *data)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_FAIL;
    }

    irq->handler_data = data;
    return VMM_OK;
}

/**
 * vmm_host_irq_get_handler_data
 *
 * @brief 获取指定中断的 handler 上下文数据指针，若不存在返回 NULL。
 * @param hirq 主机中断号
 * @return handler 的上下文数据指针或 NULL
 */
void *vmm_host_irq_get_handler_data(uint32_t hirq)
{
    vmm_host_irq_t *irq = NULL;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return NULL;
    }

    return irq->handler_data;
}

/**
 * vmm_host_irq_set_affinity
 *
 * @brief 设置指定主机中断的 CPU 亲和性。若芯片提供 `中断亲和性设置`，
 * 则会调用芯片实现并更新本地亲和性拷贝。
 */
int vmm_host_irq_set_affinity(uint32_t hirq, const vmm_cpumask_t *dest, bool force)
{
    int                  rc = VMM_OK;
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (!dest || vmm_host_irq_is_per_cpu(irq)) {
        return VMM_ERR_INVALID;
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

/**
 * vmm_host_irq_set_type
 *
 * @brief 为指定主机中断设置触发类型（边沿/电平等），并更新中断状态位。
 */
int vmm_host_irq_set_type(uint32_t hirq, uint32_t type)
{
    int                  rc = VMM_ERR_FAIL;
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * vmm_host_irq_mark_per_cpu
 *
 * @brief 将指定中断标记为每 CPU 类型（每CPU），用于区分是否需要针对单个 CPU 注册动作。
 */
int vmm_host_irq_mark_per_cpu(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_PER_CPU;
    return VMM_OK;
}

/**
 * @brief 取消中断的每CPU标记
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_unmark_per_cpu(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_PER_CPU;
    return VMM_OK;
}

/**
 * @brief 标记中断为路由类型
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_mark_routed(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_ROUTED;
    return VMM_OK;
}

/**
 * @brief 取消标记主机中断的路由状态
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_unmark_routed(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_ROUTED;
    return VMM_OK;
}

/**
 * @brief 获取主机中断的路由状态
 * @param hirq 中断号
 * @param val 待写入的值
 * @param mask 掩码值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_get_routed_state(uint32_t hirq, uint32_t *val, uint32_t mask)
{
    vmm_host_irq_t      *irq;
    vmm_host_irq_chip_t *chip;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (NULL == (chip = vmm_host_irq_get_chip(irq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (!chip->irq_get_routed_state) {
        return VMM_ERR_INVALID;
    }

    *val = chip->irq_get_routed_state(irq, mask);

    return VMM_OK;
}

/**
 * @brief 设置主机中断的路由状态
 * @param hirq 中断号
 * @param val 待写入的值
 * @param mask 掩码值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_set_routed_state(uint32_t hirq, uint32_t val, uint32_t mask)
{
    vmm_host_irq_t      *irq;
    vmm_host_irq_chip_t *chip;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (NULL == (chip = vmm_host_irq_get_chip(irq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (!chip->irq_set_routed_state) {
        return VMM_ERR_INVALID;
    }

    chip->irq_set_routed_state(irq, val, mask);

    return VMM_OK;
}

/**
 * @brief 标记中断为处理器间中断（IPI）
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_mark_ipi(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_IPI;
    return VMM_OK;
}

/**
 * @brief 取消标记中断为处理器间中断
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_unmark_ipi(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_IPI;
    return VMM_OK;
}

/**
 * @brief 标记中断为链式中断
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_mark_chained(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state |= VMM_IRQ_STATE_CHAINED;
    return VMM_OK;
}

/**
 * @brief 取消标记主机中断的级联状态
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_unmark_chained(uint32_t hirq)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    irq->state &= ~VMM_IRQ_STATE_CHAINED;
    return VMM_OK;
}

/**
 * @brief 检查指定主机中断是否被屏蔽
 * @param irq 指向主机中断结构体的指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_host_irq_is_masked(vmm_host_irq_t *irq)
{
    uint32_t per_cpu_state;

    if (!irq) {
        return FALSE;
    }

    per_cpu_state = irq->per_cpu_state[vmm_smp_processor_id()];
    return (per_cpu_state & VMM_PERCPU_IRQ_STATE_MASKED) ? TRUE : FALSE;
}

/**
 * @brief 取消屏蔽指定的主机中断
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_unmask(uint32_t hirq)
{
    uint32_t             cpu;
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * @brief 主机 中断 掩码
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_mask(uint32_t hirq)
{
    uint32_t             cpu;
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * vmm_host_irq_unmask
 *
 * @brief 解除对指定主机中断的屏蔽，使该中断在目标 CPU 上可被触发并分发。
 */


int vmm_host_irq_raise(uint32_t hirq, const vmm_cpumask_t *dest)
{
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (irq->chip && irq->chip->irq_raise) {
        irq->chip->irq_raise(irq, dest);
    }

    return VMM_OK;
}

/**
 * vmm_host_irq_mask
 *
 * @brief 屏蔽（禁用）指定主机中断，阻止其被分发到 CPU。
 */


int vmm_host_irq_compose_msi_msg(uint32_t hirq, struct vmm_msi_msg *msg)
{
    vmm_host_irq_t *irq;

    if (!msg) {
        return VMM_ERR_INVALID;
    }

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
    }

    if (!irq->chip || !irq->chip->irq_compose_msi_msg) {
        return VMM_ERR_NOSYS;
    }

    irq->chip->irq_compose_msi_msg(irq, msg);
    return VMM_OK;
}

/**
 * vmm_host_irq_raise
 *
 * @brief 通过软件方式触发指定主机中断，`dest` 指定目标 CPU 掩码（可为 NULL）。
 */


int vmm_host_irq_find(uint32_t hirq_start, uint32_t state_mask, uint32_t *hirq)
{
    uint32_t             ite;
    bool                 found = FALSE;
    vmm_host_irq_t *irq;

    if ((CONFIG_HOST_IRQ_COUNT <= hirq_start) || !hirq) {
        return VMM_ERR_INVALID;
    }

    if (!state_mask) {
        return VMM_ERR_NOTAVAIL;
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

    return (found) ? VMM_OK : VMM_ERR_NOTAVAIL;
}

/**
 * vmm_host_irq_compose_msi_msg
 *
 * @brief 为给定主机中断组装 MSI 消息，填充 `msg` 结构，若芯片不支持则返回错误码。
 */


static int host_irq_register(vmm_host_irq_t *irq, const char *name, vmm_host_irq_function_t func, void *dev, uint32_t cpu)
{
    bool                        found;
    irq_flags_t                 flags;
    vmm_host_irq_action_t *act;

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
        return VMM_ERR_FAIL;
    }

    irq->name = name;
    act       = vmm_zalloc(sizeof(vmm_host_irq_action_t));

    if (!act) {
        vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);
        return VMM_ERR_NOMEM;
    }

    INIT_LIST_HEAD(&act->head);
    act->func = func;
    act->dev  = dev;

    list_add_tail(&act->head, &irq->action_list[cpu]);

    vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    return VMM_OK;
}

/**
 * vmm_host_irq_find
 *
 * @brief 从指定起始中断号开始查找符合 状态掩码 的中断，找到则通过 `hirq` 返回中断号。
 */


int vmm_host_irq_register(uint32_t hirq, const char *name, vmm_host_irq_function_t func, void *dev)
{
    int                  rc;
    uint32_t             cpu;
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * host_irq_register
 *
 * @brief 在指定CPU的动作链表上注册中断回调函数
 */


static int host_irq_unregister(vmm_host_irq_t *irq, void *dev, uint32_t cpu, bool *disable)
{
    bool                        found;
    irq_flags_t                 flags;
    vmm_host_irq_action_t *act;

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
        return VMM_ERR_FAIL;
    }

    list_del(&act->head);
    vmm_free(act);

    if (list_empty(&irq->action_list[cpu])) {
        *disable = TRUE;
    }

    vmm_write_unlock_irq_restore_lite(&irq->action_lock[cpu], flags);

    return VMM_OK;
}

/**
 * vmm_host_irq_register
 *
 * @brief 为指定中断注册一个函数回调（对所有 CPU 或针对每 CPU），
 * 注册成功后通常会解除该中断的屏蔽。
 */


int vmm_host_irq_unregister(uint32_t hirq, void *dev)
{
    int                  rc;
    uint32_t             cpu;
    bool                 disable;
    vmm_host_irq_t *irq;

    if (NULL == (irq = vmm_host_irq_get(hirq))) {
        return VMM_ERR_NOTAVAIL;
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

/**
 * host_irq_unregister
 *
 * @brief 从指定CPU的动作链表上注销中断回调，并判断是否需禁用中断
 */


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
 *
 * @brief 初始化单个 `中断结构` 描述符的字段（供内部使用），包括编号、
 * 硬件 IRQ 编号、状态、亲和性、每 CPU 状态与动作链表的初始化。
 */
void __vmm_host_irq_init_desc(vmm_host_irq_t *irq, uint32_t hirq, uint32_t hw_irq_num, uint32_t state)
{
    uint32_t cpu = 0;

    if (!irq) {
        return;
    }

    irq->num   = hirq;
    irq->hw_irq_num = hw_irq_num;
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

/**
 * @brief 启动指定主机中断的处理
 * @param cpu_hotplug CPU热插拔结构体指针
 * @param cpu CPU编号
 * @return 中断处理结果
 */
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

/**
 * arch_host_irq_init
 *
 * @brief（弱符号）：体系结构可以覆盖此函数以完成板级 PIC/中断控制器初始化。
 */


static vmm_cpu_hotplug_notify_t host_irq_cpu_hotplug = {
    .name    = "HOST_IRQ",
    .state   = VMM_CPU_HOTPLUG_STATE_HOST_IRQ,
    .startup = host_irq_startup,
};

/*
 * @brief CPU 热插拔回调结构，注册后在 CPU 启动时会执行 `主机中断启动`，
 * 用于初始化每个 CPU 的中断相关设置。
 */

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

/**
 * host_irq_nidtable_found
 *
 * @brief 在设备树匹配到主机中断节点时调用的回调，调用对应的nodeid表初始化函数
 */


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
    hirqctrl.irq = vmm_malloc(sizeof(vmm_host_irq_t) * CONFIG_HOST_IRQ_COUNT);

    if (!hirqctrl.irq) {
        return VMM_ERR_NOMEM;
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
