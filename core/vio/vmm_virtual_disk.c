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
 * @file vmm_virtual_disk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 虚拟磁盘框架源代码
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vio/vmm_virtual_disk.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC      "Virtual Disk Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VIRTUAL_DISK_IPRIORITY)
#define MODULE_INIT      vmm_virtual_disk_init
#define MODULE_EXIT      vmm_virtual_disk_exit

/**
 * @brief 虚拟磁盘控制结构（内部），维护磁盘设备的运行时状态
 */
struct vmm_virtual_disk_ctrl {
    vmm_mutex_t                   virtual_disk_list_lock; /**< virtual_disk_list_lock成员 */
    double_list_t                 virtual_disk_list; /**< virtual_disk_list成员 */
    vmm_blocking_notifier_chain_t notifier_chain; /**< 通知器链 */
    vmm_notifier_block_t          block_client; /**< block_client成员 */
};

static struct vmm_virtual_disk_ctrl vdctrl;

/**
 * @brief 注册虚拟磁盘客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_register_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_register(&vdctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_register_client);

/**
 * @brief 注销虚拟磁盘客户端
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_unregister_client(vmm_notifier_block_t *nb)
{
    return vmm_blocking_notifier_unregister(&vdctrl.notifier_chain, nb);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_unregister_client);

/**
 * @brief 虚拟磁盘请求完成回调
 * @param r 资源或数据指针
 */
static void virtual_disk_req_completed(vmm_request_t *r)
{
    struct vmm_virtual_disk_request *vreq         = container_of(r, struct vmm_virtual_disk_request, r);
    struct vmm_virtual_disk         *virtual_disk = vreq->virtual_disk;

    if (virtual_disk->completed) {
        virtual_disk->completed(virtual_disk, vreq); /**< vreq)成员 */
    }

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d\n", __func__, virtual_disk->name, (uint64_t)r->lba, r->bcnt);
}

/**
 * @brief 虚拟磁盘请求失败回调
 * @param r 资源或数据指针
 */
static void virtual_disk_req_failed(vmm_request_t *r)
{
    struct vmm_virtual_disk_request *vreq         = container_of(r, struct vmm_virtual_disk_request, r);
    struct vmm_virtual_disk         *virtual_disk = vreq->virtual_disk;

    if (virtual_disk->failed) {
        virtual_disk->failed(virtual_disk, vreq); /**< vreq)成员 */
    }

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d\n", __func__, virtual_disk->name, (uint64_t)r->lba, r->bcnt);
}

/**
 * @brief 设置虚拟磁盘的请求类型
 * @param vreq 虚拟请求结构体指针
 * @param type 类型标识值
 */
void vmm_virtual_disk_set_request_type(struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type)
{
    if (!vreq) {
        return;
    }

    switch (type) {
        case VMM_VIRTUAL_DISK_REQUEST_READ:
            vreq->r.type = VMM_REQUEST_READ;
            break;

        case VMM_VIRTUAL_DISK_REQUEST_WRITE:
            vreq->r.type = VMM_REQUEST_WRITE;
            break;

        default:
            vreq->r.type = VMM_REQUEST_UNKNOWN;
            break;
    };
}

enum vmm_virtual_disk_request_type vmm_virtual_disk_get_request_type(struct vmm_virtual_disk_request *vreq)
{
    enum vmm_virtual_disk_request_type type; /**< 类型 */

    if (!vreq) {
        return VMM_VIRTUAL_DISK_REQUEST_UNKNOWN; /**< VMM_VIRTUAL_DISK_REQUEST_UNKNOWN成员 */
    }

    switch (vreq->r.type) {
        case VMM_REQUEST_READ:
            type = VMM_VIRTUAL_DISK_REQUEST_READ; /**< VMM_VIRTUAL_DISK_REQUEST_READ成员 */
            break;

        case VMM_REQUEST_WRITE:
            type = VMM_VIRTUAL_DISK_REQUEST_WRITE; /**< VMM_VIRTUAL_DISK_REQUEST_WRITE成员 */
            break;

        default:
            type = VMM_VIRTUAL_DISK_REQUEST_UNKNOWN; /**< VMM_VIRTUAL_DISK_REQUEST_UNKNOWN成员 */
            break;
    };

