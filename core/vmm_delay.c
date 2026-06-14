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
 * @file vmm_delay.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 软延迟子系统实现
 */

#include <arch_cpu.h>
#include <arch_delay.h>
#include <libs/mathlib.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_waitqueue.h>

static uint64_t loops_per_msec[CONFIG_CPU_COUNT];
static uint64_t loops_per_usec[CONFIG_CPU_COUNT];
static uint64_t loops_per_nsec[CONFIG_CPU_COUNT];

/**
 * @brief 纳秒级睡眠函数
 * @param nsecs 睡眠的纳秒数
 */
static void nanosec_sleep(uint64_t nsecs)
{
    int              rc;
    vmm_wait_queue_t wait_queue;

    INIT_WAITQUEUE(&wait_queue, NULL);

    rc = vmm_waitqueue_sleep_timeout(&wait_queue, &nsecs);

    if (rc != VMM_ERR_TIMEDOUT) {
        vmm_printf("%s: sleep timeout failed (error %d)\n", __func__, rc);
        WARN_ON(1);
    }
}

/**
 * @brief 微秒级睡眠函数
 * @param usecs 睡眠的微秒数
 */
void vmm_usleep(uint64_t usecs)
{
    nanosec_sleep((uint64_t)usecs * 1000ULL);
}

/**
 * @brief 毫秒级睡眠函数
 * @param msecs 睡眠的毫秒数
 */
void vmm_msleep(uint64_t msecs)
{
    nanosec_sleep((uint64_t)msecs * 1000000ULL);
}

/**
 * @brief 秒级睡眠函数
 * @param secs 睡眠的秒数
 */
void vmm_ssleep(uint64_t secs)
{
    nanosec_sleep((uint64_t)secs * 1000000000ULL);
}

/**
 * @brief 纳秒级延迟函数
 * @param nsecs 延迟的纳秒数
 */
void vmm_ndelay(uint64_t nsecs)
{
    uint64_t    lpnsec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpnsec = loops_per_nsec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    arch_delay_loop(nsecs * lpnsec);
}

/**
 * @brief 微秒级延迟函数
 * @param usecs 延迟的微秒数
 */
void vmm_udelay(uint64_t usecs)
{
    uint64_t    lpusec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpusec = loops_per_usec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    arch_delay_loop(usecs * lpusec);
}

/**
 * @brief 毫秒级延迟函数
 * @param msecs 延迟的毫秒数
 */
void vmm_mdelay(uint64_t msecs)
{
    uint64_t    lpmsec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpmsec = loops_per_msec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    arch_delay_loop(msecs * lpmsec);
}

/**
 * @brief 秒级延迟函数
 * @param secs 延迟的秒数
 */
void vmm_sdelay(uint64_t secs)
{
    uint32_t    i;
    uint64_t    lpmsec;
    irq_flags_t flags;

    arch_cpu_irq_save(flags);
    lpmsec = loops_per_msec[vmm_smp_processor_id()];
    arch_cpu_irq_restore(flags);

    for (i = 0; i < secs; i++) {
        arch_delay_loop(1000 * lpmsec);
    }
}

/**
 * @brief 估算CPU的MHz频率
 * @param cpu CPU编号
 * @return 返回估算的CPU频率（MHz）
 */
uint64_t vmm_delay_estimate_cpu_mhz(uint32_t cpu)
{
    return arch_delay_loop_cycles(loops_per_usec[cpu]);
}

/**
 * @brief 估算CPU的KHz频率
 * @param cpu CPU编号
 * @return 返回估算的CPU频率（KHz）
 */
uint64_t vmm_delay_estimate_cpu_khz(uint32_t cpu)
{
    return arch_delay_loop_cycles(loops_per_msec[cpu]);
}

/**
 * @brief 重新校准延迟循环参数
 */
void vmm_delay_recaliberate(void)
{
    uint64_t nsecs;
    uint64_t tstamp;
    irq_flags_t flags;
    uint32_t    cpu = vmm_smp_processor_id();

    arch_cpu_irq_save(flags);

    tstamp = vmm_timer_timestamp();

    arch_delay_loop(1000000);

    nsecs               = vmm_timer_timestamp() - tstamp;

    loops_per_nsec[cpu] = udiv64(1000000ULL, nsecs);
    loops_per_usec[cpu] = udiv64(1000ULL * 1000000ULL, nsecs);
    loops_per_msec[cpu] = udiv64(1000000ULL * 1000000ULL, nsecs);

    arch_cpu_irq_restore(flags);
}

/**
 * @brief 延迟子系统启动函数
 * @param cpu_hotplug CPU热插拔通知结构体
 * @param cpu CPU编号
 * @return 返回操作结果
 */
static int delay_startup(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu)
{
    vmm_delay_recaliberate();

    return VMM_OK;
}

static vmm_cpu_hotplug_notify_t delay_cpu_hotplug = {
    .name    = "DELAY",
    .state   = VMM_CPU_HOTPLUG_STATE_DELAY,
    .startup = delay_startup,
};

/**
 * @brief 初始化延迟子系统
 * @return 返回初始化结果
 */
int __init vmm_delay_init(void)
{
    uint32_t i;

    /* Clear everything */
    for (i = 0; i < CONFIG_CPU_COUNT; i++) {
        loops_per_msec[i] = 0;
        loops_per_usec[i] = 0;
    }

    /* Setup hotplug notifier */
    return vmm_cpu_hotplug_register(&delay_cpu_hotplug, TRUE);
}
