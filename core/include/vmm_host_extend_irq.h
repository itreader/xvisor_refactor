/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_host_extend_irq.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief 扩展主机IRQ支持
 */

#ifndef _VMM_HOST_IRQEXT_H__
#define _VMM_HOST_IRQEXT_H__

#include <vmm_host_irq.h>
#include <vmm_types.h>

struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * @brief 获取主机扩展中断控制器
 * @param hirq 中断号
 * @return 目标对象指针，不存在返回NULL
 */
vmm_host_irq_t *__vmm_host_extend_irq_get(uint32_t hirq);

/**
 * @brief 分配主机扩展中断区域
 * @param size 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_alloc_region(uint32_t size);

/**
 * @brief 释放主机扩展中断区域
 * @param hirq 中断号
 * @param size 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_free_region(uint32_t hirq, uint32_t size);

/**
 * @brief 创建主机扩展中断映射
 * @param hirq 中断号
 * @param hw_irq_num 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_create_mapping(uint32_t hirq, uint32_t hw_irq_num);

/**
 * @brief 释放主机扩展中断映射
 * @param hirq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_dispose_mapping(uint32_t hirq);

/**
 * @brief 输出主机扩展中断的调试信息
 * @param cdev 字符设备指针
 */
void vmm_host_extend_irq_debug_dump(vmm_char_device_t *cdev);

/**
 * @brief 初始化主机扩展中断
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_extend_irq_init(void);

/**
 * @brief 获取主机扩展中断的数量
 * @return 数量值
 */
uint32_t vmm_host_extend_irq_count(void);

#endif /* _VMM_HOST_IRQEXT_H__ */
