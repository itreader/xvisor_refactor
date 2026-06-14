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
 * @file vmm_share_memory.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 共享内存子系统源文件
 */

#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_mutex.h>
#include <vmm_share_memory.h>
#include <vmm_stdio.h>

/**
 * @brief 共享内存控制结构，管理跨客户机的共享内存段
 */
struct vmm_share_memory_ctrl {
    vmm_mutex_t   lock; /**< 自旋锁 */
    double_list_t share_memory_list; /**< share_memory_list成员 */
};

static struct vmm_share_memory_ctrl shmctrl;

/**
 * @brief 从共享内存区域读取数据
 * @param share_memory 共享内存结构体指针
 * @param off 偏移量
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_share_memory_read(vmm_share_memory_t *share_memory, physical_addr_t off, void *dst, uint32_t len, bool cacheable)
{
    if (!share_memory || !dst) {
        return 0;
    }

    if (share_memory->size < (off + len)) {
        return 0;
    }

    return vmm_host_memory_read(share_memory->addr + off, dst, len, cacheable);
}

/**
 * @brief 向共享内存区域写入数据
 * @param share_memory 共享内存结构体指针
 * @param off 偏移量
 * @param src 源设备树节点
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_share_memory_write(vmm_share_memory_t *share_memory, physical_addr_t off, void *src, uint32_t len, bool cacheable)
{
    if (!share_memory || !src) {
        return 0;
    }

    if (share_memory->size < (off + len)) {
        return 0;
    }

    return vmm_host_memory_write(share_memory->addr + off, src, len, cacheable);
}

/**
 * @brief 设置共享内存区域的数据
 * @param share_memory 共享内存结构体指针
 * @param off 偏移量
 * @param byte 字节值
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际设置的字节数，失败返回0
 */
uint32_t vmm_share_memory_set(vmm_share_memory_t *share_memory, physical_addr_t off, uint8_t byte, uint32_t len, bool cacheable)
{
    if (!share_memory) {
        return 0;
    }

    if (share_memory->size < (off + len)) {
        return 0;
    }

    return vmm_host_memory_set(share_memory->addr + off, byte, len, cacheable);
}

/**
 * @brief 遍历共享内存中的条目
 * @param (*iter 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_share_memory_iterate(int (*iter)(vmm_share_memory_t *, void *), void *private)
{
    int                 rc = VMM_OK;
    vmm_share_memory_t *share_memory;

    if (!iter) {
        return VMM_ERR_INVALID;
    }

    vmm_mutex_lock(&shmctrl.lock);

    list_for_each_entry(share_memory, &shmctrl.share_memory_list, head)
    {
        rc = iter(share_memory, private);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&shmctrl.lock);

    return rc;
}

/**
 * @brief 获取共享内存条目的数量
 * @param share_memory 共享内存结构体指针
 * @param private 私有数据指针
 * @return 数量值
 */
static int share_memory_count(vmm_share_memory_t *share_memory, void *private)
{
    uint32_t *cntp = private;

    if (cntp) {
        (*cntp)++;
    }

    return VMM_OK;
}

/**
 * @brief 获取共享内存的数量
 * @return 数量值
 */
uint32_t vmm_share_memory_count(void)
{
    uint32_t count = 0;

    return (!vmm_share_memory_iterate(share_memory_count, &count)) ? count : 0;
}

/**
 * @brief 共享内存查找上下文，用于按名称搜索共享内存段
 */
struct share_memory_find_data {
    const char         *name; /**< 名称 */
    vmm_share_memory_t *share_memory; /**< 共享内存 */
};

/**
 * @brief 按名称查找共享内存区域
 * @param share_memory 共享内存结构体指针
 * @param private 私有数据指针
 * @return 数量值
 */
