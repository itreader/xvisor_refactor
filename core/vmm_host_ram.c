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
 * @file vmm_host_ram.c
 * @author Anup patel (anup@brainfault.org)
 * @brief RAM管理源文件
 */

#include <arch_device_tree.h>
#include <libs/bitmap.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_resource.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

/**
 * @brief 主机内存段结构，描述一段连续的物理内存区域
 */
struct vmm_host_ram_bank {
    physical_addr_t start; /**< 起始 */
    physical_size_t size; /**< 大小 */
    uint32_t        frame_count; /**< 帧计数 */

    vmm_spinlock_t bmap_lock; /**< bmap_lock成员 */
    uint64_t      *bmap; /**< bmap成员 */
    uint32_t       bmap_sz; /**< bmap_sz成员 */
    uint32_t       bmap_free; /**< bmap_free成员 */

    vmm_resource_t res; /**< 保留/结果 */
};

/**
 * @brief 主机内存管理控制结构，跟踪内存段和页帧状态
 */
struct vmm_host_ram_ctrl {
    struct vmm_host_ram_color_ops *ops; /**< 操作集 */
    void                          *ops_private; /**< ops_private成员 */
    uint32_t                       bank_count; /**< bank_count成员 */
    struct vmm_host_ram_bank       banks[CONFIG_MAX_RAM_BANK_COUNT]; /**< banks成员 */
};

static struct vmm_host_ram_ctrl rctrl;

/**
 * @brief 分配主机RAM内存
 * @param pa 指向物理地址的指针，用于返回分配的物理地址
 * @param size 要分配的内存大小
 * @param align_order 对齐顺序，用于确定对齐要求
 * @param color 颜色值，用于NUMA分配
 * @param ops 颜色操作接口
 * @param ops_private 私有数据指针
 * @return 实际分配的内存大小，0表示分配失败
 */
static physical_size_t __host_ram_alloc(
    physical_addr_t *pa, physical_size_t size, uint32_t align_order, uint32_t color, struct vmm_host_ram_color_ops *ops, void *ops_private)
{
    irq_flags_t               f;
    physical_addr_t           p;
    uint32_t i;
    uint32_t bn;
    uint32_t binc;
    uint32_t bcnt;
    uint32_t bpos;
    uint32_t bfree;
    struct vmm_host_ram_bank *bank;

    if ((size == 0) || (align_order < VMM_PAGE_SHIFT) || (BITS_PER_LONG <= align_order)) {
        return 0; /**< 0 */
    }

    size = roundup2_order_size(size, align_order);
    bcnt = VMM_SIZE_TO_PAGE(size);

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank = &rctrl.banks[bn];

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, f);

        if (bank->bmap_free < bcnt) {
            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, f);
            continue;
        }

        binc = order_size(align_order) >> VMM_PAGE_SHIFT;
        bpos = bank->start & order_mask(align_order);

        if (bpos) {
            bpos = VMM_SIZE_TO_PAGE(order_size(align_order) - bpos);
        }

        for (; bpos < (bank->size >> VMM_PAGE_SHIFT); bpos += binc) {
            bfree = 0;

            for (i = bpos; i < (bpos + bcnt); i++) {
                if (bitmap_isset(bank->bmap, i)) {
                    break;
                }

                bfree++;
            }

            if (bfree != bcnt) {
                continue;
            }

            p = bank->start + bpos * VMM_PAGE_SIZE;

            if (ops && !ops->color_match(p, size, color, ops_private)) {
                continue;
            }

            *pa = p;
            bitmap_set(bank->bmap, bpos, bcnt);
            bank->bmap_free -= bcnt;

            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, f);

            return size;
        }

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, f);
    }

    return 0;
}

/**
 * @brief 获取默认颜色的数量
 * @param private 私有数据指针（未使用）
 * @return 默认颜色数量
 */
static uint32_t default_num_colors(void *private)
{
    return U32_MAX;
}

/**
 * @brief 获取默认颜色顺序
 * @param private 私有数据指针（未使用）
 * @return 默认颜色顺序
 */
static uint32_t default_color_order(void *private)
{
    return 16;
}

/**
 * @brief 默认颜色匹配函数
 * @param pa 待操作的物理地址
 * @param size 内存大小
 * @param color 颜色值
 * @param private 私有数据指针（未使用）
 * @return 总是返回TRUE
 */
static bool default_color_match(physical_addr_t pa, physical_size_t size, uint32_t color, void *private)
{
    return TRUE;
}

static struct vmm_host_ram_color_ops default_ops = {
    .name        = "default",
    .num_colors  = default_num_colors,
    .color_order = default_color_order,
    .color_match = default_color_match,
};

