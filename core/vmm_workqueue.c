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
 * @file vmm_workqueue.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 工作队列（特殊工作线程）实现
 */

#include <libs/stringlib.h>
#include <vmm_compiler.h>

#include <vmm_cpu_hotplug.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_workqueue.h>

static struct vmm_workqueue_ctrl wqctrl;

/**
 * \brief 检查工作是否为新建状态
 * \param work 指向工作结构的指针
 * \return 如果工作是新建状态则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_isnew(vmm_work_t *work)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    if (!work) {
        return FALSE;
    }

    vmm_spin_lock_irq_save(&work->lock, flags);

    ret = (work->flags & VMM_WORK_STATE_CREATED) ? TRUE : FALSE;

    vmm_spin_unlock_irq_restore(&work->lock, flags);

    return ret;
}

/**
 * \brief 检查工作是否正在进行中
 * \param work 指向工作结构的指针
 * \return 如果工作正在进行中则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_inprogress(vmm_work_t *work)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    if (!work) {
        return FALSE;
    }

    vmm_spin_lock_irq_save(&work->lock, flags);

    ret = (work->flags & VMM_WORK_STATE_INPROGRESS) ? TRUE : FALSE;

    vmm_spin_unlock_irq_restore(&work->lock, flags);

    return ret;
}

/**
 * \brief 检查工作是否已完成
 * \param work 指向工作结构的指针
 * \return 如果工作已完成则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_completed(vmm_work_t *work)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    if (!work) {
        return FALSE;
    }

    vmm_spin_lock_irq_save(&work->lock, flags);

    if (work->flags & VMM_WORK_STATE_CREATED) {
        ret = FALSE;
    } else {
        if (!(work->flags & VMM_WORK_STATE_INPROGRESS) && !(work->flags & VMM_WORK_STATE_SCHEDULED)) {
            ret = TRUE;
        } else {
            ret = FALSE;
        }
    }

    vmm_spin_unlock_irq_restore(&work->lock, flags);

    return ret;
}

/**
 * \brief 停止指定的工作
 * \param work 指向要停止的工作结构的指针
 * \return 成功返回VMM_OK，失败返回VMM_ERR_FAIL
 */
int vmm_workqueue_stop_work(vmm_work_t *work)
{
    irq_flags_t flags;
    irq_flags_t flags1;

    if (!work) {
        return VMM_ERR_FAIL;
    }

stop_retry:
    vmm_spin_lock_irq_save(&work->lock, flags);

    if (work->flags & VMM_WORK_STATE_INPROGRESS) {
        vmm_spin_unlock_irq_restore(&work->lock, flags);
        vmm_udelay(VMM_THREAD_DEF_TIME_SLICE / 1000);
        goto stop_retry;
    }

    if (work->wait_queue && (work->flags & VMM_WORK_STATE_SCHEDULED)) {
        vmm_spin_lock_irq_save(&(work->wait_queue)->lock, flags1);
        list_del(&work->head);
        vmm_spin_unlock_irq_restore(&(work->wait_queue)->lock, flags1);
    }

    work->flags &= ~VMM_WORK_STATE_CREATED;
    work->flags &= ~VMM_WORK_STATE_INPROGRESS;
    work->flags &= ~VMM_WORK_STATE_SCHEDULED;
    work->wait_queue = NULL;

    vmm_spin_unlock_irq_restore(&work->lock, flags);

    return VMM_OK;
}

/**
 * \brief 停止延迟工作
 * \param work 指向要停止的延迟工作结构的指针
 * \return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_stop_delayed_work(struct vmm_delayed_work *work)
{
    int rc;

    if (!work) {
        return VMM_ERR_FAIL;
    }

    rc = vmm_timer_event_stop(&work->event);

    if (rc) {
        return rc;
    }

    return vmm_workqueue_stop_work(&work->work);
}

/**
 * \brief 获取工作队列的线程
 * \param wait_queue 指向工作队列结构的指针
 * \return 返回工作队列的线程指针，如果失败返回NULL
 */
vmm_thread_t *vmm_workqueue_get_thread(struct vmm_workqueue *wait_queue)
{
    return (wait_queue) ? wait_queue->thread : NULL;
}

/**
 * \brief 根据索引获取工作队列
 * \param index 工作队列的索引
 * \return 返回对应索引的工作队列指针，如果不存在返回NULL
 */
struct vmm_workqueue *vmm_workqueue_index2workqueue(int index)
{
    bool                  found; /**< found成员 */
    irq_flags_t           flags; /**< 标志位 */
    struct vmm_workqueue *wait_queue; /**< 等待队列 */

    if (index < 0) {
        return NULL; /**< NULL成员 */
    }

    wait_queue = NULL; /**< NULL成员 */
    found      = FALSE; /**< FALSE成员 */

    vmm_spin_lock_irq_save(&wqctrl.lock, flags); /**< flags)成员 */

