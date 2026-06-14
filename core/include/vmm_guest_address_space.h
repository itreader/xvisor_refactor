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
 * @file vmm_guest_address_space.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 客户地址空间头文件
 */
#ifndef _VMM_GUEST_ADDRESS_SPACE_H__
#define _VMM_GUEST_ADDRESS_SPACE_H__

#include <vmm_manager.h>
#include <vmm_notifier.h>

/* Notifier event when guest addr_space is initialized */
#define VMM_GUEST_ADDRESS_SPACE_EVENT_INIT   0x01
/* Notifier event when guest addr_space is about to be uninitialized */
#define VMM_GUEST_ADDRESS_SPACE_EVENT_DEINIT 0x02
/* Notifier event when guest addr_space is reset */
#define VMM_GUEST_ADDRESS_SPACE_EVENT_RESET  0x03

/** Representation of block device notifier event */
/**
 * @brief 客户地址空间事件，记录地址空间变化的通知数据
 */
struct vmm_guest_address_space_event {
    struct vmm_guest *guest; /**< 客户机 */
    void             *data; /**< 数据 */
};

/**
 * @brief 注册客户机地址空间的客户端回调
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销客户机地址空间的客户端回调
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_unregister_client(vmm_notifier_block_t *nb);

/**
 * @brief 遍历匹配标志的每个区域，寄存器标志=0x0匹配所有区域
 */
void vmm_guest_iterate_region(
    struct vmm_guest *guest, uint32_t reg_flags, void (*func)(struct vmm_guest *, struct vmm_region *, void *), void *private);

/** Find region corresponding to a guest physical address and also
 *  resolve aliased regions to real or virtual regions if required.
 */
struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest, physical_addr_t guest_physical_addr, uint32_t reg_flags, bool resolve_alias);

/**
 * @brief 查找给定客户物理地址和客户区域的映射
 */
void vmm_guest_find_mapping(
    struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t *hphys_addr, physical_size_t *avail_size);

/**
 * @brief 遍历客户区域的每个映射
 */
void vmm_guest_iterate_mapping(
    struct vmm_guest *guest, struct vmm_region *reg,
    void (*func)(
        struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t hphys_addr, physical_size_t phys_size,
        void *private),
    void *private);

/**
 * @brief 覆盖真实设备区域映射
 */
int vmm_guest_overwrite_real_device_mapping(
    struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t hphys_addr);

/**
 * @brief 客户机 内存 读
 * @param guest 指向客户机结构体的指针
 * @param guest_physical_addr 客户机物理地址
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_guest_memory_read(struct vmm_guest *guest, physical_addr_t guest_physical_addr, void *dst, uint32_t len, bool cacheable);

/**
 * @brief 客户机 内存 写
 * @param guest 指向客户机结构体的指针
 * @param guest_physical_addr 客户机物理地址
 * @param src 源设备树节点
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_guest_memory_write(struct vmm_guest *guest, physical_addr_t guest_physical_addr, void *src, uint32_t len, bool cacheable);

/**
 * @brief 将客户物理地址映射到主机物理地址
 */
int vmm_guest_physical_map(
    struct vmm_guest *guest, physical_addr_t guest_physical_addr, physical_size_t gphys_size, physical_addr_t *hphys_addr, physical_size_t *phys_size,
    uint32_t *reg_flags);

/**
 * @brief 取消客户机物理地址到主机地址的映射
 * @param guest 指向客户机结构体的指针
 * @param guest_physical_addr 客户机物理地址
 * @param phys_size 物理内存大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_physical_unmap(struct vmm_guest *guest, physical_addr_t guest_physical_addr, physical_size_t phys_size);

/**
 * @brief 从设备树节点添加客户机地址空间区域
 * @param guest 指向客户机结构体的指针
 * @param node 设备树节点指针
 * @param rprivate 私有资源数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_add_region_from_node(struct vmm_guest *guest, vmm_device_tree_node_t *node, void *rprivate);

/**
 * @brief 添加新的客户机内存区域
 */
int vmm_guest_add_region(
    struct vmm_guest *guest, vmm_device_tree_node_t *parent, const char *name, const char *device_type, const char *mainfest_type,
    const char *address_type, const char *compatible, uint32_t compatible_len, physical_addr_t guest_physical_addr, physical_addr_t alias_physical_addr,
    physical_size_t phys_size, uint32_t align_order, physical_addr_t hphys_addr, void *rprivate);

/** Get private pointer of guest region */
static inline void *vmm_guest_get_region_private(struct vmm_region *reg)
{
    return (reg) ? reg->private : NULL;
}

/**
 * @brief 设置客户区域的私有指针
 */
static inline void vmm_guest_set_region_private(struct vmm_region *reg, void *rprivate)
{
    if (reg) {
        reg->private = rprivate;
    }
}

/**
 * @brief 删除客户机地址空间中的指定区域
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @param del_node 待删除的节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_del_region(struct vmm_guest *guest, struct vmm_region *reg, bool del_node);

/**
 * @brief 复位客户地址空间
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_reset(struct vmm_guest *guest);

/**
 * @brief 初始化客户地址空间
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_init(struct vmm_guest *guest);

/**
 * @brief 反初始化客户机地址空间
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_deinit(struct vmm_guest *guest);

#endif