    return type; /**< 类型 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_get_request_type);

/**
 * @brief 设置虚拟磁盘的请求长度
 * @param vreq 虚拟请求结构体指针
 * @param data_len 大小
 */
void vmm_virtual_disk_set_request_len(struct vmm_virtual_disk_request *vreq, uint32_t data_len)
{
    irq_flags_t              flags;
    struct vmm_virtual_disk *virtual_disk;

    if (!vreq || !vreq->virtual_disk) {
        return;
    }

    virtual_disk = vreq->virtual_disk;
    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);
    vreq->r.bcnt = udiv32(data_len, virtual_disk->block_size) * virtual_disk->block_factor;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_set_request_len);

/**
 * @brief 获取虚拟磁盘的请求长度
 * @param vreq 虚拟请求结构体指针
 * @return 成功返回请求数据长度，失败返回0
 */
uint32_t vmm_virtual_disk_get_request_len(struct vmm_virtual_disk_request *vreq)
{
    uint32_t                 ret = 0;
    irq_flags_t              flags;
    struct vmm_virtual_disk *virtual_disk;

    if (!vreq || !vreq->virtual_disk) {
        return 0; /**< 0 */
    }

    virtual_disk = vreq->virtual_disk;
    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);
    ret = udiv32(vreq->r.bcnt, virtual_disk->block_factor) * virtual_disk->block_size;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_get_request_len);

/**
 * @brief 提交虚拟磁盘I/O请求
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_submit_request(
    struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq, enum vmm_virtual_disk_request_type type, uint64_t lba, void *data,
    uint32_t data_len)
{
    int         rc; /**< rc */
    irq_flags_t flags; /**< 标志位 */

    if (!virtual_disk || !vreq || !data) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (data_len < virtual_disk->block_size) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if ((type < VMM_VIRTUAL_DISK_REQUEST_READ) || (VMM_VIRTUAL_DISK_REQUEST_WRITE < type)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags); /**< flags)成员 */

    if (virtual_disk->blk) {
        vreq->virtual_disk = virtual_disk; /**< virtual_disk成员 */
        vmm_virtual_disk_set_request_type(vreq, type); /**< type)成员 */
        vreq->r.lba       = (lba + virtual_disk->blk->start_lba) * virtual_disk->block_factor; /**< virtual_disk->block_factor成员 */
        vreq->r.bcnt      = udiv32(data_len, virtual_disk->block_size) * virtual_disk->block_factor; /**< virtual_disk->block_factor成员 */
        vreq->r.data      = data; /**< 数据 */
        vreq->r.completed = virtual_disk_req_completed; /**< virtual_disk_req_completed成员 */
        vreq->r.failed    = virtual_disk_req_failed; /**< virtual_disk_req_failed成员 */
        vreq->r.private   = NULL; /**< NULL成员 */
        rc                = vmm_block_device_submit_request(virtual_disk->blk, &vreq->r); /**< &vreq->r)成员 */
    } else {
        virtual_disk->failed(virtual_disk, vreq); /**< vreq)成员 */
        rc = VMM_ERR_NODEV; /**< VMM_ERR_NODEV成员 */
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags); /**< flags)成员 */

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d rc=%d\n", __func__, virtual_disk->name, (uint64_t)vreq->r.lba, vreq->r.bcnt, rc); /**< rc)成员 */

    return rc; /**< rc */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_submit_request);

