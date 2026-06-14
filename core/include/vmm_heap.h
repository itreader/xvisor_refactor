/**
 * Copyright (c) 2010 Himanshu Chauhan.
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_heap.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @author Ankit Jindal (thatsjindal@gmail.com)
 * @brief 堆管理接口头文件
 */
#ifndef _VMM_HEAP_H__
#define _VMM_HEAP_H__

#include <vmm_types.h>

struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * @brief malloc
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_malloc(virtual_size_t size);

/**
 * @brief zalloc
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_zalloc(virtual_size_t size);

/**
 * @brief calloc
 * @param element_count 元素数量
 * @param element_size 单个元素大小（字节）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_calloc(virtual_size_t element_count, virtual_size_t element_size);

/**
 * @brief strdup
 * @param str 待处理的字符串
 * @return 成功返回分配的内存指针，失败返回NULL
 */
char *vmm_strdup(const char *str);

/** Create a duplicate const string */
static inline const char *vmm_strdup_const(const char *str)
{
/**
 * @brief strdup
 * @param str 待处理的字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return vmm_strdup(str);
}

/**
 * @brief 分配指定大小的内存
 * @param ptr 通用指针
 * @return 大小值（字节）
 */
virtual_size_t vmm_alloc_size(const void *ptr);

/**
 * @brief free
 * @param ptr 通用指针
 */
void vmm_free(void *ptr);

/* Translate a normal address to its virtual address */
/**
 * @brief 物理地址转换为虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_normal_physicalAddr_to_virtualAddr(physical_addr_t pa);

/* Translate a normal virtual address to its physical address */
/**
 * @brief 虚拟地址转换为物理地址
 * @param va 待操作的虚拟地址
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_normal_virtualAddr_to_physicalAddr(virtual_addr_t va);

/**
 * @brief 获取普通堆的起始虚拟地址
 * @return 获取到的值，失败返回错误码
 */
virtual_addr_t vmm_normal_heap_start_va(void);

/**
 * @brief 普通 堆 大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_normal_heap_size(void);

/**
 * @brief 获取普通堆管理元数据的大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_normal_heap_hksize(void);

/**
 * @brief 获取普通堆的空闲内存大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_normal_heap_free_size(void);

/**
 * @brief 将普通堆的分配状态输出到字符设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_normal_heap_print_state(vmm_char_device_t *cdev);

/**
 * @brief 初始化堆
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_heap_init(void);

/** Possible DMA directions */
/**
 * @brief DMA传输方向枚举，定义内存到设备、设备到内存等方向
 */
enum vmm_dma_direction {
    DMA_BIDIRECTIONAL = 0, /**< 0 */
    DMA_TO_DEVICE     = 1, /**< 1 */
    DMA_FROM_DEVICE   = 2, /**< 2 */
    DMA_NONE          = 3, /**< 3 */
};

/**
 * @brief DMA malloc
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_dma_malloc(virtual_size_t size);

/**
 * @brief DMA zalloc
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_dma_zalloc(virtual_size_t size);

/**
 * @brief 分配DMA物理内存并清零
 * @param size 数据大小（字节数）
 * @param paddr 物理地址值
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_dma_zalloc_phy(virtual_size_t size, physical_addr_t *paddr);

/* Translate a DMA physical address to its virtual address */
/**
 * @brief DMA物理地址转虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回分配结果，失败返回错误码
 */
virtual_addr_t vmm_dma_physicalAddr_to_virtualAddr(physical_addr_t pa);

/* Translate a DMA virtual address to its physical address */
/**
 * @brief DMA虚拟地址转物理地址
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
physical_addr_t vmm_dma_virtualAddr_to_physicalAddr(virtual_addr_t va);

/**
 * @brief is  DMA
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_is__dma(void *va);

/* Sync the buffer to be "owned" by the device */
/**
 * @brief 将DMA缓冲区同步到设备（CPU写完成后交给设备读取）
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param dir 方向标识
 */
void vmm_dma_sync_for_device(virtual_addr_t start, virtual_addr_t end, enum vmm_dma_direction dir);

/* Sync the buffer to be "owned" by the CPU */
/**
 * @brief 将DMA缓冲区同步到CPU（设备写完成后交给CPU读取）
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param dir 方向标识
 */
void vmm_dma_sync_for_cpu(virtual_addr_t start, virtual_addr_t end, enum vmm_dma_direction dir);

/* Map a DMA buffer for the device */
/**
 * @brief DMA 映射
 * @param vaddr 虚拟地址值
 * @param size 数据大小（字节数）
 * @param dir 方向标识
 * @return 成功返回VMM_OK，失败返回错误码
 */
physical_addr_t vmm_dma_map(virtual_addr_t vaddr, virtual_size_t size, enum vmm_dma_direction dir);

/* Unmap the DMA buffer, which can be then read by the CPU */
/**
 * @brief DMA unmap
 * @param daddr 目标物理地址
 * @param size 数据大小（字节数）
 * @param dir 方向标识
 */
void vmm_dma_unmap(physical_addr_t daddr, physical_size_t size, enum vmm_dma_direction dir);

/**
 * @brief 从DMA堆中分配指定大小的内存
 * @param ptr 通用指针
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_alloc_size(const void *ptr);

/**
 * @brief 释放DMA
 * @param ptr 通用指针
 */
void vmm_dma_free(void *ptr);

/**
 * @brief 获取DMA堆的起始虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_addr_t vmm_dma_heap_start_va(void);

/**
 * @brief 获取DMA堆的总大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_heap_size(void);

/**
 * @brief 获取DMA堆管理元数据的大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_heap_hksize(void);

/**
 * @brief 获取DMA堆的空闲内存大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_dma_heap_free_size(void);

/**
 * @brief 将DMA堆的分配状态输出到字符设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_dma_heap_print_state(vmm_char_device_t *cdev);

/**
 * @brief 初始化DMA堆
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_dma_heap_init(void);

#endif
