/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_device_driver.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备驱动框架头文件
 */

#ifndef __VMM_DEVDRV_H_
#define __VMM_DEVDRV_H_

#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))

struct vmm_device;
struct vmm_driver;
struct vmm_iommu_ops;
struct vmm_iommu_group;
struct vmm_msi_domain;

typedef struct vmm_iommu_group vmm_iommu_group_t;
typedef struct vmm_device      vmm_device_t;
typedef struct vmm_driver      vmm_driver_t;
typedef struct vmm_iommu_ops   vmm_iommu_ops_t;
typedef struct vmm_msi_domain  vmm_msi_domain_t;

/**
 * @brief 设备类结构，描述一类设备的通用属性和操作
 */
typedef struct vmm_class {
    /* Private fields (for device driver framework) */
    double_list_t head; /**< 链表头 */
    vmm_mutex_t   lock; /**< 自旋锁 */
    double_list_t device_list; /**< 设备链表 */
    /* Public fields */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    void (*release)(vmm_device_t *); /**< 释放回调 */
} vmm_class_t;

/**
 * @brief 总线结构，表示设备连接的总线类型及其操作接口
 */
typedef struct vmm_bus {
    /* Private fields (for device driver framework) */
    double_list_t                 head; /**< 链表头 */
    vmm_mutex_t                   lock; /**< 自旋锁 */
    double_list_t                 device_list; /**< 设备链表 */
    double_list_t                 driver_list; /**< 驱动链表 */
    vmm_blocking_notifier_chain_t event_listeners; /**< 事件监听器 */
    /* Public fields */
    char                          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    vmm_iommu_ops_t              *iommu_ops; /**< IOMMU操作集 */
    int (*match)(vmm_device_t *dev, vmm_driver_t *drv); /**< 匹配函数 */
    int (*probe)(vmm_device_t *); /**< 探测函数 */
    int (*remove)(vmm_device_t *); /**< 移除函数 */
    void (*shutdown)(vmm_device_t *); /**< 关机回调 */
} vmm_bus_t;

/**
 * @brief 设备类型枚举，区分虚拟设备和物理设备
 */
typedef struct vmm_device_type {
    const char *name; /**< 名称 */
    void (*release)(vmm_device_t *); /**< 释放回调 */
} vmm_device_type_t;

/**
 * @brief 设备基础结构，包含设备名、类、总线、驱动等核心属性
 */
struct vmm_device {
    /* Private fields (for device driver framework) */
    double_list_t           bus_head; /**< 总线链表节点 */
    double_list_t           class_head; /**< 类链表节点 */
    struct xref             ref_count; /**< 引用计数 */
    bool                    is_registered; /**< 是否已注册 */
    double_list_t           child_head; /**< 子节点链表头 */
    vmm_mutex_t             child_list_lock; /**< 子节点链表锁 */
    double_list_t           child_list; /**< 子节点链表 */
    vmm_spinlock_t          device_resource_lock; /**< 设备资源锁 */
    double_list_t           device_resource_head; /**< 设备资源链表头 */
    double_list_t           deferred_head; /**< 延迟链表头 */
    double_list_t           msi_list; /**< MSI链表 */
    vmm_msi_domain_t       *msi_domain; /**< MSI域 */
    /* Public fields */
    char                    name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    bool                    autoprobe_disabled; /**< 自动探测禁用标志 */
    vmm_bus_t              *bus; /**< 总线 */
    vmm_device_type_t *type; /**< 类型 */
    vmm_device_tree_node_t *of_node; /**< 设备树节点 */
    vmm_device_t           *parent; /**< 父节点 */
    vmm_class_t *class; /**< 类 */
    vmm_driver_t      *driver; /**< 驱动 */
    vmm_iommu_group_t *iommu_group; /**< IOMMU组 */
    void              *iommu_private; /**< IOMMU私有数据 */
    uint64_t          *dma_mask; /**< DMA掩码 */
    void              *pins; /**< 引脚 */
    void (*release)(vmm_device_t *); /**< 释放回调 */
    void *private; /**< 私有数据 */
};

/**
 * @brief 设备驱动结构，封装驱动的探测、移除和电源管理回调
 */
struct vmm_driver {
    /* Private fields (for device driver framework) */
    double_list_t                        head; /**< 链表头 */
    /* Public fields */
    char                                 name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    vmm_bus_t                           *bus; /**< 总线 */
    const vmm_device_tree_nodeid_t *match_table; /**< 匹配表 */
    int (*probe)(vmm_device_t *); /**< 探测函数 */
    int (*suspend)(vmm_device_t *, uint32_t); /**< 挂起 */
    int (*resume)(vmm_device_t *); /**< 恢复 */
    int (*remove)(vmm_device_t *); /**< 移除函数 */
};

