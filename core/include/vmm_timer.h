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
 * @file vmm_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 定时器子系统头文件
 */
#ifndef _VMM_TIMER_H__
#define _VMM_TIMER_H__

#include <libs/list.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

struct vmm_timer_event;
typedef struct vmm_timer_event vmm_timer_event_t;

/**
 * @brief 定时器事件结构，包含定时器回调函数和触发参数
 */
struct vmm_timer_event {
    /* Publically accessible info */
    uint64_t expiry_tstamp; /**< expiry_tstamp成员 */
    uint64_t duration_nsecs; /**< duration_nsecs成员 */
    void (*handler)(struct vmm_timer_event *); /**< 处理函数 */
    void *private; /**< 私有数据 */
    /* Internal house-keeping info */
    vmm_spinlock_t active_lock; /**< active_lock成员 */
    bool           active_state; /**< active_state成员 */
    double_list_t  active_head; /**< active_head成员 */
    uint32_t       active_hcpu; /**< active_hcpu成员 */
};

#define INIT_TIMER_EVENT(ev, _hndl, _private)                                                                                                        \
    do {                                                                                                                                             \
        (ev)->expiry_tstamp  = 0;                                                                                                                    \
        (ev)->duration_nsecs = 0;                                                                                                                    \
        (ev)->handler        = _hndl;                                                                                                                \
        (ev)->private        = _private;                                                                                                             \
        INIT_SPIN_LOCK(&(ev)->active_lock);                                                                                                          \
        INIT_LIST_HEAD(&(ev)->active_head);                                                                                                          \
        (ev)->active_state = FALSE;                                                                                                                  \
        (ev)->active_hcpu  = 0;                                                                                                                      \
    } while (0)

#define __TIMER_EVENT_INITIALIZER(ev, _hndl, _private)                                                                                               \
    {                                                                                                                                                \
        .expiry_tstamp = 0, .duration_nsecs = 0, .handler = _hndl, .private = _private, .active_lock = __SPINLOCK_INITIALIZER((ev).active_lock),     \
        .active_head = {&(ev).head, &(ev).head}, .active_state = FALSE, .active_hcpu = 0,                                                            \
    }

#define DECLARE_TIMER_EVENT(ev, _hndl, _private) vmm_timer_event_t ev = __TIMER_EVENT_INITIALIZER(ev, _hndl, _private)

/**
 * @brief 获取定时器时钟源的频率
 * @return 频率值（Hz）
 */
uint32_t vmm_timer_clocksource_frequency(void);

/**
 * @brief 获取定时器时钟芯片的频率
 * @return 频率值（Hz）
 */
uint32_t vmm_timer_clock_chip_frequency(void);

/**
 * @brief 检查定时器事件是否处于等待状态
 * @param ev 定时器事件
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_timer_event_pending(vmm_timer_event_t *ev);

/**
 * @brief 获取定时器事件的到期时间
 * @param ev 定时器事件
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timer_event_expiry_time(vmm_timer_event_t *ev);

/**
 * @brief 启动定时器事件（带扩展参数）
 * @param ev 定时器事件
 * @param duration_nsecs 时间值（纳秒）
 * @param ret_expiry_tstamp 用于返回到期时间戳
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timer_event_start2(vmm_timer_event_t *ev, uint64_t duration_nsecs, uint64_t *ret_expiry_tstamp);

/**
 * @brief 启动定时器事件
 */
static inline int vmm_timer_event_start(vmm_timer_event_t *ev, uint64_t duration_nsecs)
{
/**
 * @brief 启动定时器事件（带扩展参数）
 * @param ev 事件值
 * @param duration_nsecs 持续时间（纳秒）
 * @param NULL 参数
 * @return 时间值（纳秒）
 */
    return vmm_timer_event_start2(ev, duration_nsecs, NULL);
}

/**
 * @brief 重新启动已到期的定时器事件
 * @param ev 定时器事件
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timer_event_restart(vmm_timer_event_t *ev);

/**
 * @brief 停止定时器事件
 * @param ev 定时器事件
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timer_event_stop(vmm_timer_event_t *ev);

/**
 * @brief 将CPU时钟周期数转换为纳秒
 * @param cycles CPU时钟周期数
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timer_cycles_to_ns(uint64_t cycles);

/**
 * @brief 将时钟周期差值转换为纳秒
 * @param cycles CPU时钟周期数
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timer_delta_cycles_to_ns(uint64_t cycles);

/**
 * @brief 定时器 时间戳
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timer_timestamp(void);

#if defined(CONFIG_PROFILE)
/**
 * @brief 获取用于性能分析的时间戳
 * @return 返回64位无符号整数值
 */
uint64_t vmm_timer_timestamp_for_profile(void);
#endif

/**
 * @brief 检查定时器是否已启动
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_timer_started(void);

/**
 * @brief 启动定时器
 */
void vmm_timer_start(void);

/**
 * @brief 停止定时器
 */
void vmm_timer_stop(void);

/**
 * @brief 初始化定时器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_timer_init(void);

#endif
