/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_iommu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备直通IOMMU框架实现
 *
 * The source has been largely adapted from Linux sources:
 * drivers/iommu/iommu.c
 *
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * The original source is licensed under GPL.
 */

#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_iommu.h>
#include <vmm_macros.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define pr_debug(msg...) vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

/**
 * @brief IOMMU设备结构，关联设备和其所属的IOMMU域
 */
struct vmm_iommu_device {
    double_list_t list; /**< 链表 */
    vmm_device_t *dev; /**< 设备 */
};

/* =============== IOMMU Controller APIs =============== */

static vmm_class_t iommuctrl_class = {
    .name = VMM_IOMMU_CONTROLLER_CLASS_NAME,
};

/**
 * @brief 注册IOMMU控制器
 * @param ctrl 控制器结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_register(vmm_iommu_controller_t *ctrl)
{
    if (!ctrl) {
        return VMM_ERR_INVALID;
    }

    vmm_device_driver_initialize_device(&ctrl->dev);

    if (strlcpy(ctrl->dev.name, ctrl->name, sizeof(ctrl->dev.name)) >= sizeof(ctrl->dev.name)) {
        return VMM_ERR_OVERFLOW;
    }

    ctrl->dev.class = &iommuctrl_class;
    vmm_device_driver_set_data(&ctrl->dev, ctrl);

    INIT_MUTEX(&ctrl->groups_lock);
    INIT_LIST_HEAD(&ctrl->groups);
    INIT_MUTEX(&ctrl->domains_lock);
    INIT_LIST_HEAD(&ctrl->domains);

    return vmm_device_driver_register_device(&ctrl->dev);
}

/**
 * @brief 注销IOMMU控制器
 * @param ctrl 控制器结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_unregister(vmm_iommu_controller_t *ctrl)
{
    if (!ctrl) {
        return VMM_ERR_FAIL;
    }

    return vmm_device_driver_unregister_device(&ctrl->dev);
}

/**
 * @brief 查找IOMMU控制器
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_iommu_controller_t *vmm_iommu_controller_find(const char *name)
{
    vmm_device_t *dev;

    dev = vmm_device_driver_class_find_device_by_name(&iommuctrl_class, name);

    if (!dev) {
        return NULL;
    }

    return vmm_device_driver_get_data(dev);
}

/**
 * @brief IOMMU控制器遍历上下文结构，私有上下文
 */
struct iommu_controller_iterate_priv {
    void *data; /**< 数据 */
    int (*fn)(vmm_iommu_controller_t *, void *); /**< 函数指针 */
};

/**
 * @brief 遍历所有已注册的IOMMU控制器
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int iommu_controller_iterate(vmm_device_t *dev, void *data)
{
    struct iommu_controller_iterate_priv *p    = data;
    vmm_iommu_controller_t               *ctrl = vmm_device_driver_get_data(dev);

    return p->fn(ctrl, p->data);
}

/**
 * @brief 遍历所有已注册的IOMMU控制器
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_iterate(vmm_iommu_controller_t *start, void *data, int (*fn)(vmm_iommu_controller_t *, void *))
{
    vmm_device_t                        *st = (start) ? &start->dev : NULL;
    struct iommu_controller_iterate_priv p;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&iommuctrl_class, st, &p, iommu_controller_iterate);
}

/**
 * @brief 获取IOMMU控制器的数量
 * @return 数量值
 */
uint32_t vmm_iommu_controller_count(void)
{
    return vmm_device_driver_class_device_count(&iommuctrl_class);
}

/**
 * @brief 遍历IOMMU控制器下的所有IOMMU组
 * @param ctrl 控制器结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_for_each_group(vmm_iommu_controller_t *ctrl, void *data, int (*fn)(vmm_iommu_group_t *, void *))
{
    vmm_iommu_group_t *group;
    int                ret = 0;

    if (!ctrl || !fn) {
        return VMM_ERR_INVALID;
    }

    vmm_mutex_lock(&ctrl->groups_lock);

    list_for_each_entry(group, &ctrl->groups, head)
    {
        ret = fn(group, data);

        if (ret) {
            break;
        }
    }

    vmm_mutex_unlock(&ctrl->groups_lock);

    return ret;
}

/**
 * @brief 遍历IOMMU控制器获取各组的的数量
 * @param group 组结构体指针
 * @param data 用户自定义数据指针
 * @return 数量值
 */
