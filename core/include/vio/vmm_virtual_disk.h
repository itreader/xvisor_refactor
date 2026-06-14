/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vmm_virtual_disk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟磁盘框架头文件
 */

/* The virtual disk framework helps disk controller emulators
 * in emulating disk read/write operations irrespective to
 * disk controller type. It also provides a convient way of
 * tracking various virtual disk instances of a guest.
 *
 * Each virtual disk can be attached to a block device. If a
 * block device attached to virtual disk is unregistered then
 * virtual disk is dettached automatically.
 *
 * All IO on virtual disk have to be done using opaque struct
 * vmm_virtual_disk_request. The struct vmm_virtual_disk_request is a wrapper
 * struct on-top of struct vmm_request. The emulators don't need
 * to explicity fill properties of vmm_virtual_disk_request because
 * vmm_virtual_disk_submit_request() will automatically fill it. If
 * the emulators still need access to individual properties of
 * vmm_virtual_disk_request then they will have to use vmm_virtual_disk APIs.
 */

#ifndef _VMM_VIRTUAL_DISK_H__
#define _VMM_VIRTUAL_DISK_H__

#include <block/vmm_block_device.h>
#include <libs/list.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_VIRTUAL_DISK_IPRIORITY (VMM_BLOCK_DEVICE_CLASS_IPRIORITY + 1)

struct vmm_virtual_disk_request;
struct vmm_virtual_disk;

/** Types of block IO request */
/**
 * @brief 虚拟磁盘请求类型枚举，定义读写等操作类型
 */
enum vmm_virtual_disk_request_type {
    VMM_VIRTUAL_DISK_REQUEST_UNKNOWN = 0, /**< 0 */
    VMM_VIRTUAL_DISK_REQUEST_READ    = 1, /**< 1 */
    VMM_VIRTUAL_DISK_REQUEST_WRITE   = 2
};

/** Representation of a virtual disk request  */
/**
 * @brief 虚拟磁盘请求结构，包含操作类型、偏移和数据指针
 */
struct vmm_virtual_disk_request {
    struct vmm_virtual_disk *virtual_disk; /**< virtual_disk成员 */
    vmm_request_t            r; /**< r */
};

/** Representation of a virtual disk */
/**
 * @brief 虚拟磁盘设备，维护磁盘容量和I/O请求处理回调
 */
struct vmm_virtual_disk {
    double_list_t head; /**< 链表头 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    uint32_t      block_size; /**< block_size成员 */

    void (*attached)(struct vmm_virtual_disk *); /**< attached成员 */
    void (*detached)(struct vmm_virtual_disk *); /**< detached成员 */
    void (*completed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *); /**< 已完成标志 */
    void (*failed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *); /**< 失败标志 */

    vmm_spinlock_t      block_lock; /* Protect blk pointer */
    vmm_block_device_t *blk; /**< 块 */
    uint32_t            block_factor; /**< block_factor成员 */

    void *private; /**< 私有数据 */
};

/* Notifier event when virtual disk is created */
#define VMM_VIRTUAL_DISK_EVENT_CREATE  0x01
/* Notifier event when virtual disk is destroyed */
#define VMM_VIRTUAL_DISK_EVENT_DESTROY 0x02

/** Representation of virtual disk notifier event */
/**
 * @brief 虚拟磁盘事件，通知磁盘容量变化等状态更新
 */
struct vmm_virtual_disk_event {
    struct vmm_virtual_disk *virtual_disk; /**< virtual_disk成员 */
    void                    *data; /**< 数据 */
};

/**
 * @brief 注册虚拟磁盘客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_register_client(vmm_notifier_block_t *nb);

/**
 * @brief 注销虚拟磁盘客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_unregister_client(vmm_notifier_block_t *nb);

/**
 * @brief 设置给定虚拟磁盘请求的虚拟磁盘指针
 */
static inline void vmm_virtual_disk_set_request_disk(struct vmm_virtual_disk_request *vreq, struct vmm_virtual_disk *virtual_disk)
{
    if (vreq) {
        vreq->virtual_disk = virtual_disk;
    }
}

/** Get virtual_disk pointer of given virtual disk request */
static inline struct vmm_virtual_disk *vmm_virtual_disk_get_request_disk(struct vmm_virtual_disk_request *vreq)
{
    return (vreq) ? vreq->virtual_disk : NULL;
}

/**
 * @brief 设置虚拟磁盘的请求类型
 * @param vreq 虚拟请求结构体指针
 * @param type 类型标识值
 */
void vmm_virtual_disk_set_request_type(struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type);

/**
 * @brief 获取虚拟磁盘请求的类型
 */
enum vmm_virtual_disk_request_type vmm_virtual_disk_get_request_type(struct vmm_virtual_disk_request *vreq);

/**
 * @brief 设置虚拟磁盘请求的逻辑块地址
 */
