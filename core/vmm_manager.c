/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_manager.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor管理器实现
 */

#include <arch_guest.h>
#include <arch_vcpu.h>
#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_manager.h>
#include <vmm_mutex.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>
#include <vmm_waitqueue.h>
#include <vmm_workqueue.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** Control structure for manager */
/**
 * @brief 客户机管理器控制结构，维护客户机和VCPU的全局状态
 */
struct vmm_manager_ctrl {
    /* Guest & VCPU management */
    vmm_mutex_t       lock;                /**< 管理器锁 */
    uint32_t          vcpu_count;          /**< VCPU计数 */
    uint32_t          guest_count;         /**< Guest计数 */
    vmm_cpumask_t    *vcpu_affinity_mask;   /**< VCPU亲和性掩码 */
    vmm_vcpu_t       *vcpu_array;          /**< VCPU数组 */
    bool             *vcpu_avail_array;    /**< VCPU可用标志数组 */
    struct vmm_guest *guest_array;         /**< Guest数组 */
    bool             *guest_avail_array;   /**< Guest可用标志数组 */
    double_list_t     orphan_vcpu_list;    /**< 孤儿VCPU链表 */
    double_list_t     guest_list;          /**< Guest链表 */
    /* Work structs to process guest request */
    vmm_work_t       *guest_work_array;    /**< Guest工作数组 */
};

static struct vmm_manager_ctrl m_vmm_manager;

/**
 * @brief 获取管理器锁，用于保护管理器数据结构的并发访问
 */
void vmm_manager_lock(void)
{
    vmm_mutex_lock(&m_vmm_manager.lock);
}

/**
 * @brief 释放管理器锁
 */
void vmm_manager_unlock(void)
{
    vmm_mutex_unlock(&m_vmm_manager.lock);
}

/**
 * @brief 获取系统支持的最大VCPU的数量
 * @return 最大VCPU数量
 */
uint32_t vmm_manager_max_vcpu_count(void)
{
    return CONFIG_MAX_VCPU_COUNT;
}

/**
 * @brief 获取当前系统中存在的VCPU的数量
 * @return 当前VCPU数量
 */
uint32_t vmm_manager_vcpu_count(void)
{
    uint32_t ret;

    vmm_manager_lock();
    ret = m_vmm_manager.vcpu_count;
    vmm_manager_unlock();

    return ret;
}

/**
 * @brief 根据VCPU ID获取VCPU指针
 * @param vcpu_id 要获取的VCPU的唯一标识符
 * @return VCPU指针，如果不存在则返回NULL
 */
vmm_vcpu_t *vmm_manager_vcpu(uint32_t vcpu_id)
{
    vmm_vcpu_t *ret = NULL;

    if (vcpu_id < CONFIG_MAX_VCPU_COUNT) {
        vmm_manager_lock();

        if (!m_vmm_manager.vcpu_avail_array[vcpu_id]) {
            ret = &m_vmm_manager.vcpu_array[vcpu_id];
        }

        vmm_manager_unlock();
    }

    return ret;
}

/**
 * @brief 管理器 虚拟CPU 处理器间中断 复位
 * @param vcpu_ptr VCPU指针
 * @param dummy1 预留参数1
 * @param dummy2 预留参数2
 */
static void manager_vcpu_ipi_reset(void *vcpu_ptr, void *dummy1, void *dummy2)
{
    vmm_scheduler_state_change(vcpu_ptr, VMM_VCPU_STATE_RESET);
}

