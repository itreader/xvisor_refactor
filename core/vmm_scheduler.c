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
 * @file vmm_scheduler.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor调度器源文件
 */

#include <arch_cpu_irq.h>
#include <arch_regs.h>
#include <arch_vcpu.h>
#include <libs/stringlib.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_cpumask.h>
#include <vmm_error.h>
#include <vmm_per_cpu.h>
#include <vmm_schedule_algorithm.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>

#define IDLE_VCPU_STACK_SZ    CONFIG_THREAD_STACK_SIZE
#define IDLE_VCPU_PRIORITY    VMM_VCPU_MIN_PRIORITY
#define IDLE_VCPU_TIMESLICE   (CONFIG_IDLE_TSLICE_SECS * 1000000000ULL)
#define IDLE_VCPU_DEADLINE    (IDLE_VCPU_TIMESLICE * 10)
#define IDLE_VCPU_PERIODICITY (IDLE_VCPU_DEADLINE * 10)

#define SAMPLE_EVENT_PERIOD   (CONFIG_IDLE_PERIOD_SECS * 1000000000ULL)

/**
 * @brief 调度器重调度状态，保存上下文切换时的寄存器状态
 */
enum vmm_scheduler_resched_state {
    VMM_SCHEDULER_RESCHED_IDLE = 0, /**< 0 */
    VMM_SCHEDULER_RESCHED_TRIGGERED
};

/** Control structure for Scheduler */
/**
 * @brief 调度器每CPU控制结构，维护单个CPU核心的调度状态
 */
struct vmm_scheduler_ctrl_per_cpu {
    void             *ready_queue; /**< ready_queue成员 */
    vmm_spinlock_t    rq_lock; /**< 运行队列锁 */
    atomic_t          rq_resched_state; /**< rq_resched_state成员 */
    uint64_t          current_vcpu_irq_ns; /**< current_vcpu_irq_ns成员 */
    uint64_t          current_vcpu_exp_ns; /**< current_vcpu_exp_ns成员 */
    vmm_vcpu_t       *current_vcpu; /**< current_vcpu成员 */
    vmm_vcpu_t       *idle_vcpu; /**< idle_vcpu成员 */
    bool              irq_context; /**< irq_context成员 */
    arch_regs_t      *irq_regs; /**< irq_regs成员 */
    uint64_t          irq_enter_tstamp; /**< irq_enter_tstamp成员 */
    uint64_t          irq_process_ns; /**< irq_process_ns成员 */
    uint64_t          exp_process_ns; /**< exp_process_ns成员 */
    bool              yield_on_irq_exit; /**< yield_on_irq_exit成员 */
    vmm_timer_event_t ev; /**< 事件 */
    vmm_timer_event_t sample_ev; /**< sample_ev成员 */
    vmm_rwlock_t      sample_lock; /**< sample_lock成员 */
    uint64_t          sample_period_ns; /**< sample_period_ns成员 */
    uint64_t          sample_idle_ns; /**< sample_idle_ns成员 */
    uint64_t          sample_idle_last_ns; /**< sample_idle_last_ns成员 */
    uint64_t          sample_irq_ns; /**< sample_irq_ns成员 */
    uint64_t          sample_irq_last_ns; /**< sample_irq_last_ns成员 */
};

static DEFINE_PER_CPU(struct vmm_scheduler_ctrl_per_cpu, sched);

/**
 * @brief 就绪 队列 出队
 * @param schedp 调度参数指针
 * @param next 指向VCPU结构体的指针
 * @param next_time_slice 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int ready_queue_dequeue(struct vmm_scheduler_ctrl_per_cpu *schedp, vmm_vcpu_t **next, uint64_t *next_time_slice)
{
    int         ret;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&schedp->rq_lock, flags);
    ret = vmm_schedule_algorithm_ready_queue_dequeue(schedp->ready_queue, next, next_time_slice);
    vmm_spin_unlock_irq_restore_lite(&schedp->rq_lock, flags);

    return ret;
}

/* NOTE: Must be called with vcpu->sched_lock held */
/**
 * @brief 就绪 队列 入队
 * @param schedp 调度参数指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功读取的字节数，失败返回错误码
 */
static int ready_queue_enqueue(struct vmm_scheduler_ctrl_per_cpu *schedp, vmm_vcpu_t *vcpu)
{
    int         ret;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&schedp->rq_lock, flags);
    ret = vmm_schedule_algorithm_ready_queue_enqueue(schedp->ready_queue, vcpu);
    vmm_spin_unlock_irq_restore_lite(&schedp->rq_lock, flags);

    return ret;
}

/* NOTE: Must be called with vcpu->sched_lock held */
/**
 * @brief 就绪 队列 分离
 * @param schedp 调度参数指针
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功读取的字节数，失败返回错误码
 */
static int ready_queue_detach(struct vmm_scheduler_ctrl_per_cpu *schedp, vmm_vcpu_t *vcpu)
{
    int         ret;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&schedp->rq_lock, flags);
    ret = vmm_schedule_algorithm_ready_queue_detach(schedp->ready_queue, vcpu);
    vmm_spin_unlock_irq_restore_lite(&schedp->rq_lock, flags);

    return ret;
}

/**
 * @brief 检查就绪队列是否需要抢占
 * @param schedp 调度参数指针
 * @return 成功读取的字节数，失败返回错误码
 */
static bool ready_queue_prempt_needed(struct vmm_scheduler_ctrl_per_cpu *schedp)
{
    bool        ret;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&schedp->rq_lock, flags);
    ret = vmm_schedule_algorithm_ready_queue_prempt_needed(schedp->ready_queue, schedp->current_vcpu);
    vmm_spin_unlock_irq_restore_lite(&schedp->rq_lock, flags);

    return ret;
}

