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
 * @file vmm_device_resource.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备驱动资源管理头文件
 */

#ifndef __VMM_DEVRES_H_
#define __VMM_DEVRES_H_

#include <vmm_types.h>

struct vmm_device;
typedef struct vmm_device vmm_device_t;

/**
 * @brief 设备资源匹配与释放函数
 */
typedef void (*vmm_device_resource_release_t)(vmm_device_t *dev, void *res);
typedef int (*vmm_device_resource_match_t)(vmm_device_t *dev, void *res, void *match_data);

/**
 * @brief 分配设备资源
 * @param release 释放回调函数
 * @param size 数据大小（字节数）
 */
void *vmm_device_resource_alloc(vmm_device_resource_release_t release, size_t size);

/**
 * @brief 遍历每个设备资源
 */
void vmm_device_resource_for_each_resource(
    vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data,
    void (*fn)(vmm_device_t *, void *, void *), void *data);

/**
 * @brief 释放设备资源
 * @param res 资源结构体指针
 */
void vmm_device_resource_free(void *res);

/**
 * @brief 向设备添加资源
 * @param dev 设备结构体指针
 * @param res 资源结构体指针
 */
void vmm_device_resource_add(vmm_device_t *dev, void *res);

/**
 * @brief 查找设备资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
void *vmm_device_resource_find(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/**
 * @brief 获取设备关联的资源
 * @param dev 设备结构体指针
 * @param new_res 新资源结构体指针
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
void *vmm_device_resource_get(vmm_device_t *dev, void *new_res, vmm_device_resource_match_t match, void *match_data);

/**
 * @brief 从设备中移除资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 目标对象指针，不存在返回NULL
 */
void *vmm_device_resource_remove(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/**
 * @brief 销毁设备资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_resource_destroy(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/**
 * @brief 释放设备的指定资源
 * @param dev 设备结构体指针
 * @param release 释放回调函数
 * @param match 匹配回调函数
 * @param match_data 匹配数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_resource_release(vmm_device_t *dev, vmm_device_resource_release_t release, vmm_device_resource_match_t match, void *match_data);

/**
 * @brief 释放设备的所有资源
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_resource_release_all(vmm_device_t *dev);

/**
 * @brief 设备管理的内存分配
 * @param dev 设备结构体指针
 * @param size 数据大小（字节数）
 * @return 成功返回目标指针，失败返回NULL
 */
void *vmm_devm_malloc(vmm_device_t *dev, size_t size);

/**
 * @brief 设备管理的清零内存分配
 * @param dev 设备结构体指针
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_zalloc(vmm_device_t *dev, size_t size);

/**
 * @brief 设备管理的数组内存分配
 * @param dev 设备结构体指针
 * @param n 起始位置编号
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_malloc_array(vmm_device_t *dev, size_t n, size_t size);

/**
 * @brief 设备管理的批量内存分配
 * @param dev 设备结构体指针
 * @param n 起始位置编号
 * @param size 数据大小（字节数）
 * @return 成功返回分配的内存指针，失败返回NULL
 */
void *vmm_devm_calloc(vmm_device_t *dev, size_t n, size_t size);

/**
 * @brief 设备管理的字符串复制分配
 * @param dev 设备结构体指针
 * @param s 字符串或数据指针
 */
char *vmm_devm_strdup(vmm_device_t *dev, const char *s);

/**
 * @brief 释放devm
 * @param dev 设备结构体指针
 * @param p 数据指针
 */
void vmm_devm_free(vmm_device_t *dev, void *p);

/**
 * @brief 添加托管设备动作到设备资源管理列表
 * @param dev 设备结构体指针
 * @param (*action 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_devm_add_action(vmm_device_t *dev, void (*action)(void *), void *data);

/**
 * @brief 从设备资源管理列表中移除托管设备动作
 * @param dev 设备结构体指针
 * @param (*action 指针参数
 */
void vmm_devm_remove_action(vmm_device_t *dev, void (*action)(void *), void *data);

#endif /* __VMM_DEVRES_H_ */