/**
 * @brief 中止虚拟磁盘的I/O请求
 * @param virtual_disk 虚拟磁盘设备指针
 * @param vreq 虚拟请求结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_abort_request(struct vmm_virtual_disk *virtual_disk, struct vmm_virtual_disk_request *vreq)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk || !vreq) {
        return VMM_ERR_INVALID;
    }

    if (vreq->virtual_disk != virtual_disk) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        rc = vmm_block_device_abort_request(&vreq->r);
    } else {
        rc = VMM_ERR_NODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    DPRINTF("%s: virtual_disk=%s lba=0x%llx bcnt=%d rc=%d\n", __func__, virtual_disk->name, (uint64_t)vreq->r.lba, vreq->r.bcnt, rc);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_abort_request);

/**
 * @brief 刷新虚拟磁盘的请求缓存
 * @param virtual_disk 虚拟磁盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_flush_cache(struct vmm_virtual_disk *virtual_disk)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        rc = vmm_block_device_flush_cache(virtual_disk->blk);
    } else {
        rc = VMM_ERR_NODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    DPRINTF("%s: virtual_disk=%s rc=%d\n", __func__, virtual_disk->name, rc);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_flush_cache);

/**
 * @brief 获取虚拟磁盘的容量（扇区数）
 * @param virtual_disk 虚拟磁盘设备指针
 * @return 返回64位无符号整数值
 */
uint64_t vmm_virtual_disk_capacity(struct vmm_virtual_disk *virtual_disk)
{
    uint64_t    ret = 0;
    irq_flags_t flags;

    if (!virtual_disk) {
        return 0;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        ret = udiv64(virtual_disk->blk->num_blocks, virtual_disk->block_factor);
    } else {
        ret = 0;
    }

    ret = (virtual_disk->blk) ? virtual_disk->blk->num_blocks : 0;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_capacity);

/**
 * @brief 获取虚拟磁盘当前关联的块设备
 * @param virtual_disk 虚拟磁盘设备指针
 * @param name 目标对象的名称
 * @param name_len 大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_current_block_device(struct vmm_virtual_disk *virtual_disk, char *name, uint32_t name_len)
{
    int         rc;
    irq_flags_t flags;

    if (!virtual_disk || !name || !name_len) {
        return VMM_ERR_INVALID;
    }

    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        strncpy(name, virtual_disk->blk->name, name_len);
        rc = VMM_OK;
    } else {
        rc = VMM_ERR_NODEV;
    }

    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_current_block_device);

/**
 * @brief 虚拟磁盘附加私有上下文，传递附加操作所需的参数
 */
struct virtual_disk_attach_priv {
    struct vmm_virtual_disk *virtual_disk; /**< virtual_disk成员 */
    const char              *bdev_name; /**< bdev_name成员 */
};

/**
 * @brief 将虚拟磁盘附加到块设备迭代器
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int virtual_disk_attach_iter(vmm_block_device_t *dev, void *data)
{
    bool                             attached;
    irq_flags_t                      flags;
    struct virtual_disk_attach_priv *ap           = data;
    const char                      *bdev_name    = ap->bdev_name;
    struct vmm_virtual_disk         *virtual_disk = ap->virtual_disk;

    if (strncmp(dev->name, bdev_name, sizeof(dev->name)) == 0) {
        attached = FALSE; /**< FALSE成员 */
        vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags); /**< flags)成员 */

        if (!virtual_disk->blk && (dev->block_size <= virtual_disk->block_size) && !umod32(virtual_disk->block_size, dev->block_size)) {
            virtual_disk->blk          = dev; /**< 设备 */
            virtual_disk->block_factor = udiv32(virtual_disk->block_size, virtual_disk->blk->block_size); /**< virtual_disk->blk->block_size)成员 */
            attached                   = TRUE; /**< TRUE成员 */
        }

        vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags); /**< flags)成员 */

        if (attached && virtual_disk->attached) {
            virtual_disk->attached(virtual_disk);
        }
    }

    return VMM_OK;
}

/**
 * @brief 将虚拟磁盘附加到块设备
 * @param virtual_disk 虚拟磁盘设备指针
 * @param bdev_name 块设备名称
 */
void vmm_virtual_disk_attach_block_device(struct vmm_virtual_disk *virtual_disk, const char *bdev_name)
{
    struct virtual_disk_attach_priv ap;

    if (!virtual_disk || !bdev_name) {
        return;
    }

    ap.virtual_disk = virtual_disk;
    ap.bdev_name    = bdev_name;
    vmm_block_device_iterate(NULL, &ap, virtual_disk_attach_iter);
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_attach_block_device);

