/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_initfn.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 基于设备树的初始化函数头文件
 */
#ifndef _VMM_INITFN_H__
#define _VMM_INITFN_H__

#include <vmm_compiler.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>

/** @brief 基于设备树节点ID表的初始化回调函数类型 */
typedef int (*vmm_initfn_t)(vmm_device_tree_node_t *);

/** @brief 声明最终阶段初始化函数 */
#define VMM_INITFN_DECLARE_FINAL(name, compat, fn)   VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "initfn_final", "", "", compat, fn)

/** @brief 声明早期阶段初始化函数 */
#define VMM_INITFN_DECLARE_EARLY(name, compat, fn)   VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "initfn_early", "", "", compat, fn)

/** @brief 声明初始阶段初始化函数 */
#define VMM_INITFN_DECLARE_NASCENT(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "initfn_nascent", "", "", compat, fn)

/**
 * @brief 调用初始阶段的初始化函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_initfn_nascent(void);

/**
 * @brief 调用早期阶段的初始化函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_initfn_early(void);

/**
 * @brief 调用最终阶段的初始化函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_initfn_final(void);

#endif
