/**
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
 * @file vmm_host_virtual_address_pool.h
 * @author Anup patel (anup@brainfault.org)
 * @brief 虚拟地址池管理头文件
 */

#ifndef __VMM_HOST_VIRTUAL_ADDR_POOL_H_
#define __VMM_HOST_VIRTUAL_ADDR_POOL_H_

#include <vmm_types.h>

struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * @brief 分配主机虚拟地址池
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_alloc(virtual_addr_t *va, virtual_size_t size);

/**
 * @brief 在虚拟地址池中预留指定范围的地址
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_reserve(virtual_addr_t va, virtual_size_t size);

/**
 * @brief 查找主机虚拟地址池
 * @param va 待操作的虚拟地址
 * @param alloc_va 用于返回分配到的虚拟地址
 * @param alloc_sz 用于返回分配块的大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_find(virtual_addr_t va, virtual_addr_t *alloc_va, virtual_size_t *alloc_sz);

/**
 * @brief 释放主机虚拟地址池
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_free(virtual_addr_t va, virtual_size_t size);

/**
 * @brief 检查虚拟地址池中指定页是否空闲
 * @param va 待操作的虚拟地址
 * @return 空闲返回TRUE，否则返回FALSE
 */
bool vmm_host_virtual_address_pool_page_isfree(virtual_addr_t va);

/**
 * @brief 获取主机虚拟地址池空闲页的数量
 * @return 数量值
 */
uint32_t vmm_host_virtual_address_pool_free_page_count(void);

/**
 * @brief 获取主机虚拟地址池总页数的数量
 * @return 数量值
 */
uint32_t vmm_host_virtual_address_pool_total_page_count(void);

/**
 * @brief 获取虚拟地址池的起始基地址
 * @return 起始基地址
 */
virtual_addr_t vmm_host_virtual_address_pool_base(void);

/**
 * @brief 获取虚拟地址池的总大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_host_virtual_address_pool_size(void);

/**
 * @brief 检查虚拟地址是否在地址池有效范围内
 * @param addr 地址值
 * @return 有效返回TRUE，否则返回FALSE
 */
bool vmm_host_virtual_address_pool_isvalid(virtual_addr_t addr);

/**
 * @brief 估算虚拟地址池管理元数据所需大小
 * @param size 数据大小（字节数）
 * @return 大小值（字节）
 */
virtual_size_t vmm_host_virtual_address_pool_estimate_hksize(virtual_size_t size);

/**
 * @brief 输出虚拟地址池的分配状态
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_print_state(vmm_char_device_t *cdev);

/* Initialize virtual address pool management */
/**
 * @brief 初始化主机虚拟地址池
 * @param base 起始基地址
 * @param size 数据大小（字节数）
 * @param hkbase 管理元数据存储区的基地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_init(virtual_addr_t base, virtual_size_t size, virtual_addr_t hkbase);

#endif /* __VMM_HOST_VIRTUAL_ADDR_POOL_H_ */
