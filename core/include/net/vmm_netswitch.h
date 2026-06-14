/**
 * Copyright (c) 2010 Pranav Sawargaonkar.
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_netswitch.h
 * @author Pranav Sawargaonkar <pranav.sawargaonkar@gmail.com>
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief 通用网络交换机接口头文件
 */

#ifndef __VMM_NETSWITCH_H_
#define __VMM_NETSWITCH_H_

#include <libs/list.h>
#include <vmm_device_driver.h>
#include <vmm_limits.h>
#include <vmm_types.h>

#define VMM_NETSWITCH_CLASS_NAME "netswitch"

struct vmm_netswitch_policy;
struct vmm_netswitch;
struct vmm_netport;
struct vmm_netport_lazy;
struct vmm_mbuf;

/**
 * @brief 网络交换机结构，管理虚拟网络端口的数据转发
 */
struct vmm_netswitch {
    /* === Private members === */
    /* Underly class device */
    vmm_device_t                 dev; /**< 设备 */
    /* Lock to protect port list */
    vmm_rwlock_t                 port_list_lock; /**< port_list_lock成员 */
    /* List of ports */
    double_list_t                port_list; /**< port_list成员 */
    /* === Public members === */
    /* Policy */
    struct vmm_netswitch_policy *policy; /**< policy成员 */
    /* Name */
    char                         name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    /* Flags */
    int                          flags; /**< 标志位 */
    /* Handle RX packets from port to switch */
    int (*port2switch_xfer)(struct vmm_netswitch *, struct vmm_netport *, struct vmm_mbuf *); /**< port2switch_xfer成员 */
    /* Handle enabling of a port */
    int (*port_add)(struct vmm_netswitch *, struct vmm_netport *); /**< port_add成员 */
    /* Handle disabling of a port */
    int (*port_remove)(struct vmm_netswitch *, struct vmm_netport *); /**< port_remove成员 */
    /* Switch private data */
    void *private; /**< 私有数据 */
};

/**
 * @brief 网络交换机策略，定义数据包匹配和转发规则
 */
struct vmm_netswitch_policy {
    /* === Private members === */
    double_list_t head; /**< 链表头 */
    /* === Public members === */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    struct vmm_netswitch *(*create)(struct vmm_netswitch_policy *policy, const char *name, int argc, char **argv); /**< create成员 */
    void (*destroy)(struct vmm_netswitch_policy *policy, struct vmm_netswitch *nsw); /**< destroy成员 */
};

/**
 * @brief 端口到交换机转发消息缓冲区
 * @param src 源设备树节点
 * @param mbuf 网络消息缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_port2switch_xfer_mbuf(struct vmm_netport *src, struct vmm_mbuf *mbuf);

/**
 * @brief 延迟将端口数据传输到网络交换机
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_port2switch_xfer_lazy(struct vmm_netport_lazy *lazy);

/**
 * @brief 交换机到端口转发消息缓冲区
 * @param nsw 网络交换机结构体指针
 * @param dst 目标缓冲区指针
 * @param mbuf 网络消息缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_switch2port_xfer_mbuf(struct vmm_netswitch *nsw, struct vmm_netport *dst, struct vmm_mbuf *mbuf);

/** Allocate new network switch (used by network switch policy)
 *  @name name of the network switch
 */
struct vmm_netswitch *vmm_netswitch_alloc(struct vmm_netswitch_policy *nsp, const char *name);

/**
 * @brief 释放网络交换机
 * @param nsw 网络交换机结构体指针
 */
void vmm_netswitch_free(struct vmm_netswitch *nsw);

/**
 * @brief 网络交换机 端口 添加
 * @param nsw 网络交换机结构体指针
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_port_add(struct vmm_netswitch *nsw, struct vmm_netport *port);

/**
 * @brief 网络交换机 端口 移除
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_port_remove(struct vmm_netport *port);

/**
 * @brief 注册网络交换机
 * @param nsw 网络交换机结构体指针
 * @param parent 父设备树节点
 * @param private 私有数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_register(struct vmm_netswitch *nsw, vmm_device_t *parent, void *private);

/**
 * @brief 注销网络交换机
 * @param nsw 网络交换机结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_unregister(struct vmm_netswitch *nsw);

/** Find a network switch */
struct vmm_netswitch *vmm_netswitch_find(const char *name);

/**
 * @brief 网络交换机 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_iterate(struct vmm_netswitch *start, void *data, int (*fn)(struct vmm_netswitch *nsw, void *data));

/** Get default network switch */
struct vmm_netswitch *vmm_netswitch_default(void);

/**
 * @brief 获取网络交换机的数量
 * @return 数量值
 */
uint32_t vmm_netswitch_count(void);

/**
 * @brief 注册网络交换策略
 * @param nsp 节点特定数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_register(struct vmm_netswitch_policy *nsp);

/**
 * @brief 注销网络交换策略
 * @param nsp 节点特定数据指针
 */
void vmm_netswitch_policy_unregister(struct vmm_netswitch_policy *nsp);

/**
 * @brief 按策略遍历网络交换机
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_iterate(struct vmm_netswitch_policy *start, void *data, int (*fn)(struct vmm_netswitch_policy *, void *));

/** Find a network switch policy */
struct vmm_netswitch_policy *vmm_netswitch_policy_find(const char *name);

/**
 * @brief 获取网络交换策略的数量
 * @return 数量值
 */
uint32_t vmm_netswitch_policy_count(void);

/**
 * @brief 网络交换机策略创建交换机实例
 * @param policy_name 调度策略名称
 * @param switch_name 交换机名称
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_create_switch(const char *policy_name, const char *switch_name, int argc, char **argv);

/**
 * @brief 网络交换机策略销毁交换机实例
 * @param nsw 网络交换机结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_netswitch_policy_destroy_switch(struct vmm_netswitch *nsw);

#endif /* __VMM_NETSWITCH_H_ */
