/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_vmsg.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟消息子系统头文件
 */

/*
 * This framework will be used for implementing inter-guest messaging
 * emulators (such as VirtIO RPMSG device).
 *
 * It has three important entities:
 * 1. vmm_vmsg: The acutal message
 * 2. vmm_vmsg_node: A participant in message based communication
 * 3. vmm_vmsg_domain: A group of participants doing message based
 *    communication
 *
 * Each vmm_vmsg_node will have unique address (1024 <). Any vmm_vmsg_node
 * can broadcast message to all nodes of vmm_vmsg_domain by sending
 * message to 0xffffffff.
 *
 * In addition, the vmm_vmsg_node get notifications about ready state
 * of it's peers in same vmm_vmsg_domain.
 */

#ifndef __VMM_VMSG_H__
#define __VMM_VMSG_H__

#include <arch_atomic.h>
#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_limits.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_types.h>

#define VMM_VMSG_IPRIORITY            0

#define VMM_VMSG_NODE_ADDR_MIN        1024
#define VMM_VMSG_NODE_ADDR_ANY        0xFFFFFFFF

/* Notifier event when virtual messaging domain is created */
#define VMM_VMSG_EVENT_CREATE_DOMAIN  0x01
/* Notifier event when virtual messaging domain is destroyed */
#define VMM_VMSG_EVENT_DESTROY_DOMAIN 0x02
/* Notifier event when virtual messaging node is created */
#define VMM_VMSG_EVENT_CREATE_NODE    0x03
/* Notifier event when virtual messaging node is destroyed */
#define VMM_VMSG_EVENT_DESTROY_NODE   0x04

/** Representation of virtual messaging notifier event */
/**
 * @brief 虚拟消息事件，封装跨客户机的消息通知数据
 */
struct vmm_vmsg_event {
    void *data; /**< 数据 */
};

/**
 * @brief 注册虚拟消息客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销虚拟消息客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_unregister_client(vmm_notifier_block_t *nb);

/** Representation of a virtual message */
/**
 * @brief 虚拟消息结构，维护消息源/目标节点和载荷数据
 */
struct vmm_vmsg {
    struct xref ref_count; /**< 引用计数 */
    uint32_t    dst; /**< 目标 */
    uint32_t    src; /**< 源 */
    uint32_t    local; /**< local成员 */
    void       *data; /**< 数据 */
    size_t      len; /**< 长度 */
    void *private; /**< 私有数据 */
    void (*free_data)(struct vmm_vmsg *); /**< free_data成员 */
    void (*free_hdr)(struct vmm_vmsg *); /**< free_hdr成员 */
};

#define INIT_VMSG(__m, __dst, __src, __local, __d, __l, __p, __fd, __fh)                                                                             \
    do {                                                                                                                                             \
        xref_init(&(__m)->ref_count);                                                                                                                \
        (__m)->dst       = (__dst);                                                                                                                  \
        (__m)->src       = (__src);                                                                                                                  \
        (__m)->local     = (__local);                                                                                                                \
        (__m)->data      = (__d);                                                                                                                    \
        (__m)->len       = (__l);                                                                                                                    \
        (__m)->private   = (__p);                                                                                                                    \
        (__m)->free_data = (__fd);                                                                                                                   \
        (__m)->free_hdr  = (__fh);                                                                                                                   \
    } while (0)

/** Representation of a virtual messaging domain */
/**
 * @brief 虚拟消息域，管理同一通信域内的所有消息节点
 */
struct vmm_vmsg_domain {
    double_list_t head; /**< 链表头 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    void *private; /**< 私有数据 */
    vmm_mutex_t   node_lock; /**< node_lock成员 */
    double_list_t node_list; /**< node_list成员 */
};

struct vmm_vmsg_node;

/**
 * @brief 虚拟消息节点懒加载，延迟初始化节点资源
 */
