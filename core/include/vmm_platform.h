/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vmm_platform.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 平台总线接口头文件
 */

#ifndef __VMM_PLATFORM_H_
#define __VMM_PLATFORM_H_

#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>

/** Forward declaration of platform bus */
extern vmm_bus_t platform_bus;

/**
 * @brief 绑定平台的引脚控制器
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_platform_pinctrl_bind(vmm_device_t *dev);

/**
 * @brief 初始化平台引脚控制
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_platform_pinctrl_init(vmm_device_t *dev);

/**
 * @brief 匹配平台的设备树节点ID
 * @param dev 设备结构体指针
 * @return 成功返回目标指针，失败返回NULL
 */
const vmm_device_tree_nodeid_t *vmm_platform_match_nodeid(vmm_device_t *dev);

/**
 * @brief 根据设备树节点查找平台设备
 * @param np 设备树节点指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_platform_find_device_by_node(vmm_device_tree_node_t *np);

/**
 * @brief 平台 探测
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_platform_probe(vmm_device_tree_node_t *node);

#endif /* __VMM_PLATFORM_H_ */
