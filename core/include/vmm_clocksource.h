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
 * @file vmm_clocksource.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 无状态时钟源头文件
 */
#ifndef _VMM_CLOCKSOURCE_H__
#define _VMM_CLOCKSOURCE_H__

#include <libs/list.h>
#include <libs/mathlib.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>

struct vmm_clocksource;
typedef struct vmm_clocksource vmm_clocksource_t;

/**
 * Hardware abstraction a timer subsystem clocksource
 * Provides mostly state-free accessors to the underlying hardware.
 * This is the structure used for tracking passsing time.
 *
 * @name:       ptr to clocksource name
 * @list:       list head for registration
 * @rating:     rating value for selection (higher is better)
 *          To avoid rating inflation the following
 *          list should give you a guide as to how
 *          to assign your clocksource a rating
 *          1-99: Unfit for real use
 *              Only available for bootup and testing purposes.
 *          100-199: Base level usability.
 *              Functional for real use, but not desired.
 *          200-299: Good.
 *              A correct and usable clocksource.
 *          300-399: Desired.
 *              A reasonably fast and accurate clocksource.
 *          400-499: Perfect
 *              The ideal clocksource. A must-use where
 *              available.
 * @read:       returns a cycle value, passes clocksource as argument
 * @enable:     optional function to enable the clocksource
 * @disable:        optional function to disable the clocksource
 * @mask:       bitmask for two's complement
 *          subtraction of non 64 bit counters
 * @freq:       frequency at which counter is running
 * @mult:       cycle to nanosecond multiplier
 * @shift:      cycle to nanosecond divisor (power of two)
 * @suspend:        suspend function for the clocksource, if necessary
 * @resume:     resume function for the clocksource, if necessary
 */
struct vmm_clocksource {
    double_list_t head; /**< 链表头 */
    const char   *name; /**< 名称 */
    int           rating; /**< rating成员 */
    uint64_t      mask; /**< 掩码 */
    uint32_t      freq; /**< 频率 */
    uint32_t      mult; /**< 乘数/多播 */
    uint32_t      shift; /**< shift成员 */
    uint64_t (*read)(vmm_clocksource_t *cs); /**< 读 */
    int (*enable)(vmm_clocksource_t *cs); /**< enable成员 */
    void (*disable)(vmm_clocksource_t *cs); /**< disable成员 */
    void (*clocksource)(vmm_clocksource_t *cs); /**< clocksource成员 */
    void (*resume)(vmm_clocksource_t *cs); /**< 恢复 */
    void *private; /**< 私有数据 */
};

/* simplify initialization of mask field */
#define VMM_CLOCKSOURCE_MASK(bits) (uint64_t)((bits) < 64 ? ((1ULL << (bits)) - 1) : -1)

/* nodeid table based clocksource initialization callback */
typedef int (*vmm_clocksource_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for clocksource */
#define VMM_CLOCKSOURCE_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "clocksource", "", "", compat, fn)

/**
 * Layer above a %vmm_clocksource_t which counts nanoseconds
 * Contains the state needed by vmm_timecounter_read() to detect
 * clocksource wrap around. Initialize with vmm_timecounter_init().
 * Users of this code are responsible for initializing the underlying
 * clocksource hardware, locking issues and reading the time more often
 * than the clocksource wraps around. The nanosecond counter will only
 * wrap around after ~585 years.
 *
 * @cs:         the cycle counter used by this instance
 * @cycles_last:    most recent cycle counter value seen by
 *          vmm_timecounter_read()
 * @nsec:       continuously increasing count
 */
struct vmm_timecounter {
    vmm_clocksource_t *cs; /**< 校验和/片选 */
    uint64_t           cycles_last; /**< cycles_last成员 */
    uint64_t           nsec; /**< nsec成员 */
};

typedef struct vmm_timecounter vmm_timecounter_t;

/**
 * @brief 将kHz频率转换为时钟源乘数
 */
static inline uint32_t vmm_clocksource_khz2mult(uint32_t khz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)1000000) << shift; /**< shift成员 */
    tmp += khz >> 1; /**< 1 */
    tmp = udiv64(tmp, khz); /**< khz)成员 */
    return (uint32_t)tmp; /**< (uint32_t)tmp成员 */
}

/**
 * @brief 将Hz频率转换为时钟源乘数
 */
static inline uint32_t vmm_clocksource_hz2mult(uint32_t hz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)1000000000) << shift;
    tmp += hz >> 1;
    tmp = udiv64(tmp, hz);
    return (uint32_t)tmp;
}

/** Convert delta cycles to nsecs */
#define vmm_clocksource_delta2nsecs(cycles, mult, shift) (((cycles) * (mult)) >> (shift))

/**
 * @brief 获取时间计数器时钟源的频率
 * @param tc 时钟计数器指针
 * @return 成功返回时钟源频率(Hz)，失败返回0
 */
uint32_t vmm_timecounter_clocksource_frequency(vmm_timecounter_t *tc);

/**
 * @brief 读取时间计数器当前值
 * @param tc 时钟计数器指针
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timecounter_read(vmm_timecounter_t *tc);

#if defined(CONFIG_PROFILE)
/**
 * @brief 读取时间计数器用于性能分析
 * @param tc 时钟计数器指针
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timecounter_read_for_profile(vmm_timecounter_t *tc);
#endif

/**
 * @brief 启动timecounter
 * @param tc 时钟计数器指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timecounter_start(vmm_timecounter_t *tc);

/**
 * @brief 停止timecounter
 * @param tc 时钟计数器指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timecounter_stop(vmm_timecounter_t *tc);

/**
 * @brief 初始化timecounter
 * @param tc 时钟计数器指针
 * @param cs 时钟源结构体指针
 * @param start_nsec 时间值（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timecounter_init(vmm_timecounter_t *tc, vmm_clocksource_t *cs, uint64_t start_nsec);

/**
 * @brief 注册时钟源
 * @param cs 时钟源结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clocksource_register(vmm_clocksource_t *cs);

/**
 * @brief 注销时钟源
 * @param cs 时钟源结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clocksource_unregister(vmm_clocksource_t *cs);

/**
 * @brief 获取最佳时钟源
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_clocksource_t *vmm_clocksource_best(void);

/**
 * @brief 查找时钟源
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_clocksource_t *vmm_clocksource_find(const char *name);

/**
 * @brief 时钟源 获取
 * @param index 数组中的索引位置
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_clocksource_t *vmm_clocksource_get(int index);

/**
 * @brief 获取时钟源的数量
 * @return 数量值
 */
uint32_t vmm_clocksource_count(void);

/**
 * @brief 初始化时钟源
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clocksource_init(void);

#endif
