/**
 * Copyright (c) 2013 Jean-Christophe Dubois
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
 * @file vmm_load_balancer_crude.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief 简单负载均衡算法源文件
 *
 * This is a very simple and lazy "load balancer". It will balance
 * VCPUs based on host CPU utilization. It does not consider the
 * VCPU nature (i.e. IO-bound or CPU-bound) and rather treats all
 * ready VCPUs equally when balancing. For newly created VCPUs, it
 * will try to provide host CPU with least number of READY, RUNNING,
 * and PAUSED VCPUs.
 */

#include <libs/mathlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_limits.h>
#include <vmm_load_balancer.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC      "Crude Load Balancer"
#define MODULE_AUTHOR    "Jean-Christophe Dubois"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      crude_init
#define MODULE_EXIT      crude_exit

/**
 * @brief 简单负载均衡控制结构，跟踪各主机CPU上VCPU的存活/活跃计数和空闲时间
 */
struct crude_control {
    uint32_t alive_count[CONFIG_CPU_COUNT][VMM_VCPU_MAX_PRIORITY + 1]; /**< 存活计数 */
    uint32_t active_count[CONFIG_CPU_COUNT][VMM_VCPU_MAX_PRIORITY + 1]; /**< 活跃计数 */
    uint64_t idle_ns[CONFIG_CPU_COUNT]; /**< idle_ns成员 */
    uint64_t idle_period_ns[CONFIG_CPU_COUNT]; /**< idle_period_ns成员 */
    uint32_t idle_percent[CONFIG_CPU_COUNT]; /**< idle_percent成员 */
};

/**
 * @brief VCPU迭代回调：统计各主机CPU上各优先级VCPU的存活数和活跃数
 * @param vcpu 当前遍历的VCPU指针
 * @param private crude_control结构体指针
 * @return 成功返回VMM_OK
 */
static int crude_analyze_count_iter(vmm_vcpu_t *vcpu, void *private)
{
    uint32_t host_cpu;
    uint32_t state;
    struct crude_control *crude = private;

    state                       = vmm_manager_vcpu_get_state(vcpu);

    if (state != VMM_VCPU_STATE_READY && state != VMM_VCPU_STATE_RUNNING && state != VMM_VCPU_STATE_PAUSED) {
        return VMM_OK; /**< VMM_OK成员 */
    }

    vmm_manager_vcpu_get_hcpu(vcpu, &host_cpu);

    crude->alive_count[host_cpu][vcpu->priority]++;

    if (state != VMM_VCPU_STATE_PAUSED) {
        crude->active_count[host_cpu][vcpu->priority]++;
    }

    return VMM_OK;
}

/**
 * @brief 遍历所有VCPU，统计各主机CPU上的存活和活跃VCPU计数
 * @param crude 负载均衡控制结构指针
 */
static void crude_analyze_count(struct crude_control *crude)
{
    memset(crude->alive_count, 0, sizeof(crude->alive_count));
    memset(crude->active_count, 0, sizeof(crude->active_count));

    vmm_manager_vcpu_iterate(crude_analyze_count_iter, crude);
}

/**
 * @brief 计算各主机CPU的空闲时间和空闲百分比
 * @param crude 负载均衡控制结构指针
 */
static void crude_analyze_idle(struct crude_control *crude)
{
    uint32_t host_cpu;

    memset(crude->idle_ns, 0, sizeof(crude->idle_ns));
    memset(crude->idle_period_ns, 0, sizeof(crude->idle_period_ns));
    memset(crude->idle_percent, 0, sizeof(crude->idle_percent));

    for_each_online_cpu(host_cpu)
    {
        crude->idle_ns[host_cpu]        = vmm_scheduler_idle_time(host_cpu);
        crude->idle_period_ns[host_cpu] = vmm_scheduler_get_sample_period(host_cpu);
        crude->idle_percent[host_cpu]   = udiv64(crude->idle_ns[host_cpu] * 100, crude->idle_period_ns[host_cpu]);
    }
}

/**
 * @brief 查找空闲百分比最高的主机CPU
 * @param crude 负载均衡控制结构指针
 * @return 空闲率最高的主机CPU编号
 */
