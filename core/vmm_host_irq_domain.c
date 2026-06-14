/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * @file vmm_host_irq_domain.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IRQ域支持，类似Linux IRQ域的Xvisor兼容实现
 */

#include <libs/bitmap.h>
#include <libs/list.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_extend_irq.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

/**
 * @brief 主机中断域控制结构，管理中断域的注册状态
 */
struct vmm_host_irq_domain_ctrl {
    vmm_rwlock_t  lock; /**< 自旋锁 */
    double_list_t domains; /**< domains成员 */
};

static struct vmm_host_irq_domain_ctrl idctrl;

/**
 * @brief 将主机中断域中的中断号转换为硬件中断号
 * @param domain 指向主机中断结构体的指针
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_to_hw_irq(struct vmm_host_irq_domain *domain, uint32_t hirq)
{
    if (!domain) {
        return VMM_ERR_INVALID;
    }

    if (hirq < domain->base || hirq >= domain->end) {
        return VMM_ERR_NOTAVAIL;
    }

    return hirq - domain->base;
}

/**
 * @brief 将主机中断域中的中断号转换为全局中断号
 * @param domain 指向主机中断结构体的指针
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_to_hirq(struct vmm_host_irq_domain *domain, uint32_t hw_irq_num)
{
    if (!domain) {
        return VMM_ERR_INVALID;
    }

    if (hw_irq_num >= domain->count) {
        return VMM_ERR_RANGE;
    }

    return domain->base + hw_irq_num;
}

/**
 * @brief 在主机中断域中查找已映射的中断
 * @param domain 指向主机中断结构体的指针
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_find_mapping(struct vmm_host_irq_domain *domain, uint32_t hw_irq_num)
{
    if (!domain) {
        return VMM_ERR_INVALID;
    }

    if (hw_irq_num >= domain->count) {
        return VMM_ERR_RANGE;
    }

    if (vmm_host_irq_get(domain->base + hw_irq_num)) {
        return domain->base + hw_irq_num;
    }

    return VMM_ERR_NOTAVAIL;
}

struct vmm_host_irq_domain *vmm_host_irq_domain_match(void *data, int (*fn)(struct vmm_host_irq_domain *, void *))
{
    irq_flags_t                 flags; /**< 标志位 */
    struct vmm_host_irq_domain *domain = NULL; /**< NULL成员 */
    struct vmm_host_irq_domain *found  = NULL; /**< NULL成员 */

    vmm_read_lock_irq_save_lite(&idctrl.lock, flags); /**< flags)成员 */

    list_for_each_entry(domain, &idctrl.domains, head)
    {
        if (fn(domain, data)) {
            found = domain; /**< 域 */
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags); /**< flags)成员 */

    return found; /**< found成员 */
}

/**
 * @brief 输出主机中断域的调试信息
 * @param cdev 字符设备指针
 */
void vmm_host_irq_domain_debug_dump(vmm_char_device_t *cdev)
{
    int                         idx = 0;
    irq_flags_t                 flags;
    vmm_host_irq_t        *irq    = NULL;
    struct vmm_host_irq_domain *domain = NULL;

    vmm_read_lock_irq_save_lite(&idctrl.lock, flags);

    list_for_each_entry(domain, &idctrl.domains, head)
    {
        vmm_cdev_printf(cdev, "  Group from IRQ %d to %d:\n", domain->base, domain->end);

        for (idx = domain->base; idx < domain->end; ++idx) {
            irq = vmm_host_irq_get(idx);

            if (!irq) {
                continue;
            }

            if (idx != irq->num) {
                vmm_cdev_printf(
                    cdev,
                    "WARNING: IRQ %d "
                    "not correctly set\n",
                    irq->num);
            }

            vmm_cdev_printf(
                cdev,
                "    IRQ %d mapped, name: %s, "
                "chip: %s\n",
                idx, irq->name, irq->chip ? irq->chip->name : "None");
        }
    }

    vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags);
}

struct vmm_host_irq_domain *vmm_host_irq_domain_get(uint32_t hirq)
{
    irq_flags_t                 flags; /**< 标志位 */
    struct vmm_host_irq_domain *domain = NULL; /**< NULL成员 */

