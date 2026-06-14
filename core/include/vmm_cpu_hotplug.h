/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file vmm_cpu_hotplug.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU热插拔通知器接口
 */

#ifndef __VMM_CPU_HOTPLUG_H__
#define __VMM_CPU_HOTPLUG_H__

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_types.h>

/**
 * @brief CPU热插拔状态枚举，定义CPU从离线到在线的状态转换
 */
enum vmm_cpu_hotplug_states {
    VMM_CPU_HOTPLUG_STATE_OFFLINE = 0, /**< 0 */
    VMM_CPU_HOTPLUG_STATE_HOST_IRQ,
    VMM_CPU_HOTPLUG_STATE_CLOCKSOURCE,
    VMM_CPU_HOTPLUG_STATE_CLOCKCHIP,
    VMM_CPU_HOTPLUG_STATE_TIMER,
    VMM_CPU_HOTPLUG_STATE_DELAY,
    VMM_CPU_HOTPLUG_STATE_SMP_SYNC_IPI,
    VMM_CPU_HOTPLUG_STATE_SCHEDULER,
    VMM_CPU_HOTPLUG_STATE_SMP_ASYNC_IPI,
    VMM_CPU_HOTPLUG_STATE_WORKQUEUE
};

#define VMM_CPU_HOTPLUG_STATE_ONLINE U32_MAX

struct vmm_cpu_hotplug_notify;
typedef struct vmm_cpu_hotplug_notify vmm_cpu_hotplug_notify_t;

/**
 * @brief CPU热插拔通知结构，封装热插拔事件的通知回调
 */
struct vmm_cpu_hotplug_notify {
    /* Private */
    double_list_t               head; /**< 链表头 */
    /* Public */
    enum vmm_cpu_hotplug_states state; /**< 状态 */
    char                        name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    int (*startup)(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu); /**< startup成员 */
    int (*teardown)(vmm_cpu_hotplug_notify_t *cpu_hotplug, uint32_t cpu); /**< 拆除回调 */
};

/**
 * @brief 获取CPU热插拔的状态
 * @param cpu CPU编号
 * @return CPU热插拔状态标志
 */
uint32_t vmm_cpu_hotplug_get_state(uint32_t cpu);

/* Set specified hotplug state for current CPU */
/**
 * @brief 设置CPU热插拔的状态
 * @param state 状态值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_set_state(uint32_t state);

/**
 * @brief 注册CPU热插拔
 * @param cpu_hotplug CPU热插拔结构体指针
 * @param invoke_startup 是否调用启动函数标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_register(vmm_cpu_hotplug_notify_t *cpu_hotplug, bool invoke_startup);

/**
 * @brief 注销CPU热插拔
 * @param cpu_hotplug CPU热插拔结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_unregister(vmm_cpu_hotplug_notify_t *cpu_hotplug);

/**
 * @brief 初始化CPU热插拔
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cpu_hotplug_init(void);

#endif /* __VMM_CPU_HOTPLUG_H__ */
