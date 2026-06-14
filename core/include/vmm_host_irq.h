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
 * @file vmm_host_irq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 主机中断头文件
 *
 * @brief 主机中断（Host IRQ）接口与数据结构的头文件，定义了中断触发类型、
 * 状态位、主机中断芯片抽象、主机中断实例等结构，用于体系结构相关代码和中断
 * 控制器之间的抽象层。
 */
#ifndef _VMM_HOST_IRQ_H__
#define _VMM_HOST_IRQ_H__

#include <libs/list.h>
#include <vmm_cpumask.h>
#include <vmm_device_tree.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

/**
 * enum vmm_irq_trigger_types
 * @VMM_IRQ_TYPE_NONE           - default, unspecified type
 * @VMM_IRQ_TYPE_EDGE_RISING        - rising edge triggered
 * @VMM_IRQ_TYPE_EDGE_FALLING       - falling edge triggered
 * @VMM_IRQ_TYPE_EDGE_BOTH      - rising and falling edge triggered
 * @VMM_IRQ_TYPE_LEVEL_HIGH     - high level triggered
 * @VMM_IRQ_TYPE_LEVEL_LOW      - low level triggered
 * @VMM_IRQ_TYPE_LEVEL_MASK     - Mask to filter out the level bits
 * @VMM_IRQ_TYPE_SENSE_MASK     - Mask for all the above bits
 *
 * @brief 中断触发类型枚举，用于描述中断是边沿触发还是电平触发，
 * 以及上升/下降沿或高/低电平的组合。常量用于在中断描述符中设置和查询触发类型。
 */
enum vmm_irq_trigger_types {
    VMM_IRQ_TYPE_NONE         = 0x00000000, /**< 0x00000000成员 */
    VMM_IRQ_TYPE_EDGE_RISING  = 0x00000001, /**< 0x00000001成员 */
    VMM_IRQ_TYPE_EDGE_FALLING = 0x00000002, /**< 0x00000002成员 */
    VMM_IRQ_TYPE_EDGE_BOTH    = (VMM_IRQ_TYPE_EDGE_FALLING | VMM_IRQ_TYPE_EDGE_RISING), /**< VMM_IRQ_TYPE_EDGE_RISING)成员 */
    VMM_IRQ_TYPE_LEVEL_HIGH   = 0x00000004, /**< 0x00000004成员 */
    VMM_IRQ_TYPE_LEVEL_LOW    = 0x00000008, /**< 0x00000008成员 */
    VMM_IRQ_TYPE_LEVEL_MASK   = (VMM_IRQ_TYPE_LEVEL_LOW | VMM_IRQ_TYPE_LEVEL_HIGH), /**< VMM_IRQ_TYPE_LEVEL_HIGH)成员 */
    VMM_IRQ_TYPE_SENSE_MASK   = 0x0000000f, /**< 0x0000000f成员 */
};

/**
 * enum vmm_irq_states
 * @VMM_IRQ_STATE_TRIGGER_MASK      - Mask for the trigger type bits
 * @VMM_IRQ_STATE_PER_CPU       - Interrupt is per cpu
 * @VMM_IRQ_STATE_AFFINITY_SET      - Interrupt affinity was set
 * @VMM_IRQ_STATE_LEVEL         - Interrupt is level triggered
 * @VMM_IRQ_STATE_ROUTED        - Interrupt is routed to some guest
 * @VMM_IRQ_STATE_IPI           - Interrupt is an inter-processor interrupt
 * @VMM_IRQ_STATE_EXTENDED      - Interrupt is an extended interrupt
 * @VMM_IRQ_STATE_CHAINED       - Interrupt is a chained interrupt
 */