static uint32_t crude_best_idle_hcpu(struct crude_control *crude)
{
    uint32_t host_cpu;
    uint32_t idle;
    uint32_t best_hcpu;
    uint32_t best_hcpu_idle;

    best_hcpu      = vmm_smp_processor_id();
    best_hcpu_idle = crude->idle_percent[best_hcpu];

    for_each_online_cpu(host_cpu)
    {
        idle = crude->idle_percent[host_cpu];

        if (idle > best_hcpu_idle) {
            best_hcpu      = host_cpu;
            best_hcpu_idle = idle;
        }
    }

    return best_hcpu;
}

/**
 * @brief 查找负载最重的主机CPU（空闲率最低且活跃VCPU数最多）
 * @param crude 负载均衡控制结构指针
 * @return 负载最重的主机CPU编号
 */
static uint32_t crude_worst_idle_hcpu(struct crude_control *crude)
{
    uint32_t p;
    uint32_t host_cpu;
    uint32_t count;
    uint32_t idle;
    uint32_t worst_hcpu;
    uint32_t worst_hcpu_count;
    uint32_t worst_hcpu_idle;

    worst_hcpu      = vmm_smp_processor_id();
    worst_hcpu_idle = crude->idle_percent[worst_hcpu];
    count           = 1;

    for (p = VMM_VCPU_MIN_PRIORITY; p <= VMM_VCPU_MAX_PRIORITY; p++) {
        count += crude->active_count[worst_hcpu][p];
    }

    worst_hcpu_count = count;

    for_each_online_cpu(host_cpu)
    {
        idle  = crude->idle_percent[host_cpu];
        count = 1;

        for (p = VMM_VCPU_MIN_PRIORITY; p <= VMM_VCPU_MAX_PRIORITY; p++) {
            count += crude->active_count[host_cpu][p];
        }

        if ((idle < worst_hcpu_idle) || ((idle == worst_hcpu_idle) && (count > worst_hcpu_count))) {
            worst_hcpu       = host_cpu;
            worst_hcpu_idle  = idle;
            worst_hcpu_count = count;
        }
    }

    return worst_hcpu;
}

/**
 * @brief 负载均衡迁移上下文，记录待迁移VCPU的优先级、状态及新旧主机CPU
 */
struct crude_balance_hcpu {
    struct crude_control *crude; /**< crude成员 */
    uint8_t               prio; /**< 优先级 */
    uint32_t              state; /**< 状态 */
    uint32_t              old_hcpu; /**< old_hcpu成员 */
    uint32_t              new_hcpu; /**< new_hcpu成员 */
    uint32_t              done; /**< 完成标志 */
};

/**
 * @brief VCPU迭代回调：将匹配条件的VCPU从过载CPU迁移到空闲CPU
 * @param vcpu 当前遍历的VCPU指针
 * @param private crude_balance_hcpu迁移上下文指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int crude_balance_hcpu_iter(vmm_vcpu_t *vcpu, void *private)
{
    int                        rc;
    uint32_t host_cpu;
    uint32_t state;
    const vmm_cpumask_t       *aff;
    struct crude_balance_hcpu *crude_bhp = private;

    if (crude_bhp->done) {
        return VMM_OK; /**< VMM_OK成员 */
    }

    if (crude_bhp->prio != vcpu->priority) {
        return VMM_OK;
    }

    vmm_manager_vcpu_get_hcpu(vcpu, &host_cpu);

    if (host_cpu != crude_bhp->old_hcpu) {
        return VMM_OK;
    }

    state = vmm_manager_vcpu_get_state(vcpu);

    if (state != crude_bhp->state) {
        return VMM_OK;
    }

    aff = vmm_manager_vcpu_get_affinity(vcpu);

    if (vmm_cpumask_weight(aff) < 2) {
        return VMM_OK;
    }

    if (!vmm_cpumask_test_cpu(crude_bhp->new_hcpu, aff)) {
        return VMM_OK;
    }

    DPRINTF("%s: vcpu=%s old_hcpu=%d new_hcpu=%d\n", __func__, vcpu->name, crude_bhp->old_hcpu, crude_bhp->new_hcpu);

    rc = vmm_manager_vcpu_set_hcpu(vcpu, crude_bhp->new_hcpu);

    if (rc) {
        return rc;
    }

    crude_bhp->done = 1;

    return VMM_OK;
}