static int iommu_controller_group_count_iter(vmm_iommu_group_t *group, void *data)
{
    (*((uint32_t *)data))++;

    return VMM_OK;
}

/**
 * @brief 获取IOMMU设备组的数量
 * @param ctrl 控制器结构体指针
 * @return 数量值
 */
uint32_t vmm_iommu_controller_group_count(vmm_iommu_controller_t *ctrl)
{
    uint32_t ret = 0;

    if (!ctrl) {
        return 0;
    }

    vmm_iommu_controller_for_each_group(ctrl, &ret, iommu_controller_group_count_iter);

    return ret;
}

/**
 * @brief 遍历IOMMU控制器下的所有IOMMU域
 * @param ctrl 控制器结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_for_each_domain(vmm_iommu_controller_t *ctrl, void *data, int (*fn)(vmm_iommu_domain_t *, void *))
{
    vmm_iommu_domain_t *domain;
    int                 ret = 0;

    if (!ctrl || !fn) {
        return VMM_ERR_INVALID;
    }

    vmm_mutex_lock(&ctrl->domains_lock);

    list_for_each_entry(domain, &ctrl->domains, head)
    {
        ret = fn(domain, data);

        if (ret) {
            break;
        }
    }

    vmm_mutex_unlock(&ctrl->domains_lock);

    return ret;
}

/**
 * @brief 遍历IOMMU控制器获取各域的的数量
 * @param domain 域结构体指针
 * @param data 用户自定义数据指针
 * @return 数量值
 */
static int iommu_controller_domain_count_iter(vmm_iommu_domain_t *domain, void *data)
{
    (*((uint32_t *)data))++;

    return VMM_OK;
}

/**
 * @brief 获取IOMMU域的数量
 * @param ctrl 控制器结构体指针
 * @return 数量值
 */
uint32_t vmm_iommu_controller_domain_count(vmm_iommu_controller_t *ctrl)
{
    uint32_t ret = 0;

    if (!ctrl) {
        return 0;
    }

    vmm_iommu_controller_for_each_domain(ctrl, &ret, iommu_controller_domain_count_iter);

    return ret;
}

/* =============== IOMMU Group APIs =============== */

/**
 * @brief 分配IOMMU设备组
 * @param name 目标对象的名称
 * @param ctrl 控制器结构体指针
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_iommu_group_t *vmm_iommu_group_alloc(const char *name, vmm_iommu_controller_t *ctrl)
{
    vmm_iommu_group_t *group;

    if (!name || !ctrl) {
        return VMM_ERR_RR_PTR(VMM_ERR_INVALID);
    }

    group = vmm_zalloc(sizeof(*group));

    if (!group) {
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    group->name = vmm_zalloc(strlen(name) + 1);

    if (!group->name) {
        vmm_free(group);
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    strcpy(group->name, name);
    group->name[strlen(name)] = '\0';
    group->ctrl               = ctrl;
    INIT_LIST_HEAD(&group->head);

    xref_init(&group->ref_count);
    INIT_MUTEX(&group->mutex);
    INIT_LIST_HEAD(&group->devices);
    group->domain = NULL;
    BLOCKING_INIT_NOTIFIER_CHAIN(&group->notifier);

    vmm_mutex_lock(&ctrl->groups_lock);
    list_add_tail(&group->head, &ctrl->groups);
    vmm_mutex_unlock(&ctrl->groups_lock);

    return group;
}

/**
 * @brief 获取指定名称的IOMMU组
 * @param dev 设备结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_group_t *vmm_iommu_group_get(vmm_device_t *dev)
{
    vmm_iommu_group_t *group = dev->iommu_group;

    if (group) {
        xref_get(&group->ref_count);
    }

    return group;
}

/**
 * @brief 释放IOMMU组资源
 * @param ref 引用计数结构体指针
 */
