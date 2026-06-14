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
 * @file vmm_completion.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 孤儿VCPU（或线程）完成量事件头文件
 */

#ifndef __VMM_COMPLETION_H__
#define __VMM_COMPLETION_H__

#include <vmm_waitqueue.h>

/** Completion event structure */
/**
 * @brief 完成量结构，用于线程间同步等待操作完成
 */
struct vmm_completion {
    uint32_t         done; /**< 完成标志 */
    vmm_wait_queue_t wait_queue; /**< 等待队列 */
};

typedef struct vmm_completion vmm_completion_t;

/** Initialize completion event */
#define INIT_COMPLETION(cptr)                                                                                                                        \
    do {                                                                                                                                             \
        (cptr)->done = 0;                                                                                                                            \
        INIT_WAITQUEUE(&(cptr)->wait_queue, (cptr));                                                                                                 \
    } while (0)

/** Re-initialize completion event.
 *
 * This macro should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
#define REINIT_COMPLETION(cptr)                                                                                                                      \
    do {                                                                                                                                             \
        (cptr)->done = 0;                                                                                                                            \
    } while (0)

#define __COMPLETION_INITIALIZER(cmpl)                                                                                                               \
    {                                                                                                                                                \
        .done = 0, .wait_queue = __WAITQUEUE_INITIALIZER((cmpl).wait_queue, &(cmpl)),                                                                \
    }

#define DECLARE_COMPLETION(cmpl) vmm_completion_t cmpl = __COMPLETION_INITIALIZER(cmpl)

/**
 * @brief 检查完成量是否已完成
 * @param cmpl 完成量结构体指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_completion_done(vmm_completion_t *cmpl);

/**
 * @brief 完成量 等待
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_wait(vmm_completion_t *cmpl);

/**
 * @brief 带超时的等待完成量被触发
 * @param cmpl 完成量结构体指针
 * @param timeout 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_wait_timeout(vmm_completion_t *cmpl, uint64_t *timeout);

/**
 * @brief 标记完成量已完成
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_complete(vmm_completion_t *cmpl);

/**
 * @brief 触发完成量，仅唤醒第一个等待者
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_complete_once(vmm_completion_t *cmpl);

/**
 * @brief 触发完成量，唤醒所有等待者
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_complete_all(vmm_completion_t *cmpl);

#endif /* __VMM_COMPLETION_H__ */
