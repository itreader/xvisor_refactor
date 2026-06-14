/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_address_space.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup patel (anup@brainfault.org)
 * @brief 主机虚拟地址空间管理实现
 * 宿主地址空间子系统的核心功能是管理虚拟地址分配和物理地址 - 虚拟地址的映射
 */

#include <arch_config.h>
#include <arch_cpu_addr_space.h>
#include <arch_device_tree.h>
#include <arch_sections.h>
#include <libs/red_black_tree_augmented.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_host_virtual_address_pool.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

static virtual_addr_t host_mem_rw_va[CONFIG_CPU_COUNT];

/* 映射关系 */
struct host_memory_hash_entry {
    red_black_node_t rb;//红黑树节点
    physical_addr_t       pa;//物理地址
    virtual_addr_t        va;//虚拟地址
    virtual_size_t        size;//映射大小
    uint32_t              memory_flags;//内存属性
    uint32_t              ref_count;//应用次数
};

/* 内存映射控制器 */
struct host_memory_hash_ctrl {
    vmm_rwlock_t             lock;//访问读写锁
    virtual_addr_t           start;
    virtual_size_t           size;
    uint32_t                 count;
    red_black_root_t    root;//红黑树根节点
    struct host_memory_hash_entry *entry;
};

static struct host_memory_hash_ctrl host_memory_hash;

/* NOTE: Must be called with write lock held on host_memory_hash.lock */
/**
 * @brief 在主机内存哈希表中分配物理地址到虚拟地址的映射
 * @return 成功返回分配的内存指针，失败返回NULL
 */
static struct host_memory_hash_entry *__host_memory_hash_alloc(void)
{
    uint32_t                 i;
    struct host_memory_hash_entry *e = NULL;

    for (i = 0; i < host_memory_hash.count; i++) {
        if (!host_memory_hash.entry[i].ref_count) {
            e            = &host_memory_hash.entry[i]; /**< 链表入口 */
            e->ref_count = 1; /**< 1 */
            break;
        }
    }

    return e;
}

/* NOTE: Must be called with read/write lock held on host_memory_hash.lock */
/**
 * @brief 获取主机内存哈希表中的总映射的数量
 * @return 数量值
 */
static uint32_t __host_memory_hash_total_count(void)
{
    return host_memory_hash.count;
}

/* NOTE: Must be called with read/write lock held on host_memory_hash.lock */
/**
 * @brief 获取主机内存哈希表中的空闲映射的数量
 * @return 数量值
 */
static uint32_t __host_memory_hash_free_count(void)
{
    uint32_t i;
    uint32_t ret = 0;

    for (i = 0; i < host_memory_hash.count; i++) {
        if (!host_memory_hash.entry[i].ref_count) {
            ret++;
        }
    }

    return ret;
}

/* NOTE: Must be called with read/write lock held on host_memory_hash.lock */
/**
 * @brief 在主机内存哈希表中查找物理地址对应的虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
static struct host_memory_hash_entry *__host_memory_hash_find(physical_addr_t pa)
{
    red_black_node_t   *n;
    struct host_memory_hash_entry *ret = NULL;

    n                            = host_memory_hash.root.red_black_node;

    while (n) {
        struct host_memory_hash_entry *e = rb_entry(n, struct host_memory_hash_entry, rb); /**< rb)成员 */

        if ((e->pa <= pa) && (pa < (e->pa + e->size))) {
            ret = e; /**< e */
            break;
        } 
        
        if (pa < e->pa) {
            n = n->rb_left; /**< n->rb_left成员 */
        } else if ((e->pa + e->size) <= pa) {
            n = n->rb_right; /**< n->rb_right成员 */
        } else {
            vmm_panic("%s: can't find physical address\n", __func__); /**< __func__)成员 */
        }
    }

    return ret;
}

/**
 * @brief 获取主机内存哈希表中的总映射的数量
 * @return 数量值
 */
static uint32_t host_memory_hash_total_count(void)
{
    uint32_t    ret;
    irq_flags_t flags;

    vmm_read_lock_irq_save(&host_memory_hash.lock, flags);
    ret = __host_memory_hash_total_count();
    vmm_read_unlock_irq_restore(&host_memory_hash.lock, flags);

    return ret;
}

/**
 * @brief 获取主机内存哈希表中的空闲映射的数量
 * @return 数量值
 */
static uint32_t host_memory_hash_free_count(void)
{
    uint32_t    ret;
    irq_flags_t flags;

    vmm_read_lock_irq_save(&host_memory_hash.lock, flags);
    ret = __host_memory_hash_free_count();
    vmm_read_unlock_irq_restore(&host_memory_hash.lock, flags);

    return ret;
}

/**
 * @brief 向主机内存哈希表中添加物理地址到虚拟地址的映射
 * @param pa 待操作的物理地址
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @param memory_flags 标志位
 * @return 数量值
 */
