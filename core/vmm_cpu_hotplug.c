/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file vmm_cpu_hotplug.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU热插拔通知器接口
 */

#include <vmm_compiler.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_cpumask.h>
#include <vmm_error.h>
#include <vmm_per_cpu.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/**
 * @brief CPU热插拔状态表，记录各子系统的初始化状态
 */
struct cpu_hotplug_state {
    vmm_rwlock_t                lock; /**< 自旋锁 */
    enum vmm_cpu_hotplug_states state; /**< 状态 */
};

static DEFINE_PER_CPU(struct cpu_hotplug_state, chpstate);
static DEFINE_RWLOCK(notify_lock);
static LIST_HEAD(notify_list);

/**
 * @brief 获取指定CPU的热插拔状态
 * @param cpu CPU编号
 * @return 当前热插拔状态枚举值
 */
enum vmm_cpu_hotplug_states vmm_cpu_hotplug_get_state(uint32_t cpu)
{
    uint32_t                  ret; /**< 返回值 */
    struct cpu_hotplug_state *chps; /**< chps成员 */

    if (!vmm_cpu_possible(cpu)) {
        return VMM_CPU_HOTPLUG_STATE_OFFLINE; /**< VMM_CPU_HOTPLUG_STATE_OFFLINE成员 */
    }

    chps = &per_cpu(chpstate, cpu); /**< cpu)成员 */

    vmm_read_lock_lite(&chps->lock);
    ret = chps->state; /**< chps->state成员 */
    vmm_read_unlock_lite(&chps->lock);

    return ret; /**< 返回值 */
}

/**
 * @brief 设置CPU热插拔的状态
 * @param state 状态值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_set_state(uint32_t state)
{
    int                       ret      = VMM_OK;
    bool                      teardown = FALSE;
    uint32_t                  cpu      = vmm_smp_processor_id();
    vmm_cpu_hotplug_notify_t *chpn     = NULL;
    struct cpu_hotplug_state *chps     = &per_cpu(chpstate, cpu);

    vmm_read_lock_lite(&notify_lock);

    vmm_write_lock_lite(&chps->lock);

    if (chps->state < state) {
        list_for_each_entry(chpn, &notify_list, head)
        {
            if (chpn->startup && (chps->state < chpn->state) && (chpn->state <= state)) {
                DPRINTF("CPU%d: state=%d notifier=%s %s()\n", cpu, chpn->state, chpn->name, "startup");
                ret = chpn->startup(chpn, cpu);

                if (ret) {
                    break;
                }
            }
        }
    } else if (chps->state > state) {
        teardown = TRUE;
        list_for_each_entry_reverse(chpn, &notify_list, head)
        {
            if (chpn->teardown && (state < chpn->state) && (chpn->state <= chps->state)) {
                DPRINTF("CPU%d: state=%d notifier=%s %s()\n", cpu, chpn->state, chpn->name, "teardown");
                ret = chpn->teardown(chpn, cpu);

                if (ret) {
                    break;
                }
            }
        }
    }

    chps->state = state;

    vmm_write_unlock_lite(&chps->lock);

    vmm_read_unlock_lite(&notify_lock);

    if (ret && chpn) {
        vmm_printf(
            "CPU%d: hotplug state=%d notifier=%s %s() failed "
            "(error %d)\n",
            cpu, chpn->state, chpn->name, (teardown) ? "teardown" : "startup", ret);
    }

    return ret;
}

/**
 * @brief 注册CPU热插拔同步回调函数
 * @param arg1 第一个参数值
 * @param arg2 第二个参数值
 * @param arg3 第三个参数值
 */
