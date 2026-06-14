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
 * @file vmm_iommu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备直通IOMMU框架头文件
 *
 * The source has been largely adapted from Linux sources:
 * include/linux/iommu.h
 *
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * The original source is licensed under GPL.
 */
#ifndef _VMM_IOMMU_H__
#define _VMM_IOMMU_H__

#include <libs/xref.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_types.h>

#define VMM_IOMMU_CONTROLLER_CLASS_NAME "iommu"

struct vmm_iommu_ops;
struct vmm_iommu_group;
struct vmm_iommu_domain;
struct vmm_notifier_block;

typedef struct vmm_notifier_block vmm_notifier_block_t;
typedef struct vmm_iommu_ops      vmm_iommu_ops_t;
typedef struct vmm_iommu_domain   vmm_iommu_domain_t;
typedef struct vmm_iommu_group    vmm_iommu_group_t;

/* nodeid table based IOMMU initialization callback */
typedef int (*vmm_iommu_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for IOMMU */
#define VMM_IOMMU_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "iommu", "", "", compat, fn)

/**
 * @brief IOMMU控制器结构，管理一组IOMMU设备和域
 */
typedef struct vmm_iommu_controller {
    /* Public members */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    /* Private members */
    vmm_device_t  dev; /**< 设备 */
    vmm_mutex_t   groups_lock; /**< groups_lock成员 */
    double_list_t groups; /**< groups成员 */
    vmm_mutex_t   domains_lock; /**< domains_lock成员 */
    double_list_t domains; /**< domains成员 */
} vmm_iommu_controller_t;

/**
 * @brief IOMMU设备组，将共享IOMMU上下文的设备分组管理
 */
struct vmm_iommu_group {
    char                   *name; /**< 名称 */
    vmm_iommu_controller_t *ctrl; /**< 控制 */
    double_list_t           head; /**< 链表头 */

    struct xref                   ref_count; /**< 引用计数 */
    vmm_mutex_t                   mutex; /**< 互斥锁 */
    vmm_iommu_domain_t           *domain; /**< 域 */
    double_list_t                 devices; /**< devices成员 */
    vmm_blocking_notifier_chain_t notifier; /**< notifier成员 */
    void                         *iommu_data; /**< iommu_data成员 */
    void (*iommu_data_release)(void *iommu_data); /**< iommu_data_release成员 */
};

/* iommu mapping attributes */
#define VMM_IOMMU_READ            (1 << 0)
#define VMM_IOMMU_WRITE           (1 << 1)
#define VMM_IOMMU_CACHE           (1 << 2) /* DMA cache coherency */
#define VMM_IOMMU_NOEXEC          (1 << 3)
#define VMM_IOMMU_MMIO            (1 << 4)

/* Domain feature flags */
#define __VMM_IOMMU_DOMAIN_PAGING (1U << 0)  /* Support for iommu_map/unmap */
#define __VMM_IOMMU_DOMAIN_DMA_API                                                                                                                   \
    (1U << 1)                                /* Domain for use in DMA-API                                                                            \
                            implementation              */
#define __VMM_IOMMU_DOMAIN_PT      (1U << 2) /* Domain is identity mapped   */

/*
 * This are the possible domain-types
 *
 *  VMM_IOMMU_DOMAIN_BLOCKED    - All DMA is blocked, can be used to isolate
 *                    devices
 *  VMM_IOMMU_DOMAIN_IDENTITY   - DMA addresses are system physical addresses
 *  VMM_IOMMU_DOMAIN_UNMANAGED  - DMA mappings managed by IOMMU-API user, used
 *                    for VMs
 *  VMM_IOMMU_DOMAIN_DMA        - Internally used for DMA-API implementations.
 *                    This flag allows IOMMU drivers to implement
 *                    certain optimizations for these domains
 */
#define VMM_IOMMU_DOMAIN_BLOCKED   (0U)
#define VMM_IOMMU_DOMAIN_IDENTITY  (__VMM_IOMMU_DOMAIN_PT)
#define VMM_IOMMU_DOMAIN_UNMANAGED (__VMM_IOMMU_DOMAIN_PAGING)
#define VMM_IOMMU_DOMAIN_DMA       (__VMM_IOMMU_DOMAIN_PAGING | __VMM_IOMMU_DOMAIN_DMA_API)
/* iommu fault flags */
#define VMM_IOMMU_FAULT_READ       0x0
#define VMM_IOMMU_FAULT_WRITE      0x1

typedef int (*vmm_iommu_fault_handler_t)(vmm_iommu_domain_t *, vmm_device_t *, physical_addr_t, int, void *);

struct vmm_iommu_domain_geometry {
    dma_addr_t aperture_start; /* First address that can be mapped    */
    dma_addr_t aperture_end;   /* Last address that can be mapped     */
    bool       force_aperture; /* DMA only allowed in mappable range? */
};

/**
 * @brief IOMMU地址域，定义一组设备的地址翻译和保护规则
 */
struct vmm_iommu_domain {
    /* Public members */
    char                    name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t                type; /**< 类型 */
    vmm_bus_t              *bus; /**< 总线 */
    vmm_iommu_controller_t *ctrl; /**< 控制 */
    /* Private members */
    double_list_t           head; /**< 链表头 */
    struct xref             ref_count; /**< 引用计数 */
    vmm_iommu_ops_t        *ops; /**< 操作集 */
    void *private; /**< 私有数据 */
    vmm_iommu_fault_handler_t        handler; /**< 处理函数 */
    void                            *handler_token; /**< handler_token成员 */
    struct vmm_iommu_domain_geometry geometry; /**< 几何参数 */
};

enum vmm_iommu_cap {
    VMM_IOMMU_CAP_CACHE_COHERENCY, /* IOMMU can enforce cache coherent DMA
                      transactions */
    VMM_IOMMU_CAP_INTR_REMAP,      /* IOMMU supports interrupt isolation */
    VMM_IOMMU_CAP_NOEXEC,          /* IOMMU_NOEXEC flag */
};

/*
 * Following constraints are specifc to FSL_PAMUV1:
 *  -aperture must be power of 2, and naturally aligned
 *  -number of windows must be power of 2, and address space size
 *   of each window is determined by aperture size / # of windows
 *  -the actual size of the mapped region of a window must be power
 *   of 2 starting with 4KB and physical address must be naturally
 *   aligned.
 * DOMAIN_ATTR_FSL_PAMUV1 corresponds to the above mentioned contraints.
 * The caller can invoke iommu_domain_get_attr to check if the underlying
 * iommu implementation supports these constraints.
 */
enum vmm_iommu_attr {
    VMM_DOMAIN_ATTR_GEOMETRY,
    VMM_DOMAIN_ATTR_PAGING,
    VMM_DOMAIN_ATTR_WINDOWS,
    VMM_DOMAIN_ATTR_FSL_PAMU_STASH,
    VMM_DOMAIN_ATTR_FSL_PAMU_ENABLE,
    VMM_DOMAIN_ATTR_FSL_PAMUV1,
    VMM_DOMAIN_ATTR_MAX,
};

/**
 * IOMMU ops and capabilities
 * @capable: check capability
 * @domain_alloc: allocate iommu domain
 * @domain_free: free iommu domain
 * @attach_dev: attach device to an iommu domain
 * @detach_dev: detach device from an iommu domain
 * @map: map a physically contiguous memory region to an iommu domain
 * @unmap: unmap a physically contiguous memory region from an iommu domain
 * @iova_to_phys: translate iova to physical address
 * @add_device: add device to iommu grouping
 * @remove_device: remove device from iommu grouping
 * @domain_get_attr: Query domain attributes
 * @domain_set_attr: Change domain attributes
 * @domain_window_enable: Configure and enable a particular window for a domain
 * @domain_window_disable: Disable a particular window for a domain
 * @domain_set_windows: Set the number of windows for a domain
 * @domain_get_windows: Return the number of windows for a domain
 * @of_xlate: add OF master IDs to iommu grouping
 * @pgsize_bitmap: bitmap of all possible supported page sizes
 */
struct vmm_iommu_ops {
    bool (*capable)(enum vmm_iommu_cap); /**< capable成员 */

    vmm_iommu_domain_t *(*domain_alloc)(uint32_t type, vmm_iommu_controller_t *ctrl); /**< domain_alloc成员 */
    void (*domain_free)(vmm_iommu_domain_t *domain); /**< domain_free成员 */
    int (*attach_dev)(vmm_iommu_domain_t *domain, vmm_device_t *dev); /**< 附加设备 */
    void (*detach_dev)(vmm_iommu_domain_t *domain, vmm_device_t *dev); /**< 分离设备 */
    int (*map)(vmm_iommu_domain_t *domain, physical_addr_t iova, physical_addr_t paddr, size_t size, int prot); /**< 映射 */
    size_t (*unmap)(vmm_iommu_domain_t *domain, physical_addr_t iova, size_t size); /**< unmap成员 */
    physical_addr_t (*iova_to_phys)(vmm_iommu_domain_t *domain, physical_addr_t iova); /**< IOVA转物理地址 */
    int (*add_device)(vmm_device_t *dev); /**< add_device成员 */
    void (*remove_device)(vmm_device_t *dev); /**< remove_device成员 */

    int (*domain_get_attr)(vmm_iommu_domain_t *domain, enum vmm_iommu_attr attr, void *data); /**< domain_get_attr成员 */
    int (*domain_set_attr)(vmm_iommu_domain_t *domain, enum vmm_iommu_attr attr, void *data); /**< domain_set_attr成员 */

    /* Window handling functions */
    int (*domain_window_enable)(vmm_iommu_domain_t *domain, uint32_t wnd_nr, physical_addr_t paddr, uint64_t size, int prot); /**< domain_window_enable成员 */
    void (*domain_window_disable)(vmm_iommu_domain_t *domain, uint32_t wnd_nr); /**< domain_window_disable成员 */
    /* Set the numer of window per domain */
    int (*domain_set_windows)(vmm_iommu_domain_t *domain, uint32_t w_count); /**< domain_set_windows成员 */
    /* Get the numer of window per domain */
    uint32_t (*domain_get_windows)(vmm_iommu_domain_t *domain); /**< domain_get_windows成员 */

    int (*of_xlate)(vmm_device_t *dev, struct vmm_device_tree_phandle_args *args); /**< of_xlate成员 */

    uint64_t pgsize_bitmap; /**< 页大小位图 */
};

#define VMM_IOMMU_GROUP_NOTIFY_ADD_DEVICE     1 /* Device added */
#define VMM_IOMMU_GROUP_NOTIFY_DEL_DEVICE     2 /* Pre Device removed */
#define VMM_IOMMU_GROUP_NOTIFY_BIND_DRIVER    3 /* Pre Driver bind */
#define VMM_IOMMU_GROUP_NOTIFY_BOUND_DRIVER   4 /* Post Driver bind */
#define VMM_IOMMU_GROUP_NOTIFY_UNBIND_DRIVER  5 /* Pre Driver unbind */
#define VMM_IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER 6 /* Post Driver unbind */

/* =============== IOMMU Controller APIs =============== */

/**
 * @brief 注册IOMMU控制器
 * @param ctrl 控制器结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_register(vmm_iommu_controller_t *ctrl);

/**
 * @brief 注销IOMMU控制器
 * @param ctrl 控制器结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_unregister(vmm_iommu_controller_t *ctrl);

/**
 * @brief 查找IOMMU控制器
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_iommu_controller_t *vmm_iommu_controller_find(const char *name);

/**
 * @brief 遍历所有已注册的IOMMU控制器
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_iterate(vmm_iommu_controller_t *start, void *data, int (*fn)(vmm_iommu_controller_t *, void *));

/**
 * @brief 获取IOMMU控制器的数量
 * @return 数量值
 */
uint32_t vmm_iommu_controller_count(void);

/**
 * @brief 遍历IOMMU控制器下的所有IOMMU组
 * @param ctrl 控制器结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_for_each_group(vmm_iommu_controller_t *ctrl, void *data, int (*fn)(vmm_iommu_group_t *, void *));

/**
 * @brief 获取IOMMU设备组的数量
 * @param ctrl 控制器结构体指针
 * @return 数量值
 */
uint32_t vmm_iommu_controller_group_count(vmm_iommu_controller_t *ctrl);

/**
 * @brief 遍历IOMMU控制器下的所有IOMMU域
 * @param ctrl 控制器结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_controller_for_each_domain(vmm_iommu_controller_t *ctrl, void *data, int (*fn)(vmm_iommu_domain_t *, void *));

/**
 * @brief 获取IOMMU域的数量
 * @param ctrl 控制器结构体指针
 * @return 数量值
 */
uint32_t vmm_iommu_controller_domain_count(vmm_iommu_controller_t *ctrl);

/* =============== IOMMU Group APIs =============== */

/**
 * @brief 分配IOMMU设备组
 * @param name 目标对象的名称
 * @param ctrl 控制器结构体指针
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_iommu_group_t *vmm_iommu_group_alloc(const char *name, vmm_iommu_controller_t *ctrl);

/**
 * @brief 获取指定名称的IOMMU组
 * @param dev 设备结构体指针
 */
vmm_iommu_group_t *vmm_iommu_group_get(vmm_device_t *dev);

/**
 * @brief 释放IOMMU设备组
 * @param group 组结构体指针
 */
void vmm_iommu_group_free(vmm_iommu_group_t *group);
#define vmm_iommu_group_put(group) vmm_iommu_group_free(group)

/**
 * @brief 获取IOMMU设备组的ID查找
 * @param id 标识符值
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_group_t *vmm_iommu_group_get_by_id(int id);

/**
 * @brief 获取IOMMU设备组的IOMMU私有数据
 * @param group 组结构体指针
 */
void *vmm_iommu_group_get_iommudata(vmm_iommu_group_t *group);

/**
 * @brief 设置IOMMU设备组的IOMMU私有数据
 * @param group 组结构体指针
 * @param iommu_data IOMMU私有数据指针
 * @param (*release 指针参数
 */
void vmm_iommu_group_set_iommudata(vmm_iommu_group_t *group, void *iommu_data, void (*release)(void *iommu_data));

/**
 * @brief 将设备添加到IOMMU组
 * @param group 组结构体指针
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_add_device(vmm_iommu_group_t *group, vmm_device_t *dev);

/**
 * @brief 从IOMMU组中移除设备
 * @param dev 设备结构体指针
 */
void vmm_iommu_group_remove_device(vmm_device_t *dev);

/**
 * @brief 遍历IOMMU组中的所有设备
 * @param group 组结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_for_each_dev(vmm_iommu_group_t *group, void *data, int (*fn)(vmm_device_t *, void *));

/**
 * @brief 注册IOMMU组通知器
 * @param group 组结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_register_notifier(vmm_iommu_group_t *group, vmm_notifier_block_t *nb);

/**
 * @brief 注销IOMMU组通知器
 * @param group 组结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_unregister_notifier(vmm_iommu_group_t *group, vmm_notifier_block_t *nb);

/**
 * @brief 获取IOMMU组的名称
 * @param group 组结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_iommu_group_name(vmm_iommu_group_t *group);

/**
 * @brief 获取IOMMU组所属的IOMMU控制器
 * @param group 组结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_controller_t *vmm_iommu_group_controller(vmm_iommu_group_t *group);

/**
 * @brief 将IOMMU域附加到IOMMU组
 * @param group 组结构体指针
 * @param domain 域结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_attach_domain(vmm_iommu_group_t *group, vmm_iommu_domain_t *domain);

/**
 * @brief 从IOMMU组分离IOMMU域
 * @param group 组结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_group_detach_domain(vmm_iommu_group_t *group);

/**
 * @brief 获取IOMMU设备组的域
 * @param group 组结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
vmm_iommu_domain_t *vmm_iommu_group_get_domain(vmm_iommu_group_t *group);

/* =============== IOMMU Domain APIs =============== */

/**
 * @brief 分配IOMMU域
 * @param name 目标对象的名称
 * @param bus 设备总线结构体指针
 * @param ctrl 控制器结构体指针
 * @param type 类型标识值
 */
vmm_iommu_domain_t *vmm_iommu_domain_alloc(const char *name, vmm_bus_t *bus, vmm_iommu_controller_t *ctrl, uint32_t type);

/**
 * @brief 增加IOMMU域的引用计数
 * @param domain 域结构体指针
 */
void vmm_iommu_domain_ref(vmm_iommu_domain_t *domain);

/**
 * @brief 释放IOMMU域
 * @param domain 域结构体指针
 */
void vmm_iommu_domain_free(vmm_iommu_domain_t *domain);
#define vmm_iommu_domain_dref(domain) vmm_iommu_domain_free(domain)

/**
 * @brief 设置IOMMU的故障处理回调
 * @param domain 域结构体指针
 * @param handler 信号处理函数指针
 * @param token 令牌字符串
 */
void vmm_iommu_set_fault_handler(vmm_iommu_domain_t *domain, vmm_iommu_fault_handler_t handler, void *token);

/**
 * @brief 向IOMMU框架报告IOMMU故障事件
 */
static inline int vmm_report_iommu_fault(vmm_iommu_domain_t *domain, vmm_device_t *dev, physical_addr_t iova, int flags)
{
    int ret = VMM_ERR_NOSYS;

    /*
     * if upper layers showed interest and installed a fault handler,
     * invoke it.
     */
    if (domain->handler) {
        ret = domain->handler(domain, dev, iova, flags, domain->handler_token);
    }

    return ret;
}

/**
 * @brief 将IOMMU域的IO虚拟地址转换为物理地址
 * @param domain 域结构体指针
 * @param iova IO虚拟地址
 * @return 转换结果
 */
physical_addr_t vmm_iommu_iova_to_phys(vmm_iommu_domain_t *domain, physical_addr_t iova);

/**
 * @brief 建立IOMMU地址映射
 * @param domain 域结构体指针
 * @param iova IO虚拟地址
 * @param paddr 物理地址值
 * @param size 数据大小（字节数）
 * @param prot 内存保护标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_map(vmm_iommu_domain_t *domain, physical_addr_t iova, physical_addr_t paddr, size_t size, int prot);

/**
 * @brief IOMMU解除地址映射
 * @param domain 域结构体指针
 * @param iova IO虚拟地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
size_t vmm_iommu_unmap(vmm_iommu_domain_t *domain, physical_addr_t iova, size_t size);

/**
 * @brief 启用IOMMU域的DMA窗口
 * @param domain 域结构体指针
 * @param wnd_nr 窗口编号
 * @param offset 偏移量（字节）
 * @param size 数据大小（字节数）
 * @param prot 内存保护标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_domain_window_enable(vmm_iommu_domain_t *domain, uint32_t wnd_nr, physical_addr_t offset, uint64_t size, int prot);

/**
 * @brief 禁用IOMMU域的DMA窗口
 * @param domain 域结构体指针
 * @param wnd_nr 窗口编号
 */
void vmm_iommu_domain_window_disable(vmm_iommu_domain_t *domain, uint32_t wnd_nr);

/**
 * @brief 获取IOMMU域的attr
 * @param domain 域结构体指针
 * @param vmm_iommu_attr IOMMU属性值
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_domain_get_attr(vmm_iommu_domain_t *domain, enum vmm_iommu_attr, void *data);

/**
 * @brief 设置IOMMU域的attr
 * @param domain 域结构体指针
 * @param vmm_iommu_attr IOMMU属性值
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_iommu_domain_set_attr(vmm_iommu_domain_t *domain, enum vmm_iommu_attr, void *data);

/* =============== IOMMU Misc APIs =============== */

/**
 * @brief 设置总线的IOMMU
 * @param bus 设备总线结构体指针
 * @param ops 操作集结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_bus_set_iommu(vmm_bus_t *bus, vmm_iommu_ops_t *ops);

/**
 * @brief 检查IOMMU是否存在
 * @param bus 设备总线结构体指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_iommu_present(vmm_bus_t *bus);

/**
 * @brief 检查IOMMU是否具备指定能力
 * @param bus 设备总线结构体指针
 * @param cap 能力值或容量值
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_iommu_capable(vmm_bus_t *bus, enum vmm_iommu_cap cap);

/**
 * @brief 初始化IOMMU
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_iommu_init(void);

#endif