/**
 * @brief 就绪 队列 长度
 * @param schedp 调度参数指针
 * @param priority 优先级
 * @return 就绪队列中的VCPU数量
 */
static uint32_t ready_queue_length(struct vmm_scheduler_ctrl_per_cpu *schedp, uint32_t priority)
{
    uint32_t    ret;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&schedp->rq_lock, flags);
    ret = vmm_schedule_algorithm_ready_queue_length(schedp->ready_queue, priority);
    vmm_spin_unlock_irq_restore_lite(&schedp->rq_lock, flags);

    return ret;
}

/* 不应从其他任何地方调用 */
/**
 * @brief 选择下一个要调度的VCPU（第一阶段：优先级选择）
 * @param schedp 调度参数指针
 * @param regs 寄存器上下文指针
 * @return 成功返回下一个被调度的VCPU，失败为内部错误会触发panic
 */
static vmm_vcpu_t *__vmm_scheduler_next1(struct vmm_scheduler_ctrl_per_cpu *schedp, arch_regs_t *regs)
{
    int         rc;
    irq_flags_t nf;
    uint64_t    tstamp          = vmm_timer_timestamp();
    uint64_t    next_time_slice = VMM_VCPU_DEF_TIME_SLICE;
    vmm_vcpu_t *next            = NULL;

    rc                          = ready_queue_dequeue(schedp, &next, &next_time_slice);

    if (rc) {
        /* This should never happen !!! */
        vmm_panic("%s: dequeue error %d\n", __func__, rc);
    }

    vmm_write_lock_irq_save_lite(&next->sched_lock, nf);

    arch_vcpu_switch(NULL, next, regs);
    next->state_ready_nsecs += tstamp - next->state_tstamp;
    arch_atomic_write(&next->state, VMM_VCPU_STATE_RUNNING);
    next->resumed               = FALSE;
    next->state_tstamp          = tstamp;
    schedp->current_vcpu        = next;
    schedp->current_vcpu_irq_ns = schedp->irq_process_ns;
    schedp->current_vcpu_exp_ns = schedp->exp_process_ns;
    vmm_timer_event_start(&schedp->ev, next_time_slice);

    vmm_write_unlock_irq_restore_lite(&next->sched_lock, nf);

    return next;
}

/* 必须在current->sched_lock的写锁持有状态下调用 */
/**
 * @brief 同步系统时间到调度器
 * @param schedp 调度参数指针
 * @param current 指向VCPU结构体的指针
 */
static void __vmm_scheduler_sync_system_time(struct vmm_scheduler_ctrl_per_cpu *schedp, vmm_vcpu_t *current)
{
    uint64_t system_nsecs;

    system_nsecs = schedp->irq_process_ns - schedp->current_vcpu_irq_ns;
    system_nsecs += schedp->exp_process_ns - schedp->current_vcpu_exp_ns;
    current->system_nsecs += system_nsecs;
    schedp->current_vcpu_irq_ns = schedp->irq_process_ns;
    schedp->current_vcpu_exp_ns = schedp->exp_process_ns;
}

/* 必须在current->sched_lock的写锁持有状态下调用 */
/**
 * @brief 选择下一个要调度的VCPU（第二阶段：同优先级轮转）
 * @param schedp 调度参数指针
 * @param current 指向VCPU结构体的指针
 * @param regs 寄存器上下文指针
 * @return 成功返回下一个被调度的VCPU，失败为内部错误会触发panic
 */
static vmm_vcpu_t *__vmm_scheduler_next2(struct vmm_scheduler_ctrl_per_cpu *schedp, vmm_vcpu_t *current, arch_regs_t *regs)
{
    int         rc;
    irq_flags_t nf = 0;
    uint32_t    current_state;
    uint64_t    tstamp          = vmm_timer_timestamp();
    uint64_t    next_time_slice = VMM_VCPU_DEF_TIME_SLICE;
    vmm_vcpu_t *next            = NULL;
    vmm_vcpu_t *tcurrent        = NULL;

    /* Normal scheduling */
    current_state               = arch_atomic_read(&current->state);

    if (current_state & VMM_VCPU_STATE_SAVEABLE) {
        if (current_state == VMM_VCPU_STATE_RUNNING) {
            __vmm_scheduler_sync_system_time(schedp, current);
            current->state_running_nsecs += tstamp - current->state_tstamp;
            arch_atomic_write(&current->state, VMM_VCPU_STATE_READY);
            current->state_tstamp = tstamp;
            ready_queue_enqueue(schedp, current);
        }

        tcurrent = current;
    }

dequeue_again:
    rc = ready_queue_dequeue(schedp, &next, &next_time_slice);

    if (rc) {
        /* This should never happen !!! */
        vmm_panic("%s: dequeue error %d\n", __func__, rc);
    }

    if (next != current) {
        vmm_write_lock_irq_save_lite(&next->sched_lock, nf);

        /* On SMP host, the next VCPU dequeued from ready queue can
         * be RESET, PAUSED, or HALTED from another host CPU after
         * it is dequeued and before we acquire next->sched_lock.
         *
         * To tackle the above situation, we check whether the next
         * VCPU is in READY state or not before doing context switch.
         * If the next VCPU is not in READY state then we try to
         * dequeue another next VCPU.
         */
        if (arch_atomic_read(&next->state) != VMM_VCPU_STATE_READY) {
            vmm_write_unlock_irq_restore_lite(&next->sched_lock, nf);
            goto dequeue_again;
        }

        arch_vcpu_switch(tcurrent, next, regs);
    }

    next->state_ready_nsecs += tstamp - next->state_tstamp;
    arch_atomic_write(&next->state, VMM_VCPU_STATE_RUNNING);
    next->resumed               = FALSE;
    next->state_tstamp          = tstamp;
    schedp->current_vcpu        = next;
    schedp->current_vcpu_irq_ns = schedp->irq_process_ns;
    schedp->current_vcpu_exp_ns = schedp->exp_process_ns;
    vmm_timer_event_start(&schedp->ev, next_time_slice);

    if (next != current) {
        vmm_write_unlock_irq_restore_lite(&next->sched_lock, nf);
    }

    return (next != current) ? next : NULL;
}

