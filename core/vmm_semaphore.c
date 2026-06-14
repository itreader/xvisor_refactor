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
 * @file vmm_semaphore.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 孤儿VCPU（或线程）信号量实现
 */

#include <arch_cpu_irq.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_semaphore.h>
#include <vmm_stdio.h>

/**
 * @brief 信号量资源结构，封装等待队列中的等待者信息
 */
struct vmm_semaphore_resource {
    double_list_t       head; /**< 链表头 */
    uint32_t            count; /**< 计数 */
    vmm_semaphore_t    *sem; /**< 信号量 */
    vmm_vcpu_t         *vcpu; /**< 虚拟CPU */
    vmm_vcpu_resource_t res; /**< 保留/结果 */
};

/* 注意：此函数必须在信号量等待队列锁持有状态下调用 */
/**
 * @brief 查找信号量资源
 * @param sem 信号量指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static struct vmm_semaphore_resource *__semaphore_find_resource(vmm_semaphore_t *sem, vmm_vcpu_t *vcpu)
{
    bool                           found = FALSE;
    struct vmm_semaphore_resource *sres;

    list_for_each_entry(sres, &sem->res_list, head)
    {
        if (sres->vcpu == vcpu) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    return (found) ? sres : NULL;
}

/* 注意：此函数必须在信号量等待队列锁持有状态下调用 */
/**
 * @brief 获取信号量等待队列中的首个资源
 * @param sem 信号量指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static struct vmm_semaphore_resource *__semaphore_first_resource(vmm_semaphore_t *sem)
{
    if (list_empty(&sem->res_list)) {
        return NULL;
    }

    return list_first_entry(&sem->res_list, struct vmm_semaphore_resource, head);
}

/**
 * @brief   信号量 清理
 * @param vcpu 指向VCPU结构体的指针
 * @param vcpu_res 指向VCPU结构体的指针
 */
static void __vmm_semaphore_cleanup(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *vcpu_res)
{
    irq_flags_t                    flags;
    bool                           wake_all = FALSE;
    struct vmm_semaphore_resource *sres     = container_of(vcpu_res, struct vmm_semaphore_resource, res);
    vmm_semaphore_t               *sem      = sres->sem;

    if (!sres || !sem || (sres->vcpu != vcpu)) {
        return;
    }

    vmm_spin_lock_irq_save(&sem->wait_queue.lock, flags);

    if (sres->count) {
        sem->value += sres->count;

        if (sem->value > sem->limit) {
            sem->value = sem->limit;
        }

        sres->count = 0;
        wake_all    = TRUE;
    }

    list_del(&sres->head);
    vmm_free(sres);

    if (wake_all) {
        __vmm_waitqueue_wakeall(&sem->wait_queue);
    }

    vmm_spin_unlock_irq_restore(&sem->wait_queue.lock, flags);
}

/**
 * @brief 获取信号量当前可用计数
 * @param sem 信号量指针
 * @return 当前信号量可用计数
 */
uint32_t vmm_semaphore_avail(vmm_semaphore_t *sem)
{
    uint32_t    ret;
    irq_flags_t flags;

    BUG_ON(!sem);

    vmm_spin_lock_irq_save(&sem->wait_queue.lock, flags);

    ret = sem->value;

    vmm_spin_unlock_irq_restore(&sem->wait_queue.lock, flags);

    return ret;
}

/**
 * @brief 获取信号量最大限制值
 * @param sem 信号量指针
 * @return 信号量最大限制值
 */
uint32_t vmm_semaphore_limit(vmm_semaphore_t *sem)
{
    uint32_t    ret;
    irq_flags_t flags;

    BUG_ON(!sem);

    vmm_spin_lock_irq_save(&sem->wait_queue.lock, flags);

    ret = sem->limit;

    vmm_spin_unlock_irq_restore(&sem->wait_queue.lock, flags);

    return ret;
}

/**
 * @brief 释放信号量（V操作）
 * @param sem 信号量指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_semaphore_up(vmm_semaphore_t *sem)
{
    int                            rc = VMM_OK;
    irq_flags_t                    flags;
    vmm_vcpu_t                    *current_vcpu = vmm_scheduler_current_vcpu();
    struct vmm_semaphore_resource *sres;

    BUG_ON(!sem);
    BUG_ON(!sem->limit);

    vmm_spin_lock_irq_save(&sem->wait_queue.lock, flags);

    if (sem->value < sem->limit) {
        sem->value++;

        sres = __semaphore_find_resource(sem, current_vcpu);

        if (!sres) {
            sres = __semaphore_first_resource(sem);
        }

        if (sres) {
            if (sres->count) {
                sres->count--;
            }

            if (!sres->count) {
                vmm_manager_vcpu_resource_remove(sres->vcpu, &sres->res);
                list_del(&sres->head);
                vmm_free(sres);
            }
        }

        rc = __vmm_waitqueue_wakefirst(&sem->wait_queue);

        if (rc == VMM_ERR_NOENT) {
            rc = VMM_OK;
        }
    } else {
        rc = VMM_ERR_INVALID;
    }

    vmm_spin_unlock_irq_restore(&sem->wait_queue.lock, flags);

    return rc;
}

/**
 * @brief 信号量获取的通用实现
 * @param sem 信号量指针
 * @param timeout 时间值（纳秒）
 * @return 获取到的值，失败返回错误码
 */
static int semaphore_down_common(vmm_semaphore_t *sem, uint64_t *timeout)
{
    int                            rc = VMM_OK;
    irq_flags_t                    flags;
    vmm_vcpu_t                    *current_vcpu = vmm_scheduler_current_vcpu();
    struct vmm_semaphore_resource *sres;

    BUG_ON(!sem);
    BUG_ON(!sem->limit);
    BUG_ON(!vmm_scheduler_orphan_context());

    vmm_spin_lock_irq_save(&sem->wait_queue.lock, flags);

    while (!sem->value) {
        rc = __vmm_waitqueue_sleep(&sem->wait_queue, timeout);

        if (rc) {
            /* Timeout or some other failure */
            break;
        }
    }

    if (rc == VMM_OK) {
        sres = __semaphore_find_resource(sem, current_vcpu);

        if (!sres) {
            sres = vmm_zalloc(sizeof(*sres));
            BUG_ON(!sres);
            INIT_LIST_HEAD(&sres->head);
            sres->count       = 0;
            sres->sem         = sem;
            sres->vcpu        = current_vcpu;
            sres->res.name    = "vmm_semaphore";
            sres->res.cleanup = __vmm_semaphore_cleanup;
            list_add_tail(&sres->head, &sem->res_list);
            vmm_manager_vcpu_resource_add(current_vcpu, &sres->res);
        }

        sres->count++;
        sem->value--;
    }

    vmm_spin_unlock_irq_restore(&sem->wait_queue.lock, flags);

    return rc;
}

/**
 * @brief 获取信号量（P操作）
 * @param sem 信号量指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_semaphore_down(vmm_semaphore_t *sem)
{
    return semaphore_down_common(sem, NULL);
}

/**
 * @brief 带超时的获取信号量
 * @param sem 信号量指针
 * @param timeout 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_semaphore_down_timeout(vmm_semaphore_t *sem, uint64_t *timeout)
{
    return semaphore_down_common(sem, timeout);
}