static int host_memory_hash_add(physical_addr_t pa, virtual_addr_t va, virtual_size_t size, uint32_t memory_flags)
{
    int         rc = VMM_OK;
    irq_flags_t flags;
    red_black_node_t **new = NULL;
    red_black_node_t *parent = NULL;
    struct host_memory_hash_entry *e = NULL;
    struct host_memory_hash_entry *parent_e = NULL;

    vmm_write_lock_irq_save(&host_memory_hash.lock, flags);

    e = __host_memory_hash_find(pa);

    if (e) {
        if ((va < e->va) || ((e->va + e->size) <= va) || ((va + size) < e->va) || ((e->va + e->size) < (va + size)) ||
            ((e->pa + e->size) < (pa + size)) || (e->memory_flags != memory_flags)) {
            rc = VMM_ERR_INVALID;
            goto done;
        }

        e->ref_count++;
    } else {
        e = __host_memory_hash_alloc();

        if (!e) {
            rc = VMM_ERR_NOMEM;
            goto done;
        }

        e->pa        = pa;
        e->va        = va;
        e->size      = size;
        e->memory_flags = memory_flags;

        new          = &(host_memory_hash.root.red_black_node);

        while (*new) {
            parent   = *new;
            parent_e = rb_entry(parent, struct host_memory_hash_entry, rb);

            if ((e->pa + e->size) <= parent_e->pa) {
                new = &parent->rb_left;
            } else if ((parent_e->pa + parent_e->size) <= e->pa) {
                new = &parent->rb_right;
            } else {
                vmm_panic("%s: can't add entry\n", __func__);
            }
        }

        rb_link_node(&e->rb, parent, new);
        rb_insert_color(&e->rb, &host_memory_hash.root);
    }

done:
    vmm_write_unlock_irq_restore(&host_memory_hash.lock, flags);

    return rc;
}

/**
 * @brief 从主机内存哈希表中删除指定物理地址的映射
 * @param pa 待操作的物理地址
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int host_memory_hash_delete(physical_addr_t pa, virtual_addr_t va, virtual_size_t size)
{
    int                      rc     = VMM_OK;
    uint32_t                 rflags = 0;
    physical_addr_t          rpa[2] = {0, 0};
    virtual_addr_t           rva[2] = {0, 0};
    virtual_size_t           rsz[2] = {0, 0};
    irq_flags_t              flags;
    struct host_memory_hash_entry *e;

    vmm_write_lock_irq_save(&host_memory_hash.lock, flags);

    e = __host_memory_hash_find(pa);

    if (!e) {
        rc = VMM_ERR_NOTAVAIL;
        goto done;
    }

    if ((va < e->va) || ((e->va + e->size) <= va) || ((va + size) < e->va) || ((e->va + e->size) < (va + size)) ||
        ((e->pa + e->size) < (pa + size))) {
        rc = VMM_ERR_INVALID;
        goto done;
    }

    e->ref_count--;

    if (e->ref_count) {
        rc = VMM_ERR_BUSY;
        goto done;
    }

    rb_erase(&e->rb, &host_memory_hash.root);

    rpa[0] = e->pa;
    rva[0] = e->va;
    rsz[0] = va - e->va;
    rpa[1] = pa + size;
    rva[1] = va + size;
    rsz[1] = (e->va + e->size) - (va + size);
    rflags = e->memory_flags;

    memset(e, 0, sizeof(*e));
    RB_CLEAR_NODE(&e->rb);

done:
    vmm_write_unlock_irq_restore(&host_memory_hash.lock, flags);

    if (rsz[0]) {
        if ((rc = host_memory_hash_add(rpa[0], rva[0], rsz[0], rflags))) {
            vmm_panic("%s: can't add left residue error=%d\n", __func__, rc);
        }
    }

    if (rsz[1]) {
        if ((rc = host_memory_hash_add(rpa[1], rva[1], rsz[1], rflags))) {
            vmm_panic("%s: can't add right residue error=%d\n", __func__, rc);
        }
    }

    return rc;
}

/**
 * @brief 通过主机内存哈希表将物理地址转换为虚拟地址
 * @param pa 待操作的物理地址
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @param memory_flags 标志位
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int host_memory_hash_physicalAddr_to_virtualAddr(physical_addr_t pa, virtual_addr_t *va, virtual_size_t *size, uint32_t *memory_flags)
{
    int                      rc = VMM_ERR_NOTAVAIL;
    irq_flags_t              flags;
    struct host_memory_hash_entry *e;

    vmm_read_lock_irq_save(&host_memory_hash.lock, flags);

    e = __host_memory_hash_find(pa);

    if (e) {
        if (va) {
            *va = e->va + (pa - e->pa);
        }

        if (size) {
            *size = e->size - (pa - e->pa);
        }

        if (memory_flags) {
            *memory_flags = e->memory_flags;
        }

        rc = VMM_OK;
    }

    vmm_read_unlock_irq_restore(&host_memory_hash.lock, flags);

    return rc;
}

/**
 * @brief 估算主机内存哈希表管理元数据所需大小
 * @param entry_count 条目数量
 * @return 大小值（字节）
 */