/**
 * @brief 调度器 交换机
 * @param schedp 调度参数指针
 * @param regs 寄存器上下文指针
 */
static void vmm_scheduler_switch(struct vmm_scheduler_ctrl_per_cpu *schedp, arch_regs_t *regs)
{
    uint32_t    preempt_min;
    vmm_vcpu_t *next;
    vmm_vcpu_t *current = schedp->current_vcpu;

    if (!regs) {
        /* This should never happen !!! */
        vmm_panic("%s: null pointer to regs.\n", __func__);
    }

    if (current) {
        preempt_min = (current->wait_queue_lock) ? 1 : 0;

        if (current->preempt_count == preempt_min) {
            irq_flags_t cf;

            vmm_write_lock_irq_save_lite(&current->sched_lock, cf);
            next = __vmm_scheduler_next2(schedp, current, regs);
            vmm_write_unlock_irq_restore_lite(&current->sched_lock, cf);

            if (next != current) {
                if (current->wait_queue_lock) {
                    vmm_spin_unlock_lite(current->wait_queue_lock);
                    arch_cpu_irq_save(cf);

                    if (current->preempt_count) {
                        current->preempt_count--;
                    }

                    arch_cpu_irq_restore(cf);
                }
            }
        } else {
            vmm_timer_event_restart(&schedp->ev);
            next = NULL;
        }
    } else {
        next = __vmm_scheduler_next1(schedp, regs);
    }

    if (next) {
        arch_vcpu_post_switch(next, regs);
    }
}

/**
 * @brief 调度器 定时器 事件
 * @param ev 定时器事件
 */
static void scheduler_timer_event(vmm_timer_event_t *ev)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    if (schedp->irq_regs) {
        vmm_scheduler_switch(schedp, schedp->irq_regs); /**< schedp->irq_regs)成员 */
    }
}

/**
 * @brief 调度器 抢占 禁用
 */
void vmm_scheduler_preempt_disable(void)
{
    irq_flags_t                flags;
    vmm_vcpu_t                *vcpu;
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    arch_cpu_irq_save(flags);

    if (!schedp->irq_context) {
        vcpu = schedp->current_vcpu; /**< schedp->current_vcpu成员 */

        if (vcpu) {
            vcpu->preempt_count++;
        }
    }

    arch_cpu_irq_restore(flags);
}

/**
 * @brief 调度器 抢占 启用
 */
void vmm_scheduler_preempt_enable(void)
{
    irq_flags_t                flags;
    vmm_vcpu_t                *vcpu;
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    arch_cpu_irq_save(flags);

    if (!schedp->irq_context) {
        vcpu = schedp->current_vcpu; /**< schedp->current_vcpu成员 */

        if (vcpu && vcpu->preempt_count) {
            vcpu->preempt_count--;
        }
    }

    arch_cpu_irq_restore(flags);
}

/**
 * @brief 调度器 抢占 孤儿
 * @param regs 寄存器上下文指针
 */
void vmm_scheduler_preempt_orphan(arch_regs_t *regs)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    vmm_scheduler_switch(schedp, regs);
}

/**
 * @brief 调度器 处理器间中断 重调度
 * @param arg0 第零个参数值
 * @param arg1 第一个参数值
 * @param arg2 第二个参数值
 */
static void scheduler_ipi_resched(void *arg0, void *arg1, void *arg2)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    arch_atomic_write(&schedp->rq_resched_state, VMM_SCHEDULER_RESCHED_IDLE);

    if (schedp->irq_regs && ready_queue_prempt_needed(schedp)) {
        vmm_scheduler_switch(schedp, schedp->irq_regs); /**< schedp->irq_regs)成员 */
    }
}

/**
 * @brief 强制触发当前CPU的VCPU重新调度
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_force_resched(uint32_t host_cpu)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp;

    if (CONFIG_CPU_COUNT <= host_cpu) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (!vmm_cpu_online(host_cpu)) {
        return VMM_ERR_NOTAVAIL;
    }

    if (host_cpu == vmm_smp_processor_id()) {
        return VMM_OK;
    }

    schedp = &per_cpu(sched, host_cpu);

    if (arch_atomic_cmpxchg(&schedp->rq_resched_state, VMM_SCHEDULER_RESCHED_IDLE, VMM_SCHEDULER_RESCHED_TRIGGERED) == VMM_SCHEDULER_RESCHED_IDLE) {
        vmm_smp_ipi_sync_call(vmm_cpumask_of(host_cpu), 0, scheduler_ipi_resched, NULL, NULL, NULL);
    }

    return VMM_OK;
}

/**
 * @brief 同步调度器的系统时间
 * @param arg0 第零个参数值
 * @param arg1 第一个参数值
 * @param arg2 第二个参数值
 */
static void scheduler_system_time_sync(void *arg0, void *arg1, void *arg2)
{
    irq_flags_t flags;
    irq_flags_t flags1;
    vmm_vcpu_t                *current;
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    arch_cpu_irq_save(flags);

    current = schedp->current_vcpu;

    if (current) {
        vmm_write_lock_irq_save_lite(&current->sched_lock, flags1);
        __vmm_scheduler_sync_system_time(schedp, current);
        vmm_write_unlock_irq_restore_lite(&current->sched_lock, flags1);
    }

    arch_cpu_irq_restore(flags);
}