/**
 * @brief 设置主机RAM颜色操作接口
 * @param ops 颜色操作结构体指针，如果为NULL则使用默认操作
 * @param private 私有数据指针
 */
void vmm_host_ram_set_color_ops(struct vmm_host_ram_color_ops *ops, void *private)
{
    if (ops) {
        if (!ops->num_colors || !ops->color_order || !ops->color_match) {
            return;
        }

        if (!ops->num_colors(private) || (ops->color_order(private) < VMM_PAGE_SHIFT) || (BITS_PER_LONG <= ops->color_order(private))) {
            return;
        }

        rctrl.ops         = ops;
        rctrl.ops_private = private;
    } else {
        rctrl.ops         = &default_ops;
        rctrl.ops_private = NULL;
    }
}

/**
 * @brief 获取当前颜色操作接口的名称
 * @return 颜色操作接口名称字符串
 */
const char *vmm_host_ram_color_ops_name(void)
{
    return rctrl.ops->name;
}

/**
 * @brief 获取颜色的数量
 * @return 当前颜色操作接口支持的颜色数量
 */
uint32_t vmm_host_ram_color_count(void)
{
    return rctrl.ops->num_colors(rctrl.ops_private);
}

/**
 * @brief 获取颜色顺序
 * @return 当前颜色操作接口的颜色顺序
 */
uint32_t vmm_host_ram_color_order(void)
{
    return rctrl.ops->color_order(rctrl.ops_private);
}

/**
 * @brief 分配指定颜色的RAM内存
 * @param pa 指向物理地址的指针，用于返回分配的地址
 * @param color 要分配的颜色
 * @return 实际分配的内存大小，0表示失败
 */
physical_size_t vmm_host_ram_color_alloc(physical_addr_t *pa, uint32_t color)
{
    uint32_t order = rctrl.ops->color_order(rctrl.ops_private);

    if (rctrl.ops->num_colors(rctrl.ops_private) <= color) {
        return 0;
    }

    return __host_ram_alloc(pa, (physical_size_t)1 << order, order, color, rctrl.ops, rctrl.ops_private);
}

/**
 * @brief 分配主机RAM内存
 * @param pa 指向物理地址的指针，用于返回分配的地址
 * @param size 要分配的内存大小
 * @param align_order 对齐顺序
 * @return 实际分配的内存大小，0表示失败
 */
physical_size_t vmm_host_ram_alloc(physical_addr_t *pa, physical_size_t size, uint32_t align_order)
{
    return __host_ram_alloc(pa, size, align_order, 0, NULL, NULL);
}

/**
 * @brief 保留指定的RAM内存区域
 * @param pa 要保留的物理地址起始位置
 * @param size 要保留的内存大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_ram_reserve(physical_addr_t pa, physical_size_t size)
{
    int                       rc = VMM_ERR_INVALID;
    uint32_t i;
    uint32_t bn;
    uint32_t bcnt;
    uint32_t bpos;
    uint32_t bfree;
    uint64_t bank_end;
    uint64_t pa_end;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank     = &rctrl.banks[bn]; /**< banks成员 */

        bank_end = (uint64_t)bank->start + (uint64_t)bank->size; /**< (uint64_t)bank->size成员 */
        pa_end   = (uint64_t)pa + (uint64_t)size; /**< (uint64_t)size成员 */

        if ((pa < bank->start) || (bank_end < pa_end)) {
            continue;
        }

        bpos = (pa - bank->start) >> VMM_PAGE_SHIFT; /**< VMM_PAGE_SHIFT成员 */
        bcnt = VMM_SIZE_TO_PAGE(size); /**< VMM_SIZE_TO_PAGE(size)成员 */

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags); /**< flags)成员 */

        if (bank->bmap_free < bcnt) {
            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags); /**< flags)成员 */
            rc = VMM_ERR_NOSPC; /**< VMM_ERR_NOSPC成员 */
            break;
        }

        bfree = 0; /**< 0 */

        for (i = bpos; i < (bpos + bcnt); i++) {
            if (bitmap_isset(bank->bmap, i)) {
                break;
            }

            bfree++;
        }

        if (bfree != bcnt) {
            vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags); /**< flags)成员 */
            rc = VMM_ERR_NOSPC; /**< VMM_ERR_NOSPC成员 */
            break;
        }

        bitmap_set(bank->bmap, bpos, bcnt); /**< bcnt)成员 */
        bank->bmap_free -= bcnt; /**< bcnt成员 */

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags); /**< flags)成员 */

        rc = VMM_OK; /**< VMM_OK成员 */
        break;
    }

    return rc;
}

