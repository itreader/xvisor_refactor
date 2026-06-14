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
 * @file vmm_threads.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor线程头文件
 */
#ifndef __VMM_THREADS_H__
#define __VMM_THREADS_H__

#include <vmm_manager.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_THREAD_MAX_PRIORITY    VMM_VCPU_MAX_PRIORITY
#define VMM_THREAD_MIN_PRIORITY    VMM_VCPU_MIN_PRIORITY
#define VMM_THREAD_DEF_PRIORITY    VMM_VCPU_DEF_PRIORITY
#define VMM_THREAD_DEF_TIME_SLICE  VMM_VCPU_DEF_TIME_SLICE
#define VMM_THREAD_DEF_DEADLINE    VMM_VCPU_DEF_DEADLINE
#define VMM_THREAD_DEF_PERIODICITY VMM_VCPU_DEF_PERIODICITY

/**
 * @brief 线程状态枚举，定义线程从创建到退出的各生命周期状态
 */
enum vmm_thread_states {
    VMM_THREAD_STATE_CREATED  = 0, /**< 0 */
    VMM_THREAD_STATE_RUNNING  = 1, /**< 1 */
    VMM_THREAD_STATE_SLEEPING = 2, /**< 2 */
    VMM_THREAD_STATE_STOPPED  = 3
};

typedef struct vmm_thread {
    double_list_t head;              /* thread list head */
    vmm_vcpu_t   *vcpu_on_thread;    /* vcpu on which thread runs */
    int (*thread_func)(void *udata); /* thread functions */
    void    *tdata;                  /* data passed to thread
                                      * function on execution */
    int      thread_ret_value;       /* thread return value */
    uint64_t thread_nanoseconds;     /* thread time slice in nanoseconds */
    uint64_t thread_deadline;        /* thread deadline in nanoseconds */
    uint64_t thread_periodicity;     /* thread periodicity in nanoseconds */
} vmm_thread_t;

/**
 * @brief 启动threads
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_start(vmm_thread_t *thread_info);

/**
 * @brief 停止threads
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_stop(vmm_thread_t *thread_info);

/**
 * @brief 使线程进入休眠状态
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_sleep(vmm_thread_t *thread_info);

/**
 * @brief 唤醒休眠线程
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_wakeup(vmm_thread_t *thread_info);

/**
 * @brief 获取threads的ID
 * @param thread_info 线程信息结构体指针
 * @return 成功返回线程ID，失败返回U32_MAX
 */
uint32_t vmm_threads_get_id(vmm_thread_t *thread_info);

/**
 * @brief 获取threads的优先级
 * @param thread_info 线程信息结构体指针
 * @return 编号值
 */
uint8_t vmm_threads_get_priority(vmm_thread_t *thread_info);

/**
 * @brief 获取threads的名称
 * @param dst 目标缓冲区指针
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_get_name(char *dst, vmm_thread_t *thread_info);

/**
 * @brief 获取threads的状态
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_get_state(vmm_thread_t *thread_info);

/**
 * @brief 获取threads的hcpu
 * @param thread_info 线程信息结构体指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_get_hcpu(vmm_thread_t *thread_info, uint32_t *host_cpu);

/**
 * @brief 设置线程的hcpu
 * @param thread_info 线程信息结构体指针
 * @param host_cpu 主机CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_thread_set_hcpu(vmm_thread_t *thread_info, uint32_t host_cpu);

/**
 * @brief 获取threads的亲和性
 * @param thread_info 线程信息结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
const vmm_cpumask_t *vmm_threads_get_affinity(vmm_thread_t *thread_info);

/**
 * @brief 设置threads的亲和性
 * @param thread_info 线程信息结构体指针
 * @param cpu_mask CPU亲和性掩码
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_set_affinity(vmm_thread_t *thread_info, const vmm_cpumask_t *cpu_mask);

/**
 * @brief threads id2thread
 * @param thread_id 标识符
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_thread_t *vmm_threads_id2thread(uint32_t thread_id);

/**
 * @brief threads index2thread
 * @param index 数组中的索引位置
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_thread_t *vmm_threads_index2thread(int index);

/**
 * @brief 获取threads的数量
 * @return 数量值
 */
uint32_t vmm_threads_count(void);

/** Create a new thread with explicitly specified deadline and periodicity.
 *  This is more real-time friendly API so that users can specify deadline
 *  and periodicity for a VCPU.
 */
vmm_thread_t *vmm_threads_create_rt(
    const char *thread_name, int (*thread_fn)(void *udata), void *thread_data, uint8_t thread_priority, uint64_t thread_nsecs,
    uint64_t thread_deadline, uint64_t thread_periodicity);

/** Create a new thread */
static inline vmm_thread_t *vmm_threads_create(
    const char *thread_name, int (*thread_fn)(void *udata), void *thread_data, uint8_t thread_priority, uint64_t thread_nsecs)
{
/**
 * @brief 创建实时线程
 * @param thread_name 线程名称
 * @param thread_fn 线程入口函数
 * @param thread_data 线程私有数据
 * @param thread_priority 线程优先级
 * @param thread_nsecs 线程时间片（纳秒）
 * @param 10 指针参数
 * @param 100 指针参数
 * @return 成功读取的字节数，失败返回错误码
 */
    return vmm_threads_create_rt(thread_name, thread_fn, thread_data, thread_priority, thread_nsecs, thread_nsecs * 10, thread_nsecs * 100);
}

/**
 * @brief 销毁threads
 * @param thread_info 线程信息结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_destroy(vmm_thread_t *thread_info);

/**
 * @brief 初始化threads
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_threads_init(void);

#endif /* __VMM_THREADS_H__ */