/**
 * @brief 获取调度器的状态
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_get_status(
    vmm_vcpu_t *vcpu, uint32_t *state, uint8_t *priority, uint32_t *host_cpu, uint32_t *reset_count, uint64_t *last_reset_nsecs,
    uint64_t *ready_nsecs, uint64_t *running_nsecs, uint64_t *paused_nsecs, uint64_t *halted_nsecs, uint64_t *system_nsecs)
{
    int         rc;
    irq_flags_t flags;
    uint64_t    current_tstamp;
    uint32_t vcpu_hcpu;
    uint32_t current_state;

    if (!vcpu) {
        return VMM_ERR_FAIL;
    }

    /* Get host CPU assigned to given VCPU */
    rc = vmm_scheduler_get_hcpu(vcpu, &vcpu_hcpu);

    if (rc) {
        return rc;
    }

    /* Syncup system time on assigned host CPU */
    rc = vmm_smp_ipi_sync_call(vmm_cpumask_of(vcpu_hcpu), 0, scheduler_system_time_sync, NULL, NULL, NULL);

    if (rc) {
        return rc;
    }

    /* Current timestamp */
    current_tstamp = vmm_timer_timestamp();

    /* Acquire scheduling lock */
    vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);

    /* Current state */
    current_state = arch_atomic_read(&vcpu->state);

    /* Retrive current state and current host_cpu */
    if (state) {
        *state = current_state;
    }

    if (priority) {
        *priority = vcpu->priority;
    }

    if (host_cpu) {
        *host_cpu = vcpu->host_cpu;
    }

    /* Syncup statistics based on current timestamp */
    switch (current_state) {
        case VMM_VCPU_STATE_READY:
            vcpu->state_ready_nsecs += current_tstamp - vcpu->state_tstamp;
            vcpu->state_tstamp = current_tstamp;
            break;

        case VMM_VCPU_STATE_RUNNING:
            vcpu->state_running_nsecs += current_tstamp - vcpu->state_tstamp;
            vcpu->state_tstamp = current_tstamp;
            break;

        case VMM_VCPU_STATE_PAUSED:
            vcpu->state_paused_nsecs += current_tstamp - vcpu->state_tstamp;
            vcpu->state_tstamp = current_tstamp;
            break;

        case VMM_VCPU_STATE_HALTED:
            vcpu->state_halted_nsecs += current_tstamp - vcpu->state_tstamp;
            vcpu->state_tstamp = current_tstamp;
            break;

        default:
            break;
    }

    /* Retrive statistics */
    if (reset_count) {
        *reset_count = vcpu->reset_count;
    }

    if (last_reset_nsecs) {
        *last_reset_nsecs = current_tstamp - vcpu->reset_timestamp;
    }

    if (ready_nsecs) {
        *ready_nsecs = vcpu->state_ready_nsecs;
    }

    if (running_nsecs) {
        *running_nsecs = vcpu->state_running_nsecs;
    }

    if (paused_nsecs) {
        *paused_nsecs = vcpu->state_paused_nsecs;
    }

    if (halted_nsecs) {
        *halted_nsecs = vcpu->state_halted_nsecs;
    }

    if (system_nsecs) {
        *system_nsecs = vcpu->system_nsecs;
    }

    /* Release scheduling lock */
    vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return VMM_OK;
}