static virtual_size_t host_memory_hash_estimate_hksize(virtual_size_t entry_count)
{
    return sizeof(struct host_memory_hash_entry) * entry_count;
}

/**
 * @brief 初始化主机内存哈希表
 * @param mhash_start 哈希表起始虚拟地址
 * @param mhash_size 哈希表大小
 * @return 大小值（字节）
 */
static int host_memory_hash_init(virtual_addr_t mhash_start, virtual_size_t mhash_size)
{
    uint32_t                 i;
    struct host_memory_hash_entry *e;

    INIT_RW_LOCK(&host_memory_hash.lock);
    host_memory_hash.start = mhash_start;
    host_memory_hash.size  = mhash_size;
    host_memory_hash.count = mhash_size / sizeof(struct host_memory_hash_entry);
    host_memory_hash.root  = RB_ROOT;
    host_memory_hash.entry = (struct host_memory_hash_entry *)host_memory_hash.start;

    if (!host_memory_hash.count) {
        return VMM_ERR_INVALID;
    }

    for (i = 0; i < host_memory_hash.count; i++) {
        e = &host_memory_hash.entry[i];
        memset(e, 0, sizeof(*e));
        RB_CLEAR_NODE(&e->rb);
    }

    return VMM_OK;
}

/**
 * @brief 主机内存映射
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @param memory_flags 标志位
 * @param use_huge_page 是否使用大页标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static virtual_addr_t host_memory_map(physical_addr_t pa, virtual_size_t size, uint32_t memory_flags, bool use_huge_page)
{
    int rc;
    int page_shift;
    virtual_addr_t  ite;
    virtual_addr_t  page_size;
    virtual_addr_t  page_mask;
    virtual_addr_t  va         = 0;
    virtual_addr_t  tsz        = 0;
    physical_addr_t tpa        = 0;
    uint32_t        tmem_flags = 0;

    if (use_huge_page) {
        page_shift = arch_cpu_addr_space_huge_page_log2size();
    } else {
        page_shift = VMM_PAGE_SHIFT;
    }

    page_size = (1 << page_shift);
    page_mask = (page_size - 1);

    size      = roundup2_order_size(size, page_shift);
    tpa       = pa & ~page_mask;

    rc        = host_memory_hash_physicalAddr_to_virtualAddr(tpa, &va, &tsz, &tmem_flags);

    if (rc == VMM_OK) {
        if (memory_flags != tmem_flags) {
            /* Trying to map same physical address with
             * different memory attributes.
             */
            vmm_panic("%s: memory_flags mismatch\n", __func__);
        }

        if (tsz < size) {
            /* Trying to map same physical address with
             * greater size than already mapped.
             */
            vmm_panic("%s: size mismatch\n", __func__);
        }

        va = va & ~page_mask;
    } else if (rc != VMM_ERR_NOTAVAIL) {
        /* Something went wrong. */
        vmm_panic("%s: unhandled error=%d\n", __func__, rc);
    } else {
        if ((rc = vmm_host_virtual_address_pool_alloc(&va, size))) {
            /* Don't have space */
            vmm_panic("%s: virtual_address_pool alloc failed error=%d\n", __func__, rc);
        }

        /* Sanity check on VA */
        if (va & page_mask) {
            /* Don't have space */
            vmm_panic(
                "%s: virtual_address_pool alloc returned VA not aligned "
                "to page_size\n",
                __func__);
        }

        for (ite = 0; ite < (size >> page_shift); ite++) {
            rc = arch_cpu_addr_space_map(va + ite * page_size, page_size, tpa + ite * page_size, memory_flags);

            if (rc) {
                /* We were not able to map physical address */
                vmm_panic(
                    "%s: failed to create VA->PA "
                    "mapping error=%d\n",
                    __func__, rc);
            }
        }
    }

    if ((rc = host_memory_hash_add(tpa, va, size, memory_flags))) {
        /* Failed to update MEMMAP HASH */
        vmm_panic("%s: failed to add memory_map hash entry error=%d\n", __func__, rc);
    }

    return va + (pa & page_mask);
}