    vmm_read_lock_irq_save_lite(&idctrl.lock, flags); /**< flags)成员 */

    list_for_each_entry(domain, &idctrl.domains, head)
    {
        if ((hirq >= domain->base) && (hirq < domain->end)) {
            vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags); /**< flags)成员 */
            return domain; /**< 域 */
        }
    }

    vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags); /**< flags)成员 */

    return NULL; /**< NULL成员 */
}

/**
 * @brief 在中断域中创建中断映射
 * @param domain 指向主机中断结构体的指针
 * @param hirq 中断号
 * @param hw_irq_num 数量
 * @return 中断处理结果
 */
static int __irq_domain_create_mapping(struct vmm_host_irq_domain *domain, uint32_t hirq, uint32_t hw_irq_num)
{
    int rc = VMM_OK;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        rc = __vmm_host_irq_set_hw_irq(hirq, hw_irq_num);
    } else {
        rc = vmm_host_extend_irq_create_mapping(hirq, hw_irq_num);
    }

    if (rc) {
        return rc;
    }

    if (domain->ops && domain->ops->map) {
        rc = domain->ops->map(domain, hirq, hw_irq_num);

        if (rc) {
            if (hirq < CONFIG_HOST_IRQ_COUNT) {
                __vmm_host_irq_set_hw_irq(hirq, hirq);
            } else {
                vmm_host_extend_irq_dispose_mapping(hirq);
            }

            return rc;
        }
    }

    return VMM_OK;
}

/**
 * @brief 释放中断域中的中断映射
 * @param domain 指向主机中断结构体的指针
 * @param hirq 中断号
 */
static void __irq_domain_dispose_mapping(struct vmm_host_irq_domain *domain, uint32_t hirq)
{
    if (domain->ops && domain->ops->unmap) {
        domain->ops->unmap(domain, hirq);
    }

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        __vmm_host_irq_set_hw_irq(hirq, hirq);
    } else {
        vmm_host_extend_irq_dispose_mapping(hirq);
    }
}

/**
 * @brief 在主机中断域中创建中断映射
 * @param domain 指向主机中断结构体的指针
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_create_mapping(struct vmm_host_irq_domain *domain, uint32_t hw_irq_num)
{
    int         rc = VMM_OK;
    uint32_t    hirq;
    irq_flags_t flags;

    if (!domain) {
        return VMM_ERR_NOTAVAIL;
    }

    if (hw_irq_num >= domain->count) {
        return VMM_ERR_NOTAVAIL;
    }

    hirq = domain->base + hw_irq_num;

    vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);

    if (bitmap_isset(domain->bmap, hw_irq_num)) {
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
        return hirq;
    }

    bitmap_set(domain->bmap, hw_irq_num, 1);
    vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

    rc = __irq_domain_create_mapping(domain, hirq, hw_irq_num);

    if (rc) {
        vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
        bitmap_clear(domain->bmap, hw_irq_num, 1);
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
        return rc;
    }

    return hirq;
}

/**
 * @brief 释放主机中断域中的中断映射
 * @param hirq 中断号
 */
void vmm_host_irq_domain_dispose_mapping(uint32_t hirq)
{
    int                         hw_irq_num;
    irq_flags_t                 flags;
    struct vmm_host_irq_domain *domain = vmm_host_irq_domain_get(hirq);

    if (!domain) {
        return;
    }

    hw_irq_num = vmm_host_irq_domain_to_hw_irq(domain, hirq);

    if (hw_irq_num < 0) {
        return;
    }

    vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);

    if (!bitmap_isset(domain->bmap, hw_irq_num)) {
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
        return;
    }

    bitmap_clear(domain->bmap, hw_irq_num, 1);
    vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

    __irq_domain_dispose_mapping(domain, hirq);
}