static inline void vmm_virtual_disk_set_request_lba(struct vmm_virtual_disk_request *vreq, uint64_t lba)
{
    if (vreq) {
        vreq->r.lba = lba; /**< lba成员 */
    }
}

/**
 * @brief 获取虚拟磁盘请求的逻辑块地址
 */
static inline uint64_t vmm_virtual_disk_get_request_lba(struct vmm_virtual_disk_request *vreq)
{
    return (vreq) ? vreq->r.lba : 0;
}

/**
 * @brief 设置虚拟磁盘请求的数据缓冲区
 */
static inline void vmm_virtual_disk_set_request_data(struct vmm_virtual_disk_request *vreq, void *data)
{
    if (vreq) {
        vreq->r.data = data;
    }
}

/** Get data of given virtual disk request */
static inline void *vmm_virtual_disk_get_request_data(struct vmm_virtual_disk_request *vreq)
{
    return (vreq) ? vreq->r.data : NULL;
}

/**
 * @brief 设置虚拟磁盘的请求长度
 * @param vreq 虚拟请求结构体指针
 * @param data_len 大小
 */
void vmm_virtual_disk_set_request_len(struct vmm_virtual_disk_request *vreq, uint32_t data_len);

/**
 * @brief 获取虚拟磁盘的请求长度
 * @param vreq 虚拟请求结构体指针
 * @return 成功返回请求数据长度，失败返回0
 */
uint32_t vmm_virtual_disk_get_request_len(struct vmm_virtual_disk_request *vreq);

/** Retrive private context of virtual disk */
static inline void *vmm_virtual_disk_private(struct vmm_virtual_disk *virtual_disk)
{
    return (virtual_disk) ? virtual_disk->private : NULL;
}

/**
 * @brief 向虚拟磁盘提交IO请求
 */
int vmm_virtual_disk_submit_request(
    struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type, uint64_t lba, void *data,
    uint32_t data_len);

/* Abort IO request from virtual disk */
/**
 * @brief 中止虚拟磁盘的I/O请求
 * @param virtual_disk 虚拟磁盘设备指针
 * @param vreq 虚拟请求结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_abort_request(struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq);

/**
 * @brief 刷新虚拟磁盘的请求缓存
 * @param virtual_disk 虚拟磁盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_flush_cache(struct vmm_virtual_disk *virtual_disk);

/** Name of virtual disk */
static inline const char *vmm_virtual_disk_name(struct vmm_virtual_disk *virtual_disk)
{
    return (virtual_disk) ? virtual_disk->name : NULL;
}

/**
 * @brief 获取虚拟磁盘的块大小
 */
static inline uint32_t vmm_virtual_disk_block_size(struct vmm_virtual_disk *virtual_disk)
{
    return (virtual_disk) ? virtual_disk->block_size : 0;
}

/**
 * @brief 获取虚拟磁盘的容量（扇区数）
 * @param virtual_disk 虚拟磁盘设备指针
 * @return 返回64位无符号整数值
 */
uint64_t vmm_virtual_disk_capacity(struct vmm_virtual_disk *virtual_disk);

/**
 * @brief 获取虚拟磁盘当前关联的块设备
 * @param virtual_disk 虚拟磁盘设备指针
 * @param buf 数据缓冲区指针
 * @param buf_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_current_block_device(struct vmm_virtual_disk *virtual_disk, char *buf, uint32_t buf_len);

/**
 * @brief 将虚拟磁盘附加到块设备
 * @param virtual_disk 虚拟磁盘设备指针
 * @param bdev_name 块设备名称
 */
void vmm_virtual_disk_attach_block_device(struct vmm_virtual_disk *virtual_disk, const char *bdev_name);

/**
 * @brief 将虚拟磁盘从块设备上分离
 * @param virtual_disk 虚拟磁盘设备指针
 */
void vmm_virtual_disk_detach_block_device(struct vmm_virtual_disk *virtual_disk);

/** Create a virtual disk */
struct vmm_virtual_disk *vmm_virtual_disk_create(
    const char *name, uint32_t block_size, void (*attached)(struct vmm_virtual_disk *), void (*detached)(struct vmm_virtual_disk *),
    void (*completed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *),
    void (*failed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *), void *private);

/**
 * @brief 销毁虚拟磁盘
 * @param virtual_disk 虚拟磁盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_destroy(struct vmm_virtual_disk *virtual_disk);

/** Find a virtual disk with given name */
struct vmm_virtual_disk *vmm_virtual_disk_find(const char *name);

/**
 * @brief 虚拟 磁盘 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_iterate(struct vmm_virtual_disk *start, void *data, int (*fn)(struct vmm_virtual_disk *virtual_disk, void *data));

/**
 * @brief 获取虚拟磁盘的数量
 * @return 数量值
 */
uint32_t vmm_virtual_disk_count(void);

#endif