/**
 * @brief 取消主机物理地址到虚拟地址的映射
 * @param va 待操作的虚拟地址
 * @param size 数据大小（字节数）
 * @param use_huge_page 是否使用大页标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int host_memory_unmap(virtual_addr_t va, virtual_size_t size, bool use_huge_page)
{
    int             rc;
    int page_shift;
    virtual_addr_t  ite;
    virtual_addr_t page_size;
    virtual_addr_t page_mask;
    physical_addr_t pa = 0x0;

    if (use_huge_page) {
        page_shift = arch_cpu_addr_space_huge_page_log2size();
    } else {
        page_shift = VMM_PAGE_SHIFT;
    }

    page_size = (1 << page_shift);
    page_mask = (page_size - 1);

    size      = roundup2_order_size(size, page_shift);
    va &= ~page_mask;

    if ((rc = arch_cpu_addr_space_virtualAddr_to_physicalAddr(va, &pa))) {
        return rc;
    }

    rc = host_memory_hash_delete(pa, va, size);

    if (rc == VMM_ERR_BUSY) {
        return VMM_OK;
    } else if (rc != VMM_OK) {
        vmm_panic("%s: unhandled error=%d\n", __func__, rc);
    }

    for (ite = 0; ite < (size >> page_shift); ite++) {
        rc = arch_cpu_addr_space_unmap(va + ite * page_size);

        if (rc) {
            return rc;
        }
    }

    if ((rc = vmm_host_virtual_address_pool_free(va, size))) {
        vmm_panic("%s: failed to free virtual address error=%d\n", __func__, rc);
    }

    return VMM_OK;
}

/**
 * @brief 分配对齐的物理页
 * @param page_count 数量
 * @param align_order 阶数
 * @param memory_flags 标志位
 * @param use_huge_page 是否使用大页标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
static virtual_addr_t host_alloc_aligned_pages(uint32_t page_count, uint32_t align_order, uint32_t memory_flags, bool use_huge_page)
{
    uint32_t        page_shift;
    virtual_addr_t  page_size;
    physical_addr_t pa = 0x0;

    if (use_huge_page) {
        page_shift = arch_cpu_addr_space_huge_page_log2size();
    } else {
        page_shift = VMM_PAGE_SHIFT;
    }

    page_size = (1 << page_shift);

    if (align_order < page_shift) {
        align_order = page_shift;
    }

    if (!vmm_host_ram_alloc(&pa, page_count * page_size, align_order)) {
        return 0x0;
    }

    return host_memory_map(pa, page_count * page_size, memory_flags, use_huge_page);
}

/**
 * @brief 获取主机当前空闲的物理页的数量
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @param use_huge_page 是否使用大页标志
 * @return 数量值
 */
static int host_free_pages(virtual_addr_t page_va, uint32_t page_count, bool use_huge_page)
{
    int             rc = VMM_OK;
    uint32_t        page_shift;
    virtual_addr_t page_size;
    virtual_addr_t page_mask;
    physical_addr_t pa = 0x0;

    if (use_huge_page) {
        page_shift = arch_cpu_addr_space_huge_page_log2size();
    } else {
        page_shift = VMM_PAGE_SHIFT;
    }

    page_size = (1 << page_shift);
    page_mask = (page_size - 1);

    page_va &= ~page_mask;

    if ((rc = arch_cpu_addr_space_virtualAddr_to_physicalAddr(page_va, &pa))) {
        return rc;
    }

    if ((rc = host_memory_unmap(page_va, page_count * page_size, use_huge_page))) {
        return rc;
    }

    return vmm_host_ram_free(pa, page_count * page_size);
}

/**
 * @brief 获取主机内存映射哈希总量的数量
 * @return 数量值
 */
uint32_t vmm_host_memory_map_hash_total_count(void)
{
    return host_memory_hash_total_count();
}

/**
 * @brief 获取主机内存映射哈希空闲的数量
 * @return 数量值
 */
uint32_t vmm_host_memory_map_hash_free_count(void)
{
    return host_memory_hash_free_count();
}

/**
 * @brief 主机内存映射
 * @param pa 待操作的物理地址
 * @param size 数据大小（字节数）
 * @param memory_flags 标志位
 * @return 数量值
 */
virtual_addr_t vmm_host_memory_map(physical_addr_t pa, virtual_size_t size, uint32_t memory_flags)
{
    return host_memory_map(pa, size, memory_flags, false);
}

/**
 * @brief 取消主机物理地址到虚拟地址的映射
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_memory_unmap(virtual_addr_t va)
{
    int            rc;
    virtual_addr_t alloc_va;
    virtual_size_t alloc_sz;

    rc = vmm_host_virtual_address_pool_find(va, &alloc_va, &alloc_sz);

    if (rc) {
        return rc;
    }

    return host_memory_unmap(alloc_va, alloc_sz, false);
}

/**
 * @brief 获取主机大页的位移值
 * @return 大页移位值（log2大小），不支持则返回0
 */
uint32_t vmm_host_huge_page_shift(void)
{
    return arch_cpu_addr_space_huge_page_log2size();
}

/**
 * @brief 获取主机大页的大小
 * @return 大小值（字节）
 */
virtual_size_t vmm_host_huge_page_size(void)
{
    return ((virtual_size_t)1) << arch_cpu_addr_space_huge_page_log2size();
}

/**
 * @brief 分配大页内存
 * @param page_count 数量
 * @param memory_flags 标志位
 * @return 大小值（字节）
 */
