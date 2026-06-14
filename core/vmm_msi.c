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
 * @file vmm_msi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 主机MSI框架通用实现
 */

#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_msi.h>
#include <vmm_spinlocks.h>

static DEFINE_SPINLOCK(msi_lock);
static LIST_HEAD(msi_domain_list);

/**
 * @brief MSI域操作初始化回调
 * @param domain 域结构体指针
 * @param hirq 中断号
 * @param hw_irq_num 数量
 * @param arg 参数值
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int msi_domain_ops_init(vmm_msi_domain_t *domain, uint32_t hirq, uint32_t hw_irq_num, vmm_msi_alloc_info_t *arg)
{
    return 0;
}

/**
 * @brief MSI域操作释放回调
 * @param domain 域结构体指针
 * @param hirq 中断号
 */
static void msi_domain_ops_free(vmm_msi_domain_t *domain, uint32_t hirq) {}

/**
 * @brief MSI域操作检查回调
 * @param domain 域结构体指针
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int msi_domain_ops_check(vmm_msi_domain_t *domain, vmm_device_t *dev)
{
    return 0;
}

/**
 * @brief MSI域操作准备回调
 * @param domain 域结构体指针
 * @param dev 设备结构体指针
 * @param nvec 向量数量
 * @param arg 参数值
 * @return 中断处理结果
 */
static int msi_domain_ops_prepare(vmm_msi_domain_t *domain, vmm_device_t *dev, int nvec, vmm_msi_alloc_info_t *arg)
{
    memset(arg, 0, sizeof(*arg));
    return 0;
}

/**
 * @brief MSI域操作完成回调
 * @param arg 参数值
 * @param retval 返回值指针
 */
static void msi_domain_ops_finish(vmm_msi_alloc_info_t *arg, int retval) {}

/**
 * @brief MSI域设置中断描述符回调
 * @param arg 参数值
* @param desc MSI描述符结构体指针
 */
static void msi_domain_ops_set_desc(vmm_msi_alloc_info_t *arg, struct vmm_msi_descriptor *desc)
{
    arg->desc = desc;
}

/**
 * @brief MSI域处理错误回调
 * @param domain 域结构体指针
* @param desc MSI描述符结构体指针
 * @param error 错误码值
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int msi_domain_ops_handle_error(vmm_msi_domain_t *domain, struct vmm_msi_descriptor *desc, int error)
{
    return error;
}

/**
 * @brief MSI域写入消息回调
 */
static void msi_domain_ops_write_msg(
    vmm_msi_domain_t *domain, struct vmm_msi_descriptor *desc, uint32_t hirq, uint32_t hw_irq_num, struct vmm_msi_msg *msg)
{
}

static struct vmm_msi_domain_ops msi_domain_ops_default = {
    .msi_init      = msi_domain_ops_init,
    .msi_free      = msi_domain_ops_free,
    .msi_check     = msi_domain_ops_check,
    .msi_prepare   = msi_domain_ops_prepare,
    .msi_finish    = msi_domain_ops_finish,
    .set_desc      = msi_domain_ops_set_desc,
    .handle_error  = msi_domain_ops_handle_error,
    .msi_write_msg = msi_domain_ops_write_msg,
};

/**
 * @brief 更新MSI域的操作回调函数集
 * @param domain 域结构体指针
 */
static void vmm_msi_domain_update_dom_ops(vmm_msi_domain_t *domain)
{
    struct vmm_msi_domain_ops *ops = domain->ops;

    if (ops == NULL) {
        domain->ops = &msi_domain_ops_default; /**< &msi_domain_ops_default成员 */
        return;
    }

    if (ops->msi_init == NULL) {
        ops->msi_init = msi_domain_ops_default.msi_init;
    }

    if (ops->msi_free == NULL) {
        ops->msi_free = msi_domain_ops_default.msi_free;
    }

    if (ops->msi_check == NULL) {
        ops->msi_check = msi_domain_ops_default.msi_check;
    }

    if (ops->msi_prepare == NULL) {
        ops->msi_prepare = msi_domain_ops_default.msi_prepare;
    }

    if (ops->msi_finish == NULL) {
        ops->msi_finish = msi_domain_ops_default.msi_finish;
    }

    if (ops->set_desc == NULL) {
        ops->set_desc = msi_domain_ops_default.set_desc;
    }

    if (ops->handle_error == NULL) {
        ops->handle_error = msi_domain_ops_default.handle_error;
    }

    if (ops->msi_write_msg == NULL) {
        ops->msi_write_msg = msi_domain_ops_default.msi_write_msg;
    }
}

