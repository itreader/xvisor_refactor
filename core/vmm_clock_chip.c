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
 * @file vmm_clock_chip.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 时钟芯片管理实现
 */

#include <arch_timer.h>
#include <vmm_clock_chip.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_error.h>
#include <vmm_host_irq.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

/** Control structure for clock chip manager */
/**
 * @brief 时钟芯片控制结构，管理周期性定时器的状态
 */
struct vmm_clock_chip_ctrl {
    vmm_spinlock_t lock; /**< 自旋锁 */
    double_list_t  clock_chip_list; /**< 时钟芯片链表 */
};

static struct vmm_clock_chip_ctrl ccctrl;

/**
 * @brief 默认事件处理器，仅忽略事件，不执行任何操作。
 * @param cc 时钟芯片结构体指针。
 */
static void default_event_handler(vmm_clock_chip_t *cc)
{
    /* Just ignore. Do nothing. */
}

/**
 * @brief 设置时钟芯片的事件处理器。
 * @param cc 时钟芯片结构体指针。
 * @param event_handler 事件处理器函数指针。
 */
void vmm_clock_chip_set_event_handler(vmm_clock_chip_t *cc, void (*event_handler)(vmm_clock_chip_t *))
{
    if (cc && event_handler) {
        cc->event_handler = event_handler;
    }
}

/**
 * @brief 为时钟芯片编程下一个事件。
 * @param cc 时钟芯片结构体指针。
 * @param now_ns 当前时间（纳秒）。
 * @param expires_ns 事件过期时间（纳秒）。
 * @return 成功返回0，失败返回错误码。
 */
int vmm_clock_chip_program_event(vmm_clock_chip_t *cc, uint64_t now_ns, uint64_t expires_ns)
{
    uint64_t clc;
    uint64_t delta;

    if (expires_ns < now_ns) {
        return VMM_ERR_FAIL;
    }

    if (cc->mode != VMM_CLOCKCHIP_MODE_ONESHOT) {
        return 0;
    }

    delta          = expires_ns - now_ns;
    cc->next_event = expires_ns;

    if (delta > cc->max_delta_ns) {
        delta = cc->max_delta_ns;
    }

    if (delta < cc->min_delta_ns) {
        delta = cc->min_delta_ns;
    }

    clc = delta * cc->mult;
    clc >>= cc->shift;

    return cc->set_next_event((uint64_t)clc, cc);
}

/**
 * @brief 设置时钟芯片的模式。
 * @param cc 时钟芯片结构体指针。
 * @param mode 要设置的模式。
 */
void vmm_clock_chip_set_mode(vmm_clock_chip_t *cc, vmm_clock_chip_mode_e mode)
{
    if (cc && cc->mode != mode) {
        cc->set_mode(mode, cc);
        cc->mode = mode;

        /* Multiplicator of 0 is invalid and we'd crash on it. */
        if (mode == VMM_CLOCKCHIP_MODE_ONESHOT) {
            if (!cc->mult) {
                vmm_panic("%s: clockchip mult=0 not allowed\n", __func__);
            }
        }
    }
}

/**
 * @brief 注册时钟芯片。
 * @param cc 要注册的时钟芯片结构体指针。
 * @return 成功返回VMM_OK，失败返回错误码。
 */
int vmm_clock_chip_register(vmm_clock_chip_t *cc)
{
    bool              found;
    irq_flags_t       flags;
    vmm_clock_chip_t *cct;

    if (!cc) {
        return VMM_ERR_FAIL;
    }

    cct   = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    list_for_each_entry(cct, &ccctrl.clock_chip_list, head)
    {
        if (cct == cc) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);
        return VMM_ERR_FAIL;
    }

    INIT_LIST_HEAD(&cc->head);
    cc->event_handler = default_event_handler;
    cc->bound_on      = UINT_MAX;
    list_add_tail(&cc->head, &ccctrl.clock_chip_list);

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return VMM_OK;
}

/**
 * @brief 注销时钟芯片。
 * @param cc 要注销的时钟芯片结构体指针。
 * @return 成功返回VMM_OK，失败返回错误码。
 */
int vmm_clock_chip_unregister(vmm_clock_chip_t *cc)
{
    bool              found;
    irq_flags_t       flags;
    vmm_clock_chip_t *cct;

    if (!cc) {
        return VMM_ERR_FAIL;
    }

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    if (list_empty(&ccctrl.clock_chip_list)) {
        vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);
        return VMM_ERR_FAIL;
    }

    cct   = NULL;
    found = FALSE;
    list_for_each_entry(cct, &ccctrl.clock_chip_list, head)
    {
        if (cct == cc) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&cc->head);

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return VMM_OK;
}

/**
 * @brief 为指定主机CPU绑定最佳时钟芯片。
 * @param host_cpu 主机CPU编号。
 * @return 绑定的时钟芯片结构体指针，失败返回NULL。
 */
