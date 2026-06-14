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
 * @file vmm_block_request_queue.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 通用块设备请求队列源文件
 */

#include <block/vmm_block_request_queue.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_limits.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_page_pool.h>
#include <vmm_stdio.h>

/**
 * @brief 块设备请求队列工作任务，封装异步I/O处理的上下文
 */
struct block_request_queue_work {
    vmm_block_request_queue_t *brq; /**< 块请求队列 */
    double_list_t              head; /**< 链表头 */
    vmm_work_t                 work; /**< 工作项 */
    bool                       is_rw; /**< is_rw成员 */

    union {
        struct {
            vmm_request_t *r; /**< r */
            void *private; /**< 私有数据 */
        } rw; /**< 读写 */

        struct {
            void (*func)(vmm_block_request_queue_t *, void *); /**< 函数指针 */
            void *private; /**< 私有数据 */
        } w; /**< w */
    } d; /**< d */

    bool is_free; /**< is_free成员 */
};

/**
 * @brief 块设备请求队列的缓存读写操作
 * @param brq 块设备请求队列指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_cache_rw(vmm_block_request_queue_t *brq, vmm_request_t *r)
{
    int rc = VMM_ERR_NOTAVAIL;

    switch (r->type) {
        case VMM_REQUEST_READ:
            if (brq->ops->read_cache) {
                rc = brq->ops->read_cache(brq, r, brq->private);
            }

            break;

        case VMM_REQUEST_WRITE:
            if (brq->ops->write_cache) {
                rc = brq->ops->write_cache(brq, r, brq->private);
            }

            break;

        default:
            break;
    };

    return rc;
}

/**
 * @brief 块设备请求队列的排队读写操作
 * @param brq 块设备请求队列指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_queue_rw(vmm_block_request_queue_t *brq, vmm_request_t *r)
{
    int                              rc = VMM_OK;
    irq_flags_t                      flags;
    struct block_request_queue_work *bwork;

    vmm_spin_lock_irq_save(&brq->wait_queue_lock, flags);

    if (list_empty(&brq->wq_rw_free_list)) {
        rc = VMM_ERR_NOMEM; /**< VMM_ERR_NOMEM成员 */
        goto done; /**< done成员 */
    }

    bwork = list_first_entry(&brq->wq_rw_free_list, struct block_request_queue_work, head);
    list_del(&bwork->head);
    bwork->is_rw  = TRUE;
    bwork->d.rw.r = r;

    if (r) {
        bwork->d.rw.private = r->private;
        r->private          = bwork;
    } else {
        bwork->d.rw.private = NULL;
    }

    bwork->is_free = FALSE;
    list_add_tail(&bwork->head, &brq->wq_pending_list);

    vmm_workqueue_schedule_work(brq->wait_queue, &bwork->work);

done:
    vmm_spin_unlock_irq_restore(&brq->wait_queue_lock, flags);

    return rc;
}

/**
 * @brief 块设备请求队列的工作项处理函数
 * @param brq 块设备请求队列指针
 * @param (*w_func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_queue_work(vmm_block_request_queue_t *brq, void (*w_func)(vmm_block_request_queue_t *, void *), void *w_private)
{
    int                              rc = VMM_OK;
    irq_flags_t                      flags;
    struct block_request_queue_work *bwork;

    vmm_spin_lock_irq_save(&brq->wait_queue_lock, flags);

    if (list_empty(&brq->wq_w_free_list)) {
        rc = VMM_ERR_NOMEM; /**< VMM_ERR_NOMEM成员 */
        goto done; /**< done成员 */
    }

    bwork = list_first_entry(&brq->wq_w_free_list, struct block_request_queue_work, head);
    list_del(&bwork->head);
    bwork->is_rw       = FALSE;
    bwork->d.w.func    = w_func;
    bwork->d.w.private = w_private;
    bwork->is_free     = FALSE;
    list_add_tail(&bwork->head, &brq->wq_pending_list);

    vmm_workqueue_schedule_work(brq->wait_queue, &bwork->work);

done:
    vmm_spin_unlock_irq_restore(&brq->wait_queue_lock, flags);

    return rc;
}

/**
 * @brief 块设备请求队列出队工作项
 * @param bwork 块设备工作项指针
 */
static void block_request_queue_dequeue_work(struct block_request_queue_work *bwork)
{
    irq_flags_t                flags;
    vmm_block_request_queue_t *brq = bwork->brq;

    vmm_spin_lock_irq_save(&brq->wait_queue_lock, flags);

    list_del(&bwork->head);
    bwork->is_free = TRUE;

    if (bwork->is_rw) {
        if (bwork->d.rw.r) {
            bwork->d.rw.r->private = bwork->d.rw.private;
        }

        bwork->d.rw.r       = NULL;
        bwork->d.rw.private = NULL;
        list_add_tail(&bwork->head, &brq->wq_rw_free_list);
    } else {
        bwork->d.w.func    = NULL;
        bwork->d.w.private = NULL;
        list_add_tail(&bwork->head, &brq->wq_w_free_list);
    }

    vmm_spin_unlock_irq_restore(&brq->wait_queue_lock, flags);
}

