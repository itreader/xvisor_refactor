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
 * @file vmm_initfn.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 基于设备树的初始化函数实现
 */

#include <vmm_error.h>
#include <vmm_initfn.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>

/**
 * @brief 查找初始化函数节点ID表
 * @param node 设备树节点指针
 * @param match 匹配回调函数
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __init initfn_nidtable_found(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int          err;
    vmm_initfn_t init_fn = match->data;

    if (!init_fn) {
        return;
    }

    err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE

    if (err) {
        vmm_printf("%s: CPU%d Init %s node failed (error %d)\n", __func__, vmm_smp_processor_id(), node->name, err);
    }

#else
    (void)err;
#endif
}

/**
 * @brief 执行初始化函数
 * @param subsys 子系统名称字符串
 * @return 编号值
 */
static int initfn_do(const char *subsys)
{
    const struct vmm_device_tree_nodeid *matches;

    matches = vmm_device_tree_nidtable_create_matches(subsys);

    if (!matches) {
        return VMM_OK;
    }

    vmm_device_tree_iterate_matching(NULL, matches, initfn_nidtable_found, NULL);

    if (matches) {
        vmm_device_tree_nidtable_destroy_matches(matches);
    }

    return VMM_OK;
}

/**
 * @brief 执行nascent阶段初始化函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_initfn_nascent(void)
{
    return initfn_do("initfn_nascent");
}

/**
 * @brief 执行early阶段初始化函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_initfn_early(void)
{
    return initfn_do("initfn_early");
}

/**
 * @brief 执行final阶段初始化函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_initfn_final(void)
{
    return initfn_do("initfn_final");
}