static void __iommu_group_free(struct xref *ref)
{
    vmm_iommu_group_t *group = container_of(ref, vmm_iommu_group_t, ref_count);

    vmm_mutex_lock(&group->ctrl->groups_lock);
    list_del(&group->head);
    vmm_mutex_unlock(&group->ctrl->groups_lock);

    vmm_free(group->name);

    if (group->iommu_data_release) {
        group->iommu_data_release(group->iommu_data);
    }

    vmm_free(group);
}

/**
 * @brief 释放IOMMU设备组
 * @param group 组结构体指针
 */
void vmm_iommu_group_free(vmm_iommu_group_t *group)
{
    if (group) {
        xref_put(&group->ref_count, __iommu_group_free);
    }
}

/**
 * @brief 获取IOMMU设备组的IOMMU私有数据
 * @param group 组结构体指针
 */
void *vmm_iommu_group_get_iommudata(vmm_iommu_group_t *group)
{
    return (group) ? group->iommu_data : NULL;
}

/**
 * @brief 设置IOMMU设备组的IOMMU私有数据
 * @param group 组结构体指针
 * @param iommu_data IOMMU私有数据指针
 * @param (*release 指针参数
 */
void vmm_iommu_group_set_iommudata(vmm_iommu_group_t *group, void *iommu_data, void (*release)(void *iommu_data))
{
    if (!group) {
        return;
    }

    group->iommu_data         = iommu_data;
    group->iommu_data_release = release;
}

/**
 * @brief 将设备添加到IOMMU组
 * @param group 组结构体指针
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_add_device(vmm_iommu_group_t *group, vmm_device_t *dev)
{
    struct vmm_iommu_device *device;

    if (!group || !dev) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&group->mutex);

    list_for_each_entry(device, &group->devices, list)
    {
        if (device->dev == dev) {
            vmm_mutex_unlock(&group->mutex);
            return VMM_ERR_EXIST;
        }
    }

    device = vmm_zalloc(sizeof(*device));

    if (!device) {
        vmm_mutex_unlock(&group->mutex);
        return VMM_ERR_NOMEM;
    }

    device->dev      = dev;
    dev->iommu_group = group;
    xref_get(&group->ref_count);
    list_add_tail(&device->list, &group->devices);

    vmm_mutex_unlock(&group->mutex);

    /* Notify any listeners about change to group. */
    vmm_blocking_notifier_call(&group->notifier, VMM_IOMMU_GROUP_NOTIFY_ADD_DEVICE, dev);

    return 0;
}

/**
 * @brief 从IOMMU组中移除设备
 * @param dev 设备结构体指针
 */
void vmm_iommu_group_remove_device(vmm_device_t *dev)
{
    vmm_iommu_group_t       *group = dev->iommu_group;
    struct vmm_iommu_device *tmp_device = NULL;
    struct vmm_iommu_device *device = NULL;

    if (!group) {
        return;
    }

    /* Pre-notify listeners that a device is being removed. */
    vmm_blocking_notifier_call(&group->notifier, VMM_IOMMU_GROUP_NOTIFY_DEL_DEVICE, dev);

    vmm_mutex_lock(&group->mutex);

    list_for_each_entry(tmp_device, &group->devices, list)
    {
        if (tmp_device->dev == dev) {
            device = tmp_device;
            list_del(&device->list);
            break;
        }
    }

    vmm_mutex_unlock(&group->mutex);

    if (!device) {
        return;
    }

    vmm_free(device);
    dev->iommu_group = NULL;

    vmm_iommu_group_put(group);
}

