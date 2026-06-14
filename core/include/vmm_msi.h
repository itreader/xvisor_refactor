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
 * @file vmm_msi.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 主机MSI框架通用接口
 */

#ifndef __VMM_MSI_H__
#define __VMM_MSI_H__

#include <libs/list.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>
#include <vmm_host_irq.h>

/**
 * @brief MSI中断消息结构，包含消息地址和数据内容
 */
struct vmm_msi_msg {
    uint32_t address_lo; /**< MSI消息地址的低32位 */
    uint32_t address_hi; /**< MSI消息地址的高32位 */
    uint32_t data;       /**< MSI消息数据（16位） */
};

/**
 * 平台设备特定的MSI描述符数据
 */
struct vmm_platform_msi_descriptor {
    struct vmm_platform_msi_priv_data *msi_priv_data; /**< 平台私有数据指针 */
    uint16_t                           msi_index; /**< MSI描述符索引 */
};

vmm_host_irq_t;
struct vmm_msi_domain;
typedef struct vmm_msi_domain vmm_msi_domain_t;

/**
 * MSI中断描述符结构体
 *
 * PCI MSI/X相关字段：
 * @masked: 掩码位
 * @is_msix: 是否为MSI-X
 * @multiple: 已分配消息数的log2值
 * @multi_cap: 支持消息数的log2值
 * @maskbit: 是否支持Mask-Pending位
 * @is_64: 地址宽度（0=32位，1=64位）
 * @entry_nr: 此描述符对应的条目编号
 * @default_irq: 默认预分配的非MSI中断号
 * @mask_pos: MSI掩码寄存器位置
 * @mask_base: MSI-X掩码寄存器基地址
 * @platform: 平台设备特定的MSI描述符数据
 */
struct vmm_msi_descriptor {
    /* 设备/总线类型无关的共享数据 */
    double_list_t      list; /**< 管理链表头 */
    uint32_t           hirq; /**< 基础主机中断号 */
    uint32_t           nvec_used; /**< 实际使用的向量数 */
    uint32_t           msi_index; /**< 多MSI场景下的描述符索引 */
    vmm_device_t      *dev; /**< 使用此描述符的设备 */
    vmm_msi_domain_t  *domain; /**< 使用此描述符的MSI域 */
    struct vmm_msi_msg msg; /**< 缓存的MSI消息（用于复用） */

    union {
        /* PCI MSI/X特定数据 */
        struct {
            uint32_t masked; /**< 掩码位 */

            struct {
                uint8_t  is_msix   : 1; /**< 是否为MSI-X */
                uint8_t  multiple  : 3; /**< 已分配消息数的log2 */
                uint8_t  multi_cap : 3; /**< 支持消息数的log2 */
                uint8_t  maskbit   : 1; /**< 是否支持Mask-Pending位 */
                uint8_t  is_64     : 1; /**< 地址宽度（0=32位，1=64位） */
                uint16_t entry_nr; /**< 描述符对应的条目编号 */
                unsigned default_irq; /**< 默认预分配的非MSI中断号 */
            } msi_attrib; /**< MSI属性位域 */

            union {
                uint8_t mask_pos; /**< MSI掩码寄存器位置 */
                void   *mask_base; /**< MSI-X掩码寄存器基地址 */
            };
        };

        /* 非PCI设备在此添加自定义数据结构 */
        struct vmm_platform_msi_descriptor platform; /**< 平台设备MSI描述数据 */
    };
};

#ifndef NUM_MSI_ALLOC_SCRATCHPAD_REGS
#define NUM_MSI_ALLOC_SCRATCHPAD_REGS 2
#endif

/**
 * @brief MSI中断分配的默认结构体，架构可提供自定义实现
 */
typedef struct vmm_msi_alloc_info {
    struct vmm_msi_descriptor *desc; /**< MSI描述符指针 */
    uint32_t                   hw_irq_num; /**< 硬件中断号 */

    union {
        uint64_t ul; /**< 64位整数值 */
        void    *ptr; /**< 通用指针 */
    } scratchpad[NUM_MSI_ALLOC_SCRATCHPAD_REGS]; /**< 暂存寄存器数组 */
} vmm_msi_alloc_info_t;

