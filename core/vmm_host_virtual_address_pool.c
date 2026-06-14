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
 * @file vmm_host_virtual_address_pool.c
 * @author Anup patel (anup@brainfault.org)
 * @brief 虚拟地址池管理实现
 */

#include <libs/buddy.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#define VIRTUAL_ADDR_POOL_MIN_BIN (VMM_PAGE_SHIFT)
#define VIRTUAL_ADDR_POOL_MAX_BIN (__builtin_ctz(CONFIG_VIRTUAL_ADDR_POOL_ALIGN_MB) + 20)

/**
 * @brief 主机虚拟地址池控制结构，管理虚拟地址区间的分配
 */
struct vmm_host_virtual_address_pool_ctrl {
    virtual_addr_t         virtual_address_pool_start; /**< virtual_address_pool_start成员 */
    virtual_size_t         virtual_address_pool_size; /**< virtual_address_pool_size成员 */
    uint32_t               virtual_address_pool_page_count; /**< virtual_address_pool_page_count成员 */
    struct buddy_allocator ba; /**< ba */
};

static struct vmm_host_virtual_address_pool_ctrl vpctrl;

/**
 * @brief 使用伙伴分配器从虚拟地址池中分配指定大小的虚拟地址区间
 * @param[out] va 用于返回分配到的虚拟地址
 * @param[in] size 需要分配的字节数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_alloc(virtual_addr_t *va, virtual_size_t size)
{
    int      rc;
    uint64_t addr;

    if (!va) {
        return VMM_ERR_INVALID;
    }

    rc = buddy_mem_alloc(&vpctrl.ba, size, &addr);

    if (!rc) {
        *va = addr;
    }

    return rc;
}

/**
 * @brief 在虚拟地址池中预留指定范围的地址，防止被后续分配使用
 * @param[in] va 要预留的起始虚拟地址
 * @param[in] size 要预留的字节数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_reserve(virtual_addr_t va, virtual_size_t size)
{
    if ((va < vpctrl.virtual_address_pool_start) || ((vpctrl.virtual_address_pool_start + vpctrl.virtual_address_pool_size) < (va + size))) {
        return VMM_ERR_FAIL;
    }

    return buddy_mem_reserve(&vpctrl.ba, va, size);
}

/**
 * @brief 在虚拟地址池中查找包含指定地址的分配块，返回其起始地址和大小
 * @param[in] va 待查找的虚拟地址
 * @param[out] alloc_va 返回所在分配块的起始地址
 * @param[out] alloc_sz 返回所在分配块的大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_find(virtual_addr_t va, virtual_addr_t *alloc_va, virtual_size_t *alloc_sz)
{
    int      rc;
    uint64_t ava;
    uint64_t asz;

    rc = buddy_mem_find(&vpctrl.ba, va, &ava, NULL, &asz);

    if (rc) {
        return rc;
    }

    if (alloc_va) {
        *alloc_va = ava;
    }

    if (alloc_sz) {
        *alloc_sz = asz;
    }

    return VMM_OK;
}

/**
 * @brief 将虚拟地址池中指定范围的地址归还给伙伴分配器
 * @param[in] va 要释放的起始虚拟地址
 * @param[in] size 要释放的字节数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_free(virtual_addr_t va, virtual_size_t size)
{
    if ((va < vpctrl.virtual_address_pool_start) || ((vpctrl.virtual_address_pool_start + vpctrl.virtual_address_pool_size) < (va + size))) {
        return VMM_ERR_FAIL;
    }

    return buddy_mem_partial_free(&vpctrl.ba, va, size);
}

/**
 * @brief 检查虚拟地址池中指定虚拟地址对应的页是否处于空闲状态
 * @param[in] va 待检查的虚拟地址
 * @return 空闲返回TRUE，否则返回FALSE
 */
bool vmm_host_virtual_address_pool_page_isfree(virtual_addr_t va)
{
    bool ret = FALSE;

    if ((va < vpctrl.virtual_address_pool_start) || ((vpctrl.virtual_address_pool_start + vpctrl.virtual_address_pool_size) <= va)) {
        return ret;
    }

    if (buddy_mem_find(&vpctrl.ba, va, NULL, NULL, NULL) != VMM_OK) {
        ret = TRUE;
    }

    return ret;
}

/**
 * @brief 获取虚拟地址池中当前空闲的页的数量
 * @return 空闲页数量
 */
uint32_t vmm_host_virtual_address_pool_free_page_count(void)
{
    return buddy_bins_free_space(&vpctrl.ba) >> VMM_PAGE_SHIFT;
}

/**
 * @brief 获取虚拟地址池的总页数
 * @return 总页数
 */
uint32_t vmm_host_virtual_address_pool_total_page_count(void)
{
    return vpctrl.virtual_address_pool_page_count;
}

/**
 * @brief 获取虚拟地址池的起始基地址
 * @return 虚拟地址池的起始虚拟地址
 */
virtual_addr_t vmm_host_virtual_address_pool_base(void)
{
    return vpctrl.virtual_address_pool_start;
}