static int share_memory_find_byname(vmm_share_memory_t *share_memory, void *private)
{
    struct share_memory_find_data *data = private;

    if (!data->share_memory) {
        if (!strncmp(share_memory->name, data->name, sizeof(share_memory->name))) {
            vmm_share_memory_ref(share_memory);
            data->share_memory = share_memory; /**< 共享内存 */
        }
    }

    return VMM_OK;
}

/**
 * @brief 按名称查找共享内存区域
 * @param name 目标对象的名称
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_share_memory_t *vmm_share_memory_find_byname(const char *name)
{
    struct share_memory_find_data data;

    if (!name) {
        return NULL; /**< NULL成员 */
    }

    data.name         = name;
    data.share_memory = NULL;

    return (!vmm_share_memory_iterate(share_memory_find_byname, &data)) ? data.share_memory : NULL;
}

/**
 * @brief 增加共享内存的引用计数
 * @param share_memory 共享内存结构体指针
 */
void vmm_share_memory_ref(vmm_share_memory_t *share_memory)
{
    if (!share_memory) {
        return;
    }

    xref_get(&share_memory->ref_count);
}

/**
 * @brief 释放共享内存
 * @param ref 引用计数结构体指针
 */
static void __share_memory_free(struct xref *ref)
{
    vmm_share_memory_t *share_memory = container_of(ref, vmm_share_memory_t, ref_count);

    vmm_mutex_lock(&shmctrl.lock);

    list_del(&share_memory->head);
    vmm_host_ram_free(share_memory->addr, share_memory->size);
    vmm_free(share_memory);

    vmm_mutex_unlock(&shmctrl.lock);
}

/**
 * @brief 减少共享内存的引用计数
 * @param share_memory 共享内存结构体指针
 */
void vmm_share_memory_dref(vmm_share_memory_t *share_memory)
{
    if (share_memory) {
        xref_put(&share_memory->ref_count, __share_memory_free);
    }
}

/**
 * @brief 创建共享内存
 * @param name 目标对象的名称
 * @param size 数据大小（字节数）
 * @param align_order 阶数
 * @param private 私有数据指针
 * @return 成功返回新创建的节点指针，失败返回NULL
 */
vmm_share_memory_t *vmm_share_memory_create(const char *name, physical_size_t size, uint32_t align_order, void *private)
{
    bool                found = FALSE;
    vmm_share_memory_t *share_memory;

    if (!name || !size) {
        return VMM_ERR_RR_PTR(VMM_ERR_INVALID);
    }

    size = VMM_ROUNDUP2_PAGE_SIZE(size);

    vmm_mutex_lock(&shmctrl.lock);

    list_for_each_entry(share_memory, &shmctrl.share_memory_list, head)
    {
        if (!strncmp(share_memory->name, name, sizeof(share_memory->name))) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&shmctrl.lock);
        return VMM_ERR_RR_PTR(VMM_ERR_EXIST);
    };

    share_memory = vmm_zalloc(sizeof(*share_memory));

    if (!share_memory) {
        vmm_mutex_unlock(&shmctrl.lock);
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    INIT_LIST_HEAD(&share_memory->head);
    xref_init(&share_memory->ref_count);
    strncpy(share_memory->name, name, sizeof(share_memory->name));

    share_memory->size = vmm_host_ram_alloc(&share_memory->addr, size, align_order);

    if (!share_memory->size) {
        vmm_free(share_memory);
        vmm_mutex_unlock(&shmctrl.lock);
        return VMM_ERR_RR_PTR(VMM_ERR_NOMEM);
    }

    share_memory->align_order = align_order;
    share_memory->private     = private;

    list_add_tail(&share_memory->head, &shmctrl.share_memory_list);

    vmm_mutex_unlock(&shmctrl.lock);

    return share_memory;
}

/**
 * @brief 初始化共享内存
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_share_memory_init(void)
{
    memset(&shmctrl, 0, sizeof(shmctrl));

    INIT_MUTEX(&shmctrl.lock);
    INIT_LIST_HEAD(&shmctrl.share_memory_list);

    return VMM_OK;
}
