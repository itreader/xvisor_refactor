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
 * @file vmm_load_balancer.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor负载均衡器头文件
 */

#ifndef __VMM_LOAD_BALANCER_H__
#define __VMM_LOAD_BALANCER_H__

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_types.h>

/** Load balancing algo instance */
/**
 * @brief 负载均衡算法接口，定义均衡回调和启动/停止操作
 */
struct vmm_load_balancer_algo {
    double_list_t head; /**< 链表头 */
    uint32_t      rating; /**< rating成员 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    int (*start)(struct vmm_load_balancer_algo *); /**< 起始 */
    void (*balance)(struct vmm_load_balancer_algo *); /**< 平衡 */
    void (*stop)(struct vmm_load_balancer_algo *); /**< 停止 */
    void *private; /**< 私有数据 */
};

/**
 * @brief 设置算法私有数据
 * @param algo 算法结构体指针
 * @param priv 私有数据
 * @return 无返回值
 */
static inline void vmm_load_balancer_set_algo_private(struct vmm_load_balancer_algo *lbalgo, void *private)
{
    if (lbalgo) {
        lbalgo->private = private;
    }
}

static inline void *vmm_load_balancer_get_algo_private(struct vmm_load_balancer_algo *lbalgo)
{
    return (lbalgo) ? lbalgo->private : NULL;
}

/** Current (or best rated) load balancing algo instance
 *  Note: This function must be called from Orphan (or Thread) Context
 */
struct vmm_load_balancer_algo *vmm_load_balancer_current_algo(void);

/**
 * @brief 注册负载均衡算法
 * @param lbalgo 负载均衡算法指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_load_balancer_register_algo(struct vmm_load_balancer_algo *lbalgo);

/**
 * @brief 注销负载均衡算法
 * @param lbalgo 负载均衡算法指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_load_balancer_unregister_algo(struct vmm_load_balancer_algo *lbalgo);

/**
 * @brief 初始化负载均衡器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_load_balancer_init(void);

#endif /* __VMM_LOADBAL_H__ */
