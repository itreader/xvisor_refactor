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
 * @file vmm_completion.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of completion events for Orphan VCPU (or Thread).
 */

#include <arch_cpu_irq.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_stdio.h>

bool vmm_completion_done(vmm_completion_t *cmpl)
{
    bool        ret = TRUE;
    irq_flags_t flags;

    BUG_ON(!cmpl);

    vmm_spin_lock_irq_save(&cmpl->wait_queue.lock, flags);

    if (!cmpl->done) {
        ret = FALSE;
    }

    vmm_spin_unlock_irq_restore(&cmpl->wait_queue.lock, flags);

    return ret;
}

static int completion_wait_common(vmm_completion_t *cmpl, uint64_t *timeout)
{
    int         rc = VMM_OK;
    irq_flags_t flags;

    BUG_ON(!cmpl);
    BUG_ON(!vmm_scheduler_orphan_context());

    vmm_spin_lock_irq_save(&cmpl->wait_queue.lock, flags);

    if (!cmpl->done) {
        rc = __vmm_waitqueue_sleep(&cmpl->wait_queue, timeout);
    }

    if (cmpl->done) {
        cmpl->done--;
    }

    vmm_spin_unlock_irq_restore(&cmpl->wait_queue.lock, flags);

    return rc;
}

int vmm_completion_wait(vmm_completion_t *cmpl)
{
    return completion_wait_common(cmpl, NULL);
}

int vmm_completion_wait_timeout(vmm_completion_t *cmpl, uint64_t *timeout)
{
    return completion_wait_common(cmpl, timeout);
}

int vmm_completion_complete(vmm_completion_t *cmpl)
{
    int         rc = VMM_OK;
    irq_flags_t flags;

    BUG_ON(!cmpl);

    vmm_spin_lock_irq_save(&cmpl->wait_queue.lock, flags);

    cmpl->done++;
    rc = __vmm_waitqueue_wakefirst(&cmpl->wait_queue);

    vmm_spin_unlock_irq_restore(&cmpl->wait_queue.lock, flags);

    return rc;
}

int vmm_completion_complete_once(vmm_completion_t *cmpl)
{
    int         rc = VMM_OK;
    irq_flags_t flags;

    BUG_ON(!cmpl);

    vmm_spin_lock_irq_save(&cmpl->wait_queue.lock, flags);

    if (!cmpl->done) {
        cmpl->done++;
        rc = __vmm_waitqueue_wakefirst(&cmpl->wait_queue);
    }

    vmm_spin_unlock_irq_restore(&cmpl->wait_queue.lock, flags);

    return rc;
}

int vmm_completion_complete_all(vmm_completion_t *cmpl)
{
    int         rc = VMM_OK;
    irq_flags_t flags;

    BUG_ON(!cmpl);

    vmm_spin_lock_irq_save(&cmpl->wait_queue.lock, flags);

    cmpl->done += 0xFFFFFFFFUL / 2;
    rc = __vmm_waitqueue_wakeall(&cmpl->wait_queue);

    vmm_spin_unlock_irq_restore(&cmpl->wait_queue.lock, flags);

    return rc;
}