virtual_addr_t vmm_host_alloc_huge_pages(uint32_t page_count, uint32_t memory_flags)
{
    return host_alloc_aligned_pages(page_count, arch_cpu_addr_space_huge_page_log2size(), memory_flags, true);
}

/**
 * @brief 获取主机当前空闲的大页的数量
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_free_huge_pages(virtual_addr_t page_va, uint32_t page_count)
{
    return host_free_pages(page_va, page_count, true);
}

/**
 * @brief 分配对齐的物理页
 * @param page_count 数量
 * @param align_order 阶数
 * @param memory_flags 标志位
 * @return 成功返回VMM_OK，失败返回错误码
 */
virtual_addr_t vmm_host_alloc_aligned_pages(uint32_t page_count, uint32_t align_order, uint32_t memory_flags)
{
    return host_alloc_aligned_pages(page_count, align_order, memory_flags, false);
}

/**
 * @brief 分配物理页
 * @param page_count 数量
 * @param memory_flags 标志位
 * @return 成功返回分配结果，失败返回错误码
 */
virtual_addr_t vmm_host_alloc_pages(uint32_t page_count, uint32_t memory_flags)
{
    return host_alloc_aligned_pages(page_count, VMM_PAGE_SHIFT, memory_flags, false);
}

/**
 * @brief 获取主机当前空闲的物理页的数量
 * @param page_va 页面虚拟地址
 * @param page_count 数量
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_free_pages(virtual_addr_t page_va, uint32_t page_count)
{
    return host_free_pages(page_va, page_count, false);
}

/**
 * @brief 主机虚拟地址转物理地址
 * @param va 待操作的虚拟地址
 * @param pa 待操作的物理地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_virtualAddr_to_physicalAddr(virtual_addr_t va, physical_addr_t *pa)
{
    int             rc  = VMM_OK;
    physical_addr_t _pa = 0x0;

    if ((rc = arch_cpu_addr_space_virtualAddr_to_physicalAddr(va, &_pa))) {
        return rc;
    }

    if (pa) {
        *pa = _pa;
    }

    return VMM_OK;
}

/**
 * @brief 主机物理地址转虚拟地址
 * @param pa 待操作的物理地址
 * @param va 待操作的虚拟地址
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_host_physicalAddr_to_virtualAddr(physical_addr_t pa, virtual_addr_t *va)
{
    int            rc  = VMM_OK;
    virtual_addr_t _va = 0x0;

    rc                 = host_memory_hash_physicalAddr_to_virtualAddr(pa, &_va, NULL, NULL);

    if (rc) {
        return rc;
    }

    if (va) {
        *va = _va;
    }

    return VMM_OK;
}

/**
 * @brief 从主机物理地址读取数据
 * @param hpa 主机物理地址
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_host_memory_read(physical_addr_t hpa, void *dst, uint32_t len, bool cacheable)
{
    int            rc;
    irq_flags_t    flags;
    uint32_t bytes_read = 0;
    uint32_t page_offset;
    uint32_t page_read;
    virtual_addr_t tmp_va     = host_mem_rw_va[vmm_smp_processor_id()];

    /* Read one page at time with irqs disabled since, we use
     * one virtual address per-host CPU to do read/write.
     */
    while (bytes_read < len) {
        page_offset = hpa & VMM_PAGE_MASK;

        page_read   = VMM_PAGE_SIZE - page_offset;
        page_read   = (page_read < (len - bytes_read)) ? page_read : (len - bytes_read);

        arch_cpu_irq_save(flags);

#if !defined(ARCH_HAS_MEMORY_READWRITE)
        rc = arch_cpu_addr_space_map(
            tmp_va, VMM_PAGE_SIZE, hpa & ~VMM_PAGE_MASK, (cacheable) ? VMM_MEMORY_FLAGS_NORMAL : VMM_MEMORY_FLAGS_NORMAL_NOCACHE);

        if (rc) {
            break;
        }

        memcpy(dst, (void *)(tmp_va + page_offset), page_read);

        rc = arch_cpu_addr_space_unmap(tmp_va);

        if (rc) {
            break;
        }

#else
        rc = arch_cpu_addr_space_memory_read(tmp_va, hpa, dst, page_read, cacheable);

        if (rc) {
            break;
        }

#endif

        arch_cpu_irq_restore(flags);

        hpa += page_read;
        bytes_read += page_read;
        dst += page_read;
    }

    return bytes_read;
}