/**
 * @brief 将虚拟磁盘从块设备上分离
 * @param virtual_disk 虚拟磁盘设备指针
 */
void vmm_virtual_disk_detach_block_device(struct vmm_virtual_disk *virtual_disk)
{
    bool        detached;
    irq_flags_t flags;

    if (!virtual_disk) {
        return;
    }

    detached = FALSE;
    vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

    if (virtual_disk->blk) {
        vmm_block_device_flush_cache(virtual_disk->blk);
        detached = TRUE;
    }

    virtual_disk->blk          = NULL;
    virtual_disk->block_factor = 1;
    vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);

    if (detached && virtual_disk->detached) {
        virtual_disk->detached(virtual_disk);
    }
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_detach_block_device);

struct vmm_virtual_disk *vmm_virtual_disk_create(
    const char *name, uint32_t block_size, void (*attached)(struct vmm_virtual_disk *), void (*detached)(struct vmm_virtual_disk *),
    void (*completed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *),
    void (*failed)(struct vmm_virtual_disk *, struct vmm_virtual_disk_request *), void *private)
{
    bool                          found; /**< found成员 */
    struct vmm_virtual_disk      *virtual_disk; /**< virtual_disk成员 */
    struct vmm_virtual_disk_event event; /**< 事件 */

    if (!name || !block_size || !completed || !failed) {
        return NULL; /**< NULL成员 */
    }

    virtual_disk = NULL; /**< NULL成员 */
    found        = FALSE; /**< FALSE成员 */

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        if (strcmp(name, virtual_disk->name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return NULL; /**< NULL成员 */
    }

    virtual_disk = vmm_zalloc(sizeof(struct vmm_virtual_disk)); /**< vmm_virtual_disk))成员 */

    if (!virtual_disk) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return NULL; /**< NULL成员 */
    }

    INIT_LIST_HEAD(&virtual_disk->head);

    if (strlcpy(virtual_disk->name, name, sizeof(virtual_disk->name)) >= sizeof(virtual_disk->name)) {
        vmm_free(virtual_disk);
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return NULL; /**< NULL成员 */
    }

    virtual_disk->block_size = block_size; /**< block_size成员 */
    virtual_disk->attached   = attached; /**< attached成员 */
    virtual_disk->detached   = detached; /**< detached成员 */
    virtual_disk->completed  = completed; /**< completed成员 */
    virtual_disk->failed     = failed; /**< failed成员 */
    INIT_SPIN_LOCK(&virtual_disk->block_lock);
    virtual_disk->blk          = NULL; /**< NULL成员 */
    virtual_disk->block_factor = 1; /**< 1 */
    virtual_disk->private      = private; /**< 私有数据 */

    list_add_tail(&virtual_disk->head, &vdctrl.virtual_disk_list); /**< &vdctrl.virtual_disk_list)成员 */

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    /* Broadcast create event */
    event.virtual_disk = virtual_disk; /**< virtual_disk成员 */
    event.data         = NULL; /**< NULL成员 */
    vmm_blocking_notifier_call(&vdctrl.notifier_chain, VMM_VIRTUAL_DISK_EVENT_CREATE, &event); /**< &event)成员 */

    return virtual_disk; /**< virtual_disk成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_create);

/**
 * @brief 销毁虚拟磁盘
 * @param virtual_disk 虚拟磁盘设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_destroy(struct vmm_virtual_disk *virtual_disk)
{
    bool                          found;
    struct vmm_virtual_disk      *vd;
    struct vmm_virtual_disk_event event;

    if (!virtual_disk) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    /* Detach current block device */
    vmm_virtual_disk_detach_block_device(virtual_disk);

    /* Broadcast destroy event */
    event.virtual_disk = virtual_disk;
    event.data         = NULL;
    vmm_blocking_notifier_call(&vdctrl.notifier_chain, VMM_VIRTUAL_DISK_EVENT_DESTROY, &event);

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    if (list_empty(&vdctrl.virtual_disk_list)) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return VMM_ERR_FAIL;
    }

    vd    = NULL;
    found = FALSE;

    list_for_each_entry(vd, &vdctrl.virtual_disk_list, head)
    {
        if (strcmp(vd->name, virtual_disk->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&vd->head);

    vmm_free(vd);

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_destroy);

struct vmm_virtual_disk *vmm_virtual_disk_find(const char *name)
{
    bool                     found; /**< found成员 */
    struct vmm_virtual_disk *virtual_disk; /**< virtual_disk成员 */

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    found        = FALSE; /**< FALSE成员 */
    virtual_disk = NULL; /**< NULL成员 */

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        if (strcmp(virtual_disk->name, name) == 0) {
            found = TRUE; /**< TRUE成员 */
            break;
        }
    }

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    return virtual_disk; /**< virtual_disk成员 */
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_find);

