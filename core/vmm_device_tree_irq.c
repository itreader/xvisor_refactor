/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_device_tree_irq.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备树中断处理函数
 *
 * The source has been largely adapted from the Linux kernel v3.16:
 * drivers/of/irq.c and kernel/irq/irq_domain.c
 *
 * The original code is licensed under the GPL.
 */

#include <libs/mathlib.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_stdio.h>

#define pr_warn(msg...) vmm_printf(msg)
#ifdef DEBUG
#define pr_debug(msg...) vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

/**
 * @brief 获取设备树中断的数量
 * @param node 设备树节点指针
 * @return 数量值
 */
uint32_t vmm_device_tree_irq_count(vmm_device_tree_node_t *node)
{
    int                     rc;
    uint32_t alen;
    uint32_t tmp;
    vmm_device_tree_node_t *parent;

    rc = vmm_device_tree_count_phandle_with_args(node, "interrupts-extended", "#interrupt-cells");

    if (rc >= 0) {
        return rc;
    }

    parent = vmm_device_tree_irq_find_parent(node);

    if (!node || !parent) {
        return 0;
    }

    rc = vmm_device_tree_read_u32(parent, "#interrupt-cells", &tmp);
    vmm_device_tree_dref_node(parent);

    if (rc) {
        return 0;
    }

    alen = vmm_device_tree_attrlen(node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME);

    return udiv32(alen, (sizeof(uint32_t) * tmp));
}

/**
 * @brief 查找设备树节点的中断控制器父节点
 * @param child 子设备树节点
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_tree_node_t *vmm_device_tree_irq_find_parent(vmm_device_tree_node_t *child)
{
    vmm_device_tree_node_t *p;
    const uint32_t         *parp;

    if (!child) {
        return NULL;
    }

    vmm_device_tree_ref_node(child);

    do {
        parp = vmm_device_tree_attrval(child, "interrupt-parent");

        if (parp == NULL) {
            p = child->parent;
            vmm_device_tree_ref_node(child->parent);
        } else {
            p = vmm_device_tree_find_node_by_phandle(vmm_be32_to_cpu(*parp));
        }

        vmm_device_tree_dref_node(child);
        child = p;
    } while (p && vmm_device_tree_attrval(p, "#interrupt-cells") == NULL);

    return p;
}

/**
 * @brief 解析设备树节点的第index个中断描述
 * @param device 设备结构体指针
 * @param index 数组中的索引位置
 * @param out_irq 用于返回解析后的中断描述
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_tree_irq_parse_one(vmm_device_tree_node_t *device, int index, struct vmm_device_tree_phandle_args *out_irq)
{
    vmm_device_tree_node_t      *p       = NULL;
    struct vmm_device_tree_attr *attr    = NULL;
    uint32_t                    *intspec = NULL;
    uint32_t                     intsize = 0;
    uint32_t                     intlen  = 0;
    int                          res     = VMM_ERR_INVALID;
    int                          i;

    if (!device || (index < 0) || !out_irq) {
        return VMM_ERR_INVALID;
    }

    pr_debug("%s: dev=%s, index=%d\n", __func__, device->name, index);

    /* Try the new-style interrupts-extended first */
    res = vmm_device_tree_parse_phandle_with_args(device, "interrupts-extended", "#interrupt-cells", index, out_irq);

    if (res == VMM_OK) {
        return res;
    }

    attr = vmm_device_tree_getattr(device, "interrupts");

    if (NULL == attr) {
        return VMM_ERR_INVALID;
    }

    intlen  = attr->len / sizeof(uint32_t);
    intspec = attr->value;
    pr_debug(" intspec=%d intlen=%d\n", vmm_be32_to_cpu(*intspec), intlen);

    /* Look for the interrupt parent. */
    p = vmm_device_tree_irq_find_parent(device);

    if (NULL == p) {
        /* If no interrupt-parent fount then
         * read interrupts attribute directly
         */
        res = vmm_device_tree_read_u32_atindex(device, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &intsize, index);

        if (res != VMM_OK) {
            return res;
        }

        out_irq->np         = NULL;
        out_irq->args_count = 1;
        out_irq->args[0]    = intsize;
        return VMM_OK;
    }

    /* Get size of interrupt specifier */
    res = vmm_device_tree_read_u32(p, "#interrupt-cells", &intsize);

    if (VMM_OK != res) {
        vmm_device_tree_dref_node(p);
        return res;
    }

    pr_debug(" intsize=%d intlen=%d\n", intsize, intlen);

    /* Check index */
    if ((index + 1) * intsize > intlen) {
        vmm_device_tree_dref_node(p);
        return VMM_ERR_INVALID;
    }

    /* Copy intspec into irq structure */
    intspec += index * intsize;
    out_irq->np         = p;
    out_irq->args_count = intsize;

    for (i = 0; i < intsize && i < VMM_MAX_PHANDLE_ARGS; i++) {
        out_irq->args[i] = vmm_be32_to_cpu(*intspec++);
    }

    return VMM_OK;
}