/**
 * @brief 释放指定的RAM内存区域
 * @param pa 要释放的物理地址起始位置
 * @param size 要释放的内存大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_ram_free(physical_addr_t pa, physical_size_t size)
{
    int                       rc = VMM_ERR_INVALID;
    uint32_t bn;
    uint32_t bcnt;
    uint32_t bpos;
    uint64_t bank_end;
    uint64_t pa_end;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank     = &rctrl.banks[bn]; /**< banks成员 */

        bank_end = (uint64_t)bank->start + (uint64_t)bank->size; /**< (uint64_t)bank->size成员 */
        pa_end   = (uint64_t)pa + (uint64_t)size; /**< (uint64_t)size成员 */

        if ((pa < bank->start) || (bank_end < pa_end)) {
            continue;
        }

        bpos = (pa - bank->start) >> VMM_PAGE_SHIFT; /**< VMM_PAGE_SHIFT成员 */
        bcnt = VMM_SIZE_TO_PAGE(size); /**< VMM_SIZE_TO_PAGE(size)成员 */

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags); /**< flags)成员 */

        bitmap_clear(bank->bmap, bpos, bcnt); /**< bcnt)成员 */
        bank->bmap_free += bcnt; /**< bcnt成员 */

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags); /**< flags)成员 */

        rc = VMM_OK; /**< VMM_OK成员 */
        break;
    }

    return rc;
}

/**
 * @brief 检查指定的RAM帧是否空闲
 * @param pa 要检查的物理地址
 * @return 如果空闲返回TRUE，否则返回FALSE
 */
bool vmm_host_ram_frame_isfree(physical_addr_t pa)
{
    uint32_t bn;
    uint32_t bpos;
    uint64_t                  bank_end;
    bool                      ret = FALSE;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank     = &rctrl.banks[bn]; /**< banks成员 */

        bank_end = (uint64_t)bank->start + (uint64_t)bank->size; /**< (uint64_t)bank->size成员 */

        if ((pa < bank->start) || (bank_end <= pa)) {
            continue;
        }

        bpos = (pa - bank->start) >> VMM_PAGE_SHIFT; /**< VMM_PAGE_SHIFT成员 */

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags); /**< flags)成员 */

        if (!bitmap_isset(bank->bmap, bpos)) {
            ret = TRUE; /**< TRUE成员 */
        }

        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags); /**< flags)成员 */

        break;
    }

    return ret;
}

/**
 * @brief 获取总空闲帧的数量
 * @return 所有RAM bank中的总空闲帧数量
 */
uint32_t vmm_host_ram_total_free_frames(void)
{
    uint32_t bn;
    uint32_t ret = 0;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bank;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank = &rctrl.banks[bn]; /**< banks成员 */

        vmm_spin_lock_irq_save_lite(&bank->bmap_lock, flags); /**< flags)成员 */
        ret += bank->bmap_free; /**< bank->bmap_free成员 */
        vmm_spin_unlock_irq_restore_lite(&bank->bmap_lock, flags); /**< flags)成员 */
    }

    return ret;
}

/**
 * @brief 获取总帧的数量
 * @return 所有RAM bank中的总帧数量
 */
uint32_t vmm_host_ram_total_frame_count(void)
{
    uint32_t bn;
    uint32_t ret = 0;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        ret += rctrl.banks[bn].frame_count;
    }

    return ret;
}

/**
 * @brief 获取RAM起始地址
 * @return 所有RAM bank中的最小起始地址
 */
physical_addr_t vmm_host_ram_start(void)
{
    uint32_t        bn;
    physical_addr_t start;
    physical_addr_t ret = 0;

    ret -= 1;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        start = rctrl.banks[bn].start;

        if (start <= ret) {
            ret = start;
        }
    }

    return ret;
}

/**
 * @brief 获取RAM结束地址
 * @return 所有RAM bank中的最大结束地址
 */
physical_addr_t vmm_host_ram_end(void)
{
    uint32_t        bn;
    physical_addr_t end;
    physical_addr_t ret = 0;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        end = rctrl.banks[bn].start + rctrl.banks[bn].size;
        end -= 1;

        if (ret <= end) {
            ret = end;
        }
    }

    return ret;
}

/**
 * @brief 获取总RAM大小
 * @return 所有RAM bank的总大小
 */
physical_size_t vmm_host_ram_total_size(void)
{
    uint32_t        bn;
    physical_size_t ret = 0;

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        ret += rctrl.banks[bn].size;
    }

    return ret;
}

/**
 * @brief 获取RAM bank数量
 * @return RAM bank的数量
 */
uint32_t vmm_host_ram_bank_count(void)
{
    return rctrl.bank_count;
}

/**
 * @brief 获取指定RAM bank的起始地址
 * @param bank bank索引
 * @return 指定bank的起始地址，如果bank无效返回0
 */
