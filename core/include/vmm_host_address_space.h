/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_address_space.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup patel (anup@brainfault.org)
 * @brief 虚拟地址空间管理头文件
 */

#ifndef __VMM_HOST_ASPACE_H_
#define __VMM_HOST_ASPACE_H_

#include <vmm_macros.h>
#include <vmm_types.h>

/** Page shift (or order), mask, and size */
#define VMM_PAGE_SHIFT            12
#define VMM_PAGE_SIZE             order_size(VMM_PAGE_SHIFT)
#define VMM_PAGE_MASK             order_mask(VMM_PAGE_SHIFT)

/** Convert address to page aligned address */
#define VMM_PAGE_ALIGN(x)         order_align(x, VMM_PAGE_SHIFT)

/** Roundup size to multiple of page size */
#define VMM_ROUNDUP2_PAGE_SIZE(x) roundup2_order_size(x, VMM_PAGE_SHIFT)

/** Calculate number of pages required to cover size x */
#define VMM_SIZE_TO_PAGE(x)       size_to_order(x, VMM_PAGE_SHIFT)

/** Convert pointer or virtual address
 *  to valid page base virtual address
 */
#define VMM_PAGE_ADDR(ptr)        (((virtual_addr_t)(ptr)) & ~VMM_PAGE_MASK)

/** Get page offset from pointer or virtual address */
#define VMM_PAGE_OFFSET(ptr)      (((virtual_addr_t)(ptr)) & VMM_PAGE_MASK)

/** Get nth page base address starting from page
 *  to which given pointer or virtual address belongs
 *  (Note: Unlike Linux, we assume that pointer or
 *   virtual address points to a contiguous memory)
 */
#define VMM_PAGE_NTH(ptr, n)      ((((virtual_addr_t)(ptr)) & ~VMM_PAGE_MASK) + ((n) << VMM_PAGE_SHIFT))

/** Align page frame address to page size */
#define VMM_PFN_ALIGN(x)          (((uint64_t)(x) + (VMM_PAGE_SIZE - 1)) & VMM_PAGE_MASK)

/** Round-up page frame number from page frame address */
#define VMM_PFN_UP(x)             (((x) + VMM_PAGE_SIZE - 1) >> VMM_PAGE_SHIFT)

/** Round-down page frame number from page frame address */
#define VMM_PFN_DOWN(x)           ((x) >> VMM_PAGE_SHIFT)

/** Page frame address from page frame number */
#define VMM_PFN_PHYS(x)           ((physical_addr_t)(x) << VMM_PAGE_SHIFT)

/**
 * @brief 主机内存映射标志，定义内存区域的读写和执行权限
 */
enum vmm_host_memory_flags {
    VMM_MEMORY_READABLE        = 0x00000001, /**< 0x00000001成员 */
    VMM_MEMORY_WRITEABLE       = 0x00000002, /**< 0x00000002成员 */
    VMM_MEMORY_EXECUTABLE      = 0x00000004, /**< 0x00000004成员 */
    VMM_MEMORY_CACHEABLE       = 0x00000008, /**< 0x00000008成员 */
    VMM_MEMORY_BUFFERABLE      = 0x00000010, /**< 0x00000010成员 */
    VMM_MEMORY_IO_DEVICE       = 0x00000020, /**< 0x00000020成员 */
    VMM_MEMORY_DMA_COHERENT    = 0x00000040, /**< 0x00000040成员 */
    VMM_MEMORY_DMA_NONCOHERENT = 0x00000080, /**< 0x00000080成员 */
};

#define VMM_MEMORY_FLAGS_NORMAL          (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_EXECUTABLE | VMM_MEMORY_CACHEABLE | VMM_MEMORY_BUFFERABLE)

#define VMM_MEMORY_FLAGS_NORMAL_NOCACHE  (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_EXECUTABLE)

#define VMM_MEMORY_FLAGS_NORMAL_WT       (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_EXECUTABLE | VMM_MEMORY_CACHEABLE)

#define VMM_MEMORY_FLAGS_DMA_COHERENT    (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_EXECUTABLE | VMM_MEMORY_DMA_COHERENT)

#define VMM_MEMORY_FLAGS_DMA_NONCOHERENT (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_EXECUTABLE | VMM_MEMORY_DMA_NONCOHERENT)