/**
 * @brief 通知调度器VCPU状态发生变化
 * @param vcpu 指向VCPU结构体的指针
 * @param new_state 状态值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_state_change(vmm_vcpu_t *vcpu, uint32_t new_state)
{
    uint64_t                   tstamp;
    int                        rc = VMM_OK;
    irq_flags_t                flags;
    bool resumed;
    bool preempt = FALSE;
    uint32_t chcpu = vmm_smp_processor_id();
    uint32_t vhcpu;
    struct vmm_scheduler_ctrl_per_cpu *schedp;
    uint32_t                   current_state;

    if (!vcpu) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    arch_cpu_irq_save(flags);

    vmm_write_lock_lite(&vcpu->sched_lock);

    vhcpu         = vcpu->host_cpu;
    schedp        = &per_cpu(sched, vhcpu);

    current_state = arch_atomic_read(&vcpu->state);

    switch (new_state) {
        case VMM_VCPU_STATE_UNKNOWN:
            /* Existing VCPU being destroyed */
            rc = vmm_schedule_algorithm_vcpu_cleanup(vcpu);
            break;

        case VMM_VCPU_STATE_RESET:
            if (current_state == VMM_VCPU_STATE_UNKNOWN) {
                /* New VCPU */
                rc = vmm_schedule_algorithm_vcpu_setup(vcpu);
            } else if (current_state != VMM_VCPU_STATE_RESET) {
                /* Existing VCPU */
                /* Clear resumed flag */
                vcpu->resumed = FALSE;

                /* Make sure VCPU is not in a ready queue */
                if ((schedp->current_vcpu != vcpu) && (current_state == VMM_VCPU_STATE_READY)) {
                    if ((rc = ready_queue_detach(schedp, vcpu))) {
                        break;
                    }
                }

                /* Make sure current VCPU is preempted */
                if ((schedp->current_vcpu == vcpu) && (current_state == VMM_VCPU_STATE_RUNNING)) {
                    preempt = TRUE;
                }

                vcpu->reset_count++;

                if ((rc = arch_vcpu_init(vcpu))) {
                    break;
                }

                if ((rc = vmm_vcpu_irq_init(vcpu))) {
                    break;
                }
            } else {
                goto skip_state_change;
            }

            break;

        case VMM_VCPU_STATE_READY:
            if ((current_state == VMM_VCPU_STATE_RESET) || (current_state == VMM_VCPU_STATE_PAUSED)) {
                /* Enqueue VCPU to ready queue */
                rc = ready_queue_enqueue(schedp, vcpu);

                if (!rc && (schedp->current_vcpu != vcpu)) {
                    preempt = ready_queue_prempt_needed(schedp);
                }

                if (vcpu->wait_queue_cleanup) {
                    vcpu->wait_queue_cleanup(vcpu);
                }
            } else if (current_state == VMM_VCPU_STATE_RUNNING) {
                /* Set resumed flag. This means we catch
                 * resume event while VCPU is RUNNING.
                 */
                vcpu->resumed = TRUE;
                preempt       = TRUE;
                goto skip_state_change;
            } else if (current_state == VMM_VCPU_STATE_READY) {
                /* READY->READY is a valid scenario... Do nothing. */
                goto skip_state_change;
            } else {
                rc = VMM_ERR_INVALID;
            }

            break;

        case VMM_VCPU_STATE_RUNNING:
            /* Only context-switch can set RUNNING state.
             * Any request for setting RUNNING state is invalid.
             */
            rc = VMM_ERR_INVALID;
            break;

        case VMM_VCPU_STATE_PAUSED:
            if (current_state == VMM_VCPU_STATE_PAUSED) {
                goto skip_state_change;
            }

        case VMM_VCPU_STATE_HALTED:
            if ((current_state == VMM_VCPU_STATE_READY) || (current_state == VMM_VCPU_STATE_RUNNING)) {
                resumed       = vcpu->resumed;
                vcpu->resumed = FALSE;

                if (resumed && (new_state == VMM_VCPU_STATE_PAUSED)) {
                    if (vcpu->wait_queue_cleanup) {
                        vcpu->wait_queue_cleanup(vcpu);
                    }

                    goto skip_state_change;
                } else if (schedp->current_vcpu == vcpu) {
                    /* Preempt current VCPU if paused or halted */
                    preempt = TRUE;
                } else if (current_state == VMM_VCPU_STATE_READY) {
                    /* Make sure VCPU is not in a ready queue */
                    rc = ready_queue_detach(schedp, vcpu);
                }
            } else {
                if (vcpu->wait_queue_cleanup) {
                    vcpu->wait_queue_cleanup(vcpu);
                }

                rc = VMM_ERR_INVALID;
            }

            break;
    };

    if (rc == VMM_OK) {
        tstamp = vmm_timer_timestamp();

        switch (current_state) {
            case VMM_VCPU_STATE_READY:
                vcpu->state_ready_nsecs += tstamp - vcpu->state_tstamp;
                break;

            case VMM_VCPU_STATE_RUNNING:
                vcpu->state_running_nsecs += tstamp - vcpu->state_tstamp;
                break;

            case VMM_VCPU_STATE_PAUSED:
                vcpu->state_paused_nsecs += tstamp - vcpu->state_tstamp;
                break;

            case VMM_VCPU_STATE_HALTED:
                vcpu->state_halted_nsecs += tstamp - vcpu->state_tstamp;
                break;

            default:
                break;
        }

        if (new_state == VMM_VCPU_STATE_RESET) {
            vcpu->state_ready_nsecs   = 0;
            vcpu->state_running_nsecs = 0;
            vcpu->state_paused_nsecs  = 0;
            vcpu->state_halted_nsecs  = 0;
            vcpu->system_nsecs        = 0;
            vcpu->reset_timestamp     = tstamp;
        }

        arch_atomic_write(&vcpu->state, new_state);
        vcpu->state_tstamp = tstamp;
    }

skip_state_change:
    vmm_write_unlock_lite(&vcpu->sched_lock);

    if (preempt && schedp->current_vcpu) {
        if (chcpu == vhcpu) {
            if (schedp->current_vcpu->is_normal) {
                schedp->yield_on_irq_exit = TRUE;
            } else if (schedp->irq_context) {
                vmm_scheduler_switch(schedp, schedp->irq_regs);
            } else {
                arch_vcpu_preempt_orphan();

                if (schedp->current_vcpu->wait_queue_lock) {
                    vmm_spin_lock(schedp->current_vcpu->wait_queue_lock);
                }
            }
        } else {
            rc = vmm_scheduler_force_resched(vhcpu);
        }
    }

    arch_cpu_irq_restore(flags);

    if (rc) {
        vmm_printf(
            "vcpu=%s current_state=0x%x to new_state=0x%x "
            "failed (error %d)\n",
            vcpu->name, current_state, new_state, rc);
        WARN_ON(1);
    }

    return rc;
}

/**
 * @brief 获取调度器的hcpu
 * @param vcpu 指向VCPU结构体的指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu)
{
    irq_flags_t flags;

    if ((vcpu == NULL) || (host_cpu == NULL)) {
        return VMM_ERR_FAIL;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
    *host_cpu = vcpu->host_cpu;
    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return VMM_OK;
}

/**
 * @brief 检查当前硬件CPU是否仍在运行指定的VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_check_current_hcpu(vmm_vcpu_t *vcpu)
{
    bool        ret;
    irq_flags_t flags;

    if (vcpu == NULL) {
        return FALSE;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
    ret = (vcpu->host_cpu == vmm_smp_processor_id()) ? TRUE : FALSE;
    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return ret;
}

/**
 * @brief 调度器 处理器间中断 迁移 虚拟CPU
 * @param arg0 第零个参数值
 * @param arg1 第一个参数值
 * @param arg2 第二个参数值
 */
