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
 * @file vmm_semaphore.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 孤儿VCPU（或线程）信号量头文件
 */

#ifndef __VMM_SEMAPHORE_H__
#define __VMM_SEMAPHORE_H__

#include <vmm_types.h>
#include <vmm_waitqueue.h>

/**
 * @brief 信号量锁结构，提供计数型同步机制，维护等待队列和可用计数
 */
typedef struct vmm_semaphore {
    uint32_t         limit; /**< 限制值 */
    uint32_t         value; /**< 值 */
    double_list_t    res_list; /**< res_list成员 */
    vmm_wait_queue_t wait_queue; /**< 等待队列 */
} vmm_semaphore_t;

/** Initialize semaphore lock */
#define INIT_SEMAPHORE(__sem, __lim, __val)                                                                                                          \
    do {                                                                                                                                             \
        (__sem)->limit = (__lim);                                                                                                                    \
        (__sem)->value = (__val);                                                                                                                    \
        INIT_LIST_HEAD(&((__sem)->res_list));                                                                                                        \
        INIT_WAITQUEUE(&(__sem)->wait_queue, (__sem));                                                                                               \
    } while (0)

#define __SEMAPHORE_INITIALIZER(__sem, __lim, __val)                                                                                                 \
    {                                                                                                                                                \
        .limit = (__lim), .value = (__val), .res_list = {&(__sem).res_list, &(__sem).res_list},                                                      \
        .wait_queue = __WAITQUEUE_INITIALIZER((__sem).wait_queue, &(__sem)),                                                                         \
    }

#define DEFINE_SEMAPHORE(__sem, __lim, __val) vmm_semaphore_t __sem = __SEMAPHORE_INITIALIZER(__sem, __lim, __val)

/**
 * @brief 获取信号量当前可用计数
 * @param sem 信号量指针
 * @return 当前信号量可用计数
 */
uint32_t vmm_semaphore_avail(vmm_semaphore_t *sem);

/**
 * @brief 获取信号量最大限制值
 * @param sem 信号量指针
 * @return 信号量最大限制值
 */
uint32_t vmm_semaphore_limit(vmm_semaphore_t *sem);

/**
 * @brief 释放信号量（V操作）
 * @param sem 信号量指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_semaphore_up(vmm_semaphore_t *sem);

/**
 * @brief 获取信号量（P操作）
 * @param sem 信号量指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_semaphore_down(vmm_semaphore_t *sem);

/**
 * @brief 带超时的获取信号量
 * @param sem 信号量指针
 * @param timeout 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_semaphore_down_timeout(vmm_semaphore_t *sem, uint64_t *timeout);

#endif /* __VMM_SEMAPHORE_H__ */