/**
 * @brief 执行一轮负载均衡：分析负载并将VCPU从过载CPU迁移到空闲CPU
 * @param algo 负载均衡算法实例指针
 */
static void crude_balance(struct vmm_load_balancer_algo *algo)
{
    uint8_t                   prio;
    uint32_t best_hcpu;
    uint32_t best_hcpu_idle;
    uint32_t worst_hcpu;
    uint32_t worst_hcpu_idle;
    struct crude_balance_hcpu crude_bhp;
    struct crude_control     *crude = vmm_load_balancer_get_algo_private(algo);

    if (!crude) {
        return;
    }

    crude_analyze_count(crude);
    crude_analyze_idle(crude);

    best_hcpu       = crude_best_idle_hcpu(crude);
    best_hcpu_idle  = crude->idle_percent[best_hcpu];
    worst_hcpu      = crude_worst_idle_hcpu(crude);
    worst_hcpu_idle = crude->idle_percent[worst_hcpu];

    DPRINTF("%s: best_hcpu=%d best_hcpu_idle=%d\n", __func__, best_hcpu, best_hcpu_idle);
    DPRINTF("%s: worst_hcpu=%d worst_hcpu_idle=%d\n", __func__, worst_hcpu, worst_hcpu_idle);

    if ((best_hcpu == worst_hcpu) || (worst_hcpu_idle > 50) || ((best_hcpu_idle - worst_hcpu_idle) < 10)) {
        return;
    }

    crude_bhp.crude    = crude;
    crude_bhp.state    = VMM_VCPU_STATE_READY;
    crude_bhp.old_hcpu = worst_hcpu;
    crude_bhp.new_hcpu = best_hcpu;
    crude_bhp.done     = 0;

    for (prio = VMM_VCPU_MIN_PRIORITY; prio <= VMM_VCPU_MAX_PRIORITY; prio++) {
        if (!vmm_scheduler_ready_count(worst_hcpu, prio)) {
            continue;
        }

        DPRINTF("%s: balance worst_hcpu=%d best_hcpu=%d prio=%d\n", __func__, worst_hcpu, best_hcpu, prio);
        crude_bhp.prio = prio;
        vmm_manager_vcpu_iterate(crude_balance_hcpu_iter, &crude_bhp);
    }
}

/**
 * @brief 启动负载均衡算法，分配控制结构
 * @param algo 负载均衡算法实例指针
 * @return 成功返回VMM_OK，失败返回VMM_ERR_NOMEM
 */
static int crude_start(struct vmm_load_balancer_algo *algo)
{
    struct crude_control *crude;

    crude = vmm_zalloc(sizeof(*crude));

    if (!crude) {
        return VMM_ERR_NOMEM; /**< VMM_ERR_NOMEM成员 */
    }

    vmm_load_balancer_set_algo_private(algo, crude);

    return VMM_OK;
}

/**
 * @brief 停止负载均衡算法，释放控制结构
 * @param algo 负载均衡算法实例指针
 */
static void crude_stop(struct vmm_load_balancer_algo *algo)
{
    struct crude_control *crude = vmm_load_balancer_get_algo_private(algo);

    if (!crude) {
        return;
    }

    vmm_load_balancer_set_algo_private(algo, NULL);
    vmm_free(crude);
}

static struct vmm_load_balancer_algo crude = {
    .name    = "Crude Load Balancer",
    .rating  = 1,
    .balance = crude_balance,
    .start   = crude_start,
    .stop    = crude_stop,
};

/**
 * @brief 简单负载均衡器模块初始化，注册算法
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init crude_init(void)
{
    return vmm_load_balancer_register_algo(&crude);
}

/**
 * @brief 简单负载均衡器模块退出，注销算法
 */
static void __exit crude_exit(void)
{
    vmm_load_balancer_unregister_algo(&crude);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