static void scheduler_ipi_migrate_vcpu(void *arg0, void *arg1, void *arg2)
{
    irq_flags_t flags;
    uint32_t    old_hcpu = vmm_smp_processor_id();
    uint32_t state;
    uint32_t new_hcpu = (uint32_t)(virtual_addr_t)arg1;
    vmm_vcpu_t *vcpu = arg0;

    /* Lock VCPU scheduling */
    vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);

    /* The VCPU has to be in READY state otherwise we skip */
    state = arch_atomic_read(&vcpu->state);

    if ((state != VMM_VCPU_STATE_READY) || (vcpu->host_cpu != old_hcpu) || (vcpu->host_cpu == new_hcpu)) {
        goto skip;
    }

    /* Detach VCPU from old host_cpu ready queue */
    ready_queue_detach(&per_cpu(sched, old_hcpu), vcpu);

    /* Enqueue VCPU to new host_cpu ready queue */
    vcpu->host_cpu = new_hcpu;
    ready_queue_enqueue(&per_cpu(sched, new_hcpu), vcpu);

    /* Trigger re-scheduling on new host_cpu */
    vmm_scheduler_force_resched(new_hcpu);

skip:
    /* Unlock VCPU scheduling */
    vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
}

/**
 * @brief 设置调度器的hcpu
 * @param vcpu 指向VCPU结构体的指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scheduler_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu)
{
    uint32_t old_hcpu;
    uint32_t state;
    irq_flags_t flags;
    bool        migrate_vcpu = FALSE;

    if (!vcpu) {
        return VMM_ERR_FAIL;
    }

    /* Lock VCPU scheduling */
    vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);

    /* Current host_cpu */
    old_hcpu = vcpu->host_cpu;

    /* If host_cpu not changing then do nothing */
    if (old_hcpu == host_cpu) {
        vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
        return VMM_OK;
    }

    /* Match affinity with new host_cpu */
    if (!vmm_cpumask_test_cpu(host_cpu, vcpu->cpu_affinity)) {
        vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
        return VMM_ERR_INVALID;
    }

    /* Check if we don't need to migrate VCPU to new host_cpu */
    state = arch_atomic_read(&vcpu->state);

    if ((state == VMM_VCPU_STATE_READY) || (state == VMM_VCPU_STATE_RUNNING)) {
        migrate_vcpu = TRUE;
    } else {
        vcpu->host_cpu = host_cpu;
    }

    /* Unlock VCPU scheduling */
    vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    /* If required trigger VCPU migration on old host CPU */
    if (migrate_vcpu) {
        vmm_smp_ipi_async_call(vmm_cpumask_of(old_hcpu), scheduler_ipi_migrate_vcpu, vcpu, (void *)(virtual_addr_t)host_cpu, NULL);
    }

    return VMM_OK;
}

/**
 * @brief 通知调度器进入中断上下文
 * @param regs 寄存器上下文指针
 * @param vcpu_context VCPU上下文结构体指针
 */
void vmm_scheduler_irq_enter(arch_regs_t *regs, bool vcpu_context)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    /* Indicate that we have entered in IRQ */
    schedp->irq_enter_tstamp          = vmm_timer_timestamp();

    if (vcpu_context) {
        schedp->irq_context = FALSE;
    } else {
        schedp->irq_context = TRUE;
    }

    /* Save pointer to IRQ registers */
    schedp->irq_regs          = regs;

    /* Ensure that yield on exit is disabled */
    schedp->yield_on_irq_exit = FALSE;
}

/**
 * @brief 调度器 中断 寄存器
 */
arch_regs_t *vmm_scheduler_irq_regs(void)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    return schedp->irq_regs;
}

/**
 * @brief 通知调度器退出中断上下文
 * @param regs 寄存器上下文指针
 */
void vmm_scheduler_irq_exit(arch_regs_t *regs)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);
    vmm_vcpu_t                *vcpu   = NULL;

    /* Determine current vcpu */
    vcpu                              = schedp->current_vcpu;

    if (!vcpu) {
        return;
    }

    /* If current vcpu is not RUNNING or yield on exit is set
     * then context switch
     */
    if ((vmm_manager_vcpu_get_state(vcpu) != VMM_VCPU_STATE_RUNNING) || schedp->yield_on_irq_exit) {
        vmm_scheduler_switch(schedp, schedp->irq_regs);
        schedp->yield_on_irq_exit = FALSE;
    }

    /* VCPU irq processing */
    vmm_vcpu_irq_process(vcpu, regs);

    /* Indicate that we have exited IRQ */
    if (schedp->irq_context) {
        schedp->irq_process_ns += vmm_timer_timestamp() - schedp->irq_enter_tstamp;
    } else {
        schedp->exp_process_ns += vmm_timer_timestamp() - schedp->irq_enter_tstamp;
    }

    schedp->irq_context = FALSE;

    /* Clear pointer to IRQ registers */
    schedp->irq_regs    = NULL;
}

/**
 * @brief 检查调度器是否处于中断上下文
 * @return 处于中断上下文返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_irq_context(void)
{
    return this_cpu(sched).irq_context;
}

/**
 * @brief
 *  虚拟机监视器的线程执行上下文
 */
bool vmm_scheduler_orphan_context(void)
{
    bool                       ret = FALSE;
    irq_flags_t                flags;
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    arch_cpu_irq_save(flags);

    if (schedp->current_vcpu && !schedp->irq_context) {
        ret = (schedp->current_vcpu->is_normal) ? FALSE : TRUE; /**< TRUE成员 */
    }

    arch_cpu_irq_restore(flags);

    return ret;
}

/**
 * @brief 检查调度器是否处于普通上下文
 * @return 处于普通上下文返回TRUE，否则返回FALSE
 */