/* 辅助宏：隐藏msi_desc实现细节 */
#define msi_desc_to_dev(desc)         ((desc)->dev)
#define dev_to_msi_list(dev)          (&(dev)->msi_list)
#define first_msi_entry(dev)          list_first_entry(dev_to_msi_list((dev)), struct vmm_msi_descriptor, list)
#define for_each_msi_entry(desc, dev) list_for_each_entry((desc), dev_to_msi_list((dev)), list)

typedef void (*vmm_irq_write_msi_msg_t)(struct vmm_msi_descriptor *desc, struct vmm_msi_msg *msg);

/**
 * MSI域回调操作集
 * @msi_init:       MSI中断的域特定初始化函数
 * @msi_free:       MSI中断的域特定释放函数
 * @msi_check:      域/信息/设备数据的验证回调
 * @msi_prepare:    在域中准备中断分配
 * @msi_finish:     可选的分配完成回调
 * @set_desc:       设置中断的MSI描述符
 * @handle_error:   分配失败时的可选错误处理回调
 * @msi_write_msg:  写入MSI消息的域特定回调
 */
struct vmm_msi_domain_ops {
    int (*msi_init)(vmm_msi_domain_t *domain, uint32_t hirq, uint32_t hw_irq_num, vmm_msi_alloc_info_t *arg); /**< MSI中断初始化回调 */
    void (*msi_free)(vmm_msi_domain_t *domain, uint32_t hirq); /**< MSI中断释放回调 */
    int (*msi_check)(vmm_msi_domain_t *domain, vmm_device_t *dev); /**< MSI域/设备验证回调 */
    int (*msi_prepare)(vmm_msi_domain_t *domain, vmm_device_t *dev, int nvec, vmm_msi_alloc_info_t *arg); /**< MSI分配准备回调 */
    void (*msi_finish)(vmm_msi_alloc_info_t *arg, int retval); /**< MSI分配完成回调 */
    void (*set_desc)(vmm_msi_alloc_info_t *arg, struct vmm_msi_descriptor *desc); /**< 设置MSI描述符回调 */
    int (*handle_error)(vmm_msi_domain_t *domain, struct vmm_msi_descriptor *desc, int error); /**< 分配错误处理回调 */
    void (*msi_write_msg)(vmm_msi_domain_t *domain, struct vmm_msi_descriptor *desc, uint32_t hirq, uint32_t hw_irq_num, struct vmm_msi_msg *msg); /**< 写入MSI消息回调 */
};

/** MSI域类型枚举 */
/**
 * @brief MSI域类型枚举，定义不同类型的消息信号中断域
 */
enum vmm_msi_domain_types {
    VMM_MSI_DOMAIN_UNKNOWN = 0, /**< 未知类型 */
    VMM_MSI_DOMAIN_PLATFORM, /**< 平台设备类型 */
    VMM_MSI_DOMAIN_PCI, /**< PCI设备类型 */
    VMM_MSI_DOMAIN_MAX, /**< 类型数量上限 */
};

/* MSI域标志 */
enum {
    /** 使用默认MSI域回调初始化未实现的ops */
    VMM_MSI_FLAG_USE_DEF_DOM_OPS = (1 << 0), /**< 使用默认域操作集 */
    /** 支持多PCI MSI中断 */
    VMM_MSI_FLAG_MULTI_PCI_MSI   = (1 << 1), /**< 支持多PCI MSI中断 */
    /** 支持PCI MSI-X中断 */
    VMM_MSI_FLAG_PCI_MSIX        = (1 << 2), /**< 支持PCI MSI-X中断 */
};

/**
 * MSI域结构体
 * @head:   注册管理链表头
 * @type:   MSI域类型
 * @fwnode: 底层设备_tree节点
 * @ops:    MSI域操作集指针
 * @parent: 父级主机中断域
 * @flags:  MSI域标志位
 * @data:   域特定私有数据
 */
