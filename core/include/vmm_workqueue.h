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
 * @file vmm_workqueue.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 工作队列（特殊工作线程）头文件
 */

#ifndef __VMM_WORKQUEUE_H__
#define __VMM_WORKQUEUE_H__

#include <libs/list.h>
#include <vmm_completion.h>
#include <vmm_spinlocks.h>
#include <vmm_threads.h>
#include <vmm_timer.h>

enum {
    VMM_WORK_STATE_CREATED    = 0x1, /**< 工作项已创建但尚未调度 */
    VMM_WORK_STATE_SCHEDULED  = 0x2, /**< 工作项已调度等待执行 */
    VMM_WORK_STATE_INPROGRESS = 0x4, /**< 工作项正在执行中 */
};

struct vmm_work;
typedef struct vmm_work vmm_work_t;

/**
 * @brief 工作项处理函数类型
 * @param work 指向工作项结构的指针
 */
typedef void (*vmm_work_func_t)(vmm_work_t *work);

/**
 * @brief 工作队列结构，管理待执行的异步任务队列
 */
struct vmm_workqueue {
    vmm_spinlock_t   lock;        /**< 保护工作队列的自旋锁 */
    double_list_t    head;        /**< 工作队列链表头 */
    double_list_t    work_list;   /**< 待处理工作项链表 */
    vmm_completion_t work_avail;  /**< 工作项可用完成量 */
    vmm_thread_t    *thread;      /**< 工作队列处理线程 */
};

/**
 * @brief 工作队列控制结构，管理异步任务的调度和执行
 */
struct vmm_workqueue_ctrl {
    vmm_spinlock_t        lock;        /**< 保护工作队列控制器的自旋锁 */
    double_list_t         wq_list;     /**< 工作队列链表 */
    uint32_t              wq_count;    /**< 工作队列计数 */
    struct vmm_workqueue *system_workqueue[CONFIG_CPU_COUNT]; /**< 每个CPU的系统工作队列 */
};

/**
 * @brief 工作任务结构，封装单个异步执行的工作项和回调
 */
struct vmm_work {
    vmm_spinlock_t        lock;        /**< 保护工作项的自旋锁 */
    double_list_t         head;        /**< 链表节点，用于将工作项挂载到工作队列 */
    uint32_t              flags;       /**< 工作项状态标志，使用VMM_WORK_STATE_* */
    struct vmm_workqueue *wait_queue;  /**< 工作项所属的工作队列 */
    vmm_work_func_t       func;        /**< 工作项处理函数 */
};

/**
 * @brief 延迟工作任务结构，在指定延迟后执行的工作项
 */
struct vmm_delayed_work {
    vmm_work_t        work;   /**< 基础工作项 */
    vmm_timer_event_t event;  /**< 定时器事件，用于延迟调度 */
};

/**
 * @brief 初始化工作项
 * @param w 指向工作项结构的指针
 * @param _f 工作项处理函数
 */
#define INIT_WORK(w, _f)                                                                                                                             \
    do {                                                                                                                                             \
        INIT_SPIN_LOCK(&(w)->lock);                                                                                                                  \
        INIT_LIST_HEAD(&(w)->head);                                                                                                                  \
        (w)->flags      = VMM_WORK_STATE_CREATED;                                                                                                    \
        (w)->wait_queue = NULL;                                                                                                                      \
        (w)->func       = _f;                                                                                                                        \
    } while (0)

/**
 * @brief 初始化延迟工作项
 * @param w 指向延迟工作项结构的指针
 * @param _f 工作项处理函数
 */
#define INIT_DELAYED_WORK(w, _f)                                                                                                                     \
    do {                                                                                                                                             \
        INIT_WORK(&(w)->work, _f);                                                                                                                   \
        INIT_TIMER_EVENT(&(w)->event, NULL, NULL);                                                                                                   \
    } while (0)

#define __WORK_INITIALIZER(n, f)                                                                                                                     \
    {                                                                                                                                                \
        .lock = __SPINLOCK_INITIALIZER((n).lock), .flags = VMM_WORK_STATE_CREATED, .head = {&(n).head, &(n).head}, .wait_queue = NULL, .func = (f),  \
    }

#define __DELAYED_WORK_INITIALIZER(n, f)                                                                                                             \
    {                                                                                                                                                \
        .work = __WORK_INITIALIZER((n).work, f), .event = __TIMER_EVENT_INITIALIZER((n).event, NULL, NULL),                                          \
    }

/**
 * @brief 声明并初始化工作项
 * @param n 工作项变量名
 * @param f 工作项处理函数
 */
