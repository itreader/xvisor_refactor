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
 * @brief VCPU中断头文件
 */
#ifndef _VMM_VCPU_IRQ_H__
#define _VMM_VCPU_IRQ_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/**
 * @brief 虚拟CPU 中断 处理
 * @param vcpu 指向VCPU结构体的指针
 * @param regs 寄存器上下文指针
 */
void vmm_vcpu_irq_process(vmm_vcpu_t *vcpu, arch_regs_t *regs);

/**
 * @brief 虚拟CPU 中断 断言
 * @param vcpu 指向VCPU结构体的指针
 * @param irq_no 中断号
 * @param reason 原因标识
 */
void vmm_vcpu_irq_assert(vmm_vcpu_t *vcpu, uint32_t irq_no, uint64_t reason);

/**
 * @brief 虚拟CPU 中断 清除
 * @param vcpu 指向VCPU结构体的指针
 * @param irq_no 中断号
 */
void vmm_vcpu_irq_clear(vmm_vcpu_t *vcpu, uint32_t irq_no);

/**
 * @brief 虚拟CPU 中断 去断言
 * @param vcpu 指向VCPU结构体的指针
 * @param irq_no 中断号
 */
void vmm_vcpu_irq_deassert(vmm_vcpu_t *vcpu, uint32_t irq_no);

/**
 * @brief 恢复VCPU中断等待状态
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vcpu_irq_wait_resume(vmm_vcpu_t *vcpu);

/**
 * @brief VCPU等待中断超时处理
 * @param vcpu 指向VCPU结构体的指针
 * @param nsecs 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vcpu_irq_wait_timeout(vmm_vcpu_t *vcpu, uint64_t nsecs);

/** Wait for irq on given vcpu indefinetly (no timeout) */
#define vmm_vcpu_irq_wait(vcpu) vmm_vcpu_irq_wait_timeout(vcpu, 0)

/**
 * @brief 检查虚拟CPU中断是否处于等待状态
 * @param vcpu 指向VCPU结构体的指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_vcpu_irq_wait_state(vmm_vcpu_t *vcpu);

/**
 * @brief 初始化VCPU中断
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vcpu_irq_init(vmm_vcpu_t *vcpu);

/**
 * @brief VCPU中断子系统反初始化
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vcpu_irq_deinit(vmm_vcpu_t *vcpu);

#endif