/**
 * @brief 获取虚拟地址池的总字节大小
 * @return 虚拟地址池的总大小（字节）
 */
virtual_size_t vmm_host_virtual_address_pool_size(void)
{
    return vpctrl.virtual_address_pool_size;
}

/**
 * @brief 检查给定虚拟地址是否在虚拟地址池的有效范围内
 * @param[in] addr 待检查的虚拟地址
 * @return 地址在池内返回TRUE，否则返回FALSE
 */
bool vmm_host_virtual_address_pool_isvalid(virtual_addr_t addr)
{
    if ((vpctrl.virtual_address_pool_start <= addr) && (addr < (vpctrl.virtual_address_pool_start + vpctrl.virtual_address_pool_size))) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief 估算管理指定大小的虚拟地址池所需的元数据开销内存
 * @param[in] size 虚拟地址池的总大小（字节）
 * @return 所需元数据内存的字节数，为池大小的1/256
 */
virtual_size_t __init vmm_host_virtual_address_pool_estimate_hksize(virtual_size_t size)
{
    /* VIRTUAL_ADDR_POOL House-Keeping Size = (Total VIRTUAL_ADDR_POOL Size / 256);
     * 12MB VIRTUAL_ADDR_POOL   => 48KB House-Keeping
     * 16MB VIRTUAL_ADDR_POOL   => 64KB House-Keeping
     * 32MB VIRTUAL_ADDR_POOL   => 128KB House-Keeping
     * 64MB VIRTUAL_ADDR_POOL   => 256KB House-Keeping
     * 128MB VIRTUAL_ADDR_POOL  => 512KB House-Keeping
     * 256MB VIRTUAL_ADDR_POOL  => 1024KB House-Keeping
     * 512MB VIRTUAL_ADDR_POOL  => 2048KB House-Keeping
     * 1024MB VIRTUAL_ADDR_POOL => 4096KB House-Keeping
     * ..... and so on .....
     */
    return size >> 8;
}

/**
 * @brief 将虚拟地址池的分配状态输出到字符设备
 * @param[in] cdev 输出目标字符设备
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtual_address_pool_print_state(vmm_char_device_t *cdev)
{
    uint64_t idx;

    vmm_cdev_printf(cdev, "VIRTUAL_ADDR_POOL State\n");

    for (idx = VIRTUAL_ADDR_POOL_MIN_BIN; idx <= VIRTUAL_ADDR_POOL_MAX_BIN; idx++) {
        if (idx < 10) {
            vmm_cdev_printf(cdev, "  [BLOCK %4dB]: ", 1 << idx);
        } else if (idx < 20) {
            vmm_cdev_printf(cdev, "  [BLOCK %4dK]: ", 1 << (idx - 10));
        } else {
            vmm_cdev_printf(cdev, "  [BLOCK %4dM]: ", 1 << (idx - 20));
        }

        vmm_cdev_printf(cdev, "%5lu area(s), %5lu free block(s)\n", buddy_bins_area_count(&vpctrl.ba, idx), buddy_bins_block_count(&vpctrl.ba, idx));
    }

    vmm_cdev_printf(cdev, "VIRTUAL_ADDR_POOL House-Keeping State\n");
    vmm_cdev_printf(cdev, "  Buddy Areas: %lu free out of %lu\n", buddy_hk_area_free(&vpctrl.ba), buddy_hk_area_total(&vpctrl.ba));

    return VMM_OK;
}

/**
 * @brief 初始化虚拟地址池，配置伙伴分配器的管理区域和地址范围
 * @param[in] base 虚拟地址池的起始地址
 * @param[in] size 虚拟地址池的总大小
 * @param[in] hkbase 元数据存储区的起始地址（须位于池范围内）
 * @return 状态值
 */
int __init vmm_host_virtual_address_pool_init(virtual_addr_t base, virtual_size_t size, virtual_addr_t hkbase)
{
    int            rc;
    virtual_size_t hksize;

    vmm_init_printf("virtual_address_pool: base=0x%" PRIADDR " size=%" PRISIZE "\n", base, size);

    if ((hkbase < base) || ((base + size) <= hkbase)) {
        return VMM_ERR_FAIL;
    }

    hksize = vmm_host_virtual_address_pool_estimate_hksize(size);

    vmm_init_printf("virtual_address_pool: hkbase=0x%" PRIADDR " hksize=%" PRISIZE "\n", hkbase, hksize);

    vpctrl.virtual_address_pool_start = base;
    vpctrl.virtual_address_pool_size  = size;
    vpctrl.virtual_address_pool_start &= ~VMM_PAGE_MASK;
    vpctrl.virtual_address_pool_size &= ~VMM_PAGE_MASK;
    vpctrl.virtual_address_pool_page_count = vpctrl.virtual_address_pool_size >> VMM_PAGE_SHIFT;

    rc                                     = buddy_allocator_init(&vpctrl.ba, (void *)hkbase, hksize, base, size, VIRTUAL_ADDR_POOL_MIN_BIN, VIRTUAL_ADDR_POOL_MAX_BIN);

    if (rc) {
        return rc;
    }

    return VMM_OK;
}