enum vmm_irq_states {
    VMM_IRQ_STATE_TRIGGER_MASK = 0xf, /**< 0xf成员 */
    VMM_IRQ_STATE_PER_CPU      = (1 << 11), /**< 11)成员 */
    VMM_IRQ_STATE_AFFINITY_SET = (1 << 12), /**< 12)成员 */
    VMM_IRQ_STATE_LEVEL        = (1 << 13), /**< 13)成员 */
    VMM_IRQ_STATE_ROUTED       = (1 << 14), /**< 14)成员 */
    VMM_IRQ_STATE_IPI          = (1 << 15), /**< 15)成员 */
    VMM_IRQ_STATE_EXTENDED     = (1 << 16), /**< 16)成员 */
    VMM_IRQ_STATE_CHAINED      = (1 << 17), /**< 17)成员 */
};

/**
 * enum vmm_per_cpu_irq_states
 * @VMM_PERCPU_IRQ_STATE_IN_PROG    - Interrupt in-progress
 * @VMM_PERCPU_IRQ_STATE_MASKED     - Interrupt masked
 */
enum vmm_per_cpu_irq_states {
    VMM_PERCPU_IRQ_STATE_IN_PROG = (1 << 0), /**< 0) */
    VMM_PERCPU_IRQ_STATE_MASKED  = (1 << 1), /**< 1) */
};

/**
 * enum vmm_routed_irq_states
 * @VMM_ROUTED_IRQ_STATE_PENDING    - Routed interrupt is pending
 * @VMM_ROUTED_IRQ_STATE_ACTIVE     - Routed interrupt is active
 * @VMM_ROUTED_IRQ_STATE_MASKED     - Routed interrupt is masked
 *
 * @brief 被路由到来宾的中断状态，指示路由中断的挂起/激活/屏蔽状态。
 */
enum vmm_routed_irq_states {
    VMM_ROUTED_IRQ_STATE_PENDING = (1 << 0), /**< 0) */
    VMM_ROUTED_IRQ_STATE_ACTIVE  = (1 << 1), /**< 1) */
    VMM_ROUTED_IRQ_STATE_MASKED  = (1 << 2), /**< 2) */
};

/**
 * enum vmm_irq_return
 * @VMM_IRQ_NONE        interrupt was not from this device
 * @VMM_IRQ_HANDLED     interrupt was handled by this device
 *
 * @brief 中断处理函数返回值枚举，用于指示中断是否由该设备/处理函数处理。
 */
enum vmm_irq_return {
    VMM_IRQ_NONE    = (0 << 0), /**< 0) */
    VMM_IRQ_HANDLED = (1 << 0), /**< 0) */
};

struct vmm_host_irq;
struct vmm_msi_msg;

typedef struct vmm_host_irq vmm_host_irq_t;

typedef enum vmm_irq_return vmm_irq_return_t;
typedef void (*vmm_host_irq_handler_t)(vmm_host_irq_t *, uint32_t, void *);
typedef vmm_irq_return_t (*vmm_host_irq_function_t)(int irq_no, void *dev);

/** Host IRQ Action Abstraction
 *
 * @brief 主机中断动作链表项，用于在中断源上注册回调函数或动作。
 * 字段说明：
 *  - head: 链表节点，用于将多个动作串联。
 *  - func: 回调函数指针，IRQ 触发时调用。
 *  - dev : 回调函数的设备上下文指针。
 */
typedef struct vmm_host_irq_action {
    double_list_t           head; /**< 链表头，用于挂载动作链 */
    vmm_host_irq_function_t func; /**< 回调函数，当中断发生时调用，返回值用于表示是否处理 */
    void                   *dev; /**< 用户提供的设备/上下文指针，传递给回调 */
} vmm_host_irq_action_t;

/**
 * @brief 主机中断控制器抽象层
 */