#define VMM_MEMORY_FLAGS_IO              (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE | VMM_MEMORY_EXECUTABLE | VMM_MEMORY_IO_DEVICE)

/**
 * @brief 获取主机内存映射哈希总量的数量
 * @return 数量值
 */
uint32_t vmm_host_memory_map_hash_total_count(void);

/**
 * @brief 获取主机内存映射哈希空闲的数量
 * @return 数量值
 */
uint32_t vmm_host_memory_map_hash_free_count(void);

/**
 * @brief 主机内存映射
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @param memory_flags 标志位
 * @return 数量值
 */
virtual_addr_t vmm_host_memory_map(physical_addr_t pa, virtual_size_t size, uint32_t memory_flags);

/**
 * @brief 取消主机物理地址到虚拟地址的映射
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_memory_unmap(virtual_addr_t va);

/**
 * @brief 将IO物理内存映射到虚拟内存
 */
static inline virtual_addr_t vmm_host_iomap(physical_addr_t pa, virtual_size_t size)
{
/**
 * @brief 主机内存映射
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @param VMM_MEMORY_FLAGS_IO 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return vmm_host_memory_map(pa, size, VMM_MEMORY_FLAGS_IO);
}

/**
 * @brief 解除IO虚拟内存映射
 */
static inline int vmm_host_iounmap(virtual_addr_t va)
{
/**
 * @brief 取消主机物理地址到虚拟地址的映射
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return vmm_host_memory_unmap(va);
}

/**
 * @brief 获取主机大页的位移值
 * @return 大页移位值（log2大小），不支持则返回0
 */
uint32_t vmm_host_huge_page_shift(void);

/**
 * @brief 获取主机大页的大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_host_huge_page_size(void);

/**
 * @brief 分配大页内存
 * @param page_count 数量
 * @param memory_flags 标志位
 * @return 大小值（字节）
 */
virtual_addr_t vmm_host_alloc_huge_pages(uint32_t page_count, uint32_t memory_flags);

/**
 * @brief 获取主机当前空闲的大页的数量
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_free_huge_pages(virtual_addr_t page_va, uint32_t page_count);

/**
 * @brief 分配对齐的物理页
 * @param page_count 数量
 * @param align_order 阶数
 * @param memory_flags 标志位
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_addr_t vmm_host_alloc_aligned_pages(uint32_t page_count, uint32_t align_order, uint32_t memory_flags);

/**
 * @brief 分配物理页
 * @param page_count 数量
 * @param memory_flags 标志位
 * @return 成功返回分配结果，失败返回错误码
 */
virtual_addr_t vmm_host_alloc_pages(uint32_t page_count, uint32_t memory_flags);

/**
 * @brief 获取主机当前空闲的物理页的数量
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_free_pages(virtual_addr_t page_va, uint32_t page_count);

/**
 * @brief 主机虚拟地址转物理地址
 * @param va 待操作的虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtualAddr_to_physicalAddr(virtual_addr_t va, physical_addr_t *pa);

/**
 * @brief 主机物理地址转虚拟地址
 * @param pa 待操作的物理地址
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_physicalAddr_to_virtualAddr(physical_addr_t pa, virtual_addr_t *va);

/**
 * @brief 从主机物理地址读取数据
 * @param hpa 主机物理地址
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_host_memory_read(physical_addr_t hpa, void *dst, uint32_t len, bool cacheable);

/**
 * @brief 向主机物理地址写入数据
 * @param hpa 主机物理地址
 * @param src 源设备树节点
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_host_memory_write(physical_addr_t hpa, void *src, uint32_t len, bool cacheable);

/**
 * @brief 设置主机物理地址区域的内存值
 * @param hpa 主机物理地址
 * @param byte 字节值
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际设置的字节数，失败返回0
 */
uint32_t vmm_host_memory_set(physical_addr_t hpa, uint8_t byte, uint32_t len, bool cacheable);

/**
 * @brief 释放初始化完成后不再使用的内存
 * @return 释放的初始化内存大小（KB）
 */
uint32_t vmm_host_free_initmem(void);

/**
 * @brief 初始化主机地址空间
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_address_space_init(void);

#endif /* __VMM_HOST_ASPACE_H_ */
