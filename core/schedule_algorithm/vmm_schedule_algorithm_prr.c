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
 * @brief implementation of priority round-robin scheduling algorithm
 */

#include <libs/list.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_schedule_algorithm.h>

struct vmm_schedule_algorithm_rq_entry {
    double_list_t head;
    vmm_vcpu_t   *vcpu;
};

struct vmm_schedule_algorithm_rq {
    double_list_t list[VMM_VCPU_MAX_PRIORITY + 1];
};

int vmm_schedule_algorithm_vcpu_setup(vmm_vcpu_t *vcpu)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;

    if (!vcpu) {
        return VMM_EFAIL;
    }

    rq_entry = vmm_malloc(sizeof(struct vmm_schedule_algorithm_rq_entry));

    if (!rq_entry) {
        return VMM_EFAIL;
    }

    INIT_LIST_HEAD(&rq_entry->head);
    rq_entry->vcpu      = vcpu;
    vcpu->sched_private = rq_entry;

    return VMM_OK;
}

int vmm_schedule_algorithm_vcpu_cleanup(vmm_vcpu_t *vcpu)
{
    if (!vcpu) {
        return VMM_EFAIL;
    }

    if (vcpu->sched_private) {
        vmm_free(vcpu->sched_private);
        vcpu->sched_private = NULL;
    }

    return VMM_OK;
}

int vmm_schedule_algorithm_rq_length(void *rq, uint8_t priority)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;
    struct vmm_schedule_algorithm_rq       *rqi;
    int                                     count = 0;

    if (!rq) {
        return -1;
    }

    rqi = rq;

    list_for_each_entry(rq_entry, &rqi->list[priority], head)
    {
        count++;
    }

    return count;
}

int vmm_schedule_algorithm_rq_enqueue(void *rq, vmm_vcpu_t *vcpu)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;
    struct vmm_schedule_algorithm_rq       *rqi;

    if (!rq || !vcpu) {
        return VMM_EFAIL;
    }

    rqi      = rq;
    rq_entry = vcpu->sched_private;

    if (!rq_entry) {
        return VMM_EFAIL;
    }

    list_add_tail(&rq_entry->head, &rqi->list[vcpu->priority]);

    return VMM_OK;
}

int vmm_schedule_algorithm_rq_dequeue(void *rq, vmm_vcpu_t **next, uint64_t *next_time_slice)
{
    int                                     p;
    struct vmm_schedule_algorithm_rq_entry *rq_entry;
    struct vmm_schedule_algorithm_rq       *rqi;

    if (!rq) {
        return VMM_EFAIL;
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
        return VMM_ENOTAVAIL;
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

int vmm_schedule_algorithm_rq_detach(void *rq, vmm_vcpu_t *vcpu)
{
    struct vmm_schedule_algorithm_rq_entry *rq_entry;

    if (!vcpu) {
        return VMM_EFAIL;
    }

    rq_entry = vcpu->sched_private;

    if (!rq_entry) {
        return VMM_EFAIL;
    }

    list_del_init(&rq_entry->head);

    return VMM_OK;
}

bool vmm_schedule_algorithm_rq_prempt_needed(void *rq, vmm_vcpu_t *current)
{
    int                               p;
    bool                              ret = FALSE;
    struct vmm_schedule_algorithm_rq *rqi;

    if (!rq || !current) {
        return FALSE;
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

void *vmm_schedule_algorithm_rq_create(void)
{
    int                               p;
    struct vmm_schedule_algorithm_rq *rq = vmm_malloc(sizeof(struct vmm_schedule_algorithm_rq));

    if (rq) {
        for (p = 0; p <= VMM_VCPU_MAX_PRIORITY; p++) {
            INIT_LIST_HEAD(&rq->list[p]);
        }
    }

    return rq;
}

int vmm_schedule_algorithm_rq_destroy(void *rq)
{
    if (rq) {
        vmm_free(rq);
        return VMM_OK;
    }

    return VMM_EFAIL;
}