/** Get driver data from device */
static inline void *vmm_device_driver_get_data(const vmm_device_t *dev)
{
    return (dev) ? dev->private : NULL;
}

/**
 * @brief 设置设备的驱动私有数据
 */
static inline void vmm_device_driver_set_data(vmm_device_t *dev, void *data)
{
    if (dev) {
        dev->private = data;
    }
}

/** Get MSI domain from device */
static inline vmm_msi_domain_t *vmm_device_driver_get_msi_domain(vmm_device_t *dev)
{
    return (dev) ? dev->msi_domain : NULL;
}

/**
 * @brief 设置设备的MSI中断域
 */
static inline void vmm_device_driver_set_msi_domain(vmm_device_t *dev, vmm_msi_domain_t *domain)
{
    if (dev) {
        dev->msi_domain = domain;
    }
}

/**
 * @brief 获取设备的DMA掩码
 */
static inline uint64_t vmm_dma_get_mask(vmm_device_t *dev)
{
    if (dev && dev->dma_mask && *dev->dma_mask) {
        return *dev->dma_mask;
    }

/**
 * @brief 生成指定宽度的DMA位掩码
 * @param 32 参数
 * @return 掩码值
 */
    return VMM_DMA_BIT_MASK(32);
}

/**
 * @brief 设置设备的DMA掩码
 */
static inline int vmm_dma_set_mask(vmm_device_t *dev, uint64_t mask)
{
    if (!dev->dma_mask) {
        return VMM_ERR_IO;
    }

    *dev->dma_mask = mask;
    return VMM_OK;
}

/**
 * @brief 注册设备类到设备驱动框架
 * @param cls 设备类结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_register_class(vmm_class_t *cls);

/**
 * @brief 从设备驱动框架注销设备类
 * @param cls 设备类结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_unregister_class(vmm_class_t *cls);

/**
 * @brief 根据名称查找已注册的设备类
 * @param cname 类名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_class_t *vmm_device_driver_find_class(const char *cname);

/* Iterate over each registered class */
/**
 * @brief 遍历所有已注册的设备类，对每个类执行回调函数
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_class_iterate(vmm_class_t *start, void *data, int (*fn)(vmm_class_t *cls, void *data));

/**
 * @brief 获取设备驱动类的数量
 * @return 数量值
 */
uint32_t vmm_device_driver_class_count(void);

/**
 * @brief 在设备类中查找匹配条件的设备
 * @param cls 设备类结构体指针
 * @param data 用户自定义数据指针
 * @param (*match 指针参数
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_class_find_device(vmm_class_t *cls, void *data, int (*match)(vmm_device_t *, void *));

/**
 * @brief 在设备类中根据名称查找设备
 * @param cls 设备类结构体指针
 * @param dname 设备名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_class_find_device_by_name(vmm_class_t *cls, const char *dname);

/**
 * @brief 遍历设备类中的所有设备，对每个设备执行回调
 * @param cls 设备类结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_class_device_iterate(vmm_class_t *cls, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data));

/* Count available devices in a class */
/**
 * @brief 获取设备驱动类设备的数量
 * @param cls 设备类结构体指针
 * @return 数量值
 */
uint32_t vmm_device_driver_class_device_count(vmm_class_t *cls);

/**
 * @brief 注册设备总线到设备驱动框架
 * @param bus 设备总线结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_register_bus(vmm_bus_t *bus);

/**
 * @brief 从设备驱动框架注销设备总线
 * @param bus 设备总线结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_unregister_bus(vmm_bus_t *bus);

/**
 * @brief 根据名称查找已注册的设备总线
 * @param bname 总线名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_bus_t *vmm_device_driver_find_bus(const char *bname);

/* Iterate over each registered bus */
/**
 * @brief 遍历所有已注册的设备总线，对每个总线执行回调
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_iterate(vmm_bus_t *start, void *data, int (*fn)(vmm_bus_t *bus, void *data));

/**
 * @brief 获取设备驱动总线的数量
 * @return 数量值
 */
uint32_t vmm_device_driver_bus_count(void);

/**
 * @brief 在总线上查找匹配条件的设备
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*match 指针参数
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_bus_find_device(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*match)(vmm_device_t *, void *));

/**
 * @brief 在总线上根据名称查找设备
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param dname 设备名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_bus_find_device_by_name(vmm_bus_t *bus, vmm_device_t *start, const char *dname);

/**
 * @brief 在总线上根据设备树节点查找设备
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param np 设备树节点指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_bus_find_device_by_node(vmm_bus_t *bus, vmm_device_t *start, vmm_device_tree_node_t *np);

/**
 * @brief 遍历总线上的所有设备，对每个设备执行回调
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_device_iterate(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data));

/**
 * @brief 获取设备驱动总线设备的数量
 * @param bus 设备总线结构体指针
 * @return 数量值
 */