physical_addr_t vmm_host_ram_bank_start(uint32_t bank)
{
    return (bank < rctrl.bank_count) ? rctrl.banks[bank].start : 0;
}

/**
 * @brief 获取指定RAM bank的大小
 * @param bank bank索引
 * @return 指定bank的大小，如果bank无效返回0
 */
physical_size_t vmm_host_ram_bank_size(uint32_t bank)
{
    return (bank < rctrl.bank_count) ? rctrl.banks[bank].size : 0;
}

/**
 * @brief 获取指定RAM bank的帧数量
 * @param bank bank索引
 * @return 指定bank的帧数量，如果bank无效返回0
 */
uint32_t vmm_host_ram_bank_frame_count(uint32_t bank)
{
    return (bank < rctrl.bank_count) ? rctrl.banks[bank].frame_count : 0;
}

/**
 * @brief 获取指定RAM bank的空闲帧数量
 * @param bank bank索引
 * @return 指定bank的空闲帧数量，如果bank无效返回0
 */
uint32_t vmm_host_ram_bank_free_frames(uint32_t bank)
{
    uint32_t                  ret;
    irq_flags_t               flags;
    struct vmm_host_ram_bank *bankp;

    if (bank >= rctrl.bank_count) {
        return 0; /**< 0 */
    }

    bankp = &rctrl.banks[bank];

    vmm_spin_lock_irq_save_lite(&bankp->bmap_lock, flags);
    ret = bankp->bmap_free;
    vmm_spin_unlock_irq_restore_lite(&bankp->bmap_lock, flags);

    return ret;
}

/**
 * @brief 估算主机内核空间大小
 * @return 估算的主机内核空间大小
 */
virtual_size_t __init vmm_host_ram_estimate_hksize(void)
{
    int             rc;
    uint32_t bn;
    uint32_t count;
    virtual_size_t  ret;
    physical_size_t size;

    if ((rc = arch_device_tree_ram_bank_count(&count))) {
        return 0;
    }

    if (!count || (count > CONFIG_MAX_RAM_BANK_COUNT)) {
        return 0;
    }

    ret = 0;

    for (bn = 0; bn < count; bn++) {
        if ((rc = arch_device_tree_ram_bank_size(bn, &size))) {
            return ret;
        }

        ret += bitmap_estimate_size(size >> VMM_PAGE_SHIFT);
    }

    return ret;
}

/**
 * @brief 初始化主机RAM管理器
 * @param hkbase 主机内核空间基地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_host_ram_init(virtual_addr_t hkbase)
{
    int                       rc;
    uint32_t                  bn;
    struct vmm_host_ram_bank *bank;

    memset(&rctrl, 0, sizeof(rctrl));

    rctrl.ops         = &default_ops;
    rctrl.ops_private = NULL;

    if ((rc = arch_device_tree_ram_bank_count(&rctrl.bank_count))) {
        return rc;
    }

    if (!rctrl.bank_count) {
        return VMM_ERR_NODEV;
    }

    if (rctrl.bank_count > CONFIG_MAX_RAM_BANK_COUNT) {
        return VMM_ERR_INVALID;
    }

    for (bn = 0; bn < rctrl.bank_count; bn++) {
        bank = &rctrl.banks[bn];

        if ((rc = arch_device_tree_ram_bank_start(bn, &bank->start))) {
            return rc;
        }

        if (bank->start & VMM_PAGE_MASK) {
            return VMM_ERR_INVALID;
        }

        if ((rc = arch_device_tree_ram_bank_size(bn, &bank->size))) {
            return rc;
        }

        if (bank->size & VMM_PAGE_MASK) {
            return VMM_ERR_INVALID;
        }

        bank->frame_count = bank->size >> VMM_PAGE_SHIFT;

        INIT_SPIN_LOCK(&bank->bmap_lock);

        bank->bmap      = (uint64_t *)hkbase;
        bank->bmap_sz   = bitmap_estimate_size(bank->frame_count);
        bank->bmap_free = bank->frame_count;

        bitmap_zero(bank->bmap, bank->frame_count);

        bank->res.start = bank->start;
        bank->res.end   = bank->start + bank->size - 1;
        bank->res.name  = "System RAM";
        bank->res.flags = VMM_IORESOURCE_MEM | VMM_IORESOURCE_BUSY;
        rc              = vmm_request_resource(&vmm_hostmem_resource, &bank->res);

        if (rc) {
            return rc;
        }

        vmm_init_printf("ram: bank%d phys=0x%" PRIPADDR " size=%" PRIPSIZE "\n", bn, bank->start, bank->size);

        vmm_init_printf("ram: bank%d hkbase=0x%" PRIADDR " hksize=%d\n", bn, hkbase, bank->bmap_sz);

        hkbase += bank->bmap_sz;
    }

    return VMM_OK;
}
