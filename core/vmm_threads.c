/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_threads.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor线程源文件，线程运行在VCPU之上.
 */

#include <libs/stringlib.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_main.h>
#include <vmm_scheduler.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>

/**
 * @brief 线程控制结构，管理内核线程的生命周期
 */
struct vmm_threads_ctrl {
    vmm_cpumask_t  default_affinity; /**< 默认亲和性 */
    vmm_spinlock_t lock; /**< 自旋锁 */
    uint32_t       thread_count; /**< thread_count成员 */
    double_list_t  thread_list; /**< thread_list成员 */
};

static struct vmm_threads_ctrl thctrl;

/**
 * @brief 启动threads
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_start(vmm_thread_t *thread_info)
{
    int rc;

    if (!thread_info) {
        return VMM_ERR_FAIL;
    }

    if ((rc = vmm_manager_vcpu_kick(thread_info->vcpu_on_thread))) {
        return rc;
    }

    return VMM_OK;
}

/**
 * @brief 停止threads
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_stop(vmm_thread_t *thread_info)
{
    int rc;

    if (!thread_info) {
        return VMM_ERR_FAIL;
    }

    if ((rc = vmm_manager_vcpu_reset(thread_info->vcpu_on_thread))) {
        return rc;
    }

    return VMM_OK;
}

/**
 * @brief 使线程进入休眠状态
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_sleep(vmm_thread_t *thread_info)
{
    int rc;

    if (!thread_info) {
        return VMM_ERR_FAIL;
    }

    if ((rc = vmm_manager_vcpu_pause(thread_info->vcpu_on_thread))) {
        return rc;
    }

    return VMM_OK;
}

/**
 * @brief 唤醒休眠线程
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_wakeup(vmm_thread_t *thread_info)
{
    int rc;

    if (!thread_info) {
        return VMM_ERR_FAIL;
    }

    if ((rc = vmm_manager_vcpu_resume(thread_info->vcpu_on_thread))) {
        return rc;
    }

    return VMM_OK;
}

/**
 * @brief 获取threads的ID
 * @param thread_info 线程信息结构体指针
 * @return 成功返回线程ID，失败返回U32_MAX
 */
uint32_t vmm_threads_get_id(vmm_thread_t *thread_info)
{
    if (!thread_info) {
        return 0;
    }

    return thread_info->vcpu_on_thread->id;
}

/**
 * @brief 获取threads的优先级
 * @param thread_info 线程信息结构体指针
 * @return 编号值
 */
uint8_t vmm_threads_get_priority(vmm_thread_t *thread_info)
{
    if (!thread_info) {
        return 0;
    }

    return thread_info->vcpu_on_thread->priority;
}

