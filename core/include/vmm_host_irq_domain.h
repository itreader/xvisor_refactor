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
 * @file vmm_host_irq_domain.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IRQ域支持，类似Linux IRQ域的Xvisor兼容实现
 */

#ifndef _VMM_HOST_IRQDOMAIN_H__
#define _VMM_HOST_IRQDOMAIN_H__

#include <libs/list.h>
#include <vmm_device_tree.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

struct vmm_char_device;
struct vmm_host_irq_domain;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * struct vmm_host_irq_domain_ops - Methods for vmm_host_irq_domain objects
 * @match: Match an interrupt controller device node to a host, returns
 *         1 on a match
 * @map: Create or update a mapping between a virtual irq number and a hw
 *       irq number. This is called only once for a given mapping.
 * @unmap: Dispose of such a mapping
 * @xlate: Given a device tree node and interrupt specifier, decode
 *         the hardware irq number and linux irq type value.
 * @alloc: Allocate a specified number of hardware irqs
 * @free: Free hardware irqs
 *
 * Functions below are provided by the driver and called whenever a new
 * mapping is created or an old mapping is disposed. The driver can then
 * proceed to whatever internal data structures 管理 is required.
 * It also needs to setup the irq_desc when returning from map().
 */
struct vmm_host_irq_domain_ops {
    int (*match)(struct vmm_host_irq_domain *d, vmm_device_tree_node_t *node); /**< 匹配函数 */
    int (*map)(struct vmm_host_irq_domain *d, uint32_t hirq, uint32_t hw_irq_num); /**< 映射 */
    void (*unmap)(struct vmm_host_irq_domain *d, uint32_t hirq); /**< unmap成员 */
    int (*xlate)(
        struct vmm_host_irq_domain *d, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq, /**< out_hw_irq成员 */
        uint32_t *out_type); /**< out_type)成员 */
    int (*alloc)(struct vmm_host_irq_domain *d, uint32_t nr_irqs, void *arg); /**< alloc成员 */
    void (*free)(struct vmm_host_irq_domain *d, uint32_t hw_irq_num, uint32_t nr_irqs); /**< 可用量 */
};

/**
 * struct vmm_host_irq_domain - IRQ domain, kind of Linux IRQ domain
 * @head:   List head for registration
 * @base:   Base
 * @count:  The number of IRQs contained.
 * @ops:    Pointer to vmm_host_irq_domain methods.
 * @irqs:   The extended IRQ array
 *
 * 可选的 elements
 * @of_node:    The device node using this domain
 * @host_data:  The controller private data pointer. Not touched by extended
 *      IRQ core code.
 * @bmap_lock:  The IRQ domain bitmap lock
 * @bmap:   The IRQ domain bitmap
 */
struct vmm_host_irq_domain {
    double_list_t                         head; /**< 链表头 */
    bool                                  uses_extend_irq; /**< uses_extend_irq成员 */
    uint32_t                              base; /**< 基址 */
    uint32_t                              count; /**< 计数 */
    uint32_t                              end; /**< 结束 */
    const struct vmm_host_irq_domain_ops *ops; /**< 操作集 */
    vmm_device_tree_node_t               *of_node; /**< 设备树节点 */
    void                                 *host_data; /**< host_data成员 */
    vmm_spinlock_t                        bmap_lock; /**< bmap_lock成员 */
    uint64_t                             *bmap; /**< bmap成员 */
};

/**
 * @brief 将主机中断域中的中断号转换为硬件中断号
 * @param domain 指向主机中断结构体的指针
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_to_hw_irq(struct vmm_host_irq_domain *domain, uint32_t hirq);

/**
 * @brief 将主机中断域中的中断号转换为全局中断号
 * @param domain 指向主机中断结构体的指针
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_to_hirq(struct vmm_host_irq_domain *domain, uint32_t hw_irq_num);

/**
 * @brief 在主机中断域中查找已映射的中断
 * @param domain 指向主机中断结构体的指针
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_find_mapping(struct vmm_host_irq_domain *domain, uint32_t hw_irq_num);

/** Find matching host IRQ domain based on given match function */
struct vmm_host_irq_domain *vmm_host_irq_domain_match(void *data, int (*fn)(struct vmm_host_irq_domain *, void *));

/**
 * @brief 输出主机中断域的调试信息
 * @param cdev 字符设备指针
 */
void vmm_host_irq_domain_debug_dump(vmm_char_device_t *cdev);

/** Find host IRQ domain for given host IRQ */
struct vmm_host_irq_domain *vmm_host_irq_domain_get(uint32_t hirq);

/**
 * @brief 在主机中断域中创建中断映射
 * @param domain 指向主机中断结构体的指针
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_create_mapping(struct vmm_host_irq_domain *domain, uint32_t hw_irq_num);

/**
 * @brief 释放主机中断域中的中断映射
 * @param hirq 中断号
 */
void vmm_host_irq_domain_dispose_mapping(uint32_t hirq);

/**
 * @brief 分配主机中断域
 * @param domain 指向主机中断结构体的指针
 * @param irq_count 数量
 * @param arg 参数值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_alloc(struct vmm_host_irq_domain *domain, uint32_t irq_count, void *arg);

/**
 * @brief 释放主机中断域
 * @param domain 指向主机中断结构体的指针
 * @param hirq 中断号
 * @param irq_count 数量
 */
void vmm_host_irq_domain_free(struct vmm_host_irq_domain *domain, uint32_t hirq, uint32_t irq_count);

/**
 * @brief 将设备树中断描述翻译为主机中断号
 * @param domain 指向主机中断结构体的指针
 * @param intspec 中断规格描述数组
 * @param intsize 大小
 * @param out_hw_irq 用于返回硬件中断号
 * @param out_type 用于返回中断类型
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_xlate(struct vmm_host_irq_domain *domain, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq, uint32_t *out_type);

/**
 * @brief 通用xlate()回调，转换一个设备树单元
 */
int vmm_host_irq_domain_xlate_onecell(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq,
    uint32_t *out_type);

/**
 * @brief 通用xlate()回调，转换两个设备树单元
 */
int vmm_host_irq_domain_xlate_twocells(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hw_irq,
    uint32_t *out_type);

/**
 * Allocate and register a new host IRQ domain.
 * @of_node: pointer to interrupt controller's device tree node.
 * @base: Base host IRQ number. If < 0 then extended IRQs are created.
 * @size: Number of interrupts in the domain.
 * @ops: map/unmap domain callbacks.
 * @host_data: Controller private data pointer.
 */
struct vmm_host_irq_domain *vmm_host_irq_domain_add(
    vmm_device_tree_node_t *of_node, int base, uint32_t size, const struct vmm_host_irq_domain_ops *ops, void *host_data);

/**
 * @brief 从系统中移除主机中断域
 * @param domain 指向主机中断结构体的指针
 */
void vmm_host_irq_domain_remove(struct vmm_host_irq_domain *domain);

/**
 * @brief 初始化主机中断域
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_irq_domain_init(void);

extern const struct vmm_host_irq_domain_ops irq_domain_ops;

#endif /* _VMM_HOST_IRQDOMAIN_H__ */