/**
 * @brief 中止块设备请求队列中的读写操作
 * @param brq 块设备请求队列指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_abort_rw(vmm_block_request_queue_t *brq, vmm_request_t *r)
{
    int                              rc = VMM_OK;
    struct block_request_queue_work *bwork;

    if (!brq || !r || !r->private) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    bwork = r->private;

    rc    = vmm_workqueue_stop_work(&bwork->work);

    if (rc) {
        return rc;
    }

    if (!bwork->is_free) {
        block_request_queue_dequeue_work(bwork);
    }

    if (brq->ops->abort) {
        rc = brq->ops->abort(brq, r, brq->private);
    }

    return rc;
}

/**
 * @brief 块设备请求队列读写完成回调
 * @param bwork 块设备工作项指针
 * @param error 错误码值
 */
void block_request_queue_rw_done(struct block_request_queue_work *bwork, int error)
{
    vmm_request_t *r;

    if (!bwork || !bwork->is_rw) {
        return;
    }

    if (!bwork->d.rw.r || !bwork->d.rw.r->private) {
        return;
    }

    r = bwork->d.rw.r;

    block_request_queue_dequeue_work(bwork);

    if (error) {
        vmm_block_device_fail_request(r);
    } else {
        vmm_block_device_complete_request(r);
    }
}

/**
 * @brief 块设备请求队列工作项回调函数
 * @param work 指向工作项结构体的指针
 */
