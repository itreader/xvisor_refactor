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
 * @file vmm_clocksource.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 时钟源管理实现
 */

#include <arch_timer.h>
#include <libs/stringlib.h>
#include <vmm_clocksource.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

/** Control structure for clocksource manager */
/**
 * @brief 时钟源子系统控制结构，维护已注册的时钟源列表
 */
struct vmm_clocksource_ctrl {
    vmm_spinlock_t                       lock; /**< 自旋锁 */
    double_list_t                        clock_src_list; /**< 时钟源链表 */
    const struct vmm_device_tree_nodeid *clock_src_matches; /**< 时钟源匹配计数 */
};

static struct vmm_clocksource_ctrl csctrl;

/**
 * @brief 获取时间计数器时钟源的频率
 * @param tc 时钟计数器指针
 * @return 成功返回时钟源频率(Hz)，失败返回0
 */
uint32_t vmm_timecounter_clocksource_frequency(vmm_timecounter_t *tc)
{
    return (tc && tc->cs) ? tc->cs->freq : 0;
}

#if defined(CONFIG_PROFILE)
/**
 * @brief 读取时间计数器当前值
 * @return 成功返回目标指针，失败返回NULL
 */
 * We need to have a special version of vmm_timecounter_read() for
 * profile where we do not modify the vmm_timecounter structure members.
 * Modifying them while profiling results in xvisor freezing.
 */
uint64_t __notrace vmm_timecounter_read_for_profile(vmm_timecounter_t *tc)
{
    uint64_t cycles_now;
    uint64_t cycles_delta;
    uint64_t ns_offset;

    if (!tc || !tc->cs) {
        return 0;
    }

    cycles_now   = tc->cs->read(tc->cs);
    cycles_delta = (cycles_now - tc->cycles_last) & tc->cs->mask;
    ns_offset    = vmm_clocksource_delta2nsecs(cycles_delta, tc->cs->mult, tc->cs->shift);

    return tc->nsec + ns_offset;
}
#endif