struct vmm_vmsg_node_lazy {
    struct vmm_vmsg_node *node; /**< 节点 */
    atomic_t              sched_count; /**< sched_count成员 */
    double_list_t         head; /**< 链表头 */
    int                   budget; /**< budget成员 */
    void                 *arg; /**< 参数 */
    void (*xfer)(struct vmm_vmsg_node *, void *, int); /**< 传输 */
};

#define INIT_VMM_VMSG_NODE_LAZY(__lazy, __node, __budget, __arg, __xfer)                                                                             \
    do {                                                                                                                                             \
        (__lazy)->node = (__node);                                                                                                                   \
        ARCH_ATOMIC_INIT(&(__lazy)->sched_count, 0);                                                                                                 \
        INIT_LIST_HEAD(&(__lazy)->head);                                                                                                             \
        (__lazy)->budget = (__budget);                                                                                                               \
        (__lazy)->arg    = (__arg);                                                                                                                  \
        (__lazy)->xfer   = (__xfer);                                                                                                                 \
    } while (0)

/** Representation of a virtual messaging node operations */
/**
 * @brief 虚拟消息节点操作接口，定义发送和接收回调
 */
struct vmm_vmsg_node_ops {
    void (*peer_up)(struct vmm_vmsg_node *node, const char *peer_name, uint32_t peer_addr); /**< peer_up成员 */
    void (*peer_down)(struct vmm_vmsg_node *node, const char *peer_name, uint32_t peer_addr); /**< peer_down成员 */
    bool (*can_recv_msg)(struct vmm_vmsg_node *node); /**< can_recv_msg成员 */
    int (*recv_msg)(struct vmm_vmsg_node *node, struct vmm_vmsg *msg); /**< recv_msg成员 */
};

/** Representation of a virtual messaging node */
/**
 * @brief 虚拟消息节点结构，表示域内一个可收发消息的端点
 */
struct vmm_vmsg_node {
    uint32_t      addr; /**< 地址 */
    double_list_t head; /**< 链表头 */
    double_list_t domain_head; /**< domain_head成员 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t      max_data_len; /**< max_data_len成员 */
    void *private; /**< 私有数据 */
    atomic_t                  is_ready; /**< is_ready成员 */
    struct vmm_vmsg_domain   *domain; /**< 域 */
    struct vmm_vmsg_node_ops *ops; /**< 操作集 */
};

/**
 * @brief 增加虚拟消息引用计数
 * @param msg 消息字符串
 */
void vmm_vmsg_ref(struct vmm_vmsg *msg);

/**
 * @brief 减少虚拟消息引用计数
 * @param msg 消息字符串
 */
void vmm_vmsg_dref(struct vmm_vmsg *msg);

/** Allocate new virtual message with data allocated externally */
struct vmm_vmsg *vmm_vmsg_alloc_ext(
    uint32_t dst, uint32_t src, uint32_t local, void *data, size_t len, void *private, void (*free_data)(struct vmm_vmsg *));

/** Allocate new virtual message from heap */
struct vmm_vmsg *vmm_vmsg_alloc(uint32_t dst, uint32_t src, uint32_t local, size_t len, void *private);

/**
 * @brief 释放虚拟消息
 */
static inline void vmm_vmsg_free(struct vmm_vmsg *msg)
{
    vmm_vmsg_dref(msg);
}

/** Create a virtual messaging domain */
struct vmm_vmsg_domain *vmm_vmsg_domain_create(const char *name, void *private);

/**
 * @brief 销毁消息域
 * @param domain 域结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_domain_destroy(struct vmm_vmsg_domain *domain);

/**
 * @brief 虚拟消息 域 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_domain_iterate(struct vmm_vmsg_domain *start, void *data, int (*fn)(struct vmm_vmsg_domain *, void *));

/** Find a virtual messaging domain with given name */
struct vmm_vmsg_domain *vmm_vmsg_domain_find(const char *name);

/**
 * @brief 获取消息域的数量
 * @return 数量值
 */
uint32_t vmm_vmsg_domain_count(void);

