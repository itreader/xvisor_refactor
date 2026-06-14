/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file vmm_profiler.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Hypervisor性能分析器源文件
 */

#include <arch_atomic.h>
#include <arch_atomic64.h>
#include <arch_cpu.h>
#include <libs/kallsyms.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_profiler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>

typedef void (*vmm_profile_callback_t)(void *, void *);

/**
 * @brief 性能分析控制结构，管理计数器和统计数据
 */
struct vmm_profiler_ctrl {
    bool                      is_active; /**< is_active成员 */
    bool                      is_in_trace[CONFIG_CPU_COUNT]; /**< is_in_trace成员 */
    struct vmm_profiler_stat *stat; /**< 状态 */
};

static struct vmm_profiler_ctrl pctrl;

/**
 * @brief 空性能分析器（无操作）
 * @param ip IP地址
 * @param parent_ip 父函数调用地址（用于调用栈追踪）
 */
static __notrace void vmm_profile_none(void *ip, void *parent_ip)
{
    // Default NULL function
}

static vmm_profile_callback_t _vmm_profile_enter = vmm_profile_none;
static vmm_profile_callback_t _vmm_profile_exit  = vmm_profile_none;

/**
 * @brief 性能分析函数入口回调
 * @param ip IP地址
 * @param parent_ip 父函数调用地址（用于调用栈追踪）
 * @return 成功返回VMM_OK，失败返回错误码
 */
void __notrace __cyg_profile_func_enter(void *ip, void *parent_ip)
{
    (*_vmm_profile_enter)(ip, parent_ip);
}

/**
 * @brief 性能分析函数退出回调（CygProfiler）
 * @param ip IP地址
 * @param parent_ip 父函数调用地址（用于调用栈追踪）
 * @return 成功返回VMM_OK，失败返回错误码
 */
void __notrace __cyg_profile_func_exit(void *ip, void *parent_ip)
{
    (*_vmm_profile_exit)(ip, parent_ip);
}

/**
 * @brief 性能分析函数进入回调（CygProfiler）
 * @param ip IP地址
 * @param parent_ip 父函数调用地址（用于调用栈追踪）
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __notrace vmm_profile_enter(void *ip, void *parent_ip)
{
    int index;
    int parent_index;
    int i;
    vmm_profiler_counter_t *ptr;
    int                     cpu_id = vmm_smp_processor_id();

    if (pctrl.is_in_trace[cpu_id]) {
        return;
    }

    pctrl.is_in_trace[cpu_id] = TRUE;

    index                     = kallsyms_get_symbol_pos((long uint32_t)ip, NULL, NULL);
    parent_index              = kallsyms_get_symbol_pos((long uint32_t)parent_ip, NULL, NULL);

retry:
    i = 0;

    while (pctrl.stat[index].counter[i].parent_index && (i < (VMM_PROFILE_OTHER_INDEX))) {
        if (pctrl.stat[index].counter[i].parent_index == parent_index) {
            break;
        }

        i++;
    }

    if (i < VMM_PROFILE_OTHER_INDEX) {
        if (pctrl.stat[index].counter[i].parent_index == 0) {
            pctrl.stat[index].counter[i].parent_index = parent_index;
            goto retry;
        } else {
            ptr = &pctrl.stat[index].counter[i];
        }
    } else {
        ptr = &pctrl.stat[index].counter[VMM_PROFILE_OTHER_INDEX];
    }

    arch_atomic_add(&ptr->count, 1);
    /*
     * we use time_per_call as a temporary variable, it will be
     * filled in later on with a meaningfull value.
     */
    arch_atomic64_add(&ptr->time_per_call, vmm_timer_timestamp_for_profile());

    pctrl.is_in_trace[cpu_id] = FALSE;
}

/**
 * @brief 性能分析器退出清理
 * @param ip IP地址
 * @param parent_ip 父函数调用地址（用于调用栈追踪）
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __notrace vmm_profile_exit(void *ip, void *parent_ip)
{
    int index;
    int parent_index;
    int i;
    uint64_t time;
    uint64_t previous;
    vmm_profiler_counter_t *ptr;
    int                     cpu_id = vmm_smp_processor_id();

    if (pctrl.is_in_trace[cpu_id]) {
        return;
    }

    pctrl.is_in_trace[cpu_id] = TRUE;

    index                     = kallsyms_get_symbol_pos((long uint32_t)ip, NULL, NULL);
    parent_index              = kallsyms_get_symbol_pos((long uint32_t)parent_ip, NULL, NULL);

    i                         = 0;

    while (pctrl.stat[index].counter[i].parent_index && (i < (VMM_PROFILE_OTHER_INDEX))) {
        if (pctrl.stat[index].counter[i].parent_index == parent_index) {
            break;
        }

        i++;
    }

    if (i < VMM_PROFILE_OTHER_INDEX) {
        if (pctrl.stat[index].counter[i].parent_index == 0) {
            goto out;
        } else {
            ptr = &pctrl.stat[index].counter[i];
        }
    } else {
        ptr = &pctrl.stat[index].counter[VMM_PROFILE_OTHER_INDEX];
    }

    time     = vmm_timer_timestamp_for_profile();
    previous = arch_atomic64_read(&ptr->time_per_call);

    /*
     * we use time_per_call as a temporary variable, it will be
     * filled in later on with a meaningfull value.
     */
    if (time >= previous) {
        arch_atomic64_add(&ptr->total_time, time - previous);
        arch_atomic64_sub(&ptr->time_per_call, previous);
    } else {
        arch_atomic64_sub(&ptr->time_per_call, time);
    }

out:
    pctrl.is_in_trace[cpu_id] = FALSE;
}

/**
 * @brief 检查性能分析器是否处于活动状态
 * @return 成功返回VMM_OK，失败返回错误码
 */
bool __notrace vmm_profiler_isactive(void)
{
    return pctrl.is_active;
}

/**
 * @brief 启动性能分析器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __notrace vmm_profiler_start(void)
{
    if (!vmm_profiler_isactive()) {
        int i;

        for (i = 0; i < CONFIG_CPU_COUNT; i++) {
            pctrl.is_in_trace[i] = FALSE;
        }

        memset(pctrl.stat, 0, sizeof(struct vmm_profiler_stat) * kallsyms_num_syms);

        _vmm_profile_enter = vmm_profile_enter;
        _vmm_profile_exit  = vmm_profile_exit;

        pctrl.is_active    = TRUE;
    } else {
        return VMM_ERR_FAIL;
    }

    return VMM_OK;
}

/**
 * @brief 停止性能分析器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __notrace vmm_profiler_stop(void)
{
    if (vmm_profiler_isactive()) {
        pctrl.is_active    = FALSE;

        _vmm_profile_enter = vmm_profile_none;
        _vmm_profile_exit  = vmm_profile_none;
    } else {
        return VMM_ERR_FAIL;
    }

    return VMM_OK;
}

struct vmm_profiler_stat *vmm_profiler_get_stat_array(void)
{
    return pctrl.stat; /**< pctrl.stat成员 */
}

/**
 * @brief 初始化性能分析器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_profiler_init(void)
{
    pctrl.stat = vmm_zalloc(sizeof(struct vmm_profiler_stat) * kallsyms_num_syms);

    if (pctrl.stat == NULL) {
        return VMM_ERR_FAIL;
    }

    return VMM_OK;
}
