/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vmm_notifier.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 通知器链管理接口
 *
 * The notifer chain management is highly inspired from Linux notifiers.
 *
 * The linux notifier interface can be found at:
 * <linux_source>/include/linux/notifier.h
 *
 * Linux notifier source is licensed under the GPL.
 */

#ifndef __VMM_NOTIFIER_H__
#define __VMM_NOTIFIER_H__

#include <vmm_semaphore.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

/*
 * Notifier chains are of three types:
 *
 *  Atomic notifier chains: Chain callbacks run in interrupt/atomic
 *      context. Callouts are not allowed to block.
 *  Blocking notifier chains: Chain callbacks run in process context.
 *      Callouts are allowed to block.
 *  Raw notifier chains: There are no restrictions on callbacks,
 *      registration, or unregistration.  All locking and protection
 *      must be provided by the caller.
 *
 * vmm_atomic_notifier_register() may be called from an atomic context,
 * but vmm_blocking_notifier_register()  must be called from a process
 * context. Ditto for the corresponding _unregister() routines.
 *
 * vmm_atomic_notifier_unregister() and vmm_blocking_notifier_unregister()
 * _must not_ be called from within the call chain.
 */

/**
 * @brief 通知器回调块，封装通知链中的回调函数和优先级
 */
struct vmm_notifier_block {
    int (*notifier_call)(struct vmm_notifier_block *, uint64_t, void *); /**< notifier_call成员 */
    struct vmm_notifier_block *next; /**< 下一个 */
    int                        priority; /**< 优先级 */
};

typedef struct vmm_notifier_block vmm_notifier_block_t;

/** Notifier function callback return values */
#define NOTIFY_DONE      0x0000 /* Don't care */
#define NOTIFY_OK        0x0001 /* Suits me */
#define NOTIFY_STOP_MASK 0x8000 /* Don't call further */
#define NOTIFY_BAD       (NOTIFY_STOP_MASK | 0x0002)
/* Bad/Veto action */
/*
 * Clean way to return from the notifier and stop further calls.
 */
#define NOTIFY_STOP      (NOTIFY_OK | NOTIFY_STOP_MASK)

/* Encapsulate (negative) errno value (in particular, NOTIFY_BAD <=> EPERM). */
static inline int vmm_notifier_from_errno(int err)
{
    if (err) {
        return NOTIFY_STOP_MASK | (NOTIFY_OK - err);
    }

    return NOTIFY_OK;
}

/* Restore (negative) errno value from notify return value. */
static inline int vmm_notifier_to_errno(int ret)
{
    ret &= ~NOTIFY_STOP_MASK;
    return ret > NOTIFY_OK ? NOTIFY_OK - ret : 0;
}

/**
 * @brief 原子通知器链的表示
 */
typedef struct vmm_atomic_notifier_chain {
    vmm_spinlock_t        lock; /**< 自旋锁 */
    vmm_notifier_block_t *head; /**< 链表头 */
} vmm_atomic_notifier_chain_t;

#define ATOMIC_INIT_NOTIFIER_CHAIN(name)                                                                                                             \
    do {                                                                                                                                             \
        INIT_SPIN_LOCK(&(name)->lock);                                                                                                               \
        (name)->head = NULL;                                                                                                                         \
    } while (0)
#define ATOMIC_NOTIFIER_INIT(name)                                                                                                                   \
    {                                                                                                                                                \
        .lock = __SPINLOCK_INITIALIZER(name.lock), .head = NULL                                                                                      \
    }
#define ATOMIC_NOTIFIER_CHAIN(name) vmm_atomic_notifier_chain_t name = ATOMIC_NOTIFIER_INIT(name)

/**
 * @brief 注册原子通知器
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_atomic_notifier_register(vmm_atomic_notifier_chain_t *nc, vmm_notifier_block_t *nb);

/**
 * @brief 注销原子通知器
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_atomic_notifier_unregister(vmm_atomic_notifier_chain_t *nc, vmm_notifier_block_t *nb);

/**
 * @brief 调用原子通知器链上的所有通知器
 * @param nc 网络配置结构体指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @param nr_to_call 要调用的通知器数量上限
 * @param nr_calls 实际调用的通知器数量（输出）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_atomic_notifier_call(vmm_atomic_notifier_chain_t *nc, uint64_t val, void *v, int nr_to_call, int *nr_calls);

/**
 * @brief 调用原子通知器链上的所有通知器
 * @param nc 网络配置结构体指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_atomic_notifier_call(vmm_atomic_notifier_chain_t *nc, uint64_t val, void *v);

/**
 * @brief 阻塞通知器链的表示
 */