/**
 * @brief 分配主机中断域
 * @param domain 指向主机中断结构体的指针
 * @param irq_count 数量
 * @param arg 参数值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_alloc(struct vmm_host_irq_domain *domain, uint32_t irq_count, void *arg)
{
    int         rc;
    irq_flags_t flags;
    bool        found = false;
    uint32_t i;
    uint32_t j;
    uint32_t hirq;
    uint32_t hw_irq_num;
    uint32_t count;

    if (!domain || !irq_count || (domain->count < irq_count)) {
        return VMM_ERR_INVALID;
    }

    if (domain->ops->alloc) {
        rc = domain->ops->alloc(domain, irq_count, arg);

        if (rc < 0) {
            return rc;
        }

        hw_irq_num = rc;
        vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
        bitmap_set(domain->bmap, hw_irq_num, irq_count);
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
    } else {
        vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);

        if (!found) {
            count = 0;

            for (hw_irq_num = 0; hw_irq_num < domain->count; hw_irq_num++) {
                if (bitmap_isset(domain->bmap, hw_irq_num)) {
                    count = 0;
                } else {
                    count++;
                }

                if (count == irq_count) {
                    found = true;
                    hw_irq_num = hw_irq_num - (count - 1);
                    break;
                }
            }
        }

        if (!found) {
            vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
            return VMM_ERR_NOENT;
        }

        bitmap_set(domain->bmap, hw_irq_num, irq_count);
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
    }

    hirq = domain->base + hw_irq_num;

    for (i = 0; i < irq_count; i++) {
        rc = __irq_domain_create_mapping(domain, hirq + i, hw_irq_num + i);

        if (rc) {
            for (j = 0; j < i; j++) {
                __irq_domain_dispose_mapping(domain, hirq + j);
            }

            vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
            bitmap_clear(domain->bmap, hw_irq_num, irq_count);
            vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

            if (domain->ops->free) {
                domain->ops->free(domain, hw_irq_num, irq_count);
            }

            return rc;
        }
    }

    return hirq;
}

/**
 * @brief 释放主机中断域
 * @param domain 指向主机中断结构体的指针
 * @param hirq 中断号
 * @param irq_count 数量
 */
void vmm_host_irq_domain_free(struct vmm_host_irq_domain *domain, uint32_t hirq, uint32_t irq_count)
{
    irq_flags_t flags;
    uint32_t i;
    uint32_t hw_irq_num;

    if (!domain || (hirq < domain->base) || ((hirq + irq_count) < domain->base) || ((domain->base + domain->count) <= hirq) ||
        ((domain->base + domain->count) <= (hirq + irq_count))) {
        return;
    }

    for (i = 0; i < irq_count; i++) {
        __irq_domain_dispose_mapping(domain, hirq + i);
    }

    hw_irq_num = hirq - domain->base;
    vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
    bitmap_clear(domain->bmap, hw_irq_num, irq_count);
    vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

    if (domain->ops->free) {
        domain->ops->free(domain, hw_irq_num, irq_count);
    }
}

/**
 * @brief 将设备树中断描述翻译为主机中断号
 * @param domain 指向主机中断结构体的指针
 * @param intspec 中断规格描述数组
 * @param intsize 大小
 * @param out_hw_irq 用于返回硬件中断号
 * @param out_type 用于返回中断类型
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_xlate(struct vmm_host_irq_domain *domain, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq, uint32_t *out_type)
{
    if (!domain || !intspec || !out_hw_irq || !out_type) {
        return VMM_ERR_INVALID;
    }

    /* If domain has no translation, then we assume interrupt line */
    if (!domain->ops || !domain->ops->xlate) {
        *out_hw_irq = intspec[0];
    } else {
        return domain->ops->xlate(domain, domain->of_node, intspec, intsize, out_hw_irq, out_type);
    }

    return VMM_OK;
}

/**
 * @brief 使用单单元格格式翻译设备树中断描述
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_xlate_onecell(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq,
    uint32_t *out_type)
{
    if (WARN_ON(intsize != 1)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    *out_hw_irq = intspec[0];
    *out_type  = VMM_IRQ_TYPE_NONE;

    return VMM_OK; /**< VMM_OK成员 */
}

/**
 * @brief 使用双单元格格式翻译设备树中断描述
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_xlate_twocells(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq,
    uint32_t *out_type)
{
    if (WARN_ON(intsize != 2)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    *out_hw_irq = intspec[0];
    *out_type  = intspec[1] & VMM_IRQ_TYPE_SENSE_MASK;

    return VMM_OK; /**< VMM_OK成员 */
}