uint32_t vmm_device_driver_bus_device_count(vmm_bus_t *bus);

/**
 * @brief 在总线上注册设备驱动
 * @param bus 设备总线结构体指针
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_register_driver(vmm_bus_t *bus, vmm_driver_t *drv);

/**
 * @brief 从总线上注销设备驱动
 * @param bus 设备总线结构体指针
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_unregister_driver(vmm_bus_t *bus, vmm_driver_t *drv);

/**
 * @brief 在总线上根据名称查找设备驱动
 * @param bus 设备总线结构体指针
 * @param dname 设备名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_driver_t *vmm_device_driver_bus_find_driver(vmm_bus_t *bus, const char *dname);

/**
 * @brief 遍历总线上的所有驱动，对每个驱动执行回调
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_driver_iterate(vmm_bus_t *bus, vmm_driver_t *start, void *data, int (*fn)(vmm_driver_t *drv, void *data));

/**
 * @brief 获取设备驱动总线驱动的数量
 * @param bus 设备总线结构体指针
 * @return 数量值
 */
uint32_t vmm_device_driver_bus_driver_count(vmm_bus_t *bus);

/**
 * @brief 在总线上注册设备事件通知器
 * @param bus 设备总线结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_register_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb);

/**
 * @brief 从总线上注销设备事件通知器
 * @param bus 设备总线结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_unregister_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb);

/* All 4 notifers below get called with the target struct device *
 * as an argument. Note that those functions are likely to be called
 * with the device lock held in the core, so be careful.
 */
#define VMM_BUS_NOTIFY_ADD_DEVICE 0x00000001   /* device added */
#define VMM_BUS_NOTIFY_DEL_DEVICE 0x00000002   /* device removed */
#define VMM_BUS_NOTIFY_BIND_DRIVER                                                                                                                   \
    0x00000003                                 /* driver about to be                                                                                 \
                              bound */
#define VMM_BUS_NOTIFY_BOUND_DRIVER 0x00000004 /* driver bound to device */
#define VMM_BUS_NOTIFY_UNBIND_DRIVER                                                                                                                 \
    0x00000005                                 /* driver about to be                                                                                 \
                                unbound */
#define VMM_BUS_NOTIFY_UNBOUND_DRIVER                                                                                                                \
    0x00000006                                 /* driver is unbound                                                                                  \
                                from the device */

/**
 * @brief 初始化设备结构体的基本字段
 * @param dev 设备结构体指针
 */
void vmm_device_driver_initialize_device(vmm_device_t *dev);

/**
 * @brief 增加设备的引用计数并返回设备指针
 * @param dev 设备结构体指针
 */
vmm_device_t *vmm_device_driver_ref_device(vmm_device_t *dev);

/**
 * @brief 减少设备的引用计数，计数归零时释放设备
 * @param dev 设备结构体指针
 */
void vmm_device_driver_dref_device(vmm_device_t *dev);

/**
 * @brief 检查设备是否已注册到设备驱动框架
 * @param dev 设备结构体指针
 * @return 已注册返回TRUE，否则返回FALSE
 */
bool vmm_device_driver_isregistered_device(vmm_device_t *dev);

/**
 * @brief 检查设备是否已绑定驱动
 * @param dev 设备结构体指针
 * @return 已绑定返回TRUE，否则返回FALSE
 */
bool vmm_device_driver_isattached_device(vmm_device_t *dev);

/**
 * @brief 遍历设备的所有子设备，对每个子设备执行回调
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_for_each_child(vmm_device_t *dev, void *data, int (*fn)(vmm_device_t *dev, void *data));

/**
 * @brief 将设备注册到设备驱动框架
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_register_device(vmm_device_t *dev);

/**
 * @brief 将设备附加到匹配的驱动（绑定设备与驱动）
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_attach_device(vmm_device_t *dev);

/**
 * @brief 将设备从驱动上分离（解绑设备与驱动）
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_dettach_device(vmm_device_t *dev);

/**
 * @brief 从设备驱动框架注销设备
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_unregister_device(vmm_device_t *dev);

/**
 * @brief 将驱动注册到设备驱动框架
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_register_driver(vmm_driver_t *drv);

/**
 * @brief 将驱动附加到匹配的设备（绑定驱动与设备）
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_attach_driver(vmm_driver_t *drv);

/**
 * @brief 将驱动从设备上分离（解绑驱动与设备）
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_dettach_driver(vmm_driver_t *drv);

/**
 * @brief 从设备驱动框架注销驱动
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_unregister_driver(vmm_driver_t *drv);

/**
 * @brief 初始化设备驱动
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_init(void);

#endif /* __VMM_DEVDRV_H_ */
