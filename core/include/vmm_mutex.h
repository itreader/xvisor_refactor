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
 * @file vmm_mutex.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 孤儿VCPU（或线程）互斥锁头文件
 */

#ifndef __VMM_MUTEX_H__
#define __VMM_MUTEX_H__

#include <vmm_types.h>
#include <vmm_waitqueue.h>

/**
 * @brief 互斥锁结构，基于自旋锁实现的互斥同步，维护等待队列
 */
typedef struct vmm_mutex {
    uint32_t            lock; /**< 自旋锁 */
    vmm_vcpu_resource_t res; /**< 保留/结果 */
    vmm_vcpu_t         *owner; /**< 所有者 */
    vmm_wait_queue_t    wait_queue; /**< 等待队列 */
} vmm_mutex_t;

/**
 * @brief   互斥锁 清理
 * @param vcpu 指向VCPU结构体的指针
 * @param vcpu_res 指向VCPU结构体的指针
 */
void __vmm_mutex_cleanup(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *vcpu_res);

/** Initialize mutex lock */
#define INIT_MUTEX(__mut)                                                                                                                            \
    do {                                                                                                                                             \
        (__mut)->lock        = 0;                                                                                                                    \
        (__mut)->res.name    = "vmm_mutex";                                                                                                          \
        (__mut)->res.cleanup = __vmm_mutex_cleanup;                                                                                                  \
        (__mut)->owner       = NULL;                                                                                                                 \
        INIT_WAITQUEUE(&(__mut)->wait_queue, (__mut));                                                                                               \
    } while (0)

#define __MUTEX_INITIALIZER(__mut)                                                                                                                   \
    {                                                                                                                                                \
        .lock = 0, .res = {.name = "vmm_mutex", .cleanup = __vmm_mutex_cleanup}, .owner = NULL,                                                      \
        .wait_queue = __WAITQUEUE_INITIALIZER((__mut).wait_queue, &(__mut)),                                                                         \
    }

#define DEFINE_MUTEX(__mut) vmm_mutex_t __mut = __MUTEX_INITIALIZER(__mut)

/**
 * @brief 检查互斥锁是否可用
 * @param mut 互斥锁指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_mutex_avail(vmm_mutex_t *mut);

/**
 * @brief 获取互斥锁持有者
 * @param mut 互斥锁指针
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_mutex_owner(vmm_mutex_t *mut);

/**
 * @brief 释放互斥锁锁
 * @param mut 互斥锁指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_mutex_unlock(vmm_mutex_t *mut);

/**
 * @brief 尝试获取互斥锁（非阻塞）
 * @param mut 互斥锁指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_mutex_trylock(vmm_mutex_t *mut);

/**
 * @brief 获取互斥锁锁
 * @param mut 互斥锁指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_mutex_lock(vmm_mutex_t *mut);

/**
 * @brief 带超时的获取互斥锁
 * @param mut 互斥锁指针
 * @param timeout 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_mutex_lock_timeout(vmm_mutex_t *mut, uint64_t *timeout);

#endif /* __VMM_MUTEX_H__ */