/**
 * @brief 虚拟 磁盘 遍历
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_virtual_disk_iterate(struct vmm_virtual_disk *start, void *data, int (*fn)(struct vmm_virtual_disk *virtual_disk, void *data))
{
    int                      rc          = VMM_OK;
    bool                     start_found = (start) ? FALSE : TRUE;
    struct vmm_virtual_disk *vd          = NULL;

    if (!fn) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(vd, &vdctrl.virtual_disk_list, head)
    {
        if (!start_found) {
            if (start && start == vd) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(vd, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return rc;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_iterate);

/**
 * @brief 获取虚拟磁盘的数量
 * @return 数量值
 */
uint32_t vmm_virtual_disk_count(void)
{
    uint32_t                 retval = 0;
    struct vmm_virtual_disk *virtual_disk;

    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(vmm_virtual_disk_count);

/**
 * @brief 虚拟磁盘的块设备事件通知
 * @param nb 通知器块指针
 * @param evt 事件结构体指针
 * @param data 用户自定义数据指针
 * @return 数量值
 */
static int virtual_disk_block_notification(vmm_notifier_block_t *nb, uint64_t evt, void *data)
{
    irq_flags_t                    flags;
    struct vmm_virtual_disk       *virtual_disk;
    struct vmm_block_device_event *e = data;

    if (evt != VMM_BLOCK_DEVICE_EVENT_UNREGISTER) {
        /* We are only interested in unregister events so,
         * don't care about this event.
         */
        return NOTIFY_DONE; /**< NOTIFY_DONE成员 */
    }

    /* Lock virtual disk list */
    vmm_mutex_lock(&vdctrl.virtual_disk_list_lock);

    /* Find virtual disk using block device */
    list_for_each_entry(virtual_disk, &vdctrl.virtual_disk_list, head)
    {
        vmm_spin_lock_irq_save_lite(&virtual_disk->block_lock, flags);

        if (virtual_disk->blk == e->block_device) {
            virtual_disk->blk          = NULL;
            virtual_disk->block_factor = 1;
        }

        vmm_spin_unlock_irq_restore_lite(&virtual_disk->block_lock, flags);
    }

    /* Unlock virtual disk list */
    vmm_mutex_unlock(&vdctrl.virtual_disk_list_lock);

    return NOTIFY_OK;
}

/**
 * @brief 初始化虚拟磁盘
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init vmm_virtual_disk_init(void)
{
    memset(&vdctrl, 0, sizeof(vdctrl));

    INIT_MUTEX(&vdctrl.virtual_disk_list_lock);
    INIT_LIST_HEAD(&vdctrl.virtual_disk_list);
    BLOCKING_INIT_NOTIFIER_CHAIN(&vdctrl.notifier_chain);

    vdctrl.block_client.notifier_call = &virtual_disk_block_notification;
    vdctrl.block_client.priority      = 0;
    vmm_block_device_register_client(&vdctrl.block_client);

    return VMM_OK;
}

/**
 * @brief 虚拟磁盘子系统退出清理
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __exit vmm_virtual_disk_exit(void)
{
    vmm_block_device_unregister_client(&vdctrl.block_client);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