    list_for_each_entry(wait_queue, &wqctrl.wq_list, head)
    {
        if (!index) {
            found = TRUE; /**< TRUE成员 */
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&wqctrl.lock, flags); /**< flags)成员 */

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    return wait_queue; /**< 等待队列 */
}

/**
 * \brief 获取工作队列的数量
 * \return 返回当前工作队列的数量
 */
uint32_t vmm_workqueue_count(void)
{
    return wqctrl.wq_count;
}

/**
 * \brief 刷新工作队列，清空所有待处理工作
 * \param wait_queue 指向要刷新的工作队列结构的指针
 * \return 成功返回VMM_OK，失败返回VMM_ERR_FAIL
 */
int vmm_workqueue_flush(struct vmm_workqueue *wait_queue)
{
    irq_flags_t flags;

    if (!wait_queue) {
        return VMM_ERR_FAIL;
    }

    vmm_spin_lock_irq_save(&wait_queue->lock, flags);

    while (!list_empty(&wait_queue->work_list)) {
        vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

        /* Make sure thread is running */
        vmm_threads_wakeup(wait_queue->thread);

        /* We release the processor to let the wait_queue thread do its job */
        vmm_scheduler_yield();

        vmm_spin_lock_irq_save(&wait_queue->lock, flags);
    }

    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

    return VMM_OK;
}

/**
 * \brief 调度工作到工作队列
 * \param wait_queue 指向目标工作队列结构的指针，如果为NULL则使用当前CPU的系统工作队列
 * \param work 指向要调度的工作结构的指针
 * \return 成功返回VMM_OK，如果工作已调度返回VMM_ERR_ALREADY，失败返回VMM_ERR_FAIL
 */
int vmm_workqueue_schedule_work(struct vmm_workqueue *wait_queue, vmm_work_t *work)
{
    irq_flags_t flags;
    irq_flags_t flags1;

    if (!work) {
        return VMM_ERR_FAIL;
    }

    vmm_spin_lock_irq_save(&work->lock, flags);

    if (work->flags & VMM_WORK_STATE_SCHEDULED) {
        vmm_spin_unlock_irq_restore(&work->lock, flags);
        return VMM_ERR_ALREADY;
    }

    if (!wait_queue) {
        wait_queue = wqctrl.system_workqueue[vmm_smp_processor_id()];
    }

    work->flags &= ~VMM_WORK_STATE_CREATED;
    work->flags |= VMM_WORK_STATE_SCHEDULED;
    work->wait_queue = wait_queue;

    vmm_spin_lock_irq_save(&wait_queue->lock, flags1);
    list_add_tail(&work->head, &wait_queue->work_list);
    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags1);

    vmm_spin_unlock_irq_restore(&work->lock, flags);

    vmm_completion_complete(&wait_queue->work_avail);

    return VMM_OK;
}

/**
 * \brief 延迟工作定时器事件处理函数
 * \param ev 指向定时器事件结构的指针
 */
static void delayed_work_timer_event(vmm_timer_event_t *ev)
{
    struct vmm_delayed_work *work = ev->private;

    vmm_workqueue_schedule_work(work->work.wait_queue, &work->work);
}

/**
 * \brief 调度延迟工作到工作队列
 * \param wait_queue 指向目标工作队列结构的指针，如果为NULL则使用当前CPU的系统工作队列
 * \param work 指向要调度的延迟工作结构的指针
 * \param nsecs 延迟时间（纳秒），如果为0则立即调度
 * \return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_schedule_delayed_work(struct vmm_workqueue *wait_queue, struct vmm_delayed_work *work, uint64_t nsecs)
{
    if (!wait_queue) {
        wait_queue = wqctrl.system_workqueue[vmm_smp_processor_id()];
    }

    if (!work) {
        return VMM_ERR_FAIL;
    }

    if (!nsecs) {
        return vmm_workqueue_schedule_work(wait_queue, &work->work);
    }

    work->work.wait_queue = wait_queue;
    INIT_TIMER_EVENT(&work->event, delayed_work_timer_event, work);

    return vmm_timer_event_start(&work->event, nsecs);
}

/**
 * \brief 工作队列主函数，处理工作队列中的工作
 * \param data 指向工作队列结构的指针
 * \return 成功返回VMM_OK，失败返回VMM_ERR_FAIL
 */
static int workqueue_main(void *data)
{
    bool                  do_work;
    irq_flags_t           flags;
    struct vmm_workqueue *wait_queue = data;
    vmm_work_t           *work       = NULL;

    if (!wait_queue) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    while (1) {
        vmm_completion_wait(&wait_queue->work_avail);

        vmm_spin_lock_irq_save(&wait_queue->lock, flags);

        while (!list_empty(&wait_queue->work_list)) {
            work = list_first_entry(&wait_queue->work_list, vmm_work_t, head);
            list_del(&work->head);
            vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

            do_work = FALSE;
            vmm_spin_lock_irq_save(&work->lock, flags);

            if (work->flags & VMM_WORK_STATE_SCHEDULED) {
                work->flags &= ~VMM_WORK_STATE_SCHEDULED;
                work->flags |= VMM_WORK_STATE_INPROGRESS;
                do_work = TRUE;
            }

            vmm_spin_unlock_irq_restore(&work->lock, flags);

            if (do_work) {
                work->func(work);
                vmm_spin_lock_irq_save(&work->lock, flags);
                work->flags &= ~VMM_WORK_STATE_INPROGRESS;
                vmm_spin_unlock_irq_restore(&work->lock, flags);
            }

            vmm_spin_lock_irq_save(&wait_queue->lock, flags);
        }

        vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);
    }

    return VMM_OK;
}