/**
 * @brief 向主机物理地址写入数据
 * @param hpa 主机物理地址
 * @param src 源设备树节点
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_host_memory_write(physical_addr_t hpa, void *src, uint32_t len, bool cacheable)
{
    int            rc;
    irq_flags_t    flags;
    uint32_t bytes_written = 0;
    uint32_t page_offset;
    uint32_t page_write;
    virtual_addr_t tmp_va        = host_mem_rw_va[vmm_smp_processor_id()];

    /* Write one page at time with irqs disabled since, we use
     * one virtual address per-host CPU to do read/write.
     */
    while (bytes_written < len) {
        page_offset = hpa & VMM_PAGE_MASK;

        page_write  = VMM_PAGE_SIZE - page_offset;
        page_write  = (page_write < (len - bytes_written)) ? page_write : (len - bytes_written);

        arch_cpu_irq_save(flags);

#if !defined(ARCH_HAS_MEMORY_READWRITE)
        rc = arch_cpu_addr_space_map(
            tmp_va, VMM_PAGE_SIZE, hpa & ~VMM_PAGE_MASK, (cacheable) ? VMM_MEMORY_FLAGS_NORMAL : VMM_MEMORY_FLAGS_NORMAL_NOCACHE);

        if (rc) {
            break;
        }

        memcpy((void *)(tmp_va + page_offset), src, page_write);

        rc = arch_cpu_addr_space_unmap(tmp_va);

        if (rc) {
            break;
        }

#else
        rc = arch_cpu_addr_space_memory_write(tmp_va, hpa, src, page_write, cacheable);

        if (rc) {
            break;
        }

#endif

        arch_cpu_irq_restore(flags);

        hpa += page_write;
        bytes_written += page_write;
        src += page_write;
    }

    return bytes_written;
}

/**
 * @brief 设置主机物理地址区域的内存值
 * @param hpa 主机物理地址
 * @param byte 字节值
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际设置的字节数，失败返回0
 */
uint32_t vmm_host_memory_set(physical_addr_t hpa, uint8_t byte, uint32_t len, bool cacheable)
{
    uint8_t         buf[256];
    uint32_t to_wr;
    uint32_t wr;
    uint32_t total_written = 0;
    physical_addr_t pos;
    physical_addr_t end;

    memset(buf, byte, sizeof(buf));

    pos = hpa;
    end = hpa + len;

    while (pos < end) {
        to_wr = (sizeof(buf) < (end - pos)) ? sizeof(buf) : (end - pos);

        wr    = vmm_host_memory_write(pos, buf, to_wr, cacheable);

        pos += to_wr;
        total_written += to_wr;

        if (wr < to_wr) {
            break;
        }
    }

    return total_written;
}

/**
 * @brief 释放初始化完成后不再使用的内存
 * @return 释放的初始化内存大小（KB）
 */
uint32_t vmm_host_free_initmem(void)
{
    int            rc;
    virtual_addr_t init_start;
    virtual_size_t init_size;

    init_start = arch_init_vaddr();
    init_size  = arch_init_size();
    init_size  = VMM_ROUNDUP2_PAGE_SIZE(init_size);

    if ((rc = vmm_host_free_pages(init_start, init_size >> VMM_PAGE_SHIFT))) {
        vmm_panic("%s: failed to free pages error=%d\n", __func__, rc);
    }

    return (init_size >> VMM_PAGE_SHIFT) * VMM_PAGE_SIZE / 1024;
}

/**
 * @brief 初始化从CPU的主机地址空间
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __cpuinit host_addr_space_init_secondary(void)
{
    int rc;

    /* For Non-Boot CPU just call arch code and return */
    rc = arch_cpu_addr_space_secondary_init();

    if (rc) {
        return rc;
    }

#if defined(ARCH_HAS_MEMORY_READWRITE)
    /* Initialize memory read/write for Non-Boot CPU */
    rc = arch_cpu_addr_space_memory_rwinit(host_mem_rw_va[vmm_smp_processor_id()]);

    if (rc) {
        return rc;
    }

#endif

    return VMM_OK;
}

