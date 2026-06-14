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
 * @file vmm_smp.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 对称多处理器管理API
 */

#ifndef __VMM_SMP_H__
#define __VMM_SMP_H__

#include <arch_smp.h>
#include <vmm_cpumask.h>
#include <vmm_error.h>
#include <vmm_types.h>

/** Get SMP processor ID
 *  Note: To ease development, this function returns 0 on UP systems.
 */
#if !defined(CONFIG_SMP)
#define vmm_smp_processor_id() 0
#else
#define vmm_smp_processor_id() arch_smp_id()
#endif

/**
 * @brief 获取给定SMP处理器ID的硬件ID
 */
static inline int vmm_smp_map_hwid(uint32_t cpu, uint64_t *hwid)
{
    if (!hwid || !vmm_cpu_possible(cpu)) {
        return VMM_ERR_INVALID;
    }

#if !defined(CONFIG_SMP)
    *hwid = 0;
    return VMM_OK;
#else
/**
 * @brief 将硬件CPU ID映射为逻辑CPU编号
 * @param cpu CPU编号
 * @param hwid 硬件ID值
 * @return 编号值
 */
    return arch_smp_map_hwid(cpu, hwid);
#endif
}

/** Get SMP processor ID for given Hardware ID
 *  Note: To ease development, this function returns 0 on UP systems.
 */
#if !defined(CONFIG_SMP)
static inline int vmm_smp_map_cpuid(uint64_t hwid, uint32_t *cpu)
{
    *cpu = 0;
    return VMM_OK;
}
#else
/**
 * @brief 将CPU ID映射为多处理器表索引
 * @param hwid 硬件ID值
 * @param cpu CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_smp_map_cpuid(uint64_t hwid, uint32_t *cpu);
#endif

/** Get SMP processor ID for Boot CPU
 *  Note: Boot CPU is the CPU on which we started booting.
 *  Note: To ease development, this function returns 0 on UP systems.
 */
#if !defined(CONFIG_SMP)
#define vmm_smp_bootcpu_id() 0
#else
/**
 * @brief 获取引导CPU的硬件ID
 * @return 编号值，失败返回负数错误码
 */
uint32_t vmm_smp_bootcpu_id(void);
#endif

/** Set current SMP processor as Boot CPU
 *  Note: Boot CPU is the CPU on which we started booting.
 *  Note: This will work for first CPU calling this function and for
 *  subsequent CPUs it is ignored.
 *  Note: To ease development, this function does nothing on UP systems.
 */
#if !defined(CONFIG_SMP)
#define vmm_smp_set_bootcpu()
#else
/**
 * @brief 设置多处理器的bootcpu
 */
void vmm_smp_set_bootcpu(void);
#endif

/** Check if current SMP processor is our Boot CPU.
 *  Note: Boot CPU is the CPU on which we started booting.
 *  Note: To ease development, this function returns TRUE on UP systems.
 */
#if !defined(CONFIG_SMP)
#define vmm_smp_is_bootcpu() TRUE
#else
/**
 * @brief 判断指定CPU是否为引导CPU
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_smp_is_bootcpu(void);
#endif

/**
 * @brief 多处理器 处理器间中断 执行
 */
void vmm_smp_ipi_exec(void);

/** Asynchronus call to function on multiple cores
 *  Note: To ease development, we have dummy implementation for UP systems.
 */
#if !defined(CONFIG_SMP)
static inline void vmm_smp_ipi_async_call(const vmm_cpumask_t *dest, void (*func)(void *, void *, void *), void *arg0, void *arg1, void *arg2)
{
    (func)(arg0, arg1, arg2);
}
#else
/**
 * @brief 多处理器间异步调用
 * @param dest CPU亲和性掩码
 * @param (*func 指针参数
 */
void vmm_smp_ipi_async_call(const vmm_cpumask_t *dest, void (*func)(void *, void *, void *), void *arg0, void *arg1, void *arg2);
#endif

/** Synchronus call to function on multiple cores
 *  Note: To ease development, we have dummy implementation for UP systems.
 */
#if !defined(CONFIG_SMP)
static inline int vmm_smp_ipi_sync_call(
    const vmm_cpumask_t *dest, uint32_t timeout_msecs, void (*func)(void *, void *, void *), void *arg0, void *arg1, void *arg2)
{
    (func)(arg0, arg1, arg2);
    return VMM_OK;
}
#else
int vmm_smp_ipi_sync_call(
    const vmm_cpumask_t *dest, uint32_t timeout_msecs, void (*func)(void *, void *, void *), void *arg0, void *arg1, void *arg2);
#endif

/**
 * @brief 初始化同步IPI
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_smp_sync_ipi_init(void);

/**
 * @brief 初始化异步IPI
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_smp_async_ipi_init(void);

#endif /* __VMM_SMP_H__ */