/**
 * @brief 遍历IOMMU组中的所有设备
 * @param group 组结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_for_each_dev(vmm_iommu_group_t *group, void *data, int (*fn)(vmm_device_t *, void *))
{
    struct vmm_iommu_device *device;
    int                      ret = 0;

    if (!group || !fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&group->mutex);

    list_for_each_entry(device, &group->devices, list)
    {
        ret = fn(device->dev, data);

        if (ret) {
            break;
        }
    }

    vmm_mutex_unlock(&group->mutex);

    return ret;
}

/**
 * @brief 注册IOMMU组通知器
 * @param group 组结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_register_notifier(vmm_iommu_group_t *group, vmm_notifier_block_t *nb)
{
    if (!group) {
        return VMM_ERR_INVALID;
    }

    return vmm_blocking_notifier_register(&group->notifier, nb);
}

/**
 * @brief 注销IOMMU组通知器
 * @param group 组结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_unregister_notifier(vmm_iommu_group_t *group, vmm_notifier_block_t *nb)
{
    if (!group) {
        return VMM_ERR_INVALID;
    }

    return vmm_blocking_notifier_unregister(&group->notifier, nb);
}

/**
 * @brief 获取IOMMU组的名称
 * @param group 组结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_iommu_group_name(vmm_iommu_group_t *group)
{
    return (group) ? group->name : NULL;
}

/**
 * @brief 获取IOMMU组所属的IOMMU控制器
 * @param group 组结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_controller_t *vmm_iommu_group_controller(vmm_iommu_group_t *group)
{
    return (group) ? group->ctrl : NULL;
}

/*
 * IOMMU groups are really the natrual working unit of the IOMMU, but
 * the IOMMU API works on domains and devices.  Bridge that gap by
 * iterating over the devices in a group.  Ideally we'd have a single
 * device which represents the requestor ID of the group, but we also
 * allow IOMMU drivers to create policy defined minimum sets, where
 * the physical hardware may be able to distiguish members, but we
 * wish to group them at a higher level (ex. untrusted multi-function
 * PCI devices).  Thus we attach each device.
 */
static int iommu_group_do_attach_device(vmm_device_t *dev, void *data)
{
    vmm_iommu_domain_t *domain = data;

    if (unlikely(domain->ops->attach_dev == NULL)) {
        return VMM_ERR_NODEV;
    }

    return domain->ops->attach_dev(domain, dev);
}

/**
 * @brief 执行设备从IOMMU组的分离操作
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int iommu_group_do_detach_device(vmm_device_t *dev, void *data)
{
    vmm_iommu_domain_t *domain = data;

    if (unlikely(domain->ops->detach_dev == NULL)) {
        return VMM_ERR_NODEV;
    }

    domain->ops->detach_dev(domain, dev);

    return VMM_OK;
}

/**
 * @brief 将IOMMU域附加到IOMMU组
 * @param group 组结构体指针
 * @param domain 域结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_attach_domain(vmm_iommu_group_t *group, vmm_iommu_domain_t *domain)
{
    int ret = VMM_OK;

    if (!group || !domain) {
        return VMM_ERR_INVALID;
    }

    vmm_mutex_lock(&group->mutex);

    if (group->domain == domain) {
        ret = VMM_OK;
        goto out_unlock;
    } else if (group->domain != NULL) {
        ret = VMM_ERR_EXIST;
        goto out_unlock;
    }

    ret = vmm_iommu_group_for_each_dev(group, domain, iommu_group_do_attach_device);

    if (ret) {
        goto out_unlock;
    }

    vmm_iommu_domain_ref(domain);
    group->domain = domain;

out_unlock:
    vmm_mutex_unlock(&group->mutex);

    return ret;
}

/**
 * @brief 从IOMMU组分离IOMMU域
 * @param group 组结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_detach_domain(vmm_iommu_group_t *group)
{
    int                 ret = VMM_OK;
    vmm_iommu_domain_t *domain;

    if (!group) {
        return VMM_ERR_INVALID;
    }

    vmm_mutex_lock(&group->mutex);

    domain        = group->domain;
    group->domain = NULL;

    if (!domain) {
        goto out_unlock;
    }

    ret = vmm_iommu_group_for_each_dev(group, domain, iommu_group_do_detach_device);

out_unlock:
    vmm_mutex_unlock(&group->mutex);

    vmm_iommu_domain_dref(domain);

    return ret;
}

/**
 * @brief 获取IOMMU设备组的域
 * @param group 组结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_domain_t *vmm_iommu_group_get_domain(vmm_iommu_group_t *group)
{
    vmm_iommu_domain_t *domain = NULL;

    if (!group) {
        return NULL;
    }

    vmm_mutex_lock(&group->mutex);
    domain = group->domain;
    vmm_iommu_domain_ref(domain);
    vmm_mutex_unlock(&group->mutex);

    return domain;
}

/* =============== IOMMU Domain APIs =============== */

