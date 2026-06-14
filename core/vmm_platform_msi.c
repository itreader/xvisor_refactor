/**
 * Copyright (C) 2016 Anup Patel.
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
 * @file vmm_platform_msi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 平台MSI实现
 */

#include <libs/idr.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_msi.h>

#define DEV_ID_SHIFT    21
#define MAX_DEVICE_MSIS (1 << (32 - DEV_ID_SHIFT))

/*
 * Internal data structure containing a (made up, but unique) devid
 * and the callback to write the MSI message.
 */
/**
 * @brief 平台MSI私有数据结构，保存设备ID和消息写入回调
 */
struct vmm_platform_msi_priv_data {
    vmm_device_t           *dev; /**< 设备 */
    void                   *host_data; /**< host_data成员 */
    vmm_msi_alloc_info_t    arg; /**< 参数 */
    vmm_irq_write_msi_msg_t write_msg; /**< write_msg成员 */
    int                     devid; /**< devid成员 */
};

/* The devid allocator */
static DEFINE_IDA(platform_msi_devid_ida);

/*
 * Convert an msi_desc to a globaly unique identifier (per-device
 * devid + msi_desc position in the msi_list).
 */
static uint32_t platform_msi_calc_hw_irq(struct vmm_msi_descriptor *desc)
{
    uint32_t devid = desc->platform.msi_priv_data->devid;

    return (devid << (32 - DEV_ID_SHIFT)) | desc->platform.msi_index;
}

/**
 * @brief 设置平台MSI的中断描述符
 * @param arg 参数值
* @param desc MSI描述符结构体指针
 */
static void platform_msi_set_desc(vmm_msi_alloc_info_t *arg, struct vmm_msi_descriptor *desc)
{
    arg->desc  = desc;
    arg->hw_irq_num = platform_msi_calc_hw_irq(desc);
}

/**
 * @brief 平台MSI写入中断消息
 * @param domain 域结构体指针
 * @param desc MSI描述符结构体指针
 * @param hirq 中断号
 * @param hw_irq_num 硬件中断号
 * @param msg MSI消息结构体
 */
static void platform_msi_write_msg(vmm_msi_domain_t *domain, struct vmm_msi_descriptor *desc, uint32_t hirq, uint32_t hw_irq_num, struct vmm_msi_msg *msg)
{
    desc->platform.msi_priv_data->write_msg(desc, msg);
}

/**
 * @brief 更新平台MSI域的操作回调函数集
 * @param ops 操作集结构体指针
 */
static void platform_msi_update_dom_ops(struct vmm_msi_domain_ops *ops)
{
    if (ops->set_desc == NULL) {
        ops->set_desc = platform_msi_set_desc;
    }

    if (ops->msi_write_msg == NULL) {
        ops->msi_write_msg = platform_msi_write_msg;
    }
}

/**
 * @brief 创建平台MSI中断域
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_msi_domain_t *vmm_platform_msi_create_domain(
    vmm_device_tree_node_t *fwnode, struct vmm_msi_domain_ops *ops, struct vmm_host_irq_domain *parent, uint64_t flags, void *data)
{
    if (!fwnode || !ops || !parent) {
        return NULL;
    }

    if (flags & VMM_MSI_FLAG_USE_DEF_DOM_OPS) {
        platform_msi_update_dom_ops(ops);
    }

    return vmm_msi_create_domain(VMM_MSI_DOMAIN_PLATFORM, fwnode, ops, parent, flags, data);
}

/**
 * @brief 销毁平台MSI中断域
 * @param domain 域结构体指针
 */
void vmm_platform_msi_destroy_domain(vmm_msi_domain_t *domain)
{
    vmm_msi_destroy_domain(domain);
}

/**
 * @brief 为平台MSI分配私有数据
 * @param dev 设备结构体指针
 * @param nvec 向量数量
 * @param write_msi_msg MSI消息写入回调函数
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_platform_msi_priv_data *platform_msi_alloc_priv_data(vmm_device_t *dev, uint32_t nvec, vmm_irq_write_msi_msg_t write_msi_msg)
{
    struct vmm_platform_msi_priv_data *datap;

    /**
 * @brief accordingly
 * @return 成功返回分配的内存指针，失败返回NULL
 */
    /*
     * Limit the number of interrupts to 256 per device. Should we
     * need to bump this up, DEV_ID_SHIFT should be adjusted

     * accordingly (which would impact the max number of MSI
     * capable devices).
     */
    if (!dev->msi_domain || !write_msi_msg || !nvec || nvec > MAX_DEVICE_MSIS) {
        return VMM_ERR_RR_PTR(VMM_ERR_INVALID);
    }

    if (dev->msi_domain->type != VMM_MSI_DOMAIN_PLATFORM) {
        vmm_printf("%s: Incompatible msi_domain, giving up\n", dev->name);
        return VMM_ERR_RR_PTR(VMM_ERR_INVALID);
    }

    /* Already had a helping of MSI? Greed... */
    if (!list_empty(dev_to_msi_list(dev))) {
        return VMM_ERR_RR_PTR(VMM_ERR_BUSY);
    }

    datap = vmm_zalloc(sizeof(*datap));

    if (!datap) {
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    datap->devid = ida_simple_get(&platform_msi_devid_ida, 0, 1 << DEV_ID_SHIFT, 0x0);

    if (datap->devid < 0) {
        int err = datap->devid;
        vmm_free(datap);
        return VMM_ERR_RR_PTR(err);
    }

    datap->write_msg = write_msi_msg;
    datap->dev       = dev;

    return datap;
}