bool vmm_scheduler_normal_context(void)
{
    bool                       ret = FALSE;
    irq_flags_t                flags;
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    arch_cpu_irq_save(flags);

    if (schedp->current_vcpu && !schedp->irq_context) {
        ret = (schedp->current_vcpu->is_normal) ? TRUE : FALSE; /**< FALSE成员 */
    }

    arch_cpu_irq_restore(flags);

    return ret;
}

/**
 * @brief 获取调度器就绪队列的数量
 * @param host_cpu 主机CPU编号
 * @param priority 优先级
 * @return 数量值
 */
uint32_t vmm_scheduler_ready_count(uint32_t host_cpu, uint8_t priority)
{
    if ((CONFIG_CPU_COUNT <= host_cpu) || !vmm_cpu_online(host_cpu) || (priority < VMM_VCPU_MIN_PRIORITY) || (VMM_VCPU_MAX_PRIORITY < priority)) {
        return 0;
    }

    return ready_queue_length(&per_cpu(sched, host_cpu), priority);
}

/**
 * @brief 采样调度器事件用于统计
 * @param ev 定时器事件
 */
static void scheduler_sample_event(vmm_timer_event_t *ev)
{
    irq_flags_t                flags;
    uint64_t idle_ns;
    uint64_t irq_ns;
    uint64_t next_period;
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    idle_ns                           = 0;
    vmm_scheduler_get_status(schedp->idle_vcpu, NULL, NULL, NULL, NULL, NULL, NULL, &idle_ns, NULL, NULL, NULL);

    irq_ns = 0;
    arch_cpu_irq_save(flags);
    irq_ns = schedp->irq_process_ns;
    arch_cpu_irq_restore(flags);

    vmm_write_lock_irq_save_lite(&schedp->sample_lock, flags);

    schedp->sample_idle_ns      = idle_ns - schedp->sample_idle_last_ns;
    schedp->sample_idle_last_ns = idle_ns;
    schedp->sample_irq_ns       = irq_ns - schedp->sample_irq_last_ns;
    schedp->sample_irq_last_ns  = irq_ns;

    next_period                 = schedp->sample_period_ns;

    vmm_write_unlock_irq_restore_lite(&schedp->sample_lock, flags);

    vmm_timer_event_start(&schedp->sample_ev, next_period);
}

/**
 * @brief 获取调度器的采样周期
 * @param host_cpu 主机CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_scheduler_get_sample_period(uint32_t host_cpu)
{
    uint64_t                   ret;
    irq_flags_t                flags;
    struct vmm_scheduler_ctrl_per_cpu *schedp;

    if ((CONFIG_CPU_COUNT <= host_cpu) || !vmm_cpu_online(host_cpu)) {
        return SAMPLE_EVENT_PERIOD; /**< SAMPLE_EVENT_PERIOD成员 */
    }

    schedp = &per_cpu(sched, host_cpu);

    vmm_read_lock_irq_save_lite(&schedp->sample_lock, flags);
    ret = schedp->sample_period_ns;
    vmm_read_unlock_irq_restore_lite(&schedp->sample_lock, flags);

    return ret;
}

/**
 * @brief 设置调度器的采样周期
 * @param host_cpu 主机CPU编号
 * @param period 周期
 */
void vmm_scheduler_set_sample_period(uint32_t host_cpu, uint64_t period)
{
    irq_flags_t                flags;
    struct vmm_scheduler_ctrl_per_cpu *schedp;

    if ((CONFIG_CPU_COUNT <= host_cpu) || !vmm_cpu_online(host_cpu)) {
        return;
    }

    schedp = &per_cpu(sched, host_cpu);

    vmm_write_lock_irq_save_lite(&schedp->sample_lock, flags);
    schedp->sample_period_ns = period;
    vmm_write_unlock_irq_restore_lite(&schedp->sample_lock, flags);
}

/**
 * @brief 调度器 中断 时间
 * @param host_cpu 主机CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_scheduler_irq_time(uint32_t host_cpu)
{
    uint64_t                   ret;
    irq_flags_t                flags;
    struct vmm_scheduler_ctrl_per_cpu *schedp;

    if ((CONFIG_CPU_COUNT <= host_cpu) || !vmm_cpu_online(host_cpu)) {
        return 0; /**< 0 */
    }

    schedp = &per_cpu(sched, host_cpu);

    vmm_read_lock_irq_save_lite(&schedp->sample_lock, flags);
    ret = schedp->sample_irq_ns;
    vmm_read_unlock_irq_restore_lite(&schedp->sample_lock, flags);

    return ret;
}

/**
 * @brief 调度器 空闲 时间
 * @param host_cpu 主机CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_scheduler_idle_time(uint32_t host_cpu)
{
    uint64_t                   ret;
    irq_flags_t                flags;
    struct vmm_scheduler_ctrl_per_cpu *schedp;

    if ((CONFIG_CPU_COUNT <= host_cpu) || !vmm_cpu_online(host_cpu)) {
        return 0; /**< 0 */
    }

    schedp = &per_cpu(sched, host_cpu);

    vmm_read_lock_irq_save_lite(&schedp->sample_lock, flags);
    ret = schedp->sample_idle_ns;
    vmm_read_unlock_irq_restore_lite(&schedp->sample_lock, flags);

    return ret;
}

/**
 * @brief 调度器 空闲 虚拟CPU
 * @param host_cpu 主机CPU编号
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_scheduler_idle_vcpu(uint32_t host_cpu)
{
    if ((CONFIG_CPU_COUNT <= host_cpu) || !vmm_cpu_online(host_cpu)) {
        return NULL;
    }

    return per_cpu(sched, host_cpu).idle_vcpu;
}

/**
 * @brief 调度器 当前 虚拟CPU
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_scheduler_current_vcpu(void)
{
    return this_cpu(sched).current_vcpu;
}

/**
 * @brief 调度器 当前 优先级
 * @return 调度结果
 */
