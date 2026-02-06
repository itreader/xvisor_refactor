/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vmm_block_partition.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for block device partition management
 */

#include <block/vmm_block_partition.h>
#include <libs/mathlib.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_threads.h>

#define MODULE_DESC      "Block Device Partition Management"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY VMM_BLOCKPART_IPRIORITY
#define MODULE_INIT      vmm_block_partition_init
#define MODULE_EXIT      vmm_block_partition_exit

enum block_partition_work_type {
    BLOCKPART_WORK_UNKNOWN = 0,
    BLOCKPART_WORK_PARSE   = 1,
};

struct block_partition_work {
    double_list_t                  head;
    enum block_partition_work_type type;
    vmm_block_device_t            *block_device;
};

struct block_partition_ctrl {
    vmm_spinlock_t       mngr_list_lock;
    double_list_t        mngr_list;
    vmm_spinlock_t       work_list_lock;
    double_list_t        work_list;
    vmm_completion_t     work_avail;
    uint32_t             work_count;
    vmm_thread_t        *work_thread;
    vmm_notifier_block_t client;
};

static struct block_partition_ctrl bpctrl;

static uint32_t block_partition_count_work(void)
{
    uint32_t    ret = 0;
    irq_flags_t flags;

    vmm_spin_lock_irq_save(&bpctrl.work_list_lock, flags);
    ret = bpctrl.work_count;
    vmm_spin_unlock_irq_restore(&bpctrl.work_list_lock, flags);

    return ret;
}

static struct block_partition_work *block_partition_pop_work(void)
{
    irq_flags_t                  flags;
    struct block_partition_work *w = NULL;

    vmm_spin_lock_irq_save(&bpctrl.work_list_lock, flags);

    if (!list_empty(&bpctrl.work_list)) {
        w = list_first_entry(&bpctrl.work_list, struct block_partition_work, head);
        list_del(&w->head);
        bpctrl.work_count--;
    }

    vmm_spin_unlock_irq_restore(&bpctrl.work_list_lock, flags);

    return w;
}

static void block_partition_add_work(enum block_partition_work_type type, vmm_block_device_t *block_device)
{
    bool                         found;
    irq_flags_t                  flags;
    struct block_partition_work *w;

    if (!block_device) {
        return;
    }

    vmm_spin_lock_irq_save(&bpctrl.work_list_lock, flags);

    found = FALSE;
    list_for_each_entry(w, &bpctrl.work_list, head)
    {
        if ((w->type == type) && (w->block_device == block_device)) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        w = vmm_zalloc(sizeof(struct block_partition_work));

        if (w) {
            INIT_LIST_HEAD(&w->head);
            w->type         = type;
            w->block_device = block_device;
            list_add_tail(&w->head, &bpctrl.work_list);
            bpctrl.work_count++;
        }
    }

    vmm_spin_unlock_irq_restore(&bpctrl.work_list_lock, flags);
}