struct vmm_msi_descriptor *vmm_alloc_msi_entry(vmm_device_t *dev)
{
    struct vmm_msi_descriptor *desc = vmm_zalloc(sizeof(*desc)); /**< 描述 */

    if (!desc) {
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&desc->list);
    desc->dev = dev; /**< 设备 */

    return desc; /**< 描述 */
}

/**
 * @brief 释放MSI描述符条目
 * @param entry 条目指针
 */
void vmm_free_msi_entry(struct vmm_msi_descriptor *entry)
{
    vmm_free(entry);
}

/**
 * @brief 创建MSI中断域
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_msi_domain_t *vmm_msi_create_domain(
    enum vmm_msi_domain_types type, vmm_device_tree_node_t *fwnode, struct vmm_msi_domain_ops *ops, struct vmm_host_irq_domain *parent,
    uint64_t flags, void *data)
{
    irq_flags_t       f; /**< f */
    bool              found = FALSE; /**< FALSE成员 */
    vmm_msi_domain_t *domain, *d; /**< d */

    if (type <= VMM_MSI_DOMAIN_UNKNOWN || VMM_MSI_DOMAIN_MAX <= type) {
        return NULL; /**< NULL成员 */
    }

    if (!fwnode || !ops || !parent) {
        return NULL; /**< NULL成员 */
    }

    domain = vmm_zalloc(sizeof(*domain)); /**< 域 */

    if (!domain) {
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&domain->head);
    domain->type = type; /**< 类型 */
    vmm_device_tree_ref_node(fwnode);
    domain->fwnode = fwnode; /**< 固件设备树节点 */
    domain->ops    = ops; /**< 操作集 */
    domain->parent = parent; /**< 父节点 */
    domain->flags  = flags; /**< 标志位 */
    domain->data   = data; /**< 数据 */

    vmm_spin_lock_irq_save_lite(&msi_lock, f); /**< f) */

    list_for_each_entry(d, &msi_domain_list, head)
    {
        if (d->fwnode == fwnode && d->type == type) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore_lite(&msi_lock, f); /**< f) */
        vmm_device_tree_dref_node(domain->fwnode);
        vmm_free(domain);
        return NULL; /**< NULL成员 */
    }

    list_add_tail(&domain->head, &msi_domain_list); /**< &msi_domain_list)成员 */

    vmm_spin_unlock_irq_restore_lite(&msi_lock, f); /**< f) */

    if (domain->flags & VMM_MSI_FLAG_USE_DEF_DOM_OPS) {
        vmm_msi_domain_update_dom_ops(domain);
    }

    return domain; /**< 域 */
}

/**
 * @brief 销毁MSI中断域
 * @param domain 域结构体指针
 */
void vmm_msi_destroy_domain(vmm_msi_domain_t *domain)
{
    irq_flags_t       f;
    bool              found = FALSE;
    vmm_msi_domain_t *d;

    if (!domain) {
        return;
    }

    vmm_spin_lock_irq_save_lite(&msi_lock, f);

    list_for_each_entry(d, &msi_domain_list, head)
    {
        if (d == domain) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore_lite(&msi_lock, f);
        return;
    }

    list_del(&domain->head);

    vmm_spin_unlock_irq_restore_lite(&msi_lock, f);

    vmm_device_tree_dref_node(domain->fwnode);
    vmm_free(domain);
}

/**
 * @brief 查找消息信号中断域
 * @param fwnode 固件节点指针
 * @param type 类型标识值
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_msi_domain_t *vmm_msi_find_domain(vmm_device_tree_node_t *fwnode, enum vmm_msi_domain_types type)
{
    irq_flags_t       f;
    vmm_msi_domain_t *d = NULL;
    vmm_msi_domain_t *domain = NULL;

    if (!fwnode) {
        return NULL;
    }

    vmm_spin_lock_irq_save_lite(&msi_lock, f);

    list_for_each_entry(d, &msi_domain_list, head)
    {
        if (d->fwnode == fwnode && d->type == type) {
            domain = d;
            break;
        }
    }

    vmm_spin_unlock_irq_restore_lite(&msi_lock, f);

    return domain;
}

/**
 * @brief 向MSI域写入中断消息
 * @param irq 指向主机中断结构体的指针
 */