struct vmm_host_irq_domain *vmm_host_irq_domain_add(
    vmm_device_tree_node_t *of_node, int base, uint32_t size, const struct vmm_host_irq_domain_ops *ops, void *host_data)
{
    int                         pos = 0; /**< 0 */
    irq_flags_t                 flags; /**< 标志位 */
    uint64_t                   *bmap; /**< bmap成员 */
    struct vmm_host_irq_domain *newdomain = NULL; /**< NULL成员 */

    if (!size || !ops) {
        return NULL; /**< NULL成员 */
    }

    if ((base >= 0) && ((CONFIG_HOST_IRQ_COUNT <= base) || (CONFIG_HOST_IRQ_COUNT <= (base + size)))) {
        return NULL; /**< NULL成员 */
    }

    if ((base >= 0) && (vmm_host_irq_domain_get(base) || vmm_host_irq_domain_get(base + size - 1))) {
        return NULL; /**< NULL成员 */
    }

    bmap = vmm_zalloc(bitmap_estimate_size(size)); /**< vmm_zalloc(bitmap_estimate_size(size))成员 */

    if (!bmap) {
        return NULL; /**< NULL成员 */
    }

    newdomain = vmm_zalloc(sizeof(struct vmm_host_irq_domain)); /**< vmm_host_irq_domain))成员 */

    if (!newdomain) {
        vmm_free(bmap);
        return NULL; /**< NULL成员 */
    }

    if (base < 0) {
        if ((pos = vmm_host_extend_irq_alloc_region(size)) < 0) {
            vmm_printf("%s: Failed to find available slot for IRQ\n", __func__); /**< __func__)成员 */
            vmm_free(bmap);
            vmm_free(newdomain);
            return NULL; /**< NULL成员 */
        }
    } else {
        pos = base; /**< 基址 */
    }

    INIT_LIST_HEAD(&newdomain->head);
    newdomain->uses_extend_irq = (base < 0) ? TRUE : FALSE; /**< FALSE成员 */
    newdomain->base            = pos; /**< pos成员 */
    newdomain->count           = size; /**< 大小 */
    newdomain->end             = newdomain->base + size; /**< 大小 */
    newdomain->host_data       = host_data; /**< host_data成员 */

    if (of_node) {
        vmm_device_tree_ref_node(of_node);
        newdomain->of_node = of_node; /**< of_node成员 */
    }

    newdomain->ops = ops; /**< 操作集 */
    INIT_SPIN_LOCK(&newdomain->bmap_lock);
    newdomain->bmap = bmap; /**< bmap成员 */

    vmm_write_lock_irq_save_lite(&idctrl.lock, flags); /**< flags)成员 */
    list_add_tail(&newdomain->head, &idctrl.domains); /**< &idctrl.domains)成员 */
    vmm_write_unlock_irq_restore_lite(&idctrl.lock, flags); /**< flags)成员 */

    return newdomain; /**< newdomain成员 */
}

/**
 * @brief 从系统中移除主机中断域
 * @param domain 指向主机中断结构体的指针
 */
void vmm_host_irq_domain_remove(struct vmm_host_irq_domain *domain)
{
    uint32_t    pos = 0;
    irq_flags_t flags;

    if (!domain) {
        return;
    }

    vmm_write_lock_irq_save_lite(&idctrl.lock, flags);
    list_del(&domain->head);
    vmm_write_unlock_irq_restore_lite(&idctrl.lock, flags);

    for (pos = domain->base; pos < domain->end; ++pos) {
        vmm_host_extend_irq_dispose_mapping(pos);
    }

    if (domain->uses_extend_irq) {
        vmm_host_extend_irq_free_region(domain->base, domain->count);
    }

    if (domain->of_node) {
        vmm_device_tree_dref_node(domain->of_node);
    }

    vmm_free(domain);
}

/**
 * @brief 初始化主机中断域
 * @return 中断处理结果
 */
int __init vmm_host_irq_domain_init(void)
{
    memset(&idctrl, 0, sizeof(struct vmm_host_irq_domain_ctrl));
    INIT_RW_LOCK(&idctrl.lock);
    INIT_LIST_HEAD(&idctrl.domains);

    return VMM_OK;
}

/* For future use */
const struct vmm_host_irq_domain_ops irq_domain_ops = {
    /* .xlate = extirq_xlate, */
};