/**
 * @brief 遍历所有VCPU并对每个VCPU执行指定的迭代函数
 * @param iter 指向迭代函数的指针，该函数对每个VCPU执行操作
 * @param private 传递给迭代函数的私有数据
 * @return 如果所有迭代成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_iterate(int (*iter)(vmm_vcpu_t *, void *), void *private)
{
    int rc;
    int v;
    vmm_vcpu_t *vcpu;

    /* If no iteration callback then return */
    if (!iter) {
        return VMM_ERR_INVALID;
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Iterate over each used VCPU instance */
    rc = VMM_OK;

    for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {
        if (m_vmm_manager.vcpu_avail_array[v]) {
            continue;
        }

        vcpu = &m_vmm_manager.vcpu_array[v];

        rc   = iter(vcpu, private);

        if (rc) {
            break;
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    return rc;
}

/* Note: Must be called with manager lock held */
/**
 * @brief 检查硬件CPU是否处于良好状态可供VCPU调度
 * @param priority 优先级
 * @param affinity CPU亲和性掩码
 * @return 可用的硬件CPU编号
 */
static uint32_t __vmm_manager_good_hcpu(uint8_t priority, const vmm_cpumask_t *affinity)
{
    vmm_vcpu_t *vcpu;
    uint32_t    count[CONFIG_CPU_COUNT];
    uint32_t    v;
    uint32_t    c;
    uint32_t    min;
    uint32_t    host_cpu = vmm_cpumask_first(affinity);

    if (!vmm_timer_started() || (vmm_cpumask_weight(affinity) < 1)) {
        return vmm_smp_processor_id();
    }

    for (c = 0; c < CONFIG_CPU_COUNT; c++) {
        count[c] = 0;
    }

    for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {
        if (m_vmm_manager.vcpu_avail_array[v]) {
            continue;
        }

        vcpu = &m_vmm_manager.vcpu_array[v];

        if ((vcpu->priority != priority) || !vmm_cpumask_test_cpu(vcpu->host_cpu, affinity)) {
            continue;
        }

        count[vcpu->host_cpu]++;
    }

    min = count[host_cpu];

    for (c = 0; c < CONFIG_CPU_COUNT; c++) {
        if (!vmm_cpumask_test_cpu(c, affinity)) {
            continue;
        }

        if (count[c] < min) {
            min      = count[c];
            host_cpu = c;
        }
    }

    return host_cpu;
}

/**
 * @brief 获取指定VCPU的当前状态
 * @param vcpu 指向VCPU结构的指针
 * @return VCPU的状态值
 */
uint32_t vmm_manager_vcpu_get_state(vmm_vcpu_t *vcpu)
{
    if (!vcpu) {
        return VMM_VCPU_STATE_UNKNOWN;
    }

    return (uint32_t)arch_atomic_read(&vcpu->state);
}

/**
 * @brief 设置指定VCPU的状态
 * @param vcpu 指向VCPU结构的指针
 * @param new_state 要设置的新状态值
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_set_state(vmm_vcpu_t *vcpu, uint32_t new_state)
{
    uint32_t    vhcpu;
    irq_flags_t flags;

    if (!vcpu) {
        return VMM_ERR_FAIL;
    }

    /* If new_state == VMM_VCPU_STATE_RESET then
     * we use sync IPI for proper working of VCPU reset.
     *
     * For all other states we can directly call
     * scheduler state change
     */

    if (new_state == VMM_VCPU_STATE_RESET) {
        vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
        vhcpu = vcpu->host_cpu;
        vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
        return vmm_smp_ipi_sync_call(vmm_cpumask_of(vhcpu), 1000, manager_vcpu_ipi_reset, vcpu, NULL, NULL);
    }

    return vmm_scheduler_state_change(vcpu, new_state);
}

/**
 * @brief 获取指定VCPU当前分配的主机CPU
 * @param vcpu 指向VCPU结构的指针
 * @param host_cpu 用于存储主机CPU编号的指针
 * @return 如果成
 * 功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu)
{
    return vmm_scheduler_get_hcpu(vcpu, host_cpu);
}

/**
 * @brief 检查指定VCPU是否在当前主机CPU上运行
 * @param vcpu 指向VCPU结构的指针
 * @return 如果在当前CPU上返回true，否则返回false
 */
bool vmm_manager_vcpu_check_current_hcpu(vmm_vcpu_t *vcpu)
{
    return vmm_scheduler_check_current_hcpu(vcpu);
}

/**
 * @brief 设置指定VCPU的主机CPU分配
 * @param vcpu 指向VCPU结构的指针
 * @param host_cpu 要分配的主机CPU编号
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu)
{
    return vmm_scheduler_set_hcpu(vcpu, host_cpu);
}

/**
 * @brief 强制重新调度指定VCPU所在的主机CPU
 * @param vcpu 指向VCPU结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_hcpu_resched(vmm_vcpu_t *vcpu)
{
    int         rc;
    irq_flags_t flags;

    if (!vcpu) {
        return VMM_ERR_INVALID;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
    rc = vmm_scheduler_force_resched(vcpu->host_cpu);
    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return rc;
}

/**
 * @brief 获取VCPU当前所在的硬件CPU编号
 * @param fptr 函数指针
 * @param vptr 通用指针
 * @param data 用户自定义数据指针
 */
static void manager_vcpu_hcpu_func(void *fptr, void *vptr, void *data)
{
    void (*func)(vmm_vcpu_t *, void *) = fptr;
    vmm_vcpu_t *vcpu                   = vptr;

    if (func && vcpu) {
        func(vcpu, data);
    }
}

/**
 * @brief 获取VCPU当前所在的硬件CPU编号
 * @param vcpu 指向VCPU结构体的指针
 * @param state_mask 状态值
 * @param (*func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_vcpu_hcpu_func(vmm_vcpu_t *vcpu, uint32_t state_mask, void (*func)(vmm_vcpu_t *, void *), void *data, bool use_async)
{
    irq_flags_t          flags;
    const vmm_cpumask_t *cpu_mask = NULL;

    if (!vcpu || !func) {
        return VMM_ERR_INVALID;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);

    if (arch_atomic_read(&vcpu->state) & state_mask) {
        cpu_mask = vmm_cpumask_of(vcpu->host_cpu);
    }

    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    if (cpu_mask) {
        if (use_async) {
            vmm_smp_ipi_async_call(cpu_mask, manager_vcpu_hcpu_func, func, vcpu, data);
        } else {
            vmm_smp_ipi_sync_call(cpu_mask, 0, manager_vcpu_hcpu_func, func, vcpu, data);
        }
    }

    return VMM_OK;
}

/**
 * @brief 获取VCPU管理器的亲和性
 * @param vcpu 指向VCPU结构体的指针
 * @return 目标对象指针，不存在返回NULL
 */
const vmm_cpumask_t *vmm_manager_vcpu_get_affinity(vmm_vcpu_t *vcpu)
{
    irq_flags_t          flags;
    const vmm_cpumask_t *cpu_mask = NULL;

    if (!vcpu) {
        return NULL;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
    cpu_mask = vcpu->cpu_affinity;
    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return cpu_mask;
}

/**
 * @brief 设置指定VCPU的CPU亲和性
 * @param vcpu 指向VCPU结构的指针
 * @param cpu_mask 新的CPU亲和性掩码
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_set_affinity(vmm_vcpu_t *vcpu, const vmm_cpumask_t *cpu_mask)
{
    int           rc;
    bool          locked;
    uint32_t      new_hcpu;
    irq_flags_t   flags;
    vmm_cpumask_t and_mask;

    if (!vcpu || !cpu_mask) {
        return VMM_ERR_FAIL;
    }

    /* Lock load balancing */
    vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);

    /* New affinity must overlap current affinity */
    vmm_cpumask_and(&and_mask, vcpu->cpu_affinity, cpu_mask);

    if (!vmm_cpumask_weight(&and_mask)) {
        vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
        return VMM_ERR_INVALID;
    }

    /* Make sure current host_cpu is set in both current and new affinity */
    if (!vmm_cpumask_test_cpu(vcpu->host_cpu, &and_mask)) {
        vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

        /* Acquire manager lock */
        /* NOTE: We only touch manager lock if timer subsystem
         * has started on current host CPU. This check helps
         * create boot-time orphan VCPUs.
         */
        if (vmm_timer_started()) {
            locked = TRUE;
            vmm_manager_lock();
        } else {
            locked = FALSE;
        }

        /* Find good host CPU */
        new_hcpu = __vmm_manager_good_hcpu(vcpu->priority, &and_mask);

        /* Change host CPU */
        rc       = vmm_manager_vcpu_set_hcpu(vcpu, new_hcpu);

        /* Release manager lock */
        if (locked) {
            vmm_manager_unlock();
        }

        /* If set_hcpu failed then return failure */
        if (rc) {
            return rc;
        }

        vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);
    }

    /* Update affinity */
    memcpy(&m_vmm_manager.vcpu_affinity_mask[vcpu->id], cpu_mask, sizeof(*cpu_mask));
    vcpu->cpu_affinity = &m_vmm_manager.vcpu_affinity_mask[vcpu->id];

    /* Unlock load balancing */
    vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return VMM_OK;
}

/**
 * @brief 为指定VCPU添加资源
 * @param vcpu 指向VCPU结构的指针
 * @param res 指向VCPU资源结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_resource_add(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res)
{
    irq_flags_t flags;

    if (!vcpu || !res || !res->name || !res->cleanup) {
        return VMM_ERR_INVALID;
    }

    INIT_LIST_HEAD(&res->head);
    vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);
    list_add_tail(&res->head, &vcpu->res_head);
    vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);

    return VMM_OK;
}

/**
 * @brief 从指定VCPU移除资源
 * @param vcpu 指向VCPU结构的指针
 * @param res 指向VCPU资源结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_resource_remove(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res)
{
    irq_flags_t flags;

    if (!vcpu || !res) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);
    list_del(&res->head);
    vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);

    return VMM_OK;
}

/**
 * @brief 刷新管理器VCPU关联的资源
 * @param vcpu 指向VCPU结构体的指针
 */
static void vmm_manager_vcpu_resource_flush(vmm_vcpu_t *vcpu)
{
    irq_flags_t          flags;
    vmm_vcpu_resource_t *res;

    if (!vcpu) {
        return;
    }

    vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);

    while (!list_empty(&vcpu->res_head)) {
        res = list_entry(list_pop_tail(&vcpu->res_head), vmm_vcpu_resource_t, head);

        vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);

        if (res->cleanup) {
            res->cleanup(vcpu, res);
        }

        vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);
    }

    vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);
}

/**
 * @brief 创建孤立VCPU管理器
 * @return 成功返回新创建的节点指针，失败返回NULL
 */
