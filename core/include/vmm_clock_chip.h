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
 * @file vmm_clock_chip.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 时钟芯片管理接口
 */
#ifndef _VMM_CLOCKCHIP_H__
#define _VMM_CLOCKCHIP_H__

#include <libs/list.h>
#include <libs/mathlib.h>
#include <vmm_cpumask.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>

/* Clock chip mode commands */
/* 时钟芯片模式 */
/**
 * @brief 时钟芯片工作模式，定义时钟芯片的回调接口和特性
 */
enum vmm_clock_chip_mode {
    VMM_CLOCKCHIP_MODE_UNUSED = 0, /**< 0 */
    VMM_CLOCKCHIP_MODE_SHUTDOWN,
    VMM_CLOCKCHIP_MODE_PERIODIC,
    VMM_CLOCKCHIP_MODE_ONESHOT,
    VMM_CLOCKCHIP_MODE_RESUME,
};

typedef enum vmm_clock_chip_mode vmm_clock_chip_mode_e;

/* Clockchip features */
#define VMM_CLOCKCHIP_FEAT_PERIODIC 0x000001
#define VMM_CLOCKCHIP_FEAT_ONESHOT  0x000002

struct vmm_clock_chip;
typedef struct vmm_clock_chip vmm_clock_chip_t;

/**
 * Hardware abstraction a clock chip device
 *
 * @head:       List head for registration
 * @name:       ptr to clockchip name
 * @hirq:       host irq number
 * @rating:     variable to rate clock event devices
 * @cpumask:        cpumask to indicate for which CPUs this device works
 * @features:       features
 * @freq:       frequency at which clock event device is running
 * @mult:       nanosecond to cycles multiplier
 * @shift:      nanoseconds to cycles divisor (power of two)
 * @max_delta_ns:   maximum delta value in ns
 * @min_delta_ns:   minimum delta value in ns
 * @event_handler:  Assigned by the framework to be called by the low
 *          level handler of the event source
 * @set_mode:       set mode function
 * @set_next_event: set next event function
 * @mode:       operating mode assigned by the management code
 * @bound_on:       Bound on host CPU
 * @next_event:     local storage for the next event in oneshot mode
 */
struct vmm_clock_chip {
    double_list_t        head; /**< 链表头 */
    const char          *name; /**< 名称 */
    uint32_t             hirq; /**< 主机中断 */
    int                  rating; /**< rating成员 */
    const vmm_cpumask_t *cpumask; /**< CPU掩码 */
    uint32_t             features; /**< 特性 */
    uint32_t             freq; /**< 频率 */
    uint32_t             mult; /**< 乘数/多播 */
    uint32_t             shift; /**< shift成员 */
    uint64_t             max_delta_ns; /**< max_delta_ns成员 */
    uint64_t             min_delta_ns; /**< min_delta_ns成员 */
    void (*event_handler)(vmm_clock_chip_t *cc); /**< event_handler成员 */
    void (*set_mode)(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc); /**< set_mode成员 */
    int (*set_next_event)(uint64_t evt, vmm_clock_chip_t *cc); /**< set_next_event成员 */
    vmm_clock_chip_mode_e mode; /**< 模式 */
    uint32_t              bound_on; /**< bound_on成员 */
    uint64_t              next_event; /**< next_event成员 */
    void *private; /**< 私有数据 */
};

#define VMM_NSEC_PER_SEC 1000000000UL