/**
 * @brief 获取threads的名称
 * @param dst 目标缓冲区指针
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_get_name(char *dst, vmm_thread_t *thread_info)
{
    if (!thread_info || !dst) {
        return VMM_ERR_FAIL;
    }

    strcpy(dst, thread_info->vcpu_on_thread->name);

    return VMM_OK;
}

/**
 * @brief 获取threads的状态
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_get_state(vmm_thread_t *thread_info)
{
    int      rc = -1;
    uint32_t state;

    if (!thread_info) {
        rc = -1;
    } else {
        state = vmm_manager_vcpu_get_state(thread_info->vcpu_on_thread);

        if (state & VMM_VCPU_STATE_RESET) {
            rc = VMM_THREAD_STATE_CREATED;
        } else if (state & (VMM_VCPU_STATE_READY | VMM_VCPU_STATE_RUNNING)) {
            rc = VMM_THREAD_STATE_RUNNING;
        } else if (state & VMM_VCPU_STATE_PAUSED) {
            rc = VMM_THREAD_STATE_SLEEPING;
        } else if (state & VMM_VCPU_STATE_HALTED) {
            rc = VMM_THREAD_STATE_STOPPED;
        } else {
            rc = -1;
        }
    }

    return rc;
}

/**
 * @brief 获取threads的hcpu
 * @param thread_info 线程信息结构体指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_get_hcpu(vmm_thread_t *thread_info, uint32_t *host_cpu)
{
    if (!thread_info || !host_cpu) {
        return VMM_ERR_FAIL;
    }

    return vmm_manager_vcpu_get_hcpu(thread_info->vcpu_on_thread, host_cpu);
}

/**
 * @brief 设置线程的hcpu
 * @param thread_info 线程信息结构体指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_thread_set_hcpu(vmm_thread_t *thread_info, uint32_t host_cpu)
{
    if (!thread_info) {
        return VMM_ERR_FAIL;
    }

    return vmm_manager_vcpu_set_hcpu(thread_info->vcpu_on_thread, host_cpu);
}

/**
 * @brief 获取threads的亲和性
 * @param thread_info 线程信息结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
const vmm_cpumask_t *vmm_threads_get_affinity(vmm_thread_t *thread_info)
{
    if (!thread_info) {
        return NULL;
    }

    return vmm_manager_vcpu_get_affinity(thread_info->vcpu_on_thread);
}

/**
 * @brief 设置threads的亲和性
 * @param thread_info 线程信息结构体指针
 * @param cpu_mask CPU亲和性掩码
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_set_affinity(vmm_thread_t *thread_info, const vmm_cpumask_t *cpu_mask)
{
    int           rc;
    vmm_cpumask_t mask;

    if (!thread_info || !cpu_mask) {
        return VMM_ERR_FAIL;
    }

    /* Check affinity mask */
    vmm_cpumask_and(&mask, cpu_mask, cpu_online_mask);

    if (!vmm_cpumask_weight(&mask)) {
        return VMM_ERR_INVALID;
    }

    /* Forcefully set online mask */
    rc = vmm_manager_vcpu_set_affinity(thread_info->vcpu_on_thread, cpu_online_mask);

    if (rc) {
        return rc;
    }

    /* Set final mask */
    return vmm_manager_vcpu_set_affinity(thread_info->vcpu_on_thread, cpu_mask);
}

/**
 * @brief threads id2thread
 * @param thread_id 标识符
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_thread_t *vmm_threads_id2thread(uint32_t thread_id)
{
    bool          found;
    irq_flags_t   flags;
    vmm_thread_t *thread_info;

    thread_info = NULL;
    found       = FALSE;

    /* Lock threads control */
    vmm_spin_lock_irq_save(&thctrl.lock, flags);

    list_for_each_entry(thread_info, &thctrl.thread_list, head)
    {
        if (thread_info->vcpu_on_thread->id == thread_id) {
            found = TRUE;
            break;
        }
    }

    /* Unlock threads control */
    vmm_spin_unlock_irq_restore(&thctrl.lock, flags);

    if (!found) {
        return NULL;
    }

    return thread_info;
}

/**
 * @brief threads index2thread
 * @param index 数组中的索引位置
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_thread_t *vmm_threads_index2thread(int index)
{
    bool          found;
    irq_flags_t   flags;
    vmm_thread_t *thread_info;

    if (index < 0) {
        return NULL;
    }

    thread_info = NULL;
    found       = FALSE;

    /* Lock threads control */
    vmm_spin_lock_irq_save(&thctrl.lock, flags);

    list_for_each_entry(thread_info, &thctrl.thread_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    /* Unlock threads control */
    vmm_spin_unlock_irq_restore(&thctrl.lock, flags);

    if (!found) {
        return NULL;
    }

    return thread_info;
}

/**
 * @brief 获取threads的数量
 * @return 数量值
 */
uint32_t vmm_threads_count(void)
{
    return thctrl.thread_count;
}

/**
 * @brief 线程入口函数
 */
static void vmm_threads_entry(void)
{
    vmm_vcpu_t   *vcpu        = vmm_scheduler_current_vcpu();
    vmm_thread_t *thread_info = NULL;

    /* Sanity check */
    if (!vcpu) {
        vmm_panic("Error: Null vcpu at thread entry.\n");
    }

    /* Sanity check */
    thread_info = vmm_threads_id2thread(vcpu->id);

    if (!thread_info) {
        vmm_panic("Error: Null thread at thread entry.\n");
    }

    /* Enter the thread function */
    thread_info->thread_ret_value = thread_info->thread_func(thread_info->tdata);

    /* Thread finished so, stop it. */
    vmm_threads_stop(thread_info);

    /* Nothing else to do for this thread.
     * Let us hope someone else will destroy it.
     * For now just hang. :( :(
     */
    vmm_hang();
}