vmm_vcpu_t *vmm_manager_vcpu_orphan_create(
    const char *name, virtual_addr_t start_pc, virtual_size_t stack_size, uint8_t priority, uint64_t time_slice_nsecs, uint64_t deadline,
    uint64_t periodicity, const vmm_cpumask_t *affinity)
{
    bool                 locked;
    uint32_t vnum;
    uint32_t host_cpu;
    vmm_vcpu_t          *vcpu = NULL;
    const vmm_cpumask_t *aff  = (affinity) ? affinity : cpu_online_mask;

    /* Sanity checks */
    if (name == NULL || start_pc == 0 || time_slice_nsecs == 0) {
        return NULL;
    }

    if (VMM_VCPU_MAX_PRIORITY < priority) {
        return NULL;
    }

    if (priority < VMM_VCPU_MIN_PRIORITY) {
        return NULL;
    }

    /* Acquire manager lock */
    /* NOTE: We only touch manager lock if timer subsystem
     * has started on current host CPU. This check helps
     * create boot-time orphan VCPUs.
     */
    if (vmm_timer_started()) {
        locked = TRUE;
        vmm_manager_lock();
    } else {
        locked = FALSE;
    }

    /* Find good host CPU */
    host_cpu = __vmm_manager_good_hcpu(priority, aff);

    /* Find the next available vcpu */
    for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
        if (!m_vmm_manager.vcpu_avail_array[vnum]) {
            continue;
        }

        vcpu           = &m_vmm_manager.vcpu_array[vnum];

        /* Update priority */
        vcpu->priority = priority;

        /* Update host CPU and affinity */
        vcpu->host_cpu = host_cpu;
        memcpy(&m_vmm_manager.vcpu_affinity_mask[vcpu->id], aff, sizeof(*aff));
        vcpu->cpu_affinity              = &m_vmm_manager.vcpu_affinity_mask[vcpu->id];

        m_vmm_manager.vcpu_avail_array[vcpu->id] = FALSE;
        break;
    }

    if (!vcpu) {
        goto fail;
    }

    INIT_LIST_HEAD(&vcpu->head);

    /* Update general info and state */
    vcpu->subid = 0;

    if (strlcpy(vcpu->name, name, sizeof(vcpu->name)) >= sizeof(vcpu->name)) {
        goto fail_avail;
    }

    vcpu->node        = NULL;
    vcpu->is_normal   = FALSE;
    vcpu->is_poweroff = FALSE;
    vcpu->guest       = NULL;
    arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);

    /* Add VCPU to orphan list */
    list_add_tail(&vcpu->head, &m_vmm_manager.orphan_vcpu_list);

    /* Increment vcpu count */
    m_vmm_manager.vcpu_count++;

    /* Release manager lock */
    if (locked) {
        vmm_manager_unlock();
    }

    /* Setup start program counter and stack */
    vcpu->start_pc              = start_pc;
    vcpu->stack_virtual_address = (virtual_addr_t)vmm_malloc(stack_size);

    if (!vcpu->stack_virtual_address) {
        goto fail_list_del;
    }

    vcpu->stack_size = stack_size;

    /* Intialize dynamic scheduling context */
    INIT_RW_LOCK(&vcpu->sched_lock);
    vcpu->state_tstamp        = vmm_timer_timestamp();
    vcpu->state_ready_nsecs   = 0;
    vcpu->state_running_nsecs = 0;
    vcpu->state_paused_nsecs  = 0;
    vcpu->state_halted_nsecs  = 0;
    vcpu->system_nsecs        = 0;
    vcpu->reset_count         = 0;
    vcpu->reset_timestamp     = 0;
    vcpu->preempt_count       = 0;
    vcpu->resumed             = FALSE;
    vcpu->sched_private       = NULL;

    /* Intialize static scheduling context */
    vcpu->time_slice          = time_slice_nsecs;
    vcpu->deadline            = deadline;

    if (vcpu->deadline < vcpu->time_slice) {
        vcpu->deadline = vcpu->time_slice;
    }

    vcpu->periodicity = periodicity;

    if (vcpu->periodicity < vcpu->deadline) {
        vcpu->periodicity = vcpu->deadline;
    }

    /* Initialize architecture specific context */
    vcpu->arch_private = NULL;

    if (arch_vcpu_init(vcpu)) {
        goto fail_free_stack;
    }

    /* Initialize resource list */
    INIT_SPIN_LOCK(&vcpu->res_lock);
    INIT_LIST_HEAD(&vcpu->res_head);

    /* Initialize waitqueue context and cleanup callback */
    INIT_LIST_HEAD(&vcpu->wait_queue_head);
    vcpu->wait_queue_lock    = NULL;
    vcpu->wait_queue_private = NULL;
    vcpu->wait_queue_cleanup = NULL;

    /* Notify scheduler about new VCPU */
    if (vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET)) {
        goto fail_vcpu_deinit;
    }

    return vcpu;

fail_vcpu_deinit:
    arch_vcpu_deinit(vcpu);
fail_free_stack:
    vmm_free((void *)vcpu->stack_virtual_address);
fail_list_del:

    if (vmm_timer_started()) {
        vmm_manager_lock();
        locked = TRUE;
    } else {
        locked = FALSE;
    }

    m_vmm_manager.vcpu_count--;
    list_del(&vcpu->head);
fail_avail:
    m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE;
fail:

    if (locked) {
        vmm_manager_unlock();
    }

    return NULL;
}

/**
 * @brief 销毁孤儿VCPU
 * @param vcpu 指向要销毁的VCPU结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_vcpu_orphan_destroy(vmm_vcpu_t *vcpu)
{
    int rc = VMM_ERR_FAIL;

    /* Sanity checks */
    if (!vcpu) {
        return rc;
    }

    if (vcpu->is_normal) {
        return rc;
    }

    /* Force VCPU out of waitqueue */
    vmm_waitqueue_forced_remove(vcpu);

    /* Reset the VCPU */
    if ((rc = vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET))) {
        return rc;
    }

    /* Flush all resources acquired by this VCPU */
    vmm_manager_vcpu_resource_flush(vcpu);

    /* Set VCPU to unknown state (This will clean scheduling context) */
    if ((rc = vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_UNKNOWN))) {
        return rc;
    }

    vcpu->sched_private = NULL;

    /* Deinit architecture specific context */
    if ((rc = arch_vcpu_deinit(vcpu))) {
        return rc;
    }

    /* Free stack pages */
    if (vcpu->stack_virtual_address) {
        vmm_free((void *)vcpu->stack_virtual_address);
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Decrement vcpu count */
    m_vmm_manager.vcpu_count--;

    /* Remove VCPU from orphan list */
    list_del(&vcpu->head);

    /* Mark this VCPU as available */
    m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE;

    /* Release manager lock */
    vmm_manager_unlock();

    return VMM_OK;
}

/**
 * @brief 获取系统支持的最大Guest的数量
 * @return 最大Guest数量
 */
uint32_t vmm_manager_max_guest_count(void)
{
    return CONFIG_MAX_GUEST_COUNT;
}

/**
 * @brief 获取当前系统中存在的Guest的数量
 * @return 当前Guest数量
 */
uint32_t vmm_manager_guest_count(void)
{
    uint32_t ret;

    vmm_manager_lock();
    ret = m_vmm_manager.guest_count;
    vmm_manager_unlock();

    return ret;
}

struct vmm_guest *vmm_manager_guest(uint32_t guest_id)
{
    struct vmm_guest *ret = NULL; /**< NULL成员 */

    if (guest_id < CONFIG_MAX_GUEST_COUNT) {
        vmm_manager_lock();

        if (!m_vmm_manager.guest_avail_array[guest_id]) {
            ret = &m_vmm_manager.guest_array[guest_id]; /**< 客户机数组 */
        }

        vmm_manager_unlock();
    }

    return ret; /**< 返回值 */
}

struct vmm_guest *vmm_manager_guest_find(const char *guest_name)
{
    uint32_t          g; /**< g */
    struct vmm_guest *ret; /**< 返回值 */

