/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_netport.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief 网络交换机端口框架
 */

#include <libs/stringlib.h>
#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_protocol.h>
#include <vmm_device_driver.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

struct vmm_netport *vmm_netport_alloc(char *name, uint32_t queue_size)
{
    struct vmm_netport *port; /**< 端口 */

    port = vmm_zalloc(sizeof(struct vmm_netport)); /**< vmm_netport))成员 */

    if (!port) {
        vmm_printf("%s Failed to allocate net port\n", __func__); /**< __func__)成员 */
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&port->head);

    if (strlcpy(port->name, name, sizeof(port->name)) >= sizeof(port->name)) {
        vmm_free(port);
        return NULL; /**< NULL成员 */
    };

    port->queue_size = (queue_size < VMM_NETPORT_MAX_QUEUE_SIZE) ? queue_size : VMM_NETPORT_MAX_QUEUE_SIZE; /**< VMM_NETPORT_MAX_QUEUE_SIZE成员 */

    INIT_SPIN_LOCK(&port->switch2port_xfer_lock);

    return port; /**< 端口 */
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_alloc);

/**
 * @brief 释放网络端口
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netport_free(struct vmm_netport *port)
{
    if (!port) {
        return VMM_ERR_FAIL;
    }

    vmm_free(port);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_free);

static vmm_class_t netport_class = {
    .name = VMM_NETPORT_CLASS_NAME,
};

/**
 * @brief 注册网络端口
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netport_register(struct vmm_netport *port)
{
    int rc;

    if (port == NULL) {
        return VMM_ERR_FAIL;
    }

    /* If port has invalid mac, assign a random one */
    if (!is_valid_ether_addr(port->macaddr)) {
        random_ether_addr(port->macaddr);
    }

    vmm_device_driver_initialize_device(&port->dev);

    if (strlcpy(port->dev.name, port->name, sizeof(port->dev.name)) >= sizeof(port->dev.name)) {
        return VMM_ERR_OVERFLOW;
    }

    port->dev.class = &netport_class;
    vmm_device_driver_set_data(&port->dev, port);

    rc = vmm_device_driver_register_device(&port->dev);

    if (rc != VMM_OK) {
        vmm_printf("%s: Failed to register %s %s (error %d)\n", __func__, VMM_NETPORT_CLASS_NAME, port->name, rc);
        return rc;
    }

#ifdef CONFIG_VERBOSE_MODE
    vmm_printf("%s: Registered netport %s\n", __func__, port->name);
#endif

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_register);

/**
 * @brief 注销网络端口
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netport_unregister(struct vmm_netport *port)
{
    int rc;

    if (!port) {
        return VMM_ERR_FAIL;
    }

    rc = vmm_netswitch_port_remove(port);

    if (rc) {
        return rc;
    }

    return vmm_device_driver_unregister_device(&port->dev);
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_unregister);

struct vmm_netport *vmm_netport_find(const char *name)
{
    vmm_device_t *dev; /**< 设备 */

    dev = vmm_device_driver_class_find_device_by_name(&netport_class, name); /**< name)成员 */

    if (!dev) {
        return NULL; /**< NULL成员 */
    }

    return vmm_device_driver_get_data(dev); /**< vmm_device_driver_get_data(dev)成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_find);

/**
 * @brief 网络端口遍历上下文结构，私有上下文
 */
struct netport_iterate_priv {
    void *data; /**< 数据 */
    int (*fn)(struct vmm_netport *port, void *data); /**< 函数指针 */
};

/**
 * @brief 网络端口 遍历
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int netport_iterate(vmm_device_t *dev, void *data)
{
    struct netport_iterate_priv *p    = data;
    struct vmm_netport          *port = vmm_device_driver_get_data(dev);

    return p->fn(port, p->data);
}

/**
 * @brief 网络端口 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netport_iterate(struct vmm_netport *start, void *data, int (*fn)(struct vmm_netport *dev, void *data))
{
    vmm_device_t               *st = (start) ? &start->dev : NULL;
    struct netport_iterate_priv p;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&netport_class, st, &p, netport_iterate);
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_iterate);

/**
 * @brief 获取网络端口的数量
 * @return 数量值
 */
uint32_t vmm_netport_count(void)
{
    return vmm_device_driver_class_device_count(&netport_class);
}

VMM_ERR_XPORT_SYMBOL(vmm_netport_count);

/**
 * @brief 初始化网络端口
 * @return 数量值
 */
int __init vmm_netport_init(void)
{
    int rc;

    vmm_init_printf("network port framework\n");

    rc = vmm_device_driver_register_class(&netport_class);

    if (rc) {
        vmm_printf("Failed to register %s class\n", VMM_NETPORT_CLASS_NAME);
        return rc;
    }

    return VMM_OK;
}

/**
 * @brief 网络端口子系统退出
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __exit vmm_netport_exit(void)
{
    int rc;

    rc = vmm_device_driver_unregister_class(&netport_class);

    if (rc) {
        vmm_printf("Failed to unregister %s class", VMM_NETPORT_CLASS_NAME);
        return rc;
    }

    return VMM_OK;
}
