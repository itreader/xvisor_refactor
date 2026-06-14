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
 * @file vmm_schedule_algorithm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 调度算法接口声明头文件
 */
#ifndef _VMM_SECHEDULE_ALGORITHM_H__
#define _VMM_SECHEDULE_ALGORITHM_H__

#include <vmm_manager.h>
#include <vmm_types.h>

/**
 * @brief 为调度算法设置新创建的VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_schedule_algorithm_vcpu_setup(vmm_vcpu_t *vcpu);

/**
 * @brief 清理现有VCPU的调度算法相关资源
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_schedule_algorithm_vcpu_cleanup(vmm_vcpu_t *vcpu);

/**
 * @brief 将VCPU入队到就绪队列
 * @param rq 指向就绪队列的指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_enqueue(void *rq, vmm_vcpu_t *vcpu);

/**
 * @brief 从就绪队列中出队VCPU
 * @param rq 指向就绪队列的指针
 * @param next 指向VCPU指针的指针，用于返回下一个要运行的VCPU
 * @param next_time_slice 指向uint64_t的指针，用于返回下一个VCPU的时间片
 * @return 成功返回0，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_dequeue(void *rq, vmm_vcpu_t **next, uint64_t *next_time_slice);

/**
 * @brief 将VCPU从就绪队列中分离
 * @param rq 指向就绪队列的指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_detach(void *rq, vmm_vcpu_t *vcpu);

/**
 * @brief 检查当前VCPU是否需要被抢占
 * @param rq 指向就绪队列的指针
 * @param current 指向当前VCPU结构体的指针
 * @return 如果需要抢占返回true，否则返回false
 */
bool vmm_schedule_algorithm_ready_queue_prempt_needed(void *rq, vmm_vcpu_t *current);

/**
 * @brief 创建新的就绪队列
 * @return 成功返回就绪队列指针，失败返回NULL
 */
void *vmm_schedule_algorithm_ready_queue_create(void);

/**
 * @brief 销毁现有的就绪队列
 * @param rq 指向就绪队列的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_destroy(void *rq);

/**
 * @brief 获取给定优先级的就绪VCPU的数量
 * @param rq 指向就绪队列的指针
 * @param priority 优先级
 * @return 返回就绪VCPU的数量
 */
int vmm_schedule_algorithm_ready_queue_length(void *rq, uint8_t priority);

#endif