    if (!guest_name) {
        return NULL; /**< NULL成员 */
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Iterate over each used VCPU instance */
    ret = NULL; /**< NULL成员 */

    for (g = 0; g < CONFIG_MAX_GUEST_COUNT; g++) {
        if (!m_vmm_manager.guest_avail_array[g]) {
            if (!strcmp(m_vmm_manager.guest_array[g].name, guest_name)) {
                ret = &m_vmm_manager.guest_array[g]; /**< 客户机数组 */
                break;
            }
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    return ret; /**< 返回值 */
}

/**
 * @brief 管理器 客户机 遍历
 * @param (*iter 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_iterate(int (*iter)(struct vmm_guest *, void *), void *private)
{
    int rc;
    int g;
    struct vmm_guest *guest;

    /* If no iteration callback then return */
    if (!iter) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Iterate over each used VCPU instance */
    rc = VMM_OK;

    for (g = 0; g < CONFIG_MAX_GUEST_COUNT; g++) {
        if (m_vmm_manager.guest_avail_array[g]) {
            continue;
        }

        guest = &m_vmm_manager.guest_array[g];

        rc    = iter(guest, private);

        if (rc) {
            break;
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    return rc;
}

/**
 * @brief 获取指定Guest拥有的VCPU的数量
 * @param guest 指向Guest结构的指针
 * @return Guest拥有的VCPU数量
 */
uint32_t vmm_manager_guest_vcpu_count(struct vmm_guest *guest)
{
    if (!guest) {
        return 0;
    }

    return guest->vcpu_count;
}

/**
 * @brief 管理器 客户机 虚拟CPU
 * @param guest 指向客户机结构体的指针
 * @param subid 标识符
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_manager_guest_vcpu(struct vmm_guest *guest, uint32_t subid)
{
    bool        found = FALSE;
    irq_flags_t flags;
    vmm_vcpu_t *vcpu = NULL;

    if (!guest) {
        return NULL;
    }

    vmm_read_lock_irq_save_lite(&guest->vcpu_lock, flags);

    list_for_each_entry(vcpu, &guest->vcpu_list, head)
    {
        if (vcpu->subid == subid) {
            found = TRUE;
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    if (!found) {
        vcpu = NULL;
    }

    return vcpu;
}

/**
 * @brief 管理器 客户机 下一个 虚拟CPU
 * @param guest 指向客户机结构体的指针
 * @param current 指向VCPU结构体的指针
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_manager_guest_next_vcpu(const struct vmm_guest *guest, vmm_vcpu_t *current)
{
    irq_flags_t       flags;
    vmm_vcpu_t       *ret = NULL;
    struct vmm_guest *g   = (struct vmm_guest *)guest;

    if (!g) {
        return NULL; /**< NULL成员 */
    }

    vmm_read_lock_irq_save_lite(&g->vcpu_lock, flags);

    if (!current) {
        if (!list_empty(&g->vcpu_list)) {
            ret = list_first_entry(&g->vcpu_list, vmm_vcpu_t, head);
        }
    } else if (!list_is_last(&current->head, &g->vcpu_list)) {
        ret = list_first_entry(&current->head, vmm_vcpu_t, head);
    }

    vmm_read_unlock_irq_restore_lite(&g->vcpu_lock, flags);

    return ret;
}

/**
 * @brief 管理器 客户机 虚拟CPU 遍历
 * @param guest 指向客户机结构体的指针
 * @param (*iter 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_vcpu_iterate(struct vmm_guest *guest, int (*iter)(vmm_vcpu_t *, void *), void *private)
{
    int         rc = VMM_OK;
    irq_flags_t flags;
    vmm_vcpu_t *vcpu;

    if (!guest || !iter) {
        return VMM_ERR_FAIL;
    }

    vmm_read_lock_irq_save_lite(&guest->vcpu_lock, flags);

    list_for_each_entry(vcpu, &guest->vcpu_list, head)
    {
        rc = iter(vcpu, private);

        if (rc) {
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    return rc;
}

/**
 * @brief 迭代复位指定客户机的所有VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @param private 私有数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int manager_guest_reset_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_reset(vcpu);
}

/**
 * @brief 复位指定Guest及其所有VCPU
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_reset(struct vmm_guest *guest)
{
    int rc;

    if (!guest) {
        return VMM_ERR_FAIL;
    }

    guest->reset_count++;
    guest->reset_timestamp = vmm_timer_timestamp();

    rc                     = vmm_manager_guest_vcpu_iterate(guest, manager_guest_reset_iter, NULL);

    if (rc) {
        return rc;
    }

    if (!(rc = arch_guest_init(guest))) {
        rc = vmm_guest_address_space_reset(guest);
    }

    return rc;
}

/**
 * @brief 获取指定Guest最后复位的时间戳
 * @param guest 指向Guest结构的指针
 * @return 复位时间戳
 */
uint64_t vmm_manager_guest_reset_timestamp(struct vmm_guest *guest)
{
    return (guest) ? guest->reset_timestamp : 0;
}

/**
 * @brief 迭代唤醒指定客户机的所有VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @param private 私有数据指针
 * @return 时间值（纳秒）
 */
static int manager_guest_kick_iter(vmm_vcpu_t *vcpu, void *private)
{
    /* Do not kick VCPU with poweroff flag set
     * when Guest is kicked.
     */
    if (vcpu->is_poweroff) {
        return VMM_OK;
    }

    return vmm_manager_vcpu_kick(vcpu);
}

/**
 * @brief 将指定Guest从复位状态踢出
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_kick(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_kick_iter, NULL);
}

/**
 * @brief 迭代暂停指定客户机的所有VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @param private 私有数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int manager_guest_pause_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_pause(vcpu);
}

/**
 * @brief 暂停指定Guest及其所有VCPU
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_pause(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_pause_iter, NULL);
}

/**
 * @brief 迭代恢复指定客户机的所有VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @param private 私有数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int manager_guest_resume_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_resume(vcpu);
}

/**
 * @brief 恢复指定Guest及其所有VCPU
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_resume(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_resume_iter, NULL);
}

/**
 * @brief 迭代停止指定客户机的所有VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @param private 私有数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int manager_guest_halt_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_halt(vcpu);
}

/**
 * @brief 停止指定Guest及其所有VCPU
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_halt(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_halt_iter, NULL);
}

/**
 * @brief 检查管理器是否有待处理的操作请求
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static bool manager_have_req(struct vmm_guest *guest)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);

    if (!list_empty(&guest->operation_request_list)) {
        ret = TRUE;
    }

    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);

    return ret;
}

/**
 * @brief 将请求加入管理器队列
 * @param guest 指向客户机结构体的指针
 * @param req 指向客户机结构体的指针
 */
static void manager_enqueue_request(struct vmm_guest *guest, struct vmm_guest_request *req)
{
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);
    list_add_tail(&req->head, &guest->operation_request_list);
    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);

    vmm_workqueue_schedule_work(NULL, &m_vmm_manager.guest_work_array[guest->id]);
}

/**
 * @brief 从管理器操作请求队列中取出请求
 * @param guest 指向客户机结构体的指针
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_guest_request *manager_dequeue_req(struct vmm_guest *guest)
{
    irq_flags_t               flags;
    struct vmm_guest_request *req = NULL;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);

    if (!list_empty(&guest->operation_request_list)) {
        req = list_entry(list_pop(&guest->operation_request_list), struct vmm_guest_request, head); /**< head)成员 */
    }

    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);

    return req;
}

/**
 * @brief 刷新管理器操作请求队列
 * @param guest 指向客户机结构体的指针
 */
static void manager_flush_req(struct vmm_guest *guest)
{
    irq_flags_t               flags;
    struct vmm_guest_request *req;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);

    while (!list_empty(&guest->operation_request_list)) {
        req = list_entry(list_pop(&guest->operation_request_list), struct vmm_guest_request, head); /**< head)成员 */
        vmm_free(req);
    }

    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);
}

/**
 * @brief 管理器请求的工作项处理函数
 * @param work 指向工作项结构体的指针
 */
static void manager_req_work(vmm_work_t *work)
{
    uint32_t                  id;
    void *start = NULL;
    void *end = NULL;
    void *ptr = NULL;
    struct vmm_guest         *guest;
    struct vmm_guest_request *req;

    /* Determine guest pointer from work pointer */
    ptr   = work;
    start = &m_vmm_manager.guest_work_array[0];
    end   = &m_vmm_manager.guest_work_array[CONFIG_MAX_GUEST_COUNT - 1];

    if (ptr < start || end <= ptr) {
        return;
    }

    id    = ptr - start;
    id    = id / sizeof(*work);
    guest = vmm_manager_guest(id);

    if (!guest) {
        return;
    }

    /* Process one request if available */
    if ((req = manager_dequeue_req(guest))) {
        req->func(guest, req->data);
        vmm_free(req);

        /* Reschedule work if we more request */
        if (manager_have_req(guest)) {
            vmm_workqueue_schedule_work(NULL, &m_vmm_manager.guest_work_array[guest->id]);
        }
    }
}

