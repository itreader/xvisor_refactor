/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file vmm_profiler.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Hypervisor性能分析器头文件
 */

#ifndef _VMM_PROFILER_H__
#define _VMM_PROFILER_H__

#include <vmm_types.h>

#define VMM_PROFILE_ARRAY_SIZE   15
#define VMM_PROFILE_OTHER_INDEX  (VMM_PROFILE_ARRAY_SIZE - 1)
#define VMM_PROFILE_OTHER_PARENT 0xffffffff

/**
 * @brief 性能分析计数器，记录特定事件的累计值
 */
typedef struct vmm_profiler_counter {
    uint32_t   index; /**< 索引 */
    uint32_t   parent_index; /**< parent_index成员 */
    atomic_t   count; /**< 计数 */
    atomic64_t total_time; /**< total_time成员 */
    atomic64_t time_per_call; /**< time_per_call成员 */
} vmm_profiler_counter_t;

/**
 * @brief 性能分析统计结构，保存采样数据和计数结果
 */
struct vmm_profiler_stat {
    vmm_profiler_counter_t counter[VMM_PROFILE_ARRAY_SIZE]; /**< counter成员 */
};

/**
 * @brief 检查性能分析器是否处于活动状态
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_profiler_isactive(void);

/**
 * @brief 启动性能分析器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_profiler_start(void);

/**
 * @brief 停止性能分析器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_profiler_stop(void);

/**
 * Get the base of the stat data array
 */
struct vmm_profiler_stat *vmm_profiler_get_stat_array(void);

/**
 * @brief 初始化性能分析器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_profiler_init(void);

#endif