static void block_request_queue_work_func(vmm_work_t *work)
{
    int   rc = VMM_OK;
    void *w_private;
    void (*w_func)(vmm_block_request_queue_t *, void *);
    struct block_request_queue_work *bwork = container_of(work, struct block_request_queue_work, work);
    vmm_block_request_queue_t       *brq   = bwork->brq;

    if (!bwork->is_rw) {
        w_func    = bwork->d.w.func; /**< bwork->d.w.func成员 */
        w_private = bwork->d.w.private; /**< bwork->d.w.private成员 */
        block_request_queue_dequeue_work(bwork);

        if (w_func) {
            w_func(brq, w_private); /**< w_private)成员 */
        }

        return;
    }

    switch (bwork->d.rw.r->type) {
        case VMM_REQUEST_READ:
            if (brq->ops->read) {
                rc = brq->ops->read(brq, bwork->d.rw.r, brq->private);
            } else {
                rc = VMM_ERR_IO;
            }

            break;

        case VMM_REQUEST_WRITE:
            if (brq->ops->write) {
                rc = brq->ops->write(brq, bwork->d.rw.r, brq->private);
            } else {
                rc = VMM_ERR_IO;
            }

            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    if (!brq->async_rw) {
        block_request_queue_rw_done(bwork, rc);
    }
}

/**
 * @brief 刷新块设备请求队列的工作项
 * @param brq 块设备请求队列指针
 * @param private 私有数据指针
 */
static void block_request_queue_flush_work(vmm_block_request_queue_t *brq, void *private)
{
    if (brq->ops->flush) {
        brq->ops->flush(brq, brq->private);
    }
}

/**
 * @brief 查看块设备请求队列缓存中的下一个请求
 * @param rq 请求队列指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_peek_cache(vmm_request_queue_t *rq, vmm_request_t *r)
{
    return block_request_queue_cache_rw(vmm_block_request_queue_from_rq(rq), r);
}

/**
 * @brief 为块设备请求队列创建I/O请求
 * @param rq 请求队列指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_make_request(vmm_request_queue_t *rq, vmm_request_t *r)
{
    return block_request_queue_queue_rw(vmm_block_request_queue_from_rq(rq), r);
}

/**
 * @brief 中止块设备请求队列中的I/O请求
 * @param rq 请求队列指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_abort_request(vmm_request_queue_t *rq, vmm_request_t *r)
{
    return block_request_queue_abort_rw(vmm_block_request_queue_from_rq(rq), r);
}

/**
 * @brief 刷新块设备请求队列的缓存
 * @param rq 请求队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int block_request_queue_flush_cache(vmm_request_queue_t *rq)
{
    return block_request_queue_queue_work(vmm_block_request_queue_from_rq(rq), block_request_queue_flush_work, NULL);
}

/**
 * @brief 块设备请求队列异步操作完成回调
 * @param brq 块设备请求队列指针
 * @param r 资源或数据指针
 * @param error 错误码值
 */
void vmm_block_request_queue_async_done(vmm_block_request_queue_t *brq, vmm_request_t *r, int error)
{
    struct block_request_queue_work *bwork;

    if (!brq || !brq->async_rw || !r || !r->private) {
        return;
    }

    bwork = r->private;

    block_request_queue_rw_done(bwork, error);
}

VMM_ERR_XPORT_SYMBOL(vmm_block_request_queue_async_done);

/**
 * @brief 块设备请求队列的工作项处理函数
 * @param brq 块设备请求队列指针
 * @param (*w_func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_request_queue_queue_work(vmm_block_request_queue_t *brq, void (*w_func)(vmm_block_request_queue_t *, void *), void *w_private)
{
    if (!brq || !w_func) {
        return VMM_ERR_INVALID;
    }

    return block_request_queue_queue_work(brq, w_func, w_private);
}

VMM_ERR_XPORT_SYMBOL(vmm_block_request_queue_queue_work);

/**
 * @brief 销毁请求队列
 * @param brq 块设备请求队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_request_queue_destroy(vmm_block_request_queue_t *brq)
{
    int rc;

    if (!brq) {
        return VMM_ERR_INVALID;
    }

    rc = vmm_workqueue_destroy(brq->wait_queue);

    if (rc) {
        return rc;
    }

    vmm_page_pool_free(VMM_PAGE_POOL_NORMAL, brq->wq_page_va, brq->wq_page_count);

    vmm_free(brq);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_block_request_queue_destroy);

/**
 * @brief 创建请求队列
 * @return 成功返回新创建的节点指针，失败返回NULL
 */
vmm_block_request_queue_t *vmm_block_request_queue_create(
    const char *name, uint32_t max_pending, bool async_rw, const vmm_block_request_queue_ops_t *ops, void *private)
{
    uint32_t                         i;
    vmm_block_request_queue_t       *brq;
    struct block_request_queue_work *bwork;

    if (!name || !max_pending || !ops) {
        goto fail; /**< fail成员 */
    }

    brq = vmm_zalloc(sizeof(*brq));

    if (!brq) {
        goto fail;
    }

    if (strlcpy(brq->name, name, sizeof(brq->name)) >= sizeof(brq->name)) {
        goto fail_free_brq;
    }

    brq->max_pending   = max_pending;
    brq->async_rw      = async_rw;
    brq->ops           = ops;
    brq->private       = private;

    brq->wq_page_count = VMM_SIZE_TO_PAGE(max_pending * sizeof(*bwork) * 2);
    brq->wq_page_va    = vmm_page_pool_alloc(VMM_PAGE_POOL_NORMAL, brq->wq_page_count);
    INIT_SPIN_LOCK(&brq->wait_queue_lock);
    INIT_LIST_HEAD(&brq->wq_rw_free_list);
    INIT_LIST_HEAD(&brq->wq_w_free_list);
    INIT_LIST_HEAD(&brq->wq_pending_list);

    for (i = 0; i < brq->max_pending; i++) {
        bwork      = (struct block_request_queue_work *)(brq->wq_page_va + i * sizeof(*bwork));
        bwork->brq = brq;
        INIT_LIST_HEAD(&bwork->head);
        INIT_WORK(&bwork->work, block_request_queue_work_func);
        bwork->d.rw.r       = NULL;
        bwork->d.rw.private = NULL;
        bwork->is_rw        = TRUE;
        bwork->is_free      = TRUE;
        list_add_tail(&bwork->head, &brq->wq_rw_free_list);
    }

    for (i = brq->max_pending; i < (2 * brq->max_pending); i++) {
        bwork      = (struct block_request_queue_work *)(brq->wq_page_va + i * sizeof(*bwork));
        bwork->brq = brq;
        INIT_LIST_HEAD(&bwork->head);
        INIT_WORK(&bwork->work, block_request_queue_work_func);
        bwork->d.w.func    = NULL;
        bwork->d.w.private = NULL;
        bwork->is_rw       = FALSE;
        bwork->is_free     = TRUE;
        list_add_tail(&bwork->head, &brq->wq_w_free_list);
    }

    brq->wait_queue = vmm_workqueue_create(name, VMM_THREAD_DEF_PRIORITY);

    if (!brq->wait_queue) {
        goto fail_free_pages;
    }

    INIT_REQUEST_QUEUE(
        &brq->rq, max_pending, block_request_queue_peek_cache, block_request_queue_make_request, block_request_queue_abort_request,
        block_request_queue_flush_cache, brq);

    return brq;

fail_free_pages:
    vmm_page_pool_free(VMM_PAGE_POOL_NORMAL, brq->wq_page_va, brq->wq_page_count);
fail_free_brq:
    vmm_free(brq);
fail:
    return NULL;
}

VMM_ERR_XPORT_SYMBOL(vmm_block_request_queue_create);