/**
 * @brief
 *  请求指定Guest执行操作
 * @param guest 指向Guest结构的指针
 * @param req_func 请求处理函数
 * @param req_data 请求数据
 * @return 如果成功返回VMM_OK，否则返回错误码
 */ 
int vmm_manager_guest_operation_request(struct vmm_guest *guest, void (*req_func)(struct vmm_guest *, void *), void *req_data)
{
    struct vmm_guest_request *req;

    if (!guest || !req_func) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    req = vmm_zalloc(sizeof(*req));

    if (!req) {
        return VMM_ERR_NOMEM;
    }

    INIT_LIST_HEAD(&req->head);
    req->func = req_func;
    req->data = req_data;

    manager_enqueue_request(guest, req);

    return VMM_OK;
}

/**
 * @brief 请求重启指定Guest
 * @param guest 指向Guest结构的指针
 * @param data 请求数据
 */
static void manager_reboot_request(struct vmm_guest *guest, void *data)
{
    vmm_manager_guest_reset(guest);
    vmm_manager_guest_kick(guest);
}

/**
 * @brief 请求重启指定Guest
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_reboot_request(struct vmm_guest *guest)
{
    vmm_vcpu_t *cvcpu;

    if (!guest) {
        return VMM_ERR_INVALID;
    }

    /* If current VCPU belongs to the Guest then
     * pause the VCPU so that we don't return back
     * to the VCPU after submitting request.
     */
    cvcpu = vmm_scheduler_current_vcpu();

    if (cvcpu && (cvcpu->guest == guest) && vmm_scheduler_normal_context()) {
        vmm_manager_vcpu_pause(cvcpu);
    }

    return vmm_manager_guest_operation_request(guest, manager_reboot_request, NULL);
}

/**
 * @brief 管理器 关机 请求
 * @param guest 指向客户机结构体的指针
 * @param data 用户自定义数据指针
 */
static void manager_shutdown_request(struct vmm_guest *guest, void *data)
{
    vmm_manager_guest_reset(guest);
}

/**
 * @brief 请求关闭指定Guest
 * @param guest 指向Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_shutdown_request(struct vmm_guest *guest)
{
    vmm_vcpu_t *cvcpu;

    if (!guest) {
        return VMM_ERR_INVALID;
    }

    /* If current VCPU belongs to the Guest then
     * pause the VCPU so that we don't return back
     * to the VCPU after submitting request.
     */
    cvcpu = vmm_scheduler_current_vcpu();

    if (cvcpu && (cvcpu->guest == guest) && vmm_scheduler_normal_context()) {
        vmm_manager_vcpu_pause(cvcpu);
    }

    return vmm_manager_guest_operation_request(guest, manager_shutdown_request, NULL);
}

struct vmm_guest *vmm_manager_guest_create(vmm_device_tree_node_t *gnode)
{
    uint32_t                val, vnum, gnum; /**< gnum成员 */
    const char             *str; /**< str成员 */
    irq_flags_t             flags; /**< 标志位 */
    vmm_device_tree_node_t *vsnode; /**< vsnode成员 */
    vmm_device_tree_node_t *vnode; /**< vnode成员 */
    struct vmm_guest       *guest = NULL; /**< NULL成员 */
    vmm_vcpu_t             *vcpu  = NULL; /**< NULL成员 */

    /* Sanity checks */
    if (!gnode) {
        return NULL; /**< NULL成员 */
    }

    if (vmm_device_tree_read_string(gnode, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, &str)) {
        return NULL; /**< NULL成员 */
    }

    if (strcmp(str, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_GUEST) != 0) {
        return NULL; /**< NULL成员 */
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Ensure guest node uniqueness */
    list_for_each_entry(guest, &m_vmm_manager.guest_list, head)
    {
        if ((guest->node == gnode) || (strcmp(guest->name, gnode->name) == 0)) {
            vmm_manager_unlock();
            vmm_printf("%s: Duplicate Guest %s detected\n", __func__, gnode->name); /**< gnode->name)成员 */
            return NULL; /**< NULL成员 */
        }
    }

    /* Find next available guest instance */
    for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
        if (m_vmm_manager.guest_avail_array[gnum]) {
            guest                             = &m_vmm_manager.guest_array[gnum]; /**< 客户机数组 */
            m_vmm_manager.guest_avail_array[guest->id] = FALSE; /**< 客户机可用标志数组 */
            break;
        }
    }

    if (!guest) {
        vmm_manager_unlock();
        vmm_printf("%s: No available Guest instance found\n", __func__); /**< __func__)成员 */
        return NULL; /**< NULL成员 */
    }

    /* Add guest instance to guest list */
    list_add_tail(&guest->head, &m_vmm_manager.guest_list); /**< &m_vmm_manager.guest_list)成员 */

    /* Increment guest count */
    m_vmm_manager.guest_count++;

    /* Initialize guest instance */
    strlcpy(guest->name, gnode->name, sizeof(guest->name)); /**< sizeof(guest->name))成员 */
    vmm_device_tree_ref_node(gnode);
    guest->node = gnode; /**< gnode成员 */
#ifdef CONFIG_CPU_BE
    guest->is_big_endian = TRUE; /**< TRUE成员 */
#else
    guest->is_big_endian = FALSE; /**< FALSE成员 */
#endif
    guest->reset_count     = 0; /**< 0 */
    guest->reset_timestamp = vmm_timer_timestamp(); /**< vmm_timer_timestamp()成员 */
    INIT_SPIN_LOCK(&guest->request_lock);
    INIT_LIST_HEAD(&guest->operation_request_list);
    INIT_RW_LOCK(&guest->vcpu_lock);
    guest->vcpu_count = 0; /**< 0 */
    INIT_LIST_HEAD(&guest->vcpu_list);
    memset(&guest->addr_space, 0, sizeof(guest->addr_space)); /**< sizeof(guest->addr_space))成员 */
    guest->addr_space.initialized = FALSE; /**< FALSE成员 */
    INIT_RW_LOCK(&guest->addr_space.reg_iotree_lock);
    INIT_LIST_HEAD(&guest->addr_space.reg_ioprobe_list);
    guest->addr_space.reg_iotree = RB_ROOT; /**< RB_ROOT成员 */
    INIT_RW_LOCK(&guest->addr_space.reg_memory_tree_lock);
    INIT_LIST_HEAD(&guest->addr_space.reg_memprobe_list);
    guest->addr_space.reg_memtree = RB_ROOT; /**< RB_ROOT成员 */
    guest->arch_private       = NULL; /**< NULL成员 */