/**
 * @brief 释放平台MSI的私有数据
 * @param d 数据或设备指针
 */
static void platform_msi_free_priv_data(struct vmm_platform_msi_priv_data *d)
{
    ida_simple_remove(&platform_msi_devid_ida, d->devid);
    vmm_free(d);
}

/**
 * @brief 释放平台MSI描述符
 * @param dev 设备结构体指针
 * @param base 起始基地址
 * @param nvec 向量数量
 */
static void platform_msi_free_descs(vmm_device_t *dev, int base, int nvec)
{
    struct vmm_msi_descriptor *desc = NULL;
    struct vmm_msi_descriptor *tmp = NULL;

    list_for_each_entry_safe(desc, tmp, dev_to_msi_list(dev), list)
    {
        if (desc->platform.msi_index >= base && desc->platform.msi_index < (base + nvec)) {
            list_del(&desc->list);
            vmm_free_msi_entry(desc);
        }
    }
}

/**
 * @brief 为平台MSI分配描述符并关联中断
 * @param dev 设备结构体指针
 * @param hirq 主机中断号
 * @param nvec 向量数量
 * @param data 用户自定义数据指针
 * @return 中断处理结果
 */
static int platform_msi_alloc_descs_with_irq(vmm_device_t *dev, int hirq, int nvec, struct vmm_platform_msi_priv_data *data)

{
    struct vmm_msi_descriptor *desc;
    int i;
    int base = 0;

    if (!list_empty(dev_to_msi_list(dev))) {
        desc = list_last_entry(dev_to_msi_list(dev), struct vmm_msi_descriptor, list); /**< list)成员 */
        base = desc->msi_index + 1; /**< 1 */
    }

    for (i = 0; i < nvec; i++) {
        desc = vmm_alloc_msi_entry(dev);

        if (!desc) {
            break;
        }

        desc->platform.msi_priv_data = data;
        desc->msi_index              = base + i;
        desc->nvec_used              = 1;
        desc->hirq                   = hirq ? hirq + i : 0;

        list_add_tail(&desc->list, dev_to_msi_list(dev));
    }

    if (i != nvec) {
        /* Clean up the mess */
        platform_msi_free_descs(dev, base, nvec);

        return VMM_ERR_NOMEM;
    }

    return 0;
}

/**
 * @brief 为平台MSI分配描述符
 * @param dev 设备结构体指针
 * @param nvec 向量数量
 * @param data 用户自定义数据指针
 * @return 中断处理结果
 */
static int platform_msi_alloc_descs(vmm_device_t *dev, int nvec, struct vmm_platform_msi_priv_data *data)

{
    return platform_msi_alloc_descs_with_irq(dev, 0, nvec, data);
}

/**
 * @brief 在平台MSI域中分配中断
 * @param dev 设备结构体指针
 * @param nvec 向量数量
 * @param write_msi_msg MSI消息写入回调函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_platform_msi_domain_alloc_irqs(vmm_device_t *dev, uint32_t nvec, vmm_irq_write_msi_msg_t write_msi_msg)
{
    struct vmm_platform_msi_priv_data *priv_data;
    int                                err;

    priv_data = platform_msi_alloc_priv_data(dev, nvec, write_msi_msg);

    if (VMM_IS_ERR(priv_data)) {
        return VMM_PTR_ERR(priv_data);
    }

    err = platform_msi_alloc_descs(dev, nvec, priv_data);

    if (err) {
        goto out_free_priv_data;
    }

    err = vmm_msi_domain_alloc_irqs(dev->msi_domain, dev, nvec);

    if (err) {
        goto out_free_desc;
    }

    return 0;

out_free_desc:
    platform_msi_free_descs(dev, 0, nvec);
out_free_priv_data:
    platform_msi_free_priv_data(priv_data);

    return err;
}

/**
 * @brief 释放平台MSI域中的中断
 * @param dev 设备结构体指针
 */
void vmm_platform_msi_domain_free_irqs(vmm_device_t *dev)
{
    if (!list_empty(dev_to_msi_list(dev))) {
        struct vmm_msi_descriptor *desc;

        desc = first_msi_entry(dev);
        platform_msi_free_priv_data(desc->platform.msi_priv_data);
    }

    vmm_msi_domain_free_irqs(dev->msi_domain, dev);
    platform_msi_free_descs(dev, 0, MAX_DEVICE_MSIS);
}
