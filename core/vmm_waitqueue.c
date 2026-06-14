/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file vmm_waitqueue.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 孤儿VCPU（或线程）等待队列实现
 */

#include <arch_cpu_irq.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_waitqueue.h>

/**
 * @brief 获取等待队列的数量
 * @param wait_queue 等待队列指针
 * @return 数量值
 */
uint32_t vmm_waitqueue_count(vmm_wait_queue_t *wait_queue)
{
    BUG_ON(!wait_queue);

    return wait_queue->vcpu_count;
}

/**
 * @brief 等待队列私有结构，保存等待条件和关联VCPU
 */
struct vmm_waitqueue_priv {
    vmm_wait_queue_t  *wait_queue; /**< 等待队列 */
    vmm_timer_event_t *ev; /**< 事件 */
};

/**
 * @brief 等待队列超时等待
 * @param event 定时器事件
 */
static void waitqueue_timeout(vmm_timer_event_t *event)
{
    vmm_vcpu_t *vcpu = event->private;

    vmm_waitqueue_wake(vcpu);
}

/* NOTE: Must be called with wait_queue->lock held */
/**
 * @brief 等待队列 清理
 * @param vcpu 指向VCPU结构体的指针
 */
static void waitqueue_cleanup(vmm_vcpu_t *vcpu)
{
    vmm_wait_queue_t          *wait_queue;
    vmm_timer_event_t         *ev;
    struct vmm_waitqueue_priv *p = vcpu->wait_queue_private;

    /* Sanity checks */
    if (!p || !p->wait_queue) {
        return;
    }

    wait_queue = p->wait_queue;
    ev         = p->ev;

    /* Stop timer event if started */
    if (ev) {
        vmm_timer_event_stop(ev);
    }

    /* Remove VCPU from waitqueue */
    list_del_init(&vcpu->wait_queue_head);

    /* Decrement VCPU count in waitqueue */
    if (wait_queue->vcpu_count) {
        wait_queue->vcpu_count--;
    }

    /* Clear waitqueue cleanup callback so that it's called only once */
    vcpu->wait_queue_cleanup = NULL;
}

/* NOTE: Must be called with wait_queue->lock held */
/**
 * @brief   等待队列 休眠
 * @param wait_queue 等待队列指针
 * @param timeout_nsecs 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_waitqueue_sleep(vmm_wait_queue_t *wait_queue, uint64_t *timeout_nsecs)
{
    int                       rc  = VMM_OK;
    uint64_t now = 0;
    uint64_t expiry = 0;
    vmm_vcpu_t               *vcpu;
    vmm_timer_event_t         wake_event;
    struct vmm_waitqueue_priv p = {.wait_queue = wait_queue,
                                   .ev = NULL};

    /* Sanity checks */
    BUG_ON(!wait_queue);
    BUG_ON(!vmm_scheduler_orphan_context());

    if (timeout_nsecs && (*timeout_nsecs == 0)) {
        return VMM_ERR_TIMEDOUT;
    }

    /* Get current VCPU */
    vcpu             = vmm_scheduler_current_vcpu();

    /* Update VCPU waitqueue context */
    vcpu->wait_queue_lock    = &wait_queue->lock;
    vcpu->wait_queue_private = &p;

    /* Add VCPU to waitqueue */
    list_add_tail(&vcpu->wait_queue_head, &wait_queue->vcpu_list);

    /* Increment VCPU count in waitqueue */
    wait_queue->vcpu_count++;

    /* If timeout is required then create timer event */
    if (timeout_nsecs) {
        INIT_TIMER_EVENT(&wake_event, &waitqueue_timeout, vcpu);
        p.ev = &wake_event;
        vmm_timer_event_start2(&wake_event, *timeout_nsecs, &expiry);
    }

    /* Update waitqueue cleanup callback */
    vcpu->wait_queue_cleanup = waitqueue_cleanup;

    /* Try to Pause VCPU */
    rc               = vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_PAUSED);

    /* Set VCPU waitqueue context to NULL */
    vcpu->wait_queue_lock    = NULL;
    vcpu->wait_queue_private = NULL;

    /* VCPU Wokeup successfully */
    if (rc == VMM_OK) {
        /* If timeout was used then update error code */
        if (timeout_nsecs) {
            now            = vmm_timer_timestamp();
            *timeout_nsecs = (now > expiry) ? 0 : (expiry - now);

            if (*timeout_nsecs == 0) {
                rc = VMM_ERR_TIMEDOUT;
            }
        }
    }

    return rc;
}

/**
 * @brief 等待队列 休眠
 * @param wait_queue 等待队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_waitqueue_sleep(vmm_wait_queue_t *wait_queue)
{
    int rc;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* Lock waitqueue */
    vmm_spin_lock_irq(&wait_queue->lock);

    /* Put VCPU to sleep without timeout */
    rc = __vmm_waitqueue_sleep(wait_queue, NULL);

    /* Unlock waitqueue */
    vmm_spin_unlock_irq(&wait_queue->lock);

    return rc;
}

