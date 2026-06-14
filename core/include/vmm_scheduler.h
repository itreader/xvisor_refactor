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
 * @brief Hypervisor调度器头文件
 */
#ifndef _VMM_SCHEDULER_H__
#define _VMM_SCHEDULER_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/**
 * @brief 调度器 抢占 禁用
 */
void vmm_scheduler_preempt_disable(void);

/**
 * @brief 调度器 抢占 启用
 */
void vmm_scheduler_preempt_enable(void);

/**
 * @brief 调度器 抢占 孤儿
 * @param regs 寄存器上下文指针
 */
void vmm_scheduler_preempt_orphan(arch_regs_t *regs);

/**
 * @brief 强制触发当前CPU的VCPU重新调度
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_force_resched(uint32_t host_cpu);

/**
 * @brief 获取VCPU通用统计信息
 */
int vmm_scheduler_get_status(
    vmm_vcpu_t *vcpu, uint32_t *state, uint8_t *priority, uint32_t *host_cpu, uint32_t *reset_count, uint64_t *last_reset_nsecs,
    uint64_t *ready_nsecs, uint64_t *running_nsecs, uint64_t *paused_nsecs, uint64_t *halted_nsecs, uint64_t *system_nsecs);

/**
 * @brief 通知调度器VCPU状态发生变化
 * @param vcpu 指向VCPU结构体的指针
 * @param new_state 状态值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_state_change(vmm_vcpu_t *vcpu, uint32_t new_state);

/**
 * @brief 获取调度器的hcpu
 * @param vcpu 指向VCPU结构体的指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu);

/**
 * @brief 检查当前硬件CPU是否仍在运行指定的VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_check_current_hcpu(vmm_vcpu_t *vcpu);

/**
 * @brief 设置调度器的hcpu
 * @param vcpu 指向VCPU结构体的指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu);

/**
 * @brief 通知调度器进入中断上下文
 * @param regs 寄存器上下文指针
 * @param vcpu_context VCPU上下文结构体指针
 */
void vmm_scheduler_irq_enter(arch_regs_t *regs, bool vcpu_context);

/**
 * @brief 调度器 中断 寄存器
 */
arch_regs_t *vmm_scheduler_irq_regs(void);

/**
 * @brief 通知调度器退出中断上下文
 * @param regs 寄存器上下文指针
 */
void vmm_scheduler_irq_exit(arch_regs_t *regs);

/**
 * @brief 检查调度器是否处于中断上下文
 * @return 处于中断上下文返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_irq_context(void);

/**
 * @brief 处理孤儿VCPU的调度上下文
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_orphan_context(void);

/**
 * @brief 检查调度器是否处于普通上下文
 * @return 处于普通上下文返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_normal_context(void);

/**
 * @brief 获取调度器就绪队列的数量
 * @param host_cpu 主机CPU编号
 * @param priority 优先级
 * @return 数量值
 */
uint32_t vmm_scheduler_ready_count(uint32_t host_cpu, uint8_t priority);

/**
 * @brief 获取调度器的采样周期
 * @param host_cpu 主机CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_scheduler_get_sample_period(uint32_t host_cpu);

/**
 * @brief 设置调度器的采样周期
 * @param host_cpu 主机CPU编号
 * @param period 周期
 */
void vmm_scheduler_set_sample_period(uint32_t host_cpu, uint64_t period);

/**
 * @brief 调度器 中断 时间
 * @param host_cpu 主机CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_scheduler_irq_time(uint32_t host_cpu);

/**
 * @brief 调度器 空闲 时间
 * @param host_cpu 主机CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_scheduler_idle_time(uint32_t host_cpu);

/**
 * @brief 调度器 空闲 虚拟CPU
 * @param host_cpu 主机CPU编号
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_scheduler_idle_vcpu(uint32_t host_cpu);

/**
 * @brief 调度器 当前 虚拟CPU
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_scheduler_current_vcpu(void);

/**
 * @brief 调度器 当前 优先级
 * @return 调度结果
 */
uint8_t vmm_scheduler_current_priority(void);

/** Retrive current guest */
struct vmm_guest *vmm_scheduler_current_guest(void);

/** Yield current vcpu (Should not be called in IRQ context) */
/* 主动让出VCPU的调度 */
void vmm_scheduler_yield(void);

/**
 * @brief 初始化调度器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_init(void);

#endif