/**
 * @brief 分配IOMMU域
 * @param name 目标对象的名称
 * @param bus 设备总线结构体指针
 * @param ctrl 控制器结构体指针
 * @param type 类型标识值
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_domain_t *vmm_iommu_domain_alloc(const char *name, vmm_bus_t *bus, vmm_iommu_controller_t *ctrl, uint32_t type)
{
    vmm_iommu_domain_t *domain;

    if (bus == NULL || bus->iommu_ops == NULL || ctrl == NULL) {
        return NULL;
    }

    if ((type != VMM_IOMMU_DOMAIN_BLOCKED) && (type != VMM_IOMMU_DOMAIN_IDENTITY) && (type != VMM_IOMMU_DOMAIN_UNMANAGED) &&
        (type != VMM_IOMMU_DOMAIN_DMA)) {
        return NULL;
    }

    domain = bus->iommu_ops->domain_alloc(type, ctrl);

    if (!domain) {
        return NULL;
    }

    if (strlcpy(domain->name, name, sizeof(domain->name)) >= sizeof(domain->name)) {
        vmm_free(domain);
        return NULL;
    }

    INIT_LIST_HEAD(&domain->head);
    domain->type = type;
    domain->ctrl = ctrl;
    xref_init(&domain->ref_count);
    domain->bus = bus;
    domain->ops = bus->iommu_ops;

    vmm_mutex_lock(&ctrl->domains_lock);
    list_add_tail(&domain->head, &ctrl->domains);
    vmm_mutex_unlock(&ctrl->domains_lock);

    return domain;
}

/**
 * @brief 增加IOMMU域的引用计数
 * @param domain 域结构体指针
 */
void vmm_iommu_domain_ref(vmm_iommu_domain_t *domain)
{
    if (domain == NULL) {
        return;
    }

    xref_get(&domain->ref_count);
}

/**
 * @brief 释放IOMMU域
 * @param ref 引用计数结构体指针
 */
static void __iommu_domain_free(struct xref *ref)
{
    vmm_iommu_domain_t *domain = container_of(ref, vmm_iommu_domain_t, ref_count);

    vmm_mutex_lock(&domain->ctrl->domains_lock);
    list_del(&domain->head);
    vmm_mutex_unlock(&domain->ctrl->domains_lock);

    if (likely(domain->ops->domain_free != NULL)) {
        domain->ops->domain_free(domain);
    }
}

/**
 * @brief 释放IOMMU域
 * @param domain 域结构体指针
 */
void vmm_iommu_domain_free(vmm_iommu_domain_t *domain)
{
    if (domain) {
        xref_put(&domain->ref_count, __iommu_domain_free);
    }
}

/**
 * @brief 设置IOMMU的故障处理回调
 * @param domain 域结构体指针
 * @param handler 信号处理函数指针
 * @param token 令牌字符串
 */
void vmm_iommu_set_fault_handler(vmm_iommu_domain_t *domain, vmm_iommu_fault_handler_t handler, void *token)
{
    BUG_ON(!domain);

    domain->handler       = handler;
    domain->handler_token = token;
}

/**
 * @brief 将IOMMU域的IO虚拟地址转换为物理地址
 * @param domain 域结构体指针
 * @param iova IO虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
physical_addr_t vmm_iommu_iova_to_phys(vmm_iommu_domain_t *domain, physical_addr_t iova)
{
    if (unlikely(domain->ops->iova_to_phys == NULL)) {
        return 0;
    }

    return domain->ops->iova_to_phys(domain, iova);
}

/**
 * @brief 获取IOMMU支持的页面大小
 * @param domain 域结构体指针
 * @param addr_merge 待合并的物理地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
static size_t iommu_pgsize(vmm_iommu_domain_t *domain, physical_addr_t addr_merge, size_t size)
{
    uint32_t pgsize_idx;
    size_t   pgsize;

    /* Max page size that still fits into 'size' */
    pgsize_idx = __fls(size);

    /* need to consider alignment requirements ? */
    if (likely(addr_merge)) {
        /* Max page size allowed by address */
        uint32_t align_pgsize_idx = __ffs(addr_merge);
        pgsize_idx                = min(pgsize_idx, align_pgsize_idx);
    }

    /* build a mask of acceptable page sizes */
    pgsize = (1UL << (pgsize_idx + 1)) - 1;

    /* throw away page sizes not supported by the hardware */
    pgsize &= domain->ops->pgsize_bitmap;

    /* make sure we're still sane */
    BUG_ON(!pgsize);

    /* pick the biggest page */
    pgsize_idx = __fls(pgsize);
    pgsize     = 1UL << pgsize_idx;

    return pgsize;
}