/**
 * @brief 在等待队列上休眠指定超时时间
 * @param wait_queue 等待队列指针
 * @param timeout_usecs 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_waitqueue_sleep_timeout(vmm_wait_queue_t *wait_queue, uint64_t *timeout_usecs)
{
    int rc;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* Lock waitqueue */
    vmm_spin_lock_irq(&wait_queue->lock);

    /* Put VCPU to sleep with timeout */
    rc = __vmm_waitqueue_sleep(wait_queue, timeout_usecs);

    /* Unlock waitqueue */
    vmm_spin_unlock_irq(&wait_queue->lock);

    return rc;
}

/**
 * @brief 强制从等待队列中移除等待项
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_waitqueue_forced_remove(vmm_vcpu_t *vcpu)
{
    irq_flags_t                flags;
    vmm_wait_queue_t          *wait_queue;
    struct vmm_waitqueue_priv *p = vcpu->wait_queue_private;

    /* Sanity check */
    if (!p || !p->wait_queue) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    wait_queue = p->wait_queue;

    /* Lock waitqueue */
    vmm_spin_lock_irq_save(&wait_queue->lock, flags);

    /* Cleanup waitqueue */
    if (vcpu->wait_queue_cleanup) {
        vcpu->wait_queue_cleanup(vcpu);
    }

    /* Set VCPU waitqueue context to NULL */
    vcpu->wait_queue_lock    = NULL;
    vcpu->wait_queue_private = NULL;

    /* Unlock waitqueue */
    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

    return VMM_OK;
}

/**
 * @brief   等待队列 唤醒
 * @param wait_queue 等待队列指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __vmm_waitqueue_wake(vmm_wait_queue_t *wait_queue, vmm_vcpu_t *vcpu)
{
    return vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_READY);
}

/**
 * @brief 等待队列 唤醒
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_waitqueue_wake(vmm_vcpu_t *vcpu)
{
    int                        rc = VMM_OK;
    irq_flags_t                flags;
    vmm_wait_queue_t          *wait_queue;
    struct vmm_waitqueue_priv *p = vcpu->wait_queue_private;

    /* Sanity checks */
    if (!vcpu || vcpu->is_normal || !p || !p->wait_queue) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    wait_queue = p->wait_queue;

    /* Lock waitqueue */
    vmm_spin_lock_irq_save(&wait_queue->lock, flags);

    /* Try to wake VCPU */
    rc = __vmm_waitqueue_wake(wait_queue, vcpu);

    /* Unlock waitqueue */
    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

    return rc;
}

/**
 * @brief   唤醒等待队列中的首个等待者
 * @param wait_queue 等待队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_waitqueue_wakefirst(vmm_wait_queue_t *wait_queue)
{
    vmm_vcpu_t *vcpu;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* We should have atleast one VCPU in waitqueue list */
    if (list_empty(&wait_queue->vcpu_list)) {
        return VMM_ERR_NOENT;
    }

    /* Get first VCPU from waitqueue list */
    vcpu = list_entry(list_first(&wait_queue->vcpu_list), vmm_vcpu_t, wait_queue_head);

    /* Try to Resume VCPU */
    return __vmm_waitqueue_wake(wait_queue, vcpu);
}

/**
 * @brief 唤醒等待队列中的首个等待者
 * @param wait_queue 等待队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_waitqueue_wakefirst(vmm_wait_queue_t *wait_queue)
{
    int         rc;
    irq_flags_t flags;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* Lock waitqueue */
    vmm_spin_lock_irq_save(&wait_queue->lock, flags);

    /* Wakeup first VCPU from waitqueue list */
    rc = __vmm_waitqueue_wakefirst(wait_queue);

    /* Unlock waitqueue */
    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

    return rc;
}

/**
 * @brief   唤醒等待队列中的所有等待者
 * @param wait_queue 等待队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_waitqueue_wakeall(vmm_wait_queue_t *wait_queue)
{
    int         rc;
    vmm_vcpu_t *vcpu = NULL;
    vmm_vcpu_t *nvcpu = NULL;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* We should have atleast one VCPU in waitqueue list */
    if (list_empty(&wait_queue->vcpu_list)) {
        return VMM_ERR_NOENT;
    }

    /* Try resume every VCPU till empty */
    list_for_each_entry_safe(vcpu, nvcpu, &wait_queue->vcpu_list, wait_queue_head)
    {
        /* Try to Resume VCPU */
        if ((rc = __vmm_waitqueue_wake(wait_queue, vcpu))) {
            /* Return Failure */
            return rc;
        }
    }

    return VMM_OK;
}

/**
 * @brief 唤醒等待队列中的所有等待者
 * @param wait_queue 等待队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_waitqueue_wakeall(vmm_wait_queue_t *wait_queue)
{
    int         rc;
    irq_flags_t flags;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* Lock waitqueue */
    vmm_spin_lock_irq_save(&wait_queue->lock, flags);

    /* Wake every first VCPU till empty */
    rc = __vmm_waitqueue_wakeall(wait_queue);

    /* Unlock waitqueue */
    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

    return rc;
}
