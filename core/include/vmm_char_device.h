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
 * @file vmm_char_device.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 字符设备框架头文件
 */

#ifndef __VMM_CHARDEV_H_
#define __VMM_CHARDEV_H_

#include <vmm_device_driver.h>
#include <vmm_limits.h>
#include <vmm_types.h>

#define VMM_CHARDEV_CLASS_NAME "char"
struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * @brief 字符设备结构，维护字符设备的名称、操作接口和私有数据
 */
struct vmm_char_device {
    char         name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    vmm_device_t dev; /**< 设备 */
    int (*ioctl)(vmm_char_device_t *cdev, int cmd, void *arg); /**< ioctl成员 */
    uint32_t (*read)(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool sleep); /**< 读 */
    uint32_t (*write)(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool sleep); /**< 写 */
    void *private; /**< 私有数据 */
};

/**
 * @brief 执行字符设备的ioctl控制操作
 * @param cdev 字符设备指针
 * @param cmd 命令标识或命令结构体指针
 * @param arg 参数值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_char_device_doioctl(vmm_char_device_t *cdev, int cmd, void *arg);

/**
 * @brief 执行字符设备的读操作
 * @param cdev 字符设备指针
 * @param dest 目标缓冲区或目标地址
 * @param len 数据长度
 * @param off 偏移量
 * @param block 块设备指针
 * @return 成功返回读取的字节数，失败返回错误码
 */
uint32_t vmm_char_device_doread(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool block);

/**
 * @brief 执行字符设备的写操作
 * @param cdev 字符设备指针
 * @param src 源设备树节点
 * @param len 数据长度
 * @param off 偏移量
 * @param block 块设备指针
 * @return 成功返回写入的字节数，失败返回错误码
 */
uint32_t vmm_char_device_dowrite(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool block);

/**
 * @brief 注册字符设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_char_device_register(vmm_char_device_t *cdev);

/**
 * @brief 注销字符设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_char_device_unregister(vmm_char_device_t *cdev);

/**
 * @brief 查找字符设备
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_char_device_t *vmm_char_device_find(const char *name);

/**
 * @brief 字符设备 设备 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_char_device_iterate(vmm_char_device_t *start, void *data, int (*fn)(vmm_char_device_t *dev, void *data));

/**
 * @brief 获取字符设备的数量
 * @return 数量值
 */
uint32_t vmm_char_device_count(void);

/**
 * @brief 初始化字符设备
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_char_device_init(void);

#endif /* __VMM_CHARDEV_H_ */