    /* Determine guest endianness from guest node */
    if (vmm_device_tree_read_string(gnode, VMM_DEVICE_TREE_ENDIANNESS_ATTR_NAME, &str) == VMM_OK) {
        if (!strcmp(str, VMM_DEVICE_TREE_ENDIANNESS_VAL_LITTLE)) {
            guest->is_big_endian = FALSE; /**< FALSE成员 */
        } else if (!strcmp(str, VMM_DEVICE_TREE_ENDIANNESS_VAL_BIG)) {
            guest->is_big_endian = TRUE; /**< TRUE成员 */
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    vsnode = vmm_device_tree_getchild(gnode, VMM_DEVICE_TREE_VCPUS_NODE_NAME); /**< VMM_DEVICE_TREE_VCPUS_NODE_NAME)成员 */

    if (!vsnode) {
        vmm_printf("%s: vcpus node not found for Guest %s\n", __func__, gnode->name); /**< gnode->name)成员 */
        goto fail_destroy_guest; /**< fail_destroy_guest成员 */
    }

    vmm_device_tree_for_each_child(vnode, vsnode)
    {
        int           index; /**< 索引 */
        uint32_t      cpu; /**< CPU编号 */
        vmm_cpumask_t mask; /**< 掩码 */

        /* Sanity checks */
        if (CONFIG_MAX_VCPU_COUNT <= m_vmm_manager.vcpu_count) {
            vmm_printf(
                "%s: No more free VCPUs\n"
                "for Guest %s VCPU %s\n", /**< %s\n"成员 */
                __func__, gnode->name, vnode->name); /**< vnode->name)成员 */
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        if (vmm_device_tree_read_string(vnode, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, &str)) {
            vmm_printf(
                "%s: No device_type attribute\n"
                "for Guest %s VCPU %s\n", /**< %s\n"成员 */
                __func__, gnode->name, vnode->name); /**< vnode->name)成员 */
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        if (strcmp(str, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_VCPU) != 0) {
            vmm_printf(
                "%s: Invalid device_type attribute\n"
                "for Guest %s VCPU %s\n", /**< %s\n"成员 */
                __func__, gnode->name, vnode->name); /**< vnode->name)成员 */
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        /* Setup VCPU affinity mask */
        if (vmm_device_tree_getattr(vnode, VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME)) {
            /* Start with empty affinity mask */
            mask  = VMM_CPU_MASK_NONE; /**< VMM_CPU_MASK_NONE成员 */

            /* Set all assigned CPU in the mask */
            index = 0; /**< 0 */

            while (vmm_device_tree_read_u32_atindex(vnode, VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME, &cpu, index) == VMM_OK) {
                if ((cpu < CONFIG_CPU_COUNT) && vmm_cpu_online(cpu)) {
                    vmm_cpumask_set_cpu(cpu, &mask); /**< &mask)成员 */
                } else {
                    vmm_printf(
                        "%s: CPU%d is out of bound"
                        " (%d <) or not online for"
                        " Guest %s VCPU %s\n", /**< %s\n"成员 */
                        __func__, cpu, CONFIG_CPU_COUNT, gnode->name, vnode->name); /**< vnode->name)成员 */
                    vmm_device_tree_dref_node(vnode);
                    goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
                }

                index++;
            }

            /* If affinity mask turns-out to be empty then fail */
            if (vmm_cpumask_weight(&mask) < 1) {
                vmm_printf(
                    "%s: Empty affinity mask\n"
                    "for Guest %s VCPU %s\n", /**< %s\n"成员 */
                    __func__, gnode->name, vnode->name); /**< vnode->name)成员 */
                vmm_device_tree_dref_node(vnode);
                goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
            }
        } else {
            memcpy(&mask, cpu_online_mask, sizeof(mask)); /**< sizeof(mask))成员 */
        }

        /* Acquire manager lock */
        vmm_manager_lock();

        /* Find next available VCPU instance */
        vcpu = NULL; /**< NULL成员 */

        for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
            if (!m_vmm_manager.vcpu_avail_array[vnum]) {
                continue;
            }

            vcpu = &m_vmm_manager.vcpu_array[vnum]; /**< VCPU数组 */

            /* Update priority */
            if (vmm_device_tree_read_u32(vnode, VMM_DEVICE_TREE_PRIORITY_ATTR_NAME, &val)) {
                vcpu->priority = VMM_VCPU_DEF_PRIORITY; /**< VMM_VCPU_DEF_PRIORITY成员 */
            } else {
                vcpu->priority = val; /**< 值 */
            }

            if (VMM_VCPU_MAX_PRIORITY < vcpu->priority) {
                vcpu->priority = VMM_VCPU_MAX_PRIORITY; /**< VMM_VCPU_MAX_PRIORITY成员 */
            }

            if (vcpu->priority < VMM_VCPU_MIN_PRIORITY) {
                vcpu->priority = VMM_VCPU_MIN_PRIORITY; /**< VMM_VCPU_MIN_PRIORITY成员 */
            }

            /* Update host CPU and affinity */
            memcpy(&m_vmm_manager.vcpu_affinity_mask[vcpu->id], &mask, sizeof(mask)); /**< VCPU亲和性掩码 */
            vcpu->host_cpu                  = __vmm_manager_good_hcpu(vcpu->priority, &mask); /**< &mask)成员 */
            vcpu->cpu_affinity              = &m_vmm_manager.vcpu_affinity_mask[vcpu->id]; /**< VCPU亲和性掩码 */

            m_vmm_manager.vcpu_avail_array[vcpu->id] = FALSE; /**< VCPU可用标志数组 */
            break;
        }

        if (!vcpu) {
            vmm_printf(
                "%s: No available VCPU instance found \n"
                "for Guest %s VCPU %s\n", /**< %s\n"成员 */
                __func__, gnode->name, vnode->name); /**< vnode->name)成员 */
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        /* Update general info and state */
        vcpu->subid = guest->vcpu_count; /**< guest->vcpu_count成员 */
        strlcpy(vcpu->name, gnode->name, sizeof(vcpu->name)); /**< sizeof(vcpu->name))成员 */
        strlcat(vcpu->name, VMM_DEVICE_TREE_PATH_SEPARATOR_STRING, sizeof(vcpu->name)); /**< sizeof(vcpu->name))成员 */

        if (strlcat(vcpu->name, vnode->name, sizeof(vcpu->name)) >= sizeof(vcpu->name)) {
            vmm_printf(
                "%s: name concatination failed "
                "for Guest %s VCPU %s\n", /**< %s\n"成员 */
                __func__, gnode->name, vnode->name); /**< vnode->name)成员 */
            m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE; /**< VCPU可用标志数组 */
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        vmm_device_tree_ref_node(vnode);
        vcpu->node        = vnode; /**< vnode成员 */
        vcpu->is_normal   = TRUE; /**< TRUE成员 */
        vcpu->is_poweroff = FALSE; /**< FALSE成员 */
        vcpu->guest       = guest; /**< 客户机 */
        arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN); /**< VMM_VCPU_STATE_UNKNOWN)成员 */

        /* Increment VCPU count */
        m_vmm_manager.vcpu_count++;

        /* Release manager lock */
        vmm_manager_unlock();

        /* Setup start program counter and stack */
        vmm_device_tree_read_virtaddr(vnode, VMM_DEVICE_TREE_START_PC_ATTR_NAME, &vcpu->start_pc); /**< &vcpu->start_pc)成员 */
        vcpu->stack_virtual_address = (virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE); /**< (virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE)成员 */

        if (!vcpu->stack_virtual_address) {
            vmm_printf(
                "%s: stack alloc failed "
                "for VCPU %s\n", /**< %s\n"成员 */
                __func__, vcpu->name); /**< vcpu->name)成员 */
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL; /**< NULL成员 */
            vmm_manager_lock();
            m_vmm_manager.vcpu_count--;
            m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE; /**< VCPU可用标志数组 */
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        vcpu->stack_size = CONFIG_IRQ_STACK_SIZE; /**< CONFIG_IRQ_STACK_SIZE成员 */

        /* Initialize dynamic scheduling context */
        INIT_RW_LOCK(&vcpu->sched_lock);
        vcpu->state_tstamp        = vmm_timer_timestamp(); /**< vmm_timer_timestamp()成员 */
        vcpu->state_ready_nsecs   = 0; /**< 0 */
        vcpu->state_running_nsecs = 0; /**< 0 */
        vcpu->state_paused_nsecs  = 0; /**< 0 */
        vcpu->state_halted_nsecs  = 0; /**< 0 */
        vcpu->system_nsecs        = 0; /**< 0 */
        vcpu->reset_count         = 0; /**< 0 */
        vcpu->reset_timestamp     = 0; /**< 0 */
        vcpu->preempt_count       = 0; /**< 0 */
        vcpu->resumed             = FALSE; /**< FALSE成员 */
        vcpu->sched_private       = NULL; /**< NULL成员 */

        /* Initialize static scheduling context */
        if (vmm_device_tree_read_u64(vnode, VMM_DEVICE_TREE_TIME_SLICE_ATTR_NAME, &vcpu->time_slice)) {
            vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE; /**< VMM_VCPU_DEF_TIME_SLICE成员 */
        }

        if (vcpu->time_slice == 0) {
            vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE; /**< VMM_VCPU_DEF_TIME_SLICE成员 */
        }

        if (vmm_device_tree_read_u64(vnode, VMM_DEVICE_TREE_DEADLINE_ATTR_NAME, &vcpu->deadline)) {
            vcpu->deadline = VMM_VCPU_DEF_DEADLINE; /**< VMM_VCPU_DEF_DEADLINE成员 */
        }

        if (vcpu->deadline < vcpu->time_slice) {
            vcpu->deadline = vcpu->time_slice; /**< vcpu->time_slice成员 */
        }

        if (vmm_device_tree_read_u64(vnode, VMM_DEVICE_TREE_PERIODICITY_ATTR_NAME, &vcpu->periodicity)) {
            vcpu->periodicity = VMM_VCPU_DEF_PERIODICITY; /**< VMM_VCPU_DEF_PERIODICITY成员 */
        }

        if (vcpu->periodicity < vcpu->deadline) {
            vcpu->periodicity = vcpu->deadline; /**< vcpu->deadline成员 */
        }

        /* Initialize architecture specific context */
        vcpu->arch_private = NULL; /**< NULL成员 */

        if (arch_vcpu_init(vcpu)) {
            vmm_free((void *)vcpu->stack_virtual_address); /**< )vcpu->stack_virtual_address)成员 */
            vmm_printf(
                "%s: arch_vcpu_init() failed "
                "for VCPU %s\n", /**< %s\n"成员 */
                __func__, vcpu->name); /**< vcpu->name)成员 */
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL; /**< NULL成员 */
            vmm_manager_lock();
            m_vmm_manager.vcpu_count--;
            m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE; /**< VCPU可用标志数组 */
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        /* Initialize virtual IRQ context */
        if (vmm_vcpu_irq_init(vcpu)) {
            arch_vcpu_deinit(vcpu);
            vmm_free((void *)vcpu->stack_virtual_address); /**< )vcpu->stack_virtual_address)成员 */
            vmm_printf(
                "%s: vmm_vcpu_irq_init() failed "
                "for VCPU %s\n", /**< %s\n"成员 */
                __func__, vcpu->name); /**< vcpu->name)成员 */
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL; /**< NULL成员 */
            vmm_manager_lock();
            m_vmm_manager.vcpu_count--;
            m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE; /**< VCPU可用标志数组 */
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        /* Initialize resource list */
        INIT_SPIN_LOCK(&vcpu->res_lock);
        INIT_LIST_HEAD(&vcpu->res_head);

        /* Initialize waitqueue context and cleanup callback */
        INIT_LIST_HEAD(&vcpu->wait_queue_head);
        vcpu->wait_queue_lock    = NULL; /**< NULL成员 */
        vcpu->wait_queue_private = NULL; /**< NULL成员 */
        vcpu->wait_queue_cleanup = NULL; /**< NULL成员 */

        /* Notify scheduler about new VCPU */
        if (vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET)) {
            vmm_vcpu_irq_deinit(vcpu);
            arch_vcpu_deinit(vcpu);
            vmm_free((void *)vcpu->stack_virtual_address); /**< )vcpu->stack_virtual_address)成员 */
            vmm_printf(
                "%s: Setting RESET state failed "
                "for VCPU %s\n", /**< %s\n"成员 */
                __func__, vcpu->name); /**< vcpu->name)成员 */
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL; /**< NULL成员 */
            vmm_manager_lock();
            m_vmm_manager.vcpu_count--;
            m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE; /**< VCPU可用标志数组 */
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode; /**< fail_dref_vsnode成员 */
        }

        /* Get poweroff flag from device tree */
        if (vmm_device_tree_getattr(vnode, VMM_DEVICE_TREE_VCPU_POWEROFF_ATTR_NAME)) {
            vcpu->is_poweroff = TRUE; /**< TRUE成员 */
        }

        /* Add VCPU to Guest child list */
        vmm_write_lock_irq_save_lite(&guest->vcpu_lock, flags); /**< flags)成员 */
        list_add_tail(&vcpu->head, &guest->vcpu_list); /**< &guest->vcpu_list)成员 */
        guest->vcpu_count++;
        vmm_write_unlock_irq_restore_lite(&guest->vcpu_lock, flags); /**< flags)成员 */
    }

    /* Release vcpus node */
    vmm_device_tree_dref_node(vsnode);

    /* Fail if no VCPU is associated to the guest */
    vmm_read_lock_irq_save_lite(&guest->vcpu_lock, flags); /**< flags)成员 */

    if (list_empty(&guest->vcpu_list)) {
        vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags); /**< flags)成员 */
        goto fail_destroy_guest; /**< fail_destroy_guest成员 */
    }

    vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags); /**< flags)成员 */

    /* Initialize arch guest context */
    if (arch_guest_init(guest)) {
        goto fail_destroy_guest; /**< fail_destroy_guest成员 */
    }

    /* Initialize guest address space */
    if (vmm_guest_address_space_init(guest)) {
        goto fail_destroy_guest; /**< fail_destroy_guest成员 */
    }

    /* Reset guest address space */
    if (vmm_guest_address_space_reset(guest)) {
        goto fail_destroy_guest; /**< fail_destroy_guest成员 */
    }

    return guest; /**< 客户机 */

fail_dref_vsnode:
    vmm_device_tree_dref_node(vsnode);
fail_destroy_guest:
    vmm_manager_guest_destroy(guest);
    return NULL; /**< NULL成员 */
}

/**
 * @brief 销毁指定Guest及其所有资源
 * @param guest 指向要销毁的Guest结构的指针
 * @return 如果成功返回VMM_OK，否则返回错误码
 */
int vmm_manager_guest_destroy(struct vmm_guest *guest)
{
    int         rc;
    irq_flags_t flags;
    vmm_vcpu_t *vcpu;

    /* Sanity Check */
    if (!guest) {
        return VMM_ERR_FAIL;
    }

    /* For sanity reset guest (ignore reture value) */
    vmm_manager_guest_reset(guest);

    /* Flush all request for this guest */
    manager_flush_req(guest);

    /* Deinit the guest addr_space */
    if ((rc = vmm_guest_address_space_deinit(guest))) {
        return rc;
    }

    /* Deinit arch guest context */
    if ((rc = arch_guest_deinit(guest))) {
        return rc;
    }

    /* Acquire Guest VCPU lock */
    vmm_write_lock_irq_save_lite(&guest->vcpu_lock, flags);

    /* Destroy each VCPU of guest */
    while (!list_empty(&guest->vcpu_list)) {
        vcpu = list_first_entry(&guest->vcpu_list, vmm_vcpu_t, head);

        /* Remove from guest->vcpu_list */
        guest->vcpu_count--;
        list_del(&vcpu->head);

        /* Release Guest VCPU lock */
        vmm_write_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

        /* Flush all resources acquired by this VCPU */
        vmm_manager_vcpu_resource_flush(vcpu);

        /* Set VCPU state to unknown
         * (This will clean scheduling context)
         */
        if ((rc = vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_UNKNOWN))) {
            return rc;
        }

        vcpu->sched_private = NULL;

        /* Deinit Virtual IRQ context */
        if ((rc = vmm_vcpu_irq_deinit(vcpu))) {
            return rc;
        }

        /* Deinit architecture specific context */
        if ((rc = arch_vcpu_deinit(vcpu))) {
            return rc;
        }

        /* Free stack pages */
        if (vcpu->stack_virtual_address) {
            vmm_free((void *)vcpu->stack_virtual_address);
        }

        /* De-reference VCPU node */
        vmm_device_tree_dref_node(vcpu->node);
        vcpu->node = NULL;

        /* Acquire manager lock */
        vmm_manager_lock();

        /* Decrement vcpu count */
        m_vmm_manager.vcpu_count--;

        /* Mark this VCPU as available */
        m_vmm_manager.vcpu_avail_array[vcpu->id] = TRUE;

        /* Release manager lock */
        vmm_manager_unlock();

        /* Acquire Guest VCPU lock */
        vmm_write_lock_irq_save_lite(&guest->vcpu_lock, flags);
    }

    /* Release Guest VCPU lock */
    vmm_write_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Reset guest instance members */
    vmm_device_tree_dref_node(guest->node);
    guest->node    = NULL;
    guest->name[0] = '\0';
    INIT_LIST_HEAD(&guest->vcpu_list);

    /* Decrement guest count */
    m_vmm_manager.guest_count--;

    /* Remove from guest list */
    list_del(&guest->head);
    INIT_LIST_HEAD(&guest->head);

    /* Mark this guest instance as available */
    m_vmm_manager.guest_avail_array[guest->id] = TRUE;

    /* Release manager lock */
    vmm_manager_unlock();

    return VMM_OK;
}

/**
 * @brief 初始化管理器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_manager_init(void)
{
    uint32_t vnum;
    uint32_t gnum;

    /* Reset the manager control structure */
    memset(&m_vmm_manager, 0, sizeof(m_vmm_manager));

    /* Intialize guest & vcpu management parameters */
    INIT_MUTEX(&m_vmm_manager.lock);
    m_vmm_manager.vcpu_count         = 0;
    m_vmm_manager.guest_count        = 0;
    m_vmm_manager.vcpu_affinity_mask = NULL;
    m_vmm_manager.vcpu_array         = NULL;
    m_vmm_manager.vcpu_avail_array   = NULL;
    m_vmm_manager.guest_array        = NULL;
    m_vmm_manager.guest_avail_array  = NULL;
    INIT_LIST_HEAD(&m_vmm_manager.orphan_vcpu_list);
    INIT_LIST_HEAD(&m_vmm_manager.guest_list);
    m_vmm_manager.guest_work_array   = NULL;

    /* Alloc memory for guest & vcpu management */
    m_vmm_manager.vcpu_affinity_mask = vmm_zalloc(CONFIG_MAX_VCPU_COUNT * sizeof(*m_vmm_manager.vcpu_affinity_mask));

    if (!m_vmm_manager.vcpu_affinity_mask) {
        return VMM_ERR_NOMEM;
    }

    m_vmm_manager.vcpu_array = vmm_zalloc(CONFIG_MAX_VCPU_COUNT * sizeof(*m_vmm_manager.vcpu_array));

    if (!m_vmm_manager.vcpu_array) {
        vmm_free(m_vmm_manager.vcpu_affinity_mask);
        return VMM_ERR_NOMEM;
    }

    m_vmm_manager.vcpu_avail_array = vmm_zalloc(CONFIG_MAX_VCPU_COUNT * sizeof(*m_vmm_manager.vcpu_avail_array));

    if (!m_vmm_manager.vcpu_avail_array) {
        vmm_free(m_vmm_manager.vcpu_array);
        vmm_free(m_vmm_manager.vcpu_affinity_mask);
        return VMM_ERR_NOMEM;
    }

    m_vmm_manager.guest_array = vmm_zalloc(CONFIG_MAX_GUEST_COUNT * sizeof(*m_vmm_manager.guest_array));

    if (!m_vmm_manager.guest_array) {
        vmm_free(m_vmm_manager.vcpu_avail_array);
        vmm_free(m_vmm_manager.vcpu_array);
        vmm_free(m_vmm_manager.vcpu_affinity_mask);
        return VMM_ERR_NOMEM;
    }

    m_vmm_manager.guest_avail_array = vmm_zalloc(CONFIG_MAX_GUEST_COUNT * sizeof(*m_vmm_manager.guest_avail_array));

    if (!m_vmm_manager.guest_avail_array) {
        vmm_free(m_vmm_manager.guest_array);
        vmm_free(m_vmm_manager.vcpu_avail_array);
        vmm_free(m_vmm_manager.vcpu_array);
        vmm_free(m_vmm_manager.vcpu_affinity_mask);
        return VMM_ERR_NOMEM;
    }

    m_vmm_manager.guest_work_array = vmm_zalloc(CONFIG_MAX_GUEST_COUNT * sizeof(*m_vmm_manager.guest_work_array));

    if (!m_vmm_manager.guest_work_array) {
        vmm_free(m_vmm_manager.guest_avail_array);
        vmm_free(m_vmm_manager.guest_array);
        vmm_free(m_vmm_manager.vcpu_avail_array);
        vmm_free(m_vmm_manager.vcpu_array);
        vmm_free(m_vmm_manager.vcpu_affinity_mask);
        return VMM_ERR_NOMEM;
    }

    /* Initialze memory for guest instances */
    for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
        INIT_LIST_HEAD(&m_vmm_manager.guest_array[gnum].head);
        m_vmm_manager.guest_array[gnum].id   = gnum;
        m_vmm_manager.guest_array[gnum].node = NULL;
        INIT_RW_LOCK(&m_vmm_manager.guest_array[gnum].vcpu_lock);
        m_vmm_manager.guest_array[gnum].vcpu_count = 0;
        INIT_LIST_HEAD(&m_vmm_manager.guest_array[gnum].vcpu_list);
        m_vmm_manager.guest_avail_array[gnum] = TRUE;
        INIT_WORK(&m_vmm_manager.guest_work_array[gnum], manager_req_work);
    }

    /* Initialze memory for vcpu instances */
    for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
        INIT_LIST_HEAD(&m_vmm_manager.vcpu_array[vnum].head);
        m_vmm_manager.vcpu_array[vnum].id        = vnum;
        m_vmm_manager.vcpu_array[vnum].name[0]   = 0;
        m_vmm_manager.vcpu_array[vnum].node      = NULL;
        m_vmm_manager.vcpu_array[vnum].is_normal = FALSE;
        arch_atomic_write(&m_vmm_manager.vcpu_array[vnum].state, VMM_VCPU_STATE_UNKNOWN);
        m_vmm_manager.vcpu_array[vnum].state_tstamp        = 0;
        m_vmm_manager.vcpu_array[vnum].state_ready_nsecs   = 0;
        m_vmm_manager.vcpu_array[vnum].state_running_nsecs = 0;
        m_vmm_manager.vcpu_array[vnum].state_paused_nsecs  = 0;
        m_vmm_manager.vcpu_array[vnum].state_halted_nsecs  = 0;
        m_vmm_manager.vcpu_array[vnum].system_nsecs        = 0;
        m_vmm_manager.vcpu_array[vnum].reset_count         = 0;
        m_vmm_manager.vcpu_array[vnum].reset_timestamp     = 0;
        INIT_RW_LOCK(&m_vmm_manager.vcpu_array[vnum].sched_lock);
        m_vmm_manager.vcpu_avail_array[vnum] = TRUE;
    }

    return VMM_OK;
}