vmm_clock_chip_t *vmm_clock_chip_bind_best(uint32_t host_cpu)
{
    int                  best_rating;
    irq_flags_t          flags;
    const vmm_cpumask_t *mask;
    vmm_clock_chip_t *cc = NULL;
    vmm_clock_chip_t *best_cc = NULL;

    if (CONFIG_CPU_COUNT <= host_cpu) {
        return NULL;
    }

    mask        = vmm_cpumask_of(host_cpu);
    cc          = NULL;
    best_cc     = NULL;
    best_rating = 0;

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    list_for_each_entry(cc, &ccctrl.clock_chip_list, head)
    {
        if ((cc->rating > best_rating) && (cc->bound_on == UINT_MAX) && vmm_cpumask_intersects(cc->cpumask, mask)) {
            best_cc     = cc;
            best_rating = cc->rating;
        }
    }

    if (best_cc) {
        vmm_host_irq_set_affinity(best_cc->hirq, mask, TRUE);
        best_cc->bound_on = host_cpu;
    }

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return best_cc;
}

/**
 * @brief 解绑时钟芯片。
 * @param cc 要解绑的时钟芯片结构体指针。
 * @return 成功返回VMM_OK，失败返回错误码。
 */
int vmm_clock_chip_unbind(vmm_clock_chip_t *cc)
{
    irq_flags_t flags;

    if (!cc) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);
    cc->bound_on = UINT_MAX;
    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return VMM_OK;
}

/**
 * @brief 根据索引获取时钟芯片。
 * @param index 时钟芯片的索引。
 * @return 时钟芯片结构体指针，失败返回NULL。
 */
vmm_clock_chip_t *vmm_clock_chip_get(int index)
{
    bool              found;
    irq_flags_t       flags;
    vmm_clock_chip_t *cc;

    if (index < 0) {
        return NULL;
    }

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    cc    = NULL;
    found = FALSE;

    list_for_each_entry(cc, &ccctrl.clock_chip_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    if (!found) {
        return NULL;
    }

    return cc;
}

/**
 * @brief 获取已注册时钟芯片的的数量。
 * @return 时钟芯片的数量。
 */
uint32_t vmm_clock_chip_count(void)
{
    uint32_t          retval = 0;
    irq_flags_t       flags;
    vmm_clock_chip_t *cc;

    vmm_spin_lock_irq_save(&ccctrl.lock, flags);

    list_for_each_entry(cc, &ccctrl.clock_chip_list, head)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&ccctrl.lock, flags);

    return retval;
}

/**
 * @brief 架构特定的时钟芯片初始化（弱函数）。
 * @return 成功返回VMM_OK，失败返回错误码。
 */
int __weak arch_clock_chip_init(void)
{
    /* Default weak implementation in-case
     * architecture does not provide one.
     */
    return VMM_OK;
}

/**
 * @brief 设备树节点匹配时钟芯片时的回调函数。
 * @param node 设备树节点指针。
 * @param match 节点ID匹配结构体指针。
 * @param data 用户数据指针。
 */
static void __init clockchip_nidtable_found(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int                   err;
    vmm_clock_chip_init_t init_fn = match->data;

    if (!init_fn) {
        return;
    }

    err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE

    if (err) {
        vmm_printf("%s: CPU%d Init %s node failed (error %d)\n", __func__, vmm_smp_processor_id(), node->name, err);
    }

#else
    (void)err;
#endif
}

/**
 * @brief CPU热插拔时的时钟芯片启动函数。
 * @param cpu_hotplug CPU热插拔通知器指针。
 * @param cpu CPU编号。
 * @return 成功返回VMM_OK，失败返回错误码。
 */
static int clockchip_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int rc;

    /* Initialize arch specific clockchips */
    if ((rc = arch_clock_chip_init())) {
        return rc;
    }

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t clockchip_cpu_hotplug = {
    .name    = "CLOCKCHIP",
    .state   = VMM_CPU_HOTPLUG_STATE_CLOCKCHIP,
    .startup = clockchip_startup,
};

/**
 * @brief 初始化时钟芯片管理器。
 * @return 成功返回VMM_OK，失败返回错误码。
 */
int __init vmm_clock_chip_init(void)
{
    const struct vmm_device_tree_nodeid *clock_chip_matches;

    /* Initialize clockchip list lock */
    INIT_SPIN_LOCK(&ccctrl.lock);

    /* Initialize clockchip list */
    INIT_LIST_HEAD(&ccctrl.clock_chip_list);

    /* Probe all device tree nodes matching
     * clockchip nodeid table enteries.
     */
    clock_chip_matches = vmm_device_tree_nidtable_create_matches("clockchip");

    if (clock_chip_matches) {
        vmm_device_tree_iterate_matching(NULL, clock_chip_matches, clockchip_nidtable_found, NULL);
    }

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&clockchip_cpu_hotplug, TRUE);
}