/**
 * @brief 初始化主CPU的主机地址空间
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init host_addr_space_init_primary(void)
{
    int rc;
    int cpu;
    int bank_found = 0;
    uint32_t resv;
    uint32_t resv_count;
    uint32_t bank;
    uint32_t bank_count = 0x0;
    uint64_t        ram_end;
    physical_addr_t ram_start;
    physical_addr_t core_resv_pa = 0x0;
    physical_addr_t arch_resv_pa = 0x0;
    physical_size_t ram_size;
    physical_size_t ram_total_size;
    virtual_addr_t virtual_address_pool_start;
    virtual_addr_t virtual_address_pool_hkstart;
    virtual_addr_t ram_hkstart;
    virtual_addr_t mhash_hkstart;
    virtual_size_t virtual_address_pool_size;
    virtual_size_t virtual_address_pool_hksize;
    virtual_size_t ram_hksize;
    virtual_size_t mhash_entry_count;
    virtual_size_t mhash_hksize;
    virtual_size_t  hk_total_size = 0x0;
    virtual_addr_t core_resv_va = 0x0;
    virtual_addr_t arch_resv_va = 0x0;
    virtual_size_t core_resv_sz = 0x0;
    virtual_size_t arch_resv_sz = 0x0;

    /* Setup RAM banks */
    if ((rc = arch_device_tree_ram_bank_setup())) {
        return rc;
    }

    /* Determine RAM bank count */
    if ((rc = arch_device_tree_ram_bank_count(&bank_count))) {
        return rc;
    }

    if (bank_count == 0) {
        return VMM_ERR_NOMEM;
    }

    if (bank_count > CONFIG_MAX_RAM_BANK_COUNT) {
        return VMM_ERR_INVALID;
    }

    /* Determine total RAM size */
    ram_total_size = 0x0;

    for (bank = 0; bank < bank_count; bank++) {
        if ((rc = arch_device_tree_ram_bank_size(bank, &ram_size))) {
            return rc;
        }

        if (ram_size & VMM_PAGE_MASK) {
            return VMM_ERR_INVALID;
        }

        ram_total_size += ram_size;
    }

    /* Determine RAM bank in which we are loaded */
    bank_found = 0;

    for (bank = 0; bank < bank_count; bank++) {
        if ((rc = arch_device_tree_ram_bank_start(bank, &ram_start))) {
            return rc;
        }

        if (ram_start & VMM_PAGE_MASK) {
            return VMM_ERR_INVALID;
        }

        if ((rc = arch_device_tree_ram_bank_size(bank, &ram_size))) {
            return rc;
        }

        if (ram_size & VMM_PAGE_MASK) {
            return VMM_ERR_INVALID;
        }

        /* Ensure this doesn't overflow for 32-bit */
        ram_end = (uint64_t)ram_start + (uint64_t)ram_size;

        if ((ram_start <= arch_code_paddr_start()) && (arch_code_paddr_start() < ram_end)) {
            bank_found = 1;
            break;
        }
    }

    if (!bank_found) {
        return VMM_ERR_NODEV;
    }

    /* Determine VIRTUAL_ADDR_POOL start and size */
    virtual_address_pool_start  = arch_cpu_addr_space_virtual_address_pool_start();
    virtual_address_pool_size   = arch_cpu_addr_space_virtual_address_pool_estimate_size(ram_total_size);

    /* Determine VIRTUAL_ADDR_POOL house-keeping size based on VIRTUAL_ADDR_POOL size */
    virtual_address_pool_hksize = vmm_host_virtual_address_pool_estimate_hksize(virtual_address_pool_size);

    /* Determine RAM house-keeping size */
    ram_hksize                  = vmm_host_ram_estimate_hksize();

    /* Determine memory_map hash house-keeping size */
    mhash_entry_count           = (virtual_address_pool_size / VMM_PAGE_SIZE) / CONFIG_MEMMAP_HASH_SIZE_FACTOR;

    if (!mhash_entry_count || (CONFIG_MEMMAP_HASH_SIZE_FACTOR < 1)) {
        return VMM_ERR_INVALID;
    }

    mhash_hksize  = host_memory_hash_estimate_hksize(mhash_entry_count);

    /* Calculate physical address, virtual address, and size of
     * core reserved space for VIRTUAL_ADDR_POOL, RAM, and MEMMAP HASH house-keeping
     */
    hk_total_size = virtual_address_pool_hksize + ram_hksize + mhash_hksize;
    hk_total_size = VMM_ROUNDUP2_PAGE_SIZE(hk_total_size);
    core_resv_pa  = ram_start;
    core_resv_va  = virtual_address_pool_start + arch_code_size();
    core_resv_sz  = hk_total_size;

    /* We cannot estimate the physical address, virtual address,
     * and size of arch reserved space so we set all of them to

     * zero and expect that arch_primary_cpu_addr_space_init() will
     * update them if arch code requires arch reserved space.
     */
    arch_resv_pa  = 0x0;
    arch_resv_va  = 0x0;
    arch_resv_sz  = 0x0;

    /* Call arch_primary_cpu_addr_space_init() with the estimated
     * parameters for core reserved space and arch reserved space.
     * The arch_primary_cpu_addr_space_init() can change these parameter
     * as needed.
     */
    if ((rc = arch_cpu_addr_space_primary_init(&core_resv_pa, &core_resv_va, &core_resv_sz, &arch_resv_pa, &arch_resv_va, &arch_resv_sz))) {
        return rc;
    }

    if (core_resv_sz < hk_total_size) {
        return VMM_ERR_FAIL;
    }

    if ((virtual_address_pool_size <= core_resv_sz) || (ram_size <= core_resv_sz)) {
        return VMM_ERR_FAIL;
    }

    virtual_address_pool_hkstart = core_resv_va;
    ram_hkstart                  = virtual_address_pool_hkstart + virtual_address_pool_hksize;
    mhash_hkstart                = ram_hkstart + ram_hksize;

    /* Initialize VIRTUAL_ADDR_POOL management */
    if ((rc = vmm_host_virtual_address_pool_init(virtual_address_pool_start, virtual_address_pool_size, virtual_address_pool_hkstart))) {
        return rc;
    }

    /* Initialize RAM management */
    if ((rc = vmm_host_ram_init(ram_hkstart))) {
        return rc;
    }

    /* Initialize MEMMAP HASH */
    if ((rc = host_memory_hash_init(mhash_hkstart, mhash_hksize))) {
        return rc;
    }

    /* Reserve RAM and MEMAP HASH for code area */
    vmm_init_printf("ram_reserve: phys=0x%" PRIPADDR " size=%" PRISIZE "\n", arch_code_paddr_start(), arch_code_size());

    if ((rc = vmm_host_ram_reserve(arch_code_paddr_start(), arch_code_size()))) {
        return rc;
    }

    if ((rc = host_memory_hash_add(arch_code_paddr_start(), arch_code_vaddr_start(), arch_code_size(), VMM_MEMORY_FLAGS_NORMAL))) {
        return rc;
    }

    /* Reserve RAM and MEMAP HASH for core reserved area */
    vmm_init_printf("ram_reserve: phys=0x%" PRIPADDR " size=%" PRISIZE "\n", core_resv_pa, core_resv_sz);

    if ((rc = vmm_host_ram_reserve(core_resv_pa, core_resv_sz))) {
        return rc;
    }

    if ((rc = host_memory_hash_add(core_resv_pa, core_resv_va, core_resv_sz, VMM_MEMORY_FLAGS_NORMAL))) {
        return rc;
    }

    /* Reserve RAM and MEMAP HASH for arch reserved area */
    vmm_init_printf("ram_reserve: phys=0x%" PRIPADDR " size=%" PRISIZE "\n", arch_resv_pa, arch_resv_sz);

    if ((rc = vmm_host_ram_reserve(arch_resv_pa, arch_resv_sz))) {
        return rc;
    }

    if ((rc = host_memory_hash_add(arch_resv_pa, arch_resv_va, arch_resv_sz, VMM_MEMORY_FLAGS_NORMAL))) {
        return rc;
    }

    /* Reserve VIRTUAL_ADDR_POOL for code, core reserved, and arch reserved areas */
    if (arch_code_vaddr_start() < core_resv_va) {
        core_resv_sz += (core_resv_va - arch_code_vaddr_start());
        core_resv_va = arch_code_vaddr_start();
    }

    if ((arch_resv_sz > 0) && (arch_resv_va < core_resv_va)) {
        core_resv_sz += (core_resv_va - arch_resv_va);
        core_resv_va = arch_resv_va;
    }

    if ((core_resv_va + core_resv_sz) < (arch_code_vaddr_start() + arch_code_size())) {
        core_resv_sz = (arch_code_vaddr_start() + arch_code_size()) - core_resv_va;
    }

    if ((arch_resv_sz > 0) && ((core_resv_va + core_resv_sz) < (arch_resv_va + arch_resv_sz))) {
        core_resv_sz = (arch_resv_va + arch_resv_sz) - core_resv_va;
    }

    vmm_init_printf("virtual_address_pool_reserve: virt=0x%" PRIADDR " size=%" PRISIZE "\n", core_resv_va, core_resv_sz);

    if ((rc = vmm_host_virtual_address_pool_reserve(core_resv_va, core_resv_sz))) {
        return rc;
    }

    /* Reserve portion of RAM as specified by
     * arch device tree functions.
     */
    if ((rc = arch_device_tree_reserve_count(&resv_count))) {
        return rc;
    }

    for (resv = 0; resv < resv_count; resv++) {
        if ((rc = arch_device_tree_reserve_addr(resv, &ram_start))) {
            return rc;
        }

        if ((rc = arch_device_tree_reserve_size(resv, &ram_size))) {
            return rc;
        }

        if (ram_start & VMM_PAGE_MASK) {
            ram_size += ram_start & VMM_PAGE_MASK;
            ram_start -= ram_start & VMM_PAGE_MASK;
        }

        ram_size &= ~VMM_PAGE_MASK;
        vmm_init_printf("ram_reserve: phys=0x%" PRIPADDR " size=%" PRIPSIZE "\n", ram_start, ram_size);

        if ((rc = vmm_host_ram_reserve(ram_start, ram_size))) {
            return rc;
        }
    }

    /* Setup temporary virtual address for physical read/write */
    for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
        rc = vmm_host_virtual_address_pool_alloc(&host_mem_rw_va[cpu], VMM_PAGE_SIZE);

        if (rc) {
            return rc;
        }
    }

#if defined(ARCH_HAS_MEMORY_READWRITE)
    /* Initialize memory read/write for Boot CPU */
    rc = arch_cpu_addr_space_memory_rwinit(host_mem_rw_va[vmm_smp_bootcpu_id()]);

    if (rc) {
        return rc;
    }

#endif

    return VMM_OK;
}

/**
 * @brief 初始化主机地址空间
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __cpuinit vmm_host_address_space_init(void)
{
    if (!vmm_smp_is_bootcpu()) {
        return host_addr_space_init_secondary();
    }

    return host_addr_space_init_primary();
}
