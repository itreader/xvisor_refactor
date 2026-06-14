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
 * @file vmm_char_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 字符设备框架实现
 */

#include <libs/stringlib.h>
#include <vmm_char_device.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_scheduler.h>

/**
 * @brief 执行字符设备的ioctl操作
 * @param cdev 字符设备指针
 * @param cmd ioctl命令
 * @param arg ioctl参数
 * @return 成功返回0，否则返回错误码
 */
int vmm_char_device_doioctl(vmm_char_device_t *cdev, int cmd, void *arg)
{
    if (cdev && cdev->ioctl) {
        return cdev->ioctl(cdev, cmd, arg);
    } else {
        return VMM_ERR_FAIL;
    }
}

/**
 * @brief 从字符设备读取数据
 * @param cdev 字符设备指针
 * @param dest 目标缓冲区
 * @param len 要读取的字节数
 * @param off 偏移量指针
 * @param block 是否阻塞读取
 * @return 实际读取的字节数
 */
uint32_t vmm_char_device_doread(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool block)
{
    uint32_t b;
    bool     sleep;

    if (cdev && cdev->read) {
        if (block) {
            b     = 0;
            sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;

            while (b < len) {
                b += cdev->read(cdev, &dest[b], len - b, off, sleep);
            }

            return b;
        } else {
            return cdev->read(cdev, dest, len, off, FALSE);
        }
    } else {
        return 0;
    }
}

/**
 * @brief 向字符设备写入数据
 * @param cdev 字符设备指针
 * @param src 源缓冲区
 * @param len 要写入的字节数
 * @param off 偏移量指针
 * @param block 是否阻塞写入
 * @return 实际写入的字节数
 */
uint32_t vmm_char_device_dowrite(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool block)
{
    uint32_t b;
    bool     sleep;

    if (cdev && cdev->write) {
        if (block) {
            b     = 0;
            sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;

            while (b < len) {
                b += cdev->write(cdev, &src[b], len - b, off, sleep);
            }

            return b;
        } else {
            return cdev->write(cdev, src, len, off, FALSE);
        }
    } else {
        return 0;
    }
}

static vmm_class_t char_device_class = {
    .name = VMM_CHARDEV_CLASS_NAME,
};

/**
 * @brief 注册字符设备
 * @param cdev 要注册的字符设备指针
 * @return 成功返回0，否则返回错误码
 */
int vmm_char_device_register(vmm_char_device_t *cdev)
{
    if (!(cdev && cdev->read && cdev->write)) {
        return VMM_ERR_FAIL;
    }

    vmm_device_driver_initialize_device(&cdev->dev);

    if (strlcpy(cdev->dev.name, cdev->name, sizeof(cdev->dev.name)) >= sizeof(cdev->dev.name)) {
        return VMM_ERR_OVERFLOW;
    }

    cdev->dev.class = &char_device_class;
    vmm_device_driver_set_data(&cdev->dev, cdev);

    return vmm_device_driver_register_device(&cdev->dev);
}

/**
 * @brief 注销字符设备
 * @param cdev 要注销的字符设备指针
 * @return 成功返回0，否则返回错误码
 */
int vmm_char_device_unregister(vmm_char_device_t *cdev)
{
    if (!cdev) {
        return VMM_ERR_FAIL;
    }

    return vmm_device_driver_unregister_device(&cdev->dev);
}

/**
 * @brief 根据名称查找字符设备
 * @param name 设备名称
 * @return 找到的字符设备指针，未找到返回NULL
 */
vmm_char_device_t *vmm_char_device_find(const char *name)
{
    vmm_device_t *dev;

    dev = vmm_device_driver_class_find_device_by_name(&char_device_class, name);

    if (!dev) {
        return NULL;
    }

    return vmm_device_driver_get_data(dev);
}

/**
 * @brief 字符设备遍历上下文，保存迭代回调和用户数据
 */
struct char_device_iterate_priv {
    void *data; /**< 数据 */
    int (*fn)(vmm_char_device_t *dev, void *data); /**< 函数指针 */
};

/**
 * @brief 字符设备迭代回调函数
 * @param dev 设备指针
 * @param data 私有数据
 * @return 迭代结果
 */
static int char_device_iterate(vmm_device_t *dev, void *data)
{
    struct char_device_iterate_priv *p    = data;
    vmm_char_device_t               *cdev = vmm_device_driver_get_data(dev);

    return p->fn(cdev, p->data);
}

/**
 * @brief 迭代所有字符设备
 * @param start 起始设备指针，可为NULL
 * @param data 传递给回调函数的数据
 * @param fn 回调函数
 * @return 成功返回0，否则返回错误码
 */
int vmm_char_device_iterate(vmm_char_device_t *start, void *data, int (*fn)(vmm_char_device_t *dev, void *data))
{
    vmm_device_t                   *st = (start) ? &start->dev : NULL;
    struct char_device_iterate_priv p;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&char_device_class, st, &p, char_device_iterate);
}

/**
 * @brief 获取字符设备的数量
 * @return 字符设备数量
 */
uint32_t vmm_char_device_count(void)
{
    return vmm_device_driver_class_device_count(&char_device_class);
}

/**
 * @brief 初始化字符设备框架
 * @return 成功返回0，否则返回错误码
 */
int __init vmm_char_device_init(void)
{
    return vmm_device_driver_register_class(&char_device_class);
}