uint8_t vmm_scheduler_current_priority(void)
{
    vmm_vcpu_t *cvcpu = vmm_scheduler_current_vcpu();

    return (cvcpu) ? cvcpu->priority : VMM_VCPU_MAX_PRIORITY;
}

struct vmm_guest *vmm_scheduler_current_guest(void)
{
    vmm_vcpu_t *vcpu = this_cpu(sched).current_vcpu; /**< this_cpu(sched).current_vcpu成员 */

    return (vcpu) ? vcpu->guest : NULL; /**< NULL成员 */
}

/**
 * @brief 调度器 让出
 */
void vmm_scheduler_yield(void)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);
    vmm_vcpu_t                *vcpu   = this_cpu(sched).current_vcpu;

    if (schedp->irq_context) {
        vmm_panic("%s: Cannot yield in IRQ context\n", __func__); /**< __func__)成员 */
    }

    if (!vcpu) {
        vmm_panic("%s: NULL VCPU pointer\n", __func__);
    }

    if (vmm_manager_vcpu_get_state(vcpu) == VMM_VCPU_STATE_RUNNING) {
        vmm_scheduler_state_change(vcpu, VMM_VCPU_STATE_READY);
    }
}

/**
 * @brief 空闲 孤儿
 */
static void idle_orphan(void)
{
    struct vmm_scheduler_ctrl_per_cpu *schedp = &this_cpu(sched);

    while (1) {
        if (ready_queue_length(schedp, IDLE_VCPU_PRIORITY) == 0) {
            arch_cpu_wait_for_irq();
        }

        vmm_scheduler_yield();
    }
}

/**
 * @brief 调度器初始化启动
 * @param cpu_hotplug CPU热插拔结构体指针
 * @param cpu CPU编号
 * @return 编号值
 */
static int scheduler_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    int                        rc;
    char                       vcpu_name[VMM_FIELD_NAME_SIZE];
    struct vmm_scheduler_ctrl_per_cpu *schedp = &per_cpu(sched, cpu);

    /* Reset the scheduler control structure */
    memset(schedp, 0, sizeof(struct vmm_scheduler_ctrl_per_cpu));

    /* Create ready queue (Per Host CPU) */
    schedp->ready_queue = vmm_schedule_algorithm_ready_queue_create();

    if (!schedp->ready_queue) {
        return VMM_ERR_FAIL;
    }

    INIT_SPIN_LOCK(&schedp->rq_lock);
    ARCH_ATOMIC_INIT(&schedp->rq_resched_state, VMM_SCHEDULER_RESCHED_IDLE);

    /* Initialize current VCPU and IDLE VCPU. (Per Host CPU) */
    schedp->current_vcpu_irq_ns = 0;
    schedp->current_vcpu_exp_ns = 0;
    schedp->current_vcpu        = NULL;
    schedp->idle_vcpu           = NULL;

    /* Initialize IRQ state (Per Host CPU) */
    schedp->irq_context         = FALSE;
    schedp->irq_regs            = NULL;
    schedp->irq_enter_tstamp    = 0;
    schedp->irq_process_ns      = 0;
    schedp->exp_process_ns      = 0;

    /* Initialize yield on exit (Per Host CPU) */
    schedp->yield_on_irq_exit   = FALSE;

    /* Initialize timer events (Per Host CPU) */
    INIT_TIMER_EVENT(&schedp->ev, &scheduler_timer_event, schedp);
    INIT_TIMER_EVENT(&schedp->sample_ev, &scheduler_sample_event, schedp);

    /* Initialize sampling info (Per Host CPU) */
    INIT_RW_LOCK(&schedp->sample_lock);
    schedp->sample_period_ns    = SAMPLE_EVENT_PERIOD;
    schedp->sample_idle_ns      = 0;
    schedp->sample_idle_last_ns = 0;
    schedp->sample_irq_ns       = 0;
    schedp->sample_irq_last_ns  = 0;

    /* Mark this CPU online
     * Note: must be done before creating IDLE VCPU and
     * setting affinity
     */
    vmm_set_cpu_online(cpu, TRUE);

    /* Create idle orphan vcpu with default time slice. (Per Host CPU) */
    vmm_snprintf(vcpu_name, sizeof(vcpu_name), "idle/%d", cpu);
    schedp->idle_vcpu = vmm_manager_vcpu_orphan_create(
        vcpu_name, (virtual_addr_t)&idle_orphan, IDLE_VCPU_STACK_SZ, IDLE_VCPU_PRIORITY, IDLE_VCPU_TIMESLICE, IDLE_VCPU_TIMESLICE,
        IDLE_VCPU_TIMESLICE, vmm_cpumask_of(cpu));

    if (!schedp->idle_vcpu) {
        return VMM_ERR_FAIL;
    }

    /* Kick idle orphan vcpu */
    if ((rc = vmm_manager_vcpu_kick(schedp->idle_vcpu))) {
        return rc;
    }

    /* Start timer events */
    vmm_timer_event_start(&schedp->ev, 0);
    vmm_timer_event_start(&schedp->sample_ev, SAMPLE_EVENT_PERIOD);

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t scheduler_cpu_hotplug = {
    .name    = "SCHEDULER",
    .state   = VMM_CPU_HOTPLUG_STATE_SCHEDULER,
    .startup = scheduler_startup,
};

/**
 * @brief 初始化调度器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_scheduler_init(void)
{
    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&scheduler_cpu_hotplug, TRUE);
}