/**
 * @brief 建立IOMMU地址映射
 * @param domain 域结构体指针
 * @param iova IO虚拟地址
 * @param paddr 物理地址值
 * @param size 数据大小（字节数）
 * @param prot 内存保护标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_map(vmm_iommu_domain_t *domain, physical_addr_t iova, physical_addr_t paddr, size_t size, int prot)
{
    physical_addr_t orig_iova = iova;
    size_t          min_pagesz;
    size_t          orig_size = size;
    int             ret       = 0;

    if (unlikely(domain->ops->unmap == NULL || domain->ops->pgsize_bitmap == 0UL)) {
        return VMM_ERR_NODEV;
    }

    /* find out the minimum page size supported */
    min_pagesz = 1 << __ffs(domain->ops->pgsize_bitmap);

    /*
     * both the virtual address and the physical one, as well as
     * the size of the mapping, must be aligned (at least) to the
     * size of the smallest page supported by the hardware
     */
    if (!is_aligned(iova | paddr | size, min_pagesz)) {
        vmm_lerror(
            "IOMMU",
            "unaligned iova 0x%" PRIPADDR " pa 0x%" PRIPADDR " size 0x%zx "
            "min_pagesz 0x%zx\n",
            iova, paddr, size, min_pagesz);
        return VMM_ERR_INVALID;
    }

    pr_debug("IOMMU: map iova 0x%" PRIPADDR " pa 0x%" PRIPADDR " size 0x%zx\n", iova, paddr, size);

    while (size) {
        size_t pgsize = iommu_pgsize(domain, iova | paddr, size);

        pr_debug("IOMMU: mapping iova 0x%" PRIPADDR " pa 0x%" PRIPADDR " size 0x%zx\n", iova, paddr, pgsize);

        ret = domain->ops->map(domain, iova, paddr, pgsize, prot);

        if (ret) {
            break;
        }

        iova += pgsize;
        paddr += pgsize;
        size -= pgsize;
    }

    /* unroll mapping in case something went wrong */
    if (ret) {
        vmm_iommu_unmap(domain, orig_iova, orig_size - size);
    }

    return ret;
}

/**
 * @brief IOMMU解除地址映射
 * @param domain 域结构体指针
 * @param iova IO虚拟地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
size_t vmm_iommu_unmap(vmm_iommu_domain_t *domain, physical_addr_t iova, size_t size)
{
    size_t unmapped_page;
    size_t min_pagesz;
    size_t unmapped = 0;

    if (unlikely(domain->ops->unmap == NULL || domain->ops->pgsize_bitmap == 0UL)) {
        return VMM_ERR_NODEV;
    }

    /* find out the minimum page size supported */
    min_pagesz = 1 << __ffs(domain->ops->pgsize_bitmap);

    /*
     * The virtual address, as well as the size of the mapping, must be
     * aligned (at least) to the size of the smallest page supported
     * by the hardware
     */
    if (!is_aligned(iova | size, min_pagesz)) {
        vmm_lerror("IOMMU", "unaligned iova 0x%" PRIPADDR " size 0x%zx min_pagesz 0x%zx\n", iova, size, min_pagesz);
        return VMM_ERR_INVALID;
    }

    pr_debug("IOMMU: unmap iova 0x%" PRIPADDR " size 0x%zx\n", iova, size);

    /*
     * Keep iterating until we either unmap 'size' bytes (or more)
     * or we hit an area that isn't mapped.
     */
    while (unmapped < size) {
        size_t pgsize = iommu_pgsize(domain, iova, size - unmapped);

        unmapped_page = domain->ops->unmap(domain, iova, pgsize);

        if (!unmapped_page) {
            break;
        }

        pr_debug("IOMMU: unmapped iova 0x%" PRIPADDR " size 0x%zx\n", iova, unmapped_page);

        iova += unmapped_page;
        unmapped += unmapped_page;
    }

    return unmapped;
}

