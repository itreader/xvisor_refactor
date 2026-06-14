/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_share_memory.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 共享内存子系统头文件
 */
#ifndef __VMM_SHMEM_H__
#define __VMM_SHMEM_H__

#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_limits.h>
#include <vmm_types.h>

/**
 * @brief 共享内存结构，定义客户机之间共享的内存段属性
 */
typedef struct vmm_share_memory {
    double_list_t   head; /**< 链表头 */
    struct xref     ref_count; /**< 引用计数 */
    char            name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    physical_addr_t addr; /**< 地址 */
    physical_size_t size; /**< 大小 */
    uint32_t        align_order; /**< 对齐阶 */
    void *private; /**< 私有数据 */
} vmm_share_memory_t;

/**
 * @brief 从共享内存区域读取数据
 * @param share_memory 共享内存结构体指针
 * @param off 偏移量
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_share_memory_read(vmm_share_memory_t *share_memory, physical_addr_t off, void *dst, uint32_t len, bool cacheable);

/**
 * @brief 向共享内存区域写入数据
 * @param share_memory 共享内存结构体指针
 * @param off 偏移量
 * @param src 源设备树节点
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_share_memory_write(vmm_share_memory_t *share_memory, physical_addr_t off, void *src, uint32_t len, bool cacheable);

/**
 * @brief 设置共享内存区域的数据
 * @param share_memory 共享内存结构体指针
 * @param off 偏移量
 * @param byte 字节值
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际设置的字节数，失败返回0
 */
uint32_t vmm_share_memory_set(vmm_share_memory_t *share_memory, physical_addr_t off, uint8_t byte, uint32_t len, bool cacheable);

/**
 * @brief 遍历共享内存中的条目
 * @param (*iter 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_share_memory_iterate(int (*iter)(vmm_share_memory_t *, void *), void *private);

/**
 * @brief 获取共享内存的数量
 * @return 数量值
 */
uint32_t vmm_share_memory_count(void);

/**
 * @brief 按名称查找共享内存区域
 * @param name 目标对象的名称
 */
vmm_share_memory_t *vmm_share_memory_find_byname(const char *name);

/**
 * @brief 增加共享内存的引用计数
 * @param share_memory 共享内存结构体指针
 */
void vmm_share_memory_ref(vmm_share_memory_t *share_memory);

/**
 * @brief 减少共享内存的引用计数
 * @param share_memory 共享内存结构体指针
 */
void vmm_share_memory_dref(vmm_share_memory_t *share_memory);

/**
 * @brief 创建共享内存
 * @param name 目标对象的名称
 * @param size 数据大小（字节数）
 * @param align_order 阶数
 * @param private 私有数据指针
 */
vmm_share_memory_t *vmm_share_memory_create(const char *name, physical_size_t size, uint32_t align_order, void *private);

/**
 * @brief 销毁共享内存实例
 */
static inline void vmm_share_memory_destroy(vmm_share_memory_t *share_memory)
{
    if (share_memory) {
        vmm_share_memory_dref(share_memory);
    }
}

/** Get name of shared memory instance */
static inline const char *vmm_share_memory_get_name(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->name : NULL;
}

/**
 * @brief 获取共享内存实例地址
 */
static inline physical_addr_t vmm_share_memory_get_addr(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->addr : 0x0;
}

/**
 * @brief 获取共享内存实例大小
 */
static inline physical_size_t vmm_share_memory_get_size(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->size : 0x0;
}

/**
 * @brief 获取共享内存对齐阶数
 */
static inline uint32_t vmm_share_memory_get_align_order(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->align_order : 0;
}

/**
 * @brief 获取共享内存引用计数
 */
static inline long vmm_share_memory_get_ref_count(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? xref_val(&share_memory->ref_count) : 0;
}

/** Get private pointer of shared memory instance */
static inline void *vmm_share_memory_get_private(vmm_share_memory_t *share_memory)
{
    return (share_memory) ? share_memory->private : NULL;
}

/**
 * @brief 设置共享内存实例的私有指针
 */
static inline void vmm_share_memory_set_private(vmm_share_memory_t *share_memory, void *private)
{
    if (share_memory) {
        share_memory->private = private;
    }
}

/**
 * @brief 初始化共享内存
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_share_memory_init(void);

#endif