/**
 * @brief 查找匹配设备树节点的中断域
 * @param domain 指向主机中断结构体的指针
 * @param node 设备树节点指针
 * @return 中断处理结果
 */
static int device_tree_irq_domain_match_node(struct vmm_host_irq_domain *domain, void *node)
{
    if (domain->of_node == node) {
        return 1;
    }

    return 0;
}

struct vmm_host_irq_domain *vmm_device_tree_irq_domain_find(vmm_device_tree_node_t *node)
{
    return vmm_host_irq_domain_match(node, &device_tree_irq_domain_match_node); /**< &device_tree_irq_domain_match_node)成员 */
}

/**
 * @brief 为设备树节点创建中断映射关系
 * @param irq_data 中断数据指针
 * @return 中断号
 */
static uint32_t vmm_device_tree_irq_create_mapping(struct vmm_device_tree_phandle_args *irq_data)
{
    int                         rc;
    struct vmm_host_irq_domain *domain = NULL;
    vmm_host_irq_t        *irq    = NULL;
    uint64_t                    hw_irq_num;
    uint32_t hirq;
    uint32_t type = VMM_IRQ_TYPE_NONE;

    if (irq_data->np) {
        domain = vmm_device_tree_irq_domain_find(irq_data->np);

        if (!domain) {
            /* If no domain found then fail. */
            return 0;
        }
    } else {
        return irq_data->args[0];
    }

    pr_debug("Domain %s found\n", domain->of_node->name);

    /* Determine translation */
    rc = vmm_host_irq_domain_xlate(domain, irq_data->args, irq_data->args_count, &hw_irq_num, &type);

    if (rc < 0) {
        return rc;
    }

    /* Create mapping */
    rc = vmm_host_irq_domain_create_mapping(domain, hw_irq_num);

    if (rc < 0) {
        return rc;
    }

    hirq = rc;

    pr_debug("Extended IRQ %d set as the %ldth irq on %s\n", hirq, hw_irq_num, domain->of_node->name);

    irq = vmm_host_irq_get(hirq);

    if (!irq) {
        return VMM_ERR_FAIL;
    }

    /* Set type if specified and different than the current one */
    if (type != VMM_IRQ_TYPE_NONE && type != irq->state) {
        vmm_host_irq_set_type(hirq, type);
    }

    return hirq;
}

/**
 * @brief 解析设备树节点的中断并映射到主机中断号
 * @param dev 设备结构体指针
 * @param index 数组中的索引位置
 * @return 中断号
 */
uint32_t vmm_device_tree_irq_parse_map(vmm_device_tree_node_t *dev, int index)
{
    int                                 hirq = 0;
    struct vmm_device_tree_phandle_args oirq = {.np = NULL,
                                                .args_count = 0};

    if (vmm_device_tree_irq_parse_one(dev, index, &oirq)) {
        return 0;
    }

    if (oirq.args_count) {
        hirq = vmm_device_tree_irq_create_mapping(&oirq);
    }

    if (oirq.np) {
        vmm_device_tree_dref_node(oirq.np);
    }

    return (hirq < 0) ? 0 : hirq;
}
