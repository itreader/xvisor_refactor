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
 * @file vmm_vserial.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟串口头文件
 */
#ifndef _VMM_VSERIAL_H__
#define _VMM_VSERIAL_H__

#include <libs/fifo.h>
#include <libs/list.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_VSERIAL_IPRIORITY 0

struct vmm_vserial_receiver;
struct vmm_vserial;

/** Representation of a virtual serial port recevier
 *  Note: receive callback can be called in any context hence
 *  hence we cannot sleep in receive callback.
 */
struct vmm_vserial_receiver {
    double_list_t head; /**< 链表头 */
    void (*recv)(struct vmm_vserial *vser, void *private, uint8_t data); /**< 接收 */
    void *private; /**< 私有数据 */
};

/**
 * @brief 虚拟串口端口结构体，维护发送/接收回调、接收者链表和FIFO缓冲区
 */
struct vmm_vserial {
    double_list_t head; /**< 链表头 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */

    bool (*can_send)(struct vmm_vserial *vser); /**< can_send成员 */
    int (*send)(struct vmm_vserial *vser, uint8_t data); /**< 发送 */

    vmm_spinlock_t receiver_list_lock; /**< receiver_list_lock成员 */
    double_list_t  receiver_list; /**< receiver_list成员 */
    struct fifo   *receive_fifo; /**< receive_fifo成员 */
    void *private; /**< 私有数据 */
};

/* Notifier event when virtual serial port is created */
#define VMM_VSERIAL_EVENT_CREATE  0x01
/* Notifier event when virtual serial port is destroyed */
#define VMM_VSERIAL_EVENT_DESTROY 0x02

/**
 * @brief 虚拟串口通知器事件，包含关联的虚拟串口和数据指针
 */
struct vmm_vserial_event {
    struct vmm_vserial *vser; /**< 虚拟串口 */
    void               *data; /**< 数据 */
};

/**
 * @brief 注册虚拟串口客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销虚拟串口客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_unregister_client(vmm_notifier_block_t *nb);

/** Retrive private context of virtual serial port */
static inline void *vmm_vserial_private(struct vmm_vserial *vser)
{
    return (vser) ? vser->private : NULL;
}

/**
 * @brief 虚拟串口 发送
 * @param vser 虚拟串口设备指针
 * @param src 源设备树节点
 * @param len 大小
 * @return 成功返回发送的字节数，失败返回0
 */
uint32_t vmm_vserial_send(struct vmm_vserial *vser, uint8_t *src, uint32_t len);

/**
 * @brief 虚拟串口 接收
 * @param vser 虚拟串口设备指针
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @return 成功返回接收的字节数，失败返回0
 */
uint32_t vmm_vserial_receive(struct vmm_vserial *vser, uint8_t *dst, uint32_t len);

/**
 * @brief 注册虚拟串口接收器
 * @param vser 虚拟串口设备指针
 * @param (*recv 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_register_receiver(struct vmm_vserial *vser, void (*recv)(struct vmm_vserial *, void *, uint8_t), void *private);

/**
 * @brief 注销虚拟串口接收器
 * @param vser 虚拟串口设备指针
 * @param (*recv 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_unregister_receiver(struct vmm_vserial *vser, void (*recv)(struct vmm_vserial *, void *, uint8_t), void *private);

/** Create a virtual serial port */
struct vmm_vserial *vmm_vserial_create(
    const char *name, bool (*can_send)(struct vmm_vserial *), int (*send)(struct vmm_vserial *, uint8_t), uint32_t receive_fifo_size, void *private);

/**
 * @brief 销毁虚拟串口
 * @param vser 虚拟串口设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_destroy(struct vmm_vserial *vser);

/** Find a virtual serial port with given name */
struct vmm_vserial *vmm_vserial_find(const char *name);

/**
 * @brief 虚拟串口 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vserial_iterate(struct vmm_vserial *start, void *data, int (*fn)(struct vmm_vserial *vser, void *data));

/**
 * @brief 获取虚拟串口的数量
 * @return 数量值
 */
uint32_t vmm_vserial_count(void);

#endif
