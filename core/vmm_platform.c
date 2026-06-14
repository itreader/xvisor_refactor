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
 * @file vmm_platform.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 平台总线实现
 */

#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_msi.h>
#include <vmm_platform.h>

/**
 * @brief 绑定设备的引脚控制（弱函数，可被覆盖）
 * @param dev 设备指针
 * @return VMM_OK 表示成功
 */
int __weak vmm_platform_pinctrl_bind(vmm_device_t *dev)
{
    /* Nothing to do here. */
    /* The pinctrl framework will provide actual implementation */
    return VMM_OK;
}

/**
 * @brief 初始化设备的引脚控制（弱函数，可被覆盖）
 * @param dev 设备指针
 * @return VMM_OK 表示成功
 */
int __weak vmm_platform_pinctrl_init(vmm_device_t *dev)
{
    /* Nothing to do here. */
    /* The pinctrl framework will provide actual implementation */
    return VMM_OK;
}

/**
 * @brief 获取设备的MSI域
 * @param dev 设备指针
 * @param np 设备树节点指针
 * @return MSI域指针，如果失败则返回NULL
 */
static vmm_msi_domain_t *platform_get_msi_domain(vmm_device_t *dev, vmm_device_tree_node_t *np)
{
    int                                 index = 0;
    vmm_msi_domain_t                   *d     = NULL;
    vmm_device_tree_node_t             *msi_np;
    struct vmm_device_tree_phandle_args args;

    if (!dev) {
        return NULL; /**< NULL成员 */
    }

    /* Check for a single msi-parent property */
    msi_np = vmm_device_tree_parse_phandle(np, "msi-parent", 0);

    if (msi_np) {
        if (!vmm_device_tree_getattr(msi_np, "#msi-cells")) {
            d = vmm_msi_find_domain(msi_np, VMM_MSI_DOMAIN_PLATFORM);
        }

        vmm_device_tree_dref_node(msi_np);
        return d;
    }

    /* Check for the complex msi-parent version */
    while (!vmm_device_tree_parse_phandle_with_args(np, "msi-parent", "#msi-cells", index, &args)) {
        d = vmm_msi_find_domain(args.np, VMM_MSI_DOMAIN_PLATFORM);
        vmm_device_tree_dref_node(args.np);

        if (d) {
            return d;
        }

        index++;
    }

    return NULL;
}

/**
 * @brief 配置设备的MSI
 * @param dev 设备指针
 */
static void platform_msi_configure(vmm_device_t *dev)
{
    vmm_device_driver_set_msi_domain(dev, platform_get_msi_domain(dev, dev->of_node));
}

/**
 * @brief 匹配平台总线上的设备和驱动
 * @param dev 设备指针
 * @param drv 驱动指针
 * @return 1表示匹配成功，0表示失败
 */
static int platform_bus_match(vmm_device_t *dev, vmm_driver_t *drv)
{
    const struct vmm_device_tree_nodeid *match;

    if (!dev || !dev->of_node || !drv || !drv->match_table) {
        return 0;
    }

    if (!vmm_device_tree_is_available(dev->of_node)) {
        return 0;
    }

    if (dev->parent && (dev->of_node == dev->parent->of_node)) {
        return 0;
    }

    match = vmm_device_tree_match_node(drv->match_table, dev->of_node);

    if (!match) {
        return 0;
    }

    return 1;
}

/**
 * @brief 探测平台总线上的设备
 * @param dev 设备指针
 * @return VMM_OK表示成功，否则返回错误码
 */
static int platform_bus_probe(vmm_device_t *dev)
{
    int           rc;
    vmm_driver_t *drv;

    if (!dev || !dev->of_node || !dev->driver) {
        return VMM_ERR_FAIL;
    }

    drv = dev->driver;

    if (!drv->match_table) {
        return VMM_ERR_FAIL;
    }

    rc = vmm_platform_pinctrl_bind(dev);

    if (rc == VMM_ERR_PROBE_DEFER) {
        return rc;
    }

    rc = drv->probe(dev);

    vmm_platform_pinctrl_init(dev);

    return rc;
}

/**
 * @brief 移除平台总线上的设备
 * @param dev 设备指针
 * @return VMM_OK表示成功，否则返回错误码
 */