/**
 * @brief 启用IOMMU域的DMA窗口
 * @param domain 域结构体指针
 * @param wnd_nr 窗口编号
 * @param paddr 物理地址值
 * @param size 数据大小（字节数）
 * @param prot 内存保护标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_domain_window_enable(vmm_iommu_domain_t *domain, uint32_t wnd_nr, physical_addr_t paddr, uint64_t size, int prot)
{
    if (unlikely(domain->ops->domain_window_enable == NULL)) {
        return VMM_ERR_NODEV;
    }

    return domain->ops->domain_window_enable(domain, wnd_nr, paddr, size, prot);
}

/**
 * @brief 禁用IOMMU域的DMA窗口
 * @param domain 域结构体指针
 * @param wnd_nr 窗口编号
 */
void vmm_iommu_domain_window_disable(vmm_iommu_domain_t *domain, uint32_t wnd_nr)
{
    if (unlikely(domain->ops->domain_window_disable == NULL)) {
        return;
    }

    return domain->ops->domain_window_disable(domain, wnd_nr);
}

/**
 * @brief 获取IOMMU域的attr
 * @param domain 域结构体指针
 * @param attr 属性结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_domain_get_attr(vmm_iommu_domain_t *domain, enum vmm_iommu_attr attr, void *data)
{
    struct vmm_iommu_domain_geometry *geometry;
    bool                             *paging;
    int                               ret = 0;
    uint32_t                         *count;

    switch (attr) {
        case VMM_DOMAIN_ATTR_GEOMETRY:
            geometry  = data;
            *geometry = domain->geometry;

            break;

        case VMM_DOMAIN_ATTR_PAGING:
            paging  = data;
            *paging = (domain->ops->pgsize_bitmap != 0UL);
            break;

        case VMM_DOMAIN_ATTR_WINDOWS:
            count = data;

            if (domain->ops->domain_get_windows != NULL) {
                *count = domain->ops->domain_get_windows(domain);
            } else {
                ret = VMM_ERR_NODEV;
            }

            break;

        default:
            if (!domain->ops->domain_get_attr) {
                return VMM_ERR_INVALID;
            }

            ret = domain->ops->domain_get_attr(domain, attr, data);
    }

    return ret;
}

/**
 * @brief 设置IOMMU域的attr
 * @param domain 域结构体指针
 * @param attr 属性结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_domain_set_attr(vmm_iommu_domain_t *domain, enum vmm_iommu_attr attr, void *data)
{
    int       ret = 0;
    uint32_t *count;

    switch (attr) {
        case VMM_DOMAIN_ATTR_WINDOWS:
            count = data;

            if (domain->ops->domain_set_windows != NULL) {
                ret = domain->ops->domain_set_windows(domain, *count);
            } else {
                ret = VMM_ERR_NODEV;
            }

            break;

        default:
            if (domain->ops->domain_set_attr == NULL) {
                return VMM_ERR_INVALID;
            }

            ret = domain->ops->domain_set_attr(domain, attr, data);
    }

    return ret;
}

/**
 * @brief 将设备添加到IOMMU组中
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int add_iommu_group(vmm_device_t *dev, void *data)
{
    vmm_iommu_ops_t *ops = data;

    if (!ops->add_device) {
        return VMM_ERR_NODEV;
    }

    WARN_ON(dev->iommu_group);

    ops->add_device(dev);

    return 0;
}

/**
 * @brief IOMMU总线事件通知器回调
 * @param nb 通知器块指针
 * @param action 动作标识值
 * @param data 用户自定义数据指针
 * @return 通知结果
 */