struct vmm_msi_domain {
    double_list_t               head; /**< 注册管理链表头 */
    enum vmm_msi_domain_types   type; /**< MSI域类型 */
    vmm_device_tree_node_t     *fwnode; /**< 固件设备树节点 */
    struct vmm_msi_domain_ops  *ops; /**< MSI域操作集 */
    struct vmm_host_irq_domain *parent; /**< 父级主机中断域 */
    uint64_t                    flags; /**< MSI域标志位 */
    void                       *data; /**< 域特定私有数据 */
};

/**
 * @brief 为设备分配MSI描述符条目
 * @param dev 使用此描述符的设备指针
 */
struct vmm_msi_descriptor *vmm_alloc_msi_entry(vmm_device_t *dev);

/**
 * @brief 释放MSI描述符条目
 * @param entry 待释放的MSI描述符条目指针
 */
void vmm_free_msi_entry(struct vmm_msi_descriptor *entry);

/**
 * @brief 创建MSI域
 * @param type MSI域类型
 * @param fwnode 固件设备树节点指针
 * @param ops MSI域操作集指针
 * @param parent 父级主机中断域
 * @param flags MSI域标志位
 * @param data 域特定私有数据
 */
vmm_msi_domain_t *vmm_msi_create_domain(
    enum vmm_msi_domain_types type, vmm_device_tree_node_t *fwnode, struct vmm_msi_domain_ops *ops, struct vmm_host_irq_domain *parent,
    uint64_t flags, void *data);

/**
 * @brief 销毁MSI域并释放相关资源
 * @param domain 域结构体指针
 */
void vmm_msi_destroy_domain(vmm_msi_domain_t *domain);

static inline void *vmm_msi_domain_data(vmm_msi_domain_t *domain)
{
    return (domain) ? domain->data : NULL;
}

/**
 * @brief 查找消息信号中断域
 * @param fwnode 固件节点指针
 * @param type 类型标识值
 */
vmm_msi_domain_t *vmm_msi_find_domain(vmm_device_tree_node_t *fwnode, enum vmm_msi_domain_types type);

/**
 * @brief 向指定MSI域写入中断消息
 * @param irq 指向主机中断结构体的指针
 */
void vmm_msi_domain_write_msg(vmm_host_irq_t *irq);

/**
 * @brief 在MSI域中为设备分配中断请求
 * @param domain MSI域结构体指针
 * @param dev 需要分配中断的设备指针
 * @param nvec 要分配的中断向量数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_msi_domain_alloc_irqs(vmm_msi_domain_t *domain, vmm_device_t *dev, int nvec);

/**
 * @brief 释放消息信号中断域中的中断
 * @param domain 域结构体指针
 * @param dev 设备结构体指针
 */
void vmm_msi_domain_free_irqs(vmm_msi_domain_t *domain, vmm_device_t *dev);

/**
 * @brief 创建平台MSI域
 * @param fwnode 固件设备树节点指针
 * @param ops MSI域操作集指针
 * @param parent 父级主机中断域
 * @param flags MSI域标志位
 * @param data 域特定私有数据
 */
vmm_msi_domain_t *vmm_platform_msi_create_domain(
    vmm_device_tree_node_t *fwnode, struct vmm_msi_domain_ops *ops, struct vmm_host_irq_domain *parent, uint64_t flags, void *data);

/**
 * @brief 销毁平台MSI域并释放相关资源
 * @param domain 域结构体指针
 */
void vmm_platform_msi_destroy_domain(vmm_msi_domain_t *domain);

static inline void *vmm_platform_msi_domain_data(vmm_msi_domain_t *domain)
{
    return vmm_msi_domain_data(domain);
}

/**
 * @brief 在平台MSI域中分配中断
 * @param dev 设备结构体指针
 * @param nvec 向量数量
 * @param write_msi_msg MSI消息写入回调函数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_platform_msi_domain_alloc_irqs(vmm_device_t *dev, uint32_t nvec, vmm_irq_write_msi_msg_t write_msi_msg);

/**
 * @brief 释放平台MSI域中的中断
 * @param dev 设备结构体指针
 */
void vmm_platform_msi_domain_free_irqs(vmm_device_t *dev);

#endif /* __VMM_MSI_H__ */