static void block_partition_del_work(enum block_partition_work_type type, vmm_block_device_t *block_device)
{
    irq_flags_t                  flags;
    struct block_partition_work *w;

    if (!block_device) {
        return;
    }

    vmm_spin_lock_irq_save(&bpctrl.work_list_lock, flags);

    list_for_each_entry(w, &bpctrl.work_list, head)
    {
        if ((w->type == type) && (w->block_device == block_device)) {
            list_del(&w->head);
            bpctrl.work_count--;
            vmm_free(w);
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&bpctrl.work_list_lock, flags);
}

static int block_partition_thread_main(void *udata)
{
    bool                                parsed;
    int                                 rc, i, j, cnt, wcnt;
    struct block_partition_work        *w;
    struct vmm_block_partition_manager *m;

    while (1) {
        vmm_completion_wait(&bpctrl.work_avail);

        wcnt = block_partition_count_work();

        for (i = 0; i < wcnt; i++) {
            w = block_partition_pop_work();

            if (!w) {
                continue;
            }

            switch (w->type) {
                case BLOCKPART_WORK_PARSE:
                    parsed = FALSE;
                    cnt    = vmm_block_partition_manager_count();

                    for (j = 0; j < cnt; j++) {
                        m = vmm_block_partition_manager_get(j);

                        if (!m || !m->parse_part) {
                            continue;
                        }

                        rc = m->parse_part(w->block_device);

                        if (rc) {
                            continue;
                        }

                        parsed                             = TRUE;
                        w->block_device->part_manager_sign = m->sign;
                        break;
                    }

                    if (!parsed) {
                        block_partition_add_work(w->type, w->block_device);
                    }

                    break;

                default:
                    break;
            };

            vmm_free(w);
        }
    };

    return VMM_OK;
}

static void block_partition_signal_one_work(void)
{
    vmm_completion_complete(&bpctrl.work_avail);
}

static void block_partition_signal_all_work(void)
{
    irq_flags_t                  flags;
    struct block_partition_work *w;

    vmm_spin_lock_irq_save(&bpctrl.work_list_lock, flags);

    list_for_each_entry(w, &bpctrl.work_list, head)
    {
        vmm_completion_complete(&bpctrl.work_avail);
    }

    vmm_spin_unlock_irq_restore(&bpctrl.work_list_lock, flags);
}

static int block_partition_block_notification(vmm_notifier_block_t *nb, uint64_t evt, void *data)
{
    uint32_t                            i, cnt;
    int                                 ret = NOTIFY_OK;
    struct vmm_block_device_event      *e   = data;
    struct vmm_block_partition_manager *m;

    /* Raw block device with no parent should only be parsed */
    if (e->block_device->parent) {
        return NOTIFY_DONE;
    }

    switch (evt) {
        case VMM_BLOCK_DEVICE_EVENT_REGISTER:
            block_partition_add_work(BLOCKPART_WORK_PARSE, e->block_device);
            block_partition_signal_one_work();
            break;

        case VMM_BLOCK_DEVICE_EVENT_UNREGISTER:
            block_partition_del_work(BLOCKPART_WORK_PARSE, e->block_device);
            cnt = vmm_block_partition_manager_count();

            for (i = 0; i < cnt; i++) {
                m = vmm_block_partition_manager_get(i);

                if (!m || !m->cleanup_part) {
                    continue;
                }

                if (m->sign == e->block_device->part_manager_sign) {
                    m->cleanup_part(e->block_device);
                    break;
                }
            }

            break;

        default:
            ret = NOTIFY_DONE;
            break;
    }

    return ret;
}

int vmm_block_partition_manager_register(struct vmm_block_partition_manager *mngr)
{
    bool                                found;
    irq_flags_t                         flags;
    struct vmm_block_partition_manager *mngrt;

    if (!mngr) {
        return VMM_EFAIL;
    }

    mngrt = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&bpctrl.mngr_list_lock, flags);

    list_for_each_entry(mngrt, &bpctrl.mngr_list, head)
    {
        if (mngrt->sign == mngr->sign) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);
        return VMM_EFAIL;
    }

    INIT_LIST_HEAD(&mngr->head);
    list_add_tail(&mngr->head, &bpctrl.mngr_list);

    vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);

    /* Some block_partition work might not have been processed due to
     * unavailability of appropriate partition manager.
     * To solve, we give dummy wakeup signal for each available work.
     */
    block_partition_signal_all_work();

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_block_partition_manager_register);

int vmm_block_partition_manager_unregister(struct vmm_block_partition_manager *mngr)
{
    bool                                found;
    irq_flags_t                         flags;
    struct vmm_block_partition_manager *mngrt;

    if (!mngr) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&bpctrl.mngr_list_lock, flags);

    if (list_empty(&bpctrl.mngr_list)) {
        vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);
        return VMM_EFAIL;
    }

    mngrt = NULL;
    found = FALSE;
    list_for_each_entry(mngrt, &bpctrl.mngr_list, head)
    {
        if (mngrt->sign == mngr->sign) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);
        return VMM_ENOTAVAIL;
    }

    list_del(&mngr->head);

    vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(vmm_block_partition_manager_unregister);