void vmm_msi_domain_write_msg(vmm_host_irq_t *irq)
{
    struct vmm_msi_descriptor *desc = vmm_host_irq_get_msi_data(irq);
    struct vmm_msi_domain_ops *ops;
    vmm_msi_domain_t          *domain;
    int                        ret;

    if (!desc || !desc->domain) {
        return;
    }

    domain = desc->domain;
    ops    = domain->ops;

    memset(&desc->msg, 0, sizeof(desc->msg));
    ret = vmm_host_irq_compose_msi_msg(irq->num, &desc->msg);
    BUG_ON(ret < 0);
    ops->msi_write_msg(domain, desc, irq->num, irq->hw_irq_num, &desc->msg);
}

/**
 * @brief 在MSI域中为设备分配中断请求
 * @param domain 域结构体指针
 * @param dev 设备结构体指针
 * @param nvec 向量数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_msi_domain_alloc_irqs(vmm_msi_domain_t *domain, vmm_device_t *dev, int nvec)
{
    vmm_msi_alloc_info_t       arg;
    struct vmm_msi_descriptor *desc;
    int i;
    int ret = VMM_OK;
    int hw_irq_num;
    int hirq = -1;
    struct vmm_msi_domain_ops *ops = domain->ops;

    ret                            = ops->msi_check(domain, dev);

    if (ret) {
        return ret; /**< 返回值 */
    }

    ret = ops->msi_prepare(domain, dev, nvec, &arg);

    if (ret) {
        return ret;
    }

    for_each_msi_entry(desc, dev)
    {
        ops->set_desc(&arg, desc);

        hirq = vmm_host_irq_domain_alloc(domain->parent, desc->nvec_used, &arg);

        if (hirq < 0) {
            ret = VMM_ERR_NOSPC;
            goto fail_handle_error;
        }

        hw_irq_num        = vmm_host_irq_domain_to_hw_irq(domain->parent, hirq);
        desc->hirq   = hirq;
        desc->domain = domain;

        for (i = 0; i < desc->nvec_used; i++) {
            vmm_host_irq_set_msi_data(hirq + i, desc);
            ret = ops->msi_init(domain, hirq + i, hw_irq_num + i, &arg);

            if (ret < 0) {
                for (i--; i > 0; i--) {
                    ops->msi_free(domain, hirq + i);
                }

                vmm_host_irq_domain_free(domain->parent, desc->hirq, desc->nvec_used);
                goto fail_handle_error;
            }
        }
    }

    ops->msi_finish(&arg, 0);

    /* If everything went fine then we write MSI messages */
    for_each_msi_entry(desc, dev)
    {
        for (i = 0; i < desc->nvec_used; i++) {
            vmm_msi_domain_write_msg(vmm_host_irq_get(desc->hirq + i));
        }
    }

    return VMM_OK;

fail_handle_error:
    ret = ops->handle_error(domain, desc, ret);
    ops->msi_finish(&arg, ret);
    return ret;
}

/**
 * @brief 释放消息信号中断域中的中断
 * @param domain 域结构体指针
 * @param dev 设备结构体指针
 */
void vmm_msi_domain_free_irqs(vmm_msi_domain_t *domain, vmm_device_t *dev)
{
    uint32_t i;
    uint32_t hirq;
    uint32_t hw_irq_num;
    struct vmm_msi_descriptor *desc;
    struct vmm_msi_domain_ops *ops;

    if (!domain || !dev) {
        return;
    }

    ops = domain->ops;

    for_each_msi_entry(desc, dev)
    {
        /*
         * We might have failed to allocate an MSI early enough
         * that there is no host IRQ associated to this entry.
         * If that's the case, don't do anything.
         */
        if (!desc->hirq) {
            continue;
        }

        hirq  = desc->hirq;
        hw_irq_num = vmm_host_irq_domain_to_hw_irq(domain->parent, hirq);

        memset(&desc->msg, 0, sizeof(desc->msg));

        for (i = 0; i < desc->nvec_used; i++) {
            ops->msi_write_msg(domain, desc, hirq + i, hw_irq_num + i, &desc->msg);
        }

        for (i = 0; i < desc->nvec_used; i++) {
            ops->msi_free(domain, hirq + i);
        }

        vmm_host_irq_domain_free(domain->parent, desc->hirq, desc->nvec_used);
        desc->hirq = 0;
    }
}