/**
 * @brief 遍历虚拟消息域中的所有节点
 * @param domain 域结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_domain_node_iterate(struct vmm_vmsg_domain *domain, struct vmm_vmsg_node *start, void *data, int (*fn)(struct vmm_vmsg_node *, void *));

/**
 * @brief 获取消息域的名称
 * @param domain 域结构体指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_vmsg_domain_get_name(struct vmm_vmsg_domain *domain);

/**
 * Create a virtual messaging node
 *
 * Note: If 'addr' is VMM_VMSG_NODE_ADDR_ANY then a free
 * node address is allocated using host wide ID allocator.
 */
struct vmm_vmsg_node *vmm_vmsg_node_create(
    const char *name, uint32_t addr, uint32_t max_data_len, struct vmm_vmsg_node_ops *ops, struct vmm_vmsg_domain *domain, void *private);

/**
 * @brief 销毁消息节点
 * @param node 设备树节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_destroy(struct vmm_vmsg_node *node);

/** Retrive private context of virtual messaging node */
static inline void *vmm_vmsg_node_private(struct vmm_vmsg_node *node)
{
    return (node) ? node->private : NULL;
}

/**
 * @brief 虚拟消息 节点 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_iterate(struct vmm_vmsg_node *start, void *data, int (*fn)(struct vmm_vmsg_node *, void *));

/** Find a virtual messaging node with given name */
struct vmm_vmsg_node *vmm_vmsg_node_find(const char *name);

/**
 * @brief 获取消息节点的数量
 * @return 数量值
 */
uint32_t vmm_vmsg_node_count(void);

/**
 * @brief 向虚拟消息节点发送数据
 * @param node 设备树节点指针
 * @param msg 消息字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_send(struct vmm_vmsg_node *node, struct vmm_vmsg *msg);

/**
 * @brief 通过虚拟消息节点快速发送消息
 * @param node 设备树节点指针
 * @param msg 消息字符串
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_send_fast(struct vmm_vmsg_node *node, struct vmm_vmsg *msg);

/**
 * @brief 延迟启动虚拟消息节点
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_start_lazy(struct vmm_vmsg_node_lazy *lazy);

/**
 * @brief 延迟停止虚拟消息节点
 * @param lazy 是否延迟处理标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_vmsg_node_stop_lazy(struct vmm_vmsg_node_lazy *lazy);

/**
 * @brief 虚拟消息 节点 就绪
 * @param node 设备树节点指针
 */
void vmm_vmsg_node_ready(struct vmm_vmsg_node *node);

/**
 * @brief 将虚拟消息节点标记为未就绪
 * @param node 设备树节点指针
 */
void vmm_vmsg_node_notready(struct vmm_vmsg_node *node);

/**
 * @brief 检查虚拟消息节点是否就绪
 * @param node 设备树节点指针
 * @return 就绪返回TRUE，否则返回FALSE
 */
bool vmm_vmsg_node_is_ready(struct vmm_vmsg_node *node);

/**
 * @brief 获取消息节点的名称
 * @param node 设备树节点指针
 * @return 目标对象指针，不存在返回NULL
 */
const char *vmm_vmsg_node_get_name(struct vmm_vmsg_node *node);

/**
 * @brief 获取消息节点的addr
 * @param node 设备树节点指针
 * @return 成功返回节点地址，失败返回VMM_VMSG_NODE_ADDR_ANY
 */
uint32_t vmm_vmsg_node_get_addr(struct vmm_vmsg_node *node);

/**
 * @brief 获取消息节点的最大数据长度
 * @param node 设备树节点指针
 * @return 成功返回最大数据长度，失败返回0
 */
uint32_t vmm_vmsg_node_get_max_data_len(struct vmm_vmsg_node *node);

/** Get domain of virtual messaging node */
struct vmm_vmsg_domain *vmm_vmsg_node_get_domain(struct vmm_vmsg_node *node);

#endif /* __VMM_VMSG_H__ */
