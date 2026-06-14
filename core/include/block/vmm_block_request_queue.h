/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_block_request_queue.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 通用块设备请求队列头文件
 */

#ifndef __VMM_BLOCKRQ_H__
#define __VMM_BLOCKRQ_H__

#include <block/vmm_block_device.h>
#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>
#include <vmm_workqueue.h>

struct vmm_block_request_queue;
typedef struct vmm_block_request_queue vmm_block_request_queue_t;

/**
 * @brief 通用请求队列操作表示
 */
typedef struct vmm_block_request_queue_t_ops {
    int (*read)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private); /**< 读 */
    int (*read_cache)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private); /**< read_cache成员 */
    int (*write)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private); /**< 写 */
    int (*write_cache)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private); /**< write_cache成员 */
    int (*abort)(vmm_block_request_queue_t *brq, vmm_request_t *r, void *private); /**< abort成员 */
    void (*flush)(vmm_block_request_queue_t *brq, void *private); /**< 刷新 */
} vmm_block_request_queue_ops_t;

/** Representation of generic request queue */
/**
 * @brief 块设备请求队列，管理待处理的块I/O请求的排序和调度
 */
struct vmm_block_request_queue {
    char                                 name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t                             max_pending; /**< max_pending成员 */
    bool                                 async_rw; /**< async_rw成员 */
    const vmm_block_request_queue_ops_t *ops; /**< 操作集 */
    void *private; /**< 私有数据 */

    uint32_t       wq_page_count; /**< wq_page_count成员 */
    virtual_addr_t wq_page_va; /**< wq_page_va成员 */
    vmm_spinlock_t wait_queue_lock; /**< 等待队列锁 */
    double_list_t  wq_rw_free_list; /**< wq_rw_free_list成员 */
    double_list_t  wq_w_free_list; /**< wq_w_free_list成员 */
    double_list_t  wq_pending_list; /**< wq_pending_list成员 */

    struct vmm_workqueue *wait_queue; /**< 等待队列 */

    vmm_request_queue_t rq; /**< 运行队列 */
};

#define vmm_rq_to_block_request_queue(__rq) container_of(__rq, vmm_block_request_queue_t, rq)

/** Get generic block_device request queue from request queue pointer */
static inline vmm_block_request_queue_t *vmm_block_request_queue_from_rq(vmm_request_queue_t *rq)
{
    return (vmm_block_request_queue_t *)rq->private;
}

/** Get request queue pointer from generic block_device request queue */
static inline vmm_request_queue_t *vmm_block_request_queue_to_rq(vmm_block_request_queue_t *brq)
{
    return &brq->rq;
}

/**
 * @brief 块设备请求队列异步操作完成回调
 * @param brq 块设备请求队列指针
 * @param r 资源或数据指针
 * @param error 错误码值
 */
void vmm_block_request_queue_async_done(vmm_block_request_queue_t *brq, vmm_request_t *r, int error);

/**
 * @brief 块设备请求队列的工作项处理函数
 * @param brq 块设备请求队列指针
 * @param (*w_func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_request_queue_queue_work(vmm_block_request_queue_t *brq, void (*w_func)(vmm_block_request_queue_t *, void *), void *w_private);

/**
 * @brief 销毁请求队列
 * @param brq 块设备请求队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_request_queue_destroy(vmm_block_request_queue_t *brq);

/** Create generic block_device request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
vmm_block_request_queue_t *vmm_block_request_queue_create(
    const char *name, uint32_t max_pending, bool async_rw, const vmm_block_request_queue_ops_t *ops, void *private);

#endif
