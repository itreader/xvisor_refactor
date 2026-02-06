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
 * @file vmm_mutex.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of mutext locks for Orphan VCPU (or Thread).
 */

#include <arch_cpu_irq.h>
#include <vmm_error.h>
#include <vmm_mutex.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>

void __vmm_mutex_cleanup(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *vcpu_res)
{
    irq_flags_t  flags;
    vmm_mutex_t *mut = container_of(vcpu_res, vmm_mutex_t, res);

    if (!vcpu || !vcpu_res) {
        return;
    }

    vmm_spin_lock_irq_save(&mut->wait_queue.lock, flags);

    if (mut->lock && mut->owner == vcpu) {
        mut->lock  = 0;
        mut->owner = NULL;
        __vmm_waitqueue_wakeall(&mut->wait_queue);
    }

    vmm_spin_unlock_irq_restore(&mut->wait_queue.lock, flags);
}

bool vmm_mutex_avail(vmm_mutex_t *mut)
{
    bool        ret;
    irq_flags_t flags;

    BUG_ON(!mut);

    vmm_spin_lock_irq_save(&mut->wait_queue.lock, flags);
    ret = (mut->lock) ? FALSE : TRUE;
    vmm_spin_unlock_irq_restore(&mut->wait_queue.lock, flags);

    return ret;
}

vmm_vcpu_t *vmm_mutex_owner(vmm_mutex_t *mut)
{
    vmm_vcpu_t *ret;
    irq_flags_t flags;

    BUG_ON(!mut);

    vmm_spin_lock_irq_save(&mut->wait_queue.lock, flags);
    ret = mut->owner;
    vmm_spin_unlock_irq_restore(&mut->wait_queue.lock, flags);

    return ret;
}

int vmm_mutex_unlock(vmm_mutex_t *mut)
{
    int         rc = VMM_EINVALID;
    irq_flags_t flags;
    vmm_vcpu_t *current_vcpu = vmm_scheduler_current_vcpu();

    BUG_ON(!mut);
    BUG_ON(!vmm_scheduler_orphan_context());

    vmm_spin_lock_irq_save(&mut->wait_queue.lock, flags);

    if (mut->lock && mut->owner == current_vcpu) {
        mut->lock--;

        if (!mut->lock) {
            mut->owner = NULL;
            vmm_manager_vcpu_resource_remove(current_vcpu, &mut->res);
            rc = __vmm_waitqueue_wakefirst(&mut->wait_queue);

            if (rc == VMM_ENOENT) {
                rc = VMM_OK;
            }
        } else {
            rc = VMM_OK;
        }
    }

    vmm_spin_unlock_irq_restore(&mut->wait_queue.lock, flags);

    return rc;
}

int vmm_mutex_trylock(vmm_mutex_t *mut)
{
    int         ret          = 0;
    vmm_vcpu_t *current_vcpu = vmm_scheduler_current_vcpu();

    BUG_ON(!mut);
    BUG_ON(!vmm_scheduler_orphan_context());

    vmm_spin_lock_irq(&mut->wait_queue.lock);

    if (!mut->lock) {
        mut->lock++;
        vmm_manager_vcpu_resource_add(current_vcpu, &mut->res);
        mut->owner = current_vcpu;
        ret        = 1;
    } else if (mut->owner == current_vcpu) {
        /*
         * If VCPU owning the lock try to acquire it again then let
         * it acquire lock multiple times (as-per POSIX standard).
         */
        mut->lock++;
        ret = 1;
    }

    vmm_spin_unlock_irq(&mut->wait_queue.lock);

    return ret;
}

static int mutex_lock_common(vmm_mutex_t *mut, uint64_t *timeout)
{
    int         rc = VMM_OK;
    irq_flags_t flags;
    vmm_vcpu_t *current_vcpu = vmm_scheduler_current_vcpu();

    BUG_ON(!mut);
    BUG_ON(!vmm_scheduler_orphan_context());

    vmm_spin_lock_irq_save(&mut->wait_queue.lock, flags);

    while (mut->lock) {
        /*
         * If VCPU owning the lock try to acquire it again then let
         * it acquire lock multiple times (as-per POSIX standard).
         */
        if (mut->owner == current_vcpu) {
            break;
        }

        rc = __vmm_waitqueue_sleep(&mut->wait_queue, timeout);

        if (rc) {
            /* Timeout or some other failure */
            break;
        }
    }

    if (rc == VMM_OK) {
        if (!mut->lock) {
            mut->lock = 1;
            vmm_manager_vcpu_resource_add(current_vcpu, &mut->res);
            mut->owner = current_vcpu;
        } else {
            mut->lock++;
        }
    }

    vmm_spin_unlock_irq_restore(&mut->wait_queue.lock, flags);

    return rc;
}

int vmm_mutex_lock(vmm_mutex_t *mut)
{
    return mutex_lock_common(mut, NULL);
}

int vmm_mutex_lock_timeout(vmm_mutex_t *mut, uint64_t *timeout)
{
    return mutex_lock_common(mut, timeout);
}