/* nodeid table based clockchip initialization callback */
typedef int (*vmm_clock_chip_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for clocksource */
#define VMM_CLOCKCHIP_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "clockchip", "", "", compat, fn)

/**
 * @brief 计算时钟缩放运算的乘数/移位因子
 */
static inline void vmm_clocks_calc_mult_shift(uint32_t *mult, uint32_t *shift, uint32_t from, uint32_t to, uint32_t maxsec)
{
    uint64_t tmp;
    uint32_t sft;
    uint32_t sftacc = 32;

    /*
     * Calculate the shift factor which is limiting the conversion
     * range:
     */
    tmp = ((uint64_t)maxsec * from) >> 32;

    while (tmp) {
        tmp >>= 1;
        sftacc--;
    }

    /*
     * Find the conversion shift/mult pair which has the best
     * accuracy and fits the maxsec conversion range:
     */
    for (sft = 32; sft > 0; sft--) {
        tmp = (uint64_t)to << sft;
        tmp += from / 2;
        tmp = udiv64(tmp, from);

        if ((tmp >> sftacc) == 0) {
            break;
        }
    }

    *mult  = tmp;
    *shift = sft;
}

/**
 * @brief 将kHz频率转换为时钟芯片乘数
 */
static inline uint32_t vmm_clock_chip_khz2mult(uint32_t khz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)khz) << shift;
    tmp          = udiv64(tmp, (uint64_t)1000000);
    return (uint32_t)tmp;
}

/**
 * @brief 将Hz频率转换为时钟芯片乘数
 */
static inline uint32_t vmm_clock_chip_hz2mult(uint32_t hz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)hz) << shift;
    tmp          = udiv64(tmp, (uint64_t)1000000000);
    return (uint32_t)tmp;
}

/**
 * @brief 获取时钟芯片的频率
 */
static inline uint32_t vmm_clock_chip_frequency(vmm_clock_chip_t *cc)
{
    return (cc) ? cc->freq : 0;
}

/**
 * @brief 将时钟刻度差值转换为纳秒
 */
static inline uint64_t vmm_clock_chip_delta2ns(uint64_t delta, vmm_clock_chip_t *cc)
{
    uint64_t tmp = (uint64_t)delta << cc->shift;
/**
 * @brief udiv64
 * @param tmp 临时变量
 * @param cc->mult 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return udiv64(tmp, cc->mult);
}

/**
 * @brief 设置时钟芯片的事件处理回调
 * @param cc 时钟芯片结构体指针
 * @param (*event_handler 指针参数
 */
void vmm_clock_chip_set_event_handler(vmm_clock_chip_t *cc, void (*event_handler)(vmm_clock_chip_t *));

/**
 * @brief 编程时钟芯片的定时事件
 * @param cc 时钟芯片结构体指针
 * @param now_ns 当前时间（纳秒）
 * @param expires_ns 到期时间（纳秒）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clock_chip_program_event(vmm_clock_chip_t *cc, uint64_t now_ns, uint64_t expires_ns);

/**
 * @brief 设置时钟芯片的mode
 * @param cc 时钟芯片结构体指针
 * @param mode 操作模式
 */
void vmm_clock_chip_set_mode(vmm_clock_chip_t *cc, vmm_clock_chip_mode_e mode);

/**
 * @brief 注册时钟芯片
 * @param cc 时钟芯片结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clock_chip_register(vmm_clock_chip_t *cc);

/**
 * @brief 注销时钟芯片
 * @param cc 时钟芯片结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clock_chip_unregister(vmm_clock_chip_t *cc);

/**
 * @brief 为时钟芯片绑定最佳时钟源
 * @param host_cpu 主机CPU编号
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_clock_chip_t *vmm_clock_chip_bind_best(uint32_t host_cpu);

/**
 * @brief 解绑时钟芯片
 * @param cc 时钟芯片结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clock_chip_unbind(vmm_clock_chip_t *cc);

/**
 * @brief 获取时钟芯片实例
 * @param index 数组中的索引位置
 * @return 目标对象指针，不存在返回NULL
 */
vmm_clock_chip_t *vmm_clock_chip_get(int index);

/**
 * @brief 获取时钟芯片的数量
 * @return 数量值
 */
uint32_t vmm_clock_chip_count(void);

/**
 * @brief 初始化时钟芯片
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_clock_chip_init(void);

#endif