typedef struct vmm_blocking_notifier_chain {
    vmm_semaphore_t       rwsem; /**< rwsem成员 */
    vmm_notifier_block_t *head; /**< 链表头 */
} vmm_blocking_notifier_chain_t;

#define BLOCKING_INIT_NOTIFIER_CHAIN(name)                                                                                                           \
    do {                                                                                                                                             \
        INIT_SEMAPHORE(&(name)->rwsem, 1, 1);                                                                                                        \
        (name)->head = NULL;                                                                                                                         \
    } while (0)
#define BLOCKING_NOTIFIER_INIT(name)                                                                                                                 \
    {                                                                                                                                                \
        .rwsem = __SEMAPHORE_INITIALIZER((name).rwsem, 1, 1), .head = NULL                                                                           \
    }
#define BLOCKING_NOTIFIER_CHAIN(name) vmm_blocking_notifier_chain_t name = BLOCKING_NOTIFIER_INIT(name)

/**
 * @brief 注册阻塞通知器
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_blocking_notifier_register(vmm_blocking_notifier_chain_t *nc, vmm_notifier_block_t *nb);

/**
 * @brief 注册阻塞通知器条件
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_blocking_notifier_cond_register(vmm_blocking_notifier_chain_t *nc, vmm_notifier_block_t *nb);

/**
 * @brief 注销阻塞通知器
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_blocking_notifier_unregister(vmm_blocking_notifier_chain_t *nc, vmm_notifier_block_t *nb);

/**
 * @brief 调用阻塞通知器链上的所有通知器（可睡眠）
 * @param nc 网络配置结构体指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @param nr_to_call 要调用的通知器数量上限
 * @param nr_calls 实际调用的通知器数量（输出）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_blocking_notifier_call(vmm_blocking_notifier_chain_t *nc, uint64_t val, void *v, int nr_to_call, int *nr_calls);

/**
 * @brief 调用阻塞通知器链上的所有通知器（可睡眠）
 * @param nh 节点头指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_blocking_notifier_call(vmm_blocking_notifier_chain_t *nh, uint64_t val, void *v);

/** Representation of a raw notifier chain */
/**
 * @brief 原始通知器链节点，用于非原子上下文的通知链
 */
struct vmm_raw_notifier_chain {
    vmm_notifier_block_t *head; /**< 链表头 */
};

#define RAW_INIT_NOTIFIER_CHAIN(name)                                                                                                                \
    do {                                                                                                                                             \
        (name)->head = NULL;                                                                                                                         \
    } while (0)
#define RAW_NOTIFIER_INIT(name)                                                                                                                      \
    {                                                                                                                                                \
        .head = NULL                                                                                                                                 \
    }
#define RAW_NOTIFIER_CHAIN(name) struct vmm_raw_notifier_chain name = RAW_NOTIFIER_INIT(name)

/**
 * @brief 注册原始通知器
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_raw_notifier_register(struct vmm_raw_notifier_chain *nc, vmm_notifier_block_t *nb);

/**
 * @brief 注销原始通知器
 * @param nc 网络配置结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_raw_notifier_unregister(struct vmm_raw_notifier_chain *nc, vmm_notifier_block_t *nb);

/**
 * @brief 调用原始通知器链上的所有通知器（无锁保护）
 * @param nc 网络配置结构体指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @param nr_to_call 要调用的通知器数量上限
 * @param nr_calls 实际调用的通知器数量（输出）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_raw_notifier_call(struct vmm_raw_notifier_chain *nc, uint64_t val, void *v, int nr_to_call, int *nr_calls);

/**
 * @brief 调用原始通知器链上的所有通知器（无锁保护）
 * @param nc 网络配置结构体指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_raw_notifier_call(struct vmm_raw_notifier_chain *nc, uint64_t val, void *v);

#endif /* __VMM_NOTIFIER_H__ */