static int iommu_bus_notifier(vmm_notifier_block_t *nb, uint64_t action, void *data)
{
    vmm_device_t      *dev = data;
    vmm_iommu_ops_t   *ops = dev->bus->iommu_ops;
    vmm_iommu_group_t *group;
    uint64_t           group_action = 0;

    /*
     * ADD/DEL call into iommu driver ops if provided, which may
     * result in ADD/DEL notifiers to group->notifier
     */
    if (action == VMM_BUS_NOTIFY_ADD_DEVICE) {
        if (ops->add_device) {
            return ops->add_device(dev);
        }
    } else if (action == VMM_BUS_NOTIFY_DEL_DEVICE) {
        if (ops->remove_device && dev->iommu_group) {
            ops->remove_device(dev);
            return 0;
        }
    }

    /*
     * Remaining BUS_NOTIFYs get filtered and republished to the
     * group, if anyone is listening
     */
    group = vmm_iommu_group_get(dev);

    if (!group) {
        return 0;
    }

    switch (action) {
        case VMM_BUS_NOTIFY_BIND_DRIVER:
            group_action = VMM_IOMMU_GROUP_NOTIFY_BIND_DRIVER;
            break;

        case VMM_BUS_NOTIFY_BOUND_DRIVER:
            group_action = VMM_IOMMU_GROUP_NOTIFY_BOUND_DRIVER;
            break;

        case VMM_BUS_NOTIFY_UNBIND_DRIVER:
            group_action = VMM_IOMMU_GROUP_NOTIFY_UNBIND_DRIVER;
            break;

        case VMM_BUS_NOTIFY_UNBOUND_DRIVER:
            group_action = VMM_IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER;
            break;
    }

    if (group_action) {
        vmm_blocking_notifier_call(&group->notifier, group_action, dev);
    }

    vmm_iommu_group_put(group);
    return 0;
}

/* =============== IOMMU Misc APIs =============== */

static vmm_notifier_block_t iommu_bus_nb = {
    .notifier_call = iommu_bus_notifier,
};

/**
 * @brief 初始化IOMMU总线子系统
 * @param bus 设备总线结构体指针
 * @param ops 操作集结构体指针
 */
static void iommu_bus_init(vmm_bus_t *bus, vmm_iommu_ops_t *ops)
{
    vmm_device_driver_bus_register_notifier(bus, &iommu_bus_nb);
    vmm_device_driver_bus_device_iterate(bus, NULL, ops, add_iommu_group);
}

/**
 * @brief 设置总线的IOMMU
 * @param bus 设备总线结构体指针
 * @param ops 操作集结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_bus_set_iommu(vmm_bus_t *bus, vmm_iommu_ops_t *ops)
{
    if (bus->iommu_ops != NULL) {
        return VMM_ERR_BUSY;
    }

    bus->iommu_ops = ops;

    /* Do IOMMU specific setup for this bus-type */
    iommu_bus_init(bus, ops);

    return VMM_OK;
}

/**
 * @brief 检查IOMMU是否存在
 * @param bus 设备总线结构体指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_iommu_present(vmm_bus_t *bus)
{
    return bus->iommu_ops != NULL;
}

/**
 * @brief 检查IOMMU是否具备指定能力
 * @param bus 设备总线结构体指针
 * @param cap 能力值或容量值
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_iommu_capable(vmm_bus_t *bus, enum vmm_iommu_cap cap)
{
    if (!bus->iommu_ops || !bus->iommu_ops->capable) {
        return FALSE;
    }

    return bus->iommu_ops->capable(cap);
}

/**
 * @brief 查找IOMMU节点ID表
 * @param node 设备树节点指针
 * @param match 匹配回调函数
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __init iommu_nidtable_found(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data)
{
    int              err;
    vmm_iommu_init_t init_fn = match->data;

    if (!init_fn) {
        return;
    }

    err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE

    if (err) {
        vmm_printf("%s: Init %s node failed (error %d)\n", __func__, node->name, err);
    }

#else
    (void)err;
#endif
}

/**
 * @brief 初始化IOMMU
 * @return 编号值
 */
int __init vmm_iommu_init(void)
{
    int                                  ret;
    const struct vmm_device_tree_nodeid *iommu_matches;

    ret = vmm_device_driver_register_class(&iommuctrl_class);

    if (ret) {
        return ret;
    }

    /* Probe all device tree nodes matching
     * IOMMU nodeid table enteries.
     */
    iommu_matches = vmm_device_tree_nidtable_create_matches("iommu");

    if (iommu_matches) {
        vmm_device_tree_iterate_matching(NULL, iommu_matches, iommu_nidtable_found, NULL);
    }

    return VMM_OK;
}