typedef struct vmm_host_irq_chip {
    const char *name; /**< 芯片名称，用于日志或调试 */
    void (*irq_enable)(vmm_host_irq_t *irq); /**< 启用中断 */
    void (*irq_disable)(vmm_host_irq_t *irq); /**< 禁用中断 */
    void (*irq_ack)(vmm_host_irq_t *irq); /**< 中断应答 */
    void (*irq_mask)(vmm_host_irq_t *irq); /**< 屏蔽中断 */
    void (*irq_mask_ack)(vmm_host_irq_t *irq); /**< 屏蔽并应答中断 */
    void (*irq_unmask)(vmm_host_irq_t *irq); /**< 解除中断屏蔽 */
    void (*irq_eoi)(vmm_host_irq_t *irq); /**< 中断结束处理 */
    int (*irq_set_affinity)(vmm_host_irq_t *irq, const vmm_cpumask_t *dest, bool force); /**< 设置中断CPU亲和性 */
    int (*irq_set_type)(vmm_host_irq_t *irq, uint32_t flow_type); /**< 设置中断触发类型 */
    void (*irq_raise)(vmm_host_irq_t *irq, const vmm_cpumask_t *dest); /**< 软件触发中断 */
    void (*irq_compose_msi_msg)(vmm_host_irq_t *irq, struct vmm_msi_msg *msg); /**< 组装MSI消息 */
    uint32_t (*irq_get_routed_state)(vmm_host_irq_t *irq, uint32_t mask); /**< 获取中断路由状态 */
    void (*irq_set_routed_state)(vmm_host_irq_t *irq, uint32_t val, uint32_t mask); /**< 设置中断路由状态 */
} vmm_host_irq_chip_t;


/** Host IRQ Abstraction
 *
 * @brief 主机中断描述符，表示一个主机可见的中断源及其状态、绑定的中断芯片、
 * 亲和性、每 CPU 状态和动作链表等。
 */
struct vmm_host_irq {
    uint32_t                  num; /**< 主机中断编号（在 vmm 的 IRQ 表中的索引） */
    uint32_t                  hw_irq_num; /**< 底层硬件 IRQ 编号（如果与主机 HW IRQ 关联） */
    const char               *name; /**< 中断名称，用于调试和日志 */
    uint32_t                  state; /**< 状态位，使用 vmm_irq_states 枚举 */
    vmm_cpumask_t             affinity; /**< CPU 亲和性掩码，指示该中断可以发送到的 CPU 集合 */
    uint32_t                  per_cpu_state[CONFIG_CPU_COUNT]; /**< 每个 CPU 的中断状态（如正在处理、被屏蔽等） */
    uint32_t                  count[CONFIG_CPU_COUNT]; /**< 每个 CPU 的计数器（中断发生次数统计） */
    void                     *chip_data; /**< 芯片私有数据指针，传递给 chip 回调 */
    vmm_host_irq_chip_t *chip; /**< 关联的中断芯片抽象 */
    void                     *msi_data; /**< MSI 相关私有数据指针 */
    vmm_host_irq_handler_t    handler; /**< 高级中断处理器回调（用于更复杂的处理流程） */
    void                     *handler_data; /**< handler 的上下文数据指针 */
    vmm_rwlock_t              action_lock[CONFIG_CPU_COUNT]; /**< 每 CPU 的动作锁，用于保护 action_list */
    double_list_t             action_list[CONFIG_CPU_COUNT]; /**< 每 CPU 的动作链表，保存待执行的中断动作 */
};

