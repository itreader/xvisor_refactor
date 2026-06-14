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
 * @file vmm_host_ram.h
 * @author Anup patel (anup@brainfault.org)
 * @brief RAM管理头文件
 */

#ifndef __VMM_HOST_RAM_H_
#define __VMM_HOST_RAM_H_

#include <vmm_limits.h>
#include <vmm_types.h>

/** Host RAM cache color operations */
/**
 * @brief 主机内存着色操作接口，定义缓存着色的回调函数
 */
struct vmm_host_ram_color_ops {
    char name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t (*num_colors)(void *private); /**< 颜色数量 */
    uint32_t (*color_order)(void *private); /**< color_order成员 */
    bool (*color_match)(physical_addr_t pa, physical_size_t size, uint32_t color, void *private); /**< color_match成员 */
};

/**
 * @brief 设置主机物理内存的着色操作接口
 * @param ops 操作集结构体指针
 * @param private 私有数据指针
 */
void vmm_host_ram_set_color_ops(struct vmm_host_ram_color_ops *ops, void *private);

/**
 * @brief 获取主机内存着色操作的名称
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_host_ram_color_ops_name(void);

/**
 * @brief 获取主机内存着色的数量
 * @return 数量值
 */
uint32_t vmm_host_ram_color_count(void);

/**
 * @brief 获取主机内存着色的阶数
 * @return 数量值
 */
uint32_t vmm_host_ram_color_order(void);

/**
 * @brief 分配主机内存着色
 * @param pa 待操作的物理地址
 * @param color 缓存着色值
 * @return 成功返回分配结果，失败返回错误码
 */
physical_size_t vmm_host_ram_color_alloc(physical_addr_t *pa, uint32_t color);

/**
 * @brief 分配主机物理内存
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @param align_order 阶数
 * @return 成功返回分配结果，失败返回错误码
 */
physical_size_t vmm_host_ram_alloc(physical_addr_t *pa, physical_size_t size, uint32_t align_order);

/**
 * @brief 预留主机物理内存区域
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t size);

/**
 * @brief 释放主机物理内存
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_ram_free(physical_addr_t pa, physical_size_t size);

/**
 * @brief 检查指定主机物理内存帧是否空闲
 * @param pa 待操作的物理地址
 * @return 空闲返回TRUE，否则返回FALSE
 */
bool vmm_host_ram_frame_isfree(physical_addr_t pa);

/**
 * @brief 获取主机物理内存总空闲帧数
 * @return 系统空闲内存页帧总数
 */
uint32_t vmm_host_ram_total_free_frames(void);

/**
 * @brief 获取主机物理内存总页帧的数量
 * @return 数量值
 */
uint32_t vmm_host_ram_total_frame_count(void);

/**
 * @brief 启动主机物理内存
 * @return 数量值
 */
physical_addr_t vmm_host_ram_start(void);

/**
 * @brief 主机 内存 结束
 * @return 成功返回VMM_OK，失败返回错误码
 */
physical_addr_t vmm_host_ram_end(void);

/**
 * @brief 获取主机物理内存总大小
 * @return 大小值（字节）
 */
physical_size_t vmm_host_ram_total_size(void);

/**
 * @brief 获取主机内存段的数量
 * @return 数量值
 */
uint32_t vmm_host_ram_bank_count(void);

/**
 * @brief 启动主机内存段
 * @param bank Bank编号
 * @return 数量值
 */
physical_addr_t vmm_host_ram_bank_start(uint32_t bank);

/**
 * @brief 获取主机RAM Bank的大小
 * @param bank Bank编号
 * @return 大小值（字节）
 */
physical_size_t vmm_host_ram_bank_size(uint32_t bank);

/**
 * @brief 获取主机物理内存内存段页帧的数量
 * @param bank Bank编号
 * @return 数量值
 */
uint32_t vmm_host_ram_bank_frame_count(uint32_t bank);

/**
 * @brief 获取主机RAM Bank的空闲帧数
 * @param bank Bank编号
 * @return 数量值
 */
uint32_t vmm_host_ram_bank_free_frames(uint32_t bank);

/**
 * @brief 估算主机内存管理开销大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_size_t vmm_host_ram_estimate_hksize(void);

/* Initialize RAM management */
/**
 * @brief 初始化主机物理内存
 * @param hkbase 管理元数据存储区的基地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_ram_init(virtual_addr_t hkbase);

#endif /* __VMM_HOST_RAM_H_ */