/**
 * \brief 创建一个新的工作队列
 * \param name 工作队列的名称
 * \param priority 工作队列线程的优先级
 * \return 成功返回指向新创建的工作队列结构的指针，失败返回NULL
 */
struct vmm_workqueue *vmm_workqueue_create(const char *name, uint8_t priority)
{
    struct vmm_workqueue *wait_queue; /**< 等待队列 */
    irq_flags_t           flags; /**< 标志位 */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    wait_queue = vmm_zalloc(sizeof(struct vmm_workqueue)); /**< vmm_workqueue))成员 */

    if (!wait_queue) {
        return NULL; /**< NULL成员 */
    }

    INIT_SPIN_LOCK(&wait_queue->lock);
    INIT_LIST_HEAD(&wait_queue->head);
    INIT_LIST_HEAD(&wait_queue->work_list);
    INIT_COMPLETION(&wait_queue->work_avail);

    wait_queue->thread = vmm_threads_create(name, workqueue_main, wait_queue, priority, VMM_THREAD_DEF_TIME_SLICE); /**< VMM_THREAD_DEF_TIME_SLICE)成员 */

    if (!wait_queue->thread) {
        vmm_free(wait_queue);
        return NULL; /**< NULL成员 */
    }

    if (vmm_threads_start(wait_queue->thread)) {
        vmm_threads_destroy(wait_queue->thread);
        vmm_free(wait_queue);
        return NULL; /**< NULL成员 */
    }

    vmm_spin_lock_irq_save(&wqctrl.lock, flags); /**< flags)成员 */

    list_add_tail(&wait_queue->head, &wqctrl.wq_list); /**< &wqctrl.wq_list)成员 */
    wqctrl.wq_count++;

    vmm_spin_unlock_irq_restore(&wqctrl.lock, flags); /**< flags)成员 */

    return wait_queue; /**< 等待队列 */
}

/**
 * \brief 销毁指定的工作队列
 * \param wait_queue 指向要销毁的工作队列结构的指针
 * \return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_destroy(struct vmm_workqueue *wait_queue)
{
    int         rc;
    irq_flags_t flags;

    if (!wait_queue) {
        return VMM_ERR_FAIL;
    }

    if ((rc = vmm_workqueue_flush(wait_queue))) {
        return rc;
    }

    if ((rc = vmm_threads_stop(wait_queue->thread))) {
        return rc;
    }

    vmm_spin_lock_irq_save(&wqctrl.lock, flags);

    list_del(&wait_queue->head);
    wqctrl.wq_count--;

    vmm_spin_unlock_irq_restore(&wqctrl.lock, flags);

    vmm_free(wait_queue);

    return VMM_OK;
}

/**
 * \brief 工作队列启动函数，为指定CPU创建系统工作队列
 * \param cpu_hotplug 指向CPU热插拔通知结构的指针
 * \param cpu CPU编号
 * \return 成功返回VMM_OK，失败返回VMM_ERR_FAIL
 */
static int workqueue_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    char system_workqueue_name[VMM_FIELD_NAME_SIZE];

    /* Create one system workqueue with thread priority
     * as default priority.
     */
    vmm_snprintf(system_workqueue_name, sizeof(system_workqueue_name), "system_workqueue/%d", cpu);
    wqctrl.system_workqueue[cpu] = vmm_workqueue_create(system_workqueue_name, VMM_THREAD_DEF_PRIORITY);

    if (!wqctrl.system_workqueue[cpu]) {
        return VMM_ERR_FAIL;
    }

    return vmm_threads_set_affinity(wqctrl.system_workqueue[cpu]->thread, vmm_cpumask_of(cpu));
}

static vmm_cpu_hotplug_notify_t workqueue_cpu_hotplug = {
    .name    = "WORKQUEUE",
    .state   = VMM_CPU_HOTPLUG_STATE_WORKQUEUE,
    .startup = workqueue_startup,
};

/**
 * \brief 初始化工作队列子系统
 * \return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_workqueue_init(void)
{
    /* Reset control structure */
    memset(&wqctrl, 0, sizeof(wqctrl));

    /* Initialize lock in control structure */
    INIT_SPIN_LOCK(&wqctrl.lock);

    /* Initialize workqueue list */
    INIT_LIST_HEAD(&wqctrl.wq_list);

    /* Initialize workqueue count */
    wqctrl.wq_count = 0;

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&workqueue_cpu_hotplug, TRUE);
}