#define DECLARE_WORK(n, f)         vmm_work_t n = __WORK_INITIALIZER(n, f)

/**
 * @brief 声明并初始化延迟工作项
 * @param n 延迟工作项变量名
 * @param f 工作项处理函数
 */
#define DECLARE_DELAYED_WORK(n, f) struct vmm_delayed_work n = __DELAYED_WORK_INITIALIZER(n, f)

/**
 * @brief 设置工作项的处理函数
 * @param _work 指向工作项结构的指针
 * @param _func 工作项处理函数
 */
#define PREPARE_WORK(_work, _func)                                                                                                                   \
    do {                                                                                                                                             \
        (_work)->func = (_func);                                                                                                                     \
    } while (0)

/**
 * @brief 检查工作项是否为新建状态
 * @param work 指向工作项结构的指针
 * @return 如果工作项是新建状态则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_isnew(vmm_work_t *work);

/**
 * @brief 检查工作项是否处于等待执行状态
 * @param work 指向工作项结构的指针
 * @return 如果工作项处于等待执行状态则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_pending(vmm_work_t *work);

/**
 * @brief 检查工作项是否正在执行中
 * @param work 指向工作项结构的指针
 * @return 如果工作项正在执行中则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_inprogress(vmm_work_t *work);

/**
 * @brief 检查工作项是否已完成
 * @param work 指向工作项结构的指针
 * @return 如果工作项已完成则返回TRUE，否则返回FALSE
 */
bool vmm_workqueue_work_completed(vmm_work_t *work);

/**
 * @brief 调度工作项到指定的工作队列
 * @param wait_queue 指向目标工作队列结构的指针，如果为NULL则使用系统工作队列
 * @param work 指向要调度的工作项结构的指针
 * @return 成功返回VMM_OK，如果工作已调度返回VMM_ERR_ALREADY，失败返回VMM_ERR_FAIL
 */
int vmm_workqueue_schedule_work(struct vmm_workqueue *wait_queue, vmm_work_t *work);

/**
 * @brief 调度延迟工作项到指定的工作队列
 * @param wait_queue 指向目标工作队列结构的指针，如果为NULL则使用系统工作队列
 * @param work 指向要调度的延迟工作项结构的指针
 * @param nsecs 延迟时间（纳秒），如果为0则立即调度
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_schedule_delayed_work(struct vmm_workqueue *wait_queue, struct vmm_delayed_work *work, uint64_t nsecs);

/**
 * @brief 停止已调度或正在执行的工作项
 * @param work 指向要停止的工作项结构的指针
 * @return 成功返回VMM_OK，失败返回VMM_ERR_FAIL
 */
int vmm_workqueue_stop_work(vmm_work_t *work);

/**
 * @brief 停止已调度或正在执行的延迟工作项
 * @param work 指向要停止的延迟工作项结构的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_stop_delayed_work(struct vmm_delayed_work *work);

/**
 * @brief 强制刷新工作队列，清空所有待处理工作
 * @param wait_queue 指向要刷新的工作队列结构的指针
 * @return 成功返回VMM_OK，失败返回VMM_ERR_FAIL
 */
int vmm_workqueue_flush(struct vmm_workqueue *wait_queue);

/**
 * @brief 获取工作队列的处理线程
 * @param wait_queue 指向工作队列结构的指针
 * @return 返回工作队列的处理线程指针，如果失败返回NULL
 */
vmm_thread_t *vmm_workqueue_get_thread(struct vmm_workqueue *wait_queue);

/**
 * @brief 根据索引获取工作队列
 * @param index 工作队列的索引
 * @return 返回对应索引的工作队列指针，如果不存在返回NULL
 */
struct vmm_workqueue *vmm_workqueue_index2workqueue(int index);

/**
 * @brief 获取工作队列的的数量
 * @return 返回当前工作队列的数量
 */
uint32_t vmm_workqueue_count(void);

/**
 * @brief 销毁工作队列
 * @param wait_queue 指向要销毁的工作队列结构的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_destroy(struct vmm_workqueue *wait_queue);

/**
 * @brief 创建工作队列
 * @param name 工作队列的名称
 * @param priority 工作队列线程的优先级
 * @return 成功返回指向新创建的工作队列结构的指针，失败返回NULL
 */
struct vmm_workqueue *vmm_workqueue_create(const char *name, uint8_t priority);

/**
 * @brief 初始化工作队列子系统
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_workqueue_init(void);

#endif /* __VMM_WORKQUEUE_H__ */