struct vmm_block_partition_manager *vmm_block_partition_manager_get(int index)
{
    bool                                found;
    irq_flags_t                         flags;
    struct vmm_block_partition_manager *mngrt;

    if (index < 0) {
        return NULL;
    }

    vmm_spin_lock_irq_save(&bpctrl.mngr_list_lock, flags);

    mngrt = NULL;
    found = FALSE;

    list_for_each_entry(mngrt, &bpctrl.mngr_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);

    if (!found) {
        return NULL;
    }

    return mngrt;
}

VMM_EXPORT_SYMBOL(vmm_block_partition_manager_get);

uint32_t vmm_block_partition_manager_count(void)
{
    uint32_t                            retval = 0;
    irq_flags_t                         flags;
    struct vmm_block_partition_manager *mngrt;

    vmm_spin_lock_irq_save(&bpctrl.mngr_list_lock, flags);

    list_for_each_entry(mngrt, &bpctrl.mngr_list, head)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&bpctrl.mngr_list_lock, flags);

    return retval;
}

VMM_EXPORT_SYMBOL(vmm_block_partition_manager_count);

static int __init block_partition_init_iter(vmm_block_device_t *block_device, void *data)
{
    if (!block_device || block_device->parent) {
        goto done;
    }

    block_partition_add_work(BLOCKPART_WORK_PARSE, block_device);
    block_partition_signal_one_work();

done:
    return VMM_OK;
}

static int __init vmm_block_partition_init(void)
{
    int rc;

    /* Initialize manager list lock */
    INIT_SPIN_LOCK(&bpctrl.mngr_list_lock);

    /* Initialize manager list */
    INIT_LIST_HEAD(&bpctrl.mngr_list);

    /* Initialize work list lock */
    INIT_SPIN_LOCK(&bpctrl.work_list_lock);

    /* Initialize work list */
    INIT_LIST_HEAD(&bpctrl.work_list);

    /* Initialize work available completion */
    INIT_COMPLETION(&bpctrl.work_avail);

    /* Initialize work count */
    bpctrl.work_count           = 0;

    /* Register client for block device notifications */
    bpctrl.client.notifier_call = &block_partition_block_notification;
    bpctrl.client.priority      = 0;
    rc                          = vmm_block_device_register_client(&bpctrl.client);

    if (rc) {
        return rc;
    }

    /* Create block_partition work thread */
    bpctrl.work_thread = vmm_threads_create("partd", block_partition_thread_main, NULL, VMM_THREAD_DEF_PRIORITY, VMM_THREAD_DEF_TIME_SLICE);

    if (!bpctrl.work_thread) {
        vmm_block_device_unregister_client(&bpctrl.client);
        return VMM_EFAIL;
    }

    /* We may have block device already created so we add
     * raw block devices with no parent for partition parsing
     */
    rc = vmm_block_device_iterate(NULL, NULL, block_partition_init_iter);

    if (rc) {
        vmm_threads_destroy(bpctrl.work_thread);
        vmm_block_device_unregister_client(&bpctrl.client);
        return rc;
    }

    /* Start block_partition work thread */
    vmm_threads_start(bpctrl.work_thread);

    return VMM_OK;
}

static void __exit vmm_block_partition_exit(void)
{
    /* Start block_partition workerthread */
    vmm_threads_stop(bpctrl.work_thread);

    /* Destroy block_partition work thread */
    vmm_threads_destroy(bpctrl.work_thread);

    /* Unregister client for block device notifications */
    vmm_block_device_unregister_client(&bpctrl.client);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