/**
 * @brief 读取时间计数器当前值
 * @param tc 时钟计数器指针
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timecounter_read(vmm_timecounter_t *tc)
{
    uint64_t cycles_now;
    uint64_t cycles_delta;
    uint64_t ns_offset;

    if (!tc || !tc->cs) {
        return 0;
    }

    cycles_now      = tc->cs->read(tc->cs);
    cycles_delta    = (cycles_now - tc->cycles_last) & tc->cs->mask;
    tc->cycles_last = cycles_now;

    ns_offset       = vmm_clocksource_delta2nsecs(cycles_delta, tc->cs->mult, tc->cs->shift);
    tc->nsec += ns_offset;

    return tc->nsec;
}

/**
 * @brief 启动timecounter
 * @param tc 时钟计数器指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timecounter_start(vmm_timecounter_t *tc)
{
    if (!tc || !tc->cs) {
        return VMM_ERR_FAIL;
    }

    if (tc->cs->enable) {
        tc->cs->enable(tc->cs);
    }

    return VMM_OK;
}

/**
 * @brief 停止timecounter
 * @param tc 时钟计数器指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timecounter_stop(vmm_timecounter_t *tc)
{
    if (!tc || !tc->cs) {
        return VMM_ERR_FAIL;
    }

    if (tc->cs->disable) {
        tc->cs->disable(tc->cs);
    }

    return VMM_OK;
}

/**
 * @brief 初始化timecounter
 * @param tc 时钟计数器指针
 * @param cs 时钟源结构体指针
 * @param start_nsec 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timecounter_init(vmm_timecounter_t *tc, vmm_clocksource_t *cs, uint64_t start_nsec)
{
    if (!tc || !cs) {
        return VMM_ERR_FAIL;
    }

    tc->cs          = cs;
    tc->cycles_last = cs->read(cs);
    tc->nsec        = start_nsec;

    return VMM_OK;
}

/**
 * @brief 注册时钟源
 * @param cs 时钟源结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clocksource_register(vmm_clocksource_t *cs)
{
    bool               found;
    irq_flags_t        flags;
    vmm_clocksource_t *cst;

    if (!cs) {
        return VMM_ERR_FAIL;
    }

    cst   = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&csctrl.lock, flags);

    list_for_each_entry(cst, &csctrl.clock_src_list, head)
    {
        if (strcmp(cst->name, cs->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&csctrl.lock, flags);
        return VMM_ERR_FAIL;
    }

    INIT_LIST_HEAD(&cs->head);
    list_add_tail(&cs->head, &csctrl.clock_src_list);

    vmm_spin_unlock_irq_restore(&csctrl.lock, flags);

    return VMM_OK;
}

/**
 * @brief 注销时钟源
 * @param cs 时钟源结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clocksource_unregister(vmm_clocksource_t *cs)
{
    bool               found;
    irq_flags_t        flags;
    vmm_clocksource_t *cst;

    if (!cs) {
        return VMM_ERR_FAIL;
    }

    vmm_spin_lock_irq_save(&csctrl.lock, flags);

    if (list_empty(&csctrl.clock_src_list)) {
        vmm_spin_unlock_irq_restore(&csctrl.lock, flags);
        return VMM_ERR_FAIL;
    }

    cst   = NULL;
    found = FALSE;
    list_for_each_entry(cst, &csctrl.clock_src_list, head)
    {
        if (strcmp(cst->name, cs->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&csctrl.lock, flags);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&cs->head);

    vmm_spin_unlock_irq_restore(&csctrl.lock, flags);

    return VMM_OK;
}

/**
 * @brief 获取最佳时钟源
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_clocksource_t *vmm_clocksource_best(void)
{
    int                rating = 0;
    irq_flags_t        flags;
    vmm_clocksource_t *cs = NULL;
    vmm_clocksource_t *best_cs = NULL;

    cs      = NULL;
    best_cs = NULL;

    vmm_spin_lock_irq_save(&csctrl.lock, flags);

    list_for_each_entry(cs, &csctrl.clock_src_list, head)
    {
        if (cs->rating > rating) {
            best_cs = cs;
            rating  = cs->rating;
        }
    }

    vmm_spin_unlock_irq_restore(&csctrl.lock, flags);

    return best_cs;
}

/**
 * @brief 查找时钟源
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_clocksource_t *vmm_clocksource_find(const char *name)
{
    bool               found;
    irq_flags_t        flags;
    vmm_clocksource_t *cs;

    if (!name) {
        return NULL;
    }

    found = FALSE;
    cs    = NULL;

    vmm_spin_lock_irq_save(&csctrl.lock, flags);

    list_for_each_entry(cs, &csctrl.clock_src_list, head)
    {
        if (strcmp(cs->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&csctrl.lock, flags);

    if (!found) {
        return NULL;
    }

    return cs;
}

/**
 * @brief 时钟源 获取
 * @param index 数组中的索引位置
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_clocksource_t *vmm_clocksource_get(int index)
{
    bool               found;
    irq_flags_t        flags;
    vmm_clocksource_t *cs;

    if (index < 0) {
        return NULL;
    }

    cs    = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&csctrl.lock, flags);

    list_for_each_entry(cs, &csctrl.clock_src_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&csctrl.lock, flags);

    if (!found) {
        return NULL;
    }

    return cs;
}

/**
 * @brief 获取时钟源的数量
 * @return 数量值
 */
uint32_t vmm_clocksource_count(void)
{
    uint32_t           retval = 0;
    irq_flags_t        flags;
    vmm_clocksource_t *cs;

    vmm_spin_lock_irq_save(&csctrl.lock, flags);

    list_for_each_entry(cs, &csctrl.clock_src_list, head)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&csctrl.lock, flags);

    return retval;
}

/**
 * @brief 初始化架构相关的时钟源
 * @return 数量值
 */
int __init __weak arch_clocksource_init(void)
{
    /* Default weak implementation in-case
     * architecture does not provide one.
     */
    return VMM_OK;
}

/**
 * @brief 查找时钟源节点ID表
 * @param node 设备树节点指针
 * @param match 匹配回调函数
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __init clocksource_nidtable_found(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int                    err;
    vmm_clocksource_init_t init_fn = match->data;

    if (!init_fn) {
        return;
    }

    err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE

    if (err) {
        vmm_printf("%s: Init %s node failed (error %d)\n", __func__, node->name, err);
    }

#else
    (void)err;
#endif
}

/**
 * @brief 初始化时钟源
 * @return 编号值
 */
int __init vmm_clocksource_init(void)
{
    int rc;

    /* Initialize clocksource list lock */
    INIT_SPIN_LOCK(&csctrl.lock);

    /* Initialize clocksource list */
    INIT_LIST_HEAD(&csctrl.clock_src_list);

    /* Determine clocksource matches from nodeid table */
    csctrl.clock_src_matches = vmm_device_tree_nidtable_create_matches("clocksource");

    /* Initialize arch specific clocksources */
    if ((rc = arch_clocksource_init())) {
        return rc;
    }

    /* Probe all device tree nodes matching
     * clocksource nodeid table enteries.
     */
    if (csctrl.clock_src_matches) {
        vmm_device_tree_iterate_matching(NULL, csctrl.clock_src_matches, clocksource_nidtable_found, NULL);
    }

    return VMM_OK;
}
