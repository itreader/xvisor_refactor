/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file vmm_page_pool.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 页池子系统头文件
 *
 * This subsystem provides managed page allocations so that
 * we can track page allocations and also use huge_pages for
 * all page allocations.
 */
#ifndef _VMM_PAGE_POOL_H__
#define _VMM_PAGE_POOL_H__

#include <vmm_types.h>

/**
 * @brief 页面池类型枚举，区分不同用途的内存页池
 */
enum vmm_page_pool_type {
    VMM_PAGE_POOL_NORMAL = 0, /**< 0 */
    VMM_PAGE_POOL_NORMAL_NOCACHE,
    VMM_PAGE_POOL_NORMAL_WT,
    VMM_PAGE_POOL_DMA_COHERENT,
    VMM_PAGE_POOL_DMA_NONCOHERENT,
    VMM_PAGE_POOL_IO,
    VMM_PAGE_POOL_MAX
};

/**
 * @brief 页 池 名称
 * @param page_type 页面类型标识
 * @return 成功返回目标指针，失败返回NULL
 */
const char *vmm_page_pool_name(enum vmm_page_pool_type page_type);

/**
 * @brief 页 池 空间
 * @param page_type 页面类型标识
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_size_t vmm_page_pool_space(enum vmm_page_pool_type page_type);

/**
 * @brief 获取页面池条目的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_entry_count(enum vmm_page_pool_type page_type);

/**
 * @brief 获取页面池大页的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_huge_page_count(enum vmm_page_pool_type page_type);

/**
 * @brief 获取页面池页帧的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_page_count(enum vmm_page_pool_type page_type);

/**
 * @brief 获取页面池可用页的数量
 * @param page_type 页面类型标识
 * @return 数量值
 */
uint32_t vmm_page_pool_page_avail_count(enum vmm_page_pool_type page_type);

/**
 * @brief 分配页面池
 * @param page_type 页面类型标识
 * @param page_count 数量
 * @return 数量值
 */
virtual_addr_t vmm_page_pool_alloc(enum vmm_page_pool_type page_type, uint32_t page_count);

/**
 * @brief 释放页面池
 * @param page_type 页面类型标识
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_page_pool_free(enum vmm_page_pool_type page_type, virtual_addr_t page_va, uint32_t page_count);

/**
 * @brief 初始化页面池
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_page_pool_init(void);

#endif