static void cpu_hotplug_register_sync(void *arg1, void *arg2, void *arg3)
{
    uint32_t                  cpu         = vmm_smp_processor_id();
    vmm_cpu_hotplug_notify_t *cpu_hotplug = arg1;
    struct cpu_hotplug_state *chps        = &per_cpu(chpstate, cpu);

    vmm_read_lock_lite(&chps->lock);

    if (cpu_hotplug->startup && (cpu_hotplug->state <= chps->state)) {
        cpu_hotplug->startup(cpu_hotplug, cpu); /**< cpu)成员 */
    }

    vmm_read_unlock_lite(&chps->lock);
}

/**
 * @brief 注册CPU热插拔
 * @param cpu_hotplug CPU热插拔结构体指针
 * @param invoke_startup 是否调用启动函数标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_register(vmm_cpu_hotplug_notify_t *cpu_hotplug, bool invoke_startup)
{
    uint32_t cpu;
    uint32_t curr_cpu;
    bool                      found = FALSE;
    struct cpu_hotplug_state *chps  = NULL;
    vmm_cpu_hotplug_notify_t *chpn  = NULL;

    if (!cpu_hotplug) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (cpu_hotplug->state <= VMM_CPU_HOTPLUG_STATE_OFFLINE) {
        return VMM_ERR_INVALID;
    }

    vmm_write_lock_lite(&notify_lock);

    list_for_each_entry(chpn, &notify_list, head)
    {
        if (chpn == cpu_hotplug) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_write_unlock_lite(&notify_lock);
        return VMM_ERR_EXIST;
    }

    found = FALSE;
    list_for_each_entry(chpn, &notify_list, head)
    {
        if (cpu_hotplug->state < chpn->state) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        list_add_tail(&cpu_hotplug->head, &chpn->head);
    } else {
        list_add_tail(&cpu_hotplug->head, &notify_list);
    }

    vmm_write_unlock_lite(&notify_lock);

    if (!invoke_startup || !cpu_hotplug->startup) {
        goto done;
    }

    curr_cpu = vmm_smp_processor_id();
    chps     = &per_cpu(chpstate, curr_cpu);
    vmm_read_lock_lite(&chps->lock);

    if (cpu_hotplug->state <= chps->state) {
        cpu_hotplug->startup(cpu_hotplug, curr_cpu);
    }

    vmm_read_unlock_lite(&chps->lock);

    for_each_online_cpu(cpu)
    {
        if (cpu == curr_cpu) {
            continue;
        }

        chps = &per_cpu(chpstate, cpu);
        vmm_read_lock_lite(&chps->lock);

        if (cpu_hotplug->state <= chps->state) {
            vmm_smp_ipi_async_call(vmm_cpumask_of(cpu), cpu_hotplug_register_sync, cpu_hotplug, NULL, NULL);
        }

        vmm_read_unlock_lite(&chps->lock);
    }

done:
    return VMM_OK;
}

/**
 * @brief 注销CPU热插拔
 * @param cpu_hotplug CPU热插拔结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_unregister(vmm_cpu_hotplug_notify_t *cpu_hotplug)
{
    bool                      found = FALSE;
    vmm_cpu_hotplug_notify_t *chpn;

    if (!cpu_hotplug) {
        return VMM_ERR_INVALID;
    }

    vmm_write_lock_lite(&notify_lock);

    list_for_each_entry(chpn, &notify_list, head)
    {
        if (chpn == cpu_hotplug) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_write_unlock_lite(&notify_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&cpu_hotplug->head);

    vmm_write_unlock_lite(&notify_lock);

    return VMM_OK;
}

/**
 * @brief 初始化CPU热插拔
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_cpu_hotplug_init(void)
{
    uint32_t                  cpu;
    struct cpu_hotplug_state *chps;

    for_each_possible_cpu(cpu)
    {
        chps = &per_cpu(chpstate, cpu); /**< cpu)成员 */
        INIT_RW_LOCK(&chps->lock);
        chps->state = VMM_CPU_HOTPLUG_STATE_OFFLINE; /**< VMM_CPU_HOTPLUG_STATE_OFFLINE成员 */
    }

    return VMM_OK;
}
