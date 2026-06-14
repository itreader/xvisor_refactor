/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_schedule_algorithm_prr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 优先级轮转调度算法实现
 */

#include <libs/list.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_schedule_algorithm.h>

/**
 * @brief 周期性实时调度算法的运行队列条目，包含红黑树节点、关联VCPU及周期性参数
 */
struct vmm_schedule_algorithm_rq_entry {
    double_list_t head; /**< 链表头 */
    vmm_vcpu_t   *vcpu; /**< 虚拟CPU */
};

/**
 * @brief 周期性实时调度算法的运行队列，按优先级管理各红黑树根节点和VCPU计数
 */
struct vmm_schedule_algorithm_rq {
    double_list_t list[VMM_VCPU_MAX_PRIORITY + 1]; /**< 链表 */
};

/**
 * @brief 调度算法VCPU初始化设置
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_vcpu_setup(vmm_vcpu_t *vcpu)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;

    if (!vcpu) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    rq_entry = vmm_malloc(sizeof(struct vmm_schedule_algorithm_rq_entry));

    if (!rq_entry) {
        return VMM_ERR_FAIL;
    }

    INIT_LIST_HEAD(&rq_entry->head);
    rq_entry->vcpu      = vcpu;
    vcpu->sched_private = rq_entry;

    return VMM_OK;
}

/**
 * @brief 调度算法VCPU清理回调
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_vcpu_cleanup(vmm_vcpu_t *vcpu)
{
    if (!vcpu) {
        return VMM_ERR_FAIL;
    }

    if (vcpu->sched_private) {
        vmm_free(vcpu->sched_private);
        vcpu->sched_private = NULL;
    }

    return VMM_OK;
}

/**
 * @brief 获取调度算法就绪队列的长度
 * @param rq 请求队列指针
 * @param priority 优先级
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_length(void *rq, uint8_t priority)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;
    struct vmm_schedule_algorithm_rq       *rqi;
    int                                     count = 0;

    if (!rq) {
        return -1; /**< -1 */
    }

    rqi = rq;

    list_for_each_entry(rq_entry, &rqi->list[priority], head)
    {
        count++;
    }

    return count;
}

/**
 * @brief 将VCPU加入调度算法的就绪队列
 * @param rq 请求队列指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_enqueue(void *rq, vmm_vcpu_t *vcpu)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;
    struct vmm_schedule_algorithm_rq       *rqi;

    if (!rq || !vcpu) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    rqi      = rq;
    rq_entry = vcpu->sched_private;

    if (!rq_entry) {
        return VMM_ERR_FAIL;
    }

    list_add_tail(&rq_entry->head, &rqi->list[vcpu->priority]);

    return VMM_OK;
}

/**
 * @brief 从调度算法的就绪队列中取出VCPU
 * @param rq 请求队列指针
 * @param next 指向VCPU结构体的指针
 * @param next_time_slice 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_dequeue(void *rq, vmm_vcpu_t **next, uint64_t *next_time_slice)
{
    int                                     p;
    struct vmm_schedule_algorithm_rq_entry *rq_entry;
    struct vmm_schedule_algorithm_rq       *rqi;

    if (!rq) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    rqi = rq;

    p   = VMM_VCPU_MAX_PRIORITY + 1;

    while (p) {
        if (!list_empty(&rqi->list[p - 1])) {
            break;
        }

        p--;
    }

    if (!p) {
        return VMM_ERR_NOTAVAIL;
    }

    p        = p - 1;
    rq_entry = list_first_entry(&rqi->list[p], struct vmm_schedule_algorithm_rq_entry, head);
    list_del_init(&rq_entry->head);

    if (next) {
        *next = rq_entry->vcpu;
    }

    if (next_time_slice) {
        *next_time_slice = rq_entry->vcpu->time_slice;
    }

    return VMM_OK;
}

/**
 * @brief 从调度算法的就绪队列中分离指定VCPU
 * @param rq 请求队列指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_detach(void *rq, vmm_vcpu_t *vcpu)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;

    if (!vcpu) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    rq_entry = vcpu->sched_private;

    if (!rq_entry) {
        return VMM_ERR_FAIL;
    }

    list_del_init(&rq_entry->head);

    return VMM_OK;
}

/**
 * @brief 检查调度算法就绪队列是否需要抢占
 * @param rq 请求队列指针
 * @param current 指向VCPU结构体的指针
 * @return 就绪返回TRUE，未就绪返回FALSE
 */
bool vmm_schedule_algorithm_ready_queue_prempt_needed(void *rq, vmm_vcpu_t *current)
{
    int                               p;
    bool                              ret = FALSE;
    struct vmm_schedule_algorithm_rq *rqi;

    if (!rq || !current) {
        return FALSE; /**< FALSE成员 */
    }

    rqi = rq;

    p   = VMM_VCPU_MAX_PRIORITY;

    while (p > current->priority) {
        if (!list_empty(&rqi->list[p])) {
            ret = TRUE;
            break;
        }

        p--;
    }

    return ret;
}

/**
 * @brief 创建调度算法就绪队列
 * @return 成功返回创建的对象指针，失败返回NULL
 */
void *vmm_schedule_algorithm_ready_queue_create(void)
{
    int                               p;
    struct vmm_schedule_algorithm_rq *rq = vmm_malloc(sizeof(struct vmm_schedule_algorithm_rq));

    if (rq) {
        for (p = 0; p <= VMM_VCPU_MAX_PRIORITY; p++) {
            INIT_LIST_HEAD(&rq->list[p]); /**< 链表 */
        }
    }

    return rq;
}

/**
 * @brief 销毁调度算法就绪队列
 * @param rq 请求队列指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_schedule_algorithm_ready_queue_destroy(void *rq)
{
    if (rq) {
        vmm_free(rq);
        return VMM_OK;
    }

    return VMM_ERR_FAIL;
}
