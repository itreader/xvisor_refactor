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
 * @file vmm_block_device.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 块设备框架头文件
 */

#ifndef __VMM_BLOCK_DEVICE_H_
#define __VMM_BLOCK_DEVICE_H_

#include <vmm_device_driver.h>
#include <vmm_limits.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_BLOCK_DEVICE_CLASS_NAME      "block"
#define VMM_BLOCK_DEVICE_CLASS_IPRIORITY 1

struct vmm_request;
struct vmm_request_queue;
struct vmm_block_device;
typedef struct vmm_request       vmm_request_t;
typedef struct vmm_request_queue vmm_request_queue_t;
typedef struct vmm_block_device  vmm_block_device_t;

/** Types of block IO request */
/**
 * @brief 块设备请求类型枚举，定义读/写/刷新等操作类型
 */
enum vmm_request_type {
    VMM_REQUEST_UNKNOWN = 0, /**< 0 */
    VMM_REQUEST_READ    = 1, /**< 1 */
    VMM_REQUEST_WRITE   = 2
};

/** Representation of a block IO request */
/**
 * @brief 块设备I/O请求结构，包含操作的地址、长度和完成回调
 */
struct vmm_request {
    double_list_t head; /**< 链表头 */

    vmm_block_device_t *block_device; /* No need to set this field.
                                       * submit_request() will set this field.
                                       */

    enum vmm_request_type type; /**< 类型 */
    uint64_t              lba; /**< 逻辑块地址 */
    uint32_t              bcnt; /**< 字节计数 */
    void                 *data; /**< 数据 */

    void (*completed)(vmm_request_t *); /**< 已完成标志 */
    void (*failed)(vmm_request_t *); /**< 失败标志 */
    void *private; /**< 私有数据 */
};

/** Representation of a block IO request queue */
/**
 * @brief 块设备请求队列实现，维护排队中的I/O请求列表
 */
struct vmm_request_queue {
    /* Lock to protect the request queue operations */
    vmm_spinlock_t lock; /**< 自旋锁 */

    /* Max pending requests */
    uint32_t max_pending; /**< max_pending成员 */

    /* Pending (or in-flight) request count */
    uint32_t pending_count; /**< pending_count成员 */

    /* Backlog request count */
    uint32_t backlog_count; /**< backlog_count成员 */

    /* Backlog request list */
    double_list_t backlog_list; /**< backlog_list成员 */

    /* Note: if peek_cache succeeds then we assume
     * request completed successfully.
     *
     * Note: if peek_cache returns VMM_ERR_NOTAVAIL then
     * we try make_request()
     *
     * Note: if peek_cache returns any other error then
     * we assume request failed.
     */
    int (*peek_cache)(vmm_request_queue_t *rq, vmm_request_t *r); /**< peek_cache成员 */

    /* Note: make_request must ensure that it calls
     *
     * vmm_block_device_complete_request()
     * OR
     * vmm_block_device_fail_request()
     *
     * for every request that it gets
     */
    int (*make_request)(vmm_request_queue_t *rq, vmm_request_t *r); /**< make_request成员 */

    /* Note: abort_request will be called for successfully
     * submited request only
     */
    int (*abort_request)(vmm_request_queue_t *rq, vmm_request_t *r); /**< abort_request成员 */

    /* Note: This is an optional callback only required
     * if request queue does block caching
     */
    int (*flush_cache)(vmm_request_queue_t *rq); /**< flush_cache成员 */

    void *private; /**< 私有数据 */
};

#define INIT_REQUEST_QUEUE(__rq, __max_pending, __peek_cache, __make_request, __abort_request, __flush_request, __private)                           \
    do {                                                                                                                                             \
        INIT_SPIN_LOCK(&(__rq)->lock);                                                                                                               \
        (__rq)->max_pending   = (__max_pending);                                                                                                     \
        (__rq)->pending_count = 0;                                                                                                                   \
        (__rq)->backlog_count = 0;                                                                                                                   \
        INIT_LIST_HEAD(&(__rq)->backlog_list);                                                                                                       \
        (__rq)->peek_cache    = (__peek_cache);                                                                                                      \
        (__rq)->make_request  = (__make_request);                                                                                                    \
        (__rq)->abort_request = (__abort_request);                                                                                                   \
        (__rq)->flush_cache   = (__flush_request);                                                                                                   \
        (__rq)->private       = (__private);                                                                                                         \
    } while (0)

/* Block device flags */
#define VMM_BLOCK_DEVICE_RDONLY 0x00000001
#define VMM_BLOCK_DEVICE_RW     0x00000002

/** Block device */
/**
 * @brief 块设备结构，描述块设备容量、请求队列和操作接口
 */
