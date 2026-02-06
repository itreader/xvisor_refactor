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
 * @brief Implementaion of Orphan VCPU (or Thread) wait queue.
 */

#include <arch_cpu_irq.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_waitqueue.h>

uint32_t vmm_waitqueue_count(vmm_wait_queue_t *wait_queue)
{
    BUG_ON(!wait_queue);

    return wait_queue->vcpu_count;
}

struct vmm_waitqueue_priv {
    vmm_wait_queue_t  *wait_queue;
    vmm_timer_event_t *ev;
};

static void waitqueue_timeout(vmm_timer_event_t *event)
{
    vmm_vcpu_t *vcpu = event->private;

    vmm_waitqueue_wake(vcpu);
}

/* NOTE: Must be called with wait_queue->lock held */
static void waitqueue_cleanup(vmm_vcpu_t *vcpu)
{
    vmm_wait_queue_t          *wait_queue;
    vmm_timer_event_t         *ev;
    struct vmm_waitqueue_priv *p = vcpu->wq_private;

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
    list_del_init(&vcpu->wq_head);

    /* Decrement VCPU count in waitqueue */
    if (wait_queue->vcpu_count) {
        wait_queue->vcpu_count--;
    }

    /* Clear waitqueue cleanup callback so that it's called only once */
    vcpu->wq_cleanup = NULL;
}

/* NOTE: Must be called with wait_queue->lock held */
int __vmm_waitqueue_sleep(vmm_wait_queue_t *wait_queue, uint64_t *timeout_nsecs)
{
    int                       rc  = VMM_OK;
    uint64_t                  now = 0, expiry = 0;
    vmm_vcpu_t               *vcpu;
    vmm_timer_event_t         wake_event;
    struct vmm_waitqueue_priv p = {.wait_queue = wait_queue, .ev = NULL};

    /* Sanity checks */
    BUG_ON(!wait_queue);
    BUG_ON(!vmm_scheduler_orphan_context());

    if (timeout_nsecs && (*timeout_nsecs == 0)) {
        return VMM_ETIMEDOUT;
    }

    /* Get current VCPU */
    vcpu             = vmm_scheduler_current_vcpu();

    /* Update VCPU waitqueue context */
    vcpu->wq_lock    = &wait_queue->lock;
    vcpu->wq_private = &p;

    /* Add VCPU to waitqueue */
    list_add_tail(&vcpu->wq_head, &wait_queue->vcpu_list);

    /* Increment VCPU count in waitqueue */
    wait_queue->vcpu_count++;

    /* If timeout is required then create timer event */
    if (timeout_nsecs) {
        INIT_TIMER_EVENT(&wake_event, &waitqueue_timeout, vcpu);
        p.ev = &wake_event;
        vmm_timer_event_start2(&wake_event, *timeout_nsecs, &expiry);
    }

    /* Update waitqueue cleanup callback */
    vcpu->wq_cleanup = waitqueue_cleanup;

    /* Try to Pause VCPU */
    rc               = vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_PAUSED);

    /* Set VCPU waitqueue context to NULL */
    vcpu->wq_lock    = NULL;
    vcpu->wq_private = NULL;

    /* VCPU Wokeup successfully */
    if (rc == VMM_OK) {
        /* If timeout was used then update error code */
        if (timeout_nsecs) {
            now            = vmm_timer_timestamp();
            *timeout_nsecs = (now > expiry) ? 0 : (expiry - now);

            if (*timeout_nsecs == 0) {
                rc = VMM_ETIMEDOUT;
            }
        }
    }

    return rc;
}

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

int vmm_waitqueue_forced_remove(vmm_vcpu_t *vcpu)
{
    irq_flags_t                flags;
    vmm_wait_queue_t          *wait_queue;
    struct vmm_waitqueue_priv *p = vcpu->wq_private;

    /* Sanity check */
    if (!p || !p->wait_queue) {
        return VMM_EFAIL;
    }

    wait_queue = p->wait_queue;

    /* Lock waitqueue */
    vmm_spin_lock_irq_save(&wait_queue->lock, flags);

    /* Cleanup waitqueue */
    if (vcpu->wq_cleanup) {
        vcpu->wq_cleanup(vcpu);
    }

    /* Set VCPU waitqueue context to NULL */
    vcpu->wq_lock    = NULL;
    vcpu->wq_private = NULL;

    /* Unlock waitqueue */
    vmm_spin_unlock_irq_restore(&wait_queue->lock, flags);

    return VMM_OK;
}

static int __vmm_waitqueue_wake(vmm_wait_queue_t *wait_queue, vmm_vcpu_t *vcpu)
{
    return vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_READY);
}

int vmm_waitqueue_wake(vmm_vcpu_t *vcpu)
{
    int                        rc = VMM_OK;
    irq_flags_t                flags;
    vmm_wait_queue_t          *wait_queue;
    struct vmm_waitqueue_priv *p = vcpu->wq_private;

    /* Sanity checks */
    if (!vcpu || vcpu->is_normal || !p || !p->wait_queue) {
        return VMM_EFAIL;
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

int __vmm_waitqueue_wakefirst(vmm_wait_queue_t *wait_queue)
{
    vmm_vcpu_t *vcpu;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* We should have atleast one VCPU in waitqueue list */
    if (list_empty(&wait_queue->vcpu_list)) {
        return VMM_ENOENT;
    }

    /* Get first VCPU from waitqueue list */
    vcpu = list_entry(list_first(&wait_queue->vcpu_list), vmm_vcpu_t, wq_head);

    /* Try to Resume VCPU */
    return __vmm_waitqueue_wake(wait_queue, vcpu);
}

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

int __vmm_waitqueue_wakeall(vmm_wait_queue_t *wait_queue)
{
    int         rc;
    vmm_vcpu_t *vcpu, *nvcpu;

    /* Sanity checks */
    BUG_ON(!wait_queue);

    /* We should have atleast one VCPU in waitqueue list */
    if (list_empty(&wait_queue->vcpu_list)) {
        return VMM_ENOENT;
    }

    /* Try resume every VCPU till empty */
    list_for_each_entry_safe(vcpu, nvcpu, &wait_queue->vcpu_list, wq_head)
    {
        /* Try to Resume VCPU */
        if ((rc = __vmm_waitqueue_wake(wait_queue, vcpu))) {
            /* Return Failure */
            return rc;
        }
    }

    return VMM_OK;
}

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