/* nodeid table based host irq initialization callback */
typedef int (*vmm_host_irq_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for host irq */
#define VMM_HOST_IRQ_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "host_irq", "", "", compat, fn)

/** Explicity report a host irq
 * (Note: To be called from architecture specific code)
 * (Note: This will be typically called by nested/secondary PICs)
 *
 * @brief 显式上报一个主机中断号到 VMM，通常由体系结构相关代码或嵌套的
 * 中断控制器（PIC）调用。参数 hirq_no 为主机中断编号，返回值为处理结果。
 */
int vmm_host_generic_irq_exec(uint32_t hirq_no);

/** Report active irq as seen from CPU
 * (Note: To be called from architecture specific code)
 *
 * @brief 报告CPU观察到的活动中断号，供架构代码在中断入口处调用
 * 参数 cpu_irq_no 表示 CPU 本地的中断编号。
 */
int vmm_host_active_irq_exec(uint32_t cpu_irq_no);

/** Set callback for retriving active host irq number
 *
 * @brief 设置一个回调函数以检索活动的主机中断编号。回调签名应接收两个
 * 无符号整型参数并返回活动中断号。
 */
void vmm_host_irq_set_active_callback(uint32_t (*active)(uint32_t, uint32_t));

/** Initialize host irq instance
 *  Note: This function is for internal use only.
 *  Note: Do not call this function directly.
 *
 * @brief 初始化主机中断描述符（内部接口），设置编号、硬件 IRQ 号和初始状态。
 */
void __vmm_host_irq_init_desc(vmm_host_irq_t *irq, uint32_t hirq, uint32_t hw_irq_num, uint32_t state);

/** Get host irq count
 *
 * @brief 返回系统中注册的主机中断总数
 */
uint32_t vmm_host_irq_count(void);

/** Set hw_irq_num associated with host irq instance
 *  Note: This function is for internal use only.
 *  Note: Do not call this function directly.
 *
 * @brief 为给定的主机中断设置底层硬件 IRQ 编号（内部接口）。
 */
int __vmm_host_irq_set_hw_irq(uint32_t hirq, uint32_t hw_irq_num);

/** Get hw_irq_num associated with host irq instance
 *
 * @brief 获取与指定主机中断关联的硬件IRQ编号
 */
uint32_t vmm_host_irq_get_hw_irq(uint32_t hirq);

/** Get host irq instance from host irq number
 *
 * @brief 根据主机中断号返回对应的 `中断结构` 实例指针，若不存在则返回 NULL。
 */
vmm_host_irq_t *vmm_host_irq_get(uint32_t hirq);

/** Set host irq chip for given host irq number
 *
 * @brief 为指定的主机中断绑定中断芯片抽象 `中断芯片`，返回 0 表示成功。
 */
int vmm_host_irq_set_chip(uint32_t hirq, vmm_host_irq_chip_t *chip);

/** Get host irq chip instance from host irq instance
 *
 * @brief 从主机中断实例中获取已绑定的中断芯片抽象指针，可能为 NULL。
 */
vmm_host_irq_chip_t *vmm_host_irq_get_chip(vmm_host_irq_t *irq);

/** Set host irq chip data for given host irq number
 *
 * @brief 为指定主机中断设置芯片私有数据指针，供 chip 回调使用。
 */
int vmm_host_irq_set_chip_data(uint32_t hirq, void *chip_data);

/** Get host irq chip data from host irq instance
 *
 * @brief 获取主机中断实例的芯片私有数据指针。
 */
void *vmm_host_irq_get_chip_data(vmm_host_irq_t *irq);

/** Set host irq MSI data for given host irq number
 *
 * @brief 为指定主机中断设置 MSI 相关私有数据指针。
 */
int vmm_host_irq_set_msi_data(uint32_t hirq, void *msi_data);

/** Get host irq MSI data from host irq instance
 *
 * @brief 获取主机中断实例的 MSI 私有数据指针。
 */
void *vmm_host_irq_get_msi_data(vmm_host_irq_t *irq);

/** Set host irq handler for given host irq number
 *  NOTE: For second argument, mention one of the
 *  vmm_handle_xxxxx functions from below
 *
 * @brief 设置指定主机中断的高级处理器回调（handler），通常传入
 * `vmm_handle_per_cpu_irq`、`vmm_handle_level_irq` 等内置处理函数。
 */
int vmm_host_irq_set_handler(uint32_t hirq, vmm_host_irq_handler_t handler);

/** Get host irq handler for given host irq number
 *
 * @brief 获取指定主机中断的 handler 回调指针。
 */
vmm_host_irq_handler_t vmm_host_irq_get_handler(uint32_t hirq);

/** Set host irq handler data for given host irq number  
 *
 * @brief 为指定主机中断设置 handler 的上下文数据指针。
 */
int vmm_host_irq_set_handler_data(uint32_t hirq, void *data);

/** Get host irq handler data for given host irq number
 *
 * @brief 获取指定主机中断的 handler 上下文数据指针。
 */
void *vmm_host_irq_get_handler_data(uint32_t hirq);

/** Per-CPU irq handler
 *
 * @brief 处理每CPU类型的中断
 */
void vmm_handle_per_cpu_irq(vmm_host_irq_t *irq, uint32_t cpu, void *data);

/** Fast EOI irq handler
 *
 * @brief Fast EOI控制器的中断处理函数
 */
void vmm_handle_fast_eoi(vmm_host_irq_t *irq, uint32_t cpu, void *data);

/** Level irq handler
 *
 * @brief 处理电平触发类型的中断，包含相应的应答和结束中断流程
 */
void vmm_handle_level_irq(vmm_host_irq_t *irq, uint32_t cpu, void *data);

/** Simple irq handler
 *
 * @brief 简单的边沿触发中断处理函数，实现基础的中断统计与回调分发。
 */
void vmm_handle_simple_irq(vmm_host_irq_t *irq, uint32_t cpu, void *data);

/** Get host irq number from host irq instance
 *
 * @brief 从中断实例获取主机中断编号
 */
static inline uint32_t vmm_host_irq_get_num(vmm_host_irq_t *irq)
{
    return (irq) ? irq->num : 0;
}

/** Set host irq name from host irq instance
 *
 * @brief 为中断实例设置可读名称，用于调试或日志输出。
 */
static inline void vmm_host_irq_set_name(vmm_host_irq_t *irq, const char *name)
{
    if (irq) {
        irq->name = name;
    }
}

/** Get host irq name from host irq instance
 *
 * @brief 获取中断实例的名称
 */
static inline const char *vmm_host_irq_get_name(vmm_host_irq_t *irq)
{
    return (irq) ? irq->name : 0;
}

/** Check if a host irq is per-cpu
 *
 * @brief 判断指定中断是否为每 CPU 类型，中断状态位 每CPU中断状态。
 */
static inline bool vmm_host_irq_is_per_cpu(vmm_host_irq_t *irq)
{
    return (irq->state & VMM_IRQ_STATE_PER_CPU) ? TRUE : FALSE;
}

/** Check if a host irq is affinity was set
 *
 * @brief 判断中断是否已设置CPU亲和性
 */
static inline bool vmm_host_irq_affinity_was_set(vmm_host_irq_t *irq)
{
    return (irq->state & VMM_IRQ_STATE_AFFINITY_SET) ? TRUE : FALSE;
}

/** Get trigger type of a host irq
 *
 * @brief 获取中断的触发类型（边沿/电平等）。
 */
static inline uint32_t vmm_host_irq_get_trigger_type(vmm_host_irq_t *irq)
{
    return irq->state & VMM_IRQ_STATE_TRIGGER_MASK;
}

#if 0
/*
 * Must only be called inside irq_chip.irq_set_type() functions.
 */
static inline void vmm_host_irq_set_trigger_type(vmm_host_irq_t *irq, uint32_t type)
{
    irq->state &= ~VMM_IRQ_STATE_TRIGGER_MASK;
    irq->state |= type & VMM_IRQ_STATE_TRIGGER_MASK;
}
#endif

/** Check if a host irq is of level type
 *
 * @brief 判断中断是否为电平触发类型。
 */
static inline bool vmm_host_irq_is_level_type(vmm_host_irq_t *irq)
{
    return (irq->state & VMM_IRQ_STATE_LEVEL) ? TRUE : FALSE;
}

/** Check if a host irq is routed to some guest
 *
 * @brief 判断该中断是否已被路由到某个来宾（guest）。
 */
static inline bool vmm_host_irq_is_routed(vmm_host_irq_t *irq)
{
    return (irq->state & VMM_IRQ_STATE_ROUTED) ? TRUE : FALSE;
}

/** Check if a host irq is inter-processor interrupt
 *
 * @brief 判断该中断是否为 IPI（CPU 间中断）。
 */
static inline bool vmm_host_irq_is_ipi(vmm_host_irq_t *irq)
{
    return (irq->state & VMM_IRQ_STATE_IPI) ? TRUE : FALSE;
}

/** Check if a host irq is a chained interrupt
 *
 * @brief 判断该中断是否为链式中断（chained），即由上级中断控制器转发）。
 */
static inline bool vmm_host_irq_is_chained(vmm_host_irq_t *irq)
{
    return (irq->state & VMM_IRQ_STATE_CHAINED) ? TRUE : FALSE;
}

/** Check if a host irq is masked
 *
 * @brief 判断中断是否被屏蔽（masked），实现由底层函数提供。
 */
bool vmm_host_irq_is_masked(vmm_host_irq_t *irq);

/** Check if a host irq is disabled
 *
 * @brief 判断中断是否被禁用（等价于被屏蔽）。
 */
static inline bool vmm_host_irq_is_disabled(vmm_host_irq_t *irq)
{
/**
 * @brief 检查指定主机中断是否被屏蔽
 * @param irq 中断描述符指针
 * @return 中断处理结果
 */
    return vmm_host_irq_is_masked(irq);
}

/** Check if a host irq is in-progress
 *
 * @brief 判断指定CPU上该中断是否正在处理中
 */
static inline bool vmm_host_irq_is_inprogress(vmm_host_irq_t *irq, uint32_t cpu)
{
    if (irq && (cpu < CONFIG_CPU_COUNT)) {
        return (irq->per_cpu_state[cpu] & VMM_PERCPU_IRQ_STATE_IN_PROG) ? TRUE : FALSE;
    }

    return FALSE;
}

/** Get host irq count from host irq instance
 *
 * @brief 获取指定CPU上该中断的触发计数
 */
static inline uint32_t vmm_host_irq_get_count(vmm_host_irq_t *irq, uint32_t cpu)
{
    if (cpu < CONFIG_CPU_COUNT) {
        return (irq) ? irq->count[cpu] : 0;
    }

    return 0;
}

/** Set cpu affinity of given host irq
 *
 * @brief 设置主机中断的CPU亲和性掩码
 * force 指示是否强制覆盖已有设置。
 */
int vmm_host_irq_set_affinity(uint32_t hirq, const vmm_cpumask_t *dest, bool force);

/** Get cpu affinity of given host irq
 *
 * @brief 获取指定中断的 CPU 亲和性掩码指针。
 */
static inline const vmm_cpumask_t *vmm_host_irq_get_affinity(vmm_host_irq_t *irq)
{
    return (irq) ? &irq->affinity : NULL;
}

/** Set trigger type for given host irq
 *
 * @brief 为指定主机中断设置触发类型（使用 中断触发类型 中的值）。
 */
int vmm_host_irq_set_type(uint32_t hirq, uint32_t type);

/** Mark host irq as per cpu
 *
 * @brief 将指定中断标记为每 CPU 类型（每CPU）。
 */
int vmm_host_irq_mark_per_cpu(uint32_t hirq);

/** UnMark host irq as per cpu
 *
 * @brief 取消中断的每CPU标记
 */
int vmm_host_irq_unmark_per_cpu(uint32_t hirq);

/** Mark host irq as routed to some guest
 *
 * @brief 将指定中断标记为已路由到某个来宾（guest）。
 */
int vmm_host_irq_mark_routed(uint32_t hirq);

/** UnMark host irq as routed to some guest
 *
 * @brief 取消指定中断的路由标记。
 */
int vmm_host_irq_unmark_routed(uint32_t hirq);

/** Get host irq routed state
 *
 * @brief 读取指定中断的路由状态（受屏蔽过滤），结果写回到val
 */
int vmm_host_irq_get_routed_state(uint32_t hirq, uint32_t *val, uint32_t mask);

/** Set/update host irq routed state
 *
 * @brief 根据掩码更新中断的路由状态
 */
int vmm_host_irq_set_routed_state(uint32_t hirq, uint32_t val, uint32_t mask);

/** Mark host irq as inter-processor interrupt
 *
 * @brief 将指定中断标记为 IPI（CPU 间中断）。
 */
int vmm_host_irq_mark_ipi(uint32_t hirq);

/** UnMark host irq as inter-processor interrupt
 *
 * @brief 取消处理器间中断标记
 */
int vmm_host_irq_unmark_ipi(uint32_t hirq);

/** Mark host irq as chained interrupt
 *
 * @brief 将指定中断标记为链式中断（由上级中断控制器链式转发）。
 */
int vmm_host_irq_mark_chained(uint32_t hirq);

/** UnMark host irq as chained interrupt
 *
 * @brief 取消链式中断标记。
 */
int vmm_host_irq_unmark_chained(uint32_t hirq);

/** Unmask a host irq (by default all irqs are masked)
 *
 * @brief 解除对指定主机中断的屏蔽，使其可以被触发和分发。
 */
int vmm_host_irq_unmask(uint32_t hirq);

/** Mask a host irq
 *
 * @brief 屏蔽指定主机中断，阻止其被分发到 CPU。
 */
int vmm_host_irq_mask(uint32_t hirq);

/**
 * @brief 启用指定主机中断
 */
static inline int vmm_host_irq_enable(uint32_t hirq)
{
/**
 * @brief 取消屏蔽指定的主机中断
 * @param hirq 主机中断号
 * @return 中断处理结果
 */
    return vmm_host_irq_unmask(hirq);
}

/**
 * @brief 禁用指定主机中断
 */
static inline int vmm_host_irq_disable(uint32_t hirq)
{
/**
 * @brief 主机 中断 掩码
 * @param hirq 主机中断号
 * @return 掩码值
 */
    return vmm_host_irq_mask(hirq);
}

/** Raise a host irq from software
 *
 * @brief 通过软件方式触发指定主机中断
 */
int vmm_host_irq_raise(uint32_t hirq, const vmm_cpumask_t *dest);

/** Compose a MSI message for given host irq
 *
 * @brief 为指定主机中断组装MSI消息并填充msg
 */
int vmm_host_irq_compose_msi_msg(uint32_t hirq, struct vmm_msi_msg *msg);

/** Find a host irq with matching state mask
 *
 * @brief 从 起始硬件中断号 开始查找第一个与 状态掩码 匹配的主机中断，结果写回 hirq。
 */
int vmm_host_irq_find(uint32_t hirq_start, uint32_t state_mask, uint32_t *hirq);

/** Register function callback for given irq
 *
 * @brief 为指定主机中断注册一个函数回调（低级函数式接口），用于直接处理硬件中断。
 */
int vmm_host_irq_register(uint32_t hirq, const char *name, vmm_host_irq_function_t func, void *dev);

/** Unregister function callback for given irq
 *
 * @brief 注销先前为指定中断注册的函数回调，使用 dev 指针进行匹配和移除。
 */
int vmm_host_irq_unregister(uint32_t hirq, void *dev);

/** Interrupts initialization function
 *
 * @brief 初始化中断子系统，注册必要的数据结构和默认中断描述符。
 */
int vmm_host_irq_init(void);

/*
 * Entry/exit functions for chained handlers where the primary IRQ chip
 * may implement either fasteoi or level-trigger flow control.
 *
 * @brief 链式中断处理器的进入/退出辅助函数。针对链式（chained）中断场景，
 * 上游主控器可能使用 FastEOI 或者电平触发的流控，此处封装进入与退出时的
 * 屏蔽/确认/EOI 行为，供链式中断处理器调用。
 */
static inline void vmm_chained_irq_enter(vmm_host_irq_chip_t *chip, vmm_host_irq_t *desc)
{
    /* FastEOI controllers require no action on entry. */
    if (chip->irq_eoi) {
        return;
    }

    if (chip->irq_mask_ack) {
        chip->irq_mask_ack(desc);
    } else {
        chip->irq_mask(desc);

        if (chip->irq_ack) {
            chip->irq_ack(desc);
        }
    }
}

static inline void vmm_chained_irq_exit(vmm_host_irq_chip_t *chip, vmm_host_irq_t *desc)
{
    if (chip->irq_eoi) {
        chip->irq_eoi(desc);
    } else {
        chip->irq_unmask(desc);
    }
}

#endif