static int platform_bus_remove(vmm_device_t *dev)
{
    vmm_driver_t *drv;

    if (!dev || !dev->of_node || !dev->driver) {
        return VMM_ERR_FAIL;
    }

    drv = dev->driver;

    return drv->remove(dev);
}

/**
 * @brief 释放平台设备资源
 * @param dev 设备指针
 */
static void platform_device_release(vmm_device_t *dev)
{
    vmm_device_tree_dref_node(dev->of_node);
    dev->of_node = NULL;
    vmm_free(dev);
}

/**
 * @brief 平台探测顺序表，定义设备类型的探测优先级
 */
enum platform_probe_order {
    PLATFORM_PROBE_ORDER_IRQCHIP = 0, /**< 0 */
    PLATFORM_PROBE_ORDER_MISC,
    PLATFORM_PROBE_ORDER_MAX
};

/**
 * @brief 获取设备树节点的探测顺序
 * @param node 设备树节点指针
 * @return 探测顺序枚举值
 */
static enum platform_probe_order platform_get_probe_order(vmm_device_tree_node_t *node)
{
    enum platform_probe_order ret = PLATFORM_PROBE_ORDER_MISC;

    if (vmm_device_tree_getattr(node, VMM_DEVICE_TREE_INTERRUPT_CNTRL_ATTR_NAME)) {
        ret = PLATFORM_PROBE_ORDER_IRQCHIP; /**< PLATFORM_PROBE_ORDER_IRQCHIP成员 */
    }

    return ret;
}

/**
 * @brief 探测设备树节点并创建平台设备
 * @param node 设备树节点指针
 * @param parent 父设备指针
 * @return VMM_OK表示成功，否则返回错误码
 */
static int platform_probe(vmm_device_tree_node_t *node, vmm_device_t *parent)
{
    int rc;
    int order;
    vmm_device_t           *dev;
    vmm_device_tree_node_t *child;

    if (!node) {
        return VMM_ERR_FAIL;
    }

    dev = vmm_zalloc(sizeof(vmm_device_t));

    if (!dev) {
        return VMM_ERR_NOMEM;
    }

    vmm_device_driver_initialize_device(dev);

    if (strlcpy(dev->name, node->name, sizeof(dev->name)) >= sizeof(dev->name)) {
        vmm_free(dev);
        return VMM_ERR_OVERFLOW;
    }

    vmm_device_tree_ref_node(node);
    dev->of_node = node;
    dev->parent  = parent;
    dev->bus     = &platform_bus;
    dev->release = platform_device_release;
    dev->private = NULL;

    platform_msi_configure(dev);

    rc = vmm_device_driver_register_device(dev);

    if (rc) {
        vmm_free(dev);
        return rc;
    }

    for (order = 0; order < PLATFORM_PROBE_ORDER_MAX; order++) {
        vmm_device_tree_for_each_child(child, node)
        {
            if (platform_get_probe_order(child) != order) {
                continue;
            }

            platform_probe(child, dev);
        }
    }

    return VMM_OK;
}

vmm_bus_t platform_bus = {
    .name   = "platform",
    .match  = platform_bus_match,
    .probe  = platform_bus_probe,
    .remove = platform_bus_remove,
};

/**
 * @brief 匹配平台设备的节点ID
 * @param dev 设备指针
 * @return 匹配的设备树节点ID，如果失败则返回NULL
 */
const struct vmm_device_tree_nodeid *vmm_platform_match_nodeid(vmm_device_t *dev)
{
    if (!dev || !dev->of_node || !dev->driver || !dev->driver->match_table || (dev->bus != &platform_bus)) {
        return NULL;
    }

    return vmm_device_tree_match_node(dev->driver->match_table, dev->of_node);
}

/**
 * @brief 根据设备树节点查找平台设备
 * @param np 设备树节点指针
 * @return 设备指针，如果未找到则返回NULL
 */
vmm_device_t *vmm_platform_find_device_by_node(vmm_device_tree_node_t *np)
{
    return vmm_device_driver_bus_find_device_by_node(&platform_bus, NULL, np);
}

/**
 * @brief 探测设备树节点作为平台设备
 * @param node 设备树节点指针
 * @return VMM_OK表示成功，否则返回错误码
 */
int vmm_platform_probe(vmm_device_tree_node_t *node)
{
    return platform_probe(node, NULL);
}