struct vmm_block_device {
    double_list_t       head; /**< 链表头 */
    vmm_block_device_t *parent; /**< 父节点 */

    vmm_mutex_t   child_lock; /**< child_lock成员 */
    uint32_t      child_count; /**< 子节点数量 */
    double_list_t child_list; /**< 子节点链表 */

    char         name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    char         desc[VMM_FIELD_DESC_SIZE]; /**< 描述 */
    vmm_device_t dev; /**< 设备 */

    uint32_t flags; /**< 标志位 */
    uint64_t start_lba; /**< 起始LBA */
    uint64_t num_blocks; /**< num_blocks成员 */
    uint32_t block_size; /**< block_size成员 */

    vmm_request_queue_t *rq; /**< 运行队列 */

    /* NOTE: partition management uses part_manager_sign and
     * part_manager_priv for its own use.
     * NOTE: part_manager_sign will be unique to partition style
     * NOTE: part_manager_sign=0x0 is reserved and means unknown
     * partition style
     */
    uint32_t part_manager_sign;    /* To be used for partition management */
    void    *part_manager_private; /* To be used for partition management */
};

/* Notifier event when block device is registered */
#define VMM_BLOCK_DEVICE_EVENT_REGISTER   0x01
/* Notifier event when block device is unregistered */
#define VMM_BLOCK_DEVICE_EVENT_UNREGISTER 0x02

/** Representation of block device notifier event */
/**
 * @brief 块设备事件结构，通知块设备的创建/销毁等状态变化
 */
struct vmm_block_device_event {
    vmm_block_device_t *block_device; /**< block_device成员 */
    void               *data; /**< 数据 */
};

/**
 * @brief 注册块设备客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销块设备客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_unregister_client(vmm_notifier_block_t *nb);

/**
 * @brief 获取块设备大小（字节）
 */
static inline uint64_t vmm_block_device_total_size(vmm_block_device_t *block_device)
{
    return (block_device) ? block_device->num_blocks * block_device->block_size : 0;
}

/**
 * @brief 完成块设备的I/O请求处理
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_complete_request(vmm_request_t *r);

/**
 * @brief 使块设备的I/O请求失败
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_fail_request(vmm_request_t *r);

/**
 * @brief 提交块设备的I/O请求
 * @param block_device 块设备结构体指针
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_submit_request(vmm_block_device_t *block_device, vmm_request_t *r);

/**
 * @brief 中止块设备的I/O请求
 * @param r 资源或数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_abort_request(vmm_request_t *r);

/**
 * @brief 刷新块设备的请求缓存
 * @param block_device 块设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_flush_cache(vmm_block_device_t *block_device);

/**
 * @brief 执行块设备的读写操作
 * @param block_device 块设备结构体指针
 * @param type 类型标识值
 * @param buf 数据缓冲区指针
 * @param off 偏移量
 * @param len 数据长度
 * @return 返回64位无符号整数值
 */
uint64_t vmm_block_device_rw(vmm_block_device_t *block_device, enum vmm_request_type type, uint8_t *buf, uint64_t off, uint64_t len);

/** Generic block IO read */
#define vmm_block_device_read(block_device, dst, off, len)  vmm_block_device_rw((block_device), VMM_REQUEST_READ, (dst), (off), (len))

/** Generic block IO write */
#define vmm_block_device_write(block_device, src, off, len) vmm_block_device_rw((block_device), VMM_REQUEST_WRITE, (src), (off), (len))

/**
 * @brief 分配块设备
 */
vmm_block_device_t *vmm_block_device_alloc(void);

/**
 * @brief 释放块设备
 * @param block_device 块设备结构体指针
 */
void vmm_block_device_free(vmm_block_device_t *block_device);

/**
 * @brief 注册块设备
 * @param block_device 块设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_register(vmm_block_device_t *block_device);

/**
 * @brief 向块设备添加子设备（分区）
 * @param block_device 块设备结构体指针
 * @param start_lba 起始逻辑块地址
 * @param num_blocks 块数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_add_child(vmm_block_device_t *block_device, uint64_t start_lba, uint64_t num_blocks);

/**
 * @brief 注销块设备
 * @param block_device 块设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_unregister(vmm_block_device_t *block_device);

/**
 * @brief 查找块设备
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_block_device_t *vmm_block_device_find(const char *name);

/**
 * @brief 块设备 设备 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_block_device_iterate(vmm_block_device_t *start, void *data, int (*fn)(vmm_block_device_t *dev, void *data));

/**
 * @brief 获取块设备的数量
 * @return 数量值
 */
uint32_t vmm_block_device_count(void);

#endif /* __VMM_BLOCK_DEVICE_H_ */