/**
 * @brief 创建实时线程
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_thread_t *vmm_threads_create_rt(
    const char *thread_name, int (*thread_fn)(void *udata), void *thread_data, uint8_t thread_priority, uint64_t thread_nsecs,
    uint64_t thread_deadline, uint64_t thread_periodicity)
{
    irq_flags_t   flags;
    vmm_thread_t *thread_info;
    vmm_cpumask_t mask;

    /* Sanity check */
    if (!thread_name || !thread_fn) {
        return NULL;
    }

    /* Prepare affinity mask */
    vmm_cpumask_and(&mask, &thctrl.default_affinity, cpu_online_mask);

    if (!vmm_cpumask_weight(&mask)) {
        memcpy(&mask, cpu_online_mask, sizeof(mask));
    }

    /* Create thread structure instance */
    thread_info = vmm_malloc(sizeof(vmm_thread_t));

    if (!thread_info) {
        return NULL;
    }

    thread_info->thread_func        = thread_fn;
    thread_info->tdata              = thread_data;
    thread_info->thread_nanoseconds = thread_nsecs;

    if (thread_info->thread_nanoseconds == 0) {
        thread_info->thread_nanoseconds = VMM_THREAD_DEF_TIME_SLICE;
    }

    thread_info->thread_deadline = thread_deadline;

    if (thread_info->thread_deadline < thread_info->thread_nanoseconds) {
        thread_info->thread_deadline = thread_info->thread_nanoseconds;
    }

    thread_info->thread_periodicity = thread_periodicity;

    if (thread_info->thread_periodicity < thread_info->thread_deadline) {
        thread_info->thread_periodicity = thread_info->thread_deadline;
    }

    /* Create an orphan vcpu for this thread */
    thread_info->vcpu_on_thread = vmm_manager_vcpu_orphan_create(
        thread_name, (virtual_addr_t)&vmm_threads_entry, CONFIG_THREAD_STACK_SIZE, thread_priority, thread_nsecs, thread_deadline, thread_periodicity,
        &mask);

    if (!thread_info->vcpu_on_thread) {
        vmm_free(thread_info);
        return NULL;
    }

    /* Lock threads control */
    vmm_spin_lock_irq_save(&thctrl.lock, flags);

    list_add_tail(&thread_info->head, &thctrl.thread_list);
    thctrl.thread_count++;

    /* Unlock threads control */
    vmm_spin_unlock_irq_restore(&thctrl.lock, flags);

    return thread_info;
}

/**
 * @brief 销毁threads
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_destroy(vmm_thread_t *thread_info)
{
    int         rc = VMM_OK;
    irq_flags_t flags;

    /* Sanity Check */
    if (!thread_info) {
        return VMM_ERR_FAIL;
    }

    /* Lock threads control */
    vmm_spin_lock_irq_save(&thctrl.lock, flags);

    list_del(&thread_info->head);
    thctrl.thread_count--;

    /* Unlock threads control */
    vmm_spin_unlock_irq_restore(&thctrl.lock, flags);

    /* Destroy the thread VCPU */
    if ((rc = vmm_manager_vcpu_orphan_destroy(thread_info->vcpu_on_thread))) {
        return rc;
    }

    /* Free thread memory */
    vmm_free(thread_info);

    return VMM_OK;
}

/**
 * @brief 初始化threads
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_threads_init(void)
{
    uint32_t                cpu;
    int                     index;
    vmm_device_tree_node_t *node;

    memset(&thctrl, 0, sizeof(thctrl));

    thctrl.default_affinity = VMM_CPU_MASK_NONE;

    node                    = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_CHOSEN_NODE_NAME);

    if (node) {
        index = 0;

        while (vmm_device_tree_read_u32_atindex(node, VMM_DEVICE_TREE_THREADS_AFFINITY_ATTR_NAME, &cpu, index) == VMM_OK) {
            if (cpu < CONFIG_CPU_COUNT) {
                vmm_cpumask_set_cpu(cpu, &thctrl.default_affinity);
            }

            index++;
        }

        vmm_device_tree_dref_node(node);
    }

    INIT_SPIN_LOCK(&thctrl.lock);
    INIT_LIST_HEAD(&thctrl.thread_list);

    return VMM_OK;
}
