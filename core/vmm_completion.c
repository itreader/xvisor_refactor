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
 * @brief 孤儿VCPU（或线程）完成量事件实现
 */

#include <arch_cpu_irq.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_stdio.h>

/**
 * @brief 检查完成量是否已完成
 * @param cmpl 完成量结构体指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
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

/**
 * @brief 等待完成量被触发的通用实现
 * @param cmpl 完成量结构体指针
 * @param timeout 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
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

/**
 * @brief 完成量 等待
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_wait(vmm_completion_t *cmpl)
{
    return completion_wait_common(cmpl, NULL);
}

/**
 * @brief 带超时的等待完成量被触发
 * @param cmpl 完成量结构体指针
 * @param timeout 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_completion_wait_timeout(vmm_completion_t *cmpl, uint64_t *timeout)
{
    return completion_wait_common(cmpl, timeout);
}

/**
 * @brief 标记完成量已完成
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
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

/**
 * @brief 触发完成量，仅唤醒第一个等待者
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
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

/**
 * @brief 触发完成量，唤醒所有等待者
 * @param cmpl 完成量结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
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
