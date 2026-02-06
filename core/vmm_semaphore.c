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
 * @brief Implementation of sempahore locks for Orphan VCPU (or Thread).
 */

#include <arch_cpu_irq.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_semaphore.h>
#include <vmm_stdio.h>

struct vmm_semaphore_resource {
    double_list_t       head;
    uint32_t            count;
    vmm_semaphore_t    *sem;
    vmm_vcpu_t         *vcpu;
    vmm_vcpu_resource_t res;
};

/* Note: This function must be called with semaphore waitqueue lock held */
static struct vmm_semaphore_resource *__semaphore_find_resource(vmm_semaphore_t *sem, vmm_vcpu_t *vcpu)
{
    bool                           found = FALSE;
    struct vmm_semaphore_resource *sres;

    list_for_each_entry(sres, &sem->res_list, head)
    {
        if (sres->vcpu == vcpu) {
            found = TRUE;
            break;
        }
    }

    return (found) ? sres : NULL;
}

/* Note: This function must be called with semaphore waitqueue lock held */
static struct vmm_semaphore_resource *__semaphore_first_resource(vmm_semaphore_t *sem)
{
    if (list_empty(&sem->res_list)) {
        return NULL;
    }

    return list_first_entry(&sem->res_list, struct vmm_semaphore_resource, head);
}

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

        if (rc == VMM_ENOENT) {
            rc = VMM_OK;
        }
    } else {
        rc = VMM_EINVALID;
    }

    vmm_spin_unlock_irq_restore(&sem->wait_queue.lock, flags);

    return rc;
}

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

int vmm_semaphore_down(vmm_semaphore_t *sem)
{
    return semaphore_down_common(sem, NULL);
}

int vmm_semaphore_down_timeout(vmm_semaphore_t *sem, uint64_t *timeout)
{
    return semaphore_down_common(sem, timeout);
}
